// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/partition_lock.h"

#include "partition_alloc/build_config.h"
#include "partition_alloc/buildflags.h"
#include "partition_alloc/partition_alloc_base/thread_annotations.h"
#include "partition_alloc/partition_alloc_base/threading/platform_thread_for_testing.h"
#include "partition_alloc/partition_alloc_base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace partition_alloc::internal {

TEST(PartitionAllocLockTest, Simple) {
  Lock lock;
  lock.Acquire();
  lock.Release();
}

namespace {

Lock g_lock;

}  // namespace

TEST(PartitionAllocLockTest, StaticLockStartsUnlocked) {
  g_lock.Acquire();
  g_lock.Release();
}

namespace {

class ThreadDelegateForContended
    : public base::PlatformThreadForTesting::Delegate {
 public:
  explicit ThreadDelegateForContended(Lock& start_lock,
                                      Lock& lock,
                                      int iterations,
                                      int& counter)
      : start_lock_(start_lock),
        lock_(lock),
        iterations_(iterations),
        counter_(counter) {}

  void ThreadMain() override {
    start_lock_.Acquire();
    start_lock_.Release();

    for (int i = 0; i < iterations_; i++) {
      lock_.Acquire();
      ++counter_;
      lock_.Release();
    }
  }

 private:
  Lock& start_lock_;
  Lock& lock_;
  const int iterations_;
  int& counter_;
};

}  // namespace

TEST(PartitionAllocLockTest, Contended) {
  int counter = 0;  // *Not* atomic.
  std::vector<internal::base::PlatformThreadHandle> thread_handles;
  constexpr int iterations_per_thread = 1000000;
  constexpr int num_threads = 4;

  Lock lock;
  Lock start_lock;

  ThreadDelegateForContended delegate(start_lock, lock, iterations_per_thread,
                                      counter);

  start_lock.Acquire();  // Make sure that the threads compete, by waiting until
                         // all of them have at least been created.
  for (int i = 0; i < num_threads; ++i) {
    base::PlatformThreadHandle handle;
    base::PlatformThreadForTesting::Create(0, &delegate, &handle);
    thread_handles.push_back(handle);
  }

  start_lock.Release();

  for (int i = 0; i < num_threads; ++i) {
    base::PlatformThreadForTesting::Join(thread_handles[i]);
  }
  EXPECT_EQ(iterations_per_thread * num_threads, counter);
}

namespace {

class ThreadDelegateForSlowThreads
    : public base::PlatformThreadForTesting::Delegate {
 public:
  explicit ThreadDelegateForSlowThreads(Lock& start_lock,
                                        Lock& lock,
                                        int iterations,
                                        int& counter)
      : start_lock_(start_lock),
        lock_(lock),
        iterations_(iterations),
        counter_(counter) {}

  void ThreadMain() override {
    start_lock_.Acquire();
    start_lock_.Release();

    for (int i = 0; i < iterations_; i++) {
      lock_.Acquire();
      ++counter_;
      // Hold the lock for a while, to force futex()-based locks to sleep.
      base::PlatformThread::Sleep(base::Milliseconds(1));
      lock_.Release();
    }
  }

 private:
  Lock& start_lock_;
  Lock& lock_;
  const int iterations_;
  int& counter_;
};

}  // namespace

TEST(PartitionAllocLockTest, SlowThreads) {
  int counter = 0;  // *Not* atomic.
  std::vector<base::PlatformThreadHandle> thread_handles;
  constexpr int iterations_per_thread = 100;
  constexpr int num_threads = 4;

  Lock lock;
  Lock start_lock;

  ThreadDelegateForSlowThreads delegate(start_lock, lock, iterations_per_thread,
                                        counter);

  start_lock.Acquire();  // Make sure that the threads compete, by waiting until
                         // all of them have at least been created.
  for (int i = 0; i < num_threads; i++) {
    base::PlatformThreadHandle handle;
    base::PlatformThreadForTesting::Create(0, &delegate, &handle);
    thread_handles.push_back(handle);
  }

  start_lock.Release();

  for (int i = 0; i < num_threads; i++) {
    base::PlatformThreadForTesting::Join(thread_handles[i]);
  }
  EXPECT_EQ(iterations_per_thread * num_threads, counter);
}

TEST(PartitionAllocLockTest, AssertAcquired) {
  Lock lock;
  lock.Acquire();
  lock.AssertAcquired();
  lock.Release();
}

#if defined(GTEST_HAS_DEATH_TEST) && \
    (PA_BUILDFLAG(DCHECKS_ARE_ON) || \
     PA_BUILDFLAG(ENABLE_PARTITION_LOCK_REENTRANCY_CHECK))

// Need to bypass `-Wthread-safety-analysis` for this test.
#if __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wthread-safety-analysis"
#endif

TEST(PartitionAllocLockTest, ReentrancyDeathTest) {
  Lock lock;
  lock.Acquire();
  EXPECT_DEATH(lock.Acquire(), "");
}

#if __clang__
#pragma clang diagnostic pop
#endif

#endif  // defined(GTEST_HAS_DEATH_TEST) && (PA_BUILDFLAG(DCHECKS_ARE_ON) ||
        // PA_BUILDFLAG(ENABLE_PARTITION_LOCK_REENTRANCY_CHECK))

// AssertAcquired() is only enforced with DCHECK()s.
// DCHECKs don't work with EXPECT_DEATH on official builds.
#if defined(GTEST_HAS_DEATH_TEST) && PA_BUILDFLAG(DCHECKS_ARE_ON) && \
    (!defined(OFFICIAL_BUILD) || !defined(NDEBUG))

TEST(PartitionAllocLockTest, AssertAcquiredDeathTest) {
  Lock lock;
  EXPECT_DEATH(lock.AssertAcquired(), "");
}

namespace {

class ThreadDelegateForAssertAcquiredAnotherThreadHoldsTheLock
    : public base::PlatformThreadForTesting::Delegate {
 public:
  explicit ThreadDelegateForAssertAcquiredAnotherThreadHoldsTheLock(Lock& lock)
      : lock_(lock) {}

  void ThreadMain() PA_NO_THREAD_SAFETY_ANALYSIS override { lock_.Acquire(); }

 private:
  Lock& lock_;
};

}  // namespace

TEST(PartitionAllocLockTest, AssertAcquiredAnotherThreadHoldsTheLock) {
  Lock lock;
  // PA_NO_THREAD_SAFETY_ANALYSIS: The checker rightfully points out that the
  // lock is still held at the end of the function, which is what we want here.
  ThreadDelegateForAssertAcquiredAnotherThreadHoldsTheLock delegate(lock);
  base::PlatformThreadHandle handle;
  base::PlatformThreadForTesting::Create(0, &delegate, &handle);
  // Join before the test, otherwise some platforms' gtest have trouble with
  // EXPECT_DEATH() and multiple live threads.
  base::PlatformThreadForTesting::Join(handle);

  // DCHECKs don't work with EXPECT_DEATH on official builds.
#if PA_BUILDFLAG(DCHECKS_ARE_ON) && \
    (!defined(OFFICIAL_BUILD) || !defined(NDEBUG))
  EXPECT_DEATH(lock.AssertAcquired(), "");
#endif
}

#if PA_BUILDFLAG(IS_APPLE)

namespace {

class ThreadDelegateForReinitInOtherThread
    : public base::PlatformThreadForTesting::Delegate {
 public:
  explicit ThreadDelegateForReinitInOtherThread(Lock& lock) : lock_(lock) {}

  void ThreadMain() PA_NO_THREAD_SAFETY_ANALYSIS override {
    lock_.Reinit();
    lock_.Acquire();
    lock_.Release();
  }

 private:
  Lock& lock_;
};

}  // namespace

// On Apple OSes, it is not allowed to unlock a lock from another thread, so
// we need to re-initialize it.
TEST(PartitionAllocLockTest, ReinitInOtherThread) PA_NO_THREAD_SAFETY_ANALYSIS {
  Lock lock;
  lock.Acquire();

  ThreadDelegateForReinitInOtherThread delegate(lock);
  base::PlatformThreadHandle handle;
  base::PlatformThreadForTesting::Create(0, &delegate, &handle);
  base::PlatformThreadForTesting::Join(handle);
}
#endif  // PA_BUILDFLAG(IS_APPLE)

#endif  // defined(GTEST_HAS_DEATH_TEST) && PA_BUILDFLAG(DCHECKS_ARE_ON)

}  // namespace partition_alloc::internal
