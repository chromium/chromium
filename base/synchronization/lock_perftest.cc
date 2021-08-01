// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/compiler_specific.h"
#include "base/synchronization/lock.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/timer/lap_timer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"

namespace base {
namespace {

constexpr int kWarmupRuns = 1;
constexpr TimeDelta kTimeLimit = TimeDelta::FromSeconds(1);
constexpr int kTimeCheckInterval = 100000;

constexpr char kMetricPrefixLock[] = "Lock.";
constexpr char kMetricLockUnlockThroughput[] = "lock_unlock_throughput";
constexpr char kStoryBaseline[] = "baseline_story";
constexpr char kStoryWithCompetingThread[] = "with_competing_thread";

perf_test::PerfResultReporter SetUpReporter(const std::string& story_name) {
  perf_test::PerfResultReporter reporter(kMetricPrefixLock, story_name);
  reporter.RegisterImportantMetric(kMetricLockUnlockThroughput, "runs/s");
  return reporter;
}

class Spin : public PlatformThread::Delegate {
 public:
  Spin(Lock* lock, uint32_t* data)
      : lock_(lock), data_(data), should_stop_(false) {}
  ~Spin() override = default;

  void ThreadMain() override {
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

 private:
  Lock* lock_;
  uint32_t* data_ GUARDED_BY(lock_);
  std::atomic<bool> should_stop_;
};

}  // namespace

TEST(LockPerfTest, Simple) {
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
}

TEST(LockPerfTest, WithCompetingThread) {
  LapTimer timer(kWarmupRuns, kTimeLimit, kTimeCheckInterval);
  uint32_t data = 0;

  Lock lock;

  // Starts a competing thread executing the same loop as this thread.
  Spin thread_main(&lock, &data);
  PlatformThreadHandle thread_handle;
  ASSERT_TRUE(PlatformThread::Create(0, &thread_main, &thread_handle));

  do {
    lock.Acquire();
    data += 1;
    lock.Release();
    timer.NextLap();
  } while (!timer.HasTimeLimitExpired());

  thread_main.Stop();
  PlatformThread::Join(thread_handle);

  auto reporter = SetUpReporter(kStoryWithCompetingThread);
  reporter.AddResult(kMetricLockUnlockThroughput, timer.LapsPerSecond());
}
}  // namespace base
