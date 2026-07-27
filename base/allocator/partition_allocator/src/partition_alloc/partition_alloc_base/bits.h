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
#include "partition_alloc/partition_alloc_base/notreached.h"

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

// Backport of C++20 std::countl_zero in <bit>.
//
// CountlZero(value) returns the number of zero bits following the
// most significant 1 bit in |value| if |value| is non-zero, otherwise it
// returns {sizeof(T) * 8}.
// Example: 00100010 -> 2
//
// C does not have an operator to do this, but fortunately the various
// compilers have built-ins that map to fast underlying processor instructions.
// __builtin_clz has undefined behaviour for an input of 0, even though there's
// clearly a return value that makes sense, and even though some processor clz
// instructions have defined behaviour for 0. We could drop to raw __asm__ to
// do better, but we'll avoid doing that unless we see proof that we need to.
template <typename T, int bits = sizeof(T) * 8>
PA_ALWAYS_INLINE constexpr
    typename std::enable_if<std::is_unsigned_v<T> && sizeof(T) <= 8, int>::type
    CountlZero(T value) {
  static_assert(bits > 0, "invalid instantiation");
  if (value) [[likely]] {
#if PA_BUILDFLAG(PA_COMPILER_MSVC) && !defined(__clang__)
    // We would prefer to use the _BitScanReverse(64) intrinsics, but they
    // aren't constexpr and thus unusable here.
    int leading_zeros = 0;
    constexpr T kMostSignificantBitMask = 1ull << (bits - 1);
    for (; !(value & kMostSignificantBitMask); value <<= 1, ++leading_zeros) {
    }
    return leading_zeros;
#else
    return bits == 64
               ? __builtin_clzll(static_cast<uint64_t>(value))
               : __builtin_clz(static_cast<uint32_t>(value)) - (32 - bits);
#endif
  }
  return bits;
}

// Returns the integer i such as 2^(i-1) < n <= 2^i.
constexpr int Log2Ceiling(uint32_t n) {
  // When n == 0, we want the function to return -1.
  // When n == 0, (n - 1) will underflow to 0xFFFFFFFF, which is
  // why the statement below starts with (n ? 32 : -1).
  return (n ? 32 : -1) - CountlZero(n - 1);
}

// Computes the result of bitwise left-rotating the value of x by s positions.
template <class T>
PA_ALWAYS_INLINE constexpr T RotR(T x, T s) {
  constexpr int n = std::numeric_limits<T>::digits;
  static_assert(n == 32 || n == 64);

#if PA_HAS_BUILTIN(__builtin_rotateright32) && \
    PA_HAS_BUILTIN(__builtin_rotateright64)
  if constexpr (n == 32) {
    return __builtin_rotateright32(x, s);
  } else if constexpr (n == 64) {
    return __builtin_rotateright64(x, s);
  }
#else
  int r = s % n;
  if (r == 0) {
    return x;
  } else if (r > 0) {
    return (x >> r) | (x << (n - r));
  }
#endif  // PA_HAS_BUILTIN(__builtin_rotateright32) &&
        // PA_HAS_BUILTIN(__builtin_rotateright64)

  PA_NOTREACHED();
}

}  // namespace partition_alloc::internal::base::bits

#endif  // PARTITION_ALLOC_PARTITION_ALLOC_BASE_BITS_H_
