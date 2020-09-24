// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_OBJECT_BITMAP_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_OBJECT_BITMAP_H_

#include <climits>
#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <array>
#include <atomic>
#include <tuple>

#include "base/allocator/partition_allocator/partition_alloc_check.h"
#include "base/bits.h"

namespace base {
namespace internal {

// Bitmap which tracks beginning of allocated objects. The bitmap can be safely
// accessed from multiple threads, but this doesn't imply visibility on the data
// (i.e. no ordering guaranties, since relaxed atomics are used underneath). The
// bitmap itself must be created inside a page, size and alignment of which are
// specified as template arguments |PageSize| and |PageAlignment|.
// |ObjectAlignment| specifies the minimal alignment of objects that are
// allocated inside a page (serves as the granularity in the bitmap).
template <size_t PageSize, size_t PageAlignment, size_t ObjectAlignment>
class ObjectBitmap final {
  static constexpr size_t kBitsPerCell = sizeof(uint8_t) * CHAR_BIT;
  static constexpr size_t kBitmapSize =
      (PageSize + ((kBitsPerCell * ObjectAlignment) - 1)) /
      (kBitsPerCell * ObjectAlignment);
  static constexpr size_t kPageOffsetMask = PageAlignment - 1;
  static constexpr size_t kPageBaseMask = ~kPageOffsetMask;

 public:
  static constexpr size_t kPageSize = PageSize;
  static constexpr size_t kPageAlignment = PageAlignment;
  static constexpr size_t kObjectAlignment = ObjectAlignment;
  static constexpr size_t kMaxEntries = kBitmapSize * kBitsPerCell;
  static constexpr uintptr_t kSentinel = 0u;

  inline ObjectBitmap();

  // Finds the beginning of the closest object that starts at or before
  // |address|. It may return an object from another slot if the slot where
  // |address| lies in is unallocated. The caller is responsible for range
  // checking. Returns |kSentinel| if no object was found.
  inline uintptr_t FindPotentialObjectBeginning(uintptr_t address) const;

  inline void SetBit(uintptr_t address);
  inline void ClearBit(uintptr_t address);
  inline bool CheckBit(uintptr_t address) const;

  // Iterates all objects recorded in the bitmap.
  //
  // The callback is of type
  //   void(Address)
  // and is passed the object address as parameter.
  template <typename Callback>
  inline void Iterate(Callback) const;

  inline void Clear();

 private:
  std::atomic<uint8_t>& AsAtomicCell(size_t cell_index) {
    return reinterpret_cast<std::atomic<uint8_t>&>(bitmap_[cell_index]);
  }
  const std::atomic<uint8_t>& AsAtomicCell(size_t cell_index) const {
    return reinterpret_cast<const std::atomic<uint8_t>&>(bitmap_[cell_index]);
  }

  inline uint8_t LoadCell(size_t cell_index) const;
  inline std::pair<size_t, size_t> ObjectIndexAndBit(uintptr_t) const;

  std::array<uint8_t, kBitmapSize> bitmap_;
};

template <size_t PageSize, size_t PageAlignment, size_t ObjectAlignment>
constexpr size_t
    ObjectBitmap<PageSize, PageAlignment, ObjectAlignment>::kSentinel;

// The constructor can be omitted, but the Chromium's clang plugin wrongly
// warns that the type is not trivially constructible.
template <size_t PageSize, size_t PageAlignment, size_t ObjectAlignment>
inline ObjectBitmap<PageSize, PageAlignment, ObjectAlignment>::ObjectBitmap() =
    default;

template <size_t PageSize, size_t PageAlignment, size_t ObjectAlignment>
uintptr_t ObjectBitmap<PageSize, PageAlignment, ObjectAlignment>::
    FindPotentialObjectBeginning(uintptr_t address) const {
  const uintptr_t page_base = reinterpret_cast<uintptr_t>(this) & kPageBaseMask;
  PA_DCHECK(page_base <= address && address < page_base + kPageSize);

  size_t cell_index, bit;
  std::tie(cell_index, bit) = ObjectIndexAndBit(address);

  // Find the first set bit at or before |bit|.
  uint8_t byte = LoadCell(cell_index) & ((1 << (bit + 1)) - 1);
  while (!byte && cell_index) {
    PA_DCHECK(0u < cell_index);
    byte = LoadCell(--cell_index);
  }

  if (!byte) {
    // No object was found.
    return kSentinel;
  }

  const int leading_zeroes = base::bits::CountLeadingZeroBits(byte);
  const size_t object_number =
      (cell_index * kBitsPerCell) + (kBitsPerCell - 1) - leading_zeroes;
  const size_t offset_in_page = object_number * kObjectAlignment;

  return offset_in_page + page_base;
}

template <size_t PageSize, size_t PageAlignment, size_t ObjectAlignment>
void ObjectBitmap<PageSize, PageAlignment, ObjectAlignment>::SetBit(
    uintptr_t address) {
  size_t cell_index, object_bit;
  std::tie(cell_index, object_bit) = ObjectIndexAndBit(address);
  auto& cell = AsAtomicCell(cell_index);
  cell.fetch_or(1 << object_bit, std::memory_order_relaxed);
}

template <size_t PageSize, size_t PageAlignment, size_t ObjectAlignment>
void ObjectBitmap<PageSize, PageAlignment, ObjectAlignment>::ClearBit(
    uintptr_t address) {
  size_t cell_index, object_bit;
  std::tie(cell_index, object_bit) = ObjectIndexAndBit(address);
  auto& cell = AsAtomicCell(cell_index);
  cell.fetch_and(~(1 << object_bit), std::memory_order_relaxed);
}

template <size_t PageSize, size_t PageAlignment, size_t ObjectAlignment>
bool ObjectBitmap<PageSize, PageAlignment, ObjectAlignment>::CheckBit(
    uintptr_t address) const {
  size_t cell_index, object_bit;
  std::tie(cell_index, object_bit) = ObjectIndexAndBit(address);
  return LoadCell(cell_index) & (1 << object_bit);
}

template <size_t PageSize, size_t PageAlignment, size_t ObjectAlignment>
uint8_t ObjectBitmap<PageSize, PageAlignment, ObjectAlignment>::LoadCell(
    size_t cell_index) const {
  return AsAtomicCell(cell_index).load(std::memory_order_relaxed);
}

template <size_t PageSize, size_t PageAlignment, size_t ObjectAlignment>
std::pair<size_t, size_t>
ObjectBitmap<PageSize, PageAlignment, ObjectAlignment>::ObjectIndexAndBit(
    uintptr_t address) const {
  const uintptr_t offset_in_page = address & kPageOffsetMask;
  PA_DCHECK(!(offset_in_page % kObjectAlignment));
  const size_t object_number = offset_in_page / kObjectAlignment;
  const size_t cell_index = object_number / kBitsPerCell;
  PA_DCHECK(kBitmapSize > cell_index);
  const size_t bit = object_number % kBitsPerCell;
  return {cell_index, bit};
}

template <size_t PageSize, size_t PageAlignment, size_t ObjectAlignment>
template <typename Callback>
inline void ObjectBitmap<PageSize, PageAlignment, ObjectAlignment>::Iterate(
    Callback callback) const {
  // The bitmap (|this|) is allocated inside the page with |kPageAlignment|.
  const uintptr_t base = reinterpret_cast<uintptr_t>(this) & kPageBaseMask;
  for (size_t cell_index = 0; cell_index < kBitmapSize; ++cell_index) {
    uint8_t value = LoadCell(cell_index);
    while (value) {
      const int trailing_zeroes = base::bits::CountTrailingZeroBits(value);
      const size_t object_number =
          (cell_index * kBitsPerCell) + trailing_zeroes;
      const uintptr_t object_address =
          base + (kObjectAlignment * object_number);
      callback(object_address);
      // Clear current object bit in temporary value to advance iteration.
      value &= ~(1 << trailing_zeroes);
    }
  }
}

template <size_t PageSize, size_t PageAlignment, size_t ObjectAlignment>
void ObjectBitmap<PageSize, PageAlignment, ObjectAlignment>::Clear() {
  std::fill(bitmap_.begin(), bitmap_.end(), '\0');
}

}  // namespace internal
}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_OBJECT_BITMAP_H_
