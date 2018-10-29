// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/sequence_manager/thread_controller_with_message_pump_impl.h"

#include "base/test/mock_callback.h"
#include "base/test/simple_test_tick_clock.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#include <queue>

namespace base {
namespace sequence_manager {

namespace {

class ThreadControllerForTest
    : public internal::ThreadControllerWithMessagePumpImpl {
 public:
  ThreadControllerForTest(std::unique_ptr<MessagePump> pump,
                          const TickClock* clock)
      : ThreadControllerWithMessagePumpImpl(std::move(pump), clock) {}

  using ThreadControllerWithMessagePumpImpl::DoWork;
  using ThreadControllerWithMessagePumpImpl::DoDelayedWork;
  using ThreadControllerWithMessagePumpImpl::DoIdleWork;
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

class FakeSequencedTaskSource : public internal::SequencedTaskSource {
 public:
  explicit FakeSequencedTaskSource(TickClock* clock) : clock_(clock) {}
  ~FakeSequencedTaskSource() override = default;

  Optional<PendingTask> TakeTask() override {
    if (tasks_.empty())
      return nullopt;
    if (tasks_.front().delayed_run_time > clock_->NowTicks())
      return nullopt;
    PendingTask task = std::move(tasks_.front());
    tasks_.pop();
    return task;
  }

  void DidRunTask() override {}

  TimeDelta DelayTillNextTask(LazyNow* lazy_now) override {
    if (tasks_.empty())
      return TimeDelta::Max();
    if (tasks_.front().delayed_run_time.is_null())
      return TimeDelta();
    if (lazy_now->Now() > tasks_.front().delayed_run_time)
      return TimeDelta();
    return tasks_.front().delayed_run_time - lazy_now->Now();
  }

  void AddTask(PendingTask task) {
    DCHECK(tasks_.empty() || task.delayed_run_time.is_null() ||
           tasks_.back().delayed_run_time < task.delayed_run_time);
    tasks_.push(std::move(task));
  }

  bool HasPendingHighResolutionTasks() override { return false; }

 private:
  TickClock* clock_;
  std::queue<PendingTask> tasks_;
};

TimeTicks Seconds(int seconds) {
  return TimeTicks() + TimeDelta::FromSeconds(seconds);
}

}  // namespace

TEST(ThreadControllerWithMessagePumpTest, ScheduleDelayedWork) {
  MockMessagePump* message_pump;
  std::unique_ptr<MockMessagePump> pump =
      std::make_unique<testing::StrictMock<MockMessagePump>>();
  message_pump = pump.get();

  SimpleTestTickClock clock;
  ThreadControllerForTest thread_controller(std::move(pump), &clock);
  thread_controller.SetWorkBatchSize(1);

  FakeSequencedTaskSource task_source(&clock);
  thread_controller.SetSequencedTaskSource(&task_source);

  base::TimeTicks next_run_time;

  MockCallback<OnceClosure> task1;
  task_source.AddTask(PendingTask(FROM_HERE, task1.Get(), Seconds(10)));
  MockCallback<OnceClosure> task2;
  task_source.AddTask(PendingTask(FROM_HERE, task2.Get(), TimeTicks()));
  MockCallback<OnceClosure> task3;
  task_source.AddTask(PendingTask(FROM_HERE, task3.Get(), Seconds(20)));

  // Call a no-op DoWork. Expect that it doesn't do any work, but
  // schedules a delayed wake-up appropriately.
  clock.SetNowTicks(Seconds(5));
  EXPECT_CALL(*message_pump, ScheduleDelayedWork(Seconds(10)));
  EXPECT_FALSE(thread_controller.DoWork());
  testing::Mock::VerifyAndClearExpectations(message_pump);

  // Call DoDelayedWork after the expiration of the delay.
  // Expect that a task will run and the next delay will equal to |now|
  // as we have immediate work to do.
  clock.SetNowTicks(Seconds(11));
  EXPECT_CALL(task1, Run()).Times(1);
  EXPECT_TRUE(thread_controller.DoDelayedWork(&next_run_time));
  EXPECT_EQ(next_run_time, Seconds(11));
  testing::Mock::VerifyAndClearExpectations(message_pump);
  testing::Mock::VerifyAndClearExpectations(&task1);

  // Call DoWork immeidately after the previous call. Expect a new task
  // to be run.
  EXPECT_CALL(task2, Run()).Times(1);
  EXPECT_CALL(*message_pump, ScheduleDelayedWork(Seconds(20)));
  EXPECT_TRUE(thread_controller.DoWork());
  testing::Mock::VerifyAndClearExpectations(message_pump);
  testing::Mock::VerifyAndClearExpectations(&task2);

  // Call DoDelayedWork for the last task and expect to be told
  // about the lack of further delayed work
  // (delay being base::TimeDelta::Max()).
  clock.SetNowTicks(Seconds(21));
  EXPECT_CALL(task3, Run()).Times(1);
  EXPECT_TRUE(thread_controller.DoDelayedWork(&next_run_time));
  EXPECT_EQ(next_run_time, TimeTicks::Max());
  testing::Mock::VerifyAndClearExpectations(message_pump);
  testing::Mock::VerifyAndClearExpectations(&task3);
}

}  // namespace sequence_manager
}  // namespace base
