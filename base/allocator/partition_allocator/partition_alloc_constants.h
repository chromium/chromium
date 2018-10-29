// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_CONSTANTS_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_CONSTANTS_H_

#include <limits.h>

#include "base/allocator/partition_allocator/page_allocator_constants.h"
#include "base/logging.h"

namespace base {

// Allocation granularity of sizeof(void*) bytes.
static const size_t kAllocationGranularity = sizeof(void*);
static const size_t kAllocationGranularityMask = kAllocationGranularity - 1;
static const size_t kBucketShift = (kAllocationGranularity == 8) ? 3 : 2;

// Underlying partition storage pages are a power-of-two size. It is typical
// for a partition page to be based on multiple system pages. Most references to
// "page" refer to partition pages.
// We also have the concept of "super pages" -- these are the underlying system
// allocations we make. Super pages contain multiple partition pages inside them
// and include space for a small amount of metadata per partition page.
// Inside super pages, we store "slot spans". A slot span is a continguous range
// of one or more partition pages that stores allocations of the same size.
// Slot span sizes are adjusted depending on the allocation size, to make sure
// the packing does not lead to unused (wasted) space at the end of the last
// system page of the span. For our current max slot span size of 64k and other
// constant values, we pack _all_ PartitionRootGeneric::Alloc() sizes perfectly
// up against the end of a system page.
#if defined(_MIPS_ARCH_LOONGSON)
static const size_t kPartitionPageShift = 16;  // 64KB
#else
static const size_t kPartitionPageShift = 14;  // 16KB
#endif
static const size_t kPartitionPageSize = 1 << kPartitionPageShift;
static const size_t kPartitionPageOffsetMask = kPartitionPageSize - 1;
static const size_t kPartitionPageBaseMask = ~kPartitionPageOffsetMask;
static const size_t kMaxPartitionPagesPerSlotSpan = 4;

// To avoid fragmentation via never-used freelist entries, we hand out partition
// freelist sections gradually, in units of the dominant system page size.
// What we're actually doing is avoiding filling the full partition page (16 KB)
// with freelist pointers right away. Writing freelist pointers will fault and
// dirty a private page, which is very wasteful if we never actually store
// objects there.
static const size_t kNumSystemPagesPerPartitionPage =
    kPartitionPageSize / kSystemPageSize;
static const size_t kMaxSystemPagesPerSlotSpan =
    kNumSystemPagesPerPartitionPage * kMaxPartitionPagesPerSlotSpan;

// We reserve virtual address space in 2MB chunks (aligned to 2MB as well).
// These chunks are called "super pages". We do this so that we can store
// metadata in the first few pages of each 2MB aligned section. This leads to
// a very fast free(). We specifically choose 2MB because this virtual address
// block represents a full but single PTE allocation on ARM, ia32 and x64.
//
// The layout of the super page is as follows. The sizes below are the same
// for 32 bit and 64 bit.
//
//   | Guard page (4KB)    |
//   | Metadata page (4KB) |
//   | Guard pages (8KB)   |
//   | Slot span           |
//   | Slot span           |
//   | ...                 |
//   | Slot span           |
//   | Guard page (4KB)    |
//
//   - Each slot span is a contiguous range of one or more PartitionPages.
//   - The metadata page has the following format. Note that the PartitionPage
//     that is not at the head of a slot span is "unused". In other words,
//     the metadata for the slot span is stored only in the first PartitionPage
//     of the slot span. Metadata accesses to other PartitionPages are
//     redirected to the first PartitionPage.
//
//     | SuperPageExtentEntry (32B)                 |
//     | PartitionPage of slot span 1 (32B, used)   |
//     | PartitionPage of slot span 1 (32B, unused) |
//     | PartitionPage of slot span 1 (32B, unused) |
//     | PartitionPage of slot span 2 (32B, used)   |
//     | PartitionPage of slot span 3 (32B, used)   |
//     | ...                                        |
//     | PartitionPage of slot span N (32B, unused) |
//
// A direct mapped page has a similar layout to fake it looking like a super
// page:
//
//     | Guard page (4KB)     |
//     | Metadata page (4KB)  |
//     | Guard pages (8KB)    |
//     | Direct mapped object |
//     | Guard page (4KB)     |
//
//    - The metadata page has the following layout:
//
//     | SuperPageExtentEntry (32B)    |
//     | PartitionPage (32B)           |
//     | PartitionBucket (32B)         |
//     | PartitionDirectMapExtent (8B) |
static const size_t kSuperPageShift = 21;  // 2MB
static const size_t kSuperPageSize = 1 << kSuperPageShift;
static const size_t kSuperPageOffsetMask = kSuperPageSize - 1;
static const size_t kSuperPageBaseMask = ~kSuperPageOffsetMask;
static const size_t kNumPartitionPagesPerSuperPage =
    kSuperPageSize / kPartitionPageSize;

// The following kGeneric* constants apply to the generic variants of the API.
// The "order" of an allocation is closely related to the power-of-two size of
// the allocation. More precisely, the order is the bit index of the
// most-significant-bit in the allocation size, where the bit numbers starts
// at index 1 for the least-significant-bit.
// In terms of allocation sizes, order 0 covers 0, order 1 covers 1, order 2
// covers 2->3, order 3 covers 4->7, order 4 covers 8->15.
static const size_t kGenericMinBucketedOrder = 4;  // 8 bytes.
static const size_t kGenericMaxBucketedOrder =
    20;  // Largest bucketed order is 1<<(20-1) (storing 512KB -> almost 1MB)
static const size_t kGenericNumBucketedOrders =
    (kGenericMaxBucketedOrder - kGenericMinBucketedOrder) + 1;
// Eight buckets per order (for the higher orders), e.g. order 8 is 128, 144,
// 160, ..., 240:
static const size_t kGenericNumBucketsPerOrderBits = 3;
static const size_t kGenericNumBucketsPerOrder =
    1 << kGenericNumBucketsPerOrderBits;
static const size_t kGenericNumBuckets =
    kGenericNumBucketedOrders * kGenericNumBucketsPerOrder;
static const size_t kGenericSmallestBucket = 1
                                             << (kGenericMinBucketedOrder - 1);
static const size_t kGenericMaxBucketSpacing =
    1 << ((kGenericMaxBucketedOrder - 1) - kGenericNumBucketsPerOrderBits);
static const size_t kGenericMaxBucketed =
    (1 << (kGenericMaxBucketedOrder - 1)) +
    ((kGenericNumBucketsPerOrder - 1) * kGenericMaxBucketSpacing);
static const size_t kGenericMinDirectMappedDownsize =
    kGenericMaxBucketed +
    1;  // Limit when downsizing a direct mapping using realloc().
static const size_t kGenericMaxDirectMapped =
    (1UL << 31) + kPageAllocationGranularity;  // 2 GB plus one more page.
static const size_t kBitsPerSizeT = sizeof(void*) * CHAR_BIT;

// Constant for the memory reclaim logic.
static const size_t kMaxFreeableSpans = 16;

// If the total size in bytes of allocated but not committed pages exceeds this
// value (probably it is a "out of virtual address space" crash),
// a special crash stack trace is generated at |PartitionOutOfMemory|.
// This is to distinguish "out of virtual address space" from
// "out of physical memory" in crash reports.
static const size_t kReasonableSizeOfUnusedPages = 1024 * 1024 * 1024;  // 1GB

// These two byte values match tcmalloc.
static const unsigned char kUninitializedByte = 0xAB;
static const unsigned char kFreedByte = 0xCD;

// Flags for PartitionAllocGenericFlags.
enum PartitionAllocFlags {
  PartitionAllocReturnNull = 1 << 0,
  PartitionAllocZeroFill = 1 << 1,

  PartitionAllocLastFlag = PartitionAllocZeroFill
};

}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_CONSTANTS_H_
