// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/diagnostics_ui/diagnostics_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace diagnostics {
namespace metrics {
namespace {
const char kDiagnosticsUmaFeatureFullPath[] =
    "ChromeOS.FeatureUsage.DiagnosticsUi";

class DiagnosticsMetricsTest : public testing::Test {
 public:
  DiagnosticsMetricsTest() = default;

  ~DiagnosticsMetricsTest() override = default;

 protected:
  base::test::TaskEnvironment task_environment_;
};
}  // namespace

// DiagnosticsMetricsTest is part of the ash_unittests and will only be called
// when is_chromeos_ash is true. Eligible and enabled currently will always be
// true.
TEST_F(DiagnosticsMetricsTest, IsEligibleAndEnabled) {
  DiagnosticsMetrics metrics;

  EXPECT_TRUE(metrics.IsEligible());
  EXPECT_TRUE(metrics.IsEnabled());
}

TEST_F(DiagnosticsMetricsTest, RecordUsage) {
  base::HistogramTester histograms;
  DiagnosticsMetrics metrics;
  size_t expected_success = 0;
  size_t expected_failure = 0;

  // Before RecordUsage has been triggered both events should be zero.
  histograms.ExpectBucketCount(
      kDiagnosticsUmaFeatureFullPath,
      feature_usage::FeatureUsageMetrics::Event::kUsedWithSuccess,
      expected_success);
  histograms.ExpectBucketCount(
      kDiagnosticsUmaFeatureFullPath,
      feature_usage::FeatureUsageMetrics::Event::kUsedWithFailure,
      expected_failure);

  expected_failure += 1;
  metrics.RecordUsage(false);
  // After RecordUsage(false) has been triggered failure should increment.
  histograms.ExpectBucketCount(
      kDiagnosticsUmaFeatureFullPath,
      feature_usage::FeatureUsageMetrics::Event::kUsedWithSuccess,
      expected_success);
  histograms.ExpectBucketCount(
      kDiagnosticsUmaFeatureFullPath,
      feature_usage::FeatureUsageMetrics::Event::kUsedWithFailure,
      expected_failure);

  expected_success += 1;
  metrics.RecordUsage(true);
  // After RecordUsage(true) has been triggered success should increment.
  histograms.ExpectBucketCount(
      kDiagnosticsUmaFeatureFullPath,
      feature_usage::FeatureUsageMetrics::Event::kUsedWithSuccess,
      expected_success);
  histograms.ExpectBucketCount(
      kDiagnosticsUmaFeatureFullPath,
      feature_usage::FeatureUsageMetrics::Event::kUsedWithFailure,
      expected_failure);
}
}  // namespace metrics
}  // namespace diagnostics
}  // namespace ash
