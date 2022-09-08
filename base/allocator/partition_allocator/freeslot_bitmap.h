// Copyright 2022 The Chromium Authors
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
// used slot when the freelist entry is overwritten. The state of the bitmap is
// expected to be synced with freelist (i.e. the bitmap is set to 1 if and only
// if the slot is in the freelist).

PA_ALWAYS_INLINE FreeSlotBitmapCellType CellWithAOne(size_t n) {
  return static_cast<FreeSlotBitmapCellType>(1) << n;
}

PA_ALWAYS_INLINE FreeSlotBitmapCellType CellWithTrailingOnes(size_t n) {
  return (static_cast<FreeSlotBitmapCellType>(1) << n) -
         static_cast<FreeSlotBitmapCellType>(1);
}

// Returns true if the bit corresponding to |address| is used( = 0)
PA_ALWAYS_INLINE bool FreeSlotBitmapSlotIsUsed(uintptr_t address) {
  auto [cell, bit_index] = GetFreeSlotBitmapCellPtrAndBitIndex(address);
  return (*cell & CellWithAOne(bit_index)) == 0;
}

// Mark the bit corresponding to |address| as used( = 0).
PA_ALWAYS_INLINE void FreeSlotBitmapMarkSlotAsUsed(uintptr_t address) {
  PA_DCHECK(!FreeSlotBitmapSlotIsUsed(address));
  auto [cell, bit_index] = GetFreeSlotBitmapCellPtrAndBitIndex(address);
  *cell &= ~CellWithAOne(bit_index);
}

// Mark the bit corresponding to |address| as free( = 1).
PA_ALWAYS_INLINE void FreeSlotBitmapMarkSlotAsFree(uintptr_t address) {
  PA_DCHECK(FreeSlotBitmapSlotIsUsed(address));
  auto [cell, bit_index] = GetFreeSlotBitmapCellPtrAndBitIndex(address);
  *cell |= CellWithAOne(bit_index);
}

// Sets all the bits corresponding to [begin_addr, end_addr) to 0.
PA_ALWAYS_INLINE void FreeSlotBitmapReset(uintptr_t begin_addr,
                                          uintptr_t end_addr) {
  PA_DCHECK(begin_addr <= end_addr);
  auto [begin_cell, begin_bit_index] =
      GetFreeSlotBitmapCellPtrAndBitIndex(begin_addr);
  auto [end_cell, end_bit_index] =
      GetFreeSlotBitmapCellPtrAndBitIndex(end_addr);

  if (begin_cell == end_cell) {
    *begin_cell &= (CellWithTrailingOnes(begin_bit_index) |
                    ~CellWithTrailingOnes(end_bit_index));
    return;
  }

  // The bits that should be marked to 0 are |begin_bit_index|th bit of
  // |begin_cell| to |end_bit_index - 1|th bit of |end_cell|. We can just set
  // all the bits to 0 for the cells between [begin_cell + 1, end_cell). For the
  // |begin_cell| and |end_cell|, we have to handle them separately to only mark
  // the partial bits. | begin_cell |     |...|     | end_cell |
  // |11...100...0|0...0|...|0...0|0...01...1|
  //        ^                           ^
  //        |                           |
  //    begin_addr                   end_addr

  if (begin_bit_index != 0) {
    // Sets [begin_bit_index, kFreeSlotBitmapBitsPerCell) in the begin_cell to 0
    *begin_cell &= CellWithTrailingOnes(begin_bit_index);
    ++begin_cell;
  }

  if (end_bit_index != 0) {
    // Sets [0, end_bit_index) in the end_cell to 0
    *end_cell &= ~CellWithTrailingOnes(end_bit_index);
  }

  for (FreeSlotBitmapCellType* cell = begin_cell; cell != end_cell; ++cell) {
    *cell = static_cast<FreeSlotBitmapCellType>(0);
  }
}

}  // namespace partition_alloc::internal

#endif  // BUILDFLAG(USE_FREESLOT_BITMAP)

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_FREESLOT_BITMAP_H_