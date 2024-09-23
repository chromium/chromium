// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/external_metrics/external_metrics.h"

#include <memory>

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/metrics/statistics_recorder.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
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
    // Create directory containing UMA metrics after the stateful partition is
    // mounted.
    ASSERT_TRUE(uma_events_dir_.CreateUniqueTempDir());
    std::string test_uma_dir =
        uma_events_dir_.GetPath().Append("test-uma-events.d").value();
    ASSERT_TRUE(base::CreateDirectory(base::FilePath(test_uma_dir)));

    // Create directory containing early UMA metrics before the stateful
    // partition is mounted. For the tests, we are assuming this directory
    // exists before the stateful partition is mounted.
    ASSERT_TRUE(uma_early_metrics_dir_.CreateUniqueTempDir());
    std::string test_early_uma_dir = uma_early_metrics_dir_.GetPath()
                                         .Append("metrics/early-metrics")
                                         .value();
    ASSERT_TRUE(base::CreateDirectory(base::FilePath(test_early_uma_dir)));

    external_metrics_ = ExternalMetrics::CreateForTesting(
        uma_events_dir_.GetPath().Append("testfile").value(), test_uma_dir,
        test_early_uma_dir);
  }

  void WriteSampleMetricToFile(const base::FilePath& file_path,
                               std::string histogram_name,
                               int num_of_samples) {
    base::HistogramTester histogram_tester;
    base::UserActionTester action_tester;
    std::unique_ptr<metrics::MetricSample> sample =
        metrics::MetricSample::LinearHistogramSample(
            histogram_name, /*sample=*/1, /*max=*/2,
            /*num_samples=*/num_of_samples);

    EXPECT_TRUE(metrics::SerializationUtils::WriteMetricToFile(
        *sample, file_path.value()));
  }

  base::ScopedTempDir uma_events_dir_;
  base::ScopedTempDir uma_early_metrics_dir_;
  scoped_refptr<ExternalMetrics> external_metrics_;
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(ExternalMetricsTest, CustomInterval) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kExternalMetricsCollectionInterval, "5");
  Init();

  EXPECT_EQ(base::Seconds(5), external_metrics_->collection_interval_);
}

TEST_F(ExternalMetricsTest, ProcessSamplesSuccessfully) {
  // Test the variety of histogram functions to ensure all the supported
  // metric types are collected.
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

  size_t pid = 0;
  for (const auto& sample : samples) {
    std::string pid_uma_file =
        base::StrCat({external_metrics_->uma_events_dir_, "/",
                      base::NumberToString(pid), ".uma"});
    EXPECT_TRUE(
        metrics::SerializationUtils::WriteMetricToFile(*sample, pid_uma_file));
    ++pid;
  }

  base::RunLoop loop;

  EXPECT_EQ(expected_samples, external_metrics_->CollectEvents());

  loop.RunUntilIdle();

  EXPECT_EQ(action_tester.GetActionCount("foo_useraction"), 100);
  histogram_tester.ExpectTotalCount("foo_histogram", 2);
  histogram_tester.ExpectTotalCount("foo_linear", 3);
  histogram_tester.ExpectTotalCount("foo_sparse", 20);
  EXPECT_TRUE(base::IsDirectoryEmpty(
      base::FilePath(external_metrics_->uma_events_dir_)));
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

TEST_F(ExternalMetricsTest, CanReceiveHistogramFromPidFiles) {
  Init();
  base::HistogramTester histogram_tester;
  base::UserActionTester action_tester;

  int expected_samples = 0;
  const size_t expected_num_files = 10;
  std::vector<std::unique_ptr<metrics::MetricSample>> samples;

  // We do not test CrashSample here because the underlying class,
  // ChromeOSMetricsProvider, relies heavily on |g_browser_process|, which is
  // not set in unit tests.
  // It is not currently possible to inject a mock for ChromeOSMetricsProvider.

  samples.push_back(metrics::MetricSample::UserActionSample(
      "foo_useraction", /*num_samples=*/100));
  expected_samples += 100 * expected_num_files;

  samples.push_back(metrics::MetricSample::HistogramSample(
      "foo_histogram", /*sample=*/2, /*min=*/1,
      /*max=*/100, /*bucket_count=*/10,
      /*num_samples=*/2));
  expected_samples += 2 * expected_num_files;

  samples.push_back(metrics::MetricSample::LinearHistogramSample(
      "foo_linear", /*sample=*/1, /*max=*/2, /*num_samples=*/3));
  expected_samples += 3 * expected_num_files;

  samples.push_back(metrics::MetricSample::SparseHistogramSample(
      "foo_sparse", /*sample=*/1, /*num_samples=*/20));
  expected_samples += 20 * expected_num_files;

  size_t pid = 0;
  for (const auto& sample : samples) {
    // Create 10 of each file.
    for (size_t count = 0; count < expected_num_files; count++) {
      std::string pid_uma_file =
          base::StrCat({external_metrics_->uma_events_dir_, "/",
                        base::NumberToString(pid), ".uma"});
      EXPECT_TRUE(metrics::SerializationUtils::WriteMetricToFile(*sample,
                                                                 pid_uma_file));
      ++pid;
    }
  }

  base::RunLoop loop;

  EXPECT_EQ(expected_samples, external_metrics_->CollectEvents());

  loop.RunUntilIdle();

  EXPECT_EQ(action_tester.GetActionCount("foo_useraction"), 1000);
  histogram_tester.ExpectTotalCount("foo_histogram", 20);
  histogram_tester.ExpectTotalCount("foo_linear", 30);
  histogram_tester.ExpectTotalCount("foo_sparse", 200);
  EXPECT_TRUE(base::IsDirectoryEmpty(
      base::FilePath(external_metrics_->uma_events_dir_)));
}

TEST_F(ExternalMetricsTest, CanReceiveMetricsFromEarlyMetricsDir) {
  Init();

  // Create filepaths under the early metrics directory to write metrics.
  base::FilePath foo_histogram_1 =
      base::FilePath(external_metrics_->uma_early_metrics_dir_)
          .Append("pid_early1.uma");
  base::FilePath foo_histogram_2 =
      base::FilePath(external_metrics_->uma_early_metrics_dir_)
          .Append("pid_early2.uma");

  // Expected number of samples of each histogram written to file.
  int foo_histogram_1_samples = 5;
  int foo_histogram_2_samples = 8;

  WriteSampleMetricToFile(foo_histogram_1, "foo_histogram_1",
                          foo_histogram_1_samples);  // 5 samples
  WriteSampleMetricToFile(foo_histogram_2, "foo_histogram_2",
                          foo_histogram_2_samples);  // 8 samples

  // Expectation from the test data.
  base::HistogramTester histogram_tester;
  int expected_samples = foo_histogram_1_samples + foo_histogram_2_samples;

  // Exercise the function to parse and collect the early metric event
  // histograms.
  EXPECT_EQ(expected_samples, external_metrics_->CollectEvents());

  // Verify expected behavior: the number of histograms written to file matches
  // the number of histograms parsed by the test. This ensures that histogram
  // data is correctly logged and processed.
  histogram_tester.ExpectTotalCount("foo_histogram_1", foo_histogram_1_samples);
  histogram_tester.ExpectTotalCount("foo_histogram_2", foo_histogram_2_samples);

  // Verify that the |uma_early_metrics_dir_| is cleaned up after
  // external_metrics_->CollectEvents.
  EXPECT_TRUE(base::IsDirectoryEmpty(
      base::FilePath(external_metrics_->uma_early_metrics_dir_)));
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
