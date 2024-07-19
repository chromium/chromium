// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/power/suspend_perf_reporter.h"

#include <memory>

#include "base/metrics/histogram_macros.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
class SuspendPerfReporterTest : public testing::Test {
 protected:
  void SetUp() override {
    chromeos::PowerManagerClient::InitializeFake();
    reporter_ = std::make_unique<SuspendPerfReporter>(
        chromeos::PowerManagerClient::Get());
  }

  void TearDown() override {
    reporter_.reset();
    chromeos::PowerManagerClient::Shutdown();
  }

  std::unique_ptr<SuspendPerfReporter> reporter_;

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(SuspendPerfReporterTest, EmptyMetrics) {
  base::HistogramTester histogram_tester;
  chromeos::FakePowerManagerClient::Get()->SendSuspendDone();
  task_environment_.FastForwardBy(base::Minutes(1));

  EXPECT_TRUE(
      histogram_tester.GetAllSamples("Browser.MainThreadsCongestion").empty());
  EXPECT_TRUE(
      histogram_tester
          .GetAllSamples("Browser.MainThreadsCongestion.1MinAfterResume")
          .empty());
}

TEST_F(SuspendPerfReporterTest, IncreaseMetrics) {
  base::HistogramTester histogram_tester;
  UMA_HISTOGRAM_CUSTOM_COUNTS("Browser.MainThreadsCongestion", 10, 1, 300, 50);
  UMA_HISTOGRAM_CUSTOM_COUNTS("Browser.MainThreadsCongestion", 100, 1, 300, 50);
  // Metrics not changed during the 1 minute.
  UMA_HISTOGRAM_PERCENTAGE(
      "Graphics.Smoothness.PercentDroppedFrames3.AllAnimations", 10);
  chromeos::FakePowerManagerClient::Get()->SendSuspendDone();
  UMA_HISTOGRAM_CUSTOM_COUNTS("Browser.MainThreadsCongestion", 10, 1, 300, 50);
  UMA_HISTOGRAM_CUSTOM_COUNTS("Browser.MainThreadsCongestion", 200, 1, 300, 50);
  // A new metrics which is not registered before SuspendDone signal.
  UMA_HISTOGRAM_CUSTOM_TIMES("Ash.EventLatency.KeyPressed.TotalLatency",
                             base::Milliseconds(100), base::Milliseconds(1),
                             base::Seconds(5), 100);
  task_environment_.FastForwardBy(base::Minutes(1));

  histogram_tester.ExpectBucketCount("Browser.MainThreadsCongestion", 10, 2);
  histogram_tester.ExpectBucketCount(
      "Browser.MainThreadsCongestion.1MinAfterResume", 10, 1);
  histogram_tester.ExpectBucketCount("Browser.MainThreadsCongestion", 100, 1);
  histogram_tester.ExpectBucketCount(
      "Browser.MainThreadsCongestion.1MinAfterResume", 100, 0);
  histogram_tester.ExpectBucketCount("Browser.MainThreadsCongestion", 200, 1);
  histogram_tester.ExpectBucketCount(
      "Browser.MainThreadsCongestion.1MinAfterResume", 10, 1);
  histogram_tester.ExpectUniqueTimeSample(
      "Ash.EventLatency.KeyPressed.TotalLatency", base::Milliseconds(100), 1);
  histogram_tester.ExpectUniqueTimeSample(
      "Ash.EventLatency.KeyPressed.TotalLatency.1MinAfterResume",
      base::Milliseconds(100), 1);
  histogram_tester.ExpectUniqueSample(
      "Graphics.Smoothness.PercentDroppedFrames3.AllAnimations", 10, 1);
  EXPECT_TRUE(histogram_tester
                  .GetAllSamples("Graphics.Smoothness.PercentDroppedFrames3."
                                 "AllAnimations.1MinAfterResume")
                  .empty());
}

TEST_F(SuspendPerfReporterTest, AllMetrics) {
  base::HistogramTester histogram_tester;
  chromeos::FakePowerManagerClient::Get()->SendSuspendDone();
  UMA_HISTOGRAM_CUSTOM_COUNTS("Browser.MainThreadsCongestion", 10, 1, 300, 50);
  UMA_HISTOGRAM_CUSTOM_TIMES("Ash.EventLatency.MousePressed.TotalLatency",
                             base::Milliseconds(10), base::Milliseconds(1),
                             base::Seconds(5), 100);
  UMA_HISTOGRAM_CUSTOM_TIMES("Ash.EventLatency.KeyPressed.TotalLatency",
                             base::Milliseconds(20), base::Milliseconds(1),
                             base::Seconds(5), 100);
  UMA_HISTOGRAM_PERCENTAGE(
      "Graphics.Smoothness.PercentDroppedFrames3.AllAnimations", 10);
  UMA_HISTOGRAM_PERCENTAGE(
      "Graphics.Smoothness.PercentDroppedFrames3.AllInteractions", 100);
  UMA_HISTOGRAM_CUSTOM_COUNTS("MetricNotInList", 10, 1, 300, 50);
  task_environment_.FastForwardBy(base::Minutes(1));

  histogram_tester.ExpectUniqueSample(
      "Browser.MainThreadsCongestion.1MinAfterResume", 10, 1);
  histogram_tester.ExpectUniqueTimeSample(
      "Ash.EventLatency.MousePressed.TotalLatency.1MinAfterResume",
      base::Milliseconds(10), 1);
  histogram_tester.ExpectUniqueTimeSample(
      "Ash.EventLatency.KeyPressed.TotalLatency.1MinAfterResume",
      base::Milliseconds(20), 1);
  histogram_tester.ExpectUniqueSample(
      "Graphics.Smoothness.PercentDroppedFrames3.AllAnimations.1MinAfterResume",
      10, 1);
  histogram_tester.ExpectUniqueSample(
      "Graphics.Smoothness.PercentDroppedFrames3.AllInteractions."
      "1MinAfterResume",
      100, 1);

  EXPECT_TRUE(histogram_tester.GetAllSamples("MetricNotInList.1MinAfterResume")
                  .empty());
}

TEST_F(SuspendPerfReporterTest, MultipleSuspend) {
  base::HistogramTester histogram_tester;
  UMA_HISTOGRAM_CUSTOM_COUNTS("Browser.MainThreadsCongestion", 10, 1, 300, 50);
  // This will be replaced the next signal.
  chromeos::FakePowerManagerClient::Get()->SendSuspendDone();
  task_environment_.FastForwardBy(base::Seconds(30));
  UMA_HISTOGRAM_CUSTOM_COUNTS("Browser.MainThreadsCongestion", 20, 1, 300, 50);
  // This will be replaced the next signal again.
  chromeos::FakePowerManagerClient::Get()->SendSuspendDone();
  task_environment_.FastForwardBy(base::Seconds(30));
  UMA_HISTOGRAM_CUSTOM_COUNTS("Browser.MainThreadsCongestion", 30, 1, 300, 50);
  chromeos::FakePowerManagerClient::Get()->SendSuspendDone();
  UMA_HISTOGRAM_CUSTOM_COUNTS("Browser.MainThreadsCongestion", 40, 1, 300, 50);
  task_environment_.FastForwardBy(base::Minutes(1));

  histogram_tester.ExpectUniqueSample(
      "Browser.MainThreadsCongestion.1MinAfterResume", 40, 1);
}

}  // namespace ash
