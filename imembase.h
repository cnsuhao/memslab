/**********************************************************************
 *
 * imembase.h - basic interface of memory operation
 *
 * Copyright (c) 2007 skywind <skywind3000@hotmail.com>
 *
 * - application layer slab allocator implementation
 * - unit interval time cost: almost speed up 500% - 1200% vs malloc
 * - optional page supplier: with the "GFP-Tree" algorithm
 * - memory recycle: automatic give memory back to os to avoid wasting
 * - platform independence
 *
 * for the basic information about slab algorithm, please see:
 *   The Slab Allocator: An Object Caching Kernel 
 *   Memory Allocator (Jeff Bonwick, Sun Microsystems, 1994)
 * with the URL below:
 *   http://citeseer.ist.psu.edu/bonwick94slab.html
 *
 **********************************************************************/

#ifndef __IMEMBASE_H__
#define __IMEMBASE_H__

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif


/*====================================================================*/
/* IALLOCATOR                                                         */
/*====================================================================*/
struct IALLOCATOR
{
    void *(*alloc)(struct IALLOCATOR *, unsigned long);
    void (*free)(struct IALLOCATOR *, void *);
    void *udata;
    long reserved;
};

void* internal_malloc(struct IALLOCATOR *allocator, unsigned long size);
void internal_free(struct IALLOCATOR *allocator, void *ptr);


/*====================================================================*/
/* IVECTOR                                                            */
/*====================================================================*/
struct IVECTOR
{
	unsigned char*data;       
	unsigned long size;      
	unsigned long block;       
	struct IALLOCATOR *allocator;
};

void iv_init(struct IVECTOR *v, struct IALLOCATOR *allocator);
void iv_destroy(struct IVECTOR *v);
int iv_resize(struct IVECTOR *v, unsigned long newsize);


#define IMROUNDSHIFT	3
#define IMROUNDSIZE		(1 << IMROUNDSHIFT)
#define IMROUNDUP(s)	(((s) + IMROUNDSIZE - 1) & ~(IMROUNDSIZE - 1))


/*====================================================================*/
/* IMEMNODE                                                           */
/*====================================================================*/
struct IMEMNODE
{
	struct IALLOCATOR *allocator;   /* memory allocator        */

	struct IVECTOR vprev;           /* prev node link vector   */
	struct IVECTOR vnext;           /* next node link vector   */
	struct IVECTOR vnode;           /* node information data   */
	struct IVECTOR vdata;           /* node data buffer vector */
	struct IVECTOR vmode;           /* mode of allocation      */
	long *mprev;                    /* prev node array         */
	long *mnext;                    /* next node array         */
	long *mnode;                    /* node info array         */
	void**mdata;                    /* node data array         */
	long *mmode;                    /* node mode array         */
	void *extra;                    /* extra user data         */
	long node_free;                 /* number of free nodes    */
	long node_max;                  /* number of all nodes     */
	long grow_limit;                /* limit of growing        */

	long node_size;                 /* node data fixed size    */
	long node_shift;                /* node data size shift    */

	struct IVECTOR vmem;            /* mem-pages in the pool   */
	char **mmem;                    /* mem-pages array         */
	long mem_max;                   /* max num of memory pages */
	long mem_count;                 /* number of mem-pages     */

	long list_open;                 /* the entry of open-list  */
	long list_close;                /* the entry of close-list */
	long total_mem;                 /* total memory size       */
};

void imnode_init(struct IMEMNODE *mn, long nodesize, struct IALLOCATOR *ac);
void imnode_destroy(struct IMEMNODE *mnode);
long imnode_new(struct IMEMNODE *mnode);
void imnode_del(struct IMEMNODE *mnode, long index);
long imnode_head(struct IMEMNODE *mnode);
long imnode_next(struct IMEMNODE *mnode, long index);
void*imnode_data(struct IMEMNODE *mnode, long index);

#define IMNODE_NODE(mnodeptr, i) ((mnodeptr)->mnode[i])
#define IMNODE_PREV(mnodeptr, i) ((mnodeptr)->mprev[i])
#define IMNODE_NEXT(mnodeptr, i) ((mnodeptr)->mnext[i])
#define IMNODE_DATA(mnodeptr, i) ((mnodeptr)->mdata[i])
#define IMNODE_MODE(mnodeptr, i) ((mnodeptr)->mmode[i])


/*====================================================================*/
/* QUEUE DEFINITION                                                   */
/*====================================================================*/
#ifndef __IQUEUE_DEF__
#define __IQUEUE_DEF__

struct IQUEUEHEAD {
	struct IQUEUEHEAD *next, *prev;
};

typedef struct IQUEUEHEAD iqueue_head;


/*--------------------------------------------------------------------*/
/* queue init                                                         */
/*--------------------------------------------------------------------*/
#define IQUEUE_HEAD_INIT(name) { &(name), &(name) }
#define IQUEUE_HEAD(name) \
	struct IQUEUEHEAD name = IQUEUE_HEAD_INIT(name)

#define IQUEUE_INIT(ptr) ( \
	(ptr)->next = (ptr), (ptr)->prev = (ptr))

#define IOFFSETOF(TYPE, MEMBER) ((unsigned long) &((TYPE *)0)->MEMBER)

#define ICONTAINEROF(ptr, type, member) ( \
		(type*)( ((char*)((type*)ptr)) - IOFFSETOF(type, member)) )

#define IQUEUE_ENTRY(ptr, type, member) ICONTAINEROF(ptr, type, member)


/*--------------------------------------------------------------------*/
/* queue operation                                                    */
/*--------------------------------------------------------------------*/
#define IQUEUE_ADD(node, head) ( \
	(node)->prev = (head), (node)->next = (head)->next, \
	(head)->next->prev = (node), (head)->next = (node))

#define IQUEUE_ADD_TAIL(node, head) ( \
	(node)->prev = (head)->prev, (node)->next = (head), \
	(head)->prev->next = (node), (head)->prev = (node))

#define IQUEUE_DEL_BETWEEN(p, n) ((n)->prev = (p), (p)->next = (n))

#define IQUEUE_DEL(entry) (\
	(entry)->next->prev = (entry)->prev, \
	(entry)->prev->next = (entry)->next, \
	(entry)->next = 0, (entry)->prev = 0)

#define IQUEUE_DEL_INIT(entry) do { \
	IQUEUE_DEL(entry); IQUEUE_INIT(entry); } while (0)

#define IQUEUE_IS_EMPTY(entry) ((entry) == (entry)->next)

#define iqueue_init		IQUEUE_INIT
#define iqueue_entry	IQUEUE_ENTRY
#define iqueue_add		IQUEUE_ADD
#define iqueue_add_tail	IQUEUE_ADD_TAIL
#define iqueue_del		IQUEUE_DEL
#define iqueue_del_init	IQUEUE_DEL_INIT
#define iqueue_is_empty IQUEUE_IS_EMPTY

#define IQUEUE_FOREACH(iterator, head, TYPE, MEMBER) \
	for ((iterator) = iqueue_entry((head)->next, TYPE, MEMBER); \
		&((iterator)->MEMBER) != (head); \
		(iterator) = iqueue_entry((iterator)->MEMBER.next, TYPE, MEMBER))

#define iqueue_foreach(iterator, head, TYPE, MEMBER) \
	IQUEUE_FOREACH(iterator, head, TYPE, MEMBER)

#define iqueue_foreach_entry(pos, head) \
	for( (pos) = (head)->next; (pos) != (head) ; (pos) = (pos)->next )
	

#define __iqueue_splice(list, head) do {	\
		iqueue_head *first = (list)->next, *last = (list)->prev; \
		iqueue_head *at = (head)->next; \
		(first)->prev = (head), (head)->next = (first);		\
		(last)->next = (at), (at)->prev = (last); }	while (0)

#define iqueue_splice(list, head) do { \
	if (!iqueue_is_empty(list)) __iqueue_splice(list, head); } while (0)

#define iqueue_splice_init(list, head) do {	\
	iqueue_splice(list, head);	iqueue_init(list); } while (0)


#ifdef _MSC_VER
#pragma warning(disable:4311)
#pragma warning(disable:4312)
#pragma warning(disable:4996)
#endif

#endif


/*====================================================================*/
/* IMEMSLAB                                                           */
/*====================================================================*/
struct IMEMSLAB
{
	struct IQUEUEHEAD queue;
	unsigned long coloroff;
	void*membase;
	long memsize;
	long inuse;
	void*bufctl;
	void*extra;
};

typedef struct IMEMSLAB imemslab_t;

#define IMEMSLAB_ISFULL(s) ((s)->bufctl == 0)
#define IMEMSLAB_ISEMPTY(s) ((s)->inuse == 0)

#ifdef __cplusplus
}
#endif

/*====================================================================*/
/* IMUTEX - mutex interfaces                                          */
/*====================================================================*/
#ifndef IMUTEX_TYPE

#ifndef IMUTEX_DISABLE
#if (defined(WIN32) || defined(_WIN32))
#if ((!defined(_M_PPC)) && (!defined(_M_PPC_BE)) && (!defined(_XBOX)))
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0500
#endif
#ifndef WIN32_LEAN_AND_MEAN  
#define WIN32_LEAN_AND_MEAN  
#endif
#include <windows.h>
#else
#ifndef _XBOX
#define _XBOX
#endif
#include <xtl.h>
#endif

#define IMUTEX_TYPE			CRITICAL_SECTION
#define IMUTEX_INIT(m)		InitializeCriticalSection((CRITICAL_SECTION*)(m))
#define IMUTEX_DESTROY(m)	DeleteCriticalSection((CRITICAL_SECTION*)(m))
#define IMUTEX_LOCK(m)		EnterCriticalSection((CRITICAL_SECTION*)(m))
#define IMUTEX_UNLOCK(m)	LeaveCriticalSection((CRITICAL_SECTION*)(m))

#elif defined(__unix) || defined(unix) || defined(__MACH__)
#include <unistd.h>
#include <pthread.h>
#define IMUTEX_TYPE			pthread_mutex_t
#define IMUTEX_INIT(m)		pthread_mutex_init((pthread_mutex_t*)(m), 0)
#define IMUTEX_DESTROY(m)	pthread_mutex_destroy((pthread_mutex_t*)(m))
#define IMUTEX_LOCK(m)		pthread_mutex_lock((pthread_mutex_t*)(m))
#define IMUTEX_UNLOCK(m)	pthread_mutex_unlock((pthread_mutex_t*)(m))
#endif
#endif

#ifndef IMUTEX_TYPE
#define IMUTEX_TYPE			int
#define IMUTEX_INIT(m)		
#define IMUTEX_DESTROY(m)	
#define IMUTEX_LOCK(m)		
#define IMUTEX_UNLOCK(m)	
#endif

#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef IMUTEX_TYPE imutex_t;
extern int imutex_disable;

void imutex_init(imutex_t *mutex);
void imutex_destroy(imutex_t *mutex);
void imutex_lock(imutex_t *mutex);
void imutex_unlock(imutex_t *mutex);


/*====================================================================*/
/* IMEMGFP (mem_get_free_pages) - a page-supplyer class               */
/*====================================================================*/
struct IMEMGFP
{
	unsigned long page_size;
	long refcnt;
	void*(*alloc_page)(struct IMEMGFP *gfp);
	void (*free_page)(struct IMEMGFP *gfp, void *ptr);
	void *extra;
	unsigned long pages_inuse;
	unsigned long pages_new;
	unsigned long pages_del;
};

#define IDEFAULT_PAGE_SHIFT 16

typedef struct IMEMGFP imemgfp_t;


/*====================================================================*/
/* IMEMLRU                                                            */
/*====================================================================*/
#ifndef IMCACHE_ARRAYLIMIT
#define IMCACHE_ARRAYLIMIT 64
#endif

#ifndef IMCACHE_NODECOUNT_SHIFT
#define IMCACHE_NODECOUNT_SHIFT 0
#endif

#define IMCACHE_NODECOUNT (1 << (IMCACHE_NODECOUNT_SHIFT))

#ifndef IMCACHE_NAMESIZE
#define IMCACHE_NAMESIZE 32
#endif

#ifndef IMCACHE_LRU_SHIFT
#define IMCACHE_LRU_SHIFT	2
#endif

#define IMCACHE_LRU_COUNT	(1 << IMCACHE_LRU_SHIFT)

struct IMEMLRU
{
	int avial;
	int limit;
	int batchcount;
	imutex_t lock;
	void *entry[IMCACHE_ARRAYLIMIT];
};

typedef struct IMEMLRU imemlru_t;


/*====================================================================*/
/* IMEMCACHE                                                          */
/*====================================================================*/
struct IMEMCACHE
{
	unsigned long obj_size;
	unsigned long unit_size;
	unsigned long page_size;
	unsigned long count_partial;
	unsigned long count_full;
	unsigned long count_free;
	unsigned long free_objects;
	unsigned long free_limit;
	unsigned long color_next;
	unsigned long color_limit;

	iqueue_head queue;
	imutex_t list_lock;

	iqueue_head slabs_partial;
	iqueue_head slabs_full;
	iqueue_head slabs_free;

	imemlru_t array[IMCACHE_LRU_COUNT];
	imemgfp_t *gfp;		
	imemgfp_t page_supply;

	unsigned long batchcount;
	unsigned long limit;
	unsigned long num;	
	long flags;
	long user;
	void*extra;

	char name[IMCACHE_NAMESIZE + 1];
	unsigned long pages_hiwater;
	unsigned long pages_inuse;
	unsigned long pages_new;
	unsigned long pages_del;
};

typedef struct IMEMCACHE imemcache_t;


/*====================================================================*/
/* IKMEM INTERFACE                                                    */
/*====================================================================*/
void ikmem_init(unsigned long page_shift, int pg_malloc, unsigned long *sz);
void ikmem_destroy(void);

void* ikmem_malloc(unsigned long size);
void* ikmem_realloc(void *ptr, unsigned long size);
void ikmem_free(void *ptr);
void ikmem_shrink(void);

imemcache_t *ikmem_create(const char *name, unsigned long size);
void ikmem_delete(imemcache_t *cache);
void *ikmem_cache_alloc(imemcache_t *cache);
void ikmem_cache_free(imemcache_t *cache, void *ptr);

unsigned long ikmem_ptr_size(void *ptr);
void ikmem_option(unsigned long watermark);
imemcache_t *ikmem_get(const char *name);

long ikmem_page_info(long *pg_inuse, long *pg_new, long *pg_del);
long ikmem_cache_info(int id, int *inuse, int *cnew, int *cdel, int *cfree);
long ikmem_waste_info(long *kmem_inuse, long *total_mem);


/*====================================================================*/
/* IVECTOR / IMEMNODE MANAGEMENT                                      */
/*====================================================================*/
extern struct IALLOCATOR ikmem_allocator;

typedef struct IVECTOR ivector_t;
typedef struct IMEMNODE imemnode_t;

ivector_t *iv_create(void);
void iv_delete(ivector_t *vec);

imemnode_t *imnode_create(long nodesize, int grow_limit);
void imnode_delete(imemnode_t *);



#ifdef __cplusplus
}
#endif

#endif


