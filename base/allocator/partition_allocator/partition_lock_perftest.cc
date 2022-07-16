// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/allocator/partition_allocator/partition_lock.h"
#include "base/compiler_specific.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/timer/lap_timer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"

namespace base {
namespace internal {
namespace {

constexpr int kWarmupRuns = 1;
constexpr TimeDelta kTimeLimit = Seconds(1);
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

class Spin : public PlatformThread::Delegate {
 public:
  Spin(MaybeLock<true>* lock, uint32_t* data)
      : lock_(lock), data_(data), should_stop_(false) {}
  ~Spin() override = default;

  void ThreadMain() override {
    started_count_++;
    // Local variable to avoid "cache line ping-pong" from influencing the
    // results.
    uint32_t count = 0;
    while (!should_stop_.load(std::memory_order_relaxed)) {
      lock_->Lock();
      count++;
      lock_->Unlock();
    }

    lock_->Lock();
    (*data_) += count;
    lock_->Unlock();
  }

  // Called from another thread to stop the loop.
  void Stop() { should_stop_ = true; }
  int started_count() const { return started_count_; }

 private:
  MaybeLock<true>* lock_;
  uint32_t* data_ GUARDED_BY(lock_);
  std::atomic<bool> should_stop_;
  std::atomic<int> started_count_{0};
};

}  // namespace

TEST(PartitionLockPerfTest, Simple) {
  LapTimer timer(kWarmupRuns, kTimeLimit, kTimeCheckInterval);
  uint32_t data = 0;

  Lock lock;

  do {
    lock.Acquire();
    data += 1;
    lock.Release();
    timer.NextLap();
  } while (!timer.HasTimeLimitExpired());

  ALLOW_UNUSED_LOCAL(data);
  auto reporter = SetUpReporter(kStoryBaseline);
  reporter.AddResult(kMetricLockUnlockThroughput, timer.LapsPerSecond());
  reporter.AddResult(kMetricLockUnlockLatency, 1e9 / timer.LapsPerSecond());
}

TEST(PartitionLockPerfTest, WithCompetingThreads) {
  uint32_t data = 0;

  MaybeLock<true> lock;

  // Starts a competing thread executing the same loop as this thread.
  Spin thread_main(&lock, &data);
  std::vector<PlatformThreadHandle> thread_handles;
  constexpr int kThreads = 4;

  for (int i = 0; i < kThreads; i++) {
    PlatformThreadHandle thread_handle;
    ASSERT_TRUE(PlatformThread::Create(0, &thread_main, &thread_handle));
    thread_handles.push_back(thread_handle);
  }
  // Wait for all the threads to start.
  while (thread_main.started_count() != kThreads) {
  }

  LapTimer timer(kWarmupRuns, kTimeLimit, kTimeCheckInterval);
  do {
    lock.Lock();
    data += 1;
    lock.Unlock();
    timer.NextLap();
  } while (!timer.HasTimeLimitExpired());

  thread_main.Stop();
  for (int i = 0; i < kThreads; i++) {
    PlatformThread::Join(thread_handles[i]);
  }

  auto reporter = SetUpReporter(kStoryWithCompetingThread);
  reporter.AddResult(kMetricLockUnlockThroughput, timer.LapsPerSecond());
  reporter.AddResult(kMetricLockUnlockLatency, 1e9 / timer.LapsPerSecond());
}

}  // namespace internal
}  // namespace base
