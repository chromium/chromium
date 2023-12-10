// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines some bit utilities.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_PARTITION_ALLOC_BASE_BITS_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_PARTITION_ALLOC_BASE_BITS_H_

#include <stddef.h>
#include <stdint.h>

#include <bit>
#include <concepts>

#include "partition_alloc/partition_alloc_base/check.h"

namespace partition_alloc::internal::base::bits {

// Bit functions in <bit> are restricted to a specific set of types of unsigned
// integer; restrict functions in this file that are related to those in that
// header to match for consistency.
template <typename T>
concept UnsignedInteger =
    std::unsigned_integral<T> && !std::same_as<T, bool> &&
    !std::same_as<T, char> && !std::same_as<T, char8_t> &&
    !std::same_as<T, char16_t> && !std::same_as<T, char32_t> &&
    !std::same_as<T, wchar_t>;

// Round down |size| to a multiple of alignment, which must be a power of two.
template <typename T>
  requires UnsignedInteger<T>
inline constexpr T AlignDown(T size, T alignment) {
  PA_BASE_DCHECK(std::has_single_bit(alignment));
  return size & ~(alignment - 1);
}

// Move |ptr| back to the previous multiple of alignment, which must be a power
// of two. Defined for types where sizeof(T) is one byte.
template <typename T>
  requires(sizeof(T) == 1)
inline T* AlignDown(T* ptr, size_t alignment) {
  return reinterpret_cast<T*>(
      AlignDown(reinterpret_cast<uintptr_t>(ptr), alignment));
}

// Round up |size| to a multiple of alignment, which must be a power of two.
template <typename T>
  requires UnsignedInteger<T>
inline constexpr T AlignUp(T size, T alignment) {
  PA_BASE_DCHECK(std::has_single_bit(alignment));
  return (size + alignment - 1) & ~(alignment - 1);
}

// Advance |ptr| to the next multiple of alignment, which must be a power of
// two. Defined for types where sizeof(T) is one byte.
template <typename T>
  requires(sizeof(T) == 1)
inline T* AlignUp(T* ptr, size_t alignment) {
  return reinterpret_cast<T*>(
      AlignUp(reinterpret_cast<size_t>(ptr), alignment));
}

// Returns the integer i such as 2^i <= n < 2^(i+1).
//
// A common use for this function is to measure the number of bits required to
// contain a value; for that case use std::bit_width().
//
// A common use for this function is to take its result and use it to left-shift
// a bit; instead of doing so, use std::bit_floor().
constexpr int Log2Floor(uint32_t n) {
  return 31 - std::countl_zero(n);
}

// Returns the integer i such as 2^(i-1) < n <= 2^i.
//
// A common use for this function is to measure the number of bits required to
// contain a value; for that case use std::bit_width().
//
// A common use for this function is to take its result and use it to left-shift
// a bit; instead of doing so, use std::bit_ceil().
constexpr int Log2Ceiling(uint32_t n) {
  // When n == 0, we want the function to return -1.
  // When n == 0, (n - 1) will underflow to 0xFFFFFFFF, which is
  // why the statement below starts with (n ? 32 : -1).
  return (n ? 32 : -1) - std::countl_zero(n - 1);
}

// Returns a value of type T with a single bit set in the left-most position.
// Can be used instead of manually shifting a 1 to the left. Unlike the other
// functions in this file, usable for any integral type.
template <typename T>
  requires std::integral<T>
constexpr T LeftmostBit() {
  T one(1u);
  return one << (8 * sizeof(T) - 1);
}

}  // namespace partition_alloc::internal::base::bits

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_PARTITION_ALLOC_BASE_BITS_H_
