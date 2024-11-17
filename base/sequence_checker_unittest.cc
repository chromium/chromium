// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sequence_checker.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/sequence_checker_impl.h"
#include "base/sequence_token.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/lock_subtle.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "base/threading/simple_thread.h"
#include "base/threading/thread_local.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base::internal {

namespace {

// Runs a callback on another thread.
class RunCallbackThread : public SimpleThread {
 public:
  explicit RunCallbackThread(OnceClosure callback)
      : SimpleThread("RunCallbackThread"), callback_(std::move(callback)) {
    Start();
    Join();
  }
  RunCallbackThread(const RunCallbackThread&) = delete;
  RunCallbackThread& operator=(const RunCallbackThread&) = delete;

 private:
  // SimpleThread:
  void Run() override { std::move(callback_).Run(); }

  OnceClosure callback_;
};

void ExpectCalledOnValidSequence(SequenceCheckerImpl* sequence_checker) {
  ASSERT_TRUE(sequence_checker);

  // This should bind |sequence_checker| to the current sequence if it wasn't
  // already bound to a sequence.
  EXPECT_TRUE(sequence_checker->CalledOnValidSequence());

  // Since |sequence_checker| is now bound to the current sequence, another call
  // to CalledOnValidSequence() should return true.
  EXPECT_TRUE(sequence_checker->CalledOnValidSequence());
}

void ExpectNotCalledOnValidSequence(SequenceCheckerImpl* sequence_checker) {
  ASSERT_TRUE(sequence_checker);
  EXPECT_FALSE(sequence_checker->CalledOnValidSequence());
}

}  // namespace

TEST(SequenceCheckerTest, NoTaskScope) {
  SequenceCheckerImpl sequence_checker;
  EXPECT_TRUE(sequence_checker.CalledOnValidSequence());
}

TEST(SequenceCheckerTest, TaskScope) {
  TaskScope task_scope(SequenceToken::Create(),
                       /* is_thread_bound=*/false);
  SequenceCheckerImpl sequence_checker;
  EXPECT_TRUE(sequence_checker.CalledOnValidSequence());
}

TEST(SequenceCheckerTest, TaskScopeThreadBound) {
  TaskScope task_scope(SequenceToken::Create(),
                       /* is_thread_bound=*/true);
  SequenceCheckerImpl sequence_checker;
  EXPECT_TRUE(sequence_checker.CalledOnValidSequence());
}

TEST(SequenceCheckerTest, DifferentThreadNoTaskScope) {
  SequenceCheckerImpl sequence_checker;
  RunCallbackThread thread(
      BindOnce(&ExpectNotCalledOnValidSequence, Unretained(&sequence_checker)));
}

TEST(SequenceCheckerTest, DifferentThreadDifferentSequenceToken) {
  SequenceCheckerImpl sequence_checker;
  RunCallbackThread thread(BindLambdaForTesting([&] {
    TaskScope task_scope(SequenceToken::Create(),
                         /* is_thread_bound=*/false);
    ExpectNotCalledOnValidSequence(&sequence_checker);
  }));
}

TEST(SequenceCheckerTest, DifferentThreadDifferentSequenceTokenThreadBound) {
  SequenceCheckerImpl sequence_checker;
  RunCallbackThread thread(BindLambdaForTesting([&] {
    TaskScope task_scope(SequenceToken::Create(),
                         /* is_thread_bound=*/true);
    ExpectNotCalledOnValidSequence(&sequence_checker);
  }));
}

TEST(SequenceCheckerTest, DifferentThreadSameSequenceToken) {
  const SequenceToken token = SequenceToken::Create();
  TaskScope task_scope(token, /* is_thread_bound=*/false);
  SequenceCheckerImpl sequence_checker;
  RunCallbackThread thread(BindLambdaForTesting([&] {
    TaskScope task_scope(token, /* is_thread_bound=*/false);
    ExpectCalledOnValidSequence(&sequence_checker);
  }));
}

TEST(SequenceCheckerTest, DifferentThreadSameSequenceTokenThreadBound) {
  // Note: A callback running synchronously in `RunOrPostTask()` may have a
  // non-thread-bound `TaskScope` associated with the same `SequenceToken` as
  // another thread-bound `TaskScope`. This test recreates this case.
  const SequenceToken token = SequenceToken::Create();
  TaskScope task_scope(token, /* is_thread_bound=*/true);
  SequenceCheckerImpl sequence_checker;
  RunCallbackThread thread(BindLambdaForTesting([&] {
    TaskScope task_scope(token, /* is_thread_bound=*/false);
    ExpectCalledOnValidSequence(&sequence_checker);
  }));
}

TEST(SequenceCheckerTest, SameThreadDifferentSequenceToken) {
  std::unique_ptr<SequenceCheckerImpl> sequence_checker;

  {
    TaskScope task_scope(SequenceToken::Create(),
                         /* is_thread_bound=*/false);
    sequence_checker = std::make_unique<SequenceCheckerImpl>();
  }

  {
    // Different SequenceToken.
    TaskScope task_scope(SequenceToken::Create(),
                         /* is_thread_bound=*/false);
    EXPECT_FALSE(sequence_checker->CalledOnValidSequence());
  }

  // No explicit SequenceToken.
  EXPECT_FALSE(sequence_checker->CalledOnValidSequence());
}

TEST(SequenceCheckerTest, DetachFromSequence) {
  std::unique_ptr<SequenceCheckerImpl> sequence_checker;

  {
    TaskScope task_scope(SequenceToken::Create(),
                         /* is_thread_bound=*/false);
    sequence_checker = std::make_unique<SequenceCheckerImpl>();
  }

  sequence_checker->DetachFromSequence();

  {
    // Verify that CalledOnValidSequence() returns true when called with
    // a different sequence token after a call to DetachFromSequence().
    TaskScope task_scope(SequenceToken::Create(),
                         /* is_thread_bound=*/false);
    EXPECT_TRUE(sequence_checker->CalledOnValidSequence());
  }
}

TEST(SequenceCheckerTest, DetachFromSequenceNoSequenceToken) {
  SequenceCheckerImpl sequence_checker;
  sequence_checker.DetachFromSequence();

  // Verify that CalledOnValidSequence() returns true when called on a
  // different thread after a call to DetachFromSequence().
  RunCallbackThread thread(
      BindOnce(&ExpectCalledOnValidSequence, Unretained(&sequence_checker)));

  EXPECT_FALSE(sequence_checker.CalledOnValidSequence());
}

TEST(SequenceCheckerTest, Move) {
  SequenceCheckerImpl initial;
  EXPECT_TRUE(initial.CalledOnValidSequence());

  SequenceCheckerImpl move_constructed(std::move(initial));
  EXPECT_TRUE(move_constructed.CalledOnValidSequence());

  SequenceCheckerImpl move_assigned;
  move_assigned = std::move(move_constructed);

  // The two SequenceCheckerImpls moved from should be able to rebind to another
  // sequence.
  RunCallbackThread thread1(
      BindOnce(&ExpectCalledOnValidSequence, Unretained(&initial)));
  RunCallbackThread thread2(
      BindOnce(&ExpectCalledOnValidSequence, Unretained(&move_constructed)));

  // But the latest one shouldn't be able to run on another sequence.
  RunCallbackThread thread(
      BindOnce(&ExpectNotCalledOnValidSequence, Unretained(&move_assigned)));

  EXPECT_TRUE(move_assigned.CalledOnValidSequence());
}

TEST(SequenceCheckerTest, MoveAssignIntoDetached) {
  SequenceCheckerImpl initial;

  SequenceCheckerImpl move_assigned;
  move_assigned.DetachFromSequence();
  move_assigned = std::move(initial);

  // |initial| is detached after move.
  RunCallbackThread thread1(
      BindOnce(&ExpectCalledOnValidSequence, Unretained(&initial)));

  // |move_assigned| should be associated with the main thread.
  RunCallbackThread thread2(
      BindOnce(&ExpectNotCalledOnValidSequence, Unretained(&move_assigned)));

  EXPECT_TRUE(move_assigned.CalledOnValidSequence());
}

TEST(SequenceCheckerTest, MoveFromDetachedRebinds) {
  SequenceCheckerImpl initial;
  initial.DetachFromSequence();

  SequenceCheckerImpl moved_into(std::move(initial));

  // |initial| is still detached after move.
  RunCallbackThread thread1(
      BindOnce(&ExpectCalledOnValidSequence, Unretained(&initial)));

  // |moved_into| is bound to the current sequence as part of the move.
  RunCallbackThread thread2(
      BindOnce(&ExpectNotCalledOnValidSequence, Unretained(&moved_into)));
  EXPECT_TRUE(moved_into.CalledOnValidSequence());
}

TEST(SequenceCheckerTest, MoveOffSequenceBanned) {
  GTEST_FLAG_SET(death_test_style, "threadsafe");

  SequenceCheckerImpl other_sequence;
  other_sequence.DetachFromSequence();
  RunCallbackThread thread(
      BindOnce(&ExpectCalledOnValidSequence, Unretained(&other_sequence)));

  EXPECT_DCHECK_DEATH(
      SequenceCheckerImpl main_sequence(std::move(other_sequence)));
}

TEST(SequenceCheckerMacroTest, Macros) {
  auto scope = std::make_unique<TaskScope>(SequenceToken::Create(),
                                           /* is_thread_bound=*/false);
  SEQUENCE_CHECKER(my_sequence_checker);

  {
    // Don't expect a DCHECK death when a SequenceChecker is used on the right
    // sequence.
    DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker);
  }
  scope.reset();

#if DCHECK_IS_ON()
  // Expect DCHECK death when used on a different sequence.
  EXPECT_DCHECK_DEATH(
      { DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker); });
#else
    // Happily no-ops on non-dcheck builds.
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker);
#endif

  DETACH_FROM_SEQUENCE(my_sequence_checker);

  // Don't expect a DCHECK death when a SequenceChecker is used for the first
  // time after having been detached.
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker);
}

// Owns a SequenceCheckerImpl, and asserts that CalledOnValidSequence() is valid
// in ~SequenceCheckerOwner.
class SequenceCheckerOwner {
 public:
  explicit SequenceCheckerOwner(SequenceCheckerImpl* other_checker)
      : other_checker_(other_checker) {}
  SequenceCheckerOwner(const SequenceCheckerOwner&) = delete;
  SequenceCheckerOwner& operator=(const SequenceCheckerOwner&) = delete;
  ~SequenceCheckerOwner() {
    // Check passes on TLS destruction.
    EXPECT_TRUE(checker_.CalledOnValidSequence());

    // Check also passes on TLS destruction after move assignment.
    *other_checker_ = std::move(checker_);
    EXPECT_TRUE(other_checker_->CalledOnValidSequence());
  }

 private:
  SequenceCheckerImpl checker_;
  raw_ptr<SequenceCheckerImpl> other_checker_;
};

// Verifies SequenceCheckerImpl::CalledOnValidSequence() returns true if called
// during thread destruction.
TEST(SequenceCheckerTest, FromThreadDestruction) {
  SequenceChecker::EnableStackLogging();

  SequenceCheckerImpl other_checker;
  ThreadLocalOwnedPointer<SequenceCheckerOwner> thread_local_owner;
  {
    test::TaskEnvironment task_environment;
    auto task_runner = ThreadPool::CreateSequencedTaskRunner({});
    task_runner->PostTask(
        FROM_HERE, BindLambdaForTesting([&] {
          thread_local_owner.Set(
              std::make_unique<SequenceCheckerOwner>(&other_checker));
        }));
    task_runner = nullptr;
    task_environment.RunUntilIdle();
  }
}

// Verifies sequence checking while holding the same locks from different
// sequences.
//
// Note: This is only supported in DCHECK builds.
#if DCHECK_IS_ON()
TEST(SequenceCheckerTest, LockBasic) {
  test::TaskEnvironment task_environment;
  WaitableEvent thread_pool_done;
  Lock lock;

  // Create sequence checker while holding lock.
  ReleasableAutoLock releasable_auto_lock(&lock,
                                          subtle::LockTracking::kEnabled);
  SequenceCheckerImpl sequence_checker;
  releasable_auto_lock.Release();

  ThreadPool::PostTask(BindLambdaForTesting([&] {
    // Check sequencing while holding the lock.
    {
      AutoLock auto_lock(lock, subtle::LockTracking::kEnabled);
      EXPECT_TRUE(sequence_checker.CalledOnValidSequence());
    }

    thread_pool_done.Signal();
  }));

  thread_pool_done.Wait();

  // Check sequencing from the creation sequence, without holding the lock.
  // Sequencing is *not* valid because sequencing now depends on the lock.
  EXPECT_FALSE(sequence_checker.CalledOnValidSequence());

  // Check sequencing from the creation sequence while holding the lock.
  {
    AutoLock auto_lock(lock, subtle::LockTracking::kEnabled);
    EXPECT_TRUE(sequence_checker.CalledOnValidSequence());
  }
}

TEST(SequenceCheckerTest, ManyLocks) {
  test::TaskEnvironment task_environment;
  WaitableEvent thread_pool_done;
  Lock lock_a;
  Lock lock_b;
  Lock lock_c;

  ReleasableAutoLock releasable_auto_lock_a(&lock_a,
                                            subtle::LockTracking::kEnabled);
  ReleasableAutoLock releasable_auto_lock_b(&lock_b,
                                            subtle::LockTracking::kEnabled);
  ReleasableAutoLock releasable_auto_lock_c(&lock_c,
                                            subtle::LockTracking::kEnabled);
  SequenceCheckerImpl sequence_checker;
  releasable_auto_lock_c.Release();
  releasable_auto_lock_b.Release();
  releasable_auto_lock_a.Release();

  ThreadPool::PostTask(BindLambdaForTesting([&] {
    {
      AutoLock auto_lock_a(lock_a, subtle::LockTracking::kEnabled);
      AutoLock auto_lock_b(lock_b, subtle::LockTracking::kEnabled);
      AutoLock auto_lock_c(lock_c, subtle::LockTracking::kEnabled);
      EXPECT_TRUE(sequence_checker.CalledOnValidSequence());
    }

    {
      AutoLock auto_lock_a(lock_a, subtle::LockTracking::kEnabled);
      AutoLock auto_lock_b(lock_b, subtle::LockTracking::kEnabled);
      EXPECT_TRUE(sequence_checker.CalledOnValidSequence());
    }

    {
      AutoLock auto_lock_c(lock_c, subtle::LockTracking::kEnabled);
      EXPECT_FALSE(sequence_checker.CalledOnValidSequence());
    }

    {
      AutoLock auto_lock_b(lock_b, subtle::LockTracking::kEnabled);
      EXPECT_TRUE(sequence_checker.CalledOnValidSequence());
    }

    {
      AutoLock auto_lock_b(lock_a, subtle::LockTracking::kEnabled);
      EXPECT_FALSE(sequence_checker.CalledOnValidSequence());
    }

    thread_pool_done.Signal();
  }));

  thread_pool_done.Wait();

  EXPECT_FALSE(sequence_checker.CalledOnValidSequence());
}

TEST(SequenceCheckerTest, LockAndSequence) {
  test::TaskEnvironment task_environment;
  WaitableEvent thread_pool_done;
  Lock lock;

  // Create sequence checker while holding lock.
  ReleasableAutoLock releasable_auto_lock(&lock,
                                          subtle::LockTracking::kEnabled);
  SequenceCheckerImpl sequence_checker;
  releasable_auto_lock.Release();

  // Check sequencing without holding the lock.
  EXPECT_TRUE(sequence_checker.CalledOnValidSequence());

  ThreadPool::PostTask(BindLambdaForTesting([&] {
    // Check sequencing while holding the lock. This is not valid because
    // `CalledOnValidSequence()` previously returned true while the lock wasn't
    // held.
    {
      AutoLock auto_lock(lock, subtle::LockTracking::kEnabled);
      EXPECT_FALSE(sequence_checker.CalledOnValidSequence());
    }

    thread_pool_done.Signal();
  }));

  thread_pool_done.Wait();
}

TEST(SequenceCheckerTest, LockDetachFromSequence) {
  test::TaskEnvironment task_environment;
  WaitableEvent thread_pool_done;
  Lock lock;

  // Create sequence checker and detach while holding lock.
  ReleasableAutoLock releasable_auto_lock(&lock,
                                          subtle::LockTracking::kEnabled);
  SequenceCheckerImpl sequence_checker;
  sequence_checker.DetachFromSequence();
  releasable_auto_lock.Release();

  // Re-bind without holding the lock.
  EXPECT_TRUE(sequence_checker.CalledOnValidSequence());

  ThreadPool::PostTask(BindLambdaForTesting([&] {
    // Check sequencing while holding the lock. This is not valid because the
    // sequence checker was detached and re-bound without the lock.
    {
      AutoLock auto_lock(lock, subtle::LockTracking::kEnabled);
      EXPECT_FALSE(sequence_checker.CalledOnValidSequence());
    }

    thread_pool_done.Signal();
  }));

  thread_pool_done.Wait();
}

TEST(SequenceCheckerTest, LockMoveConstruction) {
  test::TaskEnvironment task_environment;
  WaitableEvent thread_pool_done;
  Lock lock;

  // Create sequence checker and move-construct while holding a lock.
  ReleasableAutoLock releasable_auto_lock(&lock,
                                          subtle::LockTracking::kEnabled);
  SequenceCheckerImpl sequence_checker;
  SequenceCheckerImpl other_sequence_checker(std::move(sequence_checker));
  releasable_auto_lock.Release();

  ThreadPool::PostTask(BindLambdaForTesting([&] {
    // Check sequencing while holding the lock.
    {
      AutoLock auto_lock(lock, subtle::LockTracking::kEnabled);
      EXPECT_TRUE(other_sequence_checker.CalledOnValidSequence());
    }

    thread_pool_done.Signal();
  }));

  thread_pool_done.Wait();
}

TEST(SequenceCheckerTest, LockMoveAssignment) {
  test::TaskEnvironment task_environment;
  WaitableEvent thread_pool_done;
  Lock lock;

  SequenceCheckerImpl other_sequence_checker;

  // Create sequence checker and move-assign it to `other_sequence_checker`
  // while holding a lock.
  ReleasableAutoLock releasable_auto_lock(&lock,
                                          subtle::LockTracking::kEnabled);
  SequenceCheckerImpl sequence_checker;
  other_sequence_checker = std::move(sequence_checker);
  releasable_auto_lock.Release();

  ThreadPool::PostTask(BindLambdaForTesting([&] {
    // Check sequencing while holding the lock.
    {
      AutoLock auto_lock(lock, subtle::LockTracking::kEnabled);
      EXPECT_TRUE(other_sequence_checker.CalledOnValidSequence());
    }

    thread_pool_done.Signal();
  }));

  thread_pool_done.Wait();
}
#endif  // DCHECK_IS_ON()

}  // namespace base::internal
