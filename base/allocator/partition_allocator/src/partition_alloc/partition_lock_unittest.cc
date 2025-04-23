// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/partition_lock.h"

#include <algorithm>
#include <atomic>
#include <cstddef>

#if PA_BUILDFLAG(IS_POSIX)
#include <sys/resource.h>
#include <sys/utsname.h>
#endif

#include "partition_alloc/build_config.h"
#include "partition_alloc/buildflags.h"
#include "partition_alloc/partition_alloc_base/rand_util.h"
#include "partition_alloc/partition_alloc_base/thread_annotations.h"
#include "partition_alloc/partition_alloc_base/threading/platform_thread_for_testing.h"
#include "partition_alloc/partition_alloc_base/time/time.h"
#include "partition_alloc/partition_root.h"
#include "partition_alloc/spinning_mutex.h"
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
    (!defined(OFFICIAL_BUILD) || PA_BUILDFLAG(IS_DEBUG))

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
    (!defined(OFFICIAL_BUILD) || PA_BUILDFLAG(IS_DEBUG))
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

// Priority Inheritance Tests --------------------------------------------------

#if PA_BUILDFLAG(ENABLE_PARTITION_LOCK_PRIORITY_INHERITANCE)
namespace {

// CPU bound work for the threads to eat up CPU cycles.
void BusyLoop(size_t n) {
  auto generator = base::InsecureRandomGenerator::ConstructForTesting();
  int sum = 0;
  for (size_t i = 0; i < n; i++) {
    if (i == 0 || (generator.RandUint32() & 1)) {
      sum++;
    }
  }

  ASSERT_GT(sum, 0) << "The busy loop was optimized out.";
}

class TestThreadBase : public base::PlatformThreadForTesting::Delegate {
 public:
  TestThreadBase() = default;
  explicit TestThreadBase(int nice_value) : nice_value_(nice_value) {}

  void Create() {
    ASSERT_TRUE(base::PlatformThreadForTesting::Create(0, this, &handle_));
  }

  void Join() { base::PlatformThreadForTesting::Join(handle_); }

  void ThreadMain() override {
    if (nice_value_) {
      ASSERT_EQ(setpriority(PRIO_PROCESS, 0, nice_value_), 0);
    }

    Body();
  }

  virtual void Body() = 0;

 private:
  int nice_value_;
  base::PlatformThreadHandle handle_;
};

class PriorityInheritanceTest {
 public:
  // The average value of MeasureRunTime() over |num_samples| iterations.
  static base::TimeDelta MeasureAverageRunTime(int num_samples = 10) {
    base::TimeDelta total_run_time;
    for (int i = 0; i < num_samples; i++) {
      total_run_time += MeasureRunTime();
    }

    return total_run_time / num_samples;
  }

  // Measure the time taken for a low-priority thread (nice value = 2) to
  // perform CPU bound work when it holds a lock that is awaited by a
  // high-priority thread (nice value = -16).
  static base::TimeDelta MeasureRunTime() {
    Lock lock;

    {
      // Migrate the lock to use the PI futex, if enabled, by acquiring and
      // releasing the lock.
      ScopedGuard guard(lock);
    }

    HighPriorityThread thread_a(&lock);
    LowPriorityThread thread_b(&lock, &thread_a);

    // Create the low-priority thread which is responsible for creating the
    // high-priority thread. Wait for both threads to finish before recording
    // the elapsed time.
    thread_b.Create();
    thread_b.Join();
    thread_a.Join();

    return thread_b.GetTestRunTime();
  }

  // Class that spawns a thread on object creation to perform CPU bound work
  // until the object is destroyed.
  class CPUBoundWorkerThread : public TestThreadBase {
   public:
    explicit CPUBoundWorkerThread() : should_shutdown_(false) { Create(); }

    ~CPUBoundWorkerThread() override {
      should_shutdown_.store(true, std::memory_order_relaxed);
      Join();
    }

    void Body() override {
      while (!should_shutdown_.load(std::memory_order_relaxed)) {
        BusyLoop(10);
      }
    }

   private:
    std::atomic<bool> should_shutdown_;
  };

  class HighPriorityThread : public TestThreadBase {
   public:
    explicit HighPriorityThread(Lock* lock)
        : TestThreadBase(-16), lock_(lock) {}

    void Body() override {
      // Wait on the lock to be released once the low-priority thread is
      // done. In the case when priority inheritance mutexes are enabled,
      // this should boost the priority of the low-priority thread to the
      // priority of the highest priority waiter (i.e. the current thread).
      ScopedGuard guard(*lock_);
      BusyLoop(10);
    }

   private:
    Lock* lock_;
  };

  class LowPriorityThread : public TestThreadBase {
   public:
    LowPriorityThread(Lock* lock, HighPriorityThread* thread_a)
        : TestThreadBase(2), lock_(lock), thread_a_(thread_a) {}

    void Body() override {
      // Acquire the lock before creating the high-priority thread, so that
      // the higher priority thread is blocked on the current thread while
      // the current thread performs CPU-bound work.
      ScopedGuard guard(*lock_);
      thread_a_->Create();

      // Before performing the CPU bound work, wait for the thread A to
      // begin waiting on the lock.
      //
      // NOTE: Unlike //base/synchronization/lock_unittest.cc where we use an
      // atomic to signal the low-priority thread to start its work, we instead
      // inspect the lock itself to know if there is another thread waiting on
      // the lock. This is necessary because a thread trying to acquire a
      // partition allocator lock spins in userspace yielding the CPU multiple
      // times between attempts to to acquire the lock before waiting in the
      // kernel and this process can take longer than the low-priority thread's
      // workload itself on x64 emulators. By waiting for the high-priority
      // thread to enter the kernel and potentially boost the low-priority
      // thread, the test beomes more consistent and reliable.
      while (!lock_->HasWaitersForTesting()) {
        (void)0;
      }

      base::TimeTicks start_time = base::TimeTicks::Now();
      BusyLoop(2000000);
      test_run_time_ = base::TimeTicks::Now() - start_time;
    }

    base::TimeDelta GetTestRunTime() const { return test_run_time_; }

   private:
    Lock* lock_;
    HighPriorityThread* thread_a_;
    base::TimeDelta test_run_time_;
  };
};

static int32_t major_version, minor_version, bugfix_version;

bool PriorityInheritanceLocksSupported() {
  struct utsname info;

  EXPECT_EQ(uname(&info), 0);
  int num_read = sscanf(info.release, "%d.%d.%d", &major_version,
                        &minor_version, &bugfix_version);
  if (num_read < 1) {
    major_version = 0;
  }
  if (num_read < 2) {
    minor_version = 0;
  }
  if (num_read < 3) {
    bugfix_version = 0;
  }

  // Same as ::base::KernelSupportsPriorityInheritanceFutex()
  return (major_version > 6) ||
         ((major_version == 6) &&
          ((minor_version > 12) ||
           (minor_version == 12 && bugfix_version > 13) ||
           (minor_version == 6 && bugfix_version > 29) ||
           (minor_version == 1 && bugfix_version > 75)));
}
}  // namespace

// Tests that the time taken by a higher-priority thread to acquire a lock held
// by a lower-priority thread is indeed reduced by priority inheritance.
//
// NOTE: This test is a reimplementation of LockTest.PriorityIsInherited from
// //base/synchronization/lock_unittest.cc for partition alloc with a few key
// differences to prevent the CQ bots from timing out during the test:
//   a) The low-priority thread is set to nice value 2, since it might not make
//      progress quick enough in the non-PI case.
//   b) The CPU-bound worker threads are created only once instead of
//      per-iteration to minimize the number of threads created. This is because
//      test run time on emulators is significantly affected by the time taken
//      to join all the threads.
TEST(PartitionAllocLockTest, PriorityIsInherited) {
  base::TimeDelta avg_test_run_time_with_pi, avg_test_run_time_without_pi;

  // Priority inheritance locks are not supported on Android kernels < 6.1
  if (!PriorityInheritanceLocksSupported()) {
    GTEST_SKIP()
        << "internal::Lock does not handle multiple thread priorities.";
  }

  // Keep all the cores busy with a workload of CPU bound thread to reduce
  // flakiness in the test by skewing the CPU time between the high-priority
  // and low-priority measurement threads.
  std::vector<PriorityInheritanceTest::CPUBoundWorkerThread>
      cpu_bound_worker_threads(15);

  // Since we only support migration of the lock from a non-PI lock to a PI
  // lock, the the test without PI locks needs to be run first This is because
  // unlike base::Lock, where the PI-ness of the lock is stored in the lock
  // object at lock creation time and the global state is not queried later, for
  // partition_alloc::internal::Lock the PI-ness of the lock is set at the first
  // unlock after the feature is enabled but the global feature state is still
  // queried at every unlock.
  {
    avg_test_run_time_without_pi =
        PriorityInheritanceTest::MeasureAverageRunTime();
  }

  {
    SpinningMutex::EnableUsePriorityInheritance();
    avg_test_run_time_with_pi =
        PriorityInheritanceTest::MeasureAverageRunTime();
  }

  // During the time in which the thread A is waiting on the lock to be released
  // by the thread B, the thread B runs at priority 130 in the non-PI
  // case and at priority 104 in the PI case.
  //
  // Based on the Linux kernel's allocation of CPU shares documented in
  // https://elixir.bootlin.com/linux/v6.12.5/source/kernel/sched/core.c#L9998,
  // a thread running at priority 104 (nice value = -16) gets 36291 shares of
  // the CPU, a thread at priority 120 (nice value = 0) gets 1024 shares and a
  // thread at priority 122 (nice value = 2) gets 655 shares of the CPU.
  //
  // Assuming no other threads except the ones created by this test are running,
  // during the time in which thread A is waiting on the lock to be released by
  // thread B, thread B gets 655/(15*1024 + 655) ≈ 4.1% of the CPU time in the
  // non-PI case and 36291/(36291 + 15*1024) ≈ 70% of the CPU time in the PI
  // case. This is approximately a 17x difference in CPU shares allocated to
  // the thread B when it is doing CPU-bound work.
  //
  // The test is thus designed such that the measured run time is thread B's CPU
  // bound work. While there are other factors at play that determine the
  // measured run time such as the frequency at which the CPU is running, we can
  // expect that there will be at least an order of magnitude of disparity in
  // the test run times with and without PI.
  //
  // In order to reduce test flakiness while still eliminating the possibility
  // of variance in measurements accounting for the test results, we
  // conservatively expect a 3x improvement.
  EXPECT_GT(avg_test_run_time_without_pi, 3 * avg_test_run_time_with_pi)
      << " on kernel version " << major_version << "." << minor_version << "."
      << bugfix_version;
}
namespace {
constexpr int kNumThreads = 64, kNumAcquiresPerThread = 16;
class FutexMigrationTestThread : public TestThreadBase {
 public:
  FutexMigrationTestThread(Lock* lock,
                           std::atomic<int>* num_threads_started,
                           int thread_id)
      : TestThreadBase(0),
        lock_(lock),
        num_threads_started_(num_threads_started),
        thread_id_(thread_id) {}

  void Body() override {
    num_threads_started_->fetch_add(1, std::memory_order_relaxed);
    for (int j = 0; j < kNumAcquiresPerThread; j++) {
      // Only one of the many threads we spawn triggers the migration of the
      // lock futex. We want to make it likely that there are threads waiting
      // on the non-PI futex in the kernel when the migration happens to try
      // and get better code coverage for futex migration. So we try and make
      // the migration happen somewhere in the middle of the test.
      if (j == kNumAcquiresPerThread / 4 && thread_id_ == kNumThreads / 4) {
        SpinningMutex::EnableUsePriorityInheritance();
      }

      ScopedGuard guard(*lock_);
      BusyLoop(100000);
    }
  }

 private:
  Lock* lock_;
  std::atomic<int>* num_threads_started_;
  int thread_id_;
};
}  // namespace

// The |PartitionAllocUsePriorityInheritanceLocks| feature is enabled after the
// PartitionRoot lock is initialized. This requires the lock to internally
// migrate all its waiters from the non-PI futex to the PI futex. So we test if
// the migration works correctly by enabling the feature with multiple threads
// contending for the same lock.
TEST(PartitionAllocLockTest, FutexMigration) {
  Lock lock;
  std::atomic<int> num_threads_started{0};

  std::vector<FutexMigrationTestThread> threads;
  for (int i = 0; i < kNumThreads; i++) {
    threads.emplace_back(&lock, &num_threads_started, i);
  }

  {
    ScopedGuard guard(lock);
    // Create the threads after acquiring the lock and release it only after all
    // threads have started to ensure heavy and consistent contention on the
    // lock.
    std::ranges::for_each(threads, &FutexMigrationTestThread::Create);
    while (num_threads_started.load(std::memory_order_relaxed) < kNumThreads) {
      usleep(10);
    }
  }

  // The test is complete if all the threads manage to acquire the lock
  // post-migration.
  std::ranges::for_each(threads, &FutexMigrationTestThread::Join);
}

#endif  // PA_BUILDFLAG(ENABLE_PARTITION_LOCK_PRIORITY_INHERITANCE)

}  // namespace partition_alloc::internal
