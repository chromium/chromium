// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/memory/memory_kills_monitor.h"

#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "base/stl_util.h"
#include "base/time/time.h"
#include "chrome/browser/memory/memory_kills_histogram.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace memory {

namespace {

base::HistogramBase* GetLowMemoryKillsCountHistogram() {
  return base::StatisticsRecorder::FindHistogram("Arc.LowMemoryKiller.Count");
}

base::HistogramBase* GetOOMKillsCountHistogram() {
  return base::StatisticsRecorder::FindHistogram("Arc.OOMKills.Count");
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
  auto* oom_count_histogram = GetOOMKillsCountHistogram();
  // Before StartMonitoring() is called, nothing is recorded.
  ASSERT_FALSE(lmk_count_histogram);
  ASSERT_FALSE(oom_count_histogram);

  // Start monitoring.
  g_memory_kills_monitor_unittest_instance->StartMonitoring();
  lmk_count_histogram = GetLowMemoryKillsCountHistogram();
  oom_count_histogram = GetOOMKillsCountHistogram();
  ASSERT_TRUE(lmk_count_histogram);
  ASSERT_TRUE(oom_count_histogram);
  {
    auto count_samples = lmk_count_histogram->SnapshotSamples();
    EXPECT_EQ(1, count_samples->TotalCount());
    EXPECT_EQ(1, count_samples->GetCount(0));
  }
  {
    auto count_samples = oom_count_histogram->SnapshotSamples();
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
        "Arc.LowMemoryKiller.FreedSize");
    ASSERT_TRUE(histogram_freed_size);
    auto freed_size_samples = histogram_freed_size->SnapshotSamples();
    EXPECT_EQ(3, freed_size_samples->TotalCount());
    // 123 and 100 are in the same bucket.
    EXPECT_EQ(2, freed_size_samples->GetCount(123));
    EXPECT_EQ(2, freed_size_samples->GetCount(100));
    EXPECT_EQ(1, freed_size_samples->GetCount(10000));
  }

  {
    auto* histogram_time_delta = base::StatisticsRecorder::FindHistogram(
        "Arc.LowMemoryKiller.TimeDelta");
    ASSERT_TRUE(histogram_time_delta);
    auto time_delta_samples = histogram_time_delta->SnapshotSamples();
    EXPECT_EQ(3, time_delta_samples->TotalCount());
    // First time delta is set to kMaxMemoryKillTimeDelta.
    EXPECT_EQ(1, time_delta_samples->GetCount(
                     kMaxMemoryKillTimeDelta.InMilliseconds()));
    // Time delta for the other 2 events depends on Now() so we skip testing it
    // here.
  }

  // OOM kills.
  const char* sample_lines[] = {
      "3,3429,812967386,-;Out of memory: Kill process 8291 (handle-watcher-) "
      "score 674 or sacrifice child",
      "3,3431,812981331,-;Out of memory: Kill process 8271 (.gms.persistent) "
      "score 652 or sacrifice child",
      "3,3433,812993014,-;Out of memory: Kill process 9210 (lowpool[11]) "
      "score 653 or sacrifice child"
  };

  for (unsigned long i = 0; i < base::size(sample_lines); ++i) {
    MemoryKillsMonitor::TryMatchOomKillLine(sample_lines[i]);
  }

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

  {
    auto* histogram_score =
        base::StatisticsRecorder::FindHistogram("Arc.OOMKills.Score");
    ASSERT_TRUE(histogram_score);
    auto score_samples = histogram_score->SnapshotSamples();
    EXPECT_EQ(3, score_samples->TotalCount());
    EXPECT_EQ(1, score_samples->GetCount(674));
    EXPECT_EQ(1, score_samples->GetCount(652));
    EXPECT_EQ(1, score_samples->GetCount(653));
  }

  {
    auto* histogram_time_delta =
        base::StatisticsRecorder::FindHistogram("Arc.OOMKills.TimeDelta");
    ASSERT_TRUE(histogram_time_delta);
    auto time_delta_samples = histogram_time_delta->SnapshotSamples();
    EXPECT_EQ(3, time_delta_samples->TotalCount());
    // First time delta is set to kMaxMemoryKillTimeDelta.
    EXPECT_EQ(1, time_delta_samples->GetCount(
                     kMaxMemoryKillTimeDelta.InMilliseconds()));
    EXPECT_EQ(1, time_delta_samples->GetCount(11));
    EXPECT_EQ(1, time_delta_samples->GetCount(13));
  }

  // Call StartMonitoring multiple times.
  base::PlatformThreadId tid1 = g_memory_kills_monitor_unittest_instance
                                    ->non_joinable_worker_thread_->tid();
  g_memory_kills_monitor_unittest_instance->StartMonitoring();
  base::PlatformThreadId tid2 = g_memory_kills_monitor_unittest_instance
                                    ->non_joinable_worker_thread_->tid();
  EXPECT_EQ(tid1, tid2);

  lmk_count_histogram = GetLowMemoryKillsCountHistogram();
  ASSERT_TRUE(lmk_count_histogram);
  {
    auto count_samples = lmk_count_histogram->SnapshotSamples();
    // Ensure zero count is not increased.
    EXPECT_EQ(1, count_samples->GetCount(0));
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
