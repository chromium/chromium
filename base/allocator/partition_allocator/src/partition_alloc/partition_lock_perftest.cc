// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/partition_lock.h"

#include <vector>

#include "base/timer/lap_timer.h"
#include "partition_alloc/partition_alloc_base/threading/platform_thread_for_testing.h"
#include "partition_alloc/partition_alloc_base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"

namespace partition_alloc::internal {

namespace {

constexpr int kWarmupRuns = 1;
constexpr ::base::TimeDelta kTimeLimit = ::base::Seconds(1);
constexpr int kTimeCheckInterval = 100000;

constexpr char kMetricPrefixLock[] = "PartitionLock.";
constexpr char kMetricLockUnlockThroughput[] = "lock_unlock_throughput";
constexpr char kMetricLockUnlockLatency[] = "lock_unlock_latency_ns";
constexpr char kStoryBaseline[] = "baseline_story";
constexpr char kStoryWithCompetingThread[] = "with_competing_thread";

perf_test::PerfResultReporter SetUpReporter(const std::string& story_name) {
  perf_test::PerfResultReporter reporter(kMetricPrefixLock, story_name);
  reporter.RegisterImportantMetric(kMetricLockUnlockThroughput, "runs/s");
  reporter.RegisterImportantMetric(kMetricLockUnlockLatency, "ns");
  return reporter;
}

class Spin : public base::PlatformThreadForTesting::Delegate {
 public:
  Spin(Lock* lock, uint32_t* data)
      : lock_(lock), data_(data), should_stop_(false) {}
  ~Spin() override = default;

  void ThreadMain() override {
    started_count_++;
    // Local variable to avoid "cache line ping-pong" from influencing the
    // results.
    uint32_t count = 0;
    while (!should_stop_.load(std::memory_order_relaxed)) {
      lock_->Acquire();
      count++;
      lock_->Release();
    }

    lock_->Acquire();
    (*data_) += count;
    lock_->Release();
  }

  // Called from another thread to stop the loop.
  void Stop() { should_stop_ = true; }
  int started_count() const { return started_count_; }

 private:
  Lock* lock_;
  uint32_t* data_ GUARDED_BY(lock_);
  std::atomic<bool> should_stop_;
  std::atomic<int> started_count_{0};
};

}  // namespace

TEST(PartitionLockPerfTest, Simple) {
  ::base::LapTimer timer(kWarmupRuns, kTimeLimit, kTimeCheckInterval);
  [[maybe_unused]] uint32_t data = 0;

  Lock lock;

  do {
    lock.Acquire();
    data += 1;
    lock.Release();
    timer.NextLap();
  } while (!timer.HasTimeLimitExpired());

  auto reporter = SetUpReporter(kStoryBaseline);
  reporter.AddResult(kMetricLockUnlockThroughput, timer.LapsPerSecond());
  reporter.AddResult(kMetricLockUnlockLatency, 1e9 / timer.LapsPerSecond());
}

TEST(PartitionLockPerfTest, WithCompetingThreads) {
  uint32_t data = 0;

  Lock lock;

  // Starts a competing thread executing the same loop as this thread.
  Spin thread_main(&lock, &data);
  std::vector<base::PlatformThreadHandle> thread_handles;
  constexpr int kThreads = 4;

  for (int i = 0; i < kThreads; i++) {
    base::PlatformThreadHandle thread_handle;
    ASSERT_TRUE(base::PlatformThreadForTesting::Create(0, &thread_main,
                                                       &thread_handle));
    thread_handles.push_back(thread_handle);
  }
  // Wait for all the threads to start.
  while (thread_main.started_count() != kThreads) {
  }

  ::base::LapTimer timer(kWarmupRuns, kTimeLimit, kTimeCheckInterval);
  do {
    lock.Acquire();
    data += 1;
    lock.Release();
    timer.NextLap();
  } while (!timer.HasTimeLimitExpired());

  thread_main.Stop();
  for (int i = 0; i < kThreads; i++) {
    base::PlatformThreadForTesting::Join(thread_handles[i]);
  }

  auto reporter = SetUpReporter(kStoryWithCompetingThread);
  reporter.AddResult(kMetricLockUnlockThroughput, timer.LapsPerSecond());
  reporter.AddResult(kMetricLockUnlockLatency, 1e9 / timer.LapsPerSecond());
}

}  // namespace partition_alloc::internal
