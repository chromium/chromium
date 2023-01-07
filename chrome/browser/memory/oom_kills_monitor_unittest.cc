// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/memory/oom_kills_monitor.h"

#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace memory {

namespace {

base::HistogramBase* GetOOMKillsCountHistogram() {
  return base::StatisticsRecorder::FindHistogram(
      OOMKillsMonitor::kOOMKillsCountHistogramName);
}

base::HistogramBase* GetOOMKillsDailyHistogram() {
  return base::StatisticsRecorder::FindHistogram(
      OOMKillsMonitor::kOOMKillsDailyHistogramName);
}

}  // namespace.

class OOMKillsMonitorTest : public testing::Test {
 public:
  OOMKillsMonitorTest() {
    OOMKillsMonitor::RegisterPrefs(pref_service_.registry());
  }

  TestingPrefServiceSimple pref_service_;

 private:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(OOMKillsMonitorTest, TestHistograms) {
  std::unique_ptr<base::StatisticsRecorder> statistic_recorder(
      base::StatisticsRecorder::CreateTemporaryForTesting());

  OOMKillsMonitor& monitor_instance = OOMKillsMonitor::GetInstance();

  auto* oom_count_histogram = GetOOMKillsCountHistogram();
  auto* oom_daily_histogram = GetOOMKillsDailyHistogram();
  // Before StartMonitoring() is called, nothing is recorded.
  ASSERT_FALSE(oom_count_histogram);
  ASSERT_FALSE(oom_daily_histogram);

  // Start monitoring.
  monitor_instance.Initialize(&pref_service_);
  monitor_instance.StopTimersForTesting();
  oom_count_histogram = GetOOMKillsCountHistogram();
  ASSERT_TRUE(oom_count_histogram);
  {
    auto count_samples = oom_count_histogram->SnapshotSamples();
    EXPECT_EQ(1, count_samples->TotalCount());
    EXPECT_EQ(1, count_samples->GetCount(0));
  }

  // OOM kills.
  // Simulate getting 3 more oom kills.
  monitor_instance.CheckOOMKillImpl(monitor_instance.last_oom_kills_count_ + 3);

  oom_count_histogram = GetOOMKillsCountHistogram();
  ASSERT_TRUE(oom_count_histogram);
  {
    auto count_samples = oom_count_histogram->SnapshotSamples();
    EXPECT_EQ(4, count_samples->TotalCount());
    // The zero count is implicitly added when StartMonitoring() is called.
    EXPECT_EQ(1, count_samples->GetCount(0));
    EXPECT_EQ(1, count_samples->GetCount(1));
    EXPECT_EQ(1, count_samples->GetCount(2));
    EXPECT_EQ(1, count_samples->GetCount(3));
  }

  // Simulate 2 ARCVM oom kills.
  monitor_instance.LogArcOOMKill(2);
  oom_count_histogram = GetOOMKillsCountHistogram();
  ASSERT_TRUE(oom_count_histogram);
  {
    auto count_samples = oom_count_histogram->SnapshotSamples();
    EXPECT_EQ(6, count_samples->TotalCount());
    EXPECT_EQ(1, count_samples->GetCount(0));
    EXPECT_EQ(1, count_samples->GetCount(1));
    EXPECT_EQ(1, count_samples->GetCount(2));
    EXPECT_EQ(1, count_samples->GetCount(3));
    EXPECT_EQ(1, count_samples->GetCount(4));
    EXPECT_EQ(1, count_samples->GetCount(5));
  }

  oom_count_histogram = GetOOMKillsCountHistogram();
  ASSERT_TRUE(oom_count_histogram);
  {
    auto count_samples = oom_count_histogram->SnapshotSamples();
    // Ensure zero count is not increased.
    EXPECT_EQ(1, count_samples->GetCount(0));
  }

  // Test daily OOM kills metrics.
  monitor_instance.TriggerDailyEventForTesting();
  oom_daily_histogram = GetOOMKillsDailyHistogram();
  {
    auto daily_samples = oom_daily_histogram->SnapshotSamples();
    EXPECT_EQ(1, daily_samples->TotalCount());
    EXPECT_EQ(1, daily_samples->GetCount(5));
  }

  // Simulate getting 4 more oom kills.
  monitor_instance.CheckOOMKillImpl(monitor_instance.last_oom_kills_count_ + 4);
  monitor_instance.TriggerDailyEventForTesting();
  oom_daily_histogram = GetOOMKillsDailyHistogram();
  {
    auto daily_samples = oom_daily_histogram->SnapshotSamples();
    EXPECT_EQ(2, daily_samples->TotalCount());
    EXPECT_EQ(1, daily_samples->GetCount(4));
    EXPECT_EQ(1, daily_samples->GetCount(5));
  }
}

}  // namespace memory
