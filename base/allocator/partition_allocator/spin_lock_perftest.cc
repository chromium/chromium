// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/spin_lock.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/timer/lap_timer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_test.h"

namespace base {
namespace {

constexpr int kWarmupRuns = 1;
constexpr TimeDelta kTimeLimit = TimeDelta::FromSeconds(1);
constexpr int kTimeCheckInterval = 100000;

class Spin : public PlatformThread::Delegate {
 public:
  Spin(subtle::SpinLock* lock, size_t* data)
      : lock_(lock), data_(data), should_stop_(false) {}
  ~Spin() override = default;

  void ThreadMain() override {
    while (!should_stop_.load(std::memory_order_relaxed)) {
      lock_->lock();
      (*data_)++;
      lock_->unlock();
    }
  }

  void Stop() { should_stop_ = true; }

 private:
  subtle::SpinLock* lock_;
  size_t* data_;
  std::atomic<bool> should_stop_;
};

}  // namespace

TEST(SpinLockPerfTest, Simple) {
  LapTimer timer(kWarmupRuns, kTimeLimit, kTimeCheckInterval);
  size_t data = 0;

  subtle::SpinLock lock;

  do {
    lock.lock();
    data += 1;
    lock.unlock();
    timer.NextLap();
  } while (!timer.HasTimeLimitExpired());

  perf_test::PrintResult("SpinLockPerfTest", " lock()/unlock()", "",
                         timer.LapsPerSecond(), "runs/s", true);
}

TEST(SpinLockPerfTest, WithCompetingThread) {
  LapTimer timer(kWarmupRuns, kTimeLimit, kTimeCheckInterval);
  size_t data = 0;

  subtle::SpinLock lock;

  // Starts a competing thread executing the same loop as this thread.
  Spin thread_main(&lock, &data);
  PlatformThreadHandle thread_handle;
  ASSERT_TRUE(PlatformThread::Create(0, &thread_main, &thread_handle));

  do {
    lock.lock();
    data += 1;
    lock.unlock();
    timer.NextLap();
  } while (!timer.HasTimeLimitExpired());

  thread_main.Stop();
  PlatformThread::Join(thread_handle);

  perf_test::PrintResult("SpinLockPerfTest.WithCompetingThread",
                         " lock()/unlock()", "", timer.LapsPerSecond(),
                         "runs/s", true);
}

}  // namespace base
