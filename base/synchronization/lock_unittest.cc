// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/synchronization/lock.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/dcheck_is_on.h"
#include "base/functional/function_ref.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/rand_util.h"
#include "base/synchronization/lock_subtle.h"
#include "base/test/gtest_util.h"
#include "base/thread_annotations.h"
#include "base/threading/platform_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::UnorderedElementsAre;
using testing::UnorderedElementsAreArray;

namespace base {

// Basic test to make sure that Acquire()/Release()/Try() don't crash ----------

class BasicLockTestThread : public PlatformThread::Delegate {
 public:
  explicit BasicLockTestThread(Lock* lock) : lock_(lock), acquired_(0) {}

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
  int acquired_;
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
  explicit TryLockTestThread(Lock* lock) : lock_(lock), got_lock_(false) {}

  TryLockTestThread(const TryLockTestThread&) = delete;
  TryLockTestThread& operator=(const TryLockTestThread&) = delete;

  void ThreadMain() override {
    // The local variable is required for the static analyzer to see that the
    // lock is properly released.
    bool got_lock = lock_->Try();
    got_lock_ = got_lock;
    if (got_lock)
      lock_->Release();
  }

  bool got_lock() const { return got_lock_; }

 private:
  raw_ptr<Lock> lock_;
  bool got_lock_;
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
  Lock lock([&] {
    checker = std::make_unique<InvariantChecker>(lock);
    return check_ref;
  }());

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

  EXPECT_DCHECK_DEATH({
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

#endif  // DCHECK_IS_ON()

}  // namespace base
