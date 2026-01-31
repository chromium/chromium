// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/390223051): Remove C-library calls to fix the errors.
#pragma allow_unsafe_libc_calls
#endif

#ifndef BASE_NUMERICS_BASIC_OPS_IMPL_H_
#define BASE_NUMERICS_BASIC_OPS_IMPL_H_

#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <span>
#include <type_traits>

namespace base::numerics_internal {

// The correct type to perform math operations on given values of type `T`. This
// may be a larger type than `T` to avoid promotion to `int` which involves sign
// conversion!
template <class T>
  requires(std::is_integral_v<T>)
using MathType = std::conditional_t<
    sizeof(T) >= sizeof(int),
    T,
    std::conditional_t<std::is_signed_v<T>, int, unsigned int>>;

// Converts from a byte array to an integer.
template <class T>
  requires(std::is_unsigned_v<T> && std::is_integral_v<T>)
inline constexpr T FromLittleEndian(std::span<const uint8_t, sizeof(T)> bytes) {
  T val;
  if consteval {
    val = T{0};
    for (size_t i = 0u; i < sizeof(T); i += 1u) {
      // SAFETY: `i < sizeof(T)` (the number of bytes in T), so `(8 * i)` is
      // less than the number of bits in T.
      val |= MathType<T>(bytes[i]) << (8u * i);
    }
  } else {
    // SAFETY: `bytes` has sizeof(T) bytes, and `val` is of type `T` so has
    // sizeof(T) bytes, and the two can not alias as `val` is a stack variable.
    memcpy(&val, bytes.data(), sizeof(T));
  }
  return val;
}

template <class T>
  requires(std::is_signed_v<T> && std::is_integral_v<T>)
inline constexpr T FromLittleEndian(std::span<const uint8_t, sizeof(T)> bytes) {
  return static_cast<T>(FromLittleEndian<std::make_unsigned_t<T>>(bytes));
}

// Converts to a byte array from an integer.
template <class T>
  requires(std::is_unsigned_v<T> && std::is_integral_v<T>)
inline constexpr std::array<uint8_t, sizeof(T)> ToLittleEndian(T val) {
  auto bytes = std::array<uint8_t, sizeof(T)>();
  if consteval {
    for (size_t i = 0u; i < sizeof(T); i += 1u) {
      const auto last_byte = static_cast<uint8_t>(val & 0xff);
      // The low bytes go to the front of the array in little endian.
      bytes[i] = last_byte;
      // If `val` is one byte, this shift would be UB. But it's also not needed
      // since the loop will not run again.
      if constexpr (sizeof(T) > 1u) {
        val >>= 8u;
      }
    }
  } else {
    // SAFETY: `bytes` has sizeof(T) bytes, and `val` is of type `T` so has
    // sizeof(T) bytes, and the two can not alias as `val` is a stack variable.
    memcpy(bytes.data(), &val, sizeof(T));
  }
  return bytes;
}

template <class T>
  requires(std::is_signed_v<T> && std::is_integral_v<T>)
inline constexpr std::array<uint8_t, sizeof(T)> ToLittleEndian(T val) {
  return ToLittleEndian(static_cast<std::make_unsigned_t<T>>(val));
}
}  // namespace base::numerics_internal

#endif  // BASE_NUMERICS_BASIC_OPS_IMPL_H_
