// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/synchronization/cancelable_event.h"

#include <stdint.h>

#include <atomic>
#include <memory>
#include <tuple>
#include <vector>

#include "base/barrier_closure.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/numerics/clamped_math.h"
#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/test/bind.h"
#include "base/test/test_timeouts.h"
#include "base/test/test_waitable_event.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace base {

namespace {

class CancelableEventTest : public testing::Test {
 protected:
  raw_ptr<Thread> CreateThreadWithTask(RepeatingClosure& thread_task) {
    std::unique_ptr<Thread> thread = std::make_unique<Thread>(
        StringPrintf("CancelTestThread%d", threadcounter++));

    thread->Start();
    thread->task_runner()->PostTask(FROM_HERE, thread_task);
    threads_.push_back(std::move(thread));
    return threads_.back().get();
  }

  int threadcounter = 0;
  WaitableEvent shutdown_event_;
  std::vector<std::unique_ptr<Thread>> threads_;
};

}  // namespace

TEST_F(CancelableEventTest, TimedWaitFail) {
  CancelableEvent event;
  RepeatingClosure task = BindLambdaForTesting([&]() {
    TimeTicks start_time = TimeTicks::Now();
    EXPECT_FALSE(event.TimedWait(TestTimeouts::tiny_timeout()));
    EXPECT_GE(TimeTicks::Now() - start_time, TestTimeouts::tiny_timeout());
  });

  this->CreateThreadWithTask(task)->FlushForTesting();
}

TEST_F(CancelableEventTest, TimedWaitSuccess) {
  CancelableEvent event;
  RepeatingClosure task = BindLambdaForTesting(
      [&]() { EXPECT_TRUE(event.TimedWait(TestTimeouts::tiny_timeout())); });

  event.Signal();
  this->CreateThreadWithTask(task)->FlushForTesting();
}

// These are the platforms on which a functional CancelableEvent is implemented.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_ANDROID)

TEST_F(CancelableEventTest, CancelSucceedsWhenNoWaiterAndWaitTimesOut) {
  CancelableEvent event;
  event.Signal();
  EXPECT_TRUE(event.Cancel());
  EXPECT_FALSE(event.TimedWait(base::TimeDelta()));
}

TEST_F(CancelableEventTest, BothCancelFailureAndSucceedOccurWithOneWaiter) {
  bool cancel_failed = false;
  bool cancel_succeeded = false;
  for (int i = 0; i < 100; ++i) {
    CancelableEvent event;
    TestWaitableEvent thread_running;
    auto task = BindLambdaForTesting([&]() {
      thread_running.Signal();
      event.Wait();
    });
    auto thread = CreateThreadWithTask(task);
    if (!cancel_failed) {
      thread_running.Wait();
    }
    event.Signal();
#if BUILDFLAG(IS_POSIX)
    // Posix implementations of Semaphores seem to be much less greedy in waking
    // up threads currently waiting on the event - give the thread a few
    // milliseconds to wake up and acquire the semaphore before us.
    if (cancel_succeeded) {
      PlatformThread::Sleep(Milliseconds(8));
    }
#endif
    if (event.Cancel()) {
      cancel_succeeded = true;
      event.Signal();
    } else {
      cancel_failed = true;
    }
    thread->FlushForTesting();
    if (cancel_failed && cancel_succeeded) {
      break;
    }
  }
  EXPECT_TRUE(cancel_failed);
  EXPECT_TRUE(cancel_succeeded);
}

TEST_F(CancelableEventTest, BothCancelFailureAndSucceedOccurUnderContention) {
  // The following block is responsible for creating CPU contention - it creates
  // 16 threads which run for the duration of the test, crunching CPU. None of
  // the data here is used in any meaningful way in the rest of the test. beyond
  // setting `busywork_threads_should_quit` to signal exit.
  std::atomic_bool busywork_threads_should_quit = false;
  // The arena lives for the duration of the test, and so must have test-wide
  // scope, however it is only accessed by the busywork threads, and not by the
  // rest of the test.
  const int kNumThreads = 16;
  std::atomic_char busywork_arena[kNumThreads];
  {
    TestWaitableEvent threads_running;
    RepeatingClosure threads_running_barrier = BarrierClosure(
        kNumThreads,
        BindOnce(&TestWaitableEvent::Signal, Unretained(&threads_running)));
    for (int i = 0; i < kNumThreads; ++i) {
      auto task = BindLambdaForTesting([&]() {
        threads_running_barrier.Run();
        while (!busywork_threads_should_quit.load(std::memory_order_acquire)) {
          // Busy loop. Only done to burn CPU.
          uint64_t counter = 1;
          for (auto& slot : busywork_arena) {
            counter += slot + 1;
            slot.store(static_cast<char>((counter & 0xf) + 1),
                       std::memory_order_release);
          }
        }
      });
      CreateThreadWithTask(task);
    }
    threads_running.Wait();
  }

  // Used to adjust race timings to favor the case which has not yet been seen
  // (cancel fail/success).
  internal::ClampedNumeric<unsigned int> wait_ms = 0;
  bool succeeded = false;
  bool failed = false;
  for (int i = 0; i < 10 && (!failed || !succeeded); ++i) {
    CancelableEvent event;
    TestWaitableEvent thread_running;
    std::atomic_bool thread_done = false;
    auto task = BindLambdaForTesting([&]() {
      thread_running.Signal();
      EXPECT_TRUE(event.TimedWait(TestTimeouts::test_launcher_timeout()));
      thread_done = true;
    });
    auto thread = CreateThreadWithTask(task);
    PlatformThread::Sleep(Milliseconds(wait_ms));
    event.Signal();
    PlatformThread::Sleep(Milliseconds(wait_ms));
    if (event.Cancel()) {
      succeeded = true;
      event.Signal();
      wait_ms += 100;
    } else {
      failed = true;
      wait_ms -= 50;
    }
    thread->FlushForTesting();
    EXPECT_TRUE(thread_done);
  }
  busywork_threads_should_quit.store(true, std::memory_order_release);
  for (auto& thread : threads_) {
    thread->FlushForTesting();
  }
  EXPECT_TRUE(succeeded);
  EXPECT_TRUE(failed);
}

#else

TEST_F(CancelableEventTest, CancelFailsOnUnsupportedPlatforms) {
  CancelableEvent event;
  event.Signal();
  EXPECT_FALSE(event.Cancel());
  EXPECT_TRUE(event.TimedWait(base::TimeDelta()));
}

#endif

}  // namespace base
