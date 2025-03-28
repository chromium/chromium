// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/synchronization/lock.h"

#include <stdint.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <memory>
#include <utility>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/dcheck_is_on.h"
#include "base/features.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/functional/function_ref.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/profiler/thread_delegate.h"
#include "base/rand_util.h"
#include "base/synchronization/lock_impl.h"
#include "base/synchronization/lock_subtle.h"
#include "base/system/sys_info.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/thread_annotations.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::UnorderedElementsAre;
using testing::UnorderedElementsAreArray;

namespace base {

// Basic test to make sure that Acquire()/Release()/Try() don't crash ----------

class BasicLockTestThread : public PlatformThread::Delegate {
 public:
  explicit BasicLockTestThread(Lock* lock) : lock_(lock) {}

  BasicLockTestThread(const BasicLockTestThread&) = delete;
  BasicLockTestThread& operator=(const BasicLockTestThread&) = delete;

  void ThreadMain() override {
    for (int i = 0; i < 10; i++) {
      lock_->Acquire();
      acquired_++;
      lock_->Release();
    }
    for (int i = 0; i < 10; i++) {
      lock_->Acquire();
      acquired_++;
      PlatformThread::Sleep(RandTimeDeltaUpTo(Milliseconds(20)));
      lock_->Release();
    }
    for (int i = 0; i < 10; i++) {
      if (lock_->Try()) {
        acquired_++;
        PlatformThread::Sleep(RandTimeDeltaUpTo(Milliseconds(20)));
        lock_->Release();
      }
    }
  }

  int acquired() const { return acquired_; }

 private:
  raw_ptr<Lock> lock_;
  int acquired_ = 0;
};

TEST(LockTest, Basic) {
  Lock lock;
  BasicLockTestThread thread(&lock);
  PlatformThreadHandle handle;

  ASSERT_TRUE(PlatformThread::Create(0, &thread, &handle));

  int acquired = 0;
  for (int i = 0; i < 5; i++) {
    lock.Acquire();
    acquired++;
    lock.Release();
  }
  for (int i = 0; i < 10; i++) {
    lock.Acquire();
    acquired++;
    PlatformThread::Sleep(RandTimeDeltaUpTo(Milliseconds(20)));
    lock.Release();
  }
  for (int i = 0; i < 10; i++) {
    if (lock.Try()) {
      acquired++;
      PlatformThread::Sleep(RandTimeDeltaUpTo(Milliseconds(20)));
      lock.Release();
    }
  }
  for (int i = 0; i < 5; i++) {
    lock.Acquire();
    acquired++;
    PlatformThread::Sleep(RandTimeDeltaUpTo(Milliseconds(20)));
    lock.Release();
  }

  PlatformThread::Join(handle);

  EXPECT_GE(acquired, 20);
  EXPECT_GE(thread.acquired(), 20);
}

// Test that Try() works as expected -------------------------------------------

class TryLockTestThread : public PlatformThread::Delegate {
 public:
  explicit TryLockTestThread(Lock* lock) : lock_(lock) {}

  TryLockTestThread(const TryLockTestThread&) = delete;
  TryLockTestThread& operator=(const TryLockTestThread&) = delete;

  void ThreadMain() override {
    // The local variable is required for the static analyzer to see that the
    // lock is properly released.
    bool got_lock = lock_->Try();
    got_lock_ = got_lock;
    if (got_lock) {
      lock_->Release();
    }
  }

  bool got_lock() const { return got_lock_; }

 private:
  raw_ptr<Lock> lock_;
  bool got_lock_ = false;
};

TEST(LockTest, TryLock) {
  Lock lock;

  ASSERT_TRUE(lock.Try());
  lock.AssertAcquired();

  // This thread will not be able to get the lock.
  {
    TryLockTestThread thread(&lock);
    PlatformThreadHandle handle;

    ASSERT_TRUE(PlatformThread::Create(0, &thread, &handle));

    PlatformThread::Join(handle);

    ASSERT_FALSE(thread.got_lock());
  }

  lock.Release();

  // This thread will....
  {
    TryLockTestThread thread(&lock);
    PlatformThreadHandle handle;

    ASSERT_TRUE(PlatformThread::Create(0, &thread, &handle));

    PlatformThread::Join(handle);

    ASSERT_TRUE(thread.got_lock());
    // But it released it....
    ASSERT_TRUE(lock.Try());
    lock.AssertAcquired();
  }

  lock.Release();
}

// Tests that locks actually exclude -------------------------------------------

class MutexLockTestThread : public PlatformThread::Delegate {
 public:
  MutexLockTestThread(Lock* lock, int* value) : lock_(lock), value_(value) {}

  MutexLockTestThread(const MutexLockTestThread&) = delete;
  MutexLockTestThread& operator=(const MutexLockTestThread&) = delete;

  // Static helper which can also be called from the main thread.
  static void DoStuff(Lock* lock, int* value) {
    for (int i = 0; i < 40; i++) {
      lock->Acquire();
      int v = *value;
      PlatformThread::Sleep(RandTimeDeltaUpTo(Milliseconds(10)));
      *value = v + 1;
      lock->Release();
    }
  }

  void ThreadMain() override { DoStuff(lock_, value_); }

 private:
  raw_ptr<Lock> lock_;
  raw_ptr<int> value_;
};

TEST(LockTest, MutexTwoThreads) {
  Lock lock;
  int value = 0;

  MutexLockTestThread thread(&lock, &value);
  PlatformThreadHandle handle;

  ASSERT_TRUE(PlatformThread::Create(0, &thread, &handle));

  MutexLockTestThread::DoStuff(&lock, &value);

  PlatformThread::Join(handle);

  EXPECT_EQ(2 * 40, value);
}

TEST(LockTest, MutexFourThreads) {
  Lock lock;
  int value = 0;

  MutexLockTestThread thread1(&lock, &value);
  MutexLockTestThread thread2(&lock, &value);
  MutexLockTestThread thread3(&lock, &value);
  PlatformThreadHandle handle1;
  PlatformThreadHandle handle2;
  PlatformThreadHandle handle3;

  ASSERT_TRUE(PlatformThread::Create(0, &thread1, &handle1));
  ASSERT_TRUE(PlatformThread::Create(0, &thread2, &handle2));
  ASSERT_TRUE(PlatformThread::Create(0, &thread3, &handle3));

  MutexLockTestThread::DoStuff(&lock, &value);

  PlatformThread::Join(handle1);
  PlatformThread::Join(handle2);
  PlatformThread::Join(handle3);

  EXPECT_EQ(4 * 40, value);
}

// Test invariant checking -----------------------------------------------------

TEST(LockTest, InvariantIsCalled) {
  // This test should compile and execute safely regardless of invariant
  // checking, but if `kInvariantsActive` is false, we don't expect the
  // invariant to be checked when the lock state changes.
  constexpr bool kInvariantsActive = DCHECK_IS_ON();

  class InvariantChecker {
   public:
    explicit InvariantChecker(const Lock& lock LIFETIME_BOUND) : lock(lock) {}
    void Check() ASSERT_EXCLUSIVE_LOCK(lock) {
      lock->AssertAcquired();
      invariant_called = true;
    }
    bool TestAndReset() { return std::exchange(invariant_called, false); }

   private:
    const raw_ref<const Lock> lock;
    bool invariant_called = false;
  };

  // Awkward construction order here allows `checker` to refer to `lock`, which
  // refers to `check_ref`, which refers to `check`, which refers to `checker`.
  std::unique_ptr<InvariantChecker> checker;
  auto check = [&] { checker->Check(); };
  auto check_ref = base::FunctionRef<void()>(check);
  Lock lock([&](Lock* lp) {
    checker = std::make_unique<InvariantChecker>(*lp);
    return check_ref;
  }(&lock));

  EXPECT_FALSE(checker->TestAndReset());

  lock.Acquire();
  EXPECT_EQ(kInvariantsActive, checker->TestAndReset());

  lock.Release();
  EXPECT_EQ(kInvariantsActive, checker->TestAndReset());
}

// AutoLock tests --------------------------------------------------------------

TEST(LockTest, AutoLockMaybe) {
  Lock lock;
  {
    AutoLockMaybe auto_lock(&lock);
    lock.AssertAcquired();
  }
  EXPECT_DCHECK_DEATH(lock.AssertAcquired());
}

TEST(LockTest, AutoLockMaybeNull) {
  AutoLockMaybe auto_lock(nullptr);
}

TEST(LockTest, ReleasableAutoLockExplicitRelease) {
  Lock lock;
  ReleasableAutoLock auto_lock(&lock);
  lock.AssertAcquired();
  auto_lock.Release();
  EXPECT_DCHECK_DEATH(lock.AssertAcquired());
}

TEST(LockTest, ReleasableAutoLockImplicitRelease) {
  Lock lock;
  {
    ReleasableAutoLock auto_lock(&lock);
    lock.AssertAcquired();
  }
  EXPECT_DCHECK_DEATH(lock.AssertAcquired());
}

class TryLockTest : public testing::Test {
 protected:
  Lock lock_;
  int x_ GUARDED_BY(lock_) = 0;
};

// Verifies thread safety annotations do not prevent correct `AutoTryLock` usage
// from compiling. A dual of this test exists in lock_nocompile.nc. For more
// context, see <https://crbug.com/340196356>.
TEST_F(TryLockTest, CorrectlyCheckIsAcquired) {
  AutoTryLock maybe(lock_);
  // Should compile because we correctly check whether the lock is acquired
  // before writing to `x_`.
  if (maybe.is_acquired()) {
    x_ = 5;
  }
}

#if DCHECK_IS_ON()

TEST(LockTest, GetTrackedLocksHeldByCurrentThread) {
  Lock lock_a;
  Lock lock_b;
  Lock lock_c;
  const uintptr_t lock_a_ptr = reinterpret_cast<uintptr_t>(&lock_a);
  const uintptr_t lock_b_ptr = reinterpret_cast<uintptr_t>(&lock_b);
  const uintptr_t lock_c_ptr = reinterpret_cast<uintptr_t>(&lock_c);

  EXPECT_THAT(subtle::GetTrackedLocksHeldByCurrentThread(),
              UnorderedElementsAre());
  ReleasableAutoLock auto_lock_a(&lock_a, subtle::LockTracking::kEnabled);
  EXPECT_THAT(subtle::GetTrackedLocksHeldByCurrentThread(),
              UnorderedElementsAre(lock_a_ptr));
  ReleasableAutoLock auto_lock_b(&lock_b, subtle::LockTracking::kEnabled);
  EXPECT_THAT(subtle::GetTrackedLocksHeldByCurrentThread(),
              UnorderedElementsAre(lock_a_ptr, lock_b_ptr));
  auto_lock_a.Release();
  EXPECT_THAT(subtle::GetTrackedLocksHeldByCurrentThread(),
              UnorderedElementsAre(lock_b_ptr));
  ReleasableAutoLock auto_lock_c(&lock_c, subtle::LockTracking::kEnabled);
  EXPECT_THAT(subtle::GetTrackedLocksHeldByCurrentThread(),
              UnorderedElementsAre(lock_b_ptr, lock_c_ptr));
  auto_lock_c.Release();
  EXPECT_THAT(subtle::GetTrackedLocksHeldByCurrentThread(),
              UnorderedElementsAre(lock_b_ptr));
  auto_lock_b.Release();
  EXPECT_THAT(subtle::GetTrackedLocksHeldByCurrentThread(),
              UnorderedElementsAre());
}

TEST(LockTest, GetTrackedLocksHeldByCurrentThread_AutoLock) {
  Lock lock;
  const uintptr_t lock_ptr = reinterpret_cast<uintptr_t>(&lock);
  AutoLock auto_lock(lock, subtle::LockTracking::kEnabled);
  EXPECT_THAT(subtle::GetTrackedLocksHeldByCurrentThread(),
              UnorderedElementsAre(lock_ptr));
}

TEST(LockTest, GetTrackedLocksHeldByCurrentThread_MovableAutoLock) {
  Lock lock;
  const uintptr_t lock_ptr = reinterpret_cast<uintptr_t>(&lock);
  MovableAutoLock auto_lock(lock, subtle::LockTracking::kEnabled);
  EXPECT_THAT(subtle::GetTrackedLocksHeldByCurrentThread(),
              UnorderedElementsAre(lock_ptr));
}

TEST(LockTest, GetTrackedLocksHeldByCurrentThread_AutoTryLock) {
  Lock lock;
  const uintptr_t lock_ptr = reinterpret_cast<uintptr_t>(&lock);
  AutoTryLock auto_lock(lock, subtle::LockTracking::kEnabled);
  EXPECT_THAT(subtle::GetTrackedLocksHeldByCurrentThread(),
              UnorderedElementsAre(lock_ptr));
}

TEST(LockTest, GetTrackedLocksHeldByCurrentThread_AutoLockMaybe) {
  Lock lock;
  const uintptr_t lock_ptr = reinterpret_cast<uintptr_t>(&lock);
  AutoLockMaybe auto_lock(&lock, subtle::LockTracking::kEnabled);
  EXPECT_THAT(subtle::GetTrackedLocksHeldByCurrentThread(),
              UnorderedElementsAre(lock_ptr));
}

TEST(LockTest, GetTrackedLocksHeldByCurrentThreadOverCapacity)
// Thread-safety analysis doesn't handle the array of locks properly.
NO_THREAD_SAFETY_ANALYSIS {
  constexpr size_t kHeldLocksCapacity = 10;
  std::array<Lock, kHeldLocksCapacity + 1> locks;

  for (size_t i = 0; i < kHeldLocksCapacity; ++i) {
    locks[i].Acquire(subtle::LockTracking::kEnabled);
  }

  EXPECT_CHECK_DEATH({
    locks[kHeldLocksCapacity].Acquire(subtle::LockTracking::kEnabled);
    locks[kHeldLocksCapacity].Release();
  });

  for (size_t i = 0; i < kHeldLocksCapacity; ++i) {
    locks[i].Release();

    std::vector<uintptr_t> expected_locks;
    for (size_t j = i + 1; j < kHeldLocksCapacity; ++j) {
      expected_locks.push_back(reinterpret_cast<uintptr_t>(&locks[j]));
    }

    EXPECT_THAT(subtle::GetTrackedLocksHeldByCurrentThread(),
                UnorderedElementsAreArray(expected_locks));
  }
}

TEST(LockTest, TrackingDisabled) {
  Lock lock;
  AutoLock auto_lock(lock, subtle::LockTracking::kDisabled);
  EXPECT_TRUE(subtle::GetTrackedLocksHeldByCurrentThread().empty());
}

// Priority Inheritance Tests --------------------------------------------------

#if BUILDFLAG(ENABLE_MUTEX_PRIORITY_INHERITANCE)
namespace {
class PriorityInheritanceTest {
 public:
  // The average value of MeasureRunTime() over |num_samples| iterations.
  static TimeDelta MeasureAverageRunTime(int num_samples = 10) {
    TimeDelta total_runtime;
    for (int i = 0; i < num_samples; i++) {
      total_runtime += MeasureRunTime();
    }

    return total_runtime / num_samples;
  }

  // Measure the time taken for a low-priority thread (kBackground) to perform
  // CPU bound work when it holds a lock that is awaited by a high-priority
  // thread (kRealtimeAudio).
  static TimeDelta MeasureRunTime() {
    Lock lock;
    TimeDelta test_run_time;
    std::atomic<bool> signal_cpu_bound_worker_threads_shutdown{false},
        signal_thread_a_will_lock{false};

    // Keep all the cores busy with a workload of CPU bound thread to reduce
    // flakiness in the test by skewing the CPU time between the high-priority
    // and low-priority measurement threads.
    std::vector<TestThread> cpu_bound_worker_threads;
    for (int i = 0; i < 15; i++) {
      cpu_bound_worker_threads.emplace_back(
          ThreadType::kDefault, base::BindLambdaForTesting([&]() {
            while (!signal_cpu_bound_worker_threads_shutdown.load(
                std::memory_order_relaxed)) {
              BusyLoop(10);
            }
          }));
    }

    for (auto& worker_thread : cpu_bound_worker_threads) {
      worker_thread.Create();
    }

    TestThread thread_a(
        ThreadType::kRealtimeAudio, base::BindLambdaForTesting([&]() {
          // Signal to thread B that the current thread will acquire the lock
          // next, so that it can to start its CPU bound work.
          signal_thread_a_will_lock.store(true, std::memory_order_relaxed);

          // Wait on the lock to be released once the low-priority thread is
          // done. In the case when priority inheritance mutexes are enabled,
          // this should boost the priority of the low-priority thread to the
          // priority of the highest priority waiter (i.e. the current thread).
          AutoLock auto_lock(lock);
          BusyLoop(10);
        }));

    TestThread thread_b(
        ThreadType::kBackground, base::BindLambdaForTesting([&]() {
          // Acquire the lock before creating the high-priority thread, so that
          // the higher priority thread is blocked on the current thread while
          // the current thread performs CPU-bound work.
          AutoLock auto_lock(lock);
          thread_a.Create();

          // Before performing the CPU bound work, wait for the thread A to
          // signal that it has started running and will acquire the lock next.
          // While it is not a perfectly reliable signal (thread A may get
          // descheduled immediately after signalling), given the relative
          // priorities of the two threads it is good enough to reduce large
          // variations due to latencies in thread bring up.
          while (!signal_thread_a_will_lock.load(std::memory_order_relaxed)) {
            usleep(10);
          }

          ElapsedTimer timer;
          BusyLoop(1000000);
          test_run_time = timer.Elapsed();
        }));

    // Create the low-priority thread which is responsible for creating the
    // high-priority thread. Wait for both threads to finish before recording
    // the elapsed time.
    thread_b.Create();
    thread_b.Join();
    thread_a.Join();

    signal_cpu_bound_worker_threads_shutdown.store(true,
                                                   std::memory_order_relaxed);
    for (auto& worker_thread : cpu_bound_worker_threads) {
      worker_thread.Join();
    }

    return test_run_time;
  }

 private:
  // CPU bound work for the threads to eat up CPU cycles.
  static void BusyLoop(size_t n) {
    __unused int sum = 0;
    for (int i = 0; i < n; i++) {
      if (base::ShouldRecordSubsampledMetric(0.5)) {
        sum += 1;
      }
    }
  }

  class TestThread : public PlatformThread::Delegate {
   public:
    explicit TestThread(ThreadType thread_type, base::OnceClosure body)
        : thread_type_(thread_type), body_(std::move(body)) {}

    void Create() {
      ASSERT_TRUE(
          PlatformThread::CreateWithType(0, this, &handle_, thread_type_));
    }

    void ThreadMain() override { std::move(body_).Run(); }

    void Join() { PlatformThread::Join(handle_); }

   private:
    ThreadType thread_type_;
    PlatformThreadHandle handle_;
    base::OnceClosure body_;
  };
};

class ScopedConfigureUsePriorityInheritanceMutex {
 public:
  explicit ScopedConfigureUsePriorityInheritanceMutex(bool enabled) {
    feature_list_.InitWithFeatureState(features::kUsePriorityInheritanceMutex,
                                       enabled);
    ResetUsePriorityInheritanceMutexForTesting();
  }

  ~ScopedConfigureUsePriorityInheritanceMutex() {
    feature_list_.Reset();
    ResetUsePriorityInheritanceMutexForTesting();
  }

 private:
  test::ScopedFeatureList feature_list_;
};

}  // namespace

// Tests that the time taken by a higher-priority thread to acquire a lock held
// by a lower-priority thread is indeed reduced by priority inheritance.
TEST(LockTest, PriorityIsInherited) {
  TimeDelta avg_test_run_time_with_pi, avg_test_run_time_without_pi;

  {
    ScopedConfigureUsePriorityInheritanceMutex config_use_pi_mutex(true);

    // Priority inheritance mutexes are not supported on Android kernels < 6.1
    if (!Lock::HandlesMultipleThreadPriorities()) {
      GTEST_SKIP() << "base::Lock does not handle multiple thread priorities "
                   << "(Kernel version: "
                   << base::SysInfo::KernelVersionNumber::Current() << ")";
    }

    avg_test_run_time_with_pi =
        PriorityInheritanceTest::MeasureAverageRunTime();
  }

  {
    ScopedConfigureUsePriorityInheritanceMutex config_use_pi_mutex(false);

    avg_test_run_time_without_pi =
        PriorityInheritanceTest::MeasureAverageRunTime();
  }

  // During the time in which the thread A is waiting on the lock to be released
  // by the thread B, the thread B runs at kBackground priority in the non-PI
  // case and at kRealtimeAudio priority in the PI case.
  //
  // Based on the Linux kernel's allocation of CPU shares documented in
  // https://elixir.bootlin.com/linux/v6.12.5/source/kernel/sched/core.c#L9998,
  // a thread running at kRealtimeAudio (nice value = -16) gets 36291 shares
  // of the CPU, a thread at kDefault (nice value = 0) get 1024 shares and a
  // thread at kBackground (nice value = 10) gets 110 shares of the CPU.
  //
  // Assuming no other threads except the ones created by this test are running,
  // during the time in which thread A is waiting on the lock to be released by
  // thread B, thread B gets 110/(15*1024 + 110) ≈ 0.7% of the CPU time in the
  // non-PI case and 36291/(36291 + 15*1024) ≈ 70% of the CPU time in the PI
  // case. This is approximately a 100x difference in CPU shares allocated to
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
  EXPECT_GT(avg_test_run_time_without_pi, 3 * avg_test_run_time_with_pi);
}
#endif  // BUILDFLAG(ENABLE_MUTEX_PRIORITY_INHERITANCE)

#endif  // DCHECK_IS_ON()

}  // namespace base
