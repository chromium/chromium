// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/glic_instance_limit_calculator.h"

#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {

class GlicInstanceLimitCalculatorTest : public testing::Test {
 protected:
  struct FeatureParams {
    size_t baseline_limit;
    size_t moderate_limit;
    size_t critical_limit;
  };

  void SetUpFeatures(const FeatureParams& params) {
    feature_list_.Reset();
    feature_list_.InitWithFeaturesAndParameters(
        {{kGlicMaxAwakeInstances,
          {{"limit", base::NumberToString(params.baseline_limit)},
           {"moderate_pressure_limit",
            base::NumberToString(params.moderate_limit)},
           {"critical_pressure_limit",
            base::NumberToString(params.critical_limit)}}}},
        {});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(GlicInstanceLimitCalculatorTest, ZeroPercentageReturnsCriticalLimit) {
  // At 0% memory budget, the limit is equal to the configured critical limit
  // (2).
  SetUpFeatures(
      {.baseline_limit = 15, .moderate_limit = 8, .critical_limit = 2});

  constexpr int kZeroPercentage = 0;
  EXPECT_EQ(CalculateAwakeInstancesLimit(15, kZeroPercentage), 2u);
}

TEST_F(GlicInstanceLimitCalculatorTest, FiftyPercentageReturnsModerateLimit) {
  // At 50% memory budget, the limit is equal to the moderate limit (8).
  SetUpFeatures(
      {.baseline_limit = 15, .moderate_limit = 8, .critical_limit = 1});

  constexpr int kFiftyPercentage = 50;
  EXPECT_EQ(CalculateAwakeInstancesLimit(15, kFiftyPercentage), 8u);
}

TEST_F(GlicInstanceLimitCalculatorTest, HundredPercentageReturnsBaselineLimit) {
  // At 100% memory budget, the limit is equal to the full baseline limit (15).
  SetUpFeatures(
      {.baseline_limit = 15, .moderate_limit = 8, .critical_limit = 1});

  constexpr int kHundredPercentage = 100;
  EXPECT_EQ(CalculateAwakeInstancesLimit(15, kHundredPercentage), 15u);
}

TEST_F(GlicInstanceLimitCalculatorTest,
       InterpolationBetweenCriticalAndModerate) {
  // At 25% memory budget (halfway between 0% and 50%), the limit is halfway
  // between the critical limit (2) and moderate limit (8): 2 + 0.5 * 6 = 5.
  SetUpFeatures(
      {.baseline_limit = 15, .moderate_limit = 8, .critical_limit = 2});

  constexpr int kQuarterPercentage = 25;
  EXPECT_EQ(CalculateAwakeInstancesLimit(15, kQuarterPercentage), 5u);
}

TEST_F(GlicInstanceLimitCalculatorTest,
       InterpolationBetweenModerateAndBaseline) {
  // At 75% memory budget (halfway between 50% and 100%), the limit is halfway
  // between the moderate limit (8) and baseline limit (15):
  // 8 + 0.5 * 7 = 11.5, rounded to 12.
  SetUpFeatures(
      {.baseline_limit = 15, .moderate_limit = 8, .critical_limit = 2});

  constexpr int kThreeQuarterPercentage = 75;
  EXPECT_EQ(CalculateAwakeInstancesLimit(15, kThreeQuarterPercentage), 12u);
}

TEST_F(GlicInstanceLimitCalculatorTest, ExtrapolationAboveHundredPercent) {
  // At memory budgets above 100%, the limit extrapolates based on the slope
  // between 50% and 100% (slope = (15 - 8) / 50 = 0.14 per percent).
  SetUpFeatures(
      {.baseline_limit = 15, .moderate_limit = 8, .critical_limit = 2});

  // At 200%: 8 + (200 - 50) * 0.14 = 8 + 21 = 29.
  constexpr int kTwoHundredPercentage = 200;
  EXPECT_EQ(CalculateAwakeInstancesLimit(15, kTwoHundredPercentage), 29u);

  // At 150%: 8 + (150 - 50) * 0.14 = 8 + 14 = 22.
  constexpr int kOneHundredFiftyPercentage = 150;
  EXPECT_EQ(CalculateAwakeInstancesLimit(15, kOneHundredFiftyPercentage), 22u);
}

TEST_F(GlicInstanceLimitCalculatorTest, MonotonicityClampingPreventsInversion) {
  // Verify that if `critical_limit` is misconfigured to be greater than
  // `moderate_limit` (50 > 8), the implementation clamps `critical_limit` down
  // to `moderate_limit` (8). This enforces monotonic scaling across pressure
  // levels, guaranteeing that higher memory pressure never results in a larger
  // awake instances limit.
  SetUpFeatures(
      {.baseline_limit = 15, .moderate_limit = 8, .critical_limit = 50});

  constexpr int kZeroPercentage = 0;
  EXPECT_EQ(CalculateAwakeInstancesLimit(15, kZeroPercentage), 8u);
}

}  // namespace glic
