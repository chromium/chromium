// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/sequence_manager/thread_controller_with_message_pump_impl.h"

#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/single_thread_task_runner.h"
#include "base/task/sequence_manager/thread_controller_power_monitor.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using testing::_;
using testing::Invoke;
using testing::ElementsAre;

namespace base {
namespace sequence_manager {

namespace {

class ThreadControllerForTest
    : public internal::ThreadControllerWithMessagePumpImpl {
 public:
  ThreadControllerForTest(std::unique_ptr<MessagePump> pump,
                          const SequenceManager::Settings& settings)
      : ThreadControllerWithMessagePumpImpl(std::move(pump), settings) {}

  ~ThreadControllerForTest() override {
    if (trace_observer)
      RunLevelTracker::SetTraceObserverForTesting(nullptr);
  }

  using ThreadControllerWithMessagePumpImpl::BeforeWait;
  using ThreadControllerWithMessagePumpImpl::DoIdleWork;
  using ThreadControllerWithMessagePumpImpl::DoWork;
  using ThreadControllerWithMessagePumpImpl::EnsureWorkScheduled;
  using ThreadControllerWithMessagePumpImpl::OnBeginWorkItem;
  using ThreadControllerWithMessagePumpImpl::OnEndWorkItem;
  using ThreadControllerWithMessagePumpImpl::Quit;
  using ThreadControllerWithMessagePumpImpl::Run;

  using ThreadControllerWithMessagePumpImpl::MainThreadOnlyForTesting;
  using ThreadControllerWithMessagePumpImpl::
      ThreadControllerPowerMonitorForTesting;

  class MockTraceObserver : public internal::ThreadController::RunLevelTracker::
                                TraceObserverForTesting {
   public:
    MOCK_METHOD0(OnThreadControllerActiveBegin, void());
    MOCK_METHOD0(OnThreadControllerActiveEnd, void());
  };

  void InstallTraceObserver() {
    trace_observer.emplace();
    RunLevelTracker::SetTraceObserverForTesting(&trace_observer.value());
  }

  // Optionally emplaced, strict from then on.
  absl::optional<testing::StrictMock<MockTraceObserver>> trace_observer;
};

class MockMessagePump : public MessagePump {
 public:
  MockMessagePump() {}
  ~MockMessagePump() override {}

  MOCK_METHOD1(Run, void(MessagePump::Delegate*));
  MOCK_METHOD0(Quit, void());
  MOCK_METHOD0(ScheduleWork, void());
  MOCK_METHOD1(ScheduleDelayedWork, void(const TimeTicks&));
  MOCK_METHOD1(SetTimerSlack, void(TimerSlack));
};

// TODO(crbug.com/901373): Deduplicate FakeTaskRunners.
class FakeTaskRunner : public SingleThreadTaskRunner {
 public:
  bool PostDelayedTask(const Location& from_here,
                       OnceClosure task,
                       TimeDelta delay) override {
    return true;
  }

  bool PostNonNestableDelayedTask(const Location& from_here,
                                  OnceClosure task,
                                  TimeDelta delay) override {
    return true;
  }

  bool RunsTasksInCurrentSequence() const override { return true; }

 protected:
  ~FakeTaskRunner() override = default;
};

class FakeSequencedTaskSource : public internal::SequencedTaskSource {
 public:
  explicit FakeSequencedTaskSource(TickClock* clock) : clock_(clock) {}
  ~FakeSequencedTaskSource() override = default;

  Task* SelectNextTask(SelectTaskOption option) override {
    if (tasks_.empty())
      return nullptr;
    if (tasks_.front().delayed_run_time > clock_->NowTicks())
      return nullptr;
    if (option == SequencedTaskSource::SelectTaskOption::kSkipDelayedTask &&
        !tasks_.front().delayed_run_time.is_null()) {
      return nullptr;
    }
    running_stack_.push_back(std::move(tasks_.front()));
    tasks_.pop();
    return &running_stack_.back();
  }

  void DidRunTask() override { running_stack_.pop_back(); }

  TimeDelta DelayTillNextTask(LazyNow* lazy_now,
                              SelectTaskOption option) const override {
    if (tasks_.empty())
      return TimeDelta::Max();
    if (option == SequencedTaskSource::SelectTaskOption::kSkipDelayedTask &&
        !tasks_.front().delayed_run_time.is_null()) {
      return TimeDelta::Max();
    }
    if (tasks_.front().delayed_run_time.is_null())
      return TimeDelta();
    if (lazy_now->Now() > tasks_.front().delayed_run_time)
      return TimeDelta();
    return tasks_.front().delayed_run_time - lazy_now->Now();
  }

  void AddTask(Location posted_from,
               OnceClosure task,
               TimeTicks delayed_run_time) {
    DCHECK(tasks_.empty() || delayed_run_time.is_null() ||
           tasks_.back().delayed_run_time < delayed_run_time);
    tasks_.push(
        Task(internal::PostedTask(nullptr, std::move(task), posted_from),
             delayed_run_time, EnqueueOrder::FromIntForTesting(13)));
  }

  bool HasPendingHighResolutionTasks() override {
    return has_pending_high_resolution_tasks;
  }

  void SetHasPendingHighResolutionTasks(bool state) {
    has_pending_high_resolution_tasks = state;
  }

  bool OnSystemIdle() override { return false; }

 private:
  TickClock* clock_;
  std::queue<Task> tasks_;
  std::vector<Task> running_stack_;
  bool has_pending_high_resolution_tasks = false;
};

TimeTicks Seconds(int seconds) {
  return TimeTicks() + TimeDelta::FromSeconds(seconds);
}

TimeTicks Days(int seconds) {
  return TimeTicks() + TimeDelta::FromDays(seconds);
}

}  // namespace

class ThreadControllerWithMessagePumpTest : public testing::Test {
 public:
  ThreadControllerWithMessagePumpTest()
      : message_pump_(new testing::StrictMock<MockMessagePump>()),
        settings_(
            SequenceManager::Settings::Builder().SetTickClock(&clock_).Build()),
        thread_controller_(std::unique_ptr<MessagePump>(message_pump_),
                           settings_),
        task_source_(&clock_) {
    thread_controller_.SetWorkBatchSize(1);
    thread_controller_.SetSequencedTaskSource(&task_source_);
  }

  void SetUp() override {
    internal::ThreadControllerPowerMonitor::OverrideUsePowerMonitorForTesting(
        true);
  }

  void TearDown() override {
    internal::ThreadControllerPowerMonitor::ResetForTesting();
  }

 protected:
  MockMessagePump* message_pump_;
  SequenceManager::Settings settings_;
  SimpleTestTickClock clock_;
  ThreadControllerForTest thread_controller_;
  FakeSequencedTaskSource task_source_;
};

TEST_F(ThreadControllerWithMessagePumpTest, ScheduleDelayedWork) {
  MockCallback<OnceClosure> task1;
  task_source_.AddTask(FROM_HERE, task1.Get(), Seconds(10));
  MockCallback<OnceClosure> task2;
  task_source_.AddTask(FROM_HERE, task2.Get(), TimeTicks());
  MockCallback<OnceClosure> task3;
  task_source_.AddTask(FROM_HERE, task3.Get(), Seconds(20));

  // Call a no-op DoWork. Expect that it doesn't do any work.
  clock_.SetNowTicks(Seconds(5));
  EXPECT_CALL(*message_pump_, ScheduleDelayedWork(_)).Times(0);
  {
    auto next_work_info = thread_controller_.DoWork();
    EXPECT_FALSE(next_work_info.is_immediate());
    EXPECT_EQ(next_work_info.delayed_run_time, Seconds(10));
  }
  testing::Mock::VerifyAndClearExpectations(message_pump_);

  // Call DoWork after the expiration of the delay.
  // Expect that |task1| runs and the return value indicates that |task2| can
  // run immediately.
  clock_.SetNowTicks(Seconds(11));
  EXPECT_CALL(task1, Run()).Times(1);
  {
    auto next_work_info = thread_controller_.DoWork();
    EXPECT_TRUE(next_work_info.is_immediate());
  }
  testing::Mock::VerifyAndClearExpectations(&task1);

  // Call DoWork. Expect |task2| to be run and the delayed run time of
  // |task3| to be returned.
  EXPECT_CALL(task2, Run()).Times(1);
  {
    auto next_work_info = thread_controller_.DoWork();
    EXPECT_FALSE(next_work_info.is_immediate());
    EXPECT_EQ(next_work_info.delayed_run_time, Seconds(20));
  }
  testing::Mock::VerifyAndClearExpectations(&task2);

  // Call DoWork for the last task and expect to be told
  // about the lack of further delayed work (next run time being TimeTicks()).
  clock_.SetNowTicks(Seconds(21));
  EXPECT_CALL(task3, Run()).Times(1);
  {
    auto next_work_info = thread_controller_.DoWork();
    EXPECT_FALSE(next_work_info.is_immediate());
    EXPECT_EQ(next_work_info.delayed_run_time, TimeTicks::Max());
  }
  testing::Mock::VerifyAndClearExpectations(&task3);
}

TEST_F(ThreadControllerWithMessagePumpTest, SetNextDelayedDoWork) {
  EXPECT_CALL(*message_pump_, ScheduleDelayedWork(Seconds(123)));

  LazyNow lazy_now(&clock_);
  thread_controller_.SetNextDelayedDoWork(&lazy_now, Seconds(123));
}

TEST_F(ThreadControllerWithMessagePumpTest, SetNextDelayedDoWork_CapAtOneDay) {
  EXPECT_CALL(*message_pump_, ScheduleDelayedWork(Days(1)));

  LazyNow lazy_now(&clock_);
  thread_controller_.SetNextDelayedDoWork(&lazy_now, Days(2));
}

TEST_F(ThreadControllerWithMessagePumpTest, DelayedWork_CapAtOneDay) {
  MockCallback<OnceClosure> task1;
  task_source_.AddTask(FROM_HERE, task1.Get(), Days(10));

  auto next_work_info = thread_controller_.DoWork();
  EXPECT_EQ(next_work_info.delayed_run_time, Days(1));
}

TEST_F(ThreadControllerWithMessagePumpTest, DoWorkDoesntScheduleDelayedWork) {
  MockCallback<OnceClosure> task1;
  task_source_.AddTask(FROM_HERE, task1.Get(), Seconds(10));

  EXPECT_CALL(*message_pump_, ScheduleDelayedWork(_)).Times(0);
  auto next_work_info = thread_controller_.DoWork();
  EXPECT_EQ(next_work_info.delayed_run_time, Seconds(10));
}

TEST_F(ThreadControllerWithMessagePumpTest, NestedExecution) {
  // This test posts three immediate tasks. The first creates a nested RunLoop
  // and the test expects that the second and third tasks are run outside of
  // the nested loop.
  std::vector<std::string> log;

  ThreadTaskRunnerHandle handle(MakeRefCounted<FakeTaskRunner>());

  EXPECT_CALL(*message_pump_, Run(_))
      .WillOnce(Invoke([&log, this](MessagePump::Delegate* delegate) {
        log.push_back("entering top-level runloop");
        EXPECT_EQ(delegate, &thread_controller_);
        EXPECT_TRUE(delegate->DoWork().is_immediate());
        EXPECT_TRUE(delegate->DoWork().is_immediate());
        EXPECT_EQ(delegate->DoWork().delayed_run_time, TimeTicks::Max());
        log.push_back("exiting top-level runloop");
      }))
      .WillOnce(Invoke([&log, this](MessagePump::Delegate* delegate) {
        log.push_back("entering nested runloop");
        EXPECT_EQ(delegate, &thread_controller_);
        EXPECT_FALSE(thread_controller_.IsTaskExecutionAllowed());
        EXPECT_EQ(delegate->DoWork().delayed_run_time, TimeTicks::Max());
        log.push_back("exiting nested runloop");
      }));

  task_source_.AddTask(FROM_HERE,
                       base::BindOnce(
                           [](std::vector<std::string>* log,
                              ThreadControllerForTest* controller) {
                             EXPECT_FALSE(controller->IsTaskExecutionAllowed());
                             log->push_back("task1");
                             RunLoop().Run();
                           },
                           &log, &thread_controller_),
                       TimeTicks());
  task_source_.AddTask(FROM_HERE,
                       base::BindOnce(
                           [](std::vector<std::string>* log,
                              ThreadControllerForTest* controller) {
                             EXPECT_FALSE(controller->IsTaskExecutionAllowed());
                             log->push_back("task2");
                           },
                           &log, &thread_controller_),
                       TimeTicks());
  task_source_.AddTask(FROM_HERE,
                       base::BindOnce(
                           [](std::vector<std::string>* log,
                              ThreadControllerForTest* controller) {
                             EXPECT_FALSE(controller->IsTaskExecutionAllowed());
                             log->push_back("task3");
                           },
                           &log, &thread_controller_),
                       TimeTicks());

  EXPECT_TRUE(thread_controller_.IsTaskExecutionAllowed());
  RunLoop().Run();

  EXPECT_THAT(log,
              ElementsAre("entering top-level runloop", "task1",
                          "entering nested runloop", "exiting nested runloop",
                          "task2", "task3", "exiting top-level runloop"));
  testing::Mock::VerifyAndClearExpectations(message_pump_);
}

TEST_F(ThreadControllerWithMessagePumpTest,
       NestedExecutionWithApplicationTasks) {
  // This test is similar to the previous one, but execution is explicitly
  // allowed (by specifying appropriate RunLoop type), and tasks are run inside
  // nested runloop.
  std::vector<std::string> log;

  ThreadTaskRunnerHandle handle(MakeRefCounted<FakeTaskRunner>());

  EXPECT_CALL(*message_pump_, Run(_))
      .WillOnce(Invoke([&log, this](MessagePump::Delegate* delegate) {
        log.push_back("entering top-level runloop");
        EXPECT_EQ(delegate, &thread_controller_);
        EXPECT_EQ(delegate->DoWork().delayed_run_time, TimeTicks::Max());
        log.push_back("exiting top-level runloop");
      }))
      .WillOnce(Invoke([&log, this](MessagePump::Delegate* delegate) {
        log.push_back("entering nested runloop");
        EXPECT_EQ(delegate, &thread_controller_);
        EXPECT_TRUE(thread_controller_.IsTaskExecutionAllowed());
        EXPECT_TRUE(delegate->DoWork().is_immediate());
        EXPECT_EQ(delegate->DoWork().delayed_run_time, TimeTicks::Max());
        log.push_back("exiting nested runloop");
      }));

  task_source_.AddTask(
      FROM_HERE,
      base::BindOnce(
          [](std::vector<std::string>* log,
             ThreadControllerForTest* controller) {
            EXPECT_FALSE(controller->IsTaskExecutionAllowed());
            log->push_back("task1");
            RunLoop(RunLoop::Type::kNestableTasksAllowed).Run();
          },
          &log, &thread_controller_),
      TimeTicks());
  task_source_.AddTask(FROM_HERE,
                       base::BindOnce(
                           [](std::vector<std::string>* log,
                              ThreadControllerForTest* controller) {
                             EXPECT_FALSE(controller->IsTaskExecutionAllowed());
                             log->push_back("task2");
                           },
                           &log, &thread_controller_),
                       TimeTicks());
  task_source_.AddTask(FROM_HERE,
                       base::BindOnce(
                           [](std::vector<std::string>* log,
                              ThreadControllerForTest* controller) {
                             EXPECT_FALSE(controller->IsTaskExecutionAllowed());
                             log->push_back("task3");
                           },
                           &log, &thread_controller_),
                       TimeTicks());

  EXPECT_TRUE(thread_controller_.IsTaskExecutionAllowed());
  RunLoop().Run();

  EXPECT_THAT(
      log, ElementsAre("entering top-level runloop", "task1",
                       "entering nested runloop", "task2", "task3",
                       "exiting nested runloop", "exiting top-level runloop"));
  testing::Mock::VerifyAndClearExpectations(message_pump_);
}

TEST_F(ThreadControllerWithMessagePumpTest, SetDefaultTaskRunner) {
  scoped_refptr<SingleThreadTaskRunner> task_runner1 =
      MakeRefCounted<FakeTaskRunner>();
  thread_controller_.SetDefaultTaskRunner(task_runner1);
  EXPECT_EQ(task_runner1, ThreadTaskRunnerHandle::Get());

  // Check that we are correctly supporting overriding.
  scoped_refptr<SingleThreadTaskRunner> task_runner2 =
      MakeRefCounted<FakeTaskRunner>();
  thread_controller_.SetDefaultTaskRunner(task_runner2);
  EXPECT_EQ(task_runner2, ThreadTaskRunnerHandle::Get());
}

TEST_F(ThreadControllerWithMessagePumpTest, EnsureWorkScheduled) {
  task_source_.AddTask(FROM_HERE, DoNothing(), TimeTicks());

  // Ensure that the first ScheduleWork() call results in the pump being called.
  EXPECT_CALL(*message_pump_, ScheduleWork());
  thread_controller_.ScheduleWork();
  testing::Mock::VerifyAndClearExpectations(message_pump_);

  // Ensure that the subsequent ScheduleWork() does not call the pump.
  thread_controller_.ScheduleWork();
  testing::Mock::VerifyAndClearExpectations(message_pump_);

  // EnsureWorkScheduled() doesn't need to do anything because there's a pending
  // DoWork.
  EXPECT_CALL(*message_pump_, ScheduleWork()).Times(0);
  thread_controller_.EnsureWorkScheduled();
  testing::Mock::VerifyAndClearExpectations(message_pump_);

  EXPECT_EQ(thread_controller_.DoWork().delayed_run_time, TimeTicks::Max());

  // EnsureWorkScheduled() calls the pump because there's no pending DoWork.
  EXPECT_CALL(*message_pump_, ScheduleWork()).Times(1);
  thread_controller_.EnsureWorkScheduled();
  testing::Mock::VerifyAndClearExpectations(message_pump_);
}

TEST_F(ThreadControllerWithMessagePumpTest, WorkBatching) {
  ThreadTaskRunnerHandle handle(MakeRefCounted<FakeTaskRunner>());

  const int kBatchSize = 5;
  thread_controller_.SetWorkBatchSize(kBatchSize);

  int task_count = 0;
  EXPECT_CALL(*message_pump_, Run(_))
      .WillOnce(Invoke([&](MessagePump::Delegate* delegate) {
        EXPECT_EQ(delegate->DoWork().delayed_run_time, TimeTicks::Max());
        EXPECT_EQ(5, task_count);
      }));

  for (int i = 0; i < kBatchSize; i++) {
    task_source_.AddTask(FROM_HERE, BindLambdaForTesting([&] { task_count++; }),
                         TimeTicks());
  }

  RunLoop run_loop;
  run_loop.Run();
  testing::Mock::VerifyAndClearExpectations(message_pump_);
}

TEST_F(ThreadControllerWithMessagePumpTest, QuitInterruptsBatch) {
  // This check ensures that RunLoop::Quit() makes us drop back to a work batch
  // size of 1.
  ThreadTaskRunnerHandle handle(MakeRefCounted<FakeTaskRunner>());

  const int kBatchSize = 5;
  thread_controller_.SetWorkBatchSize(kBatchSize);

  int task_count = 0;
  EXPECT_CALL(*message_pump_, Run(_))
      .WillOnce(Invoke([&](MessagePump::Delegate* delegate) {
        EXPECT_EQ(delegate->DoWork().delayed_run_time, TimeTicks::Max());
        EXPECT_EQ(1, task_count);

        // Somewhat counter-intuitive, but if the pump keeps calling us after
        // Quit(), the delegate should still run tasks as normally. This is
        // needed to support nested OS-level runloops that still pump
        // application tasks (e.g., showing a popup menu on Mac).
        EXPECT_EQ(delegate->DoWork().delayed_run_time, TimeTicks::Max());
        EXPECT_EQ(2, task_count);
        EXPECT_EQ(delegate->DoWork().delayed_run_time, TimeTicks::Max());
        EXPECT_EQ(3, task_count);
      }));
  EXPECT_CALL(*message_pump_, Quit());

  RunLoop run_loop;
  for (int i = 0; i < kBatchSize; i++) {
    task_source_.AddTask(FROM_HERE, BindLambdaForTesting([&] {
                           if (!task_count++)
                             run_loop.Quit();
                         }),
                         TimeTicks());
  }

  run_loop.Run();
  testing::Mock::VerifyAndClearExpectations(message_pump_);
}

TEST_F(ThreadControllerWithMessagePumpTest, PrioritizeYieldingToNative) {
  ThreadTaskRunnerHandle handle(MakeRefCounted<FakeTaskRunner>());

  testing::InSequence sequence;

  RunLoop run_loop;
  auto delayed_time = Seconds(10);
  EXPECT_CALL(*message_pump_, Run(_))
      .WillOnce(Invoke([&](MessagePump::Delegate* delegate) {
        clock_.SetNowTicks(Seconds(5));
        MockCallback<OnceClosure> tasks[5];

        // A: Post 4 application tasks, 3 immediate 1 delayed.
        // B: Run one of them (enter active)
        //   C: Expect we return immediate work item without yield_to_native
        //      (default behaviour).
        // D: Set PrioritizeYieldingToNative until 8 seconds and run second
        //    task.
        //   E: Expect we return immediate work item with yield_to_native.
        // F: Exceed the PrioritizeYieldingToNative deadline and run third task.
        //   G: Expect we return immediate work item without yield_to_native.
        // H: Set PrioritizeYieldingToNative to Max() and run third of them
        //   I: Expect we return a delayed work item with yield_to_native.

        // A:
        task_source_.AddTask(FROM_HERE, tasks[0].Get(), TimeTicks());
        task_source_.AddTask(FROM_HERE, tasks[1].Get(), TimeTicks());
        task_source_.AddTask(FROM_HERE, tasks[2].Get(), TimeTicks());
        task_source_.AddTask(FROM_HERE, tasks[3].Get(), TimeTicks());
        task_source_.AddTask(FROM_HERE, tasks[4].Get(), delayed_time);

        // B:
        EXPECT_CALL(tasks[0], Run());
        auto next_work_item = thread_controller_.DoWork();
        // C:
        EXPECT_EQ(next_work_item.delayed_run_time, TimeTicks());
        EXPECT_FALSE(next_work_item.yield_to_native);

        // D:
        thread_controller_.PrioritizeYieldingToNative(Seconds(8));
        EXPECT_CALL(tasks[1], Run());
        next_work_item = thread_controller_.DoWork();
        // E:
        EXPECT_EQ(next_work_item.delayed_run_time, TimeTicks());
        EXPECT_TRUE(next_work_item.yield_to_native);

        // F:
        clock_.SetNowTicks(Seconds(8));
        EXPECT_CALL(tasks[2], Run());
        next_work_item = thread_controller_.DoWork();
        // G:
        EXPECT_EQ(next_work_item.delayed_run_time, TimeTicks());
        EXPECT_FALSE(next_work_item.yield_to_native);

        // H:
        thread_controller_.PrioritizeYieldingToNative(base::TimeTicks::Max());
        EXPECT_CALL(tasks[3], Run());
        next_work_item = thread_controller_.DoWork();

        // I:
        EXPECT_EQ(next_work_item.delayed_run_time, delayed_time);
        EXPECT_TRUE(next_work_item.yield_to_native);

        EXPECT_FALSE(thread_controller_.DoIdleWork());
      }));

  run_loop.Run();
  testing::Mock::VerifyAndClearExpectations(message_pump_);
}

TEST_F(ThreadControllerWithMessagePumpTest, EarlyQuit) {
  // This test ensures that a opt-of-runloop Quit() (which is possible with some
  // pump implementations) doesn't affect the next RunLoop::Run call.

  ThreadTaskRunnerHandle handle(MakeRefCounted<FakeTaskRunner>());

  std::vector<std::string> log;

  // This quit should be a no-op for future calls.
  EXPECT_CALL(*message_pump_, Quit());
  thread_controller_.Quit();
  testing::Mock::VerifyAndClearExpectations(message_pump_);

  EXPECT_CALL(*message_pump_, Run(_))
      .WillOnce(Invoke([this](MessagePump::Delegate* delegate) {
        EXPECT_EQ(delegate, &thread_controller_);
        EXPECT_TRUE(delegate->DoWork().is_immediate());
        EXPECT_EQ(delegate->DoWork().delayed_run_time, TimeTicks::Max());
      }));

  RunLoop run_loop;

  task_source_.AddTask(
      FROM_HERE,
      base::BindOnce(
          [](std::vector<std::string>* log) { log->push_back("task1"); }, &log),
      TimeTicks());
  task_source_.AddTask(
      FROM_HERE,
      base::BindOnce(
          [](std::vector<std::string>* log) { log->push_back("task2"); }, &log),
      TimeTicks());

  run_loop.RunUntilIdle();

  EXPECT_THAT(log, ElementsAre("task1", "task2"));
  testing::Mock::VerifyAndClearExpectations(message_pump_);
}

TEST_F(ThreadControllerWithMessagePumpTest, NativeNestedMessageLoop) {
  bool did_run = false;
  task_source_.AddTask(
      FROM_HERE, BindLambdaForTesting([&] {
        // Clear expectation set for the non-nested PostTask.
        testing::Mock::VerifyAndClearExpectations(message_pump_);

        EXPECT_FALSE(thread_controller_.IsTaskExecutionAllowed());
        // SetTaskExecutionAllowed(true) should ScheduleWork.
        EXPECT_CALL(*message_pump_, ScheduleWork());
        thread_controller_.SetTaskExecutionAllowed(true);
        testing::Mock::VerifyAndClearExpectations(message_pump_);

        // There's no pending work so the native loop should go
        // idle.
        EXPECT_CALL(*message_pump_, ScheduleWork()).Times(0);
        EXPECT_EQ(thread_controller_.DoWork().delayed_run_time,
                  TimeTicks::Max());
        testing::Mock::VerifyAndClearExpectations(message_pump_);

        // Simulate a native callback which posts a task, this
        // should now ask the pump to ScheduleWork();
        task_source_.AddTask(FROM_HERE, DoNothing(), TimeTicks());
        EXPECT_CALL(*message_pump_, ScheduleWork());
        thread_controller_.ScheduleWork();
        testing::Mock::VerifyAndClearExpectations(message_pump_);

        thread_controller_.SetTaskExecutionAllowed(false);

        // Simulate a subsequent PostTask by the chromium task after
        // we've left the native loop. This should not ScheduleWork
        // on the pump because the ThreadController will do that
        // after this task finishes.
        task_source_.AddTask(FROM_HERE, DoNothing(), TimeTicks());
        EXPECT_CALL(*message_pump_, ScheduleWork()).Times(0);
        thread_controller_.ScheduleWork();

        did_run = true;
      }),
      TimeTicks());

  // Simulate a PostTask that enters a native nested message loop.
  EXPECT_CALL(*message_pump_, ScheduleWork());
  thread_controller_.ScheduleWork();
  EXPECT_TRUE(thread_controller_.DoWork().is_immediate());
  EXPECT_TRUE(did_run);
}

TEST_F(ThreadControllerWithMessagePumpTest, RunWithTimeout) {
  MockCallback<OnceClosure> task1;
  task_source_.AddTask(FROM_HERE, task1.Get(), Seconds(5));
  MockCallback<OnceClosure> task2;
  task_source_.AddTask(FROM_HERE, task2.Get(), Seconds(10));
  MockCallback<OnceClosure> task3;
  task_source_.AddTask(FROM_HERE, task3.Get(), Seconds(20));

  EXPECT_CALL(*message_pump_, Run(_))
      .WillOnce(Invoke([&](MessagePump::Delegate*) {
        clock_.SetNowTicks(Seconds(5));
        EXPECT_CALL(task1, Run()).Times(1);
        EXPECT_EQ(thread_controller_.DoWork().delayed_run_time, Seconds(10));

        clock_.SetNowTicks(Seconds(10));
        EXPECT_CALL(task2, Run()).Times(1);
        EXPECT_EQ(thread_controller_.DoWork().delayed_run_time, Seconds(15));

        clock_.SetNowTicks(Seconds(15));
        EXPECT_CALL(task3, Run()).Times(0);
        EXPECT_EQ(thread_controller_.DoWork().delayed_run_time,
                  TimeTicks::Max());

        EXPECT_CALL(*message_pump_, Quit());
        EXPECT_FALSE(thread_controller_.DoIdleWork());
      }));
  thread_controller_.Run(true, TimeDelta::FromSeconds(15));
}

#if defined(OS_WIN)
TEST_F(ThreadControllerWithMessagePumpTest, SetHighResolutionTimer) {
  MockCallback<OnceClosure> task;
  task_source_.AddTask(FROM_HERE, task.Get(), Seconds(5));

  ThreadTaskRunnerHandle handle(MakeRefCounted<FakeTaskRunner>());

  EXPECT_CALL(*message_pump_, Run(_))
      .WillOnce(Invoke([&](MessagePump::Delegate* delegate) {
        // Should initially not be in high resolution.
        EXPECT_FALSE(
            thread_controller_.MainThreadOnlyForTesting().in_high_res_mode);

        // Ensures timer resolution is set to high resolution.
        task_source_.SetHasPendingHighResolutionTasks(true);
        EXPECT_FALSE(delegate->DoIdleWork());
        EXPECT_TRUE(
            thread_controller_.MainThreadOnlyForTesting().in_high_res_mode);

        // Ensures time resolution is set back to low resolution.
        task_source_.SetHasPendingHighResolutionTasks(false);
        EXPECT_FALSE(delegate->DoIdleWork());
        EXPECT_FALSE(
            thread_controller_.MainThreadOnlyForTesting().in_high_res_mode);

        EXPECT_CALL(*message_pump_, Quit());
        thread_controller_.Quit();
      }));

  RunLoop run_loop;
  run_loop.Run();
}
#endif  // OS_WIN

#if defined(OS_WIN)
TEST_F(ThreadControllerWithMessagePumpTest,
       SetHighResolutionTimerWithPowerSuspend) {
  MockCallback<OnceClosure> task;
  task_source_.AddTask(FROM_HERE, task.Get(), Seconds(5));

  ThreadTaskRunnerHandle handle(MakeRefCounted<FakeTaskRunner>());

  EXPECT_CALL(*message_pump_, Run(_))
      .WillOnce(Invoke([&](MessagePump::Delegate* delegate) {
        // Should initially not be in high resolution.
        EXPECT_FALSE(
            thread_controller_.MainThreadOnlyForTesting().in_high_res_mode);

        // The power suspend notification is sent.
        thread_controller_.ThreadControllerPowerMonitorForTesting()
            ->OnSuspend();

        // The timer resolution should NOT be updated during power suspend.
        task_source_.SetHasPendingHighResolutionTasks(true);
        EXPECT_FALSE(delegate->DoIdleWork());
        EXPECT_FALSE(
            thread_controller_.MainThreadOnlyForTesting().in_high_res_mode);

        // The power resume notification is sent.
        thread_controller_.ThreadControllerPowerMonitorForTesting()->OnResume();

        // Ensures timer resolution is set to high resolution.
        EXPECT_FALSE(delegate->DoIdleWork());
        EXPECT_TRUE(
            thread_controller_.MainThreadOnlyForTesting().in_high_res_mode);

        EXPECT_CALL(*message_pump_, Quit());
        thread_controller_.Quit();
      }));

  RunLoop run_loop;
  run_loop.Run();
}
#endif  // OS_WIN

TEST_F(ThreadControllerWithMessagePumpTest,
       ScheduleDelayedWorkWithPowerSuspend) {
  ThreadTaskRunnerHandle handle(MakeRefCounted<FakeTaskRunner>());

  MockCallback<OnceClosure> task1;
  task_source_.AddTask(FROM_HERE, task1.Get(), Seconds(10));
  MockCallback<OnceClosure> task2;
  task_source_.AddTask(FROM_HERE, task2.Get(), Seconds(15));

  clock_.SetNowTicks(Seconds(5));

  // Call a no-op DoWork. Expect that it doesn't do any work.
  EXPECT_CALL(task1, Run()).Times(0);
  EXPECT_CALL(task2, Run()).Times(0);
  EXPECT_EQ(thread_controller_.DoWork().delayed_run_time, Seconds(10));
  testing::Mock::VerifyAndClearExpectations(&task1);
  testing::Mock::VerifyAndClearExpectations(&task2);

  // Simulate a power suspend.
  thread_controller_.ThreadControllerPowerMonitorForTesting()->OnSuspend();

  // Delayed task is not yet ready to be executed.
  EXPECT_CALL(task1, Run()).Times(0);
  EXPECT_CALL(task2, Run()).Times(0);
  EXPECT_EQ(thread_controller_.DoWork().delayed_run_time, TimeTicks::Max());
  testing::Mock::VerifyAndClearExpectations(&task1);
  testing::Mock::VerifyAndClearExpectations(&task2);

  // Move time after the expiration delay of tasks.
  clock_.SetNowTicks(Seconds(17));

  // Should not process delayed tasks. The process is still in suspended power
  // state.
  EXPECT_CALL(task1, Run()).Times(0);
  EXPECT_CALL(task2, Run()).Times(0);
  EXPECT_EQ(thread_controller_.DoWork().delayed_run_time, TimeTicks::Max());
  testing::Mock::VerifyAndClearExpectations(&task1);
  testing::Mock::VerifyAndClearExpectations(&task2);

  // Simulate a power resume.
  thread_controller_.ThreadControllerPowerMonitorForTesting()->OnResume();

  // No longer in suspended state. Controller should process both delayed tasks.
  EXPECT_CALL(task1, Run()).Times(1);
  EXPECT_CALL(task2, Run()).Times(1);
  EXPECT_TRUE(thread_controller_.DoWork().is_immediate());
  EXPECT_EQ(thread_controller_.DoWork().delayed_run_time, TimeTicks::Max());
  testing::Mock::VerifyAndClearExpectations(&task1);
  testing::Mock::VerifyAndClearExpectations(&task2);
}

TEST_F(ThreadControllerWithMessagePumpTest,
       ThreadControllerActiveSingleApplicationTask) {
  ThreadTaskRunnerHandle handle(MakeRefCounted<FakeTaskRunner>());

  thread_controller_.InstallTraceObserver();

  testing::InSequence sequence;

  RunLoop run_loop;
  EXPECT_CALL(*thread_controller_.trace_observer,
              OnThreadControllerActiveBegin);
  EXPECT_CALL(*message_pump_, Run(_))
      .WillOnce(Invoke([&](MessagePump::Delegate* delegate) {
        // Don't expect a call to OnThreadControllerActiveBegin on the first
        // pass as the Run() call already triggered the active state.
        bool first_pass = true;

        // Post 1 task, run it, go idle, repeat 5 times. Expected to enter/exit
        // "ThreadController active" state 5 consecutive times.
        for (int i = 0; i < 5; ++i, first_pass = false) {
          if (!first_pass) {
            EXPECT_CALL(*thread_controller_.trace_observer,
                        OnThreadControllerActiveBegin);
          }
          MockCallback<OnceClosure> task;
          task_source_.AddTask(FROM_HERE, task.Get(), TimeTicks());
          EXPECT_CALL(task, Run());
          EXPECT_EQ(thread_controller_.DoWork().delayed_run_time,
                    TimeTicks::Max());

          testing::Mock::VerifyAndClearExpectations(
              &*thread_controller_.trace_observer);

          EXPECT_CALL(*thread_controller_.trace_observer,
                      OnThreadControllerActiveEnd);
          EXPECT_FALSE(thread_controller_.DoIdleWork());

          testing::Mock::VerifyAndClearExpectations(
              &*thread_controller_.trace_observer);
        }
      }));

  RunLoop().Run();
}

TEST_F(ThreadControllerWithMessagePumpTest,
       ThreadControllerActiveMultipleApplicationTasks) {
  ThreadTaskRunnerHandle handle(MakeRefCounted<FakeTaskRunner>());

  thread_controller_.InstallTraceObserver();

  testing::InSequence sequence;

  RunLoop run_loop;
  EXPECT_CALL(*thread_controller_.trace_observer,
              OnThreadControllerActiveBegin);
  EXPECT_CALL(*message_pump_, Run(_))
      .WillOnce(Invoke([&](MessagePump::Delegate* delegate) {
        MockCallback<OnceClosure> tasks[5];
        // Post 5 tasks, run them, go idle. Expected to only exit
        // "ThreadController active" state at the end.
        for (auto& t : tasks)
          task_source_.AddTask(FROM_HERE, t.Get(), TimeTicks());
        for (size_t i = 0; i < size(tasks); ++i) {
          const TimeTicks expected_delayed_run_time =
              i < size(tasks) - 1 ? TimeTicks() : TimeTicks::Max();

          EXPECT_CALL(tasks[i], Run());
          EXPECT_EQ(thread_controller_.DoWork().delayed_run_time,
                    expected_delayed_run_time);
        }

        EXPECT_CALL(*thread_controller_.trace_observer,
                    OnThreadControllerActiveEnd);
        EXPECT_FALSE(thread_controller_.DoIdleWork());
      }));

  RunLoop().Run();
}

TEST_F(ThreadControllerWithMessagePumpTest,
       ThreadControllerActiveWakeUpForNothing) {
  ThreadTaskRunnerHandle handle(MakeRefCounted<FakeTaskRunner>());

  thread_controller_.InstallTraceObserver();

  testing::InSequence sequence;

  RunLoop run_loop;
  EXPECT_CALL(*thread_controller_.trace_observer,
              OnThreadControllerActiveBegin);
  EXPECT_CALL(*message_pump_, Run(_))
      .WillOnce(Invoke([&](MessagePump::Delegate* delegate) {
        // Don't expect a call to OnThreadControllerActiveBegin on the first
        // pass as the Run() call already triggered the active state.
        bool first_pass = true;

        // Invoke DoWork with no pending work, go idle, repeat 5 times. Expected
        // to enter/exit "ThreadController active" state 5 consecutive times.
        for (int i = 0; i < 5; ++i, first_pass = false) {
          if (!first_pass) {
            EXPECT_CALL(*thread_controller_.trace_observer,
                        OnThreadControllerActiveBegin);
          }
          EXPECT_EQ(thread_controller_.DoWork().delayed_run_time,
                    TimeTicks::Max());

          testing::Mock::VerifyAndClearExpectations(
              &*thread_controller_.trace_observer);

          EXPECT_CALL(*thread_controller_.trace_observer,
                      OnThreadControllerActiveEnd);
          EXPECT_FALSE(thread_controller_.DoIdleWork());

          testing::Mock::VerifyAndClearExpectations(
              &*thread_controller_.trace_observer);
        }
      }));

  RunLoop().Run();
}

TEST_F(ThreadControllerWithMessagePumpTest,
       ThreadControllerActiveAdvancedNesting) {
  ThreadTaskRunnerHandle handle(MakeRefCounted<FakeTaskRunner>());

  thread_controller_.InstallTraceObserver();

  testing::InSequence sequence;

  RunLoop run_loop;
  EXPECT_CALL(*thread_controller_.trace_observer,
              OnThreadControllerActiveBegin);
  EXPECT_CALL(*message_pump_, Run(_))
      .WillOnce(Invoke([&](MessagePump::Delegate* delegate) {
        MockCallback<OnceClosure> tasks[5];

        // A: Post 2 tasks
        // B: Run one of them (enter active)
        //   C: Enter a nested loop (enter nested active)
        //     D: Run the next task (remain nested active)
        //     E: Go idle (exit active)
        //     F: Post 2 tasks
        //     G: Run one
        //     H: exit nested loop (enter nested active, exit nested active)
        // I: Run the next one, go idle (remain active, exit active)
        // J: Post/run one more task, go idle (enter active, exit active)
        // 😅

        // A:
        task_source_.AddTask(FROM_HERE, tasks[0].Get(), TimeTicks());
        task_source_.AddTask(FROM_HERE, tasks[1].Get(), TimeTicks());

        EXPECT_CALL(tasks[0], Run()).WillOnce(Invoke([]() {
          // C1:
          RunLoop(RunLoop::Type::kNestableTasksAllowed).Run();
        }));
        EXPECT_CALL(*thread_controller_.trace_observer,
                    OnThreadControllerActiveBegin);
        // C2:
        EXPECT_CALL(*message_pump_, Run(_))
            .WillOnce(Invoke([&](MessagePump::Delegate* delegate) {
              // D:
              EXPECT_CALL(tasks[1], Run());
              EXPECT_EQ(thread_controller_.DoWork().delayed_run_time,
                        TimeTicks::Max());
              testing::Mock::VerifyAndClearExpectations(
                  &*thread_controller_.trace_observer);

              // E:
              EXPECT_CALL(*thread_controller_.trace_observer,
                          OnThreadControllerActiveEnd);
              EXPECT_FALSE(thread_controller_.DoIdleWork());
              testing::Mock::VerifyAndClearExpectations(
                  &*thread_controller_.trace_observer);

              // F:
              task_source_.AddTask(FROM_HERE, tasks[2].Get(), TimeTicks());
              task_source_.AddTask(FROM_HERE, tasks[3].Get(), TimeTicks());

              EXPECT_CALL(*thread_controller_.trace_observer,
                          OnThreadControllerActiveBegin);

              // G:
              EXPECT_CALL(tasks[2], Run());
              EXPECT_EQ(thread_controller_.DoWork().delayed_run_time,
                        TimeTicks());
              testing::Mock::VerifyAndClearExpectations(
                  &*thread_controller_.trace_observer);

              // H
              EXPECT_CALL(*thread_controller_.trace_observer,
                          OnThreadControllerActiveEnd);
            }));
        // B:
        EXPECT_EQ(thread_controller_.DoWork().delayed_run_time, TimeTicks());

        // I:
        EXPECT_CALL(tasks[3], Run());
        EXPECT_EQ(thread_controller_.DoWork().delayed_run_time,
                  TimeTicks::Max());
        testing::Mock::VerifyAndClearExpectations(
            &*thread_controller_.trace_observer);

        EXPECT_CALL(*thread_controller_.trace_observer,
                    OnThreadControllerActiveEnd);
        EXPECT_FALSE(thread_controller_.DoIdleWork());
        testing::Mock::VerifyAndClearExpectations(
            &*thread_controller_.trace_observer);

        // J:
        task_source_.AddTask(FROM_HERE, tasks[4].Get(), TimeTicks());
        EXPECT_CALL(*thread_controller_.trace_observer,
                    OnThreadControllerActiveBegin);
        EXPECT_CALL(tasks[4], Run());
        EXPECT_EQ(thread_controller_.DoWork().delayed_run_time,
                  TimeTicks::Max());
        testing::Mock::VerifyAndClearExpectations(
            &*thread_controller_.trace_observer);

        EXPECT_CALL(*thread_controller_.trace_observer,
                    OnThreadControllerActiveEnd);
        EXPECT_FALSE(thread_controller_.DoIdleWork());
        testing::Mock::VerifyAndClearExpectations(
            &*thread_controller_.trace_observer);
      }));

  RunLoop().Run();
}

TEST_F(ThreadControllerWithMessagePumpTest,
       ThreadControllerActiveNestedNativeLoop) {
  ThreadTaskRunnerHandle handle(MakeRefCounted<FakeTaskRunner>());

  thread_controller_.InstallTraceObserver();

  testing::InSequence sequence;

  RunLoop run_loop;
  EXPECT_CALL(*thread_controller_.trace_observer,
              OnThreadControllerActiveBegin);
  EXPECT_CALL(*message_pump_, Run(_))
      .WillOnce(Invoke([&](MessagePump::Delegate* delegate) {
        MockCallback<OnceClosure> tasks[2];

        // A: Post 2 application tasks
        // B: Run one of them which allows nested application tasks (enter
        //    active)
        //   C: Enter a native nested loop
        //     D: Run a native task (enter nested active)
        //     E: Run an application task (remain nested active)
        //     F: Go idle (exit nested active)
        //     G: Run a native task (enter nested active)
        //     H: Exit native nested loop (end nested active)
        // I: Go idle (exit active)

        // A:
        task_source_.AddTask(FROM_HERE, tasks[0].Get(), TimeTicks());
        task_source_.AddTask(FROM_HERE, tasks[1].Get(), TimeTicks());

        EXPECT_CALL(tasks[0], Run()).WillOnce(Invoke([&]() {
          // C:
          EXPECT_FALSE(thread_controller_.IsTaskExecutionAllowed());
          EXPECT_CALL(*message_pump_, ScheduleWork());
          thread_controller_.SetTaskExecutionAllowed(true);
          // i.e. simulate that something runs code within the scope of a
          // ScopedAllowApplicationTasksInNativeNestedLoop and ends up entering
          // a nested native loop which would invoke OnBeginWorkItem()

          // D:
          EXPECT_CALL(*thread_controller_.trace_observer,
                      OnThreadControllerActiveBegin);
          thread_controller_.OnBeginWorkItem();
          testing::Mock::VerifyAndClearExpectations(
              &*thread_controller_.trace_observer);
          thread_controller_.OnEndWorkItem();

          // E:
          EXPECT_CALL(tasks[1], Run());
          EXPECT_EQ(thread_controller_.DoWork().delayed_run_time,
                    TimeTicks::Max());
          testing::Mock::VerifyAndClearExpectations(
              &*thread_controller_.trace_observer);

          // F:
          EXPECT_CALL(*thread_controller_.trace_observer,
                      OnThreadControllerActiveEnd);
          EXPECT_FALSE(thread_controller_.DoIdleWork());
          testing::Mock::VerifyAndClearExpectations(
              &*thread_controller_.trace_observer);

          // G:
          EXPECT_CALL(*thread_controller_.trace_observer,
                      OnThreadControllerActiveBegin);
          thread_controller_.OnBeginWorkItem();
          testing::Mock::VerifyAndClearExpectations(
              &*thread_controller_.trace_observer);
          thread_controller_.OnEndWorkItem();

          // H:
          EXPECT_CALL(*thread_controller_.trace_observer,
                      OnThreadControllerActiveEnd);
          thread_controller_.SetTaskExecutionAllowed(false);
        }));

        // B:
        EXPECT_EQ(thread_controller_.DoWork().delayed_run_time,
                  TimeTicks::Max());

        // I:
        EXPECT_CALL(*thread_controller_.trace_observer,
                    OnThreadControllerActiveEnd);
        EXPECT_FALSE(thread_controller_.DoIdleWork());
        testing::Mock::VerifyAndClearExpectations(
            &*thread_controller_.trace_observer);
      }));

  RunLoop().Run();
}

TEST_F(ThreadControllerWithMessagePumpTest,
       ThreadControllerActiveUnusedNativeLoop) {
  ThreadTaskRunnerHandle handle(MakeRefCounted<FakeTaskRunner>());

  thread_controller_.InstallTraceObserver();

  testing::InSequence sequence;

  RunLoop run_loop;
  EXPECT_CALL(*thread_controller_.trace_observer,
              OnThreadControllerActiveBegin);
  EXPECT_CALL(*message_pump_, Run(_))
      .WillOnce(Invoke([&](MessagePump::Delegate* delegate) {
        MockCallback<OnceClosure> tasks[2];

        // A: Post 2 application tasks
        // B: Run one of them (enter active)
        //   C: Allow entering a native loop but don't enter one (no-op)
        //   D: Complete the task without having entered a native loop (no-op)
        // E: Run an application task (remain nested active)
        // F: Go idle (exit active)

        // A:
        task_source_.AddTask(FROM_HERE, tasks[0].Get(), TimeTicks());
        task_source_.AddTask(FROM_HERE, tasks[1].Get(), TimeTicks());

        EXPECT_CALL(tasks[0], Run()).WillOnce(Invoke([&]() {
          // C:
          EXPECT_FALSE(thread_controller_.IsTaskExecutionAllowed());
          EXPECT_CALL(*message_pump_, ScheduleWork());
          thread_controller_.SetTaskExecutionAllowed(true);

          // D:
          thread_controller_.SetTaskExecutionAllowed(false);
        }));

        // B:
        EXPECT_EQ(thread_controller_.DoWork().delayed_run_time, TimeTicks());

        // E:
        EXPECT_CALL(tasks[1], Run());
        EXPECT_EQ(thread_controller_.DoWork().delayed_run_time,
                  TimeTicks::Max());

        // F:
        EXPECT_CALL(*thread_controller_.trace_observer,
                    OnThreadControllerActiveEnd);
        EXPECT_FALSE(thread_controller_.DoIdleWork());
        testing::Mock::VerifyAndClearExpectations(
            &*thread_controller_.trace_observer);
      }));

  RunLoop().Run();
}

TEST_F(ThreadControllerWithMessagePumpTest,
       ThreadControllerActiveNestedNativeLoopWithoutAllowance) {
  ThreadTaskRunnerHandle handle(MakeRefCounted<FakeTaskRunner>());

  thread_controller_.InstallTraceObserver();

  testing::InSequence sequence;

  RunLoop run_loop;
  EXPECT_CALL(*thread_controller_.trace_observer,
              OnThreadControllerActiveBegin);
  EXPECT_CALL(*message_pump_, Run(_))
      .WillOnce(Invoke([&](MessagePump::Delegate* delegate) {
        MockCallback<OnceClosure> tasks[2];

        // A: Post 2 application tasks
        // B: Run one of them (enter active)
        //   C: Enter a native nested loop (without having allowed nested
        //      application tasks in B.)
        //     D: Run a native task (enter nested active)
        // E: End task C. (which implicitly means the native loop is over).
        // F: Run an application task (remain active)
        // G: Go idle (exit active)

        // A:
        task_source_.AddTask(FROM_HERE, tasks[0].Get(), TimeTicks());
        task_source_.AddTask(FROM_HERE, tasks[1].Get(), TimeTicks());

        EXPECT_CALL(tasks[0], Run()).WillOnce(Invoke([&]() {
          // C:
          // D:
          EXPECT_CALL(*thread_controller_.trace_observer,
                      OnThreadControllerActiveBegin);
          thread_controller_.OnBeginWorkItem();
          testing::Mock::VerifyAndClearExpectations(
              &*thread_controller_.trace_observer);
          thread_controller_.OnEndWorkItem();

          // E:
          EXPECT_CALL(*thread_controller_.trace_observer,
                      OnThreadControllerActiveEnd);
        }));

        // B:
        EXPECT_EQ(thread_controller_.DoWork().delayed_run_time, TimeTicks());

        // F:
        EXPECT_CALL(tasks[1], Run());
        EXPECT_EQ(thread_controller_.DoWork().delayed_run_time,
                  TimeTicks::Max());

        // G:
        EXPECT_CALL(*thread_controller_.trace_observer,
                    OnThreadControllerActiveEnd);
        EXPECT_FALSE(thread_controller_.DoIdleWork());
        testing::Mock::VerifyAndClearExpectations(
            &*thread_controller_.trace_observer);
      }));

  RunLoop().Run();
}

TEST_F(ThreadControllerWithMessagePumpTest,
       ThreadControllerActiveMultipleNativeLoopsUnderOneApplicationTask) {
  ThreadTaskRunnerHandle handle(MakeRefCounted<FakeTaskRunner>());

  thread_controller_.InstallTraceObserver();

  testing::InSequence sequence;

  RunLoop run_loop;
  EXPECT_CALL(*thread_controller_.trace_observer,
              OnThreadControllerActiveBegin);
  EXPECT_CALL(*message_pump_, Run(_))
      .WillOnce(Invoke([&](MessagePump::Delegate* delegate) {
        MockCallback<OnceClosure> tasks[2];

        // A: Post 1 application task
        // B: Run it
        //   C: Enter a native nested loop (application tasks allowed)
        //     D: Run a native task (enter nested active)
        //     E: Exit nested loop (missed by RunLevelTracker -- no-op)
        //   F: Enter another native nested loop (application tasks allowed)
        //     G: Run a native task (no-op)
        //     H: Exit nested loop (no-op)
        //   I: End task (exit nested active)
        // J: Go idle (exit active)

        // A:
        task_source_.AddTask(FROM_HERE, tasks[0].Get(), TimeTicks());

        EXPECT_CALL(tasks[0], Run()).WillOnce(Invoke([&]() {
          for (int i = 0; i < 2; ++i) {
            // C & F:
            EXPECT_FALSE(thread_controller_.IsTaskExecutionAllowed());
            EXPECT_CALL(*message_pump_, ScheduleWork());
            thread_controller_.SetTaskExecutionAllowed(true);

            // D & G:
            if (i == 0) {
              EXPECT_CALL(*thread_controller_.trace_observer,
                          OnThreadControllerActiveBegin);
            }
            thread_controller_.OnBeginWorkItem();
            testing::Mock::VerifyAndClearExpectations(
                &*thread_controller_.trace_observer);
            thread_controller_.OnEndWorkItem();

            // E & H:
            thread_controller_.SetTaskExecutionAllowed(false);
            testing::Mock::VerifyAndClearExpectations(
                &*thread_controller_.trace_observer);
          }

          // I:
          EXPECT_CALL(*thread_controller_.trace_observer,
                      OnThreadControllerActiveEnd);
        }));

        // B:
        EXPECT_EQ(thread_controller_.DoWork().delayed_run_time,
                  TimeTicks::Max());

        // J:
        EXPECT_CALL(*thread_controller_.trace_observer,
                    OnThreadControllerActiveEnd);
        EXPECT_FALSE(thread_controller_.DoIdleWork());
        testing::Mock::VerifyAndClearExpectations(
            &*thread_controller_.trace_observer);
      }));

  RunLoop().Run();
}

TEST_F(ThreadControllerWithMessagePumpTest,
       ThreadControllerActiveNativeLoopsReachingIdle) {
  ThreadTaskRunnerHandle handle(MakeRefCounted<FakeTaskRunner>());

  thread_controller_.InstallTraceObserver();

  testing::InSequence sequence;

  RunLoop run_loop;
  EXPECT_CALL(*thread_controller_.trace_observer,
              OnThreadControllerActiveBegin);
  EXPECT_CALL(*message_pump_, Run(_))
      .WillOnce(Invoke([&](MessagePump::Delegate* delegate) {
        MockCallback<OnceClosure> task;

        // A: Post 1 application task
        // B: Run it
        //   C: Enter a native nested loop (application tasks allowed)
        //     D: Run a native task (enter nested active)
        //     E: Reach idle (nested inactive)
        //     F: Run another task (nested active)
        //     G: Exit nested loop (missed by RunLevelTracker -- no-op)
        //   H: End task B (exit nested active)
        // I: Go idle (exit active)
        //
        // This exercises the heuristic in
        // ThreadControllerWithMessagePumpImpl::SetTaskExecutionAllowed() to
        // detect the end of a nested native loop before the end of the task
        // that triggered it. When application tasks are not allowed however,
        // there's nothing we can do detect and two native nested loops in a
        // row. They may look like a single one if the first one is quit before
        // it reaches idle.

        // A:
        task_source_.AddTask(FROM_HERE, task.Get(), TimeTicks());

        EXPECT_CALL(task, Run()).WillOnce(Invoke([&]() {
          // C:
          EXPECT_FALSE(thread_controller_.IsTaskExecutionAllowed());
          EXPECT_CALL(*message_pump_, ScheduleWork());
          thread_controller_.SetTaskExecutionAllowed(true);

          // D:
          EXPECT_CALL(*thread_controller_.trace_observer,
                      OnThreadControllerActiveBegin);
          thread_controller_.OnBeginWorkItem();
          testing::Mock::VerifyAndClearExpectations(
              &*thread_controller_.trace_observer);
          thread_controller_.OnEndWorkItem();

          // E:
          EXPECT_CALL(*thread_controller_.trace_observer,
                      OnThreadControllerActiveEnd);
          thread_controller_.BeforeWait();
          testing::Mock::VerifyAndClearExpectations(
              &*thread_controller_.trace_observer);

          // F:
          EXPECT_CALL(*thread_controller_.trace_observer,
                      OnThreadControllerActiveBegin);
          thread_controller_.OnBeginWorkItem();
          testing::Mock::VerifyAndClearExpectations(
              &*thread_controller_.trace_observer);
          thread_controller_.OnEndWorkItem();

          // G:
          thread_controller_.SetTaskExecutionAllowed(false);

          // H:
          EXPECT_CALL(*thread_controller_.trace_observer,
                      OnThreadControllerActiveEnd);
        }));

        // B:
        EXPECT_EQ(thread_controller_.DoWork().delayed_run_time,
                  TimeTicks::Max());

        // I:
        EXPECT_CALL(*thread_controller_.trace_observer,
                    OnThreadControllerActiveEnd);
        EXPECT_FALSE(thread_controller_.DoIdleWork());
        testing::Mock::VerifyAndClearExpectations(
            &*thread_controller_.trace_observer);
      }));

  RunLoop().Run();
}

TEST_F(ThreadControllerWithMessagePumpTest,
       ThreadControllerActiveQuitNestedWhileApplicationIdle) {
  ThreadTaskRunnerHandle handle(MakeRefCounted<FakeTaskRunner>());

  thread_controller_.InstallTraceObserver();

  testing::InSequence sequence;

  RunLoop run_loop;
  EXPECT_CALL(*thread_controller_.trace_observer,
              OnThreadControllerActiveBegin);
  EXPECT_CALL(*message_pump_, Run(_))
      .WillOnce(Invoke([&](MessagePump::Delegate* delegate) {
        MockCallback<OnceClosure> tasks[2];

        // A: Post 2 application tasks
        // B: Run the first task
        //   C: Enter a native nested loop (application tasks allowed)
        //     D: Run the second task (enter nested active)
        //     E: Reach idle
        //     F: Run a native task (not visible to RunLevelTracker)
        //     G: F quits the native nested loop (no-op)
        //   H: End task B (exit nested active)
        // I: Go idle (exit active)

        // A:
        task_source_.AddTask(FROM_HERE, tasks[0].Get(), TimeTicks());
        task_source_.AddTask(FROM_HERE, tasks[1].Get(), TimeTicks());

        EXPECT_CALL(tasks[0], Run()).WillOnce(Invoke([&]() {
          // C:
          EXPECT_FALSE(thread_controller_.IsTaskExecutionAllowed());
          EXPECT_CALL(*message_pump_, ScheduleWork());
          thread_controller_.SetTaskExecutionAllowed(true);

          // D:
          EXPECT_CALL(*thread_controller_.trace_observer,
                      OnThreadControllerActiveBegin);
          EXPECT_CALL(tasks[1], Run());
          EXPECT_EQ(thread_controller_.DoWork().delayed_run_time,
                    TimeTicks::Max());
          testing::Mock::VerifyAndClearExpectations(
              &*thread_controller_.trace_observer);

          // E:
          EXPECT_CALL(*thread_controller_.trace_observer,
                      OnThreadControllerActiveEnd);
          thread_controller_.BeforeWait();
          testing::Mock::VerifyAndClearExpectations(
              &*thread_controller_.trace_observer);

          // F + G:
          thread_controller_.SetTaskExecutionAllowed(false);

          // H:
        }));

        // B:
        EXPECT_EQ(thread_controller_.DoWork().delayed_run_time,
                  TimeTicks::Max());

        // I:
        EXPECT_CALL(*thread_controller_.trace_observer,
                    OnThreadControllerActiveEnd);
        EXPECT_FALSE(thread_controller_.DoIdleWork());
        testing::Mock::VerifyAndClearExpectations(
            &*thread_controller_.trace_observer);
      }));

  RunLoop().Run();
}

// This test verifies the edge case where the first task on the stack is native
// task which spins a native nested loop. That inner-loop should be allowed to
// execute application tasks as the outer-loop didn't consume
// |task_execution_allowed == true|. RunLevelTracker should support this use
// case as well.
TEST_F(ThreadControllerWithMessagePumpTest,
       ThreadControllerActiveNestedWithinNativeAllowsApplicationTasks) {
  ThreadTaskRunnerHandle handle(MakeRefCounted<FakeTaskRunner>());

  thread_controller_.InstallTraceObserver();

  testing::InSequence sequence;

  RunLoop run_loop;
  EXPECT_CALL(*thread_controller_.trace_observer,
              OnThreadControllerActiveBegin);
  EXPECT_CALL(*message_pump_, Run(_))
      .WillOnce(Invoke([&](MessagePump::Delegate* delegate) {
        // Start this test idle for a change.
        EXPECT_CALL(*thread_controller_.trace_observer,
                    OnThreadControllerActiveEnd);
        EXPECT_FALSE(thread_controller_.DoIdleWork());
        testing::Mock::VerifyAndClearExpectations(
            &*thread_controller_.trace_observer);

        MockCallback<OnceClosure> task;

        // A: Post 1 application task
        // B: Run a native task
        //   C: Enter a native nested loop (application tasks still allowed)
        //     D: Run the application task (enter nested active)
        // E: End the native task (exit nested active)
        // F: Go idle (exit active)

        // A:
        task_source_.AddTask(FROM_HERE, task.Get(), TimeTicks());

        EXPECT_CALL(*thread_controller_.trace_observer,
                    OnThreadControllerActiveBegin)
            .WillOnce(Invoke([&]() {
              // C:
              EXPECT_TRUE(thread_controller_.IsTaskExecutionAllowed());

              // D:
              EXPECT_CALL(*thread_controller_.trace_observer,
                          OnThreadControllerActiveBegin);
              EXPECT_CALL(task, Run());
              EXPECT_EQ(thread_controller_.DoWork().delayed_run_time,
                        TimeTicks::Max());
              testing::Mock::VerifyAndClearExpectations(
                  &*thread_controller_.trace_observer);
            }));

        // B:
        thread_controller_.OnBeginWorkItem();

        // E:
        EXPECT_CALL(*thread_controller_.trace_observer,
                    OnThreadControllerActiveEnd);
        thread_controller_.OnEndWorkItem();

        // F:
        EXPECT_CALL(*thread_controller_.trace_observer,
                    OnThreadControllerActiveEnd);
        EXPECT_FALSE(thread_controller_.DoIdleWork());
        testing::Mock::VerifyAndClearExpectations(
            &*thread_controller_.trace_observer);
      }));

  RunLoop().Run();
}

// Same as ThreadControllerActiveNestedWithinNativeAllowsApplicationTasks but
// with a dummy ScopedAllowApplicationTasksInNativeNestedLoop that is a
// true=>true no-op for SetTaskExecutionAllowed(). This is a regression test
// against another discussed implementation for RunLevelTracker which
// would have used ScopedAllowApplicationTasksInNativeNestedLoop as a hint of
// nested native loops. Doing so would have been incorrect because it assumes
// that ScopedAllowApplicationTasksInNativeNestedLoop always toggles the
// allowance away-from and back-to |false|.
TEST_F(ThreadControllerWithMessagePumpTest,
       ThreadControllerActiveDummyScopedAllowApplicationTasks) {
  ThreadTaskRunnerHandle handle(MakeRefCounted<FakeTaskRunner>());

  thread_controller_.InstallTraceObserver();

  testing::InSequence sequence;

  RunLoop run_loop;
  EXPECT_CALL(*thread_controller_.trace_observer,
              OnThreadControllerActiveBegin);
  EXPECT_CALL(*message_pump_, Run(_))
      .WillOnce(Invoke([&](MessagePump::Delegate* delegate) {
        // Start this test idle for a change.
        EXPECT_CALL(*thread_controller_.trace_observer,
                    OnThreadControllerActiveEnd);
        EXPECT_FALSE(thread_controller_.DoIdleWork());
        testing::Mock::VerifyAndClearExpectations(
            &*thread_controller_.trace_observer);

        MockCallback<OnceClosure> task;

        // A: Post 1 application task
        // B: Run a native task
        //   C: Enter dummy ScopedAllowApplicationTasksInNativeNestedLoop
        //   D: Enter a native nested loop (application tasks still allowed)
        //     E: Run the application task (enter nested active)
        //   F: Exit dummy scope (SetTaskExecutionAllowed(true)).
        // G: End the native task (exit nested active)
        // H: Go idle (exit active)

        // A:
        task_source_.AddTask(FROM_HERE, task.Get(), TimeTicks());

        EXPECT_CALL(*thread_controller_.trace_observer,
                    OnThreadControllerActiveBegin)
            .WillOnce(Invoke([&]() {
              // C + D:
              EXPECT_TRUE(thread_controller_.IsTaskExecutionAllowed());
              EXPECT_CALL(*message_pump_, ScheduleWork());
              thread_controller_.SetTaskExecutionAllowed(true);
              testing::Mock::VerifyAndClearExpectations(
                  &*thread_controller_.trace_observer);

              // E:
              EXPECT_CALL(*thread_controller_.trace_observer,
                          OnThreadControllerActiveBegin);
              EXPECT_CALL(task, Run());
              EXPECT_EQ(thread_controller_.DoWork().delayed_run_time,
                        TimeTicks::Max());
              testing::Mock::VerifyAndClearExpectations(
                  &*thread_controller_.trace_observer);

              // F:
              EXPECT_CALL(*message_pump_, ScheduleWork());
              thread_controller_.SetTaskExecutionAllowed(true);
            }));

        // B:
        thread_controller_.OnBeginWorkItem();

        // G:
        EXPECT_CALL(*thread_controller_.trace_observer,
                    OnThreadControllerActiveEnd);
        thread_controller_.OnEndWorkItem();

        // H:
        EXPECT_CALL(*thread_controller_.trace_observer,
                    OnThreadControllerActiveEnd);
        EXPECT_FALSE(thread_controller_.DoIdleWork());
        testing::Mock::VerifyAndClearExpectations(
            &*thread_controller_.trace_observer);
      }));

  RunLoop().Run();
}

}  // namespace sequence_manager
}  // namespace base
