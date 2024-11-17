// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/thread_checker.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_token.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/simple_thread.h"
#include "base/threading/thread_local.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base::internal {
namespace {

// A thread that runs a callback.
class RunCallbackThread : public SimpleThread {
 public:
  explicit RunCallbackThread(OnceClosure callback)
      : SimpleThread("RunCallbackThread"), callback_(std::move(callback)) {}

  RunCallbackThread(const RunCallbackThread&) = delete;
  RunCallbackThread& operator=(const RunCallbackThread&) = delete;

 private:
  // SimpleThread:
  void Run() override { std::move(callback_).Run(); }

  OnceClosure callback_;
};

// Runs a callback on a new thread synchronously.
void RunCallbackOnNewThreadSynchronously(OnceClosure callback) {
  RunCallbackThread run_callback_thread(std::move(callback));
  run_callback_thread.Start();
  run_callback_thread.Join();
}

void ExpectCalledOnValidThread(ThreadCheckerImpl* thread_checker) {
  ASSERT_TRUE(thread_checker);

  // This should bind |thread_checker| to the current thread if it wasn't
  // already bound to a thread.
  EXPECT_TRUE(thread_checker->CalledOnValidThread());

  // Since |thread_checker| is now bound to the current thread, another call to
  // CalledOnValidThread() should return true.
  EXPECT_TRUE(thread_checker->CalledOnValidThread());
}

void ExpectNotCalledOnValidThread(ThreadCheckerImpl* thread_checker) {
  ASSERT_TRUE(thread_checker);
  EXPECT_FALSE(thread_checker->CalledOnValidThread());
}

void ExpectNotCalledOnValidThreadWithSequenceTokenThreadBound(
    ThreadCheckerImpl* thread_checker,
    SequenceToken sequence_token) {
  TaskScope task_scope(sequence_token, /* is_thread_bound=*/true);
  ExpectNotCalledOnValidThread(thread_checker);
}

}  // namespace

TEST(ThreadCheckerTest, AllowedSameThreadNoSequenceToken) {
  ThreadCheckerImpl thread_checker;
  EXPECT_TRUE(thread_checker.CalledOnValidThread());
}

TEST(ThreadCheckerTest, AllowedSameThreadAndSequenceDifferentThreadBoundTasks) {
  std::unique_ptr<ThreadCheckerImpl> thread_checker;
  const SequenceToken sequence_token = SequenceToken::Create();

  {
    TaskScope task_scope(sequence_token,
                         /* is_thread_bound=*/true);
    thread_checker = std::make_unique<ThreadCheckerImpl>();
  }

  {
    TaskScope task_scope(sequence_token,
                         /* is_thread_bound=*/true);
    EXPECT_TRUE(thread_checker->CalledOnValidThread());
  }
}

TEST(ThreadCheckerTest, AllowedSameThreadSequenceAndTaskNotThreadBound) {
  TaskScope task_scope(SequenceToken::Create(),
                       /* is_thread_bound=*/false);
  ThreadCheckerImpl thread_checker;
  EXPECT_TRUE(thread_checker.CalledOnValidThread());
}

TEST(ThreadCheckerTest,
     DisallowedSameThreadAndSequenceDifferentTasksNotThreadBound) {
  std::unique_ptr<ThreadCheckerImpl> thread_checker;

  {
    TaskScope task_scope(SequenceToken::Create(),
                         /* is_thread_bound=*/false);
    thread_checker = std::make_unique<ThreadCheckerImpl>();
  }

  {
    TaskScope task_scope(SequenceToken::Create(),
                         /* is_thread_bound=*/false);
    EXPECT_FALSE(thread_checker->CalledOnValidThread());
  }
}

TEST(ThreadCheckerTest, DisallowedDifferentThreadsNoSequenceToken) {
  ThreadCheckerImpl thread_checker;
  RunCallbackOnNewThreadSynchronously(
      BindOnce(&ExpectNotCalledOnValidThread, Unretained(&thread_checker)));
}

TEST(ThreadCheckerTest, DisallowedDifferentThreadsSameSequence) {
  SingleThreadTaskRunner::CurrentDefaultHandle
      single_thread_task_runner_current_default_handle(
          MakeRefCounted<TestSimpleTaskRunner>());
  const SequenceToken sequence_token(SequenceToken::Create());

  TaskScope task_scope(sequence_token,
                       /* is_thread_bound=*/false);
  ThreadCheckerImpl thread_checker;
  EXPECT_TRUE(thread_checker.CalledOnValidThread());

  RunCallbackOnNewThreadSynchronously(
      BindOnce(&ExpectNotCalledOnValidThreadWithSequenceTokenThreadBound,
               Unretained(&thread_checker), sequence_token));
}

TEST(ThreadCheckerTest, DisallowedSameThreadDifferentSequence) {
  std::unique_ptr<ThreadCheckerImpl> thread_checker;

  SingleThreadTaskRunner::CurrentDefaultHandle
      single_thread_task_runner_current_default_handle(
          MakeRefCounted<TestSimpleTaskRunner>());

  {
    TaskScope task_scope(SequenceToken::Create(),
                         /* is_thread_bound=*/false);
    thread_checker = std::make_unique<ThreadCheckerImpl>();
  }

  {
    // Different SequenceToken.
    TaskScope task_scope(SequenceToken::Create(),
                         /* is_thread_bound=*/false);
    EXPECT_FALSE(thread_checker->CalledOnValidThread());
  }

  // No SequenceToken.
  EXPECT_FALSE(thread_checker->CalledOnValidThread());
}

TEST(ThreadCheckerTest, DetachFromThread) {
  ThreadCheckerImpl thread_checker;
  thread_checker.DetachFromThread();

  // Verify that CalledOnValidThread() returns true when called on a different
  // thread after a call to DetachFromThread().
  RunCallbackOnNewThreadSynchronously(
      BindOnce(&ExpectCalledOnValidThread, Unretained(&thread_checker)));

  EXPECT_FALSE(thread_checker.CalledOnValidThread());
}

TEST(ThreadCheckerTest, DetachFromThreadWithSequenceToken) {
  SingleThreadTaskRunner::CurrentDefaultHandle
      single_thread_task_runner_current_default_handle(
          MakeRefCounted<TestSimpleTaskRunner>());
  TaskScope task_scope(SequenceToken::Create(),
                       /* is_thread_bound=*/false);
  ThreadCheckerImpl thread_checker;
  thread_checker.DetachFromThread();

  // Verify that CalledOnValidThread() returns true when called on a different
  // thread after a call to DetachFromThread().
  RunCallbackOnNewThreadSynchronously(
      BindOnce(&ExpectCalledOnValidThread, Unretained(&thread_checker)));

  EXPECT_FALSE(thread_checker.CalledOnValidThread());
}

// Owns a ThreadCheckerImpl and asserts that CalledOnValidThread() is valid
// in ~ThreadCheckerOwner.
class ThreadCheckerOwner {
 public:
  explicit ThreadCheckerOwner(bool detach_from_thread) {
    if (detach_from_thread)
      checker_.DetachFromThread();
  }

  ThreadCheckerOwner(const ThreadCheckerOwner&) = delete;
  ThreadCheckerOwner& operator=(const ThreadCheckerOwner&) = delete;

  ~ThreadCheckerOwner() { EXPECT_TRUE(checker_.CalledOnValidThread()); }

 private:
  ThreadCheckerImpl checker_;
};

// Verifies ThreadCheckerImpl::CalledOnValidThread() returns true if called
// during thread destruction.
TEST(ThreadCheckerTest, CalledOnValidThreadFromThreadDestruction) {
  ThreadLocalOwnedPointer<ThreadCheckerOwner> thread_local_owner;
  RunCallbackOnNewThreadSynchronously(BindLambdaForTesting([&] {
    thread_local_owner.Set(std::make_unique<ThreadCheckerOwner>(false));
  }));
}

// Variant of CalledOnValidThreadFromThreadDestruction that calls
// ThreadCheckerImpl::DetachFromThread().
TEST(ThreadCheckerTest, CalledOnValidThreadFromThreadDestructionDetached) {
  ThreadLocalOwnedPointer<ThreadCheckerOwner> thread_local_owner;
  RunCallbackOnNewThreadSynchronously(BindLambdaForTesting([&] {
    thread_local_owner.Set(std::make_unique<ThreadCheckerOwner>(true));
  }));
}

TEST(ThreadCheckerTest, Move) {
  ThreadCheckerImpl initial;
  EXPECT_TRUE(initial.CalledOnValidThread());

  ThreadCheckerImpl move_constructed(std::move(initial));
  EXPECT_TRUE(move_constructed.CalledOnValidThread());

  ThreadCheckerImpl move_assigned;
  move_assigned = std::move(move_constructed);
  EXPECT_TRUE(move_assigned.CalledOnValidThread());

  // The two ThreadCheckerImpls moved from should be able to rebind to another
  // thread.
  RunCallbackOnNewThreadSynchronously(
      BindOnce(&ExpectCalledOnValidThread, Unretained(&initial)));
  RunCallbackOnNewThreadSynchronously(
      BindOnce(&ExpectCalledOnValidThread, Unretained(&move_constructed)));

  // But the latest one shouldn't be able to run on another thread.
  RunCallbackOnNewThreadSynchronously(
      BindOnce(&ExpectNotCalledOnValidThread, Unretained(&move_assigned)));

  EXPECT_TRUE(move_assigned.CalledOnValidThread());
}

TEST(ThreadCheckerTest, MoveAssignIntoDetached) {
  ThreadCheckerImpl initial;

  ThreadCheckerImpl move_assigned;
  move_assigned.DetachFromThread();
  move_assigned = std::move(initial);

  // |initial| is detached after move.
  RunCallbackOnNewThreadSynchronously(
      BindOnce(&ExpectCalledOnValidThread, Unretained(&initial)));

  // |move_assigned| should be associated with the main thread.
  RunCallbackOnNewThreadSynchronously(
      BindOnce(&ExpectNotCalledOnValidThread, Unretained(&move_assigned)));

  EXPECT_TRUE(move_assigned.CalledOnValidThread());
}

TEST(ThreadCheckerTest, MoveFromDetachedRebinds) {
  ThreadCheckerImpl initial;
  initial.DetachFromThread();

  ThreadCheckerImpl moved_into(std::move(initial));

  // |initial| is still detached after move.
  RunCallbackOnNewThreadSynchronously(
      BindOnce(&ExpectCalledOnValidThread, Unretained(&initial)));

  // |moved_into| is bound to the current thread as part of the move.
  RunCallbackOnNewThreadSynchronously(
      BindOnce(&ExpectNotCalledOnValidThread, Unretained(&moved_into)));
  EXPECT_TRUE(moved_into.CalledOnValidThread());
}

TEST(ThreadCheckerTest, MoveOffThreadBanned) {
  GTEST_FLAG_SET(death_test_style, "threadsafe");

  ThreadCheckerImpl other_thread;
  other_thread.DetachFromThread();
  RunCallbackOnNewThreadSynchronously(
      BindOnce(&ExpectCalledOnValidThread, Unretained(&other_thread)));

  EXPECT_DCHECK_DEATH(ThreadCheckerImpl main_thread(std::move(other_thread)));
}

namespace {

// This fixture is a helper for unit testing the thread checker macros as it is
// not possible to inline ExpectDeathOnOtherThread() and
// ExpectNoDeathOnOtherThreadAfterDetach() as lambdas since binding
// |Unretained(&my_sequence_checker)| wouldn't compile on non-dcheck builds
// where it won't be defined.
class ThreadCheckerMacroTest : public testing::Test {
 public:
  ThreadCheckerMacroTest() = default;

  ThreadCheckerMacroTest(const ThreadCheckerMacroTest&) = delete;
  ThreadCheckerMacroTest& operator=(const ThreadCheckerMacroTest&) = delete;

  void ExpectDeathOnOtherThread() {
#if DCHECK_IS_ON()
    EXPECT_DCHECK_DEATH({ DCHECK_CALLED_ON_VALID_THREAD(thread_checker_); });
#else
    // Happily no-ops on non-dcheck builds.
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
#endif
  }

  void ExpectNoDeathOnOtherThreadAfterDetach() {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  }

 protected:
  THREAD_CHECKER(thread_checker_);
};

}  // namespace

TEST_F(ThreadCheckerMacroTest, Macros) {
  GTEST_FLAG_SET(death_test_style, "threadsafe");

  THREAD_CHECKER(my_thread_checker);

  RunCallbackOnNewThreadSynchronously(BindOnce(
      &ThreadCheckerMacroTest::ExpectDeathOnOtherThread, Unretained(this)));

  DETACH_FROM_THREAD(thread_checker_);

  RunCallbackOnNewThreadSynchronously(
      BindOnce(&ThreadCheckerMacroTest::ExpectNoDeathOnOtherThreadAfterDetach,
               Unretained(this)));
}

}  // namespace base::internal
