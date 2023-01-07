// Copyright 2021 The Chromium Authors
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
const char kDiagnosticsUmaFeatureUsetimeFullPath[] =
    "ChromeOS.FeatureUsage.DiagnosticsUi.Usetime";
const base::TimeDelta kDefaultTime = base::Minutes(10);

class DiagnosticsMetricsTest : public testing::Test {
 public:
  DiagnosticsMetricsTest() = default;
  ~DiagnosticsMetricsTest() override = default;

  void AdvanceClock(base::TimeDelta time) {
    task_environment_.AdvanceClock(time);
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
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

TEST_F(DiagnosticsMetricsTest, RecordUseTime) {
  base::HistogramTester histograms;
  DiagnosticsMetrics metrics;
  size_t expected_use_time_record_count = 0;

  // Initial state for usage timing.
  EXPECT_FALSE(metrics.GetSuccessfulUsageStartedForTesting());
  histograms.ExpectTimeBucketCount(kDiagnosticsUmaFeatureUsetimeFullPath,
                                   /** sample */ base::Minutes(0),
                                   expected_use_time_record_count);

  metrics.RecordUsage(false);
  AdvanceClock(kDefaultTime);
  metrics.StopSuccessfulUsage();

  // Usetime is related to successful usage only.
  EXPECT_FALSE(metrics.GetSuccessfulUsageStartedForTesting());
  histograms.ExpectTimeBucketCount(kDiagnosticsUmaFeatureUsetimeFullPath,
                                   /** sample */ kDefaultTime,
                                   expected_use_time_record_count);

  // Start recording `Usetime`.
  expected_use_time_record_count += 1;
  metrics.RecordUsage(true);
  EXPECT_TRUE(metrics.GetSuccessfulUsageStartedForTesting());

  // Move clock and stop recording `Usetime`.
  AdvanceClock(kDefaultTime);
  metrics.StopSuccessfulUsage();

  EXPECT_FALSE(metrics.GetSuccessfulUsageStartedForTesting());
  histograms.ExpectTimeBucketCount(kDiagnosticsUmaFeatureUsetimeFullPath,
                                   /** sample */ kDefaultTime,
                                   expected_use_time_record_count);
}
}  // namespace metrics
}  // namespace diagnostics
}  // namespace ash
