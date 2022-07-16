// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/partition_lock.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace internal {
namespace {

class LambdaThreadDelegate : public PlatformThread::Delegate {
 public:
  explicit LambdaThreadDelegate(RepeatingClosure f) : f_(f) {}
  void ThreadMain() override { f_.Run(); }

 private:
  RepeatingClosure f_;
};

TEST(PartitionAllocLockTest, Simple) {
  MaybeLock<true> lock;
  lock.Lock();
  lock.Unlock();
}

MaybeLock<true> g_lock;
TEST(PartitionAllocLockTest, StaticLockStartsUnlocked) {
  g_lock.Lock();
  g_lock.Unlock();
}

TEST(PartitionAllocLockTest, Contended) {
  int counter = 0;  // *Not* atomic.
  std::vector<PlatformThreadHandle> thread_handles;
  constexpr int iterations_per_thread = 1000000;
  constexpr int num_threads = 4;

  MaybeLock<true> lock;
  MaybeLock<true> start_lock;

  LambdaThreadDelegate delegate{BindLambdaForTesting([&]() {
    start_lock.Lock();
    start_lock.Unlock();

    for (int i = 0; i < iterations_per_thread; i++) {
      lock.Lock();
      counter++;
      lock.Unlock();
    }
  })};

  start_lock.Lock();  // Make sure that the threads compete, by waiting until
                      // all of them have at least been created.
  for (int i = 0; i < num_threads; i++) {
    PlatformThreadHandle handle;
    PlatformThread::Create(0, &delegate, &handle);
    thread_handles.push_back(handle);
  }

  start_lock.Unlock();

  for (int i = 0; i < num_threads; i++) {
    PlatformThread::Join(thread_handles[i]);
  }
  EXPECT_EQ(iterations_per_thread * num_threads, counter);
}

TEST(PartitionAllocLockTest, SlowThreads) {
  int counter = 0;  // *Not* atomic.
  std::vector<PlatformThreadHandle> thread_handles;
  constexpr int iterations_per_thread = 100;
  constexpr int num_threads = 4;

  MaybeLock<true> lock;
  MaybeLock<true> start_lock;

  LambdaThreadDelegate delegate{BindLambdaForTesting([&]() {
    start_lock.Lock();
    start_lock.Unlock();

    for (int i = 0; i < iterations_per_thread; i++) {
      lock.Lock();
      counter++;
      // Hold the lock for a while, to force futex()-based locks to sleep.
      PlatformThread::Sleep(Milliseconds(1));
      lock.Unlock();
    }
  })};

  start_lock.Lock();  // Make sure that the threads compete, by waiting until
                      // all of them have at least been created.
  for (int i = 0; i < num_threads; i++) {
    PlatformThreadHandle handle;
    PlatformThread::Create(0, &delegate, &handle);
    thread_handles.push_back(handle);
  }

  start_lock.Unlock();

  for (int i = 0; i < num_threads; i++) {
    PlatformThread::Join(thread_handles[i]);
  }
  EXPECT_EQ(iterations_per_thread * num_threads, counter);
}

TEST(PartitionAllocLockTest, AssertAcquired) {
  MaybeLock<true> lock;
  lock.Lock();
  lock.AssertAcquired();
  lock.Unlock();
}

// AssertAcquired() is only enforced with DCHECK()s.
#if defined(GTEST_HAS_DEATH_TEST) && DCHECK_IS_ON()

TEST(PartitionAllocLockTest, AssertAcquiredDeathTest) {
  MaybeLock<true> lock;
  EXPECT_DEATH(lock.AssertAcquired(), "");
}

TEST(PartitionAllocLockTest, AssertAcquiredAnotherThreadHoldsTheLock) {
  MaybeLock<true> lock;
  // NO_THREAD_SAFETY_ANALYSIS: The checker rightfully points out that the lock
  // is still held at the end of the function, which is what we want here.
  LambdaThreadDelegate delegate{
      BindLambdaForTesting([&]() NO_THREAD_SAFETY_ANALYSIS { lock.Lock(); })};
  PlatformThreadHandle handle;
  PlatformThread::Create(0, &delegate, &handle);
  // Join before the test, otherwise some platforms' gtest have trouble with
  // EXPECT_DEATH() and multiple live threads.
  PlatformThread::Join(handle);

  EXPECT_DEATH(lock.AssertAcquired(), "");
}

#if defined(OS_APPLE)
// On Apple OSes, it is not allowed to unlock a lock from another thread, so
// we need to re-initialize it.
TEST(PartitionAllocLockTest, ReinitInOtherThread) NO_THREAD_SAFETY_ANALYSIS {
  MaybeLock<true> lock;
  lock.Lock();

  LambdaThreadDelegate delegate{
      BindLambdaForTesting([&]() NO_THREAD_SAFETY_ANALYSIS {
        lock.Reinit();
        lock.Lock();
        lock.Unlock();
      })};
  PlatformThreadHandle handle;
  PlatformThread::Create(0, &delegate, &handle);
  PlatformThread::Join(handle);
}
#endif  // defined(OS_APPLE)

#endif  // defined(GTEST_HAS_DEATH_TEST) && DCHECK_IS_ON()

}  // namespace
}  // namespace internal
}  // namespace base
