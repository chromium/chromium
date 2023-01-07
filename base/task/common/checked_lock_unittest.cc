// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/common/checked_lock.h"

#include <stdlib.h>

#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "base/rand_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/gtest_util.h"
#include "base/threading/platform_thread.h"
#include "base/threading/simple_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace internal {
namespace {

// Adapted from base::Lock's BasicLockTestThread to make sure
// Acquire()/Release() don't crash.
class BasicLockTestThread : public SimpleThread {
 public:
  explicit BasicLockTestThread(CheckedLock* lock)
      : SimpleThread("BasicLockTestThread"), lock_(lock), acquired_(0) {}

  BasicLockTestThread(const BasicLockTestThread&) = delete;
  BasicLockTestThread& operator=(const BasicLockTestThread&) = delete;

  int acquired() const { return acquired_; }

 private:
  void Run() override {
    for (int i = 0; i < 10; i++) {
      lock_->Acquire();
      acquired_++;
      lock_->Release();
    }
    for (int i = 0; i < 10; i++) {
      lock_->Acquire();
      acquired_++;
      PlatformThread::Sleep(Milliseconds(base::RandInt(0, 19)));
      lock_->Release();
    }
  }

  const raw_ptr<CheckedLock> lock_;
  int acquired_;
};

class BasicLockAcquireAndWaitThread : public SimpleThread {
 public:
  explicit BasicLockAcquireAndWaitThread(CheckedLock* lock)
      : SimpleThread("BasicLockAcquireAndWaitThread"),
        lock_(lock),
        lock_acquire_event_(WaitableEvent::ResetPolicy::AUTOMATIC,
                            WaitableEvent::InitialState::NOT_SIGNALED),
        main_thread_continue_event_(WaitableEvent::ResetPolicy::AUTOMATIC,
                                    WaitableEvent::InitialState::NOT_SIGNALED) {
  }

  BasicLockAcquireAndWaitThread(const BasicLockAcquireAndWaitThread&) = delete;
  BasicLockAcquireAndWaitThread& operator=(
      const BasicLockAcquireAndWaitThread&) = delete;

  void WaitForLockAcquisition() { lock_acquire_event_.Wait(); }

  void ContinueMain() { main_thread_continue_event_.Signal(); }

 private:
  void Run() override {
    lock_->Acquire();
    lock_acquire_event_.Signal();
    main_thread_continue_event_.Wait();
    lock_->Release();
  }

  const raw_ptr<CheckedLock> lock_;
  WaitableEvent lock_acquire_event_;
  WaitableEvent main_thread_continue_event_;
};

}  // namespace

TEST(CheckedLockTest, Basic) {
  CheckedLock lock;
  BasicLockTestThread thread(&lock);

  thread.Start();

  int acquired = 0;
  for (int i = 0; i < 5; i++) {
    lock.Acquire();
    acquired++;
    lock.Release();
  }
  for (int i = 0; i < 10; i++) {
    lock.Acquire();
    acquired++;
    PlatformThread::Sleep(Milliseconds(base::RandInt(0, 19)));
    lock.Release();
  }
  for (int i = 0; i < 5; i++) {
    lock.Acquire();
    acquired++;
    PlatformThread::Sleep(Milliseconds(base::RandInt(0, 19)));
    lock.Release();
  }

  thread.Join();

  EXPECT_EQ(acquired, 20);
  EXPECT_EQ(thread.acquired(), 20);
}

TEST(CheckedLockTest, AcquirePredecessor) {
  CheckedLock predecessor;
  CheckedLock lock(&predecessor);
  predecessor.Acquire();
  lock.Acquire();
  lock.Release();
  predecessor.Release();
}

// Here and below, disable thread safety analysis, otherwise our death tests do
// not compile (the issues are caught at compile time).
TEST(CheckedLockTest, AcquirePredecessorWrongOrder)
NO_THREAD_SAFETY_ANALYSIS {
  CheckedLock predecessor;
  CheckedLock lock(&predecessor);
  EXPECT_DCHECK_DEATH({
    lock.Acquire();
    predecessor.Acquire();
  });
}

TEST(CheckedLockTest, AcquireNonPredecessor) NO_THREAD_SAFETY_ANALYSIS {
  CheckedLock lock1;
  CheckedLock lock2;
  EXPECT_DCHECK_DEATH({
    lock1.Acquire();
    lock2.Acquire();
  });
}

TEST(CheckedLockTest, AcquireMultipleLocksInOrder) {
  CheckedLock lock1;
  CheckedLock lock2(&lock1);
  CheckedLock lock3(&lock2);
  lock1.Acquire();
  lock2.Acquire();
  lock3.Acquire();
  lock3.Release();
  lock2.Release();
  lock1.Release();
}

TEST(CheckedLockTest, AcquireMultipleLocksInTheMiddleOfAChain) {
  CheckedLock lock1;
  CheckedLock lock2(&lock1);
  CheckedLock lock3(&lock2);
  lock2.Acquire();
  lock3.Acquire();
  lock3.Release();
  lock2.Release();
}

TEST(CheckedLockTest, AcquireMultipleLocksNoTransitivity)
NO_THREAD_SAFETY_ANALYSIS {
  CheckedLock lock1;
  CheckedLock lock2(&lock1);
  CheckedLock lock3(&lock2);
  EXPECT_DCHECK_DEATH({
    lock1.Acquire();
    lock3.Acquire();
  });
}

TEST(CheckedLockTest, AcquireLocksDifferentThreadsSafely) {
  CheckedLock lock1;
  CheckedLock lock2;
  BasicLockAcquireAndWaitThread thread(&lock1);
  thread.Start();

  lock2.Acquire();
  thread.WaitForLockAcquisition();
  thread.ContinueMain();
  thread.Join();
  lock2.Release();
}

TEST(CheckedLockTest,
     AcquireLocksWithPredecessorDifferentThreadsSafelyPredecessorFirst) {
  // A lock and its predecessor may be safely acquired on different threads.
  // This Thread                Other Thread
  // predecessor.Acquire()
  //                            lock.Acquire()
  // predecessor.Release()
  //                            lock.Release()
  CheckedLock predecessor;
  CheckedLock lock(&predecessor);
  predecessor.Acquire();
  BasicLockAcquireAndWaitThread thread(&lock);
  thread.Start();
  thread.WaitForLockAcquisition();
  predecessor.Release();
  thread.ContinueMain();
  thread.Join();
}

TEST(CheckedLockTest,
     AcquireLocksWithPredecessorDifferentThreadsSafelyPredecessorLast) {
  // A lock and its predecessor may be safely acquired on different threads.
  // This Thread                Other Thread
  // lock.Acquire()
  //                            predecessor.Acquire()
  // lock.Release()
  //                            predecessor.Release()
  CheckedLock predecessor;
  CheckedLock lock(&predecessor);
  lock.Acquire();
  BasicLockAcquireAndWaitThread thread(&predecessor);
  thread.Start();
  thread.WaitForLockAcquisition();
  lock.Release();
  thread.ContinueMain();
  thread.Join();
}

TEST(CheckedLockTest,
     AcquireLocksWithPredecessorDifferentThreadsSafelyNoInterference) {
  // Acquisition of an unrelated lock on another thread should not affect a
  // legal lock acquisition with a predecessor on this thread.
  // This Thread                Other Thread
  // predecessor.Acquire()
  //                            unrelated.Acquire()
  // lock.Acquire()
  //                            unrelated.Release()
  // lock.Release()
  // predecessor.Release();
  CheckedLock predecessor;
  CheckedLock lock(&predecessor);
  predecessor.Acquire();
  CheckedLock unrelated;
  BasicLockAcquireAndWaitThread thread(&unrelated);
  thread.Start();
  thread.WaitForLockAcquisition();
  lock.Acquire();
  thread.ContinueMain();
  thread.Join();
  lock.Release();
  predecessor.Release();
}

TEST(CheckedLockTest, SelfReferentialLock) {
  struct SelfReferentialLock {
    SelfReferentialLock() : lock(&lock) {}

    CheckedLock lock;
  };

  EXPECT_DCHECK_DEATH({ SelfReferentialLock lock; });
}

TEST(CheckedLockTest, PredecessorCycle) {
  struct LockCycle {
    LockCycle() : lock1(&lock2), lock2(&lock1) {}

    CheckedLock lock1;
    CheckedLock lock2;
  };

  EXPECT_DCHECK_DEATH({ LockCycle cycle; });
}

TEST(CheckedLockTest, PredecessorLongerCycle) {
  struct LockCycle {
    LockCycle()
        : lock1(&lock5),
          lock2(&lock1),
          lock3(&lock2),
          lock4(&lock3),
          lock5(&lock4) {}

    CheckedLock lock1;
    CheckedLock lock2;
    CheckedLock lock3;
    CheckedLock lock4;
    CheckedLock lock5;
  };

  EXPECT_DCHECK_DEATH({ LockCycle cycle; });
}

TEST(CheckedLockTest, AcquireLockAfterUniversalPredecessor) {
  // Acquisition of a universal-predecessor lock should not prevent acquisition
  // of a CheckedLock after it.
  CheckedLock universal_predecessor((UniversalPredecessor()));
  CheckedLock lock;

  universal_predecessor.Acquire();
  lock.Acquire();
  lock.Release();
  universal_predecessor.Release();
}

TEST(CheckedLockTest, AcquireMultipleLocksAfterUniversalPredecessor)
NO_THREAD_SAFETY_ANALYSIS {
  // Acquisition of a universal-predecessor lock does not affect acquisition
  // rules for locks beyond the one acquired directly after it.
  CheckedLock universal_predecessor{UniversalPredecessor()};
  CheckedLock lock;
  CheckedLock lock2(&lock);
  CheckedLock lock3;

  universal_predecessor.Acquire();
  lock.Acquire();
  lock2.Acquire();
  lock2.Release();
  lock.Release();
  universal_predecessor.Release();

  EXPECT_DCHECK_DEATH({
    universal_predecessor.Acquire();
    lock.Acquire();
    lock3.Acquire();
  });
}

TEST(CheckedLockTest, AcquireUniversalPredecessorAfterLock)
NO_THREAD_SAFETY_ANALYSIS {
  // A universal-predecessor lock may not be acquired after any other lock.
  CheckedLock universal_predecessor{UniversalPredecessor()};
  CheckedLock lock;

  EXPECT_DCHECK_DEATH({
    lock.Acquire();
    universal_predecessor.Acquire();
  });
}

TEST(CheckedLockTest, AcquireUniversalPredecessorAfterUniversalPredecessor)
NO_THREAD_SAFETY_ANALYSIS {
  // A universal-predecessor lock may not be acquired after any other lock, not
  // even another universal predecessor.
  CheckedLock universal_predecessor{UniversalPredecessor()};
  CheckedLock universal_predecessor2{UniversalPredecessor()};

  EXPECT_DCHECK_DEATH({
    universal_predecessor.Acquire();
    universal_predecessor2.Acquire();
  });
}

TEST(CheckedLockTest, AcquireLockBeforeUniversalSuccessor) {
  // Acquisition of a universal-successor lock should be allowed
  // after any other acquisition.
  CheckedLock universal_successor{UniversalSuccessor()};
  CheckedLock lock;

  lock.Acquire();
  universal_successor.Acquire();
  universal_successor.Release();
  lock.Release();
}

TEST(CheckedLockTest, AcquireMultipleLocksBeforeAndAfterUniversalSuccessor)
NO_THREAD_SAFETY_ANALYSIS {
  // Acquisition of a universal-successor lock does not affect acquisition
  // rules for locks beyond the one acquired directly after it.
  CheckedLock lock;
  CheckedLock universal_successor{UniversalSuccessor()};
  CheckedLock lock2;

  lock.Acquire();
  universal_successor.Acquire();
  universal_successor.Release();
  lock.Release();

  EXPECT_DCHECK_DEATH({
    universal_successor.Acquire();
    lock2.Acquire();
  });
}

TEST(CheckedLockTest, AcquireUniversalSuccessorBeforeLock)
NO_THREAD_SAFETY_ANALYSIS {
  // A universal-successor lock may not be acquired before any other lock.
  CheckedLock universal_successor{UniversalSuccessor()};
  CheckedLock lock;

  EXPECT_DCHECK_DEATH({
    universal_successor.Acquire();
    lock.Acquire();
  });
}

TEST(CheckedLockTest, AcquireUniversalSuccessorAfterUniversalSuccessor)
NO_THREAD_SAFETY_ANALYSIS {
  // A universal-successor lock may not be acquired before any other lock, not
  // even another universal successor.
  CheckedLock universal_successor{UniversalSuccessor()};
  CheckedLock universal_successor2{UniversalSuccessor()};

  EXPECT_DCHECK_DEATH({
    universal_successor.Acquire();
    universal_successor2.Acquire();
  });
}

TEST(CheckedLockTest, UniversalSuccessorAsPredecessor)
NO_THREAD_SAFETY_ANALYSIS {
  // A universal-successor lock cannot be declared as a predecessor to
  // any other lock.
  CheckedLock universal_successor{UniversalSuccessor()};
  EXPECT_DCHECK_DEATH({ CheckedLock banned_successor(&universal_successor); });
}

TEST(CheckedLockTest, AssertNoLockHeldOnCurrentThread) {
  // AssertNoLockHeldOnCurrentThread() shouldn't fail when no lock is acquired.
  CheckedLock::AssertNoLockHeldOnCurrentThread();

  // AssertNoLockHeldOnCurrentThread() should fail when a lock is acquired.
  CheckedLock lock;
  {
    CheckedAutoLock auto_lock(lock);
    EXPECT_DCHECK_DEATH({ CheckedLock::AssertNoLockHeldOnCurrentThread(); });
  }
}

namespace {

class MemberGuardedByLock {
 public:
  CheckedLock lock_;
  int value GUARDED_BY(lock_) = 0;
};

}  // namespace

TEST(CheckedLockTest, AnnotateAcquiredLockAlias) {
  MemberGuardedByLock member_guarded_by_lock;
  CheckedLock* acquired = &member_guarded_by_lock.lock_;
  CheckedAutoLock auto_lock(*acquired);
  AnnotateAcquiredLockAlias annotate(*acquired, member_guarded_by_lock.lock_);
  member_guarded_by_lock.value = 42;  // Doesn't compile without |annotate|.
}

}  // namespace internal
}  // namespace base
