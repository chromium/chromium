// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_FREESLOT_BITMAP_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_FREESLOT_BITMAP_H_

#include <climits>
#include <cstdint>
#include <utility>

#include "base/allocator/partition_allocator/freeslot_bitmap_constants.h"
#include "base/allocator/partition_allocator/partition_alloc_base/bits.h"
#include "base/allocator/partition_allocator/partition_alloc_base/compiler_specific.h"
#include "base/allocator/partition_allocator/partition_alloc_buildflags.h"
#include "base/allocator/partition_allocator/partition_alloc_constants.h"
#include "base/allocator/partition_allocator/partition_page.h"

#if BUILDFLAG(USE_FREESLOT_BITMAP)

namespace partition_alloc::internal {

PA_ALWAYS_INLINE uintptr_t GetFreeSlotBitmapAddressForPointer(uintptr_t ptr) {
  uintptr_t super_page = ptr & kSuperPageBaseMask;
  return SuperPageFreeSlotBitmapAddr(super_page);
}

// Calculates the cell address and the offset inside the cell corresponding to
// the |slot_address|.
PA_ALWAYS_INLINE std::pair<FreeSlotBitmapCellType*, size_t>
GetFreeSlotBitmapCellPtrAndBitIndex(uintptr_t slot_address) {
  uintptr_t slot_superpage_offset = slot_address & kSuperPageOffsetMask;
  uintptr_t superpage_bitmap_start =
      GetFreeSlotBitmapAddressForPointer(slot_address);
  uintptr_t cell_addr = base::bits::AlignDown(
      superpage_bitmap_start + (slot_superpage_offset / kAlignment) / CHAR_BIT,
      sizeof(FreeSlotBitmapCellType));
  PA_DCHECK(cell_addr < superpage_bitmap_start + kFreeSlotBitmapSize);
  size_t bit_index =
      (slot_superpage_offset / kAlignment) & kFreeSlotBitmapOffsetMask;
  PA_DCHECK(bit_index < kFreeSlotBitmapBitsPerCell);
  return {reinterpret_cast<FreeSlotBitmapCellType*>(cell_addr), bit_index};
}

// This bitmap marks the used slot as 0 and free one as 1. This is because we
// would like to set all the slots as "used" by default to prevent allocating a
// used slot when the freelist entry is overwritten.

// Returns true if the bit corresponding to |address| is used( = 0)
PA_ALWAYS_INLINE bool FreeSlotBitmapSlotIsUsed(uintptr_t address) {
  auto [cell, bit_index] = GetFreeSlotBitmapCellPtrAndBitIndex(address);
  return (*cell & (static_cast<FreeSlotBitmapCellType>(1) << bit_index)) == 0;
}

// Mark the bit corresponding to |address| as used( = 0).
PA_ALWAYS_INLINE void FreeSlotBitmapMarkSlotAsUsed(uintptr_t address) {
  PA_DCHECK(!FreeSlotBitmapSlotIsUsed(address));
  auto [cell, bit_index] = GetFreeSlotBitmapCellPtrAndBitIndex(address);
  *cell &= ~(static_cast<FreeSlotBitmapCellType>(1) << bit_index);
}

// Mark the bit corresponding to |address| as free( = 1).
PA_ALWAYS_INLINE void FreeSlotBitmapMarkSlotAsFree(uintptr_t address) {
  PA_DCHECK(FreeSlotBitmapSlotIsUsed(address));
  auto [cell, bit_index] = GetFreeSlotBitmapCellPtrAndBitIndex(address);
  *cell |= (static_cast<FreeSlotBitmapCellType>(1) << bit_index);
}

}  // namespace partition_alloc::internal

#endif  // BUILDFLAG(USE_FREESLOT_BITMAP)

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_FREESLOT_BITMAP_H_