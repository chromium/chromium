// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_CONSTANTS_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_CONSTANTS_H_

#include <limits.h>
#include <cstddef>

#include <algorithm>

#include "base/allocator/buildflags.h"
#include "base/allocator/partition_allocator/page_allocator_constants.h"
#include "build/build_config.h"

#if defined(OS_APPLE)
#include <mach/vm_page_size.h>
#endif

namespace base {

// Underlying partition storage pages (`PartitionPage`s) are a power-of-2 size.
// It is typical for a `PartitionPage` to be based on multiple system pages.
// Most references to "page" refer to `PartitionPage`s.
//
// *Super pages* are the underlying system allocations we make. Super pages
// contain multiple partition pages and include space for a small amount of
// metadata per partition page.
//
// Inside super pages, we store *slot spans*. A slot span is a continguous range
// of one or more `PartitionPage`s that stores allocations of the same size.
// Slot span sizes are adjusted depending on the allocation size, to make sure
// the packing does not lead to unused (wasted) space at the end of the last
// system page of the span. For our current maximum slot span size of 64 KiB and
// other constant values, we pack _all_ `PartitionRoot::Alloc` sizes perfectly
// up against the end of a system page.

#if defined(_MIPS_ARCH_LOONGSON)
PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR ALWAYS_INLINE int
PartitionPageShift() {
  return 16;  // 64 KiB
}
#elif defined(ARCH_CPU_PPC64)
PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR ALWAYS_INLINE int
PartitionPageShift() {
  return 18;  // 256 KiB
}
#elif defined(OS_APPLE)
PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR ALWAYS_INLINE int
PartitionPageShift() {
  return vm_page_shift + 2;
}
#else
PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR ALWAYS_INLINE int
PartitionPageShift() {
  return 14;  // 16 KiB
}
#endif
PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR ALWAYS_INLINE size_t
PartitionPageSize() {
  return 1 << PartitionPageShift();
}
PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR ALWAYS_INLINE size_t
PartitionPageOffsetMask() {
  return PartitionPageSize() - 1;
}
PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR ALWAYS_INLINE size_t
PartitionPageBaseMask() {
  return ~PartitionPageOffsetMask();
}
// TODO: Should this be 1 if defined(_MIPS_ARCH_LOONGSON)?
static const size_t kMaxPartitionPagesPerSlotSpan = 4;

// To avoid fragmentation via never-used freelist entries, we hand out partition
// freelist sections gradually, in units of the dominant system page size. What
// we're actually doing is avoiding filling the full `PartitionPage` (16 KiB)
// with freelist pointers right away. Writing freelist pointers will fault and
// dirty a private page, which is very wasteful if we never actually store
// objects there.

PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR ALWAYS_INLINE size_t
NumSystemPagesPerPartitionPage() {
  return PartitionPageSize() / SystemPageSize();
}

PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR ALWAYS_INLINE size_t
MaxSystemPagesPerSlotSpan() {
  return NumSystemPagesPerPartitionPage() * kMaxPartitionPagesPerSlotSpan;
}

// We reserve virtual address space in 2 MiB chunks (aligned to 2 MiB as well).
// These chunks are called *super pages*. We do this so that we can store
// metadata in the first few pages of each 2 MiB-aligned section. This makes
// freeing memory very fast. 2 MiB size & alignment were chosen, because this
// virtual address block represents a full but single page table allocation on
// ARM, ia32 and x64, which may be slightly more performance&memory efficient.
// (Note, these super pages are backed by 4 KiB system pages and have nothing to
// do with OS concept of "huge pages"/"large pages", even though the size
// coincides.)
//
// The layout of the super page is as follows. The sizes below are the same for
// 32- and 64-bit platforms.
//
//     +-----------------------+
//     | Guard page (4 KiB)    |
//     | Metadata page (4 KiB) |
//     | Guard pages (8 KiB)   |
//     | QuarantineBitmaps     |
//     | Slot span             |
//     | Slot span             |
//     | ...                   |
//     | Slot span             |
//     | Guard pages (16 KiB)  |
//     +-----------------------+
//
// QuarantineBitmaps are inserted for partitions that may have PCScan enabled.
//
// If refcount_at_end_allocation is enabled, RefcountBitmap(4KiB) is inserted
// after the Metadata page for BackupRefPtr. The guard pages after the bitmap
// will be 4KiB.
//
//...
//     | Metadata page (4 KiB) |
//     | RefcountBitmap (4 KiB)|
//     | Guard pages (4 KiB)   |
//...
//
// Each slot span is a contiguous range of one or more `PartitionPage`s. Note
// that slot spans of different sizes may co-exist with one super page. Even
// slot spans of the same size may support different slot sizes. However, all
// slots within a span have to be of the same size.
//
// The metadata page has the following format. Note that the `PartitionPage`
// that is not at the head of a slot span is "unused" (by most part, it only
// stores the offset from the head page). In other words, the metadata for the
// slot span is stored only in the first `PartitionPage` of the slot span.
// Metadata accesses to other `PartitionPage`s are redirected to the first
// `PartitionPage`.
//
//     +---------------------------------------------+
//     | SuperPageExtentEntry (32 B)                 |
//     | PartitionPage of slot span 1 (32 B, used)   |
//     | PartitionPage of slot span 1 (32 B, unused) |
//     | PartitionPage of slot span 1 (32 B, unused) |
//     | PartitionPage of slot span 2 (32 B, used)   |
//     | PartitionPage of slot span 3 (32 B, used)   |
//     | ...                                         |
//     | PartitionPage of slot span N (32 B, used)   |
//     | PartitionPage of slot span N (32 B, unused) |
//     | PartitionPage of slot span N (32 B, unused) |
//     +---------------------------------------------+
//
// A direct-mapped page has an identical layout at the beginning to fake it
// looking like a super page:
//
//     +---------------------------------+
//     | Guard page (4 KiB)              |
//     | Metadata page (4 KiB)           |
//     | Guard pages (8 KiB)             |
//     | Direct mapped object            |
//     | Guard page (4 KiB, 32-bit only) |
//     +---------------------------------+
//
// A direct-mapped page's metadata page has the following layout (on 64 bit
// architectures. On 32 bit ones, the layout is identical, some sizes are
// different due to smaller pointers.):
//
//     +----------------------------------+
//     | SuperPageExtentEntry (32 B)      |
//     | PartitionPage (32 B)             |
//     | PartitionBucket (40 B)           |
//     | PartitionDirectMapExtent (32 B)  |
//     +----------------------------------+
//
// See |PartitionDirectMapMetadata| for details.

static const size_t kSuperPageShift = 21;  // 2 MiB
static const size_t kSuperPageSize = 1 << kSuperPageShift;
static const size_t kSuperPageAlignment = kSuperPageSize;
static const size_t kSuperPageOffsetMask = kSuperPageAlignment - 1;
static const size_t kSuperPageBaseMask = ~kSuperPageOffsetMask;
PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR ALWAYS_INLINE size_t
NumPartitionPagesPerSuperPage() {
  return kSuperPageSize / PartitionPageSize();
}

// Alignment has two constraints:
// - Alignment requirement for scalar types: alignof(std::max_align_t)
// - Alignment requirement for operator new().
//
// The two are separate on Windows 64 bits, where the first one is 8 bytes, and
// the second one 16. We could technically return something different for
// malloc() and operator new(), but this would complicate things, and most of
// our allocations are presumably coming from operator new() anyway.
//
// __STDCPP_DEFAULT_NEW_ALIGNMENT__ is C++17. As such, it is not defined on all
// platforms, as Chrome's requirement is C++14 as of 2020.
#if defined(__STDCPP_DEFAULT_NEW_ALIGNMENT__)
static constexpr size_t kAlignment =
    std::max(alignof(max_align_t), __STDCPP_DEFAULT_NEW_ALIGNMENT__);
#else
static constexpr size_t kAlignment = alignof(max_align_t);
#endif
static_assert(kAlignment <= 16,
              "PartitionAlloc doesn't support a fundamental alignment larger "
              "than 16 bytes.");

// The "order" of an allocation is closely related to the power-of-1 size of the
// allocation. More precisely, the order is the bit index of the
// most-significant-bit in the allocation size, where the bit numbers starts at
// index 1 for the least-significant-bit.
//
// In terms of allocation sizes, order 0 covers 0, order 1 covers 1, order 2
// covers 2->3, order 3 covers 4->7, order 4 covers 8->15.

// PartitionAlloc should return memory properly aligned for any type, to behave
// properly as a generic allocator. This is not strictly required as long as
// types are explicitly allocated with PartitionAlloc, but is to use it as a
// malloc() implementation, and generally to match malloc()'s behavior.
//
// In practice, this means 8 bytes alignment on 32 bit architectures, and 16
// bytes on 64 bit ones.
static const size_t kMinBucketedOrder =
    kAlignment == 16 ? 5 : 4;  // 2^(order - 1), that is 16 or 8.
// The largest bucketed order is 1 << (20 - 1), storing [512 KiB, 1 MiB):
static const size_t kMaxBucketedOrder = 20;
static const size_t kNumBucketedOrders =
    (kMaxBucketedOrder - kMinBucketedOrder) + 1;
// Eight buckets per order (for the higher orders), e.g. order 8 is 128, 144,
// 160, ..., 240:
static const size_t kNumBucketsPerOrderBits = 3;
static const size_t kNumBucketsPerOrder = 1 << kNumBucketsPerOrderBits;
static const size_t kNumBuckets = kNumBucketedOrders * kNumBucketsPerOrder;
static const size_t kSmallestBucket = 1 << (kMinBucketedOrder - 1);
static const size_t kMaxBucketSpacing =
    1 << ((kMaxBucketedOrder - 1) - kNumBucketsPerOrderBits);
static const size_t kMaxBucketed =
    (1 << (kMaxBucketedOrder - 1)) +
    ((kNumBucketsPerOrder - 1) * kMaxBucketSpacing);
// Limit when downsizing a direct mapping using `realloc`:
static const size_t kMinDirectMappedDownsize = kMaxBucketed + 1;
// Intentionally set to less than 2GiB to make sure that a 2GiB allocation
// fails. This is a security choice in Chrome, to help making size_t vs int bugs
// harder to exploit.
//
// There are matching limits in other allocators, such as tcmalloc. See
// crbug.com/998048 for details.
PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR ALWAYS_INLINE size_t
MaxDirectMapped() {
  // Subtract kSuperPageSize to accommodate for alignment inside
  // PartitionRoot::GetDirectMapReservedSize.
  return (1UL << 31) - kSuperPageSize;
}
static const size_t kBitsPerSizeT = sizeof(void*) * CHAR_BIT;

// Constant for the memory reclaim logic.
static const size_t kMaxFreeableSpans = 16;

// If the total size in bytes of allocated but not committed pages exceeds this
// value (probably it is a "out of virtual address space" crash), a special
// crash stack trace is generated at
// `PartitionOutOfMemoryWithLotsOfUncommitedPages`. This is to distinguish "out
// of virtual address space" from "out of physical memory" in crash reports.
static const size_t kReasonableSizeOfUnusedPages = 1024 * 1024 * 1024;  // 1 GiB

// These byte values match tcmalloc.
static const unsigned char kUninitializedByte = 0xAB;
static const unsigned char kFreedByte = 0xCD;

static const unsigned char kQuarantinedByte = 0xEF;

// Flags for `PartitionAllocFlags`.
enum PartitionAllocFlags {
  PartitionAllocReturnNull = 1 << 0,
  PartitionAllocZeroFill = 1 << 1,
  PartitionAllocNoHooks = 1 << 2,  // Internal only.
  // If the allocation requires a "slow path" (such as allocating/committing a
  // new slot span), return nullptr instead. Note this makes all large
  // allocations return nullptr, such as direct-mapped ones, and even for
  // smaller ones, a nullptr value is common.
  PartitionAllocFastPathOrReturnNull = 1 << 3,  // Internal only.

  PartitionAllocLastFlag = PartitionAllocFastPathOrReturnNull
};

}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_CONSTANTS_H_
