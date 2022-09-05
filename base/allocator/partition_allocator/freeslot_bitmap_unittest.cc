// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/freeslot_bitmap.h"

#include <cstdint>
#include <limits>

#include "base/allocator/partition_allocator/freeslot_bitmap_constants.h"
#include "base/allocator/partition_allocator/partition_alloc.h"
#include "base/allocator/partition_allocator/partition_alloc_buildflags.h"
#include "base/allocator/partition_allocator/partition_alloc_constants.h"
#include "base/allocator/partition_allocator/partition_alloc_forward.h"
#include "base/allocator/partition_allocator/partition_page.h"
#include "testing/gtest/include/gtest/gtest.h"

// This test is disabled when MEMORY_TOOL_REPLACES_ALLOCATOR is defined because
// we cannot locate the freeslot bitmap address in that case.
#if BUILDFLAG(USE_FREESLOT_BITMAP) && !defined(MEMORY_TOOL_REPLACES_ALLOCATOR)

namespace partition_alloc::internal {

namespace {

class PartitionAllocFreeSlotBitmapTest : public ::testing::Test {
 protected:
  static constexpr FreeSlotBitmapCellType kAllUsed = 0u;
  static constexpr FreeSlotBitmapCellType kAllFree =
      std::numeric_limits<FreeSlotBitmapCellType>::max();

  void SetUp() override {
    // Allocates memory and creates a pseudo superpage in it. We need to
    // allocate |2 * kSuperPageSize| so that a whole superpage is contained in
    // the allocated region.
    allocator_.init({
        PartitionOptions::AlignedAlloc::kDisallowed,
        PartitionOptions::ThreadCache::kDisabled,
        PartitionOptions::Quarantine::kDisallowed,
        PartitionOptions::Cookie::kAllowed,
        PartitionOptions::BackupRefPtr::kDisabled,
        PartitionOptions::BackupRefPtrZapping::kDisabled,
        PartitionOptions::UseConfigurablePool::kNo,
    });
    allocated_ptr_ = reinterpret_cast<uintptr_t>(
        allocator_.root()->Alloc(2 * kSuperPageSize, ""));
    super_page_ = (allocated_ptr_ + kSuperPageSize) & kSuperPageBaseMask;

    // Checks that the whole superpage is in the allocated region.
    PA_DCHECK(super_page_ + kSuperPageSize <=
              allocated_ptr_ + 2 * kSuperPageSize);
  }

  void TearDown() override {
    allocator_.root()->Free(reinterpret_cast<void*>(allocated_ptr_));
  }

  // Returns the |index|-th slot address in the virtual superpage. It assumes
  // that there are no slot spans and the superpage is only filled with the slot
  // of size |kAlignment|.
  uintptr_t SlotAddr(size_t index) {
    return SuperPagePayloadBegin(super_page_, false) + index * kAlignment;
  }

  // Returns the last slot address in the virtual superpage. It assumes that
  // there are no slot spans but the superpage is only filled with the slot of
  // size |kAlignment|.
  uintptr_t LastSlotAddr() {
    return super_page_ + kSuperPageSize - PartitionPageSize() - kAlignment;
  }

 private:
  uintptr_t allocated_ptr_;
  uintptr_t super_page_;
  PartitionAllocator<ThreadSafe> allocator_;
};

}  // namespace

TEST_F(PartitionAllocFreeSlotBitmapTest, MarkFirstSlotAsUsed) {
  uintptr_t slot_addr = SlotAddr(0);
  FreeSlotBitmapMarkSlotAsFree(slot_addr);
  EXPECT_FALSE(FreeSlotBitmapSlotIsUsed(slot_addr));

  FreeSlotBitmapMarkSlotAsUsed(slot_addr);
  EXPECT_TRUE(FreeSlotBitmapSlotIsUsed(slot_addr));
}

TEST_F(PartitionAllocFreeSlotBitmapTest, MarkFirstSlotAsFree) {
  uintptr_t slot_addr = SlotAddr(0);
  // All slots are set to "used" by default.
  EXPECT_TRUE(FreeSlotBitmapSlotIsUsed(slot_addr));

  FreeSlotBitmapMarkSlotAsFree(slot_addr);
  EXPECT_FALSE(FreeSlotBitmapSlotIsUsed(slot_addr));
}

TEST_F(PartitionAllocFreeSlotBitmapTest, MarkAllBitsInCellAsUsed) {
  const size_t kFirstSlotAddr = SlotAddr(0);
  const size_t kLastSlotAddr = SlotAddr(kFreeSlotBitmapBitsPerCell);

  auto [cell_first_slot, bit_index_first_slot] =
      GetFreeSlotBitmapCellPtrAndBitIndex(kFirstSlotAddr);
  auto [cell_last_slot, bit_index_last_slot] =
      GetFreeSlotBitmapCellPtrAndBitIndex(kLastSlotAddr);

  // Check that the bit corresponding to |kFirstSlotAddr| is the first bit in
  // some cell (= |cell_first_slot|), and the bit for |kLastSlotAddr| is the
  // first bit in the next cell. This means that we are manipulating all the
  // bits in |cell_first_slot| in this test.
  EXPECT_EQ(0u, bit_index_first_slot);
  EXPECT_EQ(0u, bit_index_last_slot);
  EXPECT_NE(cell_first_slot, cell_last_slot);

  for (size_t slot_addr = kFirstSlotAddr; slot_addr < kLastSlotAddr;
       slot_addr += kAlignment) {
    FreeSlotBitmapMarkSlotAsFree(slot_addr);
  }

  // Check all the bits in |cell_first_slot| are 1 (= free).
  EXPECT_EQ(kAllFree, *cell_first_slot);

  for (size_t slot_addr = kFirstSlotAddr; slot_addr < kLastSlotAddr;
       slot_addr += kAlignment) {
    FreeSlotBitmapMarkSlotAsUsed(slot_addr);
  }

  // Check all the bits in |cell_first_slot| are 0 (= used).
  EXPECT_EQ(kAllUsed, *cell_first_slot);
}

TEST_F(PartitionAllocFreeSlotBitmapTest, MarkLastSlotAsUsed) {
  uintptr_t last_slot_addr = LastSlotAddr();
  FreeSlotBitmapMarkSlotAsFree(last_slot_addr);
  EXPECT_FALSE(FreeSlotBitmapSlotIsUsed(last_slot_addr));

  FreeSlotBitmapMarkSlotAsUsed(last_slot_addr);
  EXPECT_TRUE(FreeSlotBitmapSlotIsUsed(last_slot_addr));
}

TEST_F(PartitionAllocFreeSlotBitmapTest, ResetBitmap) {
  const size_t kNumSlots = 3 * kFreeSlotBitmapBitsPerCell;
  for (size_t i = 0; i < kNumSlots; ++i) {
    FreeSlotBitmapMarkSlotAsFree(SlotAddr(i));
  }

  auto [cell_first_slot, bit_index_first_slot] =
      GetFreeSlotBitmapCellPtrAndBitIndex(SlotAddr(0));
  EXPECT_EQ(0u, bit_index_first_slot);
  EXPECT_EQ(kAllFree, *cell_first_slot);
  EXPECT_EQ(kAllFree, *(cell_first_slot + 1));
  EXPECT_EQ(kAllFree, *(cell_first_slot + 2));

  FreeSlotBitmapReset(SlotAddr(kFreeSlotBitmapBitsPerCell),
                      SlotAddr(2 * kFreeSlotBitmapBitsPerCell));
  EXPECT_EQ(kAllFree, *cell_first_slot);
  EXPECT_EQ(kAllUsed, *(cell_first_slot + 1));
  EXPECT_EQ(kAllFree, *(cell_first_slot + 2));
}

TEST_F(PartitionAllocFreeSlotBitmapTest, ResetBitmapWithinSingleCell) {
  const size_t kNumSlots = kFreeSlotBitmapBitsPerCell;
  const size_t kResetStart = 0.25 * kFreeSlotBitmapBitsPerCell;
  const size_t kResetEnd = 0.5 * kFreeSlotBitmapBitsPerCell;
  for (size_t i = 0; i < kNumSlots; ++i) {
    FreeSlotBitmapMarkSlotAsFree(SlotAddr(i));
  }

  auto [cell, bit_index] = GetFreeSlotBitmapCellPtrAndBitIndex(SlotAddr(0));
  EXPECT_EQ(0u, bit_index);
  EXPECT_EQ(kAllFree, *cell);

  FreeSlotBitmapReset(SlotAddr(kResetStart), SlotAddr(kResetEnd));
  for (size_t i = 0; i < kNumSlots; ++i) {
    if (kResetStart <= i & i < kResetEnd) {
      EXPECT_TRUE(FreeSlotBitmapSlotIsUsed(SlotAddr(i)));
    } else {
      EXPECT_FALSE(FreeSlotBitmapSlotIsUsed(SlotAddr(i)));
    }
  }
}

TEST_F(PartitionAllocFreeSlotBitmapTest, ResetBitmapIncludingPartialCells) {
  const size_t kNumSlots = 3 * kFreeSlotBitmapBitsPerCell;
  const size_t kResetStart = 0.5 * kFreeSlotBitmapBitsPerCell;
  const size_t kResetEnd = 2.5 * kFreeSlotBitmapBitsPerCell;
  for (size_t i = 0; i < kNumSlots; ++i) {
    FreeSlotBitmapMarkSlotAsFree(SlotAddr(i));
  }

  auto [cell_first_slot, bit_index_first_slot] =
      GetFreeSlotBitmapCellPtrAndBitIndex(SlotAddr(0));
  EXPECT_EQ(0u, bit_index_first_slot);
  EXPECT_EQ(kAllFree, *cell_first_slot);
  EXPECT_EQ(kAllFree, *(cell_first_slot + 1));
  EXPECT_EQ(kAllFree, *(cell_first_slot + 2));

  FreeSlotBitmapReset(SlotAddr(kResetStart), SlotAddr(kResetEnd));
  for (size_t i = 0; i < kNumSlots; ++i) {
    if (kResetStart <= i & i < kResetEnd) {
      EXPECT_TRUE(FreeSlotBitmapSlotIsUsed(SlotAddr(i)));
    } else {
      EXPECT_FALSE(FreeSlotBitmapSlotIsUsed(SlotAddr(i)));
    }
  }
}

}  // namespace partition_alloc::internal

#endif  // BUILDFLAG(USE_FREESLOT_BITMAP) &&
        // !defined(MEMORY_TOOL_REPLACES_ALLOCATOR)
