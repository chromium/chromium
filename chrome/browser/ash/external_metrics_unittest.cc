// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/external_metrics.h"

#include <memory>

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/metrics/statistics_recorder.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "components/metrics/serialization/metric_sample.h"
#include "components/metrics/serialization/serialization_utils.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

class ExternalMetricsTest : public testing::Test {
 public:
  ExternalMetricsTest() = default;
  ~ExternalMetricsTest() override = default;

  void Init() {
    ASSERT_TRUE(dir_.CreateUniqueTempDir());
    external_metrics_ = ExternalMetrics::CreateForTesting(
        dir_.GetPath().Append("testfile").value());
  }

  base::ScopedTempDir dir_;
  scoped_refptr<ExternalMetrics> external_metrics_;
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(ExternalMetricsTest, CustomInterval) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kExternalMetricsCollectionInterval, "5");
  Init();

  EXPECT_EQ(base::Seconds(5), external_metrics_->collection_interval_);
}

TEST_F(ExternalMetricsTest, HandleMissingFile) {
  Init();
  ASSERT_TRUE(
      base::DeleteFile(base::FilePath(external_metrics_->uma_events_file_)));

  EXPECT_EQ(0, external_metrics_->CollectEvents());
}

TEST_F(ExternalMetricsTest, CanReceiveHistogram) {
  Init();
  base::HistogramTester histogram_tester;
  base::UserActionTester action_tester;

  int expected_samples = 0;
  std::vector<std::unique_ptr<metrics::MetricSample>> samples;

  // We do not test CrashSample here because the underlying class,
  // ChromeOSMetricsProvider, relies heavily on |g_browser_process|, which is
  // not set in unit tests.
  // It is not currently possible to inject a mock for ChromeOSMetricsProvider.

  samples.push_back(metrics::MetricSample::UserActionSample(
      "foo_useraction", /*num_samples=*/100));
  expected_samples += 100;

  samples.push_back(metrics::MetricSample::HistogramSample(
      "foo_histogram", /*sample=*/2, /*min=*/1,
      /*max=*/100, /*bucket_count=*/10,
      /*num_samples=*/2));
  expected_samples += 2;

  samples.push_back(metrics::MetricSample::LinearHistogramSample(
      "foo_linear", /*sample=*/1, /*max=*/2, /*num_samples=*/3));
  expected_samples += 3;

  samples.push_back(metrics::MetricSample::SparseHistogramSample(
      "foo_sparse", /*sample=*/1, /*num_samples=*/20));
  expected_samples += 20;

  for (const auto& sample : samples) {
    EXPECT_TRUE(metrics::SerializationUtils::WriteMetricToFile(
        *sample, external_metrics_->uma_events_file_));
  }

  base::RunLoop loop;

  EXPECT_EQ(expected_samples, external_metrics_->CollectEvents());

  loop.RunUntilIdle();

  EXPECT_EQ(action_tester.GetActionCount("foo_useraction"), 100);
  histogram_tester.ExpectTotalCount("foo_histogram", 2);
  histogram_tester.ExpectTotalCount("foo_linear", 3);
  histogram_tester.ExpectTotalCount("foo_sparse", 20);
}

TEST_F(ExternalMetricsTest, IncorrectHistogramsAreDiscarded) {
  Init();
  base::HistogramTester histogram_tester;

  // Malformed histogram (min > max).
  std::unique_ptr<metrics::MetricSample> hist =
      metrics::MetricSample::HistogramSample("bar", /*sample=*/30, /*min=*/200,
                                             /*max=*/20, /*bucket_count=*/10,
                                             /*num_samples=*/1);

  EXPECT_TRUE(metrics::SerializationUtils::WriteMetricToFile(
      *hist.get(), external_metrics_->uma_events_file_));

  external_metrics_->CollectEvents();

  histogram_tester.ExpectTotalCount("bar", 0);
}

}  // namespace ash
