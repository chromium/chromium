// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_STARSCAN_STATE_BITMAP_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_STARSCAN_STATE_BITMAP_H_

#include <climits>
#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <array>
#include <atomic>
#include <tuple>

#include "base/allocator/partition_allocator/partition_alloc_check.h"
#include "base/bits.h"
#include "base/compiler_specific.h"

namespace base {
namespace internal {

// Bitmap which tracks allocation states. An allocation can be in one of 3
// states:
// - freed (00),
// - allocated (11),
// - quarantined (01 or 10, depending on the *Scan epoch).
//
// The state machine of allocation states:
//         +-------------+                +-------------+
//         |             |    malloc()    |             |
//         |    Freed    +--------------->|  Allocated  |
//         |    (00)     |    (or 11)     |    (11)     |
//         |             |                |             |
//         +-------------+                +------+------+
//                ^                              |
//                |                              |
//    real_free() | (and 00)              free() | (and 01(10))
//                |                              |
//                |       +-------------+        |
//                |       |             |        |
//                +-------+ Quarantined |<-------+
//                        |   (01,10)   |
//                        |             |
//                        +-------------+
//                         ^           |
//                         |  mark()   |
//                         +-----------+
//                           (xor 11)
//
// The bitmap can be safely accessed from multiple threads, but this doesn't
// imply visibility on the data (i.e. no ordering guaranties, since relaxed
// atomics are used underneath). The bitmap itself must be created inside a
// page, size and alignment of which are specified as template arguments
// |PageSize| and |PageAlignment|. |AllocationAlignment| specifies the minimal
// alignment of objects that are allocated inside a page (serves as the
// granularity in the bitmap).
template <size_t PageSize, size_t PageAlignment, size_t AllocationAlignment>
class StateBitmap final {
  enum class State : uint8_t {
    kFreed = 0b00,
    kQuarantined1 = 0b01,
    kQuarantined2 = 0b10,
    kAlloced = 0b11,
    kNumOfStates = 4,
  };

  using CellType = uintptr_t;
  static constexpr size_t kBitsPerCell = sizeof(CellType) * CHAR_BIT;
  static constexpr size_t kBitsNeededForAllocation =
      bits::Log2Floor(static_cast<size_t>(State::kNumOfStates));
  static constexpr CellType kStateMask = (1 << kBitsNeededForAllocation) - 1;

  static constexpr size_t kBitmapSize =
      (PageSize + ((kBitsPerCell * AllocationAlignment) - 1)) /
      (kBitsPerCell * AllocationAlignment) * kBitsNeededForAllocation;
  static constexpr size_t kPageOffsetMask = PageAlignment - 1;
  static constexpr size_t kPageBaseMask = ~kPageOffsetMask;

 public:
  using Epoch = size_t;

  static constexpr size_t kPageSize = PageSize;
  static constexpr size_t kPageAlignment = PageAlignment;
  static constexpr size_t kAllocationAlignment = AllocationAlignment;
  static constexpr size_t kMaxEntries =
      (kBitmapSize / kBitsNeededForAllocation) * kBitsPerCell;

  inline StateBitmap();

  // Sets the bits corresponding to |address| as allocated.
  ALWAYS_INLINE void Allocate(uintptr_t address);

  // Sets the bits corresponding to |address| as quarantined. Must be called
  // only once, in which case returns |true|. Otherwise, if the object was
  // already quarantined or freed before, returns |false|.
  ALWAYS_INLINE bool Quarantine(uintptr_t address, Epoch epoch);

  // Marks ("promotes") quarantined object.
  ALWAYS_INLINE void MarkQuarantined(uintptr_t address);

  // Sets the bits corresponding to |address| as freed.
  ALWAYS_INLINE void Free(uintptr_t address);

  // Getters that check object state.
  ALWAYS_INLINE bool IsAllocated(uintptr_t address) const;
  ALWAYS_INLINE bool IsQuarantined(uintptr_t address) const;
  ALWAYS_INLINE bool IsFreed(uintptr_t address) const;

  // Iterate objects depending on their state.
  //
  // The callback is of type
  //   void(Address)
  // and is passed the object address as parameter.
  template <typename Callback>
  inline void IterateAllocated(Callback) const;
  template <typename Callback>
  inline void IterateQuarantined(Callback) const;
  template <typename Callback>
  inline void IterateUnmarkedQuarantined(size_t epoch, Callback) const;

  inline void Clear();

 private:
  std::atomic<CellType>& AsAtomicCell(size_t cell_index) {
    return reinterpret_cast<std::atomic<CellType>&>(bitmap_[cell_index]);
  }
  const std::atomic<CellType>& AsAtomicCell(size_t cell_index) const {
    return reinterpret_cast<const std::atomic<CellType>&>(bitmap_[cell_index]);
  }

  ALWAYS_INLINE unsigned GetBits(uintptr_t address) const;

  struct FilterQuarantine {
    ALWAYS_INLINE bool operator()(CellType cell) const;
    const size_t epoch;
  };

  struct FilterUnmarkedQuarantine {
    ALWAYS_INLINE bool operator()(CellType cell) const;
    const size_t epoch;
  };

  struct FilterAllocated {
    ALWAYS_INLINE bool operator()(CellType cell) const;
    const size_t epoch;
  };

  template <typename Filter, typename Callback>
  inline void IterateImpl(size_t epoch, Callback);

  ALWAYS_INLINE CellType LoadCell(size_t cell_index) const;
  ALWAYS_INLINE static constexpr std::pair<size_t, size_t>
      AllocationIndexAndBit(uintptr_t);

  std::array<CellType, kBitmapSize> bitmap_;
};

// The constructor can be omitted, but the Chromium's clang plugin wrongly
// warns that the type is not trivially constructible.
template <size_t PageSize, size_t PageAlignment, size_t AllocationAlignment>
inline StateBitmap<PageSize, PageAlignment, AllocationAlignment>::
    StateBitmap() = default;

template <size_t PageSize, size_t PageAlignment, size_t AllocationAlignment>
ALWAYS_INLINE void
StateBitmap<PageSize, PageAlignment, AllocationAlignment>::Allocate(
    uintptr_t address) {
  PA_DCHECK(IsFreed(address));
  size_t cell_index, object_bit;
  std::tie(cell_index, object_bit) = AllocationIndexAndBit(address);
  const CellType mask = static_cast<CellType>(State::kAlloced) << object_bit;
  auto& cell = AsAtomicCell(cell_index);
  cell.fetch_or(mask, std::memory_order_relaxed);
}

template <size_t PageSize, size_t PageAlignment, size_t AllocationAlignment>
ALWAYS_INLINE bool
StateBitmap<PageSize, PageAlignment, AllocationAlignment>::Quarantine(
    uintptr_t address,
    Epoch epoch) {
  // *Scan is enabled at runtime, which means that we can quarantine allocation,
  // that was previously not recorded in the bitmap. Hence, we can't reliably
  // check transition from kAlloced to kQuarantined.
  static_assert((~static_cast<CellType>(State::kQuarantined1) & kStateMask) ==
                    (static_cast<CellType>(State::kQuarantined2) & kStateMask),
                "kQuarantined1 must be inverted kQuarantined2");
  const State quarantine_state =
      epoch & 0b1 ? State::kQuarantined1 : State::kQuarantined2;
  size_t cell_index, object_bit;
  std::tie(cell_index, object_bit) = AllocationIndexAndBit(address);
  const CellType mask =
      ~(static_cast<CellType>(quarantine_state) << object_bit);
  auto& cell = AsAtomicCell(cell_index);
  const CellType cell_before = cell.fetch_and(mask, std::memory_order_relaxed);
  // Check if the previous state was also quarantined.
  return __builtin_popcount(static_cast<unsigned>((cell_before >> object_bit) &
                                                  kStateMask)) != 1;
}

template <size_t PageSize, size_t PageAlignment, size_t AllocationAlignment>
ALWAYS_INLINE void
StateBitmap<PageSize, PageAlignment, AllocationAlignment>::MarkQuarantined(
    uintptr_t address) {
  PA_DCHECK(IsQuarantined(address));
  static_assert((~static_cast<CellType>(State::kQuarantined1) & kStateMask) ==
                    (static_cast<CellType>(State::kQuarantined2) & kStateMask),
                "kQuarantined1 must be inverted kQuarantined2");
  static constexpr CellType kXorMask = 0b11;
  size_t cell_index, object_bit;
  std::tie(cell_index, object_bit) = AllocationIndexAndBit(address);
  const CellType mask = kXorMask << object_bit;
  auto& cell = AsAtomicCell(cell_index);
  cell.fetch_xor(mask, std::memory_order_relaxed);
}

template <size_t PageSize, size_t PageAlignment, size_t AllocationAlignment>
ALWAYS_INLINE void
StateBitmap<PageSize, PageAlignment, AllocationAlignment>::Free(
    uintptr_t address) {
  PA_DCHECK(IsQuarantined(address));
  static_assert((~static_cast<CellType>(State::kAlloced) & kStateMask) ==
                    (static_cast<CellType>(State::kFreed) & kStateMask),
                "kFreed must be inverted kAlloced");
  size_t cell_index, object_bit;
  std::tie(cell_index, object_bit) = AllocationIndexAndBit(address);
  const CellType mask = ~(static_cast<CellType>(State::kAlloced) << object_bit);
  auto& cell = AsAtomicCell(cell_index);
  cell.fetch_and(mask, std::memory_order_relaxed);
}

template <size_t PageSize, size_t PageAlignment, size_t AllocationAlignment>
ALWAYS_INLINE bool
StateBitmap<PageSize, PageAlignment, AllocationAlignment>::IsAllocated(
    uintptr_t address) const {
  return GetBits(address) == static_cast<unsigned>(State::kAlloced);
}

template <size_t PageSize, size_t PageAlignment, size_t AllocationAlignment>
ALWAYS_INLINE bool
StateBitmap<PageSize, PageAlignment, AllocationAlignment>::IsQuarantined(
    uintptr_t address) const {
  // On x86 CPI of popcnt is the same as tzcnt, so we use it instead of tzcnt +
  // inversion.
  return __builtin_popcount(GetBits(address)) == 1;
}

template <size_t PageSize, size_t PageAlignment, size_t AllocationAlignment>
ALWAYS_INLINE bool
StateBitmap<PageSize, PageAlignment, AllocationAlignment>::IsFreed(
    uintptr_t address) const {
  return GetBits(address) == static_cast<unsigned>(State::kFreed);
}

template <size_t PageSize, size_t PageAlignment, size_t AllocationAlignment>
ALWAYS_INLINE
    typename StateBitmap<PageSize, PageAlignment, AllocationAlignment>::CellType
    StateBitmap<PageSize, PageAlignment, AllocationAlignment>::LoadCell(
        size_t cell_index) const {
  return AsAtomicCell(cell_index).load(std::memory_order_relaxed);
}

template <size_t PageSize, size_t PageAlignment, size_t AllocationAlignment>
ALWAYS_INLINE constexpr std::pair<size_t, size_t>
StateBitmap<PageSize, PageAlignment, AllocationAlignment>::
    AllocationIndexAndBit(uintptr_t address) {
  const uintptr_t offset_in_page = address & kPageOffsetMask;
  const size_t allocation_number =
      (offset_in_page / kAllocationAlignment) * kBitsNeededForAllocation;
  const size_t cell_index = allocation_number / kBitsPerCell;
  PA_DCHECK(kBitmapSize > cell_index);
  const size_t bit = allocation_number % kBitsPerCell;
  return {cell_index, bit};
}

template <size_t PageSize, size_t PageAlignment, size_t AllocationAlignment>
unsigned StateBitmap<PageSize, PageAlignment, AllocationAlignment>::GetBits(
    uintptr_t address) const {
  size_t cell_index, object_bit;
  std::tie(cell_index, object_bit) = AllocationIndexAndBit(address);
  return (LoadCell(cell_index) >> object_bit) & kStateMask;
}

template <size_t PageSize, size_t PageAlignment, size_t AllocationAlignment>
bool StateBitmap<PageSize, PageAlignment, AllocationAlignment>::
    FilterQuarantine::operator()(CellType bits) const {
  return __builtin_popcount(bits) == 1;
}

template <size_t PageSize, size_t PageAlignment, size_t AllocationAlignment>
bool StateBitmap<PageSize, PageAlignment, AllocationAlignment>::
    FilterUnmarkedQuarantine::operator()(CellType bits) const {
  // Truth table:
  // epoch & 1 | bits | result
  //     0     |  01  |   1
  //     1     |  10  |   1
  //     *     |  **  |   0
  return bits - (epoch & 0b01) == 0b01;
}

template <size_t PageSize, size_t PageAlignment, size_t AllocationAlignment>
bool StateBitmap<PageSize, PageAlignment, AllocationAlignment>::
    FilterAllocated::operator()(CellType bits) const {
  return bits == 0b11;
}

template <size_t PageSize, size_t PageAlignment, size_t AllocationAlignment>
template <typename Filter, typename Callback>
inline void
StateBitmap<PageSize, PageAlignment, AllocationAlignment>::IterateImpl(
    size_t epoch,
    Callback callback) {
  // The bitmap (|this|) is allocated inside the page with |kPageAlignment|.
  Filter filter{epoch};
  const uintptr_t base = reinterpret_cast<uintptr_t>(this) & kPageBaseMask;
  for (size_t cell_index = 0; cell_index < kBitmapSize; ++cell_index) {
    CellType value = LoadCell(cell_index);
    while (value) {
      const size_t trailing_zeroes =
          (base::bits::CountTrailingZeroBits(value) & ~0b1);
      const size_t clear_value_mask =
          ~(static_cast<CellType>(kStateMask) << trailing_zeroes);
      if (!filter((value >> trailing_zeroes) & kStateMask)) {
        // Clear current object bit in temporary value to advance iteration.
        value &= clear_value_mask;
        continue;
      }
      const size_t object_number =
          (cell_index * kBitsPerCell) + trailing_zeroes;
      const uintptr_t object_address =
          base +
          (object_number * kAllocationAlignment / kBitsNeededForAllocation);
      callback(object_address);
      // Clear current object bit in temporary value to advance iteration.
      value &= clear_value_mask;
    }
  }
}

template <size_t PageSize, size_t PageAlignment, size_t AllocationAlignment>
template <typename Callback>
inline void
StateBitmap<PageSize, PageAlignment, AllocationAlignment>::IterateAllocated(
    Callback callback) const {
  const_cast<StateBitmap&>(*this)
      .template IterateImpl<FilterAllocated, Callback>(0, std::move(callback));
}

template <size_t PageSize, size_t PageAlignment, size_t AllocationAlignment>
template <typename Callback>
inline void
StateBitmap<PageSize, PageAlignment, AllocationAlignment>::IterateQuarantined(
    Callback callback) const {
  const_cast<StateBitmap&>(*this)
      .template IterateImpl<FilterQuarantine, Callback>(0, std::move(callback));
}

template <size_t PageSize, size_t PageAlignment, size_t AllocationAlignment>
template <typename Callback>
inline void StateBitmap<PageSize, PageAlignment, AllocationAlignment>::
    IterateUnmarkedQuarantined(size_t epoch, Callback callback) const {
  const_cast<StateBitmap&>(*this)
      .template IterateImpl<FilterUnmarkedQuarantine, Callback>(
          epoch, std::move(callback));
}

template <size_t PageSize, size_t PageAlignment, size_t AllocationAlignment>
void StateBitmap<PageSize, PageAlignment, AllocationAlignment>::Clear() {
  std::fill(bitmap_.begin(), bitmap_.end(), '\0');
}

}  // namespace internal
}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_STARSCAN_STATE_BITMAP_H_
