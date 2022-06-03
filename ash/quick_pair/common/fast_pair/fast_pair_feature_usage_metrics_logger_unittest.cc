// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/quick_pair/common/fast_pair/fast_pair_feature_usage_metrics_logger.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chromeos/components/feature_usage/feature_usage_metrics.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace quick_pair {

class FastPairFeatureUsageMetricsLoggerTest : public ::testing::Test {
 protected:
  FastPairFeatureUsageMetricsLoggerTest() = default;
  ~FastPairFeatureUsageMetricsLoggerTest() override = default;

  void SetUp() override {}

  base::test::TaskEnvironment task_environment_;
};

TEST_F(FastPairFeatureUsageMetricsLoggerTest, IsEligible) {
  FastPairFeatureUsageMetricsLogger feature_usage_metrics;
  EXPECT_TRUE(feature_usage_metrics.IsEligible());
}

TEST_F(FastPairFeatureUsageMetricsLoggerTest, IsEnabled) {
  FastPairFeatureUsageMetricsLogger feature_usage_metrics;
  EXPECT_TRUE(feature_usage_metrics.IsEnabled());
}

TEST_F(FastPairFeatureUsageMetricsLoggerTest, RecordUsage) {
  FastPairFeatureUsageMetricsLogger feature_usage_metrics;

  base::HistogramTester histograms;
  histograms.ExpectBucketCount(
      "ChromeOS.FeatureUsage.FastPair",
      feature_usage::FeatureUsageMetrics::Event::kUsedWithSuccess, 0);
  histograms.ExpectBucketCount(
      "ChromeOS.FeatureUsage.FastPair",
      feature_usage::FeatureUsageMetrics::Event::kUsedWithFailure, 0);

  feature_usage_metrics.RecordUsage(/*success=*/true);
  histograms.ExpectBucketCount(
      "ChromeOS.FeatureUsage.FastPair",
      feature_usage::FeatureUsageMetrics::Event::kUsedWithSuccess, 1);
  histograms.ExpectBucketCount(
      "ChromeOS.FeatureUsage.FastPair",
      feature_usage::FeatureUsageMetrics::Event::kUsedWithFailure, 0);

  feature_usage_metrics.RecordUsage(/*success=*/false);
  histograms.ExpectBucketCount(
      "ChromeOS.FeatureUsage.FastPair",
      feature_usage::FeatureUsageMetrics::Event::kUsedWithSuccess, 1);
  histograms.ExpectBucketCount(
      "ChromeOS.FeatureUsage.FastPair",
      feature_usage::FeatureUsageMetrics::Event::kUsedWithFailure, 1);
}

}  // namespace quick_pair
}  // namespace ash
