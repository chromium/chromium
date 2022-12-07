// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/metrics/periodic_metrics_service.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

class PeriodicMetricsServiceTest : public testing::Test {
 public:
  PeriodicMetricsServiceTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        histogram_tester_(std::make_unique<base::HistogramTester>()) {}

  PeriodicMetricsServiceTest(const PeriodicMetricsServiceTest&) = delete;
  PeriodicMetricsServiceTest& operator=(const PeriodicMetricsServiceTest&) =
      delete;

  base::test::TaskEnvironment* task_environment() { return &task_environment_; }

  base::HistogramTester* histogram_tester() { return histogram_tester_.get(); }

  void StartRecordingPeriodicMetrics() {
    periodic_metrics_service.StartRecordingPeriodicMetrics();
    // Some periodic metrics are calculated asynchronously.
    task_environment_.RunUntilIdle();
  }

 private:
  PeriodicMetricsService periodic_metrics_service;
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

TEST_F(PeriodicMetricsServiceTest, PeriodicMetrics) {
  const char* const kPeriodicMetrics[] = {
      kKioskRamUsagePercentageHistogram, kKioskSwapUsagePercentageHistogram,
      kKioskDiskUsagePercentageHistogram, kKioskChromeProcessCountHistogram};
  for (const char* metric : kPeriodicMetrics) {
    histogram_tester()->ExpectTotalCount(metric, 0);
  }

  StartRecordingPeriodicMetrics();
  // Check that periodic metrics were recoreded right calling
  // `StartRecordingPeriodicMetrics`.
  for (const char* metric : kPeriodicMetrics) {
    histogram_tester()->ExpectTotalCount(metric, 1);
  }

  // Next time periodic metrics should be recorded only after
  // `kPeriodicMetricsInterval`.
  task_environment()->FastForwardBy(kPeriodicMetricsInterval / 2);
  for (const char* metric : kPeriodicMetrics) {
    histogram_tester()->ExpectTotalCount(metric, 1);
  }

  task_environment()->FastForwardBy(kPeriodicMetricsInterval / 2);
  for (const char* metric : kPeriodicMetrics) {
    histogram_tester()->ExpectTotalCount(metric, 2);
  }
}

}  // namespace ash
