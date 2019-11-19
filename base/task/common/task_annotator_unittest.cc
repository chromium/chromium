// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/common/task_annotator.h"

#include <algorithm>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/pending_task.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/post_task.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

void TestTask(int* result) {
  *result = 123;
}

}  // namespace

TEST(TaskAnnotatorTest, QueueAndRunTask) {
  int result = 0;
  PendingTask pending_task(FROM_HERE, BindOnce(&TestTask, &result));

  TaskAnnotator annotator;
  annotator.WillQueueTask("TaskAnnotatorTest::Queue", &pending_task, "?");
  EXPECT_EQ(0, result);
  annotator.RunTask("TaskAnnotatorTest::Queue", &pending_task);
  EXPECT_EQ(123, result);
}

// Test task annotator integration in base APIs and ensuing support for
// backtraces. Tasks posted across multiple threads in this test fixture should
// be synchronized as BeforeRunTask() and VerifyTraceAndPost() assume tasks are
// observed in lock steps, one at a time.
class TaskAnnotatorBacktraceIntegrationTest
    : public ::testing::Test,
      public TaskAnnotator::ObserverForTesting {
 public:
  using ExpectedTrace = std::vector<const void*>;

  TaskAnnotatorBacktraceIntegrationTest() = default;

  ~TaskAnnotatorBacktraceIntegrationTest() override = default;

  // TaskAnnotator::ObserverForTesting:
  void BeforeRunTask(const PendingTask* pending_task) override {
    AutoLock auto_lock(on_before_run_task_lock_);
    last_posted_from_ = pending_task->posted_from;
    last_task_backtrace_ = pending_task->task_backtrace;
    last_ipc_hash_ = pending_task->ipc_hash;
  }

  void SetUp() override { TaskAnnotator::RegisterObserverForTesting(this); }

  void TearDown() override { TaskAnnotator::ClearObserverForTesting(); }

  void VerifyTraceAndPost(const scoped_refptr<SequencedTaskRunner>& task_runner,
                          const Location& posted_from,
                          const Location& next_from_here,
                          const ExpectedTrace& expected_trace,
                          uint32_t expected_ipc_hash,
                          OnceClosure task) {
    SCOPED_TRACE(StringPrintf("Callback Depth: %zu", expected_trace.size()));

    EXPECT_EQ(posted_from, last_posted_from_);
    for (size_t i = 0; i < last_task_backtrace_.size(); i++) {
      SCOPED_TRACE(StringPrintf("Trace frame: %zu", i));
      if (i < expected_trace.size())
        EXPECT_EQ(expected_trace[i], last_task_backtrace_[i]);
      else
        EXPECT_EQ(nullptr, last_task_backtrace_[i]);
    }
    EXPECT_EQ(expected_ipc_hash, last_ipc_hash_);

    task_runner->PostTask(next_from_here, std::move(task));
  }

  void VerifyTraceAndPostWithIpcContext(
      const scoped_refptr<SequencedTaskRunner>& task_runner,
      const Location& posted_from,
      const Location& next_from_here,
      const ExpectedTrace& expected_trace,
      uint32_t expected_ipc_hash,
      OnceClosure task,
      uint32_t new_ipc_hash) {
    TaskAnnotator::ScopedSetIpcHash scoped_ipc_hash(new_ipc_hash);
    VerifyTraceAndPost(task_runner, posted_from, next_from_here, expected_trace,
                       expected_ipc_hash, std::move(task));
  }

  // Same as VerifyTraceAndPost() with the exception that it also posts a task
  // that will prevent |task| from running until |wait_before_next_task| is
  // signaled.
  void VerifyTraceAndPostWithBlocker(
      const scoped_refptr<SequencedTaskRunner>& task_runner,
      const Location& posted_from,
      const Location& next_from_here,
      const ExpectedTrace& expected_trace,
      uint32_t expected_ipc_hash,
      OnceClosure task,
      WaitableEvent* wait_before_next_task) {
    DCHECK(wait_before_next_task);

    // Need to lock to ensure the upcoming VerifyTraceAndPost() runs before the
    // BeforeRunTask() hook for the posted WaitableEvent::Wait(). Otherwise the
    // upcoming VerifyTraceAndPost() will race to read the state saved in the
    // BeforeRunTask() hook preceding the current task.
    AutoLock auto_lock(on_before_run_task_lock_);
    task_runner->PostTask(
        FROM_HERE,
        BindOnce(&WaitableEvent::Wait, Unretained(wait_before_next_task)));
    VerifyTraceAndPost(task_runner, posted_from, next_from_here, expected_trace,
                       expected_ipc_hash, std::move(task));
  }

 protected:
  static void RunTwo(OnceClosure c1, OnceClosure c2) {
    std::move(c1).Run();
    std::move(c2).Run();
  }

 private:
  // While calls to VerifyTraceAndPost() are strictly ordered in tests below
  // (and hence non-racy), some helper methods (e.g. Wait/Signal) do racily call
  // into BeforeRunTask(). This Lock ensures these unobserved writes are not
  // racing. Locking isn't required on read per the VerifyTraceAndPost()
  // themselves being ordered.
  Lock on_before_run_task_lock_;

  Location last_posted_from_ = {};
  std::array<const void*, PendingTask::kTaskBacktraceLength>
      last_task_backtrace_ = {};

  uint32_t last_ipc_hash_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TaskAnnotatorBacktraceIntegrationTest);
};

// Ensure the task backtrace populates correctly.
TEST_F(TaskAnnotatorBacktraceIntegrationTest, SingleThreadedSimple) {
  test::TaskEnvironment task_environment;
  const uint32_t dummy_ipc_hash = 0xDEADBEEF;
  const Location location0 = FROM_HERE;
  const Location location1 = FROM_HERE;
  const Location location2 = FROM_HERE;
  const Location location3 = FROM_HERE;
  const Location location4 = FROM_HERE;
  const Location location5 = FROM_HERE;

  RunLoop run_loop;

  // Task 0 executes with no IPC context. Task 1 executes under an explicitly
  // set IPC context, and tasks 2-5 inherit that context.

  // Task 5 has tasks 4/3/2/1 as parents (task 0 isn't visible as only the
  // last 4 parents are kept).
  OnceClosure task5 = BindOnce(
      &TaskAnnotatorBacktraceIntegrationTest::VerifyTraceAndPost,
      Unretained(this), ThreadTaskRunnerHandle::Get(), location5, FROM_HERE,
      ExpectedTrace({location4.program_counter(), location3.program_counter(),
                     location2.program_counter(), location1.program_counter()}),
      dummy_ipc_hash, run_loop.QuitClosure());

  // Task i=4/3/2/1/0 have tasks [0,i) as parents.
  OnceClosure task4 = BindOnce(
      &TaskAnnotatorBacktraceIntegrationTest::VerifyTraceAndPost,
      Unretained(this), ThreadTaskRunnerHandle::Get(), location4, location5,
      ExpectedTrace({location3.program_counter(), location2.program_counter(),
                     location1.program_counter(), location0.program_counter()}),
      dummy_ipc_hash, std::move(task5));
  OnceClosure task3 = BindOnce(
      &TaskAnnotatorBacktraceIntegrationTest::VerifyTraceAndPost,
      Unretained(this), ThreadTaskRunnerHandle::Get(), location3, location4,
      ExpectedTrace({location2.program_counter(), location1.program_counter(),
                     location0.program_counter()}),
      dummy_ipc_hash, std::move(task4));
  OnceClosure task2 = BindOnce(
      &TaskAnnotatorBacktraceIntegrationTest::VerifyTraceAndPost,
      Unretained(this), ThreadTaskRunnerHandle::Get(), location2, location3,
      ExpectedTrace({location1.program_counter(), location0.program_counter()}),
      dummy_ipc_hash, std::move(task3));
  OnceClosure task1 = BindOnce(
      &TaskAnnotatorBacktraceIntegrationTest::VerifyTraceAndPostWithIpcContext,
      Unretained(this), ThreadTaskRunnerHandle::Get(), location1, location2,
      ExpectedTrace({location0.program_counter()}), 0, std::move(task2),
      dummy_ipc_hash);
  OnceClosure task0 =
      BindOnce(&TaskAnnotatorBacktraceIntegrationTest::VerifyTraceAndPost,
               Unretained(this), ThreadTaskRunnerHandle::Get(), location0,
               location1, ExpectedTrace({}), 0, std::move(task1));

  ThreadTaskRunnerHandle::Get()->PostTask(location0, std::move(task0));

  run_loop.Run();
}

// Ensure it works when posting tasks across multiple threads managed by //base.
TEST_F(TaskAnnotatorBacktraceIntegrationTest, MultipleThreads) {
  test::TaskEnvironment task_environment;

  // Use diverse task runners (a task environment main thread, a ThreadPool
  // based SequencedTaskRunner, and a ThreadPool based
  // SingleThreadTaskRunner) to verify that TaskAnnotator can capture backtraces
  // for PostTasks back-and-forth between these.
  auto main_thread_a = ThreadTaskRunnerHandle::Get();
  auto task_runner_b = CreateSingleThreadTaskRunner({ThreadPool()});
  auto task_runner_c = CreateSequencedTaskRunner(
      {ThreadPool(), base::MayBlock(), base::WithBaseSyncPrimitives()});

  const Location& location_a0 = FROM_HERE;
  const Location& location_a1 = FROM_HERE;
  const Location& location_a2 = FROM_HERE;
  const Location& location_a3 = FROM_HERE;

  const Location& location_b0 = FROM_HERE;
  const Location& location_b1 = FROM_HERE;

  const Location& location_c0 = FROM_HERE;

  RunLoop run_loop;

  // All tasks below happen in lock step by nature of being posted by the
  // previous one (plus the synchronous nature of RunTwo()) with the exception
  // of the follow-up local task to |task_b0_local|. This WaitableEvent ensures
  // it completes before |task_c0| runs to avoid racy invocations of
  // BeforeRunTask()+VerifyTraceAndPost().
  WaitableEvent lock_step(WaitableEvent::ResetPolicy::AUTOMATIC,
                          WaitableEvent::InitialState::NOT_SIGNALED);

  // Here is the execution order generated below:
  //  A: TA0 -> TA1 \                                    TA2
  //  B:            TB0L \ + TB0F \  Signal \           /
  //                      ---------\--/      \         /
  //                                \         \       /
  //  C:                            Wait........ TC0 /

  // IPC contexts:
  // TA0 and TA1 execute with no IPC context.
  // TB0L is the first task to execute with an explicit IPC context.
  // TB0F inherits no context.
  // TC0 is posted with a new IPC context from TB0L.
  // TA2 inherits that IPC context.
  const uint32_t dummy_ipc_hash0 = 0xDEADBEEF;
  const uint32_t dummy_ipc_hash1 = 0xBAADF00D;

  // On task runner c, post a task back to main thread that verifies its trace
  // and terminates after one more self-post.
  OnceClosure task_a2 = BindOnce(
      &TaskAnnotatorBacktraceIntegrationTest::VerifyTraceAndPost,
      Unretained(this), main_thread_a, location_a2, location_a3,
      ExpectedTrace(
          {location_c0.program_counter(), location_b0.program_counter(),
           location_a1.program_counter(), location_a0.program_counter()}),
      dummy_ipc_hash1, run_loop.QuitClosure());
  OnceClosure task_c0 = BindOnce(
      &TaskAnnotatorBacktraceIntegrationTest::VerifyTraceAndPostWithIpcContext,
      Unretained(this), main_thread_a, location_c0, location_a2,
      ExpectedTrace({location_b0.program_counter(),
                     location_a1.program_counter(),
                     location_a0.program_counter()}),
      0, std::move(task_a2), dummy_ipc_hash1);

  // On task runner b run two tasks that conceptually come from the same
  // location (managed via RunTwo().) One will post back to task runner b and
  // another will post to task runner c to test spawning multiple tasks on
  // different message loops. The task posted to task runner c will not get
  // location b1 whereas the one posted back to task runner b will.
  OnceClosure task_b0_fork = BindOnce(
      &TaskAnnotatorBacktraceIntegrationTest::VerifyTraceAndPostWithBlocker,
      Unretained(this), task_runner_c, location_b0, location_c0,
      ExpectedTrace(
          {location_a1.program_counter(), location_a0.program_counter()}),
      0, std::move(task_c0), &lock_step);
  OnceClosure task_b0_local = BindOnce(
      &TaskAnnotatorBacktraceIntegrationTest::VerifyTraceAndPostWithIpcContext,
      Unretained(this), task_runner_b, location_b0, location_b1,
      ExpectedTrace(
          {location_a1.program_counter(), location_a0.program_counter()}),
      0, BindOnce(&WaitableEvent::Signal, Unretained(&lock_step)),
      dummy_ipc_hash0);

  OnceClosure task_a1 =
      BindOnce(&TaskAnnotatorBacktraceIntegrationTest::VerifyTraceAndPost,
               Unretained(this), task_runner_b, location_a1, location_b0,
               ExpectedTrace({location_a0.program_counter()}), 0,
               BindOnce(&TaskAnnotatorBacktraceIntegrationTest::RunTwo,
                        std::move(task_b0_local), std::move(task_b0_fork)));
  OnceClosure task_a0 =
      BindOnce(&TaskAnnotatorBacktraceIntegrationTest::VerifyTraceAndPost,
               Unretained(this), main_thread_a, location_a0, location_a1,
               ExpectedTrace({}), 0, std::move(task_a1));

  main_thread_a->PostTask(location_a0, std::move(task_a0));

  run_loop.Run();
}

// Ensure nesting doesn't break the chain.
TEST_F(TaskAnnotatorBacktraceIntegrationTest, SingleThreadedNested) {
  test::TaskEnvironment task_environment;
  uint32_t dummy_ipc_hash = 0xDEADBEEF;
  uint32_t dummy_ipc_hash1 = 0xBAADF00D;
  uint32_t dummy_ipc_hash2 = 0x900DD099;
  const Location location0 = FROM_HERE;
  const Location location1 = FROM_HERE;
  const Location location2 = FROM_HERE;
  const Location location3 = FROM_HERE;
  const Location location4 = FROM_HERE;
  const Location location5 = FROM_HERE;

  RunLoop run_loop;

  // Task execution below looks like this, w.r.t. to RunLoop depths:
  // 1 : T0 \ + NRL1 \                                 ---------> T4 -> T5
  // 2 :     ---------> T1 \ -> NRL2 \ ----> T2 -> T3 / + Quit /
  // 3 :                    ---------> DN /

  // NRL1 tests that tasks that occur at a different nesting depth than their
  // parent have a sane backtrace nonetheless (both ways).

  // NRL2 tests that posting T2 right after exiting the RunLoop (from the same
  // task) results in NRL2 being its parent (and not the DoNothing() task that
  // just ran -- which would have been the case if the "current task" wasn't
  // restored properly when returning from a task within a task).

  // In other words, this is regression test for a bug in the previous
  // implementation. In the current implementation, replacing
  //   tls_for_current_pending_task->Set(previous_pending_task);
  // by
  //   tls_for_current_pending_task->Set(nullptr);
  // at the end of TaskAnnotator::RunTask() makes this test fail.

  // This test also validates the IPC contexts are propagated appropriately, and
  // then a context in an outer loop does not color tasks posted from a nested
  // loop.

  RunLoop nested_run_loop1(RunLoop::Type::kNestableTasksAllowed);

  // Expectations are the same as in SingleThreadedSimple test despite the
  // nested loop starting between tasks 0 and 1 and stopping between tasks 3 and
  // 4.
  OnceClosure task5 = BindOnce(
      &TaskAnnotatorBacktraceIntegrationTest::VerifyTraceAndPost,
      Unretained(this), ThreadTaskRunnerHandle::Get(), location5, FROM_HERE,
      ExpectedTrace({location4.program_counter(), location3.program_counter(),
                     location2.program_counter(), location1.program_counter()}),
      dummy_ipc_hash, run_loop.QuitClosure());
  OnceClosure task4 = BindOnce(
      &TaskAnnotatorBacktraceIntegrationTest::VerifyTraceAndPost,
      Unretained(this), ThreadTaskRunnerHandle::Get(), location4, location5,
      ExpectedTrace({location3.program_counter(), location2.program_counter(),
                     location1.program_counter(), location0.program_counter()}),
      dummy_ipc_hash, std::move(task5));
  OnceClosure task3 = BindOnce(
      &TaskAnnotatorBacktraceIntegrationTest::VerifyTraceAndPost,
      Unretained(this), ThreadTaskRunnerHandle::Get(), location3, location4,
      ExpectedTrace({location2.program_counter(), location1.program_counter(),
                     location0.program_counter()}),
      dummy_ipc_hash, std::move(task4));

  OnceClosure run_task_3_then_quit_nested_loop1 =
      BindOnce(&TaskAnnotatorBacktraceIntegrationTest::RunTwo, std::move(task3),
               nested_run_loop1.QuitClosure());

  OnceClosure task2 = BindOnce(
      &TaskAnnotatorBacktraceIntegrationTest::VerifyTraceAndPost,
      Unretained(this), ThreadTaskRunnerHandle::Get(), location2, location3,
      ExpectedTrace({location1.program_counter(), location0.program_counter()}),
      dummy_ipc_hash, std::move(run_task_3_then_quit_nested_loop1));

  // Task 1 is custom. It enters another nested RunLoop, has it do work and exit
  // before posting the next task. This confirms that |task1| is restored as the
  // current task before posting |task2| after returning from the nested loop.
  RunLoop nested_run_loop2(RunLoop::Type::kNestableTasksAllowed);
  OnceClosure task1 = BindOnce(
      BindLambdaForTesting([dummy_ipc_hash1](RunLoop* nested_run_loop,
                                             const Location& location2,
                                             OnceClosure task2) {
        {
          // Run the nested message loop with an explicitly set IPC context.
          // This context should not leak out of the inner loop and color the
          // tasks in the outer loop.
          TaskAnnotator::ScopedSetIpcHash scoped_ipc_hash(dummy_ipc_hash1);
          ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, DoNothing());
          nested_run_loop->RunUntilIdle();
        }
        ThreadTaskRunnerHandle::Get()->PostTask(location2, std::move(task2));
      }),
      Unretained(&nested_run_loop2), location2, std::move(task2));

  OnceClosure task0 = BindOnce(
      &TaskAnnotatorBacktraceIntegrationTest::VerifyTraceAndPostWithIpcContext,
      Unretained(this), ThreadTaskRunnerHandle::Get(), location0, location1,
      ExpectedTrace({}), 0, std::move(task1), dummy_ipc_hash);

  ThreadTaskRunnerHandle::Get()->PostTask(location0, std::move(task0));

  {
    TaskAnnotator::ScopedSetIpcHash scoped_ipc_hash(dummy_ipc_hash2);
    ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, BindOnce(&RunLoop::Run, Unretained(&nested_run_loop1)));
  }

  run_loop.Run();
}

}  // namespace base
