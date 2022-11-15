// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/memory/memory_kills_monitor.h"

#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "base/time/time.h"
#include "chrome/browser/memory/memory_kills_histogram.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace memory {

namespace {

base::HistogramBase* GetLowMemoryKillsCountHistogram() {
  return base::StatisticsRecorder::FindHistogram(
      "Memory.LowMemoryKiller.Count");
}

}  // namespace.

class MemoryKillsMonitorTest : public testing::Test {
 private:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(MemoryKillsMonitorTest, TestHistograms) {
  std::unique_ptr<base::StatisticsRecorder> statistic_recorder(
      base::StatisticsRecorder::CreateTemporaryForTesting());

  MemoryKillsMonitor* g_memory_kills_monitor_unittest_instance =
      MemoryKillsMonitor::GetForTesting();

  MemoryKillsMonitor::LogLowMemoryKill("APP", 123);
  MemoryKillsMonitor::LogLowMemoryKill("APP", 100);
  MemoryKillsMonitor::LogLowMemoryKill("TAB", 10000);

  auto* lmk_count_histogram = GetLowMemoryKillsCountHistogram();
  // Before StartMonitoring() is called, nothing is recorded.
  ASSERT_FALSE(lmk_count_histogram);

  // Start monitoring.
  g_memory_kills_monitor_unittest_instance->StartMonitoring();
  lmk_count_histogram = GetLowMemoryKillsCountHistogram();
  ASSERT_TRUE(lmk_count_histogram);
  {
    auto count_samples = lmk_count_histogram->SnapshotSamples();
    EXPECT_EQ(1, count_samples->TotalCount());
    EXPECT_EQ(1, count_samples->GetCount(0));
  }

  // Low memory kills.
  MemoryKillsMonitor::LogLowMemoryKill("APP", 123);
  MemoryKillsMonitor::LogLowMemoryKill("APP", 100);
  MemoryKillsMonitor::LogLowMemoryKill("TAB", 10000);
  lmk_count_histogram = GetLowMemoryKillsCountHistogram();
  ASSERT_TRUE(lmk_count_histogram);
  {
    auto count_samples = lmk_count_histogram->SnapshotSamples();
    EXPECT_EQ(4, count_samples->TotalCount());
    // The zero count is implicitly added when StartMonitoring() is called.
    EXPECT_EQ(1, count_samples->GetCount(0));
    EXPECT_EQ(1, count_samples->GetCount(1));
    EXPECT_EQ(1, count_samples->GetCount(2));
    EXPECT_EQ(1, count_samples->GetCount(3));
  }

  {
    auto* histogram_freed_size = base::StatisticsRecorder::FindHistogram(
        "Memory.LowMemoryKiller.FreedSize");
    ASSERT_TRUE(histogram_freed_size);
    auto freed_size_samples = histogram_freed_size->SnapshotSamples();
    EXPECT_EQ(3, freed_size_samples->TotalCount());
    // 123 and 100 are in the same bucket.
    EXPECT_EQ(2, freed_size_samples->GetCount(123));
    EXPECT_EQ(2, freed_size_samples->GetCount(100));
    EXPECT_EQ(1, freed_size_samples->GetCount(10000));
  }

  lmk_count_histogram = GetLowMemoryKillsCountHistogram();
  ASSERT_TRUE(lmk_count_histogram);
  {
    auto count_samples = lmk_count_histogram->SnapshotSamples();
    // Ensure zero count is not increased.
    EXPECT_EQ(1, count_samples->GetCount(0));
  }
}

}  // namespace memory
