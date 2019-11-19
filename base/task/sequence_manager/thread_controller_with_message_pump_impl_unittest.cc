// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/sequence_manager/thread_controller_with_message_pump_impl.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/single_thread_task_runner.h"
#include "base/test/bind_test_util.h"
#include "base/test/mock_callback.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/threading/thread_task_runner_handle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#include <queue>

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
                          SequenceManager::Settings& settings)
      : ThreadControllerWithMessagePumpImpl(std::move(pump), settings) {}

  using ThreadControllerWithMessagePumpImpl::DoDelayedWork;
  using ThreadControllerWithMessagePumpImpl::DoIdleWork;
  using ThreadControllerWithMessagePumpImpl::DoWork;
  using ThreadControllerWithMessagePumpImpl::EnsureWorkScheduled;
  using ThreadControllerWithMessagePumpImpl::Quit;
  using ThreadControllerWithMessagePumpImpl::Run;
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

  Task* SelectNextTask() override {
    if (tasks_.empty())
      return nullptr;
    if (tasks_.front().delayed_run_time > clock_->NowTicks())
      return nullptr;
    running_stack_.push_back(std::move(tasks_.front()));
    tasks_.pop();
    return &running_stack_.back();
  }

  void DidRunTask() override { running_stack_.pop_back(); }

  TimeDelta DelayTillNextTask(LazyNow* lazy_now) const override {
    if (tasks_.empty())
      return TimeDelta::Max();
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

  bool HasPendingHighResolutionTasks() override { return false; }

  bool OnSystemIdle() override { return false; }

 private:
  TickClock* clock_;
  std::queue<Task> tasks_;
  std::vector<Task> running_stack_;
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

 protected:
  MockMessagePump* message_pump_;
  SequenceManager::Settings settings_;
  SimpleTestTickClock clock_;
  ThreadControllerForTest thread_controller_;
  FakeSequencedTaskSource task_source_;
};

TEST_F(ThreadControllerWithMessagePumpTest, ScheduleDelayedWork) {
  TimeTicks next_run_time;

  MockCallback<OnceClosure> task1;
  task_source_.AddTask(FROM_HERE, task1.Get(), Seconds(10));
  MockCallback<OnceClosure> task2;
  task_source_.AddTask(FROM_HERE, task2.Get(), TimeTicks());
  MockCallback<OnceClosure> task3;
  task_source_.AddTask(FROM_HERE, task3.Get(), Seconds(20));

  // Call a no-op DoWork. Expect that it doesn't do any work.
  clock_.SetNowTicks(Seconds(5));
  EXPECT_CALL(*message_pump_, ScheduleDelayedWork(_)).Times(0);
  EXPECT_FALSE(thread_controller_.DoWork());
  testing::Mock::VerifyAndClearExpectations(message_pump_);

  // DoDelayedWork is always called after DoWork. Expect that it doesn't do
  // any work, but schedules a delayed wake-up appropriately.
  EXPECT_FALSE(thread_controller_.DoDelayedWork(&next_run_time));
  EXPECT_EQ(next_run_time, Seconds(10));
  testing::Mock::VerifyAndClearExpectations(message_pump_);

  // Call DoDelayedWork after the expiration of the delay.
  // Expect that a task will run and the next delay will equal to
  // TimeTicks() as we have immediate work to do.
  clock_.SetNowTicks(Seconds(11));
  EXPECT_CALL(task1, Run()).Times(1);
  // There's no pending DoWork so a ScheduleWork gets called.
  EXPECT_CALL(*message_pump_, ScheduleWork());
  EXPECT_TRUE(thread_controller_.DoDelayedWork(&next_run_time));
  EXPECT_EQ(next_run_time, TimeTicks());
  testing::Mock::VerifyAndClearExpectations(message_pump_);
  testing::Mock::VerifyAndClearExpectations(&task1);

  // Call DoWork immediately after the previous call. Expect a new task
  // to be run.
  EXPECT_CALL(task2, Run()).Times(1);
  EXPECT_TRUE(thread_controller_.DoWork());
  testing::Mock::VerifyAndClearExpectations(message_pump_);
  testing::Mock::VerifyAndClearExpectations(&task2);

  // DoDelayedWork is always called after DoWork.
  EXPECT_FALSE(thread_controller_.DoDelayedWork(&next_run_time));
  EXPECT_EQ(next_run_time, Seconds(20));
  testing::Mock::VerifyAndClearExpectations(message_pump_);

  // Call DoDelayedWork for the last task and expect to be told
  // about the lack of further delayed work (next run time being TimeTicks()).
  clock_.SetNowTicks(Seconds(21));
  EXPECT_CALL(task3, Run()).Times(1);
  EXPECT_TRUE(thread_controller_.DoDelayedWork(&next_run_time));
  EXPECT_EQ(next_run_time, TimeTicks());
  testing::Mock::VerifyAndClearExpectations(message_pump_);
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

  TimeTicks next_run_time;
  EXPECT_FALSE(thread_controller_.DoDelayedWork(&next_run_time));
  EXPECT_EQ(next_run_time, Days(1));
}

TEST_F(ThreadControllerWithMessagePumpTest, DoWorkDoesntScheduleDelayedWork) {
  MockCallback<OnceClosure> task1;
  task_source_.AddTask(FROM_HERE, task1.Get(), Seconds(10));

  EXPECT_CALL(*message_pump_, ScheduleDelayedWork(_)).Times(0);
  EXPECT_FALSE(thread_controller_.DoWork());
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
        EXPECT_TRUE(delegate->DoWork());
        EXPECT_TRUE(delegate->DoWork());
        EXPECT_TRUE(delegate->DoWork());
        EXPECT_FALSE(delegate->DoWork());
        log.push_back("exiting top-level runloop");
      }))
      .WillOnce(Invoke([&log, this](MessagePump::Delegate* delegate) {
        log.push_back("entering nested runloop");
        EXPECT_EQ(delegate, &thread_controller_);
        EXPECT_FALSE(thread_controller_.IsTaskExecutionAllowed());
        EXPECT_FALSE(delegate->DoWork());
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
        EXPECT_TRUE(delegate->DoWork());
        EXPECT_FALSE(delegate->DoWork());
        log.push_back("exiting top-level runloop");
      }))
      .WillOnce(Invoke([&log, this](MessagePump::Delegate* delegate) {
        log.push_back("entering nested runloop");
        EXPECT_EQ(delegate, &thread_controller_);
        EXPECT_TRUE(thread_controller_.IsTaskExecutionAllowed());
        EXPECT_TRUE(delegate->DoWork());
        EXPECT_TRUE(delegate->DoWork());
        EXPECT_FALSE(delegate->DoWork());
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

TEST_F(ThreadControllerWithMessagePumpTest, ScheduleWorkFromDelayedTask) {
  ThreadTaskRunnerHandle handle(MakeRefCounted<FakeTaskRunner>());

  EXPECT_CALL(*message_pump_, Run(_))
      .WillOnce(Invoke([](MessagePump::Delegate* delegate) {
        base::TimeTicks run_time;
        delegate->DoDelayedWork(&run_time);
      }));
  EXPECT_CALL(*message_pump_, ScheduleWork());

  task_source_.AddTask(FROM_HERE, base::BindLambdaForTesting([&]() {
                         // Triggers a ScheduleWork call.
                         task_source_.AddTask(FROM_HERE,
                                              base::BindOnce([]() {}),
                                              base::TimeTicks());
                       }),
                       TimeTicks());
  RunLoop().Run();

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

  EXPECT_TRUE(thread_controller_.DoWork());

  // EnsureWorkScheduled() doesn't need to call the pump because there's no
  // DoWork pending.
  EXPECT_CALL(*message_pump_, ScheduleWork()).Times(0);
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
        EXPECT_TRUE(delegate->DoWork());
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
        TimeTicks next_time;
        EXPECT_TRUE(delegate->DoWork());
        EXPECT_EQ(1, task_count);

        // Somewhat counter-intuitive, but if the pump keeps calling us after
        // Quit(), the delegate should still run tasks as normally. This is
        // needed to support nested OS-level runloops that still pump
        // application tasks (e.g., showing a popup menu on Mac).
        EXPECT_TRUE(delegate->DoDelayedWork(&next_time));
        EXPECT_EQ(2, task_count);
        EXPECT_TRUE(delegate->DoWork());
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
        EXPECT_TRUE(delegate->DoWork());
        EXPECT_TRUE(delegate->DoWork());
        EXPECT_FALSE(delegate->DoWork());
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
        EXPECT_FALSE(thread_controller_.DoWork());
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
  EXPECT_TRUE(thread_controller_.DoWork());
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
        TimeTicks next_run_time;
        clock_.SetNowTicks(Seconds(5));
        EXPECT_CALL(task1, Run()).Times(1);
        EXPECT_TRUE(thread_controller_.DoDelayedWork(&next_run_time));
        EXPECT_EQ(next_run_time, Seconds(10));

        clock_.SetNowTicks(Seconds(10));
        EXPECT_CALL(task2, Run()).Times(1);
        EXPECT_TRUE(thread_controller_.DoDelayedWork(&next_run_time));
        EXPECT_EQ(next_run_time, Seconds(15));

        clock_.SetNowTicks(Seconds(15));
        EXPECT_CALL(task3, Run()).Times(0);
        EXPECT_FALSE(thread_controller_.DoDelayedWork(&next_run_time));
        EXPECT_EQ(next_run_time, TimeTicks());

        EXPECT_CALL(*message_pump_, Quit());
        EXPECT_FALSE(thread_controller_.DoIdleWork());
      }));
  thread_controller_.Run(true, TimeDelta::FromSeconds(15));
}

}  // namespace sequence_manager
}  // namespace base
