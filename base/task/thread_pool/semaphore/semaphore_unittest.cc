// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/thread_pool/semaphore.h"

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/test/bind.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace base {

namespace {

class SemaphoreTest : public PlatformTest {
 protected:
  raw_ptr<Thread> CreateThreadWithTask(RepeatingClosure& thread_task) {
    std::unique_ptr<Thread> thread = std::make_unique<Thread>(
        StringPrintf("SemTestThread%d", threadcounter++));

    thread->Start();
    thread->task_runner()->PostTask(FROM_HERE, thread_task);
    threads_.push_back(std::move(thread));
    return threads_.back().get();
  }

  int threadcounter = 0;
  WaitableEvent shutdown_event_{};
  std::vector<std::unique_ptr<Thread>> threads_{};
};

}  // namespace

TEST_F(SemaphoreTest, TimedWaitFail) {
  internal::Semaphore sem{0};
  RepeatingClosure task = BindLambdaForTesting([&]() {
    TimeTicks start_time = TimeTicks::Now();
    EXPECT_FALSE(sem.TimedWait(TestTimeouts::tiny_timeout()));
    EXPECT_GE(TimeTicks::Now() - start_time, TestTimeouts::tiny_timeout());
  });

  CreateThreadWithTask(task)->FlushForTesting();
}

TEST_F(SemaphoreTest, TimedWaitSuccess) {
  internal::Semaphore sem{0};
  RepeatingClosure task = BindLambdaForTesting(
      [&]() { EXPECT_TRUE(sem.TimedWait(TestTimeouts::tiny_timeout())); });

  sem.Signal();
  CreateThreadWithTask(task)->FlushForTesting();
}

TEST_F(SemaphoreTest, PingPongCounter) {
  internal::Semaphore sem{0};
  int counter = 0;
  RepeatingClosure task = BindLambdaForTesting([&]() {
    while (!shutdown_event_.IsSignaled()) {
      sem.Wait();
      {
        if (shutdown_event_.IsSignaled()) {
          return;
        }
      }
      ++counter;
      if (counter > 999) {
        shutdown_event_.Signal();
      }
      sem.Signal();
      PlatformThread::Sleep(Microseconds(100));
    }
  });

  sem.Signal();
  raw_ptr<Thread> thread = CreateThreadWithTask(task);
  raw_ptr<Thread> thread2 = CreateThreadWithTask(task);
  thread->FlushForTesting();
  thread2->FlushForTesting();
  thread->Stop();
  thread2->Stop();
  EXPECT_EQ(counter, 1000);
}

}  // namespace base
