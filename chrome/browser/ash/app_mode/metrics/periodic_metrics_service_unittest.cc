// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/metrics/periodic_metrics_service.h"

#include <string>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/sync_wifi/network_test_helper.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/user_activity/user_activity_detector.h"

namespace ash {

class BasePeriodicMetricsServiceTest {
 public:
  BasePeriodicMetricsServiceTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        network_handler_test_helper_(
            std::make_unique<NetworkHandlerTestHelper>()),
        histogram_tester_(std::make_unique<base::HistogramTester>()) {}

  BasePeriodicMetricsServiceTest(const BasePeriodicMetricsServiceTest&) =
      delete;
  BasePeriodicMetricsServiceTest& operator=(
      const BasePeriodicMetricsServiceTest&) = delete;

  base::test::TaskEnvironment* task_environment() { return &task_environment_; }

  base::HistogramTester* histogram_tester() { return histogram_tester_.get(); }

  void StartRecordingPeriodicMetrics() {
    periodic_metrics_service_.StartRecordingPeriodicMetrics();
    // Some periodic metrics are calculated asynchronously.
    task_environment_.RunUntilIdle();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  sync_preferences::TestingPrefServiceSyncable user_prefs_;
  std::unique_ptr<NetworkHandlerTestHelper> network_handler_test_helper_;
  PeriodicMetricsService periodic_metrics_service_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

class PeriodicMetricsServiceTest : public BasePeriodicMetricsServiceTest,
                                   public ::testing::Test {
 public:
  PeriodicMetricsServiceTest() = default;
  PeriodicMetricsServiceTest(const PeriodicMetricsServiceTest&) = delete;
  PeriodicMetricsServiceTest& operator=(const PeriodicMetricsServiceTest&) =
      delete;
};

TEST_F(PeriodicMetricsServiceTest, PeriodicMetrics) {
  const char* const kPeriodicMetrics[] = {kKioskRamUsagePercentageHistogram,
                                          kKioskSwapUsagePercentageHistogram};
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
