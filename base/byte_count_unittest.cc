// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/byte_count.h"

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

TEST(ByteCount, ConstructionUnsignedInvalid) {
  BASE_EXPECT_DEATH(
      { ByteCount::FromUnsigned(std::numeric_limits<uint64_t>::max()); }, "");
}

TEST(ByteCount, ConstructionOtherUnit) {
  auto kib5 = KiB(5);
  EXPECT_EQ(5 * 1024, kib5.InBytes());

  auto mib5 = MiB(5);
  EXPECT_EQ(5 * 1024 * 1024, mib5.InBytes());

  auto gib5 = GiB(5);
  EXPECT_EQ(5ll * 1024 * 1024 * 1024, gib5.InBytes());
}

TEST(ByteCount, ConstructionOtherUnitInvalid) {
  BASE_EXPECT_DEATH({ KiB(std::numeric_limits<int64_t>::max()); }, "");

  BASE_EXPECT_DEATH({ MiB(std::numeric_limits<int64_t>::max()); }, "");

  BASE_EXPECT_DEATH({ GiB(std::numeric_limits<int64_t>::max()); }, "");
}

TEST(ByteCount, IsZero) {
  EXPECT_TRUE(ByteCount(0).is_zero());
  EXPECT_FALSE(ByteCount(-2).is_zero());
  EXPECT_FALSE(ByteCount(2).is_zero());
}

TEST(ByteCount, InFloating) {
  constexpr ByteCount bytes(3435973836);
  EXPECT_THAT(bytes.InKiBF(), testing::DoubleEq(3355443.19921875));
  EXPECT_THAT(bytes.InMiBF(), testing::DoubleEq(3276.7999992370605));
  EXPECT_THAT(bytes.InGiBF(), testing::DoubleEq(3.1999999992549419));
}

TEST(ByteCount, InUnsignedInvalid) {
  ByteCount bytes(-2);
  BASE_EXPECT_DEATH({ bytes.InBytesUnsigned(); }, "");
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

TEST(ByteCount, ArithmeticInvalid) {
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
