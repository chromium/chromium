// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/sequence_manager/thread_controller_with_message_pump_impl.h"

#include <queue>
#include <string>
#include <utility>
#include <vector>
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequence_manager/task_queue.h"
#include "base/task/sequence_manager/thread_controller_power_monitor.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_features.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using testing::_;
using testing::Invoke;
using testing::ElementsAre;

namespace base::sequence_manager::internal {

namespace {

class ThreadControllerForTest : public ThreadControllerWithMessagePumpImpl {
 public:
  ThreadControllerForTest(std::unique_ptr<MessagePump> pump,
                          const SequenceManager::Settings& settings)
      : ThreadControllerWithMessagePumpImpl(std::move(pump), settings) {}

  ~ThreadControllerForTest() override {
    if (trace_observer_)
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

  class MockTraceObserver
      : public ThreadController::RunLevelTracker::TraceObserverForTesting {
   public:
    MOCK_METHOD0(OnThreadControllerActiveBegin, void());
    MOCK_METHOD0(OnThreadControllerActiveEnd, void());
    MOCK_METHOD1(OnPhaseRecorded, void(Phase));
  };

  void InstallTraceObserver() {
    trace_observer_.emplace();
    RunLevelTracker::SetTraceObserverForTesting(&trace_observer_.value());

    // EnableMessagePumpTimeKeeperMetrics is a no-op on hardware which lacks
    // high-res clocks.
    ASSERT_TRUE(TimeTicks::IsHighResolution());
    EnableMessagePumpTimeKeeperMetrics("TestMainThread");
  }

  // Optionally emplaced, strict from then on.
  absl::optional<testing::StrictMock<MockTraceObserver>> trace_observer_;
};

class MockMessagePump : public MessagePump {
 public:
  MockMessagePump() {}
  ~MockMessagePump() override {}

  MOCK_METHOD1(Run, void(MessagePump::Delegate*));
  MOCK_METHOD0(Quit, void());
  MOCK_METHOD0(ScheduleWork, void());
  MOCK_METHOD1(ScheduleDelayedWork_TimeTicks, void(const TimeTicks&));
  MOCK_METHOD1(SetTimerSlack, void(TimerSlack));

  void ScheduleDelayedWork(
      const MessagePump::Delegate::NextWorkInfo& next_work_info) override {
    ScheduleDelayedWork_TimeTicks(next_work_info.delayed_run_time);
  }
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

class FakeSequencedTaskSource : public SequencedTaskSource {
 public:
  explicit FakeSequencedTaskSource(TickClock* clock) : clock_(clock) {}
  ~FakeSequencedTaskSource() override = default;

  absl::optional<SelectedTask> SelectNextTask(
      LazyNow& lazy_now,
      SelectTaskOption option) override {
    if (tasks_.empty())
      return absl::nullopt;
    if (tasks_.front().delayed_run_time > clock_->NowTicks())
      return absl::nullopt;
    if (option == SequencedTaskSource::SelectTaskOption::kSkipDelayedTask &&
        !tasks_.front().delayed_run_time.is_null()) {
      return absl::nullopt;
    }
    running_stack_.push_back(std::move(tasks_.front()));
    tasks_.pop();
    return SelectedTask(running_stack_.back(), TaskExecutionTraceLogger(),
                        TaskQueue::QueuePriority::kNormalPriority,
                        QueueName::TEST_TQ);
  }

  void DidRunTask(LazyNow& lazy_now) override { running_stack_.pop_back(); }

  void RemoveAllCanceledDelayedTasksFromFront(LazyNow* lazy_now) override {}

  absl::optional<WakeUp> GetPendingWakeUp(
      LazyNow* lazy_now,
      SelectTaskOption option) const override {
    if (tasks_.empty())
      return absl::nullopt;
    if (option == SequencedTaskSource::SelectTaskOption::kSkipDelayedTask &&
        !tasks_.front().delayed_run_time.is_null()) {
      return absl::nullopt;
    }
    if (tasks_.front().delayed_run_time.is_null())
      return WakeUp{};
    if (lazy_now->Now() > tasks_.front().delayed_run_time)
      return WakeUp{};
    return WakeUp{tasks_.front().delayed_run_time};
  }

  void AddTask(Location posted_from,
               OnceClosure task,
               TimeTicks delayed_run_time = TimeTicks(),
               TimeTicks queue_time = TimeTicks()) {
    DCHECK(tasks_.empty() || delayed_run_time.is_null() ||
           tasks_.back().delayed_run_time < delayed_run_time);
    tasks_.push(
        Task(PostedTask(nullptr, std::move(task), posted_from, delayed_run_time,
                        base::subtle::DelayPolicy::kFlexibleNoSooner),
             EnqueueOrder::FromIntForTesting(13), EnqueueOrder(), queue_time));
  }

  bool HasPendingHighResolutionTasks() override {
    return has_pending_high_resolution_tasks;
  }

  void SetHasPendingHighResolutionTasks(bool state) {
    has_pending_high_resolution_tasks = state;
  }

  bool OnSystemIdle() override { return false; }

 private:
  raw_ptr<TickClock> clock_;
  std::queue<Task> tasks_;
  std::vector<Task> running_stack_;
  bool has_pending_high_resolution_tasks = false;
};

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
    // SimpleTestTickClock starts at zero, but that also satisfies
    // TimeTicks::is_null() and that throws off some ThreadController state.
    // Move away from 0. All ThreadControllerWithMessagePumpTests should favor
    // Advance() over SetNowTicks() for this reason.
    clock_.Advance(Seconds(1000));

    thread_controller_.SetWorkBatchSize(1);
    thread_controller_.SetSequencedTaskSource(&task_source_);
  }

  void SetUp() override {
    ThreadControllerPowerMonitor::OverrideUsePowerMonitorForTesting(true);
  }

  void TearDown() override { ThreadControllerPowerMonitor::ResetForTesting(); }

  TimeTicks FromNow(TimeDelta delta) { return clock_.NowTicks() + delta; }

 protected:
  raw_ptr<MockMessagePump> message_pump_;
  SequenceManager::Settings settings_;
  SimpleTestTickClock clock_;
  ThreadControllerForTest thread_controller_;
  FakeSequencedTaskSource task_source_;
};

TEST_F(ThreadControllerWithMessagePumpTest, ScheduleDelayedWork) {
  MockCallback<OnceClosure> task1;
  task_source_.AddTask(FROM_HERE, task1.Get(), FromNow(Seconds(10)),
                       clock_.NowTicks());
  MockCallback<OnceClosure> task2;
  task_source_.AddTask(FROM_HERE, task2.Get());
  MockCallback<OnceClosure> task3;
  task_source_.AddTask(FROM_HERE, task3.Get(), FromNow(Seconds(20)),
                       clock_.NowTicks());

  // Call a no-op DoWork. Expect that it doesn't do any work.
  clock_.Advance(Seconds(5));
  EXPECT_CALL(*message_pump_, ScheduleDelayedWork_TimeTicks(_)).Times(0);
  {
    auto next_work_info = thread_controller_.DoWork();
    EXPECT_FALSE(next_work_info.is_immediate());
    EXPECT_EQ(next_work_info.delayed_run_time, FromNow(Seconds(5)));
  }
  testing::Mock::VerifyAndClearExpectations(message_pump_);

  // Call DoWork after the expiration of the delay.
  // Expect that |task1| runs and the return value indicates that |task2| can
  // run immediately.
  clock_.Advance(Seconds(6));
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
    EXPECT_EQ(next_work_info.delayed_run_time, FromNow(Seconds(9)));
  }
  testing::Mock::VerifyAndClearExpectations(&task2);

  // Call DoWork for the last task and expect to be told
  // about the lack of further delayed work (next run time being TimeTicks()).
  clock_.Advance(Seconds(10));
  EXPECT_CALL(task3, Run()).Times(1);
  {
    auto next_work_info = thread_controller_.DoWork();
    EXPECT_FALSE(next_work_info.is_immediate());
    EXPECT_EQ(next_work_info.delayed_run_time, TimeTicks::Max());
  }
  testing::Mock::VerifyAndClearExpectations(&task3);
}

TEST_F(ThreadControllerWithMessagePumpTest, SetNextDelayedDoWork) {
  EXPECT_CALL(*message_pump_,
              ScheduleDelayedWork_TimeTicks(FromNow(Seconds(123))));

  LazyNow lazy_now(&clock_);
  thread_controller_.SetNextDelayedDoWork(&lazy_now,
                                          WakeUp{FromNow(Seconds(123))});
}

TEST_F(ThreadControllerWithMessagePumpTest, SetNextDelayedDoWork_CapAtOneDay) {
  EXPECT_CALL(*message_pump_, ScheduleDelayedWork_TimeTicks(FromNow(Days(1))));

  LazyNow lazy_now(&clock_);
  thread_controller_.SetNextDelayedDoWork(&lazy_now, WakeUp{FromNow(Days(2))});
}

TEST_F(ThreadControllerWithMessagePumpTest, DelayedWork_CapAtOneDay) {
  MockCallback<OnceClosure> task1;
  task_source_.AddTask(FROM_HERE, task1.Get(), FromNow(Days(10)),
                       clock_.NowTicks());

  auto next_work_info = thread_controller_.DoWork();
  EXPECT_EQ(next_work_info.delayed_run_time, FromNow(Days(1)));
}

TEST_F(ThreadControllerWithMessagePumpTest, DoWorkDoesntScheduleDelayedWork) {
  MockCallback<OnceClosure> task1;
  task_source_.AddTask(FROM_HERE, task1.Get(), FromNow(Seconds(10)),
                       clock_.NowTicks());

  EXPECT_CALL(*message_pump_, ScheduleDelayedWork_TimeTicks(_)).Times(0);
  auto next_work_info = thread_controller_.DoWork();
  EXPECT_EQ(next_work_info.delayed_run_time, FromNow(Seconds(10)));
}

TEST_F(ThreadControllerWithMessagePumpTest, NestedExecution) {
  // This test posts three immediate tasks. The first creates a nested RunLoop
  // and the test expects that the second and third tasks are run outside of
  // the nested loop.
  std::vector<std::string> log;

  SingleThreadTaskRunner::CurrentDefaultHandle handle(
      MakeRefCounted<FakeTaskRunner>());

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

  SingleThreadTaskRunner::CurrentDefaultHandle handle(
      MakeRefCounted<FakeTaskRunner>());

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
  EXPECT_EQ(task_runner1, SingleThreadTaskRunner::GetCurrentDefault());

  // Check that we are correctly supporting overriding.
  scoped_refptr<SingleThreadTaskRunner> task_runner2 =
      MakeRefCounted<FakeTaskRunner>();
  thread_controller_.SetDefaultTaskRunner(task_runner2);
  EXPECT_EQ(task_runner2, SingleThreadTaskRunner::GetCurrentDefault());
}

TEST_F(ThreadControllerWithMessagePumpTest, EnsureWorkScheduled) {
  task_source_.AddTask(FROM_HERE, DoNothing());

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
  SingleThreadTaskRunner::CurrentDefaultHandle handle(
      MakeRefCounted<FakeTaskRunner>());

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
  SingleThreadTaskRunner::CurrentDefaultHandle handle(
      MakeRefCounted<FakeTaskRunner>());

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

#if BUILDFLAG(IS_ANDROID)
TEST_F(ThreadControllerWithMessagePumpTest, PeriodicYieldingToNative) {
  SingleThreadTaskRunner::CurrentDefaultHandle handle(
      MakeRefCounted<FakeTaskRunner>());

  testing::InSequence sequence;
  RunLoop run_loop;
  EXPECT_CALL(*message_pump_, Run(_))
      .WillOnce(Invoke([&](MessagePump::Delegate* delegate) {
        // B: Post 5 application tasks
        //    First two combined should take 4 ms
        //    Third one should take 4 ms
        //    4th and 5th should be 1ms each
        // B: EnabledPeriodicYieldingToNative within thread controller to 3ms
        //    window.
        // C: Call DoWork() to run the first 2 tasks
        //   D: Expect we return immediate work and have executed Run() on
        //      task 1 and 2, yield_to_native should be true
        //      executed_task_count should be 2.
        // E: Call DoWork() again to execute the 3rd task only,
        //   F: yield_to_native should still be true.
        //    executed_task_count should be 3
        // G: Disable EnabledPeriodicYieldingToNative
        // H: Task count should increment by 1 per execution as that's the
        //    batch size, yield_to_native should remain false.

        // A:
        int executed_task_count = 0;
        task_source_.AddTask(
            FROM_HERE,
            base::BindOnce(
                [](int* executed_task_count, SimpleTestTickClock* clock) {
                  clock->Advance(base::Milliseconds(2));
                  (*executed_task_count)++;
                },
                &executed_task_count, &clock_));
        task_source_.AddTask(
            FROM_HERE,
            base::BindOnce(
                [](int* executed_task_count, SimpleTestTickClock* clock) {
                  clock->Advance(base::Milliseconds(2));
                  (*executed_task_count)++;
                },
                &executed_task_count, &clock_));
        task_source_.AddTask(
            FROM_HERE,
            base::BindOnce(
                [](int* executed_task_count, SimpleTestTickClock* clock) {
                  clock->Advance(base::Milliseconds(4));
                  (*executed_task_count)++;
                },
                &executed_task_count, &clock_));
        task_source_.AddTask(
            FROM_HERE,
            base::BindOnce(
                [](int* executed_task_count, SimpleTestTickClock* clock) {
                  clock->Advance(base::Milliseconds(1));
                  (*executed_task_count)++;
                },
                &executed_task_count, &clock_));
        task_source_.AddTask(
            FROM_HERE,
            base::BindOnce(
                [](int* executed_task_count, SimpleTestTickClock* clock) {
                  clock->Advance(base::Milliseconds(1));
                  (*executed_task_count)++;
                },
                &executed_task_count, &clock_));

        // B:
        thread_controller_.EnablePeriodicYieldingToNative(
            base::Milliseconds(3));

        // C:
        auto next_work_item = thread_controller_.DoWork();
        // D:
        EXPECT_EQ(next_work_item.delayed_run_time, TimeTicks());
        EXPECT_EQ(executed_task_count, 2);
        EXPECT_TRUE(next_work_item.yield_to_native);

        // E:
        next_work_item = thread_controller_.DoWork();
        // F:
        EXPECT_EQ(next_work_item.delayed_run_time, TimeTicks());
        EXPECT_EQ(executed_task_count, 3);
        EXPECT_TRUE(next_work_item.yield_to_native);

        // G:
        thread_controller_.EnablePeriodicYieldingToNative(
            base::TimeDelta::Max());

        // H:
        next_work_item = thread_controller_.DoWork();
        EXPECT_FALSE(next_work_item.yield_to_native);
        EXPECT_EQ(executed_task_count, 4);
        next_work_item = thread_controller_.DoWork();
        EXPECT_FALSE(next_work_item.yield_to_native);
        EXPECT_EQ(executed_task_count, 5);
      }));

  run_loop.Run();
  testing::Mock::VerifyAndClearExpectations(message_pump_);
}
#endif  // BUILDFLAG(IS_ANDROID)

TEST_F(ThreadControllerWithMessagePumpTest, EarlyQuit) {
  // This test ensures that a opt-of-runloop Quit() (which is possible with some
  // pump implementations) doesn't affect the next RunLoop::Run call.

  SingleThreadTaskRunner::CurrentDefaultHandle handle(
      MakeRefCounted<FakeTaskRunner>());

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
        task_source_.AddTask(FROM_HERE, DoNothing());
        EXPECT_CALL(*message_pump_, ScheduleWork());
        thread_controller_.ScheduleWork();
        testing::Mock::VerifyAndClearExpectations(message_pump_);

        thread_controller_.SetTaskExecutionAllowed(false);

        // Simulate a subsequent PostTask by the chromium task after
        // we've left the native loop. This should not ScheduleWork
        // on the pump because the ThreadController will do that
        // after this task finishes.
        task_source_.AddTask(FROM_HERE, DoNothing());
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
  task_source_.AddTask(FROM_HERE, task1.Get(), FromNow(Seconds(5)),
                       clock_.NowTicks());
  MockCallback<OnceClosure> task2;
  task_source_.AddTask(FROM_HERE, task2.Get(), FromNow(Seconds(10)),
                       clock_.NowTicks());
  MockCallback<OnceClosure> task3;
  task_source_.AddTask(FROM_HERE, task3.Get(), FromNow(Seconds(20)),
                       clock_.NowTicks());

  EXPECT_CALL(*message_pump_, Run(_))
      .WillOnce(Invoke([&](MessagePump::Delegate*) {
        clock_.Advance(Seconds(5));
        EXPECT_CALL(task1, Run()).Times(1);
        EXPECT_EQ(thread_controller_.DoWork().delayed_run_time,
                  FromNow(Seconds(5)));

        clock_.Advance(Seconds(5));
        EXPECT_CALL(task2, Run()).Times(1);
        EXPECT_EQ(thread_controller_.DoWork().delayed_run_time,
                  FromNow(Seconds(5)));

        clock_.Advance(Seconds(5));
        EXPECT_CALL(task3, Run()).Times(0);
        EXPECT_EQ(thread_controller_.DoWork().delayed_run_time,
                  TimeTicks::Max());

        EXPECT_CALL(*message_pump_, Quit());
        EXPECT_FALSE(thread_controller_.DoIdleWork());
      }));
  thread_controller_.Run(true, Seconds(15));
}

#if BUILDFLAG(IS_WIN)
TEST_F(ThreadControllerWithMessagePumpTest, SetHighResolutionTimer) {
  MockCallback<OnceClosure> task;
  task_source_.AddTask(FROM_HERE, task.Get(), FromNow(Seconds(5)),
                       clock_.NowTicks());

  SingleThreadTaskRunner::CurrentDefaultHandle handle(
      MakeRefCounted<FakeTaskRunner>());

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
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_WIN)
TEST_F(ThreadControllerWithMessagePumpTest,
       SetHighResolutionTimerWithPowerSuspend) {
  MockCallback<OnceClosure> task;
  task_source_.AddTask(FROM_HERE, task.Get(), FromNow(Seconds(5)),
                       clock_.NowTicks());

  SingleThreadTaskRunner::CurrentDefaultHandle handle(
      MakeRefCounted<FakeTaskRunner>());

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
#endif  // BUILDFLAG(IS_WIN)

TEST_F(ThreadControllerWithMessagePumpTest,
       ScheduleDelayedWorkWithPowerSuspend) {
  SingleThreadTaskRunner::CurrentDefaultHandle handle(
      MakeRefCounted<FakeTaskRunner>());

  MockCallback<OnceClosure> task1;
  task_source_.AddTask(FROM_HERE, task1.Get(), FromNow(Seconds(10)),
                       clock_.NowTicks());
  MockCallback<OnceClosure> task2;
  task_source_.AddTask(FROM_HERE, task2.Get(), FromNow(Seconds(15)),
                       clock_.NowTicks());

  clock_.Advance(Seconds(5));

  // Call a no-op DoWork. Expect that it doesn't do any work.
  EXPECT_CALL(task1, Run()).Times(0);
  EXPECT_CALL(task2, Run()).Times(0);
  EXPECT_EQ(thread_controller_.DoWork().delayed_run_time, FromNow(Seconds(5)));
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
  clock_.Advance(Seconds(42));

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
  SingleThreadTaskRunner::CurrentDefaultHandle handle(
      MakeRefCounted<FakeTaskRunner>());

  thread_controller_.InstallTraceObserver();

  testing::InSequence sequence;

  RunLoop run_loop;
  EXPECT_CALL(*thread_controller_.trace_observer_,
              OnThreadControllerActiveBegin);
  EXPECT_CALL(*message_pump_, Run(_))
      .WillOnce(Invoke([&](MessagePump::Delegate* delegate) {
        // Don't expect a call to OnThreadControllerActiveBegin on the first
        // pass as the Run() call already triggered the active state.
        bool first_pass = true;

        // Post 1 task, run it, go idle, repeat 5 times. Expected to enter/exit
        // "ThreadController active" state 5 consecutive times.
        for (int i = 0; i < 5; ++i, first_pass = false) {
          MockCallback<OnceClosure> task;
          task_source_.AddTask(FROM_HERE, task.Get());

          if (!first_pass) {
            EXPECT_CALL(*thread_controller_.trace_observer_,
                        OnThreadControllerActiveBegin);
          } else {
            // The first pass begins with a kPumpOverhead phase as the
            // RunLoop::Run() begins active. Subsequent passes begin idle and
            // thus start with a kSelectingApplicationTask phase.
            EXPECT_CALL(*thread_controller_.trace_observer_,
                        OnPhaseRecorded(ThreadController::kPumpOverhead));
          }
          EXPECT_CALL(
              *thread_controller_.trace_observer_,
              OnPhaseRecorded(ThreadController::kSelectingApplicationTask));
          EXPECT_CALL(task, Run());
          EXPECT_CALL(*thread_controller_.trace_observer_,
                      OnPhaseRecorded(ThreadController::kApplicationTask));
          EXPECT_EQ(thread_controller_.DoWork().delayed_run_time,
                    TimeTicks::Max());

          testing::Mock::VerifyAndClearExpectations(
              &*thread_controller_.trace_observer_);

          EXPECT_CALL(*thread_controller_.trace_observer_,
                      OnPhaseRecorded(ThreadController::kIdleWork));
          EXPECT_CALL(*thread_controller_.trace_observer_,
                      OnThreadControllerActiveEnd);
          EXPECT_FALSE(thread_controller_.DoIdleWork());

          testing::Mock::VerifyAndClearExpectations(
              &*thread_controller_.trace_observer_);
        }
      }));

  RunLoop().Run();
}

TEST_F(ThreadControllerWithMessagePumpTest,
       ThreadControllerActiveMultipleApplicationTasks) {
  SingleThreadTaskRunner::CurrentDefaultHandle handle(
      MakeRefCounted<FakeTaskRunner>());

  thread_controller_.InstallTraceObserver();

  testing::InSequence sequence;

  RunLoop run_loop;
  EXPECT_CALL(*thread_controller_.trace_observer_,
              OnThreadControllerActiveBegin);
  EXPECT_CALL(*message_pump_, Run(_))
      .WillOnce(Invoke([&](MessagePump::Delegate* delegate) {
        MockCallback<OnceClosure> tasks[5];
        // Post 5 tasks, run them, go idle. Expected to only exit
        // "ThreadController active" state at the end.
        for (auto& t : tasks)
          task_source_.AddTask(FROM_HERE, t.Get());
        for (size_t i = 0; i < std::size(tasks); ++i) {
          const TimeTicks expected_delayed_run_time =
              i < std::size(tasks) - 1 ? TimeTicks() : TimeTicks::Max();

          // The first pass begins with a kPumpOverhead phase as the
          // RunLoop::Run() begins active and the subsequent ones also do
          // (between the end of the last kChromeTask and the next
          // kSelectingApplicationTask).
          EXPECT_CALL(*thread_controller_.trace_observer_,
                      OnPhaseRecorded(ThreadController::kPumpOverhead));

          EXPECT_CALL(
              *thread_controller_.trace_observer_,
              OnPhaseRecorded(ThreadController::kSelectingApplicationTask));
          EXPECT_CALL(tasks[i], Run());
          EXPECT_CALL(*thread_controller_.trace_observer_,
                      OnPhaseRecorded(ThreadController::kApplicationTask));
          EXPECT_EQ(thread_controller_.DoWork().delayed_run_time,
                    expected_delayed_run_time);
        }

        EXPECT_CALL(*thread_controller_.trace_observer_,
                    OnPhaseRecorded(ThreadController::kIdleWork));
        EXPECT_CALL(*thread_controller_.trace_observer_,
                    OnThreadControllerActiveEnd);
        EXPECT_FALSE(thread_controller_.DoIdleWork());
      }));

  RunLoop().Run();
}

TEST_F(ThreadControllerWithMessagePumpTest,
       ThreadControllerActiveWakeUpForNothing) {
  SingleThreadTaskRunner::CurrentDefaultHandle handle(
      MakeRefCounted<FakeTaskRunner>());

  thread_controller_.InstallTraceObserver();

  testing::InSequence sequence;

  RunLoop run_loop;
  EXPECT_CALL(*thread_controller_.trace_observer_,
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
            EXPECT_CALL(*thread_controller_.trace_observer_,
                        OnThreadControllerActiveBegin);
          } else {
            // As-in ThreadControllerActiveSingleApplicationTask.
            EXPECT_CALL(*thread_controller_.trace_observer_,
                        OnPhaseRecorded(ThreadController::kPumpOverhead));
          }
          EXPECT_CALL(
              *thread_controller_.trace_observer_,
              OnPhaseRecorded(ThreadController::kSelectingApplicationTask));
          EXPECT_CALL(*thread_controller_.trace_observer_,
                      OnPhaseRecorded(ThreadController::kApplicationTask));
          EXPECT_EQ(thread_controller_.DoWork().delayed_run_time,
                    TimeTicks::Max());

          testing::Mock::VerifyAndClearExpectations(
              &*thread_controller_.trace_observer_);

          EXPECT_CALL(*thread_controller_.trace_observer_,
                      OnPhaseRecorded(ThreadController::kIdleWork));
          EXPECT_CALL(*thread_controller_.trace_observer_,
                      OnThreadControllerActiveEnd);
          EXPECT_FALSE(thread_controller_.DoIdleWork());

          testing::Mock::VerifyAndClearExpectations(
              &*thread_controller_.trace_observer_);
        }
      }));

  RunLoop().Run();
}

#if not BUILDFLAG(IS_ANDROID)
TEST_F(ThreadControllerWithMessagePumpTest, DoWorkBatches) {
  base::test::ScopedFeatureList scoped_feature_list_;

  scoped_feature_list_.InitAndEnableFeature(kRunTasksByBatches);
  ThreadControllerWithMessagePumpImpl::InitializeFeatures();

  int task_counter = 0;
  for (int i = 0; i < 2; i++) {
    task_source_.AddTask(
        FROM_HERE, BindLambdaForTesting([&] { task_counter++; }), TimeTicks());
  }
  thread_controller_.DoWork();

  EXPECT_EQ(task_counter, 2);
  ThreadControllerWithMessagePumpImpl::ResetFeatures();
}

TEST_F(ThreadControllerWithMessagePumpTest, DoWorkBatchesForSetTime) {
  base::test::ScopedFeatureList scoped_feature_list_;

  scoped_feature_list_.InitAndEnableFeature(kRunTasksByBatches);
  ThreadControllerWithMessagePumpImpl::InitializeFeatures();

  int task_counter = 0;

  for (int i = 0; i < 4; i++) {
    task_source_.AddTask(FROM_HERE, BindLambdaForTesting([&] {
                           clock_.Advance(Milliseconds(4));
                           task_counter++;
                         }));
  }
  thread_controller_.DoWork();

  EXPECT_EQ(task_counter, 2);
  ThreadControllerWithMessagePumpImpl::ResetFeatures();
}
#endif  // BUILDFLAG(IS_ANDROID)
TEST_F(ThreadControllerWithMessagePumpTest,
       ThreadControllerActiveAdvancedNesting) {
  SingleThreadTaskRunner::CurrentDefaultHandle handle(
      MakeRefCounted<FakeTaskRunner>());

  thread_controller_.InstallTraceObserver();

  testing::InSequence sequence;

  RunLoop run_loop;
  EXPECT_CALL(*thread_controller_.trace_observer_,
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
        //     G: Run one (enter nested active)
        //     H: exit nested loop (exit nested active)
        // I: Run the next one, go idle (remain active, exit active)
        // J: Post/run one more task, go idle (enter active, exit active)
        // 😅

        // A:
        task_source_.AddTask(FROM_HERE, tasks[0].Get());
        task_source_.AddTask(FROM_HERE, tasks[1].Get());

        EXPECT_CALL(*thread_controller_.trace_observer_,
                    OnPhaseRecorded(ThreadController::kPumpOverhead));
        EXPECT_CALL(
            *thread_controller_.trace_observer_,
            OnPhaseRecorded(ThreadController::kSelectingApplicationTask));
        EXPECT_CALL(tasks[0], Run()).WillOnce(Invoke([&]() {
          // C:
          // kChromeTask phase is suspended when the nested loop is entered.
          EXPECT_CALL(*thread_controller_.trace_observer_,
                      OnPhaseRecorded(ThreadController::kApplicationTask));
          EXPECT_CALL(*thread_controller_.trace_observer_,
                      OnThreadControllerActiveBegin);
          EXPECT_CALL(*message_pump_, Run(_))
              .WillOnce(Invoke([&](MessagePump::Delegate* delegate) {
                // D:
                EXPECT_CALL(tasks[1], Run());
                EXPECT_EQ(thread_controller_.DoWork().delayed_run_time,
                          TimeTicks::Max());
                testing::Mock::VerifyAndClearExpectations(
                    &*thread_controller_.trace_observer_);

                // E:
                EXPECT_CALL(*thread_controller_.trace_observer_,
                            OnThreadControllerActiveEnd);
                EXPECT_FALSE(thread_controller_.DoIdleWork());
                testing::Mock::VerifyAndClearExpectations(
                    &*thread_controller_.trace_observer_);

                // F:
                task_source_.AddTask(FROM_HERE, tasks[2].Get());
                task_source_.AddTask(FROM_HERE, tasks[3].Get());

                // G:
                EXPECT_CALL(*thread_controller_.trace_observer_,
                            OnThreadControllerActiveBegin);
                EXPECT_CALL(tasks[2], Run());
                EXPECT_EQ(thread_controller_.DoWork().delayed_run_time,
                          TimeTicks());
                testing::Mock::VerifyAndClearExpectations(
                    &*thread_controller_.trace_observer_);

                // H:
                EXPECT_CALL(*thread_controller_.trace_observer_,
                            OnThreadControllerActiveEnd);
                // The kNested phase (C) ends when the RunLoop exits...
                EXPECT_CALL(*thread_controller_.trace_observer_,
                            OnPhaseRecorded(ThreadController::kNested));
                // ... and then the calling task (B) ends.
                EXPECT_CALL(
                    *thread_controller_.trace_observer_,
                    OnPhaseRecorded(ThreadController::kApplicationTask));
              }));
          RunLoop(RunLoop::Type::kNestableTasksAllowed).Run();
        }));

        // B:
        EXPECT_EQ(thread_controller_.DoWork().delayed_run_time, TimeTicks());
        testing::Mock::VerifyAndClearExpectations(
            &*thread_controller_.trace_observer_);

        // I:
        EXPECT_CALL(*thread_controller_.trace_observer_,
                    OnPhaseRecorded(ThreadController::kPumpOverhead));
        EXPECT_CALL(
            *thread_controller_.trace_observer_,
            OnPhaseRecorded(ThreadController::kSelectingApplicationTask));
        EXPECT_CALL(tasks[3], Run());
        EXPECT_CALL(*thread_controller_.trace_observer_,
                    OnPhaseRecorded(ThreadController::kApplicationTask));
        EXPECT_EQ(thread_controller_.DoWork().delayed_run_time,
                  TimeTicks::Max());
        testing::Mock::VerifyAndClearExpectations(
            &*thread_controller_.trace_observer_);

        EXPECT_CALL(*thread_controller_.trace_observer_,
                    OnPhaseRecorded(ThreadController::kIdleWork));
        EXPECT_CALL(*thread_controller_.trace_observer_,
                    OnThreadControllerActiveEnd);
        EXPECT_FALSE(thread_controller_.DoIdleWork());
        testing::Mock::VerifyAndClearExpectations(
            &*thread_controller_.trace_observer_);

        // J:
        task_source_.AddTask(FROM_HERE, tasks[4].Get());
        EXPECT_CALL(*thread_controller_.trace_observer_,
                    OnThreadControllerActiveBegin);
        EXPECT_CALL(
            *thread_controller_.trace_observer_,
            OnPhaseRecorded(ThreadController::kSelectingApplicationTask));
        EXPECT_CALL(tasks[4], Run());
        EXPECT_CALL(*thread_controller_.trace_observer_,
                    OnPhaseRecorded(ThreadController::kApplicationTask));
        EXPECT_EQ(thread_controller_.DoWork().delayed_run_time,
                  TimeTicks::Max());
        testing::Mock::VerifyAndClearExpectations(
            &*thread_controller_.trace_observer_);

        EXPECT_CALL(*thread_controller_.trace_observer_,
                    OnPhaseRecorded(ThreadController::kIdleWork));
        EXPECT_CALL(*thread_controller_.trace_observer_,
                    OnThreadControllerActiveEnd);
        EXPECT_FALSE(thread_controller_.DoIdleWork());
        testing::Mock::VerifyAndClearExpectations(
            &*thread_controller_.trace_observer_);
      }));

  RunLoop().Run();
}

TEST_F(ThreadControllerWithMessagePumpTest,
       ThreadControllerActiveNestedNativeLoop) {
  SingleThreadTaskRunner::CurrentDefaultHandle handle(
      MakeRefCounted<FakeTaskRunner>());

  thread_controller_.InstallTraceObserver();

  testing::InSequence sequence;

  RunLoop run_loop;
  EXPECT_CALL(*thread_controller_.trace_observer_,
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
        task_source_.AddTask(FROM_HERE, tasks[0].Get());
        task_source_.AddTask(FROM_HERE, tasks[1].Get());

        EXPECT_CALL(*thread_controller_.trace_observer_,
                    OnPhaseRecorded(ThreadController::kPumpOverhead));
        EXPECT_CALL(
            *thread_controller_.trace_observer_,
            OnPhaseRecorded(ThreadController::kSelectingApplicationTask));
        EXPECT_CALL(tasks[0], Run()).WillOnce(Invoke([&]() {
          // C:
          EXPECT_FALSE(thread_controller_.IsTaskExecutionAllowed());
          EXPECT_CALL(*message_pump_, ScheduleWork());
          thread_controller_.SetTaskExecutionAllowed(true);
          // i.e. simulate that something runs code within the scope of a
          // ScopedAllowApplicationTasksInNativeNestedLoop and ends up entering
          // a nested native loop which would invoke OnBeginWorkItem()

          // D:

          // kChromeTask phase is suspended when the nested native loop is
          // entered.
          EXPECT_CALL(*thread_controller_.trace_observer_,
                      OnPhaseRecorded(ThreadController::kApplicationTask));
          EXPECT_CALL(*thread_controller_.trace_observer_,
                      OnThreadControllerActiveBegin);
          thread_controller_.OnBeginWorkItem();
          testing::Mock::VerifyAndClearExpectations(
              &*thread_controller_.trace_observer_);
          thread_controller_.OnEndWorkItem();

          // E:
          EXPECT_CALL(tasks[1], Run());
          EXPECT_EQ(thread_controller_.DoWork().delayed_run_time,
                    TimeTicks::Max());
          testing::Mock::VerifyAndClearExpectations(
              &*thread_controller_.trace_observer_);

          // F:
          EXPECT_CALL(*thread_controller_.trace_observer_,
                      OnThreadControllerActiveEnd);
          EXPECT_FALSE(thread_controller_.DoIdleWork());
          testing::Mock::VerifyAndClearExpectations(
              &*thread_controller_.trace_observer_);

          // G:
          EXPECT_CALL(*thread_controller_.trace_observer_,
                      OnThreadControllerActiveBegin);
          thread_controller_.OnBeginWorkItem();
          testing::Mock::VerifyAndClearExpectations(
              &*thread_controller_.trace_observer_);
          thread_controller_.OnEndWorkItem();

          // H:
          EXPECT_CALL(*thread_controller_.trace_observer_,
                      OnThreadControllerActiveEnd);
          // The kNested phase (C) ends when the RunLoop exits and kChromeTask
          // doesn't resume as task (B) is done.
          EXPECT_CALL(*thread_controller_.trace_observer_,
                      OnPhaseRecorded(ThreadController::kNested));
          thread_controller_.SetTaskExecutionAllowed(false);
        }));

        // B:
        EXPECT_EQ(thread_controller_.DoWork().delayed_run_time,
                  TimeTicks::Max());
        testing::Mock::VerifyAndClearExpectations(
            &*thread_controller_.trace_observer_);

        // I:
        EXPECT_CALL(*thread_controller_.trace_observer_,
                    OnPhaseRecorded(ThreadController::kIdleWork));
        EXPECT_CALL(*thread_controller_.trace_observer_,
                    OnThreadControllerActiveEnd);
        EXPECT_FALSE(thread_controller_.DoIdleWork());
        testing::Mock::VerifyAndClearExpectations(
            &*thread_controller_.trace_observer_);
      }));

  RunLoop().Run();
}

TEST_F(ThreadControllerWithMessagePumpTest,
       ThreadControllerActiveUnusedNativeLoop) {
  SingleThreadTaskRunner::CurrentDefaultHandle handle(
      MakeRefCounted<FakeTaskRunner>());

  thread_controller_.InstallTraceObserver();

  testing::InSequence sequence;

  RunLoop run_loop;
  EXPECT_CALL(*thread_controller_.trace_observer_,
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
        task_source_.AddTask(FROM_HERE, tasks[0].Get());
        task_source_.AddTask(FROM_HERE, tasks[1].Get());

        EXPECT_CALL(*thread_controller_.trace_observer_,
                    OnPhaseRecorded(ThreadController::kPumpOverhead));
        EXPECT_CALL(
            *thread_controller_.trace_observer_,
            OnPhaseRecorded(ThreadController::kSelectingApplicationTask));
        EXPECT_CALL(tasks[0], Run()).WillOnce(Invoke([&]() {
          // C:
          EXPECT_FALSE(thread_controller_.IsTaskExecutionAllowed());
          EXPECT_CALL(*message_pump_, ScheduleWork());
          thread_controller_.SetTaskExecutionAllowed(true);

          // D:
          thread_controller_.SetTaskExecutionAllowed(false);
          EXPECT_CALL(*thread_controller_.trace_observer_,
                      OnPhaseRecorded(ThreadController::kApplicationTask));
        }));

        // B:
        EXPECT_EQ(thread_controller_.DoWork().delayed_run_time, TimeTicks());
        testing::Mock::VerifyAndClearExpectations(
            &*thread_controller_.trace_observer_);

        // E:
        EXPECT_CALL(*thread_controller_.trace_observer_,
                    OnPhaseRecorded(ThreadController::kPumpOverhead));
        EXPECT_CALL(
            *thread_controller_.trace_observer_,
            OnPhaseRecorded(ThreadController::kSelectingApplicationTask));
        EXPECT_CALL(tasks[1], Run());
        EXPECT_CALL(*thread_controller_.trace_observer_,
                    OnPhaseRecorded(ThreadController::kApplicationTask));
        EXPECT_EQ(thread_controller_.DoWork().delayed_run_time,
                  TimeTicks::Max());
        testing::Mock::VerifyAndClearExpectations(
            &*thread_controller_.trace_observer_);

        // F:
        EXPECT_CALL(*thread_controller_.trace_observer_,
                    OnPhaseRecorded(ThreadController::kIdleWork));
        EXPECT_CALL(*thread_controller_.trace_observer_,
                    OnThreadControllerActiveEnd);
        EXPECT_FALSE(thread_controller_.DoIdleWork());
        testing::Mock::VerifyAndClearExpectations(
            &*thread_controller_.trace_observer_);
      }));

  RunLoop().Run();
}

TEST_F(ThreadControllerWithMessagePumpTest,
       ThreadControllerActiveNestedNativeLoopWithoutAllowance) {
  SingleThreadTaskRunner::CurrentDefaultHandle handle(
      MakeRefCounted<FakeTaskRunner>());

  thread_controller_.InstallTraceObserver();

  testing::InSequence sequence;

  RunLoop run_loop;
  EXPECT_CALL(*thread_controller_.trace_observer_,
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
        task_source_.AddTask(FROM_HERE, tasks[0].Get());
        task_source_.AddTask(FROM_HERE, tasks[1].Get());

        EXPECT_CALL(*thread_controller_.trace_observer_,
                    OnPhaseRecorded(ThreadController::kPumpOverhead));
        EXPECT_CALL(
            *thread_controller_.trace_observer_,
            OnPhaseRecorded(ThreadController::kSelectingApplicationTask));
        EXPECT_CALL(tasks[0], Run()).WillOnce(Invoke([&]() {
          // C:
          // D:
          // kChromeTask phase is suspended when the nested loop is entered.
          EXPECT_CALL(*thread_controller_.trace_observer_,
                      OnPhaseRecorded(ThreadController::kApplicationTask));
          EXPECT_CALL(*thread_controller_.trace_observer_,
                      OnThreadControllerActiveBegin);
          thread_controller_.OnBeginWorkItem();
          testing::Mock::VerifyAndClearExpectations(
              &*thread_controller_.trace_observer_);
          thread_controller_.OnEndWorkItem();

          // E:
          EXPECT_CALL(*thread_controller_.trace_observer_,
                      OnThreadControllerActiveEnd);
          EXPECT_CALL(*thread_controller_.trace_observer_,
                      OnPhaseRecorded(ThreadController::kNested));
        }));

        // B:
        EXPECT_EQ(thread_controller_.DoWork().delayed_run_time, TimeTicks());
        testing::Mock::VerifyAndClearExpectations(
            &*thread_controller_.trace_observer_);

        // F:
        EXPECT_CALL(*thread_controller_.trace_observer_,
                    OnPhaseRecorded(ThreadController::kPumpOverhead));
        EXPECT_CALL(
            *thread_controller_.trace_observer_,
            OnPhaseRecorded(ThreadController::kSelectingApplicationTask));
        EXPECT_CALL(tasks[1], Run());
        EXPECT_CALL(*thread_controller_.trace_observer_,
                    OnPhaseRecorded(ThreadController::kApplicationTask));
        EXPECT_EQ(thread_controller_.DoWork().delayed_run_time,
                  TimeTicks::Max());

        // G:
        EXPECT_CALL(*thread_controller_.trace_observer_,
                    OnPhaseRecorded(ThreadController::kIdleWork));
        EXPECT_CALL(*thread_controller_.trace_observer_,
                    OnThreadControllerActiveEnd);
        EXPECT_FALSE(thread_controller_.DoIdleWork());
        testing::Mock::VerifyAndClearExpectations(
            &*thread_controller_.trace_observer_);
      }));

  RunLoop().Run();
}

TEST_F(ThreadControllerWithMessagePumpTest,
       ThreadControllerActiveMultipleNativeLoopsUnderOneApplicationTask) {
  SingleThreadTaskRunner::CurrentDefaultHandle handle(
      MakeRefCounted<FakeTaskRunner>());

  thread_controller_.InstallTraceObserver();

  testing::InSequence sequence;

  RunLoop run_loop;
  EXPECT_CALL(*thread_controller_.trace_observer_,
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
        task_source_.AddTask(FROM_HERE, tasks[0].Get());

        EXPECT_CALL(*thread_controller_.trace_observer_,
                    OnPhaseRecorded(ThreadController::kPumpOverhead));
        EXPECT_CALL(
            *thread_controller_.trace_observer_,
            OnPhaseRecorded(ThreadController::kSelectingApplicationTask));
        EXPECT_CALL(tasks[0], Run()).WillOnce(Invoke([&]() {
          for (int i = 0; i < 2; ++i) {
            // C & F:
            EXPECT_FALSE(thread_controller_.IsTaskExecutionAllowed());
            EXPECT_CALL(*message_pump_, ScheduleWork());
            thread_controller_.SetTaskExecutionAllowed(true);

            // D & G:
            if (i == 0) {
              // kChromeTask phase is suspended when the nested loop is entered.
              EXPECT_CALL(*thread_controller_.trace_observer_,
                          OnPhaseRecorded(ThreadController::kApplicationTask));
              EXPECT_CALL(*thread_controller_.trace_observer_,
                          OnThreadControllerActiveBegin);
            }
            thread_controller_.OnBeginWorkItem();
            testing::Mock::VerifyAndClearExpectations(
                &*thread_controller_.trace_observer_);
            thread_controller_.OnEndWorkItem();

            // E & H:
            thread_controller_.SetTaskExecutionAllowed(false);
            testing::Mock::VerifyAndClearExpectations(
                &*thread_controller_.trace_observer_);
          }

          // I:
          EXPECT_CALL(*thread_controller_.trace_observer_,
                      OnThreadControllerActiveEnd);
          EXPECT_CALL(*thread_controller_.trace_observer_,
                      OnPhaseRecorded(ThreadController::kNested));
        }));

        // B:
        EXPECT_EQ(thread_controller_.DoWork().delayed_run_time,
                  TimeTicks::Max());
        testing::Mock::VerifyAndClearExpectations(
            &*thread_controller_.trace_observer_);

        // J:
        EXPECT_CALL(*thread_controller_.trace_observer_,
                    OnPhaseRecorded(ThreadController::kIdleWork));
        EXPECT_CALL(*thread_controller_.trace_observer_,
                    OnThreadControllerActiveEnd);
        EXPECT_FALSE(thread_controller_.DoIdleWork());
        testing::Mock::VerifyAndClearExpectations(
            &*thread_controller_.trace_observer_);
      }));

  RunLoop().Run();
}

TEST_F(ThreadControllerWithMessagePumpTest,
       ThreadControllerActiveNativeLoopsReachingIdle) {
  SingleThreadTaskRunner::CurrentDefaultHandle handle(
      MakeRefCounted<FakeTaskRunner>());

  thread_controller_.InstallTraceObserver();

  testing::InSequence sequence;

  RunLoop run_loop;
  EXPECT_CALL(*thread_controller_.trace_observer_,
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
        task_source_.AddTask(FROM_HERE, task.Get());

        EXPECT_CALL(*thread_controller_.trace_observer_,
                    OnPhaseRecorded(ThreadController::kPumpOverhead));
        EXPECT_CALL(
            *thread_controller_.trace_observer_,
            OnPhaseRecorded(ThreadController::kSelectingApplicationTask));
        EXPECT_CALL(task, Run()).WillOnce(Invoke([&]() {
          // C:
          EXPECT_FALSE(thread_controller_.IsTaskExecutionAllowed());
          EXPECT_CALL(*message_pump_, ScheduleWork());
          thread_controller_.SetTaskExecutionAllowed(true);

          // D:
          // kChromeTask phase is suspended when the nested loop is entered.
          EXPECT_CALL(*thread_controller_.trace_observer_,
                      OnPhaseRecorded(ThreadController::kApplicationTask));
          EXPECT_CALL(*thread_controller_.trace_observer_,
                      OnThreadControllerActiveBegin);
          thread_controller_.OnBeginWorkItem();
          testing::Mock::VerifyAndClearExpectations(
              &*thread_controller_.trace_observer_);
          thread_controller_.OnEndWorkItem();

          // E:
          EXPECT_CALL(*thread_controller_.trace_observer_,
                      OnThreadControllerActiveEnd);
          thread_controller_.BeforeWait();
          testing::Mock::VerifyAndClearExpectations(
              &*thread_controller_.trace_observer_);

          // F:
          EXPECT_CALL(*thread_controller_.trace_observer_,
                      OnThreadControllerActiveBegin);
          thread_controller_.OnBeginWorkItem();
          testing::Mock::VerifyAndClearExpectations(
              &*thread_controller_.trace_observer_);
          thread_controller_.OnEndWorkItem();

          // G:
          thread_controller_.SetTaskExecutionAllowed(false);

          // H:
          EXPECT_CALL(*thread_controller_.trace_observer_,
                      OnThreadControllerActiveEnd);
          EXPECT_CALL(*thread_controller_.trace_observer_,
                      OnPhaseRecorded(ThreadController::kNested));
        }));

        // B:
        EXPECT_EQ(thread_controller_.DoWork().delayed_run_time,
                  TimeTicks::Max());
        testing::Mock::VerifyAndClearExpectations(
            &*thread_controller_.trace_observer_);

        // I:
        EXPECT_CALL(*thread_controller_.trace_observer_,
                    OnPhaseRecorded(ThreadController::kIdleWork));
        EXPECT_CALL(*thread_controller_.trace_observer_,
                    OnThreadControllerActiveEnd);
        EXPECT_FALSE(thread_controller_.DoIdleWork());
        testing::Mock::VerifyAndClearExpectations(
            &*thread_controller_.trace_observer_);
      }));

  RunLoop().Run();
}

TEST_F(ThreadControllerWithMessagePumpTest,
       ThreadControllerActiveQuitNestedWhileApplicationIdle) {
  SingleThreadTaskRunner::CurrentDefaultHandle handle(
      MakeRefCounted<FakeTaskRunner>());

  thread_controller_.InstallTraceObserver();

  testing::InSequence sequence;

  RunLoop run_loop;
  EXPECT_CALL(*thread_controller_.trace_observer_,
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
        task_source_.AddTask(FROM_HERE, tasks[0].Get());
        task_source_.AddTask(FROM_HERE, tasks[1].Get());

        EXPECT_CALL(*thread_controller_.trace_observer_,
                    OnPhaseRecorded(ThreadController::kPumpOverhead));
        EXPECT_CALL(
            *thread_controller_.trace_observer_,
            OnPhaseRecorded(ThreadController::kSelectingApplicationTask));
        EXPECT_CALL(tasks[0], Run()).WillOnce(Invoke([&]() {
          // C:
          EXPECT_FALSE(thread_controller_.IsTaskExecutionAllowed());
          EXPECT_CALL(*message_pump_, ScheduleWork());
          thread_controller_.SetTaskExecutionAllowed(true);

          // D:
          // kChromeTask phase is suspended when the nested loop is entered.
          EXPECT_CALL(*thread_controller_.trace_observer_,
                      OnPhaseRecorded(ThreadController::kApplicationTask));
          EXPECT_CALL(*thread_controller_.trace_observer_,
                      OnThreadControllerActiveBegin);
          EXPECT_CALL(tasks[1], Run());
          EXPECT_EQ(thread_controller_.DoWork().delayed_run_time,
                    TimeTicks::Max());
          testing::Mock::VerifyAndClearExpectations(
              &*thread_controller_.trace_observer_);

          // E:
          EXPECT_CALL(*thread_controller_.trace_observer_,
                      OnThreadControllerActiveEnd);
          thread_controller_.BeforeWait();
          testing::Mock::VerifyAndClearExpectations(
              &*thread_controller_.trace_observer_);

          // F + G:
          thread_controller_.SetTaskExecutionAllowed(false);

          // H:
          EXPECT_CALL(*thread_controller_.trace_observer_,
                      OnPhaseRecorded(ThreadController::kNested));
        }));

        // B:
        EXPECT_EQ(thread_controller_.DoWork().delayed_run_time,
                  TimeTicks::Max());
        testing::Mock::VerifyAndClearExpectations(
            &*thread_controller_.trace_observer_);

        // I:
        EXPECT_CALL(*thread_controller_.trace_observer_,
                    OnPhaseRecorded(ThreadController::kIdleWork));
        EXPECT_CALL(*thread_controller_.trace_observer_,
                    OnThreadControllerActiveEnd);
        EXPECT_FALSE(thread_controller_.DoIdleWork());
        testing::Mock::VerifyAndClearExpectations(
            &*thread_controller_.trace_observer_);
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
  SingleThreadTaskRunner::CurrentDefaultHandle handle(
      MakeRefCounted<FakeTaskRunner>());

  thread_controller_.InstallTraceObserver();

  testing::InSequence sequence;

  RunLoop run_loop;
  EXPECT_CALL(*thread_controller_.trace_observer_,
              OnThreadControllerActiveBegin);
  EXPECT_CALL(*message_pump_, Run(_))
      .WillOnce(Invoke([&](MessagePump::Delegate* delegate) {
        // Start this test idle for a change.
        EXPECT_CALL(*thread_controller_.trace_observer_,
                    OnPhaseRecorded(ThreadController::kIdleWork));
        EXPECT_CALL(*thread_controller_.trace_observer_,
                    OnThreadControllerActiveEnd);
        EXPECT_FALSE(thread_controller_.DoIdleWork());
        testing::Mock::VerifyAndClearExpectations(
            &*thread_controller_.trace_observer_);

        MockCallback<OnceClosure> task;

        // A: Post 1 application task
        // B: Run a native task
        //   C: Enter a native nested loop (application tasks still allowed)
        //     D: Run the application task (enter nested active)
        // E: End the native task (exit nested active)
        // F: Go idle (exit active)

        // A:
        task_source_.AddTask(FROM_HERE, task.Get());

        EXPECT_CALL(*thread_controller_.trace_observer_,
                    OnThreadControllerActiveBegin)
            .WillOnce(Invoke([&]() {
              // C:
              EXPECT_TRUE(thread_controller_.IsTaskExecutionAllowed());

              // D:
              // kNativeWork phase is suspended when the nested loop is entered.
              EXPECT_CALL(*thread_controller_.trace_observer_,
                          OnPhaseRecorded(ThreadController::kNativeWork));
              EXPECT_CALL(*thread_controller_.trace_observer_,
                          OnThreadControllerActiveBegin);
              EXPECT_CALL(task, Run());
              EXPECT_EQ(thread_controller_.DoWork().delayed_run_time,
                        TimeTicks::Max());
              testing::Mock::VerifyAndClearExpectations(
                  &*thread_controller_.trace_observer_);
            }));

        // B:
        thread_controller_.OnBeginWorkItem();
        testing::Mock::VerifyAndClearExpectations(
            &*thread_controller_.trace_observer_);

        // E:
        EXPECT_CALL(*thread_controller_.trace_observer_,
                    OnThreadControllerActiveEnd);
        EXPECT_CALL(*thread_controller_.trace_observer_,
                    OnPhaseRecorded(ThreadController::kNested));
        thread_controller_.OnEndWorkItem();

        // F:
        EXPECT_CALL(*thread_controller_.trace_observer_,
                    OnPhaseRecorded(ThreadController::kIdleWork));
        EXPECT_CALL(*thread_controller_.trace_observer_,
                    OnThreadControllerActiveEnd);
        EXPECT_FALSE(thread_controller_.DoIdleWork());
        testing::Mock::VerifyAndClearExpectations(
            &*thread_controller_.trace_observer_);
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
  SingleThreadTaskRunner::CurrentDefaultHandle handle(
      MakeRefCounted<FakeTaskRunner>());

  thread_controller_.InstallTraceObserver();

  testing::InSequence sequence;

  RunLoop run_loop;
  EXPECT_CALL(*thread_controller_.trace_observer_,
              OnThreadControllerActiveBegin);
  EXPECT_CALL(*message_pump_, Run(_))
      .WillOnce(Invoke([&](MessagePump::Delegate* delegate) {
        // Start this test idle for a change.
        EXPECT_CALL(*thread_controller_.trace_observer_,
                    OnPhaseRecorded(ThreadController::kIdleWork));
        EXPECT_CALL(*thread_controller_.trace_observer_,
                    OnThreadControllerActiveEnd);
        EXPECT_FALSE(thread_controller_.DoIdleWork());
        testing::Mock::VerifyAndClearExpectations(
            &*thread_controller_.trace_observer_);

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
        task_source_.AddTask(FROM_HERE, task.Get());

        EXPECT_CALL(*thread_controller_.trace_observer_,
                    OnThreadControllerActiveBegin)
            .WillOnce(Invoke([&]() {
              // C + D:
              EXPECT_TRUE(thread_controller_.IsTaskExecutionAllowed());
              EXPECT_CALL(*message_pump_, ScheduleWork());
              thread_controller_.SetTaskExecutionAllowed(true);
              testing::Mock::VerifyAndClearExpectations(
                  &*thread_controller_.trace_observer_);

              // E:
              // kNativeWork phase is suspended when the nested loop is entered.
              EXPECT_CALL(*thread_controller_.trace_observer_,
                          OnPhaseRecorded(ThreadController::kNativeWork));
              EXPECT_CALL(*thread_controller_.trace_observer_,
                          OnThreadControllerActiveBegin);
              EXPECT_CALL(task, Run());
              EXPECT_EQ(thread_controller_.DoWork().delayed_run_time,
                        TimeTicks::Max());
              testing::Mock::VerifyAndClearExpectations(
                  &*thread_controller_.trace_observer_);

              // F:
              EXPECT_CALL(*message_pump_, ScheduleWork());
              thread_controller_.SetTaskExecutionAllowed(true);
            }));

        // B:
        thread_controller_.OnBeginWorkItem();
        testing::Mock::VerifyAndClearExpectations(
            &*thread_controller_.trace_observer_);

        // G:
        EXPECT_CALL(*thread_controller_.trace_observer_,
                    OnThreadControllerActiveEnd);
        EXPECT_CALL(*thread_controller_.trace_observer_,
                    OnPhaseRecorded(ThreadController::kNested));
        thread_controller_.OnEndWorkItem();

        // H:
        EXPECT_CALL(*thread_controller_.trace_observer_,
                    OnPhaseRecorded(ThreadController::kIdleWork));
        EXPECT_CALL(*thread_controller_.trace_observer_,
                    OnThreadControllerActiveEnd);
        EXPECT_FALSE(thread_controller_.DoIdleWork());
        testing::Mock::VerifyAndClearExpectations(
            &*thread_controller_.trace_observer_);
      }));

  RunLoop().Run();
}

// Verify that the kScheduled phase is emitted when coming out of idle and
// `queue_time` is set on PendingTasks.
TEST_F(ThreadControllerWithMessagePumpTest, MessagePumpPhasesWithQueuingTime) {
  SingleThreadTaskRunner::CurrentDefaultHandle handle(
      MakeRefCounted<FakeTaskRunner>());

  thread_controller_.InstallTraceObserver();

  testing::InSequence sequence;

  RunLoop run_loop;
  EXPECT_CALL(*thread_controller_.trace_observer_,
              OnThreadControllerActiveBegin);
  EXPECT_CALL(*message_pump_, Run(_))
      .WillOnce(Invoke([&](MessagePump::Delegate* delegate) {
        // Start this test idle.
        EXPECT_CALL(*thread_controller_.trace_observer_,
                    OnPhaseRecorded(ThreadController::kIdleWork));
        EXPECT_CALL(*thread_controller_.trace_observer_,
                    OnThreadControllerActiveEnd);
        EXPECT_FALSE(thread_controller_.DoIdleWork());
        testing::Mock::VerifyAndClearExpectations(
            &*thread_controller_.trace_observer_);

        MockCallback<OnceClosure> task1;
        task_source_.AddTask(FROM_HERE, task1.Get(),
                             /*delayed_run_time=*/TimeTicks(),
                             /*queue_time=*/clock_.NowTicks());
        MockCallback<OnceClosure> task2;
        task_source_.AddTask(FROM_HERE, task2.Get(),
                             /*delayed_run_time=*/TimeTicks(),
                             /*queue_time=*/clock_.NowTicks());
        // kScheduled is only emitted if in past.
        clock_.Advance(Milliseconds(1));

        EXPECT_CALL(*thread_controller_.trace_observer_,
                    OnThreadControllerActiveBegin);
        EXPECT_CALL(*thread_controller_.trace_observer_,
                    OnPhaseRecorded(ThreadController::kScheduled));
        EXPECT_CALL(
            *thread_controller_.trace_observer_,
            OnPhaseRecorded(ThreadController::kSelectingApplicationTask));
        EXPECT_CALL(task1, Run());
        EXPECT_CALL(*thread_controller_.trace_observer_,
                    OnPhaseRecorded(ThreadController::kApplicationTask));
        EXPECT_EQ(thread_controller_.DoWork().delayed_run_time, TimeTicks());

        EXPECT_CALL(*thread_controller_.trace_observer_,
                    OnPhaseRecorded(ThreadController::kPumpOverhead));
        EXPECT_CALL(
            *thread_controller_.trace_observer_,
            OnPhaseRecorded(ThreadController::kSelectingApplicationTask));
        EXPECT_CALL(task2, Run());
        EXPECT_CALL(*thread_controller_.trace_observer_,
                    OnPhaseRecorded(ThreadController::kApplicationTask));
        EXPECT_EQ(thread_controller_.DoWork().delayed_run_time,
                  TimeTicks::Max());

        testing::Mock::VerifyAndClearExpectations(
            &*thread_controller_.trace_observer_);

        EXPECT_CALL(*thread_controller_.trace_observer_,
                    OnPhaseRecorded(ThreadController::kIdleWork));
        EXPECT_CALL(*thread_controller_.trace_observer_,
                    OnThreadControllerActiveEnd);
        EXPECT_FALSE(thread_controller_.DoIdleWork());

        testing::Mock::VerifyAndClearExpectations(
            &*thread_controller_.trace_observer_);
      }));

  RunLoop().Run();
}

}  // namespace base::sequence_manager::internal
