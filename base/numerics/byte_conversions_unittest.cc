// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/numerics/byte_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base::numerics {

TEST(NumericsTest, FromNativeEndian) {
  // The implementation of FromNativeEndian and FromLittleEndian assumes the
  // native endian is little. If support of big endian is desired, compile-time
  // branches will need to be added to the implementation, and the test results
  // will differ there (they would match FromBigEndian in this test).
  static_assert(std::endian::native == std::endian::little);
  {
    constexpr uint8_t bytes[] = {0x12u};
    EXPECT_EQ(U8FromNativeEndian(bytes), 0x12u);
    static_assert(std::same_as<uint8_t, decltype(U8FromNativeEndian(bytes))>);
    static_assert(U8FromNativeEndian(bytes) == 0x12u);
  }
  {
    constexpr uint8_t bytes[] = {0x12u, 0x34u};
    EXPECT_EQ(U16FromNativeEndian(bytes), 0x34'12u);
    static_assert(std::same_as<uint16_t, decltype(U16FromNativeEndian(bytes))>);
    static_assert(U16FromNativeEndian(bytes) == 0x34'12u);
  }
  {
    constexpr uint8_t bytes[] = {0x12u, 0x34u, 0x56u, 0x78u};
    EXPECT_EQ(U32FromNativeEndian(bytes), 0x78'56'34'12u);
    static_assert(std::same_as<uint32_t, decltype(U32FromNativeEndian(bytes))>);
    static_assert(U32FromNativeEndian(bytes) == 0x78'56'34'12u);
  }
  {
    constexpr uint8_t bytes[] = {0x12u, 0x34u, 0x56u, 0x78u,
                                 0x90u, 0x12u, 0x34u, 0x56u};
    EXPECT_EQ(U64FromNativeEndian(bytes), 0x56'34'12'90'78'56'34'12u);
    static_assert(std::same_as<uint64_t, decltype(U64FromNativeEndian(bytes))>);
    static_assert(U64FromNativeEndian(bytes) == 0x56'34'12'90'78'56'34'12u);
  }

  {
    constexpr uint8_t bytes[] = {0x12u, 0x34u, 0x56u, 0x78u};
    EXPECT_EQ(FloatFromNativeEndian(bytes), 1.73782443614e+34f);
    EXPECT_EQ(std::bit_cast<uint32_t>(FloatFromNativeEndian(bytes)),
              0x78'56'34'12u);
    static_assert(std::same_as<float, decltype(FloatFromNativeEndian(bytes))>);
    static_assert(FloatFromNativeEndian(bytes) == 1.73782443614e+34f);
    static_assert(std::bit_cast<uint32_t>(FloatFromNativeEndian(bytes)) ==
                  0x78'56'34'12u);
  }

  {
    constexpr uint8_t bytes[] = {0x12u, 0x34u, 0x56u, 0x78u,
                                 0x90u, 0x12u, 0x34u, 0x56u};
    EXPECT_EQ(DoubleFromNativeEndian(bytes),
              1.84145159269283616391989849435e107);
    EXPECT_EQ(std::bit_cast<uint64_t>(DoubleFromNativeEndian(bytes)),
              0x56'34'12'90'78'56'34'12u);
    static_assert(
        std::same_as<double, decltype(DoubleFromNativeEndian(bytes))>);
    static_assert(DoubleFromNativeEndian(bytes) ==
                  1.84145159269283616391989849435e107);
    static_assert(std::bit_cast<uint64_t>(DoubleFromNativeEndian(bytes)) ==
                  0x56'34'12'90'78'56'34'12u);
  }
}

TEST(NumericsTest, FromLittleEndian) {
  // The implementation of FromNativeEndian and FromLittleEndian assumes the
  // native endian is little. If support of big endian is desired, compile-time
  // branches will need to be added to the implementation, and the test results
  // will differ there (they would match FromBigEndian in this test).
  static_assert(std::endian::native == std::endian::little);
  {
    constexpr uint8_t bytes[] = {0x12u};
    EXPECT_EQ(U8FromLittleEndian(bytes), 0x12u);
    static_assert(std::same_as<uint8_t, decltype(U8FromLittleEndian(bytes))>);
    static_assert(U8FromLittleEndian(bytes) == 0x12u);
  }
  {
    constexpr uint8_t bytes[] = {0x12u, 0x34u};
    EXPECT_EQ(U16FromLittleEndian(bytes), 0x34'12u);
    static_assert(std::same_as<uint16_t, decltype(U16FromLittleEndian(bytes))>);
    static_assert(U16FromLittleEndian(bytes) == 0x34'12u);
  }
  {
    constexpr uint8_t bytes[] = {0x12u, 0x34u, 0x56u, 0x78u};
    EXPECT_EQ(U32FromLittleEndian(bytes), 0x78'56'34'12u);
    static_assert(std::same_as<uint32_t, decltype(U32FromLittleEndian(bytes))>);
    static_assert(U32FromLittleEndian(bytes) == 0x78'56'34'12u);
  }
  {
    constexpr uint8_t bytes[] = {0x12u, 0x34u, 0x56u, 0x78u,
                                 0x90u, 0x12u, 0x34u, 0x56u};
    EXPECT_EQ(U64FromLittleEndian(bytes), 0x56'34'12'90'78'56'34'12u);
    static_assert(std::same_as<uint64_t, decltype(U64FromLittleEndian(bytes))>);
    static_assert(U64FromLittleEndian(bytes) == 0x56'34'12'90'78'56'34'12u);
  }

  {
    constexpr uint8_t bytes[] = {0x12u, 0x34u, 0x56u, 0x78u};
    EXPECT_EQ(FloatFromLittleEndian(bytes), 1.73782443614e+34f);
    EXPECT_EQ(std::bit_cast<uint32_t>(FloatFromLittleEndian(bytes)),
              0x78'56'34'12u);
    static_assert(std::same_as<float, decltype(FloatFromLittleEndian(bytes))>);
    static_assert(FloatFromLittleEndian(bytes) == 1.73782443614e+34f);
    static_assert(std::bit_cast<uint32_t>(FloatFromLittleEndian(bytes)) ==
                  0x78'56'34'12u);
  }

  {
    constexpr uint8_t bytes[] = {0x12u, 0x34u, 0x56u, 0x78u,
                                 0x90u, 0x12u, 0x34u, 0x56u};
    EXPECT_EQ(DoubleFromLittleEndian(bytes),
              1.84145159269283616391989849435e107);
    EXPECT_EQ(std::bit_cast<uint64_t>(DoubleFromLittleEndian(bytes)),
              0x56'34'12'90'78'56'34'12u);
    static_assert(
        std::same_as<double, decltype(DoubleFromLittleEndian(bytes))>);
    static_assert(DoubleFromLittleEndian(bytes) ==
                  1.84145159269283616391989849435e107);
    static_assert(std::bit_cast<uint64_t>(DoubleFromLittleEndian(bytes)) ==
                  0x56'34'12'90'78'56'34'12u);
  }
}

TEST(NumericsTest, FromBigEndian) {
  // The implementation of FromNativeEndian and FromLittleEndian assumes the
  // native endian is little. If support of big endian is desired, compile-time
  // branches will need to be added to the implementation, and the test results
  // will differ there (they would match FromLittleEndian in this test).
  static_assert(std::endian::native == std::endian::little);
  {
    constexpr uint8_t bytes[] = {0x12u};
    EXPECT_EQ(U8FromBigEndian(bytes), 0x12u);
    static_assert(U8FromBigEndian(bytes) == 0x12u);
    static_assert(std::same_as<uint8_t, decltype(U8FromBigEndian(bytes))>);
  }
  {
    constexpr uint8_t bytes[] = {0x12u, 0x34u};
    EXPECT_EQ(U16FromBigEndian(bytes), 0x12'34u);
    static_assert(U16FromBigEndian(bytes) == 0x12'34u);
    static_assert(std::same_as<uint16_t, decltype(U16FromBigEndian(bytes))>);
  }
  {
    constexpr uint8_t bytes[] = {0x12u, 0x34u, 0x56u, 0x78u};
    EXPECT_EQ(U32FromBigEndian(bytes), 0x12'34'56'78u);
    static_assert(U32FromBigEndian(bytes) == 0x12'34'56'78u);
    static_assert(std::same_as<uint32_t, decltype(U32FromBigEndian(bytes))>);
  }
  {
    constexpr uint8_t bytes[] = {0x12u, 0x34u, 0x56u, 0x78u,
                                 0x90u, 0x12u, 0x34u, 0x56u};
    EXPECT_EQ(U64FromBigEndian(bytes), 0x12'34'56'78'90'12'34'56u);
    static_assert(U64FromBigEndian(bytes) == 0x12'34'56'78'90'12'34'56u);
    static_assert(std::same_as<uint64_t, decltype(U64FromBigEndian(bytes))>);
  }

  {
    constexpr uint8_t bytes[] = {0x12u, 0x34u, 0x56u, 0x78u};
    EXPECT_EQ(FloatFromBigEndian(bytes), 5.6904566139e-28f);
    EXPECT_EQ(std::bit_cast<uint32_t>(FloatFromBigEndian(bytes)),
              0x12'34'56'78u);
    static_assert(std::same_as<float, decltype(FloatFromBigEndian(bytes))>);
    static_assert(FloatFromBigEndian(bytes) == 5.6904566139e-28f);
    static_assert(std::bit_cast<uint32_t>(FloatFromBigEndian(bytes)) ==
                  0x12'34'56'78u);
  }
  {
    constexpr uint8_t bytes[] = {0x12u, 0x34u, 0x56u, 0x78u,
                                 0x90u, 0x12u, 0x34u, 0x56u};
    EXPECT_EQ(DoubleFromBigEndian(bytes), 5.62634909901491201382066931077e-221);
    EXPECT_EQ(std::bit_cast<uint64_t>(DoubleFromBigEndian(bytes)),
              0x12'34'56'78'90'12'34'56u);
    static_assert(std::same_as<double, decltype(DoubleFromBigEndian(bytes))>);
    static_assert(DoubleFromBigEndian(bytes) ==
                  5.62634909901491201382066931077e-221);
    static_assert(std::bit_cast<uint64_t>(DoubleFromBigEndian(bytes)) ==
                  0x12'34'56'78'90'12'34'56u);
  }
}

}  // namespace base::numerics
