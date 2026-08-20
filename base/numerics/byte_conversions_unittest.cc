// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/numerics/byte_conversions.h"

#include <algorithm>
#include <array>
#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
// libc++ does not yet provide <stdfloat>. See
// https://github.com/llvm/llvm-project/issues/105196.
#if __has_include(<stdfloat>)
#include <stdfloat>  // nocheck
#endif
#include <utility>

#include "testing/gtest/include/gtest/gtest.h"

namespace base::numerics {

enum class ScopedEnum8 : uint8_t { kA };
enum class ScopedEnum16 : uint16_t { kA };
enum class ScopedEnum32 : uint32_t { kA };
enum class ScopedEnum64 : uint64_t { kA };
enum class SignedEnum : int32_t { kA };
enum UnscopedEnum : uint32_t { kUnscopedA };

template <typename T>
constexpr auto GetLittleEndianBytes() {
  std::array<uint8_t, sizeof(T)> bytes;
  std::ranges::generate(bytes, [b = uint8_t{0}]() mutable { return ++b; });
  return bytes;
}

template <typename T>
constexpr auto GetBigEndianBytes() {
  auto bytes = GetLittleEndianBytes<T>();
  std::ranges::reverse(bytes);
  return bytes;
}

using TestedTypes = ::testing::Types<
    // Fixed-width unsigned integers
    uint8_t,
    uint16_t,
    uint32_t,
    uint64_t,
    // Fixed-width signed integers
    int8_t,
    int16_t,
    int32_t,
    int64_t,
    // Standard integer types
    char,
    signed char,
    unsigned char,
    short,
    unsigned short,
    int,
    unsigned int,
    long,
    unsigned long,
    long long,           // nocheck
    unsigned long long,  // nocheck
    // Character types
    char8_t,  // nocheck
    char16_t,
    char32_t,
    wchar_t,
    // Size and pointer types
    size_t,
    ptrdiff_t,
    uintptr_t,
    intptr_t,
    // Floating point types
    float,
    double,
    // Enum types
    ScopedEnum8,
    ScopedEnum16,
    ScopedEnum32,
    ScopedEnum64,
    SignedEnum,
    UnscopedEnum
#if defined(__SIZEOF_INT128__)
    ,
    __uint128_t,
    __int128_t
#endif
#if defined(__STDCPP_FLOAT16_T__)
    ,
    std::float16_t
#endif
#if defined(__STDCPP_BFLOAT16_T__)
    ,
    std::bfloat16_t
#endif
#if defined(__STDCPP_FLOAT32_T__)
    ,
    std::float32_t
#endif
#if defined(__STDCPP_FLOAT64_T__)
    ,
    std::float64_t
#endif
#if defined(__STDCPP_FLOAT128_T__) && defined(__SIZEOF_INT128__)
    ,
    std::float128_t
#endif
    >;

template <typename T>
class ByteConversionsTest : public ::testing::Test {};

TYPED_TEST_SUITE(ByteConversionsTest, TestedTypes);

TYPED_TEST(ByteConversionsTest, FromNativeEndian) {
  static_assert(std::endian::native == std::endian::little);
  constexpr auto kLeBytes = GetLittleEndianBytes<TypeParam>();

  EXPECT_EQ(ToNativeEndian<TypeParam>(FromNativeEndian<TypeParam>(kLeBytes)),
            kLeBytes);
  static_assert(
      std::same_as<TypeParam, decltype(FromNativeEndian<TypeParam>(kLeBytes))>);
  static_assert(ToNativeEndian<TypeParam>(
                    FromNativeEndian<TypeParam>(kLeBytes)) == kLeBytes);
}

TYPED_TEST(ByteConversionsTest, FromLittleEndian) {
  static_assert(std::endian::native == std::endian::little);
  constexpr auto kLeBytes = GetLittleEndianBytes<TypeParam>();

  EXPECT_EQ(ToLittleEndian<TypeParam>(FromLittleEndian<TypeParam>(kLeBytes)),
            kLeBytes);
  static_assert(
      std::same_as<TypeParam, decltype(FromLittleEndian<TypeParam>(kLeBytes))>);
  static_assert(ToLittleEndian<TypeParam>(
                    FromLittleEndian<TypeParam>(kLeBytes)) == kLeBytes);
}

TYPED_TEST(ByteConversionsTest, FromBigEndian) {
  static_assert(std::endian::native == std::endian::little);
  constexpr auto kBeBytes = GetBigEndianBytes<TypeParam>();
  constexpr auto kLeBytes = GetLittleEndianBytes<TypeParam>();

  EXPECT_EQ(ToLittleEndian<TypeParam>(FromBigEndian<TypeParam>(kBeBytes)),
            kLeBytes);
  static_assert(
      std::same_as<TypeParam, decltype(FromBigEndian<TypeParam>(kBeBytes))>);
  static_assert(ToLittleEndian<TypeParam>(FromBigEndian<TypeParam>(kBeBytes)) ==
                kLeBytes);
}

TYPED_TEST(ByteConversionsTest, ToNativeEndian) {
  static_assert(std::endian::native == std::endian::little);
  constexpr auto kLeBytes = GetLittleEndianBytes<TypeParam>();
  constexpr auto kVal = std::bit_cast<TypeParam>(kLeBytes);

  EXPECT_EQ(ToNativeEndian<TypeParam>(kVal), kLeBytes);
  static_assert(std::same_as<std::array<uint8_t, sizeof(TypeParam)>,
                             decltype(ToNativeEndian<TypeParam>(kVal))>);
  static_assert(ToNativeEndian<TypeParam>(kVal) == kLeBytes);
}

TYPED_TEST(ByteConversionsTest, ToLittleEndian) {
  static_assert(std::endian::native == std::endian::little);
  constexpr auto kLeBytes = GetLittleEndianBytes<TypeParam>();
  constexpr auto kVal = std::bit_cast<TypeParam>(kLeBytes);

  EXPECT_EQ(ToLittleEndian<TypeParam>(kVal), kLeBytes);
  static_assert(std::same_as<std::array<uint8_t, sizeof(TypeParam)>,
                             decltype(ToLittleEndian<TypeParam>(kVal))>);
  static_assert(ToLittleEndian<TypeParam>(kVal) == kLeBytes);
}

TYPED_TEST(ByteConversionsTest, ToBigEndian) {
  static_assert(std::endian::native == std::endian::little);
  constexpr auto kBeBytes = GetBigEndianBytes<TypeParam>();
  constexpr auto kLeBytes = GetLittleEndianBytes<TypeParam>();
  constexpr auto kVal = std::bit_cast<TypeParam>(kLeBytes);

  EXPECT_EQ(ToBigEndian<TypeParam>(kVal), kBeBytes);
  static_assert(std::same_as<std::array<uint8_t, sizeof(TypeParam)>,
                             decltype(ToBigEndian<TypeParam>(kVal))>);
  static_assert(ToBigEndian<TypeParam>(kVal) == kBeBytes);
}

TYPED_TEST(ByteConversionsTest, RoundTrip) {
  constexpr auto kLeBytes = GetLittleEndianBytes<TypeParam>();
  constexpr auto kBeBytes = GetBigEndianBytes<TypeParam>();

  EXPECT_EQ(ToLittleEndian<TypeParam>(FromLittleEndian<TypeParam>(kLeBytes)),
            kLeBytes);
  EXPECT_EQ(ToBigEndian<TypeParam>(FromBigEndian<TypeParam>(kBeBytes)),
            kBeBytes);
  EXPECT_EQ(ToLittleEndian<TypeParam>(FromNativeEndian<TypeParam>(kLeBytes)),
            kLeBytes);

  static_assert(ToLittleEndian<TypeParam>(
                    FromLittleEndian<TypeParam>(kLeBytes)) == kLeBytes);
  static_assert(ToBigEndian<TypeParam>(FromBigEndian<TypeParam>(kBeBytes)) ==
                kBeBytes);
  static_assert(ToLittleEndian<TypeParam>(
                    FromNativeEndian<TypeParam>(kLeBytes)) == kLeBytes);
}

using TestedEnumTypes = ::testing::Types<ScopedEnum8,
                                         ScopedEnum16,
                                         ScopedEnum32,
                                         ScopedEnum64,
                                         SignedEnum,
                                         UnscopedEnum>;

template <typename T>
class EnumByteConversionsTest : public ::testing::Test {};

TYPED_TEST_SUITE(EnumByteConversionsTest, TestedEnumTypes);

TYPED_TEST(EnumByteConversionsTest, EnumConversions) {
  constexpr auto kLeBytes = GetLittleEndianBytes<TypeParam>();
  constexpr auto kBeBytes = GetBigEndianBytes<TypeParam>();
  constexpr auto kVal = std::bit_cast<TypeParam>(kLeBytes);

  EXPECT_EQ(EnumFromNativeEndian<TypeParam>(kLeBytes), kVal);
  static_assert(EnumFromNativeEndian<TypeParam>(kLeBytes) == kVal);
  EXPECT_EQ(EnumToNativeEndian(kVal), kLeBytes);
  static_assert(EnumToNativeEndian(kVal) == kLeBytes);

  EXPECT_EQ(EnumFromLittleEndian<TypeParam>(kLeBytes), kVal);
  static_assert(EnumFromLittleEndian<TypeParam>(kLeBytes) == kVal);
  EXPECT_EQ(EnumToLittleEndian(kVal), kLeBytes);
  static_assert(EnumToLittleEndian(kVal) == kLeBytes);

  EXPECT_EQ(EnumFromBigEndian<TypeParam>(kBeBytes), kVal);
  static_assert(EnumFromBigEndian<TypeParam>(kBeBytes) == kVal);
  EXPECT_EQ(EnumToBigEndian(kVal), kBeBytes);
  static_assert(EnumToBigEndian(kVal) == kBeBytes);
}

}  // namespace base::numerics
