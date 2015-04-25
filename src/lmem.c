/*
** $Id: lmem.c,v 1.84.1.1 2013/04/12 18:48:47 roberto Exp $
** Interface to Memory Manager
** See Copyright Notice in lua.h
*/


#include <stddef.h>

#define lmem_c
#define LUA_CORE

#include "lua.h"

#include "ldebug.h"
#include "ldo.h"
#include "lgc.h"
#include "lmem.h"
#include "lobject.h"
#include "lstate.h"



/*
** About the realloc function:
** void * frealloc (void *ud, void *ptr, size_t osize, size_t nsize);
** (`osize' is the old size, `nsize' is the new size)
**
** * frealloc(ud, NULL, x, s) creates a new block of size `s' (no
** matter 'x').
**
** * frealloc(ud, p, x, 0) frees the block `p'
** (in this specific case, frealloc must return NULL);
** particularly, frealloc(ud, NULL, 0, 0) does nothing
** (which is equivalent to free(NULL) in ANSI C)
**
** frealloc returns NULL if it cannot create or reallocate the area
** (any reallocation to an equal or smaller size cannot fail!)
*/



#define MINSIZEARRAY	4

/** *size 为要回收的个数
 ** size_elems  新增的每个对象的大小
 ** what 新增的对象是什么类型
 */
void *luaM_growaux_ (lua_State *L, void *block, int *size, size_t size_elems,
                     int limit, const char *what) {
  void *newblock;
  int newsize;
  //如果要回收的大小，大于limit的0.5 
  if (*size >= limit/2) {  /* cannot double it? */
    //如果要回收的大小大于limit则报错，新增的newsize = limit
    if (*size >= limit)  /* cannot grow even a little? */
      luaG_runerror(L, "too many %s (limit is %d)", what, limit);
    //在已增加到一半以上的情况下，直接新增到最大
    newsize = limit;  /* still have at least one free place */
  }
  else {
    //回收的大小，小于limit的0.5时，
    //新大小为原大小的2倍
    newsize = (*size)*2;
    if (newsize < MINSIZEARRAY)
      newsize = MINSIZEARRAY;  /* minimum size */
  }
  newblock = luaM_reallocv(L, block, *size, newsize, size_elems);
  *size = newsize;  /* update only when everything else is OK */
  return newblock;
}


l_noret luaM_toobig (lua_State *L) {
  luaG_runerror(L, "memory allocation error: block too big");
}



/*
** generic allocation routine.
** lua_State *L 线程对象
** void *block  要删除的老的对象
** size_t osize 删除的老的大小
** size_t nsize 新增的大小
*/
void *luaM_realloc_ (lua_State *L, void *block, size_t osize, size_t nsize) {
  void *newblock;
  global_State *g = G(L);
  size_t realosize = (block) ? osize : 0;
  lua_assert((realosize == 0) == (block == NULL));
#if defined(HARDMEMTESTS)
  //如果要新增的大小，大于要删除的大小，并且gc正在运行,则运行fullgc
  if (nsize > realosize && g->gcrunning)
    luaC_fullgc(L, 1);  /* force a GC whenever possible */
#endif
  //申请空间
  newblock = (*g->frealloc)(g->ud, block, osize, nsize);
  //如果没有申请到空间，并且要求申请的空间大于0
  if (newblock == NULL && nsize > 0) {
    api_check(L, nsize > realosize,
                 "realloc cannot fail when shrinking a block");
    if (g->gcrunning) {
      luaC_fullgc(L, 1);  /* try to free some memory... */
      newblock = (*g->frealloc)(g->ud, block, osize, nsize);  /* try again */
    }
    if (newblock == NULL)
      luaD_throw(L, LUA_ERRMEM);
  }
  lua_assert((nsize == 0) == (newblock == NULL));
  //重置g->GCdebt
  g->GCdebt = (g->GCdebt + nsize) - realosize;
  return newblock;
}

