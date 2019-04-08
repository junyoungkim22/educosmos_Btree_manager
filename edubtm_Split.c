/******************************************************************************/
/*                                                                            */
/*    ODYSSEUS/EduCOSMOS Educational-Purpose Object Storage System            */
/*                                                                            */
/*    Developed by Professor Kyu-Young Whang et al.                           */
/*                                                                            */
/*    Database and Multimedia Laboratory                                      */
/*                                                                            */
/*    Computer Science Department and                                         */
/*    Advanced Information Technology Research Center (AITrc)                 */
/*    Korea Advanced Institute of Science and Technology (KAIST)              */
/*                                                                            */
/*    e-mail: kywhang@cs.kaist.ac.kr                                          */
/*    phone: +82-42-350-7722                                                  */
/*    fax: +82-42-350-8380                                                    */
/*                                                                            */
/*    Copyright (c) 1995-2013 by Kyu-Young Whang                              */
/*                                                                            */
/*    All rights reserved. No part of this software may be reproduced,        */
/*    stored in a retrieval system, or transmitted, in any form or by any     */
/*    means, electronic, mechanical, photocopying, recording, or otherwise,   */
/*    without prior written permission of the copyright owner.                */
/*                                                                            */
/******************************************************************************/
/*
 * Module: edubtm_Split.c
 *
 * Description : 
 *  This file has three functions about 'split'.
 *  'edubtm_SplitInternal(...) and edubtm_SplitLeaf(...) insert the given item
 *  after spliting, and return 'ritem' which should be inserted into the
 *  parent page.
 *
 * Exports:
 *  Four edubtm_SplitInternal(ObjectID*, BtreeInternal*, Two, InternalItem*, InternalItem*)
 *  Four edubtm_SplitLeaf(ObjectID*, PageID*, BtreeLeaf*, Two, LeafItem*, InternalItem*)
 */


#include <string.h>
#include "EduBtM_common.h"
#include "BfM.h"
#include "EduBtM_Internal.h"



/*@================================
 * edubtm_SplitInternal()
 *================================*/
/*
 * Function: Four edubtm_SplitInternal(ObjectID*, BtreeInternal*,Two, InternalItem*, InternalItem*)
 *
 * Description:
 * (Following description is for original ODYSSEUS/COSMOS BtM.
 *  For ODYSSEUS/EduCOSMOS EduBtM, refer to the EduBtM project manual.)
 *
 *  At first, the function edubtm_SplitInternal(...) allocates a new internal page
 *  and initialize it.  Secondly, all items in the given page and the given
 *  'item' are divided by halves and stored to the two pages.  By spliting,
 *  the new internal item should be inserted into their parent and the item will
 *  be returned by 'ritem'.
 *
 *  A temporary page is used because it is difficult to use the given page
 *  directly and the temporary page will be copied to the given page later.
 *
 * Returns:
 *  error code
 *    some errors caused by function calls
 *
 * Note:
 *  The caller should call BfM_SetDirty() for 'fpage'.
 */
Four edubtm_SplitInternal(
    ObjectID                    *catObjForFile,         /* IN catalog object of B+ tree file */
    BtreeInternal               *fpage,                 /* INOUT the page which will be splitted */
    Two                         high,                   /* IN slot No. for the given 'item' */
    InternalItem                *item,                  /* IN the item which will be inserted */
    InternalItem                *ritem)                 /* OUT the item which will be returned by spliting */
{
    Four                        e;                      /* error number */
    Two                         i;                      /* slot No. in the given page, fpage */
    Two                         j;                      /* slot No. in the splitted pages */
    Two                         k;                      /* slot No. in the new page */
    Two                         maxLoop;                /* # of max loops; # of slots in fpage + 1 */
    Four                        sum;                    /* the size of a filled area */
    Boolean                     flag=FALSE;             /* TRUE if 'item' become a member of fpage */
    PageID                      newPid;                 /* for a New Allocated Page */
    BtreeInternal               *npage;                 /* a page pointer for the new allocated page */
    Two                         fEntryOffset;           /* starting offset of an entry in fpage */
    Two                         nEntryOffset;           /* starting offset of an entry in npage */
    Two                         entryLen;               /* length of an entry */
    btm_InternalEntry           *fEntry;                /* internal entry in the given page, fpage */
    btm_InternalEntry           *nEntry;                /* internal entry in the new page, npage*/
    Boolean                     isTmp;

	e = btm_AllocPage(catObjForFile, &fpage->hdr.pid, &newPid);
	if(e < 0) ERR(e);

	e = edubtm_InitInternal(&newPid, FALSE, isTmp);
   	if(e < 0) ERR(e); 

	maxLoop = fpage->hdr.nSlots + 1;

	sum = 0;
	for(i = 0; i < maxLoop; i++)
	{
		sum += (sizeof(Two) + sizeof(ShortPageID));
		if(i == high + 1)
		{
			sum +=  ALIGNED_LENGTH(sizeof(Two) + item->klen);
			flag = TRUE;
		}
		else if(i < high + 1)
		{
			fEntry = &fpage->data[fpage->slot[-i]];
			sum += ALIGNED_LENGTH(sizeof(Two) + fEntry->klen);
		}
		else if(i > high + 1)
		{
			fEntry = &fpage->data[fpage->slot[-(i-1)]];
			sum += ALIGNED_LENGTH(sizeof(Two) + fEntry->klen);
		}

		if(sum >= BI_HALF)
			break;
	}
	j = i;

	e = BfM_GetTrain((TrainID*)&newPid, (char**)&npage, PAGE_BUF);
	if(e < 0) ERR(e);

	i++;
	if(i == high + 1)
	{
		fEntry = item;
	}
	else if(i < high + 1)
	{
		fEntry = &fpage->data[fpage->slot[-i]];
	}
	else if(i > high + 1)
	{
		fEntry = &fpage->data[fpage->slot[-(i-1)]];
	}
	npage->hdr.p0 = fEntry->spid;

	ritem->spid = npage->hdr.pid.pageNo;
	
	nEntryOffset = 0;
	k = 0;
	i++;
	while(i < maxLoop)
	{
		npage->slot[-k] = nEntryOffset;
		nEntry = &npage->data[nEntryOffset];
		if(i == high + 1)
		{
			nEntry->spid = item->spid;
			nEntry->klen = item->klen;
			memcpy(nEntry->kval, item->kval, item->klen);
		}
		else if(i < high + 1)
		{
			fEntry = &fpage->data[fpage->slot[-i]];
			nEntry->spid =  fEntry->spid;
			nEntry->klen = fEntry->klen;
			memcpy(nEntry->kval, fEntry->kval, fEntry->klen);
		}
		else if(i > high + 1)
		{
			fEntry = &fpage->data[fpage->slot[-(i-1)]];
			nEntry->spid =  fEntry->spid;
			nEntry->klen = fEntry->klen;
			memcpy(nEntry->kval, fEntry->kval, fEntry->klen);
		}
		nEntryOffset += sizeof(ShortPageID) + ALIGNED_LENGTH(sizeof(Two) + nEntry->klen);
		i++;
		k++;
	}
	npage->hdr.free = nEntryOffset;
	npage->hdr.unused = 0;
	npage->hdr.nSlots = k;

	if(flag)
	{
		fpage->hdr.nSlots = j;
		edubtm_CompactInternalPage(fpage, NIL);
		for(i = j; i >= high + 2; i--)
		{
			fpage->slot[-i] = fpage->slot[-(i-1)];
		}
		fpage->slot[-(high + 1)] = npage->hdr.free;
		fEntry = &fpage->data[fpage->slot[-(high + 1)]];
		fEntry->spid = item->spid;
		fEntry->klen = item->klen;
		memcpy(fEntry->kval, item->kval, fEntry->klen);
		entryLen = sizeof(ShortPageID) + ALIGNED_LENGTH(sizeof(Two) + fEntry->klen);
		npage->hdr.free += entryLen;
		npage->hdr.nSlots++;
	}
	else
	{
		fpage->hdr.nSlots = j + 1;
		edubtm_CompactInternalPage(fpage, NIL);
	}

	if((fpage->hdr.type & ROOT) == ROOT)
	{
		fpage->hdr.type &= ~ROOT;
	}

	e = BfM_SetDirty(&fpage->hdr.pid, PAGE_BUF);
	if(e < 0) ERRB1(e, &fpage->hdr.pid, PAGE_BUF);
	e = BfM_SetDirty(&newPid, PAGE_BUF);
	if(e < 0) ERRB1(e, &newPid, PAGE_BUF);

	e = BfM_FreeTrain(&newPid, PAGE_BUF);
	if(e < 0) ERR(e);

    return(eNOERROR);
    
} /* edubtm_SplitInternal() */



/*@================================
 * edubtm_SplitLeaf()
 *================================*/
/*
 * Function: Four edubtm_SplitLeaf(ObjectID*, PageID*, BtreeLeaf*, Two, LeafItem*, InternalItem*)
 *
 * Description: 
 * (Following description is for original ODYSSEUS/COSMOS BtM.
 *  For ODYSSEUS/EduCOSMOS EduBtM, refer to the EduBtM project manual.)
 *
 *  The function edubtm_SplitLeaf(...) is similar to edubtm_SplitInternal(...) except
 *  that the entry of a leaf differs from the entry of an internal and the first
 *  key value of a new page is used to make an internal item of their parent.
 *  Internal pages do not maintain the linked list, but leaves do it, so links
 *  are properly updated.
 *
 * Returns:
 *  Error code
 *  eDUPLICATEDOBJECTID_BTM
 *    some errors caused by function calls
 *
 * Note:
 *  The caller should call BfM_SetDirty() for 'fpage'.
 */
Four edubtm_SplitLeaf(
    ObjectID                    *catObjForFile, /* IN catalog object of B+ tree file */
    PageID                      *root,          /* IN PageID for the given page, 'fpage' */
    BtreeLeaf                   *fpage,         /* INOUT the page which will be splitted */
    Two                         high,           /* IN slotNo for the given 'item' */
    LeafItem                    *item,          /* IN the item which will be inserted */
    InternalItem                *ritem)         /* OUT the item which will be returned by spliting */
{
    Four                        e;              /* error number */
    Two                         i;              /* slot No. in the given page, fpage */
    Two                         j;              /* slot No. in the splitted pages */
    Two                         k;              /* slot No. in the new page */
    Two                         maxLoop;        /* # of max loops; # of slots in fpage + 1 */
    Four                        sum;            /* the size of a filled area */
    PageID                      newPid;         /* for a New Allocated Page */
    PageID                      nextPid;        /* for maintaining doubly linked list */
    BtreeLeaf                   tpage;          /* a temporary page for the given page */
    BtreeLeaf                   *npage;         /* a page pointer for the new page */
    BtreeLeaf                   *mpage;         /* for doubly linked list */
    btm_LeafEntry               *itemEntry;     /* entry for the given 'item' */
    btm_LeafEntry               *fEntry;        /* an entry in the given page, 'fpage' */
    btm_LeafEntry               *nEntry;        /* an entry in the new page, 'npage' */
    ObjectID                    *iOidArray;     /* ObjectID array of 'itemEntry' */
    ObjectID                    *fOidArray;     /* ObjectID array of 'fEntry' */
    Two                         fEntryOffset;   /* starting offset of 'fEntry' */
    Two                         nEntryOffset;   /* starting offset of 'nEntry' */
    Two                         oidArrayNo;     /* element No in an ObjectID array */
    Two                         alignedKlen;    /* aligned length of the key length */
    Two                         itemEntryLen;   /* length of entry for item */
    Two                         entryLen;       /* entry length */
    Boolean                     flag;
    Boolean                     isTmp;
 
	e = btm_AllocPage(catObjForFile, &fpage->hdr.pid, &newPid);
	if(e < 0) ERR(e);

	e = edubtm_InitLeaf(&newPid, FALSE, isTmp);
   	if(e < 0) ERR(e); 

	maxLoop = fpage->hdr.nSlots + 1;

	memcpy(&tpage, fpage, PAGESIZE);

	sum = 0;
	i = 0;
	fEntryOffset = 0;
	while(sum < BL_HALF)
	{
		fEntry = &fpage->data[fEntryOffset];
		if(i < (high + 1))
		{
			itemEntry = &tpage.data[tpage.slot[-i]];
			entryLen = sizeof(Two) + sizeof(Two) + ALIGNED_LENGTH(itemEntry->klen + sizeof(ObjectID));
			fpage->slot[-i] = fEntryOffset;
			memcpy(fEntry, itemEntry, entryLen);
		}
		else if(i == (high + 1))
		{
			entryLen = sizeof(Two) + sizeof(Two) + ALIGNED_LENGTH(item->klen + sizeof(ObjectID));
			alignedKlen = ALIGNED_LENGTH(item->klen + sizeof(ObjectID));
			fpage->slot[-i] = fEntryOffset;
			fEntry->nObjects = 1;
			fEntry->klen = item->klen;
			memcpy(fEntry->kval, item->kval, alignedKlen);
		}
		else if(i > high + 1)
		{
			itemEntry = &tpage.data[tpage.slot[-(i-1)]];
			entryLen = sizeof(Two) + sizeof(Two) + ALIGNED_LENGTH(itemEntry->klen + sizeof(ObjectID));
			fpage->slot[-(i-1)] = fEntryOffset;
			memcpy(fEntry, itemEntry, entryLen);
		}
		fEntryOffset += entryLen;
		sum += (entryLen + sizeof(Two));
		i++;
	}

	j = i;
	fpage->hdr.nSlots = j;
	fpage->hdr.free = fEntryOffset;
	fpage->hdr.unused = 0;

	e = BfM_GetTrain(&newPid, (char**)&npage, PAGE_BUF);
	if(e < 0) ERR(e);

	k = 0;
	nEntryOffset = 0;
	i++;
	while(i < maxLoop)
	{
		nEntry = &npage->data[nEntryOffset];
		if(i < (high + 1))
		{
			itemEntry = &tpage.data[tpage.slot[-i]];
			entryLen = sizeof(Two) + sizeof(Two) + ALIGNED_LENGTH(itemEntry->klen + sizeof(ObjectID));
			npage->slot[-k] = nEntryOffset;
			memcpy(nEntry, itemEntry, entryLen);
		}
		else if(i == (high + 1))
		{
			entryLen = sizeof(Two) + sizeof(Two) + ALIGNED_LENGTH(item->klen + sizeof(ObjectID));
			alignedKlen = ALIGNED_LENGTH(item->klen + sizeof(ObjectID));
			npage->slot[-k] = nEntryOffset;
			nEntry->nObjects = 1;
			nEntry->klen = item->klen;
			memcpy(nEntry->kval, item->kval, alignedKlen);
		}
		else if(i > high + 1)
		{
			itemEntry = &tpage.data[tpage.slot[-(i-1)]];
			entryLen = sizeof(Two) + sizeof(Two) + ALIGNED_LENGTH(itemEntry->klen + sizeof(ObjectID));
			npage->slot[-(k-1)] = nEntryOffset;
			memcpy(nEntry, itemEntry, entryLen);
		}
		nEntryOffset += entryLen;
		k++;
		i++;
	} 
	npage->hdr.nSlots = k;
	npage->hdr.free = nEntryOffset;
	npage->hdr.unused = 0;

	MAKE_PAGEID(nextPid, root->volNo, fpage->hdr.nextPage);
	e = BfM_GetTrain(&nextPid, (char**)&mpage, PAGE_BUF);
	if(e < 0) ERR(e);

	npage->hdr.nextPage = mpage->hdr.pid.pageNo;
	npage->hdr.prevPage = fpage->hdr.pid.pageNo;

	fpage->hdr.nextPage = npage->hdr.pid.pageNo;
	mpage->hdr.prevPage = npage->hdr.pid.pageNo;

	nEntry = &npage->data[npage->slot[0]];
	ritem->spid = npage->hdr.pid.pageNo;
	ritem->klen = nEntry->klen;
	memcpy(ritem->kval, nEntry->kval, nEntry->klen);

	if(fpage->hdr.type & ROOT)
	{
		fpage->hdr.type &= ~ROOT;
	}

	e = BfM_SetDirty(root, PAGE_BUF);
	if(e < 0) ERRB1(e, root, PAGE_BUF);
	e = BfM_SetDirty(&newPid, PAGE_BUF);
	if(e < 0) ERRB1(e, &newPid, PAGE_BUF);
	e = BfM_SetDirty(&nextPid, PAGE_BUF);
	if(e < 0) ERRB1(e, &nextPid, PAGE_BUF);

	e = BfM_FreeTrain(&newPid, PAGE_BUF);
	if(e < 0) ERR(e);
	e = BfM_FreeTrain(&nextPid, PAGE_BUF);
	if(e < 0) ERR(e);

    return(eNOERROR);
    
} /* edubtm_SplitLeaf() */
