// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines some bit utilities.

#ifndef BASE_BITS_H_
#define BASE_BITS_H_

#include <stddef.h>
#include <stdint.h>

#include <bit>
#include <type_traits>

#include "base/check.h"

namespace base::bits {

// Returns true iff |value| is a power of 2.
//
// TODO(https://crbug.com/1414634): Replace with std::has_single_bit().
template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
constexpr bool IsPowerOfTwo(T value) {
  // From "Hacker's Delight": Section 2.1 Manipulating Rightmost Bits.
  //
  // Only positive integers with a single bit set are powers of two. If only one
  // bit is set in x (e.g. 0b00000100000000) then |x-1| will have that bit set
  // to zero and all bits to its right set to 1 (e.g. 0b00000011111111). Hence
  // |x & (x-1)| is 0 iff x is a power of two.
  return value > 0 && (value & (value - 1)) == 0;
}

// Round down |size| to a multiple of alignment, which must be a power of two.
template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
constexpr T AlignDown(T size, T alignment) {
  DCHECK(IsPowerOfTwo(alignment));
  return size & ~(alignment - 1);
}

// Move |ptr| back to the previous multiple of alignment, which must be a power
// of two. Defined for types where sizeof(T) is one byte.
template <typename T, typename = std::enable_if_t<sizeof(T) == 1>>
inline T* AlignDown(T* ptr, uintptr_t alignment) {
  return reinterpret_cast<T*>(
      AlignDown(reinterpret_cast<uintptr_t>(ptr), alignment));
}

// Round up |size| to a multiple of alignment, which must be a power of two.
template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
constexpr T AlignUp(T size, T alignment) {
  DCHECK(IsPowerOfTwo(alignment));
  return (size + alignment - 1) & ~(alignment - 1);
}

// Advance |ptr| to the next multiple of alignment, which must be a power of
// two. Defined for types where sizeof(T) is one byte.
template <typename T, typename = std::enable_if_t<sizeof(T) == 1>>
inline T* AlignUp(T* ptr, uintptr_t alignment) {
  return reinterpret_cast<T*>(
      AlignUp(reinterpret_cast<uintptr_t>(ptr), alignment));
}

// Returns the integer i such as 2^i <= n < 2^(i+1).
//
// There is a common `BitLength` function, which returns the number of bits
// required to represent a value. Rather than implement that function,
// use `Log2Floor` and add 1 to the result.
//
// TODO(https://crbug.com/1414634): Replace with std::bit_xxx().
constexpr int Log2Floor(uint32_t n) {
  return 31 - std::countl_zero(n);
}

// Returns the integer i such as 2^(i-1) < n <= 2^i.
constexpr int Log2Ceiling(uint32_t n) {
  // When n == 0, we want the function to return -1.
  // When n == 0, (n - 1) will underflow to 0xFFFFFFFF, which is
  // why the statement below starts with (n ? 32 : -1).
  return (n ? 32 : -1) - std::countl_zero(n - 1);
}

// Returns a value of type T with a single bit set in the left-most position.
// Can be used instead of manually shifting a 1 to the left.
template <typename T>
constexpr T LeftmostBit() {
  static_assert(std::is_integral_v<T>,
                "This function can only be used with integral types.");
  T one(1u);
  return one << (8 * sizeof(T) - 1);
}

}  // namespace base::bits

#endif  // BASE_BITS_H_
