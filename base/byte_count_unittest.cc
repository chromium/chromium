// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/byte_count.h"

#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/test/gtest_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TEST(ByteCount, ConstructionDefault) {
  constexpr ByteCount bytes;
  EXPECT_EQ(0, bytes.InBytes());
  EXPECT_EQ(0, bytes.InKiB());
  EXPECT_EQ(0, bytes.InMiB());
  EXPECT_EQ(0, bytes.InGiB());
  EXPECT_EQ(0, bytes.InBytesUnsigned());
}

TEST(ByteCount, ConstructionByteCount) {
  constexpr ByteCount bytes(1024 * 1024 * 1024);
  EXPECT_EQ(1024 * 1024 * 1024, bytes.InBytes());
  EXPECT_EQ(1024 * 1024, bytes.InKiB());
  EXPECT_EQ(1024, bytes.InMiB());
  EXPECT_EQ(1, bytes.InGiB());
  EXPECT_EQ(1024u * 1024 * 1024, bytes.InBytesUnsigned());
}

TEST(ByteCount, ConstructionUnsigned) {
  auto bytes = ByteCount::FromUnsigned(5u);
  EXPECT_EQ(5, bytes.InBytes());
}

TEST(ByteCountDeathTest, ConstructionUnsignedInvalid) {
  BASE_EXPECT_DEATH(
      ByteCount::FromUnsigned(std::numeric_limits<uint64_t>::max()), "");
}

TEST(ByteCount, ConstructionChecked) {
  auto bytes = ByteCount::FromChecked(CheckedNumeric<uint64_t>(5));
  EXPECT_EQ(5, bytes.InBytes());
}

TEST(ByteCountDeathTest, ConstructionCheckedInvalid) {
  BASE_EXPECT_DEATH(
      ByteCount::FromChecked(
          CheckedNumeric<int64_t>(std::numeric_limits<int64_t>::max()) + 1),
      "");
}

TEST(ByteCount, ConstructionOtherUnitIntegral) {
  // 32-bit numbers that will overflow if multiplied as
  // CheckedNumeric<uint32_t>, but not if multiplied as CheckedNumeric<int64_t>.
  // This verifies that the implementations of the KiB, MiB and GiB templates
  // avoid a subtle conversion order bug.
  constexpr int64_t kLargeKiB32 =
      std::numeric_limits<uint32_t>::max() / 1024 + 1;
  constexpr int64_t kLargeMiB32 =
      std::numeric_limits<uint32_t>::max() / 1024 / 1024 + 1;
  constexpr int64_t kLargeGiB32 =
      std::numeric_limits<uint32_t>::max() / 1024 / 1024 / 1024 + 1;

  static_assert(kLargeKiB32 * 1024 > std::numeric_limits<uint32_t>::max());
  static_assert(kLargeMiB32 * 1024 * 1024 >
                std::numeric_limits<uint32_t>::max());
  static_assert(kLargeGiB32 * 1024 * 1024 * 1024 >
                std::numeric_limits<uint32_t>::max());

  auto kib5 = KiB(5);
  EXPECT_EQ(5 * 1024, kib5.InBytes());
  EXPECT_EQ(kLargeKiB32 * 1024,
            KiB(checked_cast<uint32_t>(kLargeKiB32)).InBytes());

  auto mib5 = MiB(5);
  EXPECT_EQ(5 * 1024 * 1024, mib5.InBytes());
  EXPECT_EQ(kLargeMiB32 * 1024 * 1024,
            MiB(checked_cast<uint32_t>(kLargeMiB32)).InBytes());

  auto gib5 = GiB(5);
  EXPECT_EQ(5ll * 1024 * 1024 * 1024, gib5.InBytes());
  EXPECT_EQ(kLargeGiB32 * 1024 * 1024 * 1024,
            GiB(checked_cast<uint32_t>(kLargeGiB32)).InBytes());
}

TEST(ByteCount, ConstructionOtherUnitFloat) {
  auto kib = KiB(5.5);
  EXPECT_EQ(5632, kib.InBytes());
  EXPECT_EQ(5632.0, kib.InBytesF());

  auto mib = MiB(2.3);
  // Round down from 2411724.8
  EXPECT_EQ(2411724, mib.InBytes());
  EXPECT_EQ(2411724.0, mib.InBytesF());

  auto gib = GiB(12.81);
  // Round down from 13754632765.4
  EXPECT_EQ(13754632765, gib.InBytes());
  EXPECT_EQ(13754632765.0, gib.InBytesF());

  auto negative_kib = KiB(-4.2);
  // Round up from -4300.8
  EXPECT_EQ(-4300, negative_kib.InBytes());
  EXPECT_EQ(-4300.0, negative_kib.InBytesF());

  auto negative_mib = MiB(-9.89);
  // Round up from -10370416.64
  EXPECT_EQ(-10370416, negative_mib.InBytes());
  EXPECT_EQ(-10370416.0, negative_mib.InBytesF());

  auto negative_gib = GiB(-5.17);
  // Round up from -5551245230.08
  EXPECT_EQ(-5551245230, negative_gib.InBytes());
  EXPECT_EQ(-5551245230.0, negative_gib.InBytesF());
}

TEST(ByteCountDeathTest, ConstructionOtherUnitInvalid) {
  BASE_EXPECT_DEATH(KiB(std::numeric_limits<int64_t>::max()), "");

  BASE_EXPECT_DEATH(MiB(std::numeric_limits<int64_t>::max()), "");

  BASE_EXPECT_DEATH(GiB(std::numeric_limits<int64_t>::max()), "");
}

TEST(ByteCount, IsZero) {
  EXPECT_TRUE(ByteCount(0).is_zero());
  EXPECT_FALSE(ByteCount(-2).is_zero());
  EXPECT_FALSE(ByteCount(2).is_zero());
}

TEST(ByteCount, InFloating) {
  constexpr ByteCount bytes(3435973836);
  EXPECT_THAT(bytes.InBytesF(), testing::DoubleEq(3435973836.0));
  EXPECT_THAT(bytes.InKiBF(), testing::DoubleEq(3355443.19921875));
  EXPECT_THAT(bytes.InMiBF(), testing::DoubleEq(3276.7999992370605));
  EXPECT_THAT(bytes.InGiBF(), testing::DoubleEq(3.1999999992549419));
}

TEST(ByteCountDeathTest, InUnsignedInvalid) {
  ByteCount bytes(-2);
  BASE_EXPECT_DEATH(bytes.InBytesUnsigned(), "");
}

TEST(ByteCount, Arithmetic) {
  ByteCount bytes(42);

  ByteCount add = bytes + ByteCount(10);
  EXPECT_EQ(52, add.InBytes());

  ByteCount sub = bytes - ByteCount(10);
  EXPECT_EQ(32, sub.InBytes());

  ByteCount mul = bytes * 10;
  EXPECT_EQ(420, mul.InBytes());

  ByteCount div = bytes / 2;
  EXPECT_EQ(21, div.InBytes());
}

TEST(ByteCount, ArithmeticCompound) {
  ByteCount bytes(42);

  bytes += ByteCount(10);
  EXPECT_EQ(52, bytes.InBytes());

  bytes -= ByteCount(10);
  EXPECT_EQ(42, bytes.InBytes());

  bytes *= 10;
  EXPECT_EQ(420, bytes.InBytes());

  bytes /= 2;
  EXPECT_EQ(210, bytes.InBytes());
}

TEST(ByteCountDeathTest, ArithmeticInvalid) {
  ByteCount max_bytes(std::numeric_limits<int64_t>::max());

  BASE_EXPECT_DEATH({ max_bytes + max_bytes; }, "");

  BASE_EXPECT_DEATH({ ByteCount() - max_bytes - max_bytes; }, "");

  BASE_EXPECT_DEATH({ max_bytes * 2; }, "");

  BASE_EXPECT_DEATH({ max_bytes / 0; }, "");
}

TEST(ByteCount, Comparison) {
  ByteCount a(1);
  ByteCount b(2);
  ByteCount c(2);

  EXPECT_TRUE(a < b);
  EXPECT_FALSE(b < a);
  EXPECT_FALSE(b < c);

  EXPECT_TRUE(a <= b);
  EXPECT_FALSE(b <= a);
  EXPECT_TRUE(b <= c);

  EXPECT_FALSE(a > b);
  EXPECT_TRUE(b > a);
  EXPECT_FALSE(b > c);

  EXPECT_FALSE(a >= b);
  EXPECT_TRUE(b >= a);
  EXPECT_TRUE(b >= c);

  EXPECT_FALSE(a == b);
  EXPECT_TRUE(b == c);

  EXPECT_TRUE(a != b);
  EXPECT_FALSE(b != c);
}

}  // namespace base
