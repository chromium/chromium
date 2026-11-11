// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_NUMERICS_BASIC_OPS_IMPL_H_
#define BASE_NUMERICS_BASIC_OPS_IMPL_H_

#include <algorithm>
#include <array>
#include <bit>
#include <climits>
#include <concepts>
#include <cstdint>
#include <limits>
#include <span>
#include <type_traits>
#include <utility>

namespace base::numerics_internal {

// Matches any unsigned integral type with no padding bits.
//
// Automatically rejects `bool` because `digits == 1` vs `sizeof(bool) * 8 ==
// 8`. Accepts `uint8_t`..`uint64_t`, `size_t`, `char8_t`, `char16_t`,
// `char32_t`, and `char` (on platforms where char is unsigned).
template <typename T>
concept PackedUnsignedIntegral =
    std::unsigned_integral<T> &&
    (std::numeric_limits<T>::digits == sizeof(T) * CHAR_BIT);

// Matches any signed integral type with no padding bits.
//
// Accepts `int8_t`..`int64_t`, `ptrdiff_t`, and `char` (on platforms where
// char is signed).
template <typename T>
concept PackedSignedIntegral =
    std::signed_integral<T> &&
    (std::numeric_limits<T>::digits + 1 == sizeof(T) * CHAR_BIT);

// Matches any signed or unsigned integral type with no padding bits.
template <typename T>
concept PackedIntegral = PackedUnsignedIntegral<T> || PackedSignedIntegral<T>;

// Matches IEEE 754 standard binary floating-point types (`float`, `double`,
// `std::float16_t`, `std::float128_t`) with no padding bits.
//
// The formula calculates the total stored bits:
//   1 (sign bit) +
//   bit_width(max_exponent - 1) (exponent bits - 1) +
//   digits (stored mantissa bits + 1 for implicit leading 1 bit)
//
// This guarantees zero padding bits and inherently rejects non-standard formats
// like x87 80-bit `long double` in 12- or 16-byte containers (as well as 80-bit
// floats with explicit integer bits).
template <typename T>
concept IeeeBinaryFloat =
    std::floating_point<T> && std::numeric_limits<T>::is_iec559 &&
    (1 + std::bit_width(size_t{std::numeric_limits<T>::max_exponent - 1}) +
         std::numeric_limits<T>::digits ==
     sizeof(T) * CHAR_BIT);

// Matches any scoped or unscoped enumeration whose underlying integral type
// satisfies `PackedIntegral`.
//
// This accepts standard enums backed by `uint8_t`..`uint64_t` or `int`, but
// rejects enums explicitly backed by `bool` (`enum class E : bool`).
template <typename T>
concept PackedEnum =
    std::is_enum_v<T> && PackedIntegral<std::underlying_type_t<T>>;

// Type trait to provide an unsigned int of a desired exact bit size.
// Simplified version of `boost::uint_t<N>::exact`:
// https://www.boost.org/doc/libs/latest/libs/integer/doc/html/boost_integer/integer.html
template <size_t N>
struct uint_exact;

template <>
struct uint_exact<8> {
  using type = uint8_t;
};
template <>
struct uint_exact<16> {
  using type = uint16_t;
};
template <>
struct uint_exact<32> {
  using type = uint32_t;
};
template <>
struct uint_exact<64> {
  using type = uint64_t;
};
#if defined(__SIZEOF_INT128__)
template <>
struct uint_exact<128> {
  using type = __uint128_t;
};
#endif

template <size_t N>
using uint_exact_t = typename uint_exact<N>::type;

// Reads a value of type `T` from `bytes` with the specified endianness.
template <std::endian Endian, typename T>
  requires(PackedUnsignedIntegral<T>)
constexpr T FromEndian(std::span<const uint8_t, sizeof(T)> bytes) {
  // std::bit_cast<T, U> requires matching sizeof. Thus we materialize the span
  // into an array first. At runtime this is completely elided:
  // https://godbolt.org/z/cssPcEnG4
  std::array<uint8_t, sizeof(T)> arr;
  std::ranges::copy(bytes, arr.begin());
  T val = std::bit_cast<T>(arr);
  return Endian == std::endian::native ? val : std::byteswap(val);
}

template <std::endian Endian, typename T>
  requires(PackedSignedIntegral<T>)
constexpr T FromEndian(std::span<const uint8_t, sizeof(T)> bytes) {
  return static_cast<T>(FromEndian<Endian, std::make_unsigned_t<T>>(bytes));
}

template <std::endian Endian, typename T>
  requires(IeeeBinaryFloat<T>)
constexpr T FromEndian(std::span<const uint8_t, sizeof(T)> bytes) {
  return std::bit_cast<T>(
      FromEndian<Endian, uint_exact_t<sizeof(T) * 8>>(bytes));
}

template <std::endian Endian, typename T>
  requires(PackedEnum<T>)
constexpr T FromEndian(std::span<const uint8_t, sizeof(T)> bytes) {
  return static_cast<T>(FromEndian<Endian, std::underlying_type_t<T>>(bytes));
}

// Converts `val` of type `T` to a byte array with the specified endianness.
template <std::endian Endian, typename T>
  requires(PackedUnsignedIntegral<T>)
constexpr std::array<uint8_t, sizeof(T)> ToEndian(T val) {
  return std::bit_cast<std::array<uint8_t, sizeof(T)>>(
      Endian == std::endian::native ? val : std::byteswap(val));
}

template <std::endian Endian, typename T>
  requires(PackedSignedIntegral<T>)
constexpr std::array<uint8_t, sizeof(T)> ToEndian(T val) {
  return ToEndian<Endian>(static_cast<std::make_unsigned_t<T>>(val));
}

template <std::endian Endian, typename T>
  requires(IeeeBinaryFloat<T>)
constexpr std::array<uint8_t, sizeof(T)> ToEndian(T val) {
  return ToEndian<Endian>(std::bit_cast<uint_exact_t<sizeof(T) * 8>>(val));
}

template <std::endian Endian, typename T>
  requires(PackedEnum<T>)
constexpr std::array<uint8_t, sizeof(T)> ToEndian(T val) {
  return ToEndian<Endian>(std::to_underlying(val));
}

}  // namespace base::numerics_internal

#endif  // BASE_NUMERICS_BASIC_OPS_IMPL_H_
