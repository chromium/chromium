// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory_coordinator/memory_limit.h"

#include <cstdint>

#include "base/byte_size.h"
#include "base/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TEST(MemoryLimitTest, Constants) {
  EXPECT_EQ(MemoryLimit::Default().percent(), 100);
  EXPECT_DOUBLE_EQ(MemoryLimit::Default().ratio(), 1.0);

  EXPECT_EQ(MemoryLimit::NoPressureThreshold().percent(), 100);
  EXPECT_DOUBLE_EQ(MemoryLimit::NoPressureThreshold().ratio(), 1.0);

  EXPECT_EQ(MemoryLimit::ModeratePressureThreshold().percent(), 50);
  EXPECT_DOUBLE_EQ(MemoryLimit::ModeratePressureThreshold().ratio(), 0.5);

  EXPECT_EQ(MemoryLimit::CriticalPressureThreshold().percent(), 0);
  EXPECT_DOUBLE_EQ(MemoryLimit::CriticalPressureThreshold().ratio(), 0.0);
}

TEST(MemoryLimitTest, Factories) {
  constexpr MemoryLimit limit_percent = MemoryLimit::FromPercent(75);
  EXPECT_EQ(limit_percent.percent(), 75);
  EXPECT_DOUBLE_EQ(limit_percent.ratio(), 0.75);
}

TEST(MemoryLimitTest, ImplicitConversion) {
  // Implicit construction from int.
  MemoryLimit limit = 40;
  EXPECT_EQ(limit.percent(), 40);

  // Implicit conversion to int.
  int raw_int = limit;
  EXPECT_EQ(raw_int, 40);
}

TEST(MemoryLimitTest, Comparisons) {
  constexpr MemoryLimit limit_50 = MemoryLimit::FromPercent(50);
  constexpr MemoryLimit limit_100 = MemoryLimit::FromPercent(100);

  EXPECT_EQ(limit_50, MemoryLimit::ModeratePressureThreshold());
  EXPECT_NE(limit_50, limit_100);
  EXPECT_LT(limit_50, limit_100);
  EXPECT_LE(limit_50, limit_100);
  EXPECT_LE(limit_50, limit_50);
  EXPECT_GT(limit_100, limit_50);
  EXPECT_GE(limit_100, limit_50);
  EXPECT_GE(limit_100, limit_100);
}

TEST(MemoryLimitTest, ScaleIntegral) {
  constexpr MemoryLimit limit_50 = MemoryLimit::FromPercent(50);
  EXPECT_EQ(limit_50.Scale(100), 50);
  EXPECT_EQ(limit_50.Scale(100u), 50u);
  EXPECT_EQ(limit_50.Scale(1000ULL), 500ULL);

  // Truncation towards zero (15% of 10 is 1.5 -> 1).
  constexpr MemoryLimit limit_15 = MemoryLimit::FromPercent(15);
  EXPECT_EQ(limit_15.Scale(10), 1);
  EXPECT_EQ(limit_15.Scale(10u), 1u);

  // Zero limit.
  EXPECT_EQ(MemoryLimit::CriticalPressureThreshold().Scale(100), 0);

  // Scaling above 100%.
  constexpr MemoryLimit limit_200 = MemoryLimit::FromPercent(200);
  EXPECT_EQ(limit_200.Scale(100), 200);
  EXPECT_EQ(limit_200.Scale<int8_t>(100), 127);  // Saturation
}

TEST(MemoryLimitTest, ScaleByteSize) {
  constexpr MemoryLimit limit_50 = MemoryLimit::FromPercent(50);
  EXPECT_EQ(limit_50.Scale(KiB(100)), KiB(50));
}

#if defined(GTEST_HAS_DEATH_TEST)
TEST(MemoryLimitTest, NegativePercentDeathTest) {
  EXPECT_CHECK_DEATH({ MemoryLimit(-1); });
}
#endif

}  // namespace base
