// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/sequence_manager/time_domain.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/message_loop/message_pump.h"
#include "base/message_loop/message_pump_type.h"
#include "base/task/sequence_manager/sequence_manager_impl.h"
#include "base/task/sequence_manager/task_queue_impl.h"
#include "base/task/sequence_manager/work_queue.h"
#include "base/test/mock_callback.h"
#include "base/test/simple_test_tick_clock.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using testing::_;
using testing::AnyNumber;
using testing::Mock;

namespace base {
namespace sequence_manager {

class TaskQueueImplForTest : public internal::TaskQueueImpl {
 public:
  TaskQueueImplForTest(internal::SequenceManagerImpl* sequence_manager,
                       TimeDomain* time_domain,
                       const TaskQueue::Spec& spec)
      : TaskQueueImpl(sequence_manager, time_domain, spec) {}
  ~TaskQueueImplForTest() {}

  using TaskQueueImpl::SetNextDelayedWakeUp;
};

class TestTimeDomain : public TimeDomain {
 public:
  TestTimeDomain() : now_(TimeTicks() + TimeDelta::FromSeconds(1)) {}

  TestTimeDomain(const TestTimeDomain&) = delete;
  TestTimeDomain& operator=(const TestTimeDomain&) = delete;
  ~TestTimeDomain() override = default;

  using TimeDomain::MoveReadyDelayedTasksToWorkQueues;
  using TimeDomain::NextScheduledRunTime;
  using TimeDomain::SetNextWakeUpForQueue;
  using TimeDomain::UnregisterQueue;

  LazyNow CreateLazyNow() const override { return LazyNow(now_); }
  TimeTicks Now() const override { return now_; }

  absl::optional<TimeDelta> DelayTillNextTask(LazyNow* lazy_now) override {
    return absl::optional<TimeDelta>();
  }

  bool MaybeFastForwardToNextTask(bool quit_when_idle_requested) override {
    return false;
  }

  const char* GetName() const override { return "Test"; }

  internal::TaskQueueImpl* NextScheduledTaskQueue() const {
    if (delayed_wake_up_queue_.empty())
      return nullptr;
    return delayed_wake_up_queue_.Min().queue;
  }

  MOCK_METHOD2(SetNextDelayedDoWork,
               void(LazyNow* lazy_now, TimeTicks run_time));

  void SetNow(TimeTicks now) { now_ = now; }

 private:
  TimeTicks now_;
};

class TimeDomainTest : public testing::Test {
 public:
  void SetUp() final {
    time_domain_ = WrapUnique(CreateTestTimeDomain());
    task_queue_ = std::make_unique<TaskQueueImplForTest>(
        nullptr, time_domain_.get(), TaskQueue::Spec("test"));
  }

  void TearDown() final {
    if (task_queue_)
      task_queue_->UnregisterTaskQueue();
  }

  virtual TestTimeDomain* CreateTestTimeDomain() {
    return new TestTimeDomain();
  }

  std::unique_ptr<TestTimeDomain> time_domain_;
  std::unique_ptr<TaskQueueImplForTest> task_queue_;
};

TEST_F(TimeDomainTest, ScheduleWakeUpForQueue) {
  TimeDelta delay = TimeDelta::FromMilliseconds(10);
  TimeTicks delayed_runtime = time_domain_->Now() + delay;
  EXPECT_TRUE(time_domain_->empty());
  EXPECT_CALL(*time_domain_.get(), SetNextDelayedDoWork(_, delayed_runtime));
  TimeTicks now = time_domain_->Now();
  LazyNow lazy_now(now);
  task_queue_->SetNextDelayedWakeUp(&lazy_now, DelayedWakeUp{now + delay});

  EXPECT_FALSE(time_domain_->empty());
  EXPECT_EQ(delayed_runtime, time_domain_->NextScheduledRunTime());

  EXPECT_EQ(task_queue_.get(), time_domain_->NextScheduledTaskQueue());
  Mock::VerifyAndClearExpectations(time_domain_.get());

  EXPECT_CALL(*time_domain_.get(), SetNextDelayedDoWork(_, TimeTicks::Max()))
      .Times(AnyNumber());
}

TEST_F(TimeDomainTest, ScheduleWakeUpForQueueSupersedesPreviousWakeUp) {
  TimeDelta delay1 = TimeDelta::FromMilliseconds(10);
  TimeDelta delay2 = TimeDelta::FromMilliseconds(100);
  TimeTicks delayed_runtime1 = time_domain_->Now() + delay1;
  TimeTicks delayed_runtime2 = time_domain_->Now() + delay2;
  EXPECT_CALL(*time_domain_.get(), SetNextDelayedDoWork(_, delayed_runtime1));
  TimeTicks now = time_domain_->Now();
  LazyNow lazy_now(now);
  task_queue_->SetNextDelayedWakeUp(&lazy_now, DelayedWakeUp{delayed_runtime1});

  EXPECT_EQ(delayed_runtime1, time_domain_->NextScheduledRunTime());

  Mock::VerifyAndClearExpectations(time_domain_.get());

  // Now schedule a later wake_up, which should replace the previously
  // requested one.
  EXPECT_CALL(*time_domain_.get(), SetNextDelayedDoWork(_, delayed_runtime2));
  task_queue_->SetNextDelayedWakeUp(&lazy_now, DelayedWakeUp{delayed_runtime2});

  EXPECT_EQ(delayed_runtime2, time_domain_->NextScheduledRunTime());
  Mock::VerifyAndClearExpectations(time_domain_.get());

  EXPECT_CALL(*time_domain_.get(), SetNextDelayedDoWork(_, TimeTicks::Max()))
      .Times(AnyNumber());
}

TEST_F(TimeDomainTest, SetNextDelayedDoWork_OnlyCalledForEarlierTasks) {
  std::unique_ptr<TaskQueueImplForTest> task_queue2 =
      std::make_unique<TaskQueueImplForTest>(nullptr, time_domain_.get(),
                                             TaskQueue::Spec("test"));

  std::unique_ptr<TaskQueueImplForTest> task_queue3 =
      std::make_unique<TaskQueueImplForTest>(nullptr, time_domain_.get(),
                                             TaskQueue::Spec("test"));

  std::unique_ptr<TaskQueueImplForTest> task_queue4 =
      std::make_unique<TaskQueueImplForTest>(nullptr, time_domain_.get(),
                                             TaskQueue::Spec("test"));

  TimeDelta delay1 = TimeDelta::FromMilliseconds(10);
  TimeDelta delay2 = TimeDelta::FromMilliseconds(20);
  TimeDelta delay3 = TimeDelta::FromMilliseconds(30);
  TimeDelta delay4 = TimeDelta::FromMilliseconds(1);

  // SetNextDelayedDoWork should always be called if there are no other
  // wake-ups.
  TimeTicks now = time_domain_->Now();
  LazyNow lazy_now(now);
  EXPECT_CALL(*time_domain_.get(), SetNextDelayedDoWork(_, now + delay1));
  task_queue_->SetNextDelayedWakeUp(&lazy_now, DelayedWakeUp{now + delay1});

  Mock::VerifyAndClearExpectations(time_domain_.get());

  // SetNextDelayedDoWork should not be called when scheduling later tasks.
  EXPECT_CALL(*time_domain_.get(), SetNextDelayedDoWork(_, _)).Times(0);
  task_queue2->SetNextDelayedWakeUp(&lazy_now, DelayedWakeUp{now + delay2});
  task_queue3->SetNextDelayedWakeUp(&lazy_now, DelayedWakeUp{now + delay3});

  // SetNextDelayedDoWork should be called when scheduling earlier tasks.
  Mock::VerifyAndClearExpectations(time_domain_.get());
  EXPECT_CALL(*time_domain_.get(), SetNextDelayedDoWork(_, now + delay4));
  task_queue4->SetNextDelayedWakeUp(&lazy_now, DelayedWakeUp{now + delay4});

  Mock::VerifyAndClearExpectations(time_domain_.get());

  EXPECT_CALL(*time_domain_.get(), SetNextDelayedDoWork(_, _)).Times(2);
  task_queue2->UnregisterTaskQueue();
  task_queue3->UnregisterTaskQueue();
  task_queue4->UnregisterTaskQueue();
}

TEST_F(TimeDomainTest, UnregisterQueue) {
  std::unique_ptr<TaskQueueImplForTest> task_queue2 =
      std::make_unique<TaskQueueImplForTest>(nullptr, time_domain_.get(),
                                             TaskQueue::Spec("test"));
  EXPECT_TRUE(time_domain_->empty());

  TimeTicks now = time_domain_->Now();
  LazyNow lazy_now(now);
  TimeTicks wake_up1 = now + TimeDelta::FromMilliseconds(10);
  EXPECT_CALL(*time_domain_.get(), SetNextDelayedDoWork(_, wake_up1)).Times(1);
  task_queue_->SetNextDelayedWakeUp(&lazy_now, DelayedWakeUp{wake_up1});
  TimeTicks wake_up2 = now + TimeDelta::FromMilliseconds(100);
  task_queue2->SetNextDelayedWakeUp(&lazy_now, DelayedWakeUp{wake_up2});
  EXPECT_FALSE(time_domain_->empty());

  EXPECT_EQ(task_queue_.get(), time_domain_->NextScheduledTaskQueue());

  testing::Mock::VerifyAndClearExpectations(time_domain_.get());

  EXPECT_CALL(*time_domain_.get(), SetNextDelayedDoWork(_, wake_up2)).Times(1);

  time_domain_->UnregisterQueue(task_queue_.get());
  EXPECT_EQ(task_queue2.get(), time_domain_->NextScheduledTaskQueue());

  task_queue_->UnregisterTaskQueue();
  task_queue_ = nullptr;

  EXPECT_FALSE(time_domain_->empty());
  testing::Mock::VerifyAndClearExpectations(time_domain_.get());

  EXPECT_CALL(*time_domain_.get(), SetNextDelayedDoWork(_, TimeTicks::Max()))
      .Times(1);

  time_domain_->UnregisterQueue(task_queue2.get());
  EXPECT_FALSE(time_domain_->NextScheduledTaskQueue());

  task_queue2->UnregisterTaskQueue();
  task_queue2 = nullptr;
  EXPECT_TRUE(time_domain_->empty());
}

TEST_F(TimeDomainTest, MoveReadyDelayedTasksToWorkQueues) {
  TimeDelta delay = TimeDelta::FromMilliseconds(50);
  TimeTicks now = time_domain_->Now();
  LazyNow lazy_now_1(now);
  TimeTicks delayed_runtime = now + delay;
  EXPECT_CALL(*time_domain_.get(), SetNextDelayedDoWork(_, delayed_runtime));
  task_queue_->SetNextDelayedWakeUp(&lazy_now_1,
                                    DelayedWakeUp{delayed_runtime});

  EXPECT_EQ(delayed_runtime, time_domain_->NextScheduledRunTime());

  time_domain_->MoveReadyDelayedTasksToWorkQueues(&lazy_now_1);
  EXPECT_EQ(delayed_runtime, time_domain_->NextScheduledRunTime());

  EXPECT_CALL(*time_domain_.get(), SetNextDelayedDoWork(_, TimeTicks::Max()));
  time_domain_->SetNow(delayed_runtime);
  LazyNow lazy_now_2(time_domain_->CreateLazyNow());
  time_domain_->MoveReadyDelayedTasksToWorkQueues(&lazy_now_2);
  ASSERT_FALSE(time_domain_->NextScheduledRunTime());
}

TEST_F(TimeDomainTest, CancelDelayedWork) {
  TimeTicks now = time_domain_->Now();
  LazyNow lazy_now(now);
  TimeTicks run_time = now + TimeDelta::FromMilliseconds(20);

  EXPECT_CALL(*time_domain_.get(), SetNextDelayedDoWork(_, run_time));
  task_queue_->SetNextDelayedWakeUp(&lazy_now, DelayedWakeUp{run_time});

  EXPECT_EQ(task_queue_.get(), time_domain_->NextScheduledTaskQueue());

  EXPECT_CALL(*time_domain_.get(), SetNextDelayedDoWork(_, TimeTicks::Max()));
  task_queue_->SetNextDelayedWakeUp(&lazy_now, absl::nullopt);
  EXPECT_FALSE(time_domain_->NextScheduledTaskQueue());
}

TEST_F(TimeDomainTest, CancelDelayedWork_TwoQueues) {
  std::unique_ptr<TaskQueueImplForTest> task_queue2 =
      std::make_unique<TaskQueueImplForTest>(nullptr, time_domain_.get(),
                                             TaskQueue::Spec("test"));

  TimeTicks now = time_domain_->Now();
  LazyNow lazy_now(now);
  TimeTicks run_time1 = now + TimeDelta::FromMilliseconds(20);
  TimeTicks run_time2 = now + TimeDelta::FromMilliseconds(40);
  EXPECT_CALL(*time_domain_.get(), SetNextDelayedDoWork(_, run_time1));
  task_queue_->SetNextDelayedWakeUp(&lazy_now, DelayedWakeUp{run_time1});
  Mock::VerifyAndClearExpectations(time_domain_.get());

  EXPECT_CALL(*time_domain_.get(), SetNextDelayedDoWork(_, _)).Times(0);
  task_queue2->SetNextDelayedWakeUp(&lazy_now, DelayedWakeUp{run_time2});
  Mock::VerifyAndClearExpectations(time_domain_.get());

  EXPECT_EQ(task_queue_.get(), time_domain_->NextScheduledTaskQueue());

  EXPECT_EQ(run_time1, time_domain_->NextScheduledRunTime());

  EXPECT_CALL(*time_domain_.get(), SetNextDelayedDoWork(_, run_time2));
  task_queue_->SetNextDelayedWakeUp(&lazy_now, absl::nullopt);
  EXPECT_EQ(task_queue2.get(), time_domain_->NextScheduledTaskQueue());

  EXPECT_EQ(run_time2, time_domain_->NextScheduledRunTime());

  Mock::VerifyAndClearExpectations(time_domain_.get());
  EXPECT_CALL(*time_domain_.get(), SetNextDelayedDoWork(_, _))
      .Times(AnyNumber());

  // Tidy up.
  task_queue2->UnregisterTaskQueue();
}

TEST_F(TimeDomainTest, HighResolutionWakeUps) {
  TimeTicks now = time_domain_->Now();
  LazyNow lazy_now(now);
  TimeTicks run_time1 = now + TimeDelta::FromMilliseconds(20);
  TimeTicks run_time2 = now + TimeDelta::FromMilliseconds(40);
  TaskQueueImplForTest q1(nullptr, time_domain_.get(), TaskQueue::Spec("test"));
  TaskQueueImplForTest q2(nullptr, time_domain_.get(), TaskQueue::Spec("test"));

  // Add two high resolution wake-ups.
  EXPECT_FALSE(time_domain_->has_pending_high_resolution_tasks());
  time_domain_->SetNextWakeUpForQueue(
      &q1, DelayedWakeUp{run_time1, WakeUpResolution::kHigh}, &lazy_now);
  EXPECT_TRUE(time_domain_->has_pending_high_resolution_tasks());
  time_domain_->SetNextWakeUpForQueue(
      &q2, DelayedWakeUp{run_time2, WakeUpResolution::kHigh}, &lazy_now);
  EXPECT_TRUE(time_domain_->has_pending_high_resolution_tasks());

  // Remove one of the wake-ups.
  time_domain_->SetNextWakeUpForQueue(&q1, absl::nullopt, &lazy_now);
  EXPECT_TRUE(time_domain_->has_pending_high_resolution_tasks());

  // Remove the second one too.
  time_domain_->SetNextWakeUpForQueue(&q2, absl::nullopt, &lazy_now);
  EXPECT_FALSE(time_domain_->has_pending_high_resolution_tasks());

  // Change a low resolution wake-up to a high resolution one.
  time_domain_->SetNextWakeUpForQueue(
      &q1, DelayedWakeUp{run_time1, WakeUpResolution::kLow}, &lazy_now);
  EXPECT_FALSE(time_domain_->has_pending_high_resolution_tasks());
  time_domain_->SetNextWakeUpForQueue(
      &q1, DelayedWakeUp{run_time1, WakeUpResolution::kHigh}, &lazy_now);
  EXPECT_TRUE(time_domain_->has_pending_high_resolution_tasks());

  // Move a high resolution wake-up in time.
  time_domain_->SetNextWakeUpForQueue(
      &q1, DelayedWakeUp{run_time2, WakeUpResolution::kHigh}, &lazy_now);
  EXPECT_TRUE(time_domain_->has_pending_high_resolution_tasks());

  // Cancel the wake-up twice.
  time_domain_->SetNextWakeUpForQueue(&q1, absl::nullopt, &lazy_now);
  time_domain_->SetNextWakeUpForQueue(&q1, absl::nullopt, &lazy_now);
  EXPECT_FALSE(time_domain_->has_pending_high_resolution_tasks());

  // Tidy up.
  q1.UnregisterTaskQueue();
  q2.UnregisterTaskQueue();
}

TEST_F(TimeDomainTest, SetNextWakeUpForQueueInThePast) {
  constexpr auto kType = MessagePumpType::DEFAULT;
  constexpr auto kDelay = TimeDelta::FromMilliseconds(20);
  SimpleTestTickClock clock;
  auto sequence_manager = sequence_manager::CreateUnboundSequenceManager(
      SequenceManager::Settings::Builder()
          .SetMessagePumpType(kType)
          .SetTickClock(&clock)
          .Build());
  sequence_manager->BindToMessagePump(MessagePump::Create(kType));
  auto high_prio_queue =
      sequence_manager->CreateTaskQueue(TaskQueue::Spec("high_prio_queue"));
  high_prio_queue->SetQueuePriority(TaskQueue::kHighestPriority);
  auto high_prio_runner = high_prio_queue->CreateTaskRunner(kTaskTypeNone);
  auto low_prio_queue =
      sequence_manager->CreateTaskQueue(TaskQueue::Spec("low_prio_queue"));
  low_prio_queue->SetQueuePriority(TaskQueue::kBestEffortPriority);
  auto low_prio_runner = low_prio_queue->CreateTaskRunner(kTaskTypeNone);
  sequence_manager->SetDefaultTaskRunner(high_prio_runner);
  base::MockCallback<base::OnceCallback<void()>> task_1, task_2;

  testing::Sequence s;
  // Expect task_2 to run after task_1
  EXPECT_CALL(task_1, Run);
  EXPECT_CALL(task_2, Run);
  // Schedule high and low priority tasks in such a way that clock.Now() will be
  // way into the future by the time the low prio task run time is used to setup
  // a wake up.
  low_prio_runner->PostDelayedTask(FROM_HERE, task_2.Get(), kDelay);
  high_prio_runner->PostDelayedTask(FROM_HERE, task_1.Get(), kDelay * 2);
  high_prio_runner->PostTask(
      FROM_HERE, BindOnce([](SimpleTestTickClock* clock,
                             TimeDelta delay) { clock->Advance(delay); },
                          base::Unretained(&clock), kDelay * 2));
  RunLoop().RunUntilIdle();
}

}  // namespace sequence_manager
}  // namespace base
