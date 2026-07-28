// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines some bit utilities.

#ifndef PARTITION_ALLOC_PARTITION_ALLOC_BASE_BITS_H_
#define PARTITION_ALLOC_PARTITION_ALLOC_BASE_BITS_H_

#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>

#include "partition_alloc/build_config.h"
#include "partition_alloc/partition_alloc_base/check.h"
#include "partition_alloc/partition_alloc_base/compiler_specific.h"

namespace partition_alloc::internal::base::bits {

// Round down |size| to a multiple of alignment, which must be a power of two.
template <typename T>
inline constexpr T AlignDown(T size, T alignment) {
  static_assert(std::is_unsigned_v<T>);
  PA_BASE_DCHECK(std::has_single_bit(alignment));
  return size & ~(alignment - 1);
}

// Move |ptr| back to the previous multiple of alignment, which must be a power
// of two. Defined for types where sizeof(T) is one byte.
template <typename T>
inline T* AlignDown(T* ptr, size_t alignment) {
  return reinterpret_cast<T*>(
      AlignDown(reinterpret_cast<uintptr_t>(ptr), alignment));
}

// Round up |size| to a multiple of alignment, which must be a power of two.
template <typename T>
inline constexpr T AlignUp(T size, T alignment) {
  static_assert(std::is_unsigned_v<T>);
  PA_BASE_DCHECK(std::has_single_bit(alignment));
  return (size + alignment - 1) & ~(alignment - 1);
}

// Advance |ptr| to the next multiple of alignment, which must be a power of
// two. Defined for types where sizeof(T) is one byte.
template <typename T>
inline T* AlignUp(T* ptr, size_t alignment) {
  return reinterpret_cast<T*>(
      AlignUp(reinterpret_cast<size_t>(ptr), alignment));
}

// Returns the integer i such as 2^(i-1) < n <= 2^i.
constexpr int Log2Ceiling(uint32_t n) {
  // When n == 0, we want the function to return -1.
  // When n == 0, (n - 1) will underflow to 0xFFFFFFFF, which is
  // why the statement below starts with (n ? 32 : -1).
  return (n ? 32 : -1) - std::countl_zero(n - 1);
}

// Computes the result of bitwise left-rotating the value of x by s positions.
template <class T>
PA_ALWAYS_INLINE constexpr T RotR(T x, T s) {
  static_assert(std::is_unsigned_v<T>);
  constexpr int n = std::numeric_limits<T>::digits;
  static_assert(n == 32 || n == 64);

#if PA_HAS_BUILTIN(__builtin_rotateright32) && \
    PA_HAS_BUILTIN(__builtin_rotateright64)
  if constexpr (n == 32) {
    return __builtin_rotateright32(x, s);
  }
  return __builtin_rotateright64(x, s);
#else
  T r = s % n;
  if (r == 0) {
    return x;
  }
  return (x >> r) | (x << (n - r));
#endif  // PA_HAS_BUILTIN(__builtin_rotateright32) &&
        // PA_HAS_BUILTIN(__builtin_rotateright64)
}

}  // namespace partition_alloc::internal::base::bits

#endif  // PARTITION_ALLOC_PARTITION_ALLOC_BASE_BITS_H_
