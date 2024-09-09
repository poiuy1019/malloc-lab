
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "6team",
    /* First member's full name */
    "Park Jin Woo",
    /* First member's email address */
    "jwpisthebest@gmail.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1<<12) 

#define MAX(x, y) ((x) > (y)? (x) : (y))

#define PACK(size, alloc) ((size) | (alloc))

#define GET(p)          (*(unsigned int *)(p))
#define PUT(p, val)     (*(unsigned int *)(p) = (val))

#define GET_SIZE(p)     (GET(p) & ~0x7)
#define GET_ALLOC(p)    (GET(p) & 0x1)

#define HDRP(bp)        ((char *)(bp) - WSIZE)
#define FTRP(bp)        ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

# define NEXT_BLKP(bp)  ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
# define PREV_BLKP(bp)  ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

static char *heap_listp;

int mm_init(void);
void *mm_malloc(size_t size);
void mm_free(void *bp);
void *mm_realloc(void *ptr, size_t size);

static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void place(void *bp, size_t newsize);


int mm_init(void)
{   

    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1)
        return -1;

    PUT(heap_listp, 0);
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1));    /* Prologue header */
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1));    /* Prologue footer */
    PUT(heap_listp + (3*WSIZE), PACK(0, 1));        /* Epilogue header */
    heap_listp += (2*WSIZE);

    if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
        return -1;
    return 0;
}

void mm_free(void *bp)
{   
    size_t size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));

    coalesce(bp);
}

void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    
    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;

    size_t copySize = GET_SIZE(HDRP(ptr));
    if (size < copySize)                        
        copySize = size;  
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}

static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE; 
    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

    return coalesce(bp);
}


static void *coalesce(void *bp){   

    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    // case 1: 직전, 직후 블록이 모두 할당
    // 연결 불가, 그대로 bp 리턴 
    if (prev_alloc && next_alloc)
        return bp;

    else if (prev_alloc && !next_alloc)
    {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));      // 직후 블록 사이즈를 현재 블록 사이즈와 합체
        PUT(HDRP(bp), PACK(size, 0));               // 현재 블록 헤더값 갱신
        PUT(FTRP(bp), PACK(size, 0));               // 현재 블록 푸터값 갱신
                                                    // 블록 포인터는 변경할 필요 없다.
    }

    // case 3: 직전 블록 가용, 직후 블록 할당
    else if (!prev_alloc && next_alloc)
    {
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));      // 직전 블록 사이즈를 현재 블록 사이즈와 합체
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));    // 직전 블록 헤더값 갱신
        PUT(FTRP(bp), PACK(size, 0));               // 현재 블록 푸터값 갱신
        bp = PREV_BLKP(bp);                         // 블록 포인터를 직전 블록으로 옮긴다.
    }

    // case 4: 직전, 직후 블록 모두 가용
    else
    {
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));    // 직전 블록 헤더값 갱신
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));    // 직후 블록 푸터값 갱신
        bp = PREV_BLKP(bp);                         // 블록 포인터를 직전 블록으로 옮긴다.
    }

    // 최종 가용 블록의 주소를 리턴한다.
    return bp;
}

void *mm_malloc(size_t size)
{
    size_t asize;           // 정렬 조건과 헤더 & 푸터 용량을 고려하여 조정된 블록 사이즈
    size_t extendsize;      // 메모리를 할당할 자리가 없을 때 (no fit) 힙을 연장할 크기
    char *bp;

    // 가짜 요청 spurious request 무시
    if (size == 0)
        return NULL;

    if (size <= DSIZE)      // 정렬 조건 및 오버헤드(= 헤더 & 푸터 크기)를 고려하여 블록 사이즈 조정
        asize = 2 * DSIZE;  // 요청받은 크기가 8바이트보다 작으면 aszie를 16바이트로 만든다. 
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);
        // 요청받은 크기가 8바이트 보다 크다면, 사이에 8바이트를 더하고 (오버헤드) 다시 7을 더해서(올림 효과를 주기 위함) 8로 나눈 몫에 8을 곲한다. 

    // 가용 리스트에서 적합한 자리를 찾는다.
    if ((bp = find_fit(asize)) != NULL)
    {                       
        place(bp, asize);   
        return bp;
    }

    // 만약 맞는 크기의 가용 블록이 없다면 새로 힙을 늘려서
    extendsize = MAX(asize, CHUNKSIZE); // 둘 중 더 큰 값으로 사이즈를 정한다.

    if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
        return NULL;

    place(bp, asize);
    return bp;
}


static void *find_fit(size_t asize)
{
    void *bp;
    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))) {
            return bp;
        }
    }
    return NULL;
}

static void place(void *bp, size_t asize){

    size_t csize = GET_SIZE(HDRP(bp));

    if ((csize - asize) >= (2 * DSIZE))
    {

        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));

        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));
    }

    else
    {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}







