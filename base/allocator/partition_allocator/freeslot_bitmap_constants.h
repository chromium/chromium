// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_FREESLOT_BITMAP_CONSTANTS_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_FREESLOT_BITMAP_CONSTANTS_H_

#include <cstdint>

#include "base/allocator/partition_allocator/partition_alloc_base/bits.h"
#include "base/allocator/partition_allocator/partition_alloc_base/compiler_specific.h"
#include "base/allocator/partition_allocator/partition_alloc_buildflags.h"
#include "base/allocator/partition_allocator/partition_alloc_constants.h"
#include "base/allocator/partition_allocator/partition_alloc_forward.h"
#include "base/allocator/partition_allocator/partition_tag_bitmap.h"
#include "base/allocator/partition_allocator/reservation_offset_table.h"

namespace partition_alloc::internal {

using FreeSlotBitmapCellType = uint64_t;
constexpr size_t kFreeSlotBitmapBitsPerCell =
    sizeof(FreeSlotBitmapCellType) * CHAR_BIT;
constexpr size_t kFreeSlotBitmapOffsetMask = kFreeSlotBitmapBitsPerCell - 1;

// The number of bits necessary for the bitmap is equal to the maximum number of
// slots in a super page.
constexpr size_t kFreeSlotBitmapSize =
    (kSuperPageSize / kSmallestBucket) / CHAR_BIT;

PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR PA_ALWAYS_INLINE size_t
ReservedFreeSlotBitmapSize() {
#if BUILDFLAG(USE_FREESLOT_BITMAP)
  return base::bits::AlignUp(kFreeSlotBitmapSize, PartitionPageSize());
#else
  return 0;
#endif
}

PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR PA_ALWAYS_INLINE size_t
CommittedFreeSlotBitmapSize() {
#if BUILDFLAG(USE_FREESLOT_BITMAP)
  return base::bits::AlignUp(kFreeSlotBitmapSize, SystemPageSize());
#else
  return 0;
#endif
}

PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR PA_ALWAYS_INLINE size_t
NumPartitionPagesPerFreeSlotBitmap() {
  return ReservedFreeSlotBitmapSize() / PartitionPageSize();
}

#if BUILDFLAG(USE_FREESLOT_BITMAP)
PA_ALWAYS_INLINE uintptr_t SuperPageFreeSlotBitmapAddr(uintptr_t super_page) {
  PA_DCHECK(!(super_page % kSuperPageAlignment));
  return super_page + PartitionPageSize() +
         (IsManagedByNormalBuckets(super_page) ? ReservedTagBitmapSize() : 0);
}
#endif

}  // namespace partition_alloc::internal

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_FREESLOT_BITMAP_CONSTANTS_H_