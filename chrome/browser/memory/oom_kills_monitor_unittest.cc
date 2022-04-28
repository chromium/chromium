// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/memory/oom_kills_monitor.h"

#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace memory {

namespace {

base::HistogramBase* GetOOMKillsCountHistogram() {
  return base::StatisticsRecorder::FindHistogram("Memory.OOMKills.Count");
}

}  // namespace.

class OOMKillsMonitorTest : public testing::Test {
 private:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(OOMKillsMonitorTest, TestHistograms) {
  std::unique_ptr<base::StatisticsRecorder> statistic_recorder(
      base::StatisticsRecorder::CreateTemporaryForTesting());

  OOMKillsMonitor& monitor_instance = OOMKillsMonitor::GetInstance();

  auto* oom_count_histogram = GetOOMKillsCountHistogram();
  // Before StartMonitoring() is called, nothing is recorded.
  ASSERT_FALSE(oom_count_histogram);

  // Start monitoring.
  monitor_instance.Initialize();
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
}

}  // namespace memory
