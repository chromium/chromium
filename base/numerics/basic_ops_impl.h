// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_NUMERICS_BASIC_OPS_IMPL_H_
#define BASE_NUMERICS_BASIC_OPS_IMPL_H_

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <span>
#include <type_traits>

namespace base::numerics_internal {

// Converts from a byte array to an integer. We use `std::ranges::copy` and
// `std::bit_cast` to support `constexpr` evaluation while maintaining optimal
// runtime performance (compilers can optimize this to a single load). We avoid
// `base::span` here to keep `base/numerics` free of dependencies on other parts
// of `base`. See https://godbolt.org/z/cssPcEnG4 for a proof.
template <class T>
  requires(std::is_unsigned_v<T> && std::is_integral_v<T>)
inline constexpr T FromLittleEndian(std::span<const uint8_t, sizeof(T)> bytes) {
  std::array<uint8_t, sizeof(T)> arr;
  std::ranges::copy(bytes, arr.begin());
  return std::bit_cast<T>(arr);
}

template <class T>
  requires(std::is_signed_v<T> && std::is_integral_v<T>)
inline constexpr T FromLittleEndian(std::span<const uint8_t, sizeof(T)> bytes) {
  return static_cast<T>(FromLittleEndian<std::make_unsigned_t<T>>(bytes));
}

// Converts to a byte array from an integer. We use `std::bit_cast` for
// `constexpr` support and optimal performance.
template <class T>
  requires(std::is_unsigned_v<T> && std::is_integral_v<T>)
inline constexpr std::array<uint8_t, sizeof(T)> ToLittleEndian(T val) {
  return std::bit_cast<std::array<uint8_t, sizeof(T)>>(val);
}

template <class T>
  requires(std::is_signed_v<T> && std::is_integral_v<T>)
inline constexpr std::array<uint8_t, sizeof(T)> ToLittleEndian(T val) {
  return ToLittleEndian(static_cast<std::make_unsigned_t<T>>(val));
}
}  // namespace base::numerics_internal

#endif  // BASE_NUMERICS_BASIC_OPS_IMPL_H_
