// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"

#include <functional>
#include <memory>
#include <utility>

#include "base/containers/queue.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker_impl.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

void QuitWhenIdleTask(RunLoop* run_loop, int* counter) {
  run_loop->QuitWhenIdle();
  ++(*counter);
}

void RunNestedLoopTask(int* counter) {
  RunLoop nested_run_loop(RunLoop::Type::kNestableTasksAllowed);

  // This task should quit |nested_run_loop| but not the main RunLoop.
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, BindOnce(&QuitWhenIdleTask, Unretained(&nested_run_loop),
                          Unretained(counter)));

  SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, MakeExpectedNotRunClosure(FROM_HERE), Days(1));

  nested_run_loop.Run();

  ++(*counter);
}

// A simple SingleThreadTaskRunner that just queues undelayed tasks (and ignores
// delayed tasks). Tasks can then be processed one by one by ProcessTask() which
// will return true if it processed a task and false otherwise.
class SimpleSingleThreadTaskRunner : public SingleThreadTaskRunner {
 public:
  SimpleSingleThreadTaskRunner() = default;
  SimpleSingleThreadTaskRunner(const SimpleSingleThreadTaskRunner&) = delete;
  SimpleSingleThreadTaskRunner& operator=(const SimpleSingleThreadTaskRunner&) =
      delete;

  bool PostDelayedTask(const Location& from_here,
                       OnceClosure task,
                       base::TimeDelta delay) override {
    if (delay.is_positive())
      return false;
    AutoLock auto_lock(tasks_lock_);
    pending_tasks_.push(std::move(task));
    return true;
  }

  bool PostNonNestableDelayedTask(const Location& from_here,
                                  OnceClosure task,
                                  base::TimeDelta delay) override {
    return PostDelayedTask(from_here, std::move(task), delay);
  }

  bool RunsTasksInCurrentSequence() const override {
    return origin_thread_checker_.CalledOnValidThread();
  }

  bool ProcessSingleTask() {
    OnceClosure task;
    {
      AutoLock auto_lock(tasks_lock_);
      if (pending_tasks_.empty())
        return false;
      task = std::move(pending_tasks_.front());
      pending_tasks_.pop();
    }
    // It's important to Run() after pop() and outside the lock as |task| may
    // run a nested loop which will re-enter ProcessSingleTask().
    std::move(task).Run();
    return true;
  }

 private:
  ~SimpleSingleThreadTaskRunner() override = default;

  Lock tasks_lock_;
  base::queue<OnceClosure> pending_tasks_;

  // RunLoop relies on RunsTasksInCurrentSequence() signal. Use a
  // ThreadCheckerImpl to be able to reliably provide that signal even in
  // non-dcheck builds.
  ThreadCheckerImpl origin_thread_checker_;
};

// The basis of all TestDelegates, allows safely injecting a OnceClosure to be
// run in the next idle phase of this delegate's Run() implementation. This can
// be used to have code run on a thread that is otherwise livelocked in an idle
// phase (sometimes a simple PostTask() won't do it -- e.g. when processing
// application tasks is disallowed).
class InjectableTestDelegate : public RunLoop::Delegate {
 public:
  void InjectClosureOnDelegate(OnceClosure closure) {
    AutoLock auto_lock(closure_lock_);
    closure_ = std::move(closure);
  }

  bool RunInjectedClosure() {
    AutoLock auto_lock(closure_lock_);
    if (closure_.is_null())
      return false;
    std::move(closure_).Run();
    return true;
  }

 private:
  Lock closure_lock_;
  OnceClosure closure_;
};

// A simple test RunLoop::Delegate to exercise Runloop logic independent of any
// other base constructs. BindToCurrentThread() must be called before this
// TestBoundDelegate is operational.
class TestBoundDelegate final : public InjectableTestDelegate {
 public:
  TestBoundDelegate() = default;

  // Makes this TestBoundDelegate become the RunLoop::Delegate and
  // SingleThreadTaskRunner::CurrentDefaultHandle for this thread.
  void BindToCurrentThread() {
    thread_task_runner_handle_ =
        std::make_unique<SingleThreadTaskRunner::CurrentDefaultHandle>(
            simple_task_runner_);
    RunLoop::RegisterDelegateForCurrentThread(this);
  }

 private:
  void Run(bool application_tasks_allowed, TimeDelta timeout) override {
    if (nested_run_allowing_tasks_incoming_) {
      EXPECT_TRUE(RunLoop::IsNestedOnCurrentThread());
      EXPECT_TRUE(application_tasks_allowed);
    } else if (RunLoop::IsNestedOnCurrentThread()) {
      EXPECT_FALSE(application_tasks_allowed);
    }
    nested_run_allowing_tasks_incoming_ = false;

    while (!should_quit_) {
      if (application_tasks_allowed && simple_task_runner_->ProcessSingleTask())
        continue;

      if (ShouldQuitWhenIdle())
        break;

      if (RunInjectedClosure())
        continue;

      PlatformThread::YieldCurrentThread();
    }
    should_quit_ = false;
  }

  void Quit() override { should_quit_ = true; }

  void EnsureWorkScheduled() override {
    nested_run_allowing_tasks_incoming_ = true;
  }

  // True if the next invocation of Run() is expected to be from a
  // kNestableTasksAllowed RunLoop.
  bool nested_run_allowing_tasks_incoming_ = false;

  scoped_refptr<SimpleSingleThreadTaskRunner> simple_task_runner_ =
      MakeRefCounted<SimpleSingleThreadTaskRunner>();

  std::unique_ptr<SingleThreadTaskRunner::CurrentDefaultHandle>
      thread_task_runner_handle_;

  bool should_quit_ = false;
};

enum class RunLoopTestType {
  // Runs all RunLoopTests under a TaskEnvironment to make sure real world
  // scenarios work.
  kRealEnvironment,

  // Runs all RunLoopTests under a test RunLoop::Delegate to make sure the
  // delegate interface fully works standalone.
  kTestDelegate,
};

// The task environment for the RunLoopTest of a given type. A separate class
// so it can be instantiated on the stack in the RunLoopTest fixture.
class RunLoopTestEnvironment {
 public:
  explicit RunLoopTestEnvironment(RunLoopTestType type) {
    switch (type) {
      case RunLoopTestType::kRealEnvironment: {
        task_environment_ = std::make_unique<test::TaskEnvironment>();
        break;
      }
      case RunLoopTestType::kTestDelegate: {
        auto test_delegate = std::make_unique<TestBoundDelegate>();
        test_delegate->BindToCurrentThread();
        test_delegate_ = std::move(test_delegate);
        break;
      }
    }
  }

 private:
  // Instantiates one or the other based on the RunLoopTestType.
  std::unique_ptr<test::TaskEnvironment> task_environment_;
  std::unique_ptr<InjectableTestDelegate> test_delegate_;
};

class RunLoopTest : public testing::TestWithParam<RunLoopTestType> {
 public:
  RunLoopTest(const RunLoopTest&) = delete;
  RunLoopTest& operator=(const RunLoopTest&) = delete;

 protected:
  RunLoopTest() : test_environment_(GetParam()) {}

  RunLoopTestEnvironment test_environment_;
  RunLoop run_loop_;
};

}  // namespace

TEST_P(RunLoopTest, QuitWhenIdle) {
  int counter = 0;
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, BindOnce(&QuitWhenIdleTask, Unretained(&run_loop_),
                          Unretained(&counter)));
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, MakeExpectedRunClosure(FROM_HERE));
  SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, MakeExpectedNotRunClosure(FROM_HERE), Days(1));

  run_loop_.Run();
  EXPECT_EQ(1, counter);
}

TEST_P(RunLoopTest, QuitWhenIdleNestedLoop) {
  int counter = 0;
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, BindOnce(&RunNestedLoopTask, Unretained(&counter)));
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, BindOnce(&QuitWhenIdleTask, Unretained(&run_loop_),
                          Unretained(&counter)));
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, MakeExpectedRunClosure(FROM_HERE));
  SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, MakeExpectedNotRunClosure(FROM_HERE), Days(1));

  run_loop_.Run();
  EXPECT_EQ(3, counter);
}

TEST_P(RunLoopTest, QuitWhenIdleClosure) {
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop_.QuitWhenIdleClosure());
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, MakeExpectedRunClosure(FROM_HERE));
  SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, MakeExpectedNotRunClosure(FROM_HERE), Days(1));

  run_loop_.Run();
}

// Verify that the QuitWhenIdleClosure() can run after the RunLoop has been
// deleted. It should have no effect.
TEST_P(RunLoopTest, QuitWhenIdleClosureAfterRunLoopScope) {
  RepeatingClosure quit_when_idle_closure;
  {
    RunLoop run_loop;
    quit_when_idle_closure = run_loop.QuitWhenIdleClosure();
    run_loop.RunUntilIdle();
  }
  quit_when_idle_closure.Run();
}

// Verify that Quit can be executed from another sequence.
TEST_P(RunLoopTest, QuitFromOtherSequence) {
  Thread other_thread("test");
  other_thread.Start();
  scoped_refptr<SequencedTaskRunner> other_sequence =
      other_thread.task_runner();

  // Always expected to run before asynchronous Quit() kicks in.
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, MakeExpectedRunClosure(FROM_HERE));

  WaitableEvent loop_was_quit(WaitableEvent::ResetPolicy::MANUAL,
                              WaitableEvent::InitialState::NOT_SIGNALED);
  other_sequence->PostTask(
      FROM_HERE, base::BindOnce([](RunLoop* run_loop) { run_loop->Quit(); },
                                Unretained(&run_loop_)));
  other_sequence->PostTask(
      FROM_HERE,
      base::BindOnce(&WaitableEvent::Signal, base::Unretained(&loop_was_quit)));

  // Anything that's posted after the Quit closure was posted back to this
  // sequence shouldn't get a chance to run.
  loop_was_quit.Wait();
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, MakeExpectedNotRunClosure(FROM_HERE));

  run_loop_.Run();
}

// Verify that QuitClosure can be executed from another sequence.
TEST_P(RunLoopTest, QuitFromOtherSequenceWithClosure) {
  Thread other_thread("test");
  other_thread.Start();
  scoped_refptr<SequencedTaskRunner> other_sequence =
      other_thread.task_runner();

  // Always expected to run before asynchronous Quit() kicks in.
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, MakeExpectedRunClosure(FROM_HERE));

  WaitableEvent loop_was_quit(WaitableEvent::ResetPolicy::MANUAL,
                              WaitableEvent::InitialState::NOT_SIGNALED);
  other_sequence->PostTask(FROM_HERE, run_loop_.QuitClosure());
  other_sequence->PostTask(
      FROM_HERE,
      base::BindOnce(&WaitableEvent::Signal, base::Unretained(&loop_was_quit)));

  // Anything that's posted after the Quit closure was posted back to this
  // sequence shouldn't get a chance to run.
  loop_was_quit.Wait();
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, MakeExpectedNotRunClosure(FROM_HERE));

  run_loop_.Run();
}

// Verify that Quit can be executed from another sequence even when the
// Quit is racing with Run() -- i.e. forgo the WaitableEvent used above.
TEST_P(RunLoopTest, QuitFromOtherSequenceRacy) {
  Thread other_thread("test");
  other_thread.Start();
  scoped_refptr<SequencedTaskRunner> other_sequence =
      other_thread.task_runner();

  // Always expected to run before asynchronous Quit() kicks in.
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, MakeExpectedRunClosure(FROM_HERE));

  other_sequence->PostTask(FROM_HERE, run_loop_.QuitClosure());

  run_loop_.Run();
}

// Verify that QuitClosure can be executed from another sequence even when the
// Quit is racing with Run() -- i.e. forgo the WaitableEvent used above.
TEST_P(RunLoopTest, QuitFromOtherSequenceRacyWithClosure) {
  Thread other_thread("test");
  other_thread.Start();
  scoped_refptr<SequencedTaskRunner> other_sequence =
      other_thread.task_runner();

  // Always expected to run before asynchronous Quit() kicks in.
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, MakeExpectedRunClosure(FROM_HERE));

  other_sequence->PostTask(FROM_HERE, run_loop_.QuitClosure());

  run_loop_.Run();
}

// Verify that QuitWhenIdle can be executed from another sequence.
TEST_P(RunLoopTest, QuitWhenIdleFromOtherSequence) {
  Thread other_thread("test");
  other_thread.Start();
  scoped_refptr<SequencedTaskRunner> other_sequence =
      other_thread.task_runner();

  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, MakeExpectedRunClosure(FROM_HERE));

  other_sequence->PostTask(
      FROM_HERE,
      base::BindOnce([](RunLoop* run_loop) { run_loop->QuitWhenIdle(); },
                     Unretained(&run_loop_)));

  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, MakeExpectedRunClosure(FROM_HERE));

  run_loop_.Run();

  // Regardless of the outcome of the race this thread shouldn't have been idle
  // until both tasks posted to this sequence have run.
}

// Verify that QuitWhenIdleClosure can be executed from another sequence.
TEST_P(RunLoopTest, QuitWhenIdleFromOtherSequenceWithClosure) {
  Thread other_thread("test");
  other_thread.Start();
  scoped_refptr<SequencedTaskRunner> other_sequence =
      other_thread.task_runner();

  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, MakeExpectedRunClosure(FROM_HERE));

  other_sequence->PostTask(FROM_HERE, run_loop_.QuitWhenIdleClosure());

  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, MakeExpectedRunClosure(FROM_HERE));

  run_loop_.Run();

  // Regardless of the outcome of the race this thread shouldn't have been idle
  // until the both tasks posted to this sequence have run.
}

TEST_P(RunLoopTest, IsRunningOnCurrentThread) {
  EXPECT_FALSE(RunLoop::IsRunningOnCurrentThread());
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      BindOnce([] { EXPECT_TRUE(RunLoop::IsRunningOnCurrentThread()); }));
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop_.QuitClosure());
  run_loop_.Run();
}

TEST_P(RunLoopTest, IsNestedOnCurrentThread) {
  EXPECT_FALSE(RunLoop::IsNestedOnCurrentThread());

  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, BindOnce([] {
        EXPECT_FALSE(RunLoop::IsNestedOnCurrentThread());

        RunLoop nested_run_loop(RunLoop::Type::kNestableTasksAllowed);

        SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE,
            BindOnce([] { EXPECT_TRUE(RunLoop::IsNestedOnCurrentThread()); }));
        SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, nested_run_loop.QuitClosure());

        EXPECT_FALSE(RunLoop::IsNestedOnCurrentThread());
        nested_run_loop.Run();
        EXPECT_FALSE(RunLoop::IsNestedOnCurrentThread());
      }));

  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop_.QuitClosure());
  run_loop_.Run();
}

TEST_P(RunLoopTest, CannotRunMoreThanOnce) {
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop_.QuitClosure());
  run_loop_.Run();
  EXPECT_DCHECK_DEATH({ run_loop_.Run(); });
}

TEST_P(RunLoopTest, CanRunUntilIdleMoreThanOnce) {
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE, DoNothing());
  run_loop_.RunUntilIdle();

  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE, DoNothing());
  run_loop_.RunUntilIdle();
  run_loop_.RunUntilIdle();
}

TEST_P(RunLoopTest, CanRunUntilIdleThenRunIfNotQuit) {
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE, DoNothing());
  run_loop_.RunUntilIdle();

  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop_.QuitClosure());
  run_loop_.Run();
}

TEST_P(RunLoopTest, CannotRunUntilIdleThenRunIfQuit) {
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop_.QuitClosure());
  run_loop_.RunUntilIdle();

  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE, DoNothing());
  EXPECT_DCHECK_DEATH({ run_loop_.Run(); });
}

TEST_P(RunLoopTest, CannotRunAgainIfQuitWhenIdle) {
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop_.QuitWhenIdleClosure());
  run_loop_.RunUntilIdle();

  EXPECT_DCHECK_DEATH({ run_loop_.RunUntilIdle(); });
}

namespace {

class MockNestingObserver : public RunLoop::NestingObserver {
 public:
  MockNestingObserver() = default;
  MockNestingObserver(const MockNestingObserver&) = delete;
  MockNestingObserver& operator=(const MockNestingObserver&) = delete;

  // RunLoop::NestingObserver:
  MOCK_METHOD0(OnBeginNestedRunLoop, void());
  MOCK_METHOD0(OnExitNestedRunLoop, void());
};

class MockTask {
 public:
  MockTask() = default;
  MockTask(const MockTask&) = delete;
  MockTask& operator=(const MockTask&) = delete;

  MOCK_METHOD0(Task, void());
};

}  // namespace

TEST_P(RunLoopTest, NestingObservers) {
  testing::StrictMock<MockNestingObserver> nesting_observer;
  testing::StrictMock<MockTask> mock_task_a;
  testing::StrictMock<MockTask> mock_task_b;

  RunLoop::AddNestingObserverOnCurrentThread(&nesting_observer);

  const RepeatingClosure run_nested_loop = BindRepeating([] {
    RunLoop nested_run_loop(RunLoop::Type::kNestableTasksAllowed);
    SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, nested_run_loop.QuitClosure());
    nested_run_loop.Run();
  });

  // Generate a stack of nested RunLoops. OnBeginNestedRunLoop() is expected
  // when beginning each nesting depth and OnExitNestedRunLoop() is expected
  // when exiting each nesting depth. Each one of these tasks is ahead of the
  // QuitClosures as those are only posted at the end of the queue when
  // |run_nested_loop| is executed.
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                        run_nested_loop);
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&MockTask::Task, base::Unretained(&mock_task_a)));
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                        run_nested_loop);
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&MockTask::Task, base::Unretained(&mock_task_b)));

  {
    testing::InSequence in_sequence;
    EXPECT_CALL(nesting_observer, OnBeginNestedRunLoop());
    EXPECT_CALL(mock_task_a, Task());
    EXPECT_CALL(nesting_observer, OnBeginNestedRunLoop());
    EXPECT_CALL(mock_task_b, Task());
    EXPECT_CALL(nesting_observer, OnExitNestedRunLoop()).Times(2);
  }
  run_loop_.RunUntilIdle();

  RunLoop::RemoveNestingObserverOnCurrentThread(&nesting_observer);
}

TEST_P(RunLoopTest, DisallowRunning) {
  ScopedDisallowRunningRunLoop disallow_running;
  EXPECT_DCHECK_DEATH({ run_loop_.RunUntilIdle(); });
}

TEST_P(RunLoopTest, ExpiredDisallowRunning) {
  { ScopedDisallowRunningRunLoop disallow_running; }
  // Running should be fine after |disallow_running| goes out of scope.
  run_loop_.RunUntilIdle();
}

INSTANTIATE_TEST_SUITE_P(Real,
                         RunLoopTest,
                         testing::Values(RunLoopTestType::kRealEnvironment));
INSTANTIATE_TEST_SUITE_P(Mock,
                         RunLoopTest,
                         testing::Values(RunLoopTestType::kTestDelegate));

TEST(RunLoopDeathTest, MustRegisterBeforeInstantiating) {
  TestBoundDelegate unbound_test_delegate_;
  // RunLoop::RunLoop() should CHECK fetching the
  // SingleThreadTaskRunner::CurrentDefaultHandle.
  EXPECT_DEATH_IF_SUPPORTED({ RunLoop(); }, "");
}

TEST(RunLoopDelegateTest, NestableTasksDontRunInDefaultNestedLoops) {
  TestBoundDelegate test_delegate;
  test_delegate.BindToCurrentThread();

  base::Thread other_thread("test");
  other_thread.Start();

  RunLoop main_loop;
  // A nested run loop which isn't kNestableTasksAllowed.
  RunLoop nested_run_loop(RunLoop::Type::kDefault);

  bool nested_run_loop_ended = false;

  // The first task on the main loop will result in a nested run loop. Since
  // it's not kNestableTasksAllowed, no further task should be processed until
  // it's quit.
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      BindOnce([](RunLoop* nested_run_loop) { nested_run_loop->Run(); },
               Unretained(&nested_run_loop)));

  // Post a task that will fail if it runs inside the nested run loop.
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      BindOnce(
          [](const bool& nested_run_loop_ended,
             OnceClosure continuation_callback) {
            EXPECT_TRUE(nested_run_loop_ended);
            EXPECT_FALSE(RunLoop::IsNestedOnCurrentThread());
            std::move(continuation_callback).Run();
          },
          std::cref(nested_run_loop_ended), main_loop.QuitClosure()));

  // Post a task flipping the boolean bit for extra verification right before
  // quitting |nested_run_loop|.
  other_thread.task_runner()->PostDelayedTask(
      FROM_HERE,
      BindOnce(
          [](bool* nested_run_loop_ended) {
            EXPECT_FALSE(*nested_run_loop_ended);
            *nested_run_loop_ended = true;
          },
          Unretained(&nested_run_loop_ended)),
      TestTimeouts::tiny_timeout());
  // Post an async delayed task to exit the run loop when idle. This confirms
  // that (1) the test task only ran in the main loop after the nested loop
  // exited and (2) the nested run loop actually considers itself idle while
  // spinning. Note: The quit closure needs to be injected directly on the
  // delegate as invoking QuitWhenIdle() off-thread results in a thread bounce
  // which will not processed because of the very logic under test (nestable
  // tasks don't run in |nested_run_loop|).
  other_thread.task_runner()->PostDelayedTask(
      FROM_HERE,
      BindOnce(
          [](TestBoundDelegate* test_delegate, OnceClosure injected_closure) {
            test_delegate->InjectClosureOnDelegate(std::move(injected_closure));
          },
          Unretained(&test_delegate), nested_run_loop.QuitWhenIdleClosure()),
      TestTimeouts::tiny_timeout());

  main_loop.Run();
}

}  // namespace base
