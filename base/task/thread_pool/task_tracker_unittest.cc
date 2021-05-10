// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/thread_pool/task_tracker.h"

#include <stdint.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/check_op.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_samples.h"
#include "base/sequence_token.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/atomic_flag.h"
#include "base/task/common/checked_lock.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool/task.h"
#include "base/task/thread_pool/test_utils.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_simple_task_runner.h"
#include "base/test/test_timeouts.h"
#include "base/test/test_waitable_event.h"
#include "base/threading/platform_thread.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/simple_thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace internal {

namespace {

constexpr size_t kLoadTestNumIterations = 75;

// Invokes a closure asynchronously.
class CallbackThread : public SimpleThread {
 public:
  explicit CallbackThread(OnceClosure closure)
      : SimpleThread("CallbackThread"), closure_(std::move(closure)) {}
  CallbackThread(const CallbackThread&) = delete;
  CallbackThread& operator=(const CallbackThread&) = delete;

  // Returns true once the callback returns.
  bool has_returned() { return has_returned_.IsSet(); }

 private:
  void Run() override {
    std::move(closure_).Run();
    has_returned_.Set();
  }

  OnceClosure closure_;
  AtomicFlag has_returned_;
};

class ThreadPostingAndRunningTask : public SimpleThread {
 public:
  enum class Action {
    WILL_POST,
    RUN,
    WILL_POST_AND_RUN,
  };

  // |action| must be either WILL_POST of WILL_POST_AND_RUN.
  // |task| will be pushed to |sequence| and |sequence| will be registered. If
  // |action| is WILL_POST_AND_RUN, a task from |sequence| will run.
  ThreadPostingAndRunningTask(TaskTracker* tracker,
                              scoped_refptr<Sequence> sequence,
                              Action action,
                              bool expect_post_succeeds,
                              Task task)
      : SimpleThread("ThreadPostingAndRunningTask"),
        tracker_(tracker),
        task_(std::move(task)),
        sequence_(std::move(sequence)),
        action_(action),
        expect_post_succeeds_(expect_post_succeeds) {
    EXPECT_TRUE(task_.task);
    EXPECT_TRUE(sequence_);
    EXPECT_NE(Action::RUN, action_);
  }

  // A task from |task_source| will run.
  ThreadPostingAndRunningTask(TaskTracker* tracker,
                              RegisteredTaskSource task_source)
      : SimpleThread("ThreadPostingAndRunningTask"),
        tracker_(tracker),
        task_source_(std::move(task_source)),
        action_(Action::RUN),
        expect_post_succeeds_(false) {
    EXPECT_TRUE(task_source_);
  }
  ThreadPostingAndRunningTask(const ThreadPostingAndRunningTask&) = delete;
  ThreadPostingAndRunningTask& operator=(const ThreadPostingAndRunningTask&) =
      delete;

  RegisteredTaskSource TakeTaskSource() { return std::move(task_source_); }

 private:
  void Run() override {
    bool post_and_queue_succeeded = true;
    if (action_ == Action::WILL_POST || action_ == Action::WILL_POST_AND_RUN) {
      EXPECT_TRUE(task_.task);

      post_and_queue_succeeded =
          tracker_->WillPostTask(&task_, sequence_->shutdown_behavior());
      sequence_->BeginTransaction().PushTask(std::move(task_));
      task_source_ = tracker_->RegisterTaskSource(std::move(sequence_));

      post_and_queue_succeeded &= !!task_source_;

      EXPECT_EQ(expect_post_succeeds_, post_and_queue_succeeded);
    }
    if (post_and_queue_succeeded &&
        (action_ == Action::RUN || action_ == Action::WILL_POST_AND_RUN)) {
      EXPECT_TRUE(task_source_);
      task_source_.WillRunTask();

      // Expect RunAndPopNextTask to return nullptr since |sequence| is empty
      // after popping a task from it.
      EXPECT_FALSE(tracker_->RunAndPopNextTask(std::move(task_source_)));
    }
  }

  TaskTracker* const tracker_;
  Task task_;
  scoped_refptr<Sequence> sequence_;
  RegisteredTaskSource task_source_;
  const Action action_;
  const bool expect_post_succeeds_;
};

class ScopedSetSingletonAllowed {
 public:
  explicit ScopedSetSingletonAllowed(bool singleton_allowed)
      : previous_value_(
            ThreadRestrictions::SetSingletonAllowed(singleton_allowed)) {}
  ~ScopedSetSingletonAllowed() {
    ThreadRestrictions::SetSingletonAllowed(previous_value_);
  }

 private:
  const bool previous_value_;
};

class ThreadPoolTaskTrackerTest
    : public testing::TestWithParam<TaskShutdownBehavior> {
 public:
  ThreadPoolTaskTrackerTest(const ThreadPoolTaskTrackerTest&) = delete;
  ThreadPoolTaskTrackerTest& operator=(const ThreadPoolTaskTrackerTest&) =
      delete;

 protected:
  ThreadPoolTaskTrackerTest() = default;

  // Creates a task.
  Task CreateTask() {
    return Task(
        FROM_HERE,
        BindOnce(&ThreadPoolTaskTrackerTest::RunTaskCallback, Unretained(this)),
        TimeTicks::Now(), TimeDelta());
  }

  RegisteredTaskSource WillPostTaskAndQueueTaskSource(
      Task task,
      const TaskTraits& traits) {
    if (!tracker_.WillPostTask(&task, traits.shutdown_behavior()))
      return nullptr;
    auto sequence = test::CreateSequenceWithTask(std::move(task), traits);
    return tracker_.RegisterTaskSource(std::move(sequence));
  }
  RegisteredTaskSource RunAndPopNextTask(RegisteredTaskSource task_source) {
    task_source.WillRunTask();
    return tracker_.RunAndPopNextTask(std::move(task_source));
  }

  // Calls tracker_->CompleteShutdown() on a new thread and expects it to block.
  void ExpectAsyncCompleteShutdownBlocks() {
    ASSERT_FALSE(thread_calling_shutdown_);
    ASSERT_TRUE(tracker_.HasShutdownStarted());
    thread_calling_shutdown_ = std::make_unique<CallbackThread>(
        BindOnce(&TaskTracker::CompleteShutdown, Unretained(&tracker_)));
    thread_calling_shutdown_->Start();
    PlatformThread::Sleep(TestTimeouts::tiny_timeout());
    VerifyAsyncShutdownInProgress();
  }

  void WaitForAsyncIsShutdownComplete() {
    ASSERT_TRUE(thread_calling_shutdown_);
    thread_calling_shutdown_->Join();
    EXPECT_TRUE(thread_calling_shutdown_->has_returned());
    EXPECT_TRUE(tracker_.IsShutdownComplete());
  }

  void VerifyAsyncShutdownInProgress() {
    ASSERT_TRUE(thread_calling_shutdown_);
    EXPECT_FALSE(thread_calling_shutdown_->has_returned());
    EXPECT_TRUE(tracker_.HasShutdownStarted());
    EXPECT_FALSE(tracker_.IsShutdownComplete());
  }

  // Calls tracker_->FlushForTesting() on a new thread.
  void CallFlushFromAnotherThread() {
    ASSERT_FALSE(thread_calling_flush_);
    thread_calling_flush_ = std::make_unique<CallbackThread>(
        BindOnce(&TaskTracker::FlushForTesting, Unretained(&tracker_)));
    thread_calling_flush_->Start();
  }

  void WaitForAsyncFlushReturned() {
    ASSERT_TRUE(thread_calling_flush_);
    thread_calling_flush_->Join();
    EXPECT_TRUE(thread_calling_flush_->has_returned());
  }

  void VerifyAsyncFlushInProgress() {
    ASSERT_TRUE(thread_calling_flush_);
    EXPECT_FALSE(thread_calling_flush_->has_returned());
  }

  size_t NumTasksExecuted() {
    CheckedAutoLock auto_lock(lock_);
    return num_tasks_executed_;
  }

  TaskTracker tracker_;

 private:
  void RunTaskCallback() {
    CheckedAutoLock auto_lock(lock_);
    ++num_tasks_executed_;
  }

  std::unique_ptr<CallbackThread> thread_calling_shutdown_;
  std::unique_ptr<CallbackThread> thread_calling_flush_;

  // Synchronizes accesses to |num_tasks_executed_|.
  CheckedLock lock_;

  size_t num_tasks_executed_ = 0;
};

#define WAIT_FOR_ASYNC_SHUTDOWN_COMPLETED() \
  do {                                      \
    SCOPED_TRACE("");                       \
    WaitForAsyncIsShutdownComplete();       \
  } while (false)

#define VERIFY_ASYNC_SHUTDOWN_IN_PROGRESS() \
  do {                                      \
    SCOPED_TRACE("");                       \
    VerifyAsyncShutdownInProgress();        \
  } while (false)

#define WAIT_FOR_ASYNC_FLUSH_RETURNED() \
  do {                                  \
    SCOPED_TRACE("");                   \
    WaitForAsyncFlushReturned();        \
  } while (false)

#define VERIFY_ASYNC_FLUSH_IN_PROGRESS() \
  do {                                   \
    SCOPED_TRACE("");                    \
    VerifyAsyncFlushInProgress();        \
  } while (false)

}  // namespace

TEST_P(ThreadPoolTaskTrackerTest, WillPostAndRunBeforeShutdown) {
  Task task(CreateTask());

  // Inform |task_tracker_| that |task| will be posted.
  EXPECT_TRUE(tracker_.WillPostTask(&task, GetParam()));

  // Run the task.
  EXPECT_EQ(0U, NumTasksExecuted());

  test::QueueAndRunTaskSource(
      &tracker_, test::CreateSequenceWithTask(std::move(task), {GetParam()}));
  EXPECT_EQ(1U, NumTasksExecuted());

  // Shutdown() shouldn't block.
  test::ShutdownTaskTracker(&tracker_);
}

TEST_P(ThreadPoolTaskTrackerTest, WillPostAndRunLongTaskBeforeShutdown) {
  // Create a task that signals |task_running| and blocks until |task_barrier|
  // is signaled.
  TestWaitableEvent task_running;
  TestWaitableEvent task_barrier;
  Task blocked_task(FROM_HERE, BindLambdaForTesting([&]() {
                      task_running.Signal();
                      task_barrier.Wait();
                    }),
                    TimeTicks::Now(), TimeDelta());

  // Inform |task_tracker_| that |blocked_task| will be posted.
  auto sequence =
      WillPostTaskAndQueueTaskSource(std::move(blocked_task), {GetParam()});
  EXPECT_TRUE(sequence);

  // Create a thread to run the task. Wait until the task starts running.
  ThreadPostingAndRunningTask thread_running_task(&tracker_,
                                                  std::move(sequence));
  thread_running_task.Start();
  task_running.Wait();

  // Initiate shutdown after the task has started to run.
  tracker_.StartShutdown();

  if (GetParam() == TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN) {
    // Shutdown should complete even with a CONTINUE_ON_SHUTDOWN in progress.
    tracker_.CompleteShutdown();
  } else {
    // Shutdown should block with any non CONTINUE_ON_SHUTDOWN task in progress.
    ExpectAsyncCompleteShutdownBlocks();
  }

  // Unblock the task.
  task_barrier.Signal();
  thread_running_task.Join();

  // Shutdown should now complete for a non CONTINUE_ON_SHUTDOWN task.
  if (GetParam() != TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN)
    WAIT_FOR_ASYNC_SHUTDOWN_COMPLETED();
}

// Verify that an undelayed task whose sequence wasn't queued does not block
// shutdown, regardless of its shutdown behavior.
TEST_P(ThreadPoolTaskTrackerTest, WillPostBeforeShutdownQueueDuringShutdown) {
  // Simulate posting a undelayed task.
  Task task{CreateTask()};
  EXPECT_TRUE(tracker_.WillPostTask(&task, GetParam()));
  auto sequence = test::CreateSequenceWithTask(std::move(task), {GetParam()});

  // Inform |task_tracker_| that a BLOCK_SHUTDOWN task will be posted just to
  // block shutdown.
  auto block_shutdown_sequence = WillPostTaskAndQueueTaskSource(
      CreateTask(), {TaskShutdownBehavior::BLOCK_SHUTDOWN});
  EXPECT_TRUE(block_shutdown_sequence);

  // Start shutdown and try to complete it asynchronously.
  tracker_.StartShutdown();
  ExpectAsyncCompleteShutdownBlocks();

  const bool should_run = GetParam() == TaskShutdownBehavior::BLOCK_SHUTDOWN;
  if (should_run) {
    test::QueueAndRunTaskSource(&tracker_, std::move(sequence));
    EXPECT_EQ(1U, NumTasksExecuted());
    VERIFY_ASYNC_SHUTDOWN_IN_PROGRESS();
  } else {
    EXPECT_FALSE(tracker_.RegisterTaskSource(std::move(sequence)));
  }

  // Unblock shutdown by running the remaining BLOCK_SHUTDOWN task.
  RunAndPopNextTask(std::move(block_shutdown_sequence));
  EXPECT_EQ(should_run ? 2U : 1U, NumTasksExecuted());
  WAIT_FOR_ASYNC_SHUTDOWN_COMPLETED();
}

TEST_P(ThreadPoolTaskTrackerTest, WillPostBeforeShutdownRunDuringShutdown) {
  // Inform |task_tracker_| that a task will be posted.
  auto sequence = WillPostTaskAndQueueTaskSource(CreateTask(), {GetParam()});
  EXPECT_TRUE(sequence);

  // Inform |task_tracker_| that a BLOCK_SHUTDOWN task will be posted just to
  // block shutdown.
  auto block_shutdown_sequence = WillPostTaskAndQueueTaskSource(
      CreateTask(), {TaskShutdownBehavior::BLOCK_SHUTDOWN});
  EXPECT_TRUE(block_shutdown_sequence);

  // Start shutdown and try to complete it asynchronously.
  tracker_.StartShutdown();
  ExpectAsyncCompleteShutdownBlocks();

  // Try to run |task|. It should only run it it's BLOCK_SHUTDOWN. Otherwise it
  // should be discarded.
  EXPECT_EQ(0U, NumTasksExecuted());
  const bool should_run = GetParam() == TaskShutdownBehavior::BLOCK_SHUTDOWN;

  RunAndPopNextTask(std::move(sequence));
  EXPECT_EQ(should_run ? 1U : 0U, NumTasksExecuted());
  VERIFY_ASYNC_SHUTDOWN_IN_PROGRESS();

  // Unblock shutdown by running the remaining BLOCK_SHUTDOWN task.
  RunAndPopNextTask(std::move(block_shutdown_sequence));
  EXPECT_EQ(should_run ? 2U : 1U, NumTasksExecuted());
  WAIT_FOR_ASYNC_SHUTDOWN_COMPLETED();
}

TEST_P(ThreadPoolTaskTrackerTest, WillPostBeforeShutdownRunAfterShutdown) {
  // Inform |task_tracker_| that a task will be posted.
  auto sequence = WillPostTaskAndQueueTaskSource(CreateTask(), {GetParam()});
  EXPECT_TRUE(sequence);

  // Start shutdown.
  tracker_.StartShutdown();
  EXPECT_EQ(0U, NumTasksExecuted());

  if (GetParam() == TaskShutdownBehavior::BLOCK_SHUTDOWN) {
    // Verify that CompleteShutdown() blocks.
    ExpectAsyncCompleteShutdownBlocks();

    // Run the task to unblock shutdown.
    RunAndPopNextTask(std::move(sequence));
    EXPECT_EQ(1U, NumTasksExecuted());
    WAIT_FOR_ASYNC_SHUTDOWN_COMPLETED();

    // It is not possible to test running a BLOCK_SHUTDOWN task posted before
    // shutdown after shutdown because Shutdown() won't return if there are
    // pending BLOCK_SHUTDOWN tasks.
  } else {
    tracker_.CompleteShutdown();

    // The task shouldn't be allowed to run after shutdown.
    RunAndPopNextTask(std::move(sequence));
    EXPECT_EQ(0U, NumTasksExecuted());
  }
}

TEST_P(ThreadPoolTaskTrackerTest, WillPostAndRunDuringShutdown) {
  // Inform |task_tracker_| that a BLOCK_SHUTDOWN task will be posted just to
  // block shutdown.
  auto block_shutdown_sequence = WillPostTaskAndQueueTaskSource(
      CreateTask(), {TaskShutdownBehavior::BLOCK_SHUTDOWN});
  EXPECT_TRUE(block_shutdown_sequence);

  // Start shutdown.
  tracker_.StartShutdown();

  if (GetParam() == TaskShutdownBehavior::BLOCK_SHUTDOWN) {
    // Inform |task_tracker_| that a BLOCK_SHUTDOWN task will be posted.
    auto sequence = WillPostTaskAndQueueTaskSource(CreateTask(), {GetParam()});
    EXPECT_TRUE(sequence);

    // Run the BLOCK_SHUTDOWN task.
    EXPECT_EQ(0U, NumTasksExecuted());
    RunAndPopNextTask(std::move(sequence));
    EXPECT_EQ(1U, NumTasksExecuted());
  } else {
    // It shouldn't be allowed to post a non BLOCK_SHUTDOWN task.
    auto sequence = WillPostTaskAndQueueTaskSource(CreateTask(), {GetParam()});
    EXPECT_FALSE(sequence);

    // Don't try to run the task, because it wasn't allowed to be posted.
  }

  // Verify that CompleteShutdown() blocks.
  ExpectAsyncCompleteShutdownBlocks();

  // Unblock shutdown by running |block_shutdown_task|.
  RunAndPopNextTask(std::move(block_shutdown_sequence));
  EXPECT_EQ(GetParam() == TaskShutdownBehavior::BLOCK_SHUTDOWN ? 2U : 1U,
            NumTasksExecuted());
  WAIT_FOR_ASYNC_SHUTDOWN_COMPLETED();
}

TEST_P(ThreadPoolTaskTrackerTest, WillPostAfterShutdown) {
  test::ShutdownTaskTracker(&tracker_);

  Task task(CreateTask());

  // |task_tracker_| shouldn't allow a task to be posted after shutdown.
  if (GetParam() == TaskShutdownBehavior::BLOCK_SHUTDOWN) {
    EXPECT_DCHECK_DEATH(tracker_.WillPostTask(&task, GetParam()));
  } else {
    EXPECT_FALSE(tracker_.WillPostTask(&task, GetParam()));
  }
}

// Verify that BLOCK_SHUTDOWN and SKIP_ON_SHUTDOWN tasks can
// AssertSingletonAllowed() but CONTINUE_ON_SHUTDOWN tasks can't.
TEST_P(ThreadPoolTaskTrackerTest, SingletonAllowed) {
  const bool can_use_singletons =
      (GetParam() != TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN);

  Task task(FROM_HERE, BindOnce(&ThreadRestrictions::AssertSingletonAllowed),
            TimeTicks::Now(), TimeDelta());
  auto sequence = WillPostTaskAndQueueTaskSource(std::move(task), {GetParam()});
  EXPECT_TRUE(sequence);

  // Set the singleton allowed bit to the opposite of what it is expected to be
  // when |tracker| runs |task| to verify that |tracker| actually sets the
  // correct value.
  ScopedSetSingletonAllowed scoped_singleton_allowed(!can_use_singletons);

  // Running the task should fail iff the task isn't allowed to use singletons.
  if (can_use_singletons) {
    EXPECT_FALSE(RunAndPopNextTask(std::move(sequence)));
  } else {
    EXPECT_DCHECK_DEATH({ RunAndPopNextTask(std::move(sequence)); });
  }
}

// Verify that AssertIOAllowed() succeeds only for a MayBlock() task.
TEST_P(ThreadPoolTaskTrackerTest, IOAllowed) {
  // Unset the IO allowed bit. Expect TaskTracker to set it before running a
  // task with the MayBlock() trait.
  ThreadRestrictions::SetIOAllowed(false);
  Task task_with_may_block(FROM_HERE, BindOnce([]() {
                             // Shouldn't fail.
                             ScopedBlockingCall scope_blocking_call(
                                 FROM_HERE, BlockingType::WILL_BLOCK);
                           }),
                           TimeTicks::Now(), TimeDelta());
  TaskTraits traits_with_may_block{MayBlock(), GetParam()};
  auto sequence_with_may_block = WillPostTaskAndQueueTaskSource(
      std::move(task_with_may_block), traits_with_may_block);
  EXPECT_TRUE(sequence_with_may_block);
  RunAndPopNextTask(std::move(sequence_with_may_block));

  // Set the IO allowed bit. Expect TaskTracker to unset it before running a
  // task without the MayBlock() trait.
  ThreadRestrictions::SetIOAllowed(true);
  Task task_without_may_block(FROM_HERE, BindOnce([]() {
                                EXPECT_DCHECK_DEATH({
                                  ScopedBlockingCall scope_blocking_call(
                                      FROM_HERE, BlockingType::WILL_BLOCK);
                                });
                              }),
                              TimeTicks::Now(), TimeDelta());
  TaskTraits traits_without_may_block = TaskTraits(GetParam());
  auto sequence_without_may_block = WillPostTaskAndQueueTaskSource(
      std::move(task_without_may_block), traits_without_may_block);
  EXPECT_TRUE(sequence_without_may_block);
  RunAndPopNextTask(std::move(sequence_without_may_block));
}

static void RunTaskRunnerHandleVerificationTask(
    TaskTracker* tracker,
    Task verify_task,
    TaskTraits traits,
    scoped_refptr<TaskRunner> task_runner,
    TaskSourceExecutionMode execution_mode) {
  // Pretend |verify_task| is posted to respect TaskTracker's contract.
  EXPECT_TRUE(tracker->WillPostTask(&verify_task, traits.shutdown_behavior()));

  // Confirm that the test conditions are right (no TaskRunnerHandles set
  // already).
  EXPECT_FALSE(ThreadTaskRunnerHandle::IsSet());
  EXPECT_FALSE(SequencedTaskRunnerHandle::IsSet());

  test::QueueAndRunTaskSource(
      tracker,
      test::CreateSequenceWithTask(std::move(verify_task), traits,
                                   std::move(task_runner), execution_mode));

  // TaskRunnerHandle state is reset outside of task's scope.
  EXPECT_FALSE(ThreadTaskRunnerHandle::IsSet());
  EXPECT_FALSE(SequencedTaskRunnerHandle::IsSet());
}

static void VerifyNoTaskRunnerHandle() {
  EXPECT_FALSE(ThreadTaskRunnerHandle::IsSet());
  EXPECT_FALSE(SequencedTaskRunnerHandle::IsSet());
}

TEST_P(ThreadPoolTaskTrackerTest, TaskRunnerHandleIsNotSetOnParallel) {
  // Create a task that will verify that TaskRunnerHandles are not set in its
  // scope per no TaskRunner ref being set to it.
  Task verify_task(FROM_HERE, BindOnce(&VerifyNoTaskRunnerHandle),
                   TimeTicks::Now(), TimeDelta());

  RunTaskRunnerHandleVerificationTask(&tracker_, std::move(verify_task),
                                      TaskTraits(GetParam()), nullptr,
                                      TaskSourceExecutionMode::kParallel);
}

static void VerifySequencedTaskRunnerHandle(
    const SequencedTaskRunner* expected_task_runner) {
  EXPECT_FALSE(ThreadTaskRunnerHandle::IsSet());
  EXPECT_TRUE(SequencedTaskRunnerHandle::IsSet());
  EXPECT_EQ(expected_task_runner, SequencedTaskRunnerHandle::Get());
}

TEST_P(ThreadPoolTaskTrackerTest, SequencedTaskRunnerHandleIsSetOnSequenced) {
  scoped_refptr<SequencedTaskRunner> test_task_runner(new TestSimpleTaskRunner);

  // Create a task that will verify that SequencedTaskRunnerHandle is properly
  // set to |test_task_runner| in its scope per |sequenced_task_runner_ref|
  // being set to it.
  Task verify_task(FROM_HERE,
                   BindOnce(&VerifySequencedTaskRunnerHandle,
                            Unretained(test_task_runner.get())),
                   TimeTicks::Now(), TimeDelta());

  RunTaskRunnerHandleVerificationTask(
      &tracker_, std::move(verify_task), TaskTraits(GetParam()),
      std::move(test_task_runner), TaskSourceExecutionMode::kSequenced);
}

static void VerifyThreadTaskRunnerHandle(
    const SingleThreadTaskRunner* expected_task_runner) {
  EXPECT_TRUE(ThreadTaskRunnerHandle::IsSet());
  // SequencedTaskRunnerHandle inherits ThreadTaskRunnerHandle for thread.
  EXPECT_TRUE(SequencedTaskRunnerHandle::IsSet());
  EXPECT_EQ(expected_task_runner, ThreadTaskRunnerHandle::Get());
}

TEST_P(ThreadPoolTaskTrackerTest, ThreadTaskRunnerHandleIsSetOnSingleThreaded) {
  scoped_refptr<SingleThreadTaskRunner> test_task_runner(
      new TestSimpleTaskRunner);

  // Create a task that will verify that ThreadTaskRunnerHandle is properly set
  // to |test_task_runner| in its scope per |single_thread_task_runner_ref|
  // being set on it.
  Task verify_task(FROM_HERE,
                   BindOnce(&VerifyThreadTaskRunnerHandle,
                            Unretained(test_task_runner.get())),
                   TimeTicks::Now(), TimeDelta());

  RunTaskRunnerHandleVerificationTask(
      &tracker_, std::move(verify_task), TaskTraits(GetParam()),
      std::move(test_task_runner), TaskSourceExecutionMode::kSingleThread);
}

TEST_P(ThreadPoolTaskTrackerTest, FlushPendingDelayedTask) {
  Task delayed_task(FROM_HERE, DoNothing(), TimeTicks::Now(),
                    TimeDelta::FromDays(1));
  tracker_.WillPostTask(&delayed_task, GetParam());
  // FlushForTesting() should return even if the delayed task didn't run.
  tracker_.FlushForTesting();
}

TEST_P(ThreadPoolTaskTrackerTest, FlushAsyncForTestingPendingDelayedTask) {
  Task delayed_task(FROM_HERE, DoNothing(), TimeTicks::Now(),
                    TimeDelta::FromDays(1));
  tracker_.WillPostTask(&delayed_task, GetParam());
  // FlushAsyncForTesting() should callback even if the delayed task didn't run.
  bool called_back = false;
  tracker_.FlushAsyncForTesting(
      BindOnce([](bool* called_back) { *called_back = true; },
               Unretained(&called_back)));
  EXPECT_TRUE(called_back);
}

TEST_P(ThreadPoolTaskTrackerTest, FlushPendingUndelayedTask) {
  Task undelayed_task(FROM_HERE, DoNothing(), TimeTicks::Now(), TimeDelta());
  auto undelayed_sequence =
      WillPostTaskAndQueueTaskSource(std::move(undelayed_task), {GetParam()});

  // FlushForTesting() shouldn't return before the undelayed task runs.
  CallFlushFromAnotherThread();
  PlatformThread::Sleep(TestTimeouts::tiny_timeout());
  VERIFY_ASYNC_FLUSH_IN_PROGRESS();

  // FlushForTesting() should return after the undelayed task runs.
  RunAndPopNextTask(std::move(undelayed_sequence));
  WAIT_FOR_ASYNC_FLUSH_RETURNED();
}

TEST_P(ThreadPoolTaskTrackerTest, FlushAsyncForTestingPendingUndelayedTask) {
  Task undelayed_task(FROM_HERE, DoNothing(), TimeTicks::Now(), TimeDelta());
  auto undelayed_sequence =
      WillPostTaskAndQueueTaskSource(std::move(undelayed_task), {GetParam()});

  // FlushAsyncForTesting() shouldn't callback before the undelayed task runs.
  TestWaitableEvent event;
  tracker_.FlushAsyncForTesting(
      BindOnce(&TestWaitableEvent::Signal, Unretained(&event)));
  PlatformThread::Sleep(TestTimeouts::tiny_timeout());
  EXPECT_FALSE(event.IsSignaled());

  // FlushAsyncForTesting() should callback after the undelayed task runs.
  RunAndPopNextTask(std::move(undelayed_sequence));
  event.Wait();
}

TEST_P(ThreadPoolTaskTrackerTest, PostTaskDuringFlush) {
  Task undelayed_task(FROM_HERE, DoNothing(), TimeTicks::Now(), TimeDelta());
  auto undelayed_sequence =
      WillPostTaskAndQueueTaskSource(std::move(undelayed_task), {GetParam()});

  // FlushForTesting() shouldn't return before the undelayed task runs.
  CallFlushFromAnotherThread();
  PlatformThread::Sleep(TestTimeouts::tiny_timeout());
  VERIFY_ASYNC_FLUSH_IN_PROGRESS();

  // Simulate posting another undelayed task.
  Task other_undelayed_task(FROM_HERE, DoNothing(), TimeTicks::Now(),
                            TimeDelta());
  auto other_undelayed_sequence = WillPostTaskAndQueueTaskSource(
      std::move(other_undelayed_task), {GetParam()});

  // Run the first undelayed task.
  RunAndPopNextTask(std::move(undelayed_sequence));

  // FlushForTesting() shouldn't return before the second undelayed task runs.
  PlatformThread::Sleep(TestTimeouts::tiny_timeout());
  VERIFY_ASYNC_FLUSH_IN_PROGRESS();

  // FlushForTesting() should return after the second undelayed task runs.
  RunAndPopNextTask(std::move(other_undelayed_sequence));
  WAIT_FOR_ASYNC_FLUSH_RETURNED();
}

TEST_P(ThreadPoolTaskTrackerTest, PostTaskDuringFlushAsyncForTesting) {
  Task undelayed_task(FROM_HERE, DoNothing(), TimeTicks::Now(), TimeDelta());
  auto undelayed_sequence =
      WillPostTaskAndQueueTaskSource(std::move(undelayed_task), {GetParam()});

  // FlushAsyncForTesting() shouldn't callback before the undelayed task runs.
  TestWaitableEvent event;
  tracker_.FlushAsyncForTesting(
      BindOnce(&TestWaitableEvent::Signal, Unretained(&event)));
  PlatformThread::Sleep(TestTimeouts::tiny_timeout());
  EXPECT_FALSE(event.IsSignaled());

  // Simulate posting another undelayed task.
  Task other_undelayed_task(FROM_HERE, DoNothing(), TimeTicks::Now(),
                            TimeDelta());
  auto other_undelayed_sequence = WillPostTaskAndQueueTaskSource(
      std::move(other_undelayed_task), {GetParam()});

  // Run the first undelayed task.
  RunAndPopNextTask(std::move(undelayed_sequence));

  // FlushAsyncForTesting() shouldn't callback before the second undelayed task
  // runs.
  PlatformThread::Sleep(TestTimeouts::tiny_timeout());
  EXPECT_FALSE(event.IsSignaled());

  // FlushAsyncForTesting() should callback after the second undelayed task
  // runs.
  RunAndPopNextTask(std::move(other_undelayed_sequence));
  event.Wait();
}

TEST_P(ThreadPoolTaskTrackerTest, RunDelayedTaskDuringFlush) {
  // Simulate posting a delayed and an undelayed task.
  Task delayed_task(FROM_HERE, DoNothing(), TimeTicks::Now(),
                    TimeDelta::FromDays(1));
  auto delayed_sequence =
      WillPostTaskAndQueueTaskSource(std::move(delayed_task), {GetParam()});
  Task undelayed_task(FROM_HERE, DoNothing(), TimeTicks::Now(), TimeDelta());
  auto undelayed_sequence =
      WillPostTaskAndQueueTaskSource(std::move(undelayed_task), {GetParam()});

  // FlushForTesting() shouldn't return before the undelayed task runs.
  CallFlushFromAnotherThread();
  PlatformThread::Sleep(TestTimeouts::tiny_timeout());
  VERIFY_ASYNC_FLUSH_IN_PROGRESS();

  // Run the delayed task.
  RunAndPopNextTask(std::move(delayed_sequence));

  // FlushForTesting() shouldn't return since there is still a pending undelayed
  // task.
  PlatformThread::Sleep(TestTimeouts::tiny_timeout());
  VERIFY_ASYNC_FLUSH_IN_PROGRESS();

  // Run the undelayed task.
  RunAndPopNextTask(std::move(undelayed_sequence));

  // FlushForTesting() should now return.
  WAIT_FOR_ASYNC_FLUSH_RETURNED();
}

TEST_P(ThreadPoolTaskTrackerTest, RunDelayedTaskDuringFlushAsyncForTesting) {
  // Simulate posting a delayed and an undelayed task.
  Task delayed_task(FROM_HERE, DoNothing(), TimeTicks::Now(),
                    TimeDelta::FromDays(1));
  auto delayed_sequence =
      WillPostTaskAndQueueTaskSource(std::move(delayed_task), {GetParam()});
  Task undelayed_task(FROM_HERE, DoNothing(), TimeTicks::Now(), TimeDelta());
  auto undelayed_sequence =
      WillPostTaskAndQueueTaskSource(std::move(undelayed_task), {GetParam()});

  // FlushAsyncForTesting() shouldn't callback before the undelayed task runs.
  TestWaitableEvent event;
  tracker_.FlushAsyncForTesting(
      BindOnce(&TestWaitableEvent::Signal, Unretained(&event)));
  PlatformThread::Sleep(TestTimeouts::tiny_timeout());
  EXPECT_FALSE(event.IsSignaled());

  // Run the delayed task.
  RunAndPopNextTask(std::move(delayed_sequence));

  // FlushAsyncForTesting() shouldn't callback since there is still a pending
  // undelayed task.
  PlatformThread::Sleep(TestTimeouts::tiny_timeout());
  EXPECT_FALSE(event.IsSignaled());

  // Run the undelayed task.
  RunAndPopNextTask(std::move(undelayed_sequence));

  // FlushAsyncForTesting() should now callback.
  event.Wait();
}

TEST_P(ThreadPoolTaskTrackerTest, FlushAfterShutdown) {
  if (GetParam() == TaskShutdownBehavior::BLOCK_SHUTDOWN)
    return;

  // Simulate posting a task.
  Task undelayed_task(FROM_HERE, DoNothing(), TimeTicks::Now(), TimeDelta());
  tracker_.WillPostTask(&undelayed_task, GetParam());

  // Shutdown() should return immediately since there are no pending
  // BLOCK_SHUTDOWN tasks.
  test::ShutdownTaskTracker(&tracker_);

  // FlushForTesting() should return immediately after shutdown, even if an
  // undelayed task hasn't run.
  tracker_.FlushForTesting();
}

TEST_P(ThreadPoolTaskTrackerTest, FlushAfterShutdownAsync) {
  if (GetParam() == TaskShutdownBehavior::BLOCK_SHUTDOWN)
    return;

  // Simulate posting a task.
  Task undelayed_task(FROM_HERE, DoNothing(), TimeTicks::Now(), TimeDelta());
  tracker_.WillPostTask(&undelayed_task, GetParam());

  // Shutdown() should return immediately since there are no pending
  // BLOCK_SHUTDOWN tasks.
  test::ShutdownTaskTracker(&tracker_);

  // FlushForTesting() should callback immediately after shutdown, even if an
  // undelayed task hasn't run.
  bool called_back = false;
  tracker_.FlushAsyncForTesting(
      BindOnce([](bool* called_back) { *called_back = true; },
               Unretained(&called_back)));
  EXPECT_TRUE(called_back);
}

TEST_P(ThreadPoolTaskTrackerTest, ShutdownDuringFlush) {
  if (GetParam() == TaskShutdownBehavior::BLOCK_SHUTDOWN)
    return;

  // Simulate posting a task.
  Task undelayed_task(FROM_HERE, DoNothing(), TimeTicks::Now(), TimeDelta());
  auto undelayed_sequence =
      WillPostTaskAndQueueTaskSource(std::move(undelayed_task), {GetParam()});

  // FlushForTesting() shouldn't return before the undelayed task runs or
  // shutdown completes.
  CallFlushFromAnotherThread();
  PlatformThread::Sleep(TestTimeouts::tiny_timeout());
  VERIFY_ASYNC_FLUSH_IN_PROGRESS();

  // Shutdown() should return immediately since there are no pending
  // BLOCK_SHUTDOWN tasks.
  test::ShutdownTaskTracker(&tracker_);

  // FlushForTesting() should now return, even if an undelayed task hasn't run.
  WAIT_FOR_ASYNC_FLUSH_RETURNED();
}

TEST_P(ThreadPoolTaskTrackerTest, ShutdownDuringFlushAsyncForTesting) {
  if (GetParam() == TaskShutdownBehavior::BLOCK_SHUTDOWN)
    return;

  // Simulate posting a task.
  Task undelayed_task(FROM_HERE, DoNothing(), TimeTicks::Now(), TimeDelta());
  auto undelayed_sequence =
      WillPostTaskAndQueueTaskSource(std::move(undelayed_task), {GetParam()});

  // FlushAsyncForTesting() shouldn't callback before the undelayed task runs or
  // shutdown completes.
  TestWaitableEvent event;
  tracker_.FlushAsyncForTesting(
      BindOnce(&TestWaitableEvent::Signal, Unretained(&event)));
  PlatformThread::Sleep(TestTimeouts::tiny_timeout());
  EXPECT_FALSE(event.IsSignaled());

  // Shutdown() should return immediately since there are no pending
  // BLOCK_SHUTDOWN tasks.
  test::ShutdownTaskTracker(&tracker_);

  // FlushAsyncForTesting() should now callback, even if an undelayed task
  // hasn't run.
  event.Wait();
}

TEST_P(ThreadPoolTaskTrackerTest, DoublePendingFlushAsyncForTestingFails) {
  Task undelayed_task(FROM_HERE, DoNothing(), TimeTicks::Now(), TimeDelta());
  auto undelayed_sequence =
      WillPostTaskAndQueueTaskSource(std::move(undelayed_task), {GetParam()});

  // FlushAsyncForTesting() shouldn't callback before the undelayed task runs.
  bool called_back = false;
  tracker_.FlushAsyncForTesting(
      BindOnce([](bool* called_back) { *called_back = true; },
               Unretained(&called_back)));
  EXPECT_FALSE(called_back);
  EXPECT_DCHECK_DEATH({ tracker_.FlushAsyncForTesting(BindOnce([]() {})); });
  undelayed_sequence.Unregister();
}

TEST_P(ThreadPoolTaskTrackerTest, PostTasksDoNotBlockShutdown) {
  // Simulate posting an undelayed task.
  Task undelayed_task(FROM_HERE, DoNothing(), TimeTicks::Now(), TimeDelta());
  EXPECT_TRUE(tracker_.WillPostTask(&undelayed_task, GetParam()));

  // Since no sequence was queued, a call to Shutdown() should not hang.
  test::ShutdownTaskTracker(&tracker_);
}

// Verify that a delayed task does not block shutdown once it's run, regardless
// of its shutdown behavior.
TEST_P(ThreadPoolTaskTrackerTest, DelayedRunTasks) {
  // Simulate posting a delayed task.
  Task delayed_task(FROM_HERE, DoNothing(), TimeTicks::Now(),
                    TimeDelta::FromDays(1));
  auto sequence =
      WillPostTaskAndQueueTaskSource(std::move(delayed_task), {GetParam()});
  EXPECT_TRUE(sequence);

  RunAndPopNextTask(std::move(sequence));

  // Since the delayed task doesn't block shutdown, a call to Shutdown() should
  // not hang.
  test::ShutdownTaskTracker(&tracker_);
}

INSTANTIATE_TEST_SUITE_P(
    ContinueOnShutdown,
    ThreadPoolTaskTrackerTest,
    ::testing::Values(TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN));
INSTANTIATE_TEST_SUITE_P(
    SkipOnShutdown,
    ThreadPoolTaskTrackerTest,
    ::testing::Values(TaskShutdownBehavior::SKIP_ON_SHUTDOWN));
INSTANTIATE_TEST_SUITE_P(
    BlockShutdown,
    ThreadPoolTaskTrackerTest,
    ::testing::Values(TaskShutdownBehavior::BLOCK_SHUTDOWN));

namespace {

void ExpectSequenceToken(SequenceToken sequence_token) {
  EXPECT_EQ(sequence_token, SequenceToken::GetForCurrentThread());
}

}  // namespace

// Verify that SequenceToken::GetForCurrentThread() returns the Sequence's token
// when a Task runs.
TEST_F(ThreadPoolTaskTrackerTest, CurrentSequenceToken) {
  scoped_refptr<Sequence> sequence = MakeRefCounted<Sequence>(
      TaskTraits(), nullptr, TaskSourceExecutionMode::kParallel);

  const SequenceToken sequence_token = sequence->token();
  Task task(FROM_HERE, BindOnce(&ExpectSequenceToken, sequence_token),
            TimeTicks::Now(), TimeDelta());
  tracker_.WillPostTask(&task, sequence->shutdown_behavior());

  {
    Sequence::Transaction sequence_transaction(sequence->BeginTransaction());
    sequence_transaction.PushTask(std::move(task));

    EXPECT_FALSE(SequenceToken::GetForCurrentThread().IsValid());
  }

  test::QueueAndRunTaskSource(&tracker_, std::move(sequence));
  EXPECT_FALSE(SequenceToken::GetForCurrentThread().IsValid());
}

TEST_F(ThreadPoolTaskTrackerTest, LoadWillPostAndRunBeforeShutdown) {
  // Post and run tasks asynchronously.
  std::vector<std::unique_ptr<ThreadPostingAndRunningTask>> threads;

  for (size_t i = 0; i < kLoadTestNumIterations; ++i) {
    threads.push_back(std::make_unique<ThreadPostingAndRunningTask>(
        &tracker_,
        MakeRefCounted<Sequence>(
            TaskTraits{TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN}, nullptr,
            TaskSourceExecutionMode::kParallel),
        ThreadPostingAndRunningTask::Action::WILL_POST_AND_RUN, true,
        CreateTask()));
    threads.back()->Start();

    threads.push_back(std::make_unique<ThreadPostingAndRunningTask>(
        &tracker_,
        MakeRefCounted<Sequence>(
            TaskTraits{TaskShutdownBehavior::SKIP_ON_SHUTDOWN}, nullptr,
            TaskSourceExecutionMode::kParallel),
        ThreadPostingAndRunningTask::Action::WILL_POST_AND_RUN, true,
        CreateTask()));
    threads.back()->Start();

    threads.push_back(std::make_unique<ThreadPostingAndRunningTask>(
        &tracker_,
        MakeRefCounted<Sequence>(
            TaskTraits{TaskShutdownBehavior::BLOCK_SHUTDOWN}, nullptr,
            TaskSourceExecutionMode::kParallel),
        ThreadPostingAndRunningTask::Action::WILL_POST_AND_RUN, true,
        CreateTask()));
    threads.back()->Start();
  }

  for (const auto& thread : threads)
    thread->Join();

  // Expect all tasks to be executed.
  EXPECT_EQ(kLoadTestNumIterations * 3, NumTasksExecuted());

  // Should return immediately because no tasks are blocking shutdown.
  test::ShutdownTaskTracker(&tracker_);
}

TEST_F(ThreadPoolTaskTrackerTest,
       LoadWillPostBeforeShutdownAndRunDuringShutdown) {
  constexpr TaskTraits traits_continue_on_shutdown =
      TaskTraits(TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN);
  constexpr TaskTraits traits_skip_on_shutdown =
      TaskTraits(TaskShutdownBehavior::SKIP_ON_SHUTDOWN);
  constexpr TaskTraits traits_block_shutdown =
      TaskTraits(TaskShutdownBehavior::BLOCK_SHUTDOWN);

  // Post tasks asynchronously.
  std::vector<std::unique_ptr<ThreadPostingAndRunningTask>> post_threads;
  {
    std::vector<scoped_refptr<Sequence>> sequences_continue_on_shutdown;
    std::vector<scoped_refptr<Sequence>> sequences_skip_on_shutdown;
    std::vector<scoped_refptr<Sequence>> sequences_block_shutdown;
    for (size_t i = 0; i < kLoadTestNumIterations; ++i) {
      sequences_continue_on_shutdown.push_back(
          MakeRefCounted<Sequence>(traits_continue_on_shutdown, nullptr,
                                   TaskSourceExecutionMode::kParallel));
      sequences_skip_on_shutdown.push_back(
          MakeRefCounted<Sequence>(traits_skip_on_shutdown, nullptr,
                                   TaskSourceExecutionMode::kParallel));
      sequences_block_shutdown.push_back(MakeRefCounted<Sequence>(
          traits_block_shutdown, nullptr, TaskSourceExecutionMode::kParallel));
    }

    for (size_t i = 0; i < kLoadTestNumIterations; ++i) {
      post_threads.push_back(std::make_unique<ThreadPostingAndRunningTask>(
          &tracker_, sequences_continue_on_shutdown[i],
          ThreadPostingAndRunningTask::Action::WILL_POST, true, CreateTask()));
      post_threads.back()->Start();

      post_threads.push_back(std::make_unique<ThreadPostingAndRunningTask>(
          &tracker_, sequences_skip_on_shutdown[i],
          ThreadPostingAndRunningTask::Action::WILL_POST, true, CreateTask()));
      post_threads.back()->Start();

      post_threads.push_back(std::make_unique<ThreadPostingAndRunningTask>(
          &tracker_, sequences_block_shutdown[i],
          ThreadPostingAndRunningTask::Action::WILL_POST, true, CreateTask()));
      post_threads.back()->Start();
    }
  }

  for (const auto& thread : post_threads)
    thread->Join();

  // Start shutdown and try to complete shutdown asynchronously.
  tracker_.StartShutdown();
  ExpectAsyncCompleteShutdownBlocks();

  // Run tasks asynchronously.
  std::vector<std::unique_ptr<ThreadPostingAndRunningTask>> run_threads;
  for (size_t i = 0; i < kLoadTestNumIterations; ++i) {
    run_threads.push_back(std::make_unique<ThreadPostingAndRunningTask>(
        &tracker_, post_threads[i * 3]->TakeTaskSource()));
    run_threads.back()->Start();

    run_threads.push_back(std::make_unique<ThreadPostingAndRunningTask>(
        &tracker_, post_threads[i * 3 + 1]->TakeTaskSource()));
    run_threads.back()->Start();

    run_threads.push_back(std::make_unique<ThreadPostingAndRunningTask>(
        &tracker_, post_threads[i * 3 + 2]->TakeTaskSource()));
    run_threads.back()->Start();
  }

  for (const auto& thread : run_threads)
    thread->Join();

  WAIT_FOR_ASYNC_SHUTDOWN_COMPLETED();

  // Expect BLOCK_SHUTDOWN tasks to have been executed.
  EXPECT_EQ(kLoadTestNumIterations, NumTasksExecuted());
}

TEST_F(ThreadPoolTaskTrackerTest, LoadWillPostAndRunDuringShutdown) {
  // Inform |task_tracker_| that a BLOCK_SHUTDOWN task will be posted just to
  // block shutdown.
  auto block_shutdown_sequence = WillPostTaskAndQueueTaskSource(
      CreateTask(), {TaskShutdownBehavior::BLOCK_SHUTDOWN});
  EXPECT_TRUE(block_shutdown_sequence);

  // Start shutdown and try to complete it asynchronously.
  tracker_.StartShutdown();
  ExpectAsyncCompleteShutdownBlocks();

  // Post and run tasks asynchronously.
  std::vector<std::unique_ptr<ThreadPostingAndRunningTask>> threads;

  for (size_t i = 0; i < kLoadTestNumIterations; ++i) {
    threads.push_back(std::make_unique<ThreadPostingAndRunningTask>(
        &tracker_,
        MakeRefCounted<Sequence>(
            TaskTraits{TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN}, nullptr,
            TaskSourceExecutionMode::kParallel),
        ThreadPostingAndRunningTask::Action::WILL_POST_AND_RUN, false,
        CreateTask()));
    threads.back()->Start();

    threads.push_back(std::make_unique<ThreadPostingAndRunningTask>(
        &tracker_,
        MakeRefCounted<Sequence>(
            TaskTraits{TaskShutdownBehavior::SKIP_ON_SHUTDOWN}, nullptr,
            TaskSourceExecutionMode::kParallel),
        ThreadPostingAndRunningTask::Action::WILL_POST_AND_RUN, false,
        CreateTask()));
    threads.back()->Start();

    threads.push_back(std::make_unique<ThreadPostingAndRunningTask>(
        &tracker_,
        MakeRefCounted<Sequence>(
            TaskTraits{TaskShutdownBehavior::BLOCK_SHUTDOWN}, nullptr,
            TaskSourceExecutionMode::kParallel),
        ThreadPostingAndRunningTask::Action::WILL_POST_AND_RUN, true,
        CreateTask()));
    threads.back()->Start();
  }

  for (const auto& thread : threads)
    thread->Join();

  // Expect BLOCK_SHUTDOWN tasks to have been executed.
  EXPECT_EQ(kLoadTestNumIterations, NumTasksExecuted());

  // Shutdown() shouldn't return before |block_shutdown_task| is executed.
  VERIFY_ASYNC_SHUTDOWN_IN_PROGRESS();

  // Unblock shutdown by running |block_shutdown_task|.
  RunAndPopNextTask(std::move(block_shutdown_sequence));
  EXPECT_EQ(kLoadTestNumIterations + 1, NumTasksExecuted());
  WAIT_FOR_ASYNC_SHUTDOWN_COMPLETED();
}

// Verify that RunAndPopNextTask() returns the sequence from which it ran a task
// when it can be rescheduled.
TEST_F(ThreadPoolTaskTrackerTest,
       RunAndPopNextTaskReturnsSequenceToReschedule) {
  TaskTraits default_traits;
  Task task_1(FROM_HERE, DoNothing(), TimeTicks::Now(), TimeDelta());
  EXPECT_TRUE(
      tracker_.WillPostTask(&task_1, default_traits.shutdown_behavior()));
  Task task_2(FROM_HERE, DoNothing(), TimeTicks::Now(), TimeDelta());
  EXPECT_TRUE(
      tracker_.WillPostTask(&task_2, default_traits.shutdown_behavior()));

  scoped_refptr<Sequence> sequence =
      test::CreateSequenceWithTask(std::move(task_1), default_traits);
  sequence->BeginTransaction().PushTask(std::move(task_2));
  EXPECT_EQ(sequence,
            test::QueueAndRunTaskSource(&tracker_, sequence).Unregister());
}

namespace {

class WaitAllowedTestThread : public SimpleThread {
 public:
  WaitAllowedTestThread() : SimpleThread("WaitAllowedTestThread") {}
  WaitAllowedTestThread(const WaitAllowedTestThread&) = delete;
  WaitAllowedTestThread& operator=(const WaitAllowedTestThread&) = delete;

 private:
  void Run() override {
    auto task_tracker = std::make_unique<TaskTracker>();

    // Waiting is allowed by default. Expect TaskTracker to disallow it before
    // running a task without the WithBaseSyncPrimitives() trait.
    internal::AssertBaseSyncPrimitivesAllowed();
    Task task_without_sync_primitives(
        FROM_HERE, BindOnce([]() {
          EXPECT_DCHECK_DEATH({ internal::AssertBaseSyncPrimitivesAllowed(); });
        }),
        TimeTicks::Now(), TimeDelta());
    TaskTraits default_traits;
    EXPECT_TRUE(task_tracker->WillPostTask(&task_without_sync_primitives,
                                           default_traits.shutdown_behavior()));
    auto sequence_without_sync_primitives = test::CreateSequenceWithTask(
        std::move(task_without_sync_primitives), default_traits);
    test::QueueAndRunTaskSource(task_tracker.get(),
                                std::move(sequence_without_sync_primitives));

    // Disallow waiting. Expect TaskTracker to allow it before running a task
    // with the WithBaseSyncPrimitives() trait.
    ThreadRestrictions::DisallowWaiting();
    Task task_with_sync_primitives(
        FROM_HERE, BindOnce([]() {
          // Shouldn't fail.
          internal::AssertBaseSyncPrimitivesAllowed();
        }),
        TimeTicks::Now(), TimeDelta());
    TaskTraits traits_with_sync_primitives =
        TaskTraits(WithBaseSyncPrimitives());
    EXPECT_TRUE(task_tracker->WillPostTask(
        &task_with_sync_primitives,
        traits_with_sync_primitives.shutdown_behavior()));
    auto sequence_with_sync_primitives = test::CreateSequenceWithTask(
        std::move(task_with_sync_primitives), traits_with_sync_primitives);
    test::QueueAndRunTaskSource(task_tracker.get(),
                                std::move(sequence_with_sync_primitives));

    ScopedAllowBaseSyncPrimitivesForTesting
        allow_wait_in_task_tracker_destructor;
    task_tracker.reset();
  }
};

}  // namespace

// Verify that AssertIOAllowed() succeeds only for a WithBaseSyncPrimitives()
// task.
TEST(ThreadPoolTaskTrackerWaitAllowedTest, WaitAllowed) {
  // Run the test on the separate thread since it is not possible to reset the
  // "wait allowed" bit of a thread without being a friend of
  // ThreadRestrictions.
  testing::GTEST_FLAG(death_test_style) = "threadsafe";
  WaitAllowedTestThread wait_allowed_test_thread;
  wait_allowed_test_thread.Start();
  wait_allowed_test_thread.Join();
}

}  // namespace internal
}  // namespace base
