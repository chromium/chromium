// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/partition_lock.h"

#include "base/allocator/partition_allocator/base/migration_adapter.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace partition_alloc::internal {

namespace base {

// TODO(https://crbug.com/1288247): Remove these 'using' declarations once
// the migration to the new namespaces gets done.
using ::base::BindLambdaForTesting;
using ::base::OnceCallback;
using ::base::RepeatingClosure;

}  // namespace base

namespace {

class LambdaThreadDelegate : public base::PlatformThread::Delegate {
 public:
  explicit LambdaThreadDelegate(base::RepeatingClosure f) : f_(f) {}
  void ThreadMain() override { f_.Run(); }

 private:
  base::RepeatingClosure f_;
};

TEST(PartitionAllocLockTest, Simple) {
  Lock lock;
  lock.Acquire();
  lock.Release();
}

Lock g_lock;
TEST(PartitionAllocLockTest, StaticLockStartsUnlocked) {
  g_lock.Acquire();
  g_lock.Release();
}

TEST(PartitionAllocLockTest, Contended) {
  int counter = 0;  // *Not* atomic.
  std::vector<base::PlatformThreadHandle> thread_handles;
  constexpr int iterations_per_thread = 1000000;
  constexpr int num_threads = 4;

  Lock lock;
  Lock start_lock;

  LambdaThreadDelegate delegate{base::BindLambdaForTesting([&]() {
    start_lock.Acquire();
    start_lock.Release();

    for (int i = 0; i < iterations_per_thread; i++) {
      lock.Acquire();
      counter++;
      lock.Release();
    }
  })};

  start_lock.Acquire();  // Make sure that the threads compete, by waiting until
                         // all of them have at least been created.
  for (int i = 0; i < num_threads; i++) {
    base::PlatformThreadHandle handle;
    base::PlatformThread::Create(0, &delegate, &handle);
    thread_handles.push_back(handle);
  }

  start_lock.Release();

  for (int i = 0; i < num_threads; i++) {
    base::PlatformThread::Join(thread_handles[i]);
  }
  EXPECT_EQ(iterations_per_thread * num_threads, counter);
}

TEST(PartitionAllocLockTest, SlowThreads) {
  int counter = 0;  // *Not* atomic.
  std::vector<base::PlatformThreadHandle> thread_handles;
  constexpr int iterations_per_thread = 100;
  constexpr int num_threads = 4;

  Lock lock;
  Lock start_lock;

  LambdaThreadDelegate delegate{base::BindLambdaForTesting([&]() {
    start_lock.Acquire();
    start_lock.Release();

    for (int i = 0; i < iterations_per_thread; i++) {
      lock.Acquire();
      counter++;
      // Hold the lock for a while, to force futex()-based locks to sleep.
      base::PlatformThread::Sleep(base::Milliseconds(1));
      lock.Release();
    }
  })};

  start_lock.Acquire();  // Make sure that the threads compete, by waiting until
                         // all of them have at least been created.
  for (int i = 0; i < num_threads; i++) {
    base::PlatformThreadHandle handle;
    base::PlatformThread::Create(0, &delegate, &handle);
    thread_handles.push_back(handle);
  }

  start_lock.Release();

  for (int i = 0; i < num_threads; i++) {
    base::PlatformThread::Join(thread_handles[i]);
  }
  EXPECT_EQ(iterations_per_thread * num_threads, counter);
}

TEST(PartitionAllocLockTest, AssertAcquired) {
  Lock lock;
  lock.Acquire();
  lock.AssertAcquired();
  lock.Release();
}

// AssertAcquired() is only enforced with DCHECK()s.
#if defined(GTEST_HAS_DEATH_TEST) && DCHECK_IS_ON()

TEST(PartitionAllocLockTest, AssertAcquiredDeathTest) {
  Lock lock;
  EXPECT_DEATH(lock.AssertAcquired(), "");
}

TEST(PartitionAllocLockTest, AssertAcquiredAnotherThreadHoldsTheLock) {
  Lock lock;
  // NO_THREAD_SAFETY_ANALYSIS: The checker rightfully points out that the lock
  // is still held at the end of the function, which is what we want here.
  LambdaThreadDelegate delegate{base::BindLambdaForTesting(
      [&]() NO_THREAD_SAFETY_ANALYSIS { lock.Acquire(); })};
  base::PlatformThreadHandle handle;
  base::PlatformThread::Create(0, &delegate, &handle);
  // Join before the test, otherwise some platforms' gtest have trouble with
  // EXPECT_DEATH() and multiple live threads.
  base::PlatformThread::Join(handle);

  EXPECT_DEATH(lock.AssertAcquired(), "");
}

#if BUILDFLAG(IS_APPLE)
// On Apple OSes, it is not allowed to unlock a lock from another thread, so
// we need to re-initialize it.
TEST(PartitionAllocLockTest, ReinitInOtherThread) NO_THREAD_SAFETY_ANALYSIS {
  Lock lock;
  lock.Acquire();

  LambdaThreadDelegate delegate{
      base::BindLambdaForTesting([&]() NO_THREAD_SAFETY_ANALYSIS {
        lock.Reinit();
        lock.Acquire();
        lock.Release();
      })};
  base::PlatformThreadHandle handle;
  base::PlatformThread::Create(0, &delegate, &handle);
  base::PlatformThread::Join(handle);
}
#endif  // BUILDFLAG(IS_APPLE)

#endif  // defined(GTEST_HAS_DEATH_TEST) && DCHECK_IS_ON()

}  // namespace
}  // namespace partition_alloc::internal
