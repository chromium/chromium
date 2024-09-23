// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/sequence_manager/wake_up_queue.h"

#include <memory>
#include <optional>

#include "base/memory/ptr_util.h"
#include "base/message_loop/message_pump.h"
#include "base/message_loop/message_pump_type.h"
#include "base/task/sequence_manager/enqueue_order.h"
#include "base/task/sequence_manager/sequence_manager_impl.h"
#include "base/task/sequence_manager/task_queue_impl.h"
#include "base/test/mock_callback.h"
#include "base/test/simple_test_tick_clock.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::_;
using testing::AnyNumber;
using testing::Mock;

namespace base {
namespace sequence_manager {
namespace internal {

class TaskQueueImplForTest : public internal::TaskQueueImpl {
 public:
  TaskQueueImplForTest(internal::SequenceManagerImpl* sequence_manager,
                       WakeUpQueue* wake_up_queue,
                       const TaskQueue::Spec& spec)
      : TaskQueueImpl(sequence_manager, wake_up_queue, spec) {}
  ~TaskQueueImplForTest() override = default;

  using TaskQueueImpl::SetNextWakeUp;
};

class MockWakeUpQueue : public WakeUpQueue {
 public:
  MockWakeUpQueue()
      : WakeUpQueue(internal::AssociatedThreadId::CreateBound()) {}

  MockWakeUpQueue(const MockWakeUpQueue&) = delete;
  MockWakeUpQueue& operator=(const MockWakeUpQueue&) = delete;
  ~MockWakeUpQueue() override = default;

  using WakeUpQueue::MoveReadyDelayedTasksToWorkQueues;
  using WakeUpQueue::SetNextWakeUpForQueue;
  using WakeUpQueue::UnregisterQueue;

  void OnNextWakeUpChanged(LazyNow* lazy_now,
                           std::optional<WakeUp> wake_up) override {
    TimeTicks time = wake_up ? wake_up->time : TimeTicks::Max();
    OnNextWakeUpChanged_TimeTicks(time);
  }
  const char* GetName() const override { return "Test"; }
  void UnregisterQueue(internal::TaskQueueImpl* queue) override {
    SetNextWakeUpForQueue(queue, nullptr, std::nullopt);
  }

  internal::TaskQueueImpl* NextScheduledTaskQueue() const {
    if (wake_up_queue_.empty())
      return nullptr;
    return wake_up_queue_.top().queue;
  }

  TimeTicks NextScheduledRunTime() const {
    if (wake_up_queue_.empty())
      return TimeTicks::Max();
    return wake_up_queue_.top().wake_up.time;
  }

  MOCK_METHOD1(OnNextWakeUpChanged_TimeTicks, void(TimeTicks time));
};

class WakeUpQueueTest : public testing::Test {
 public:
  void SetUp() final {
    // A null clock triggers some assertions.
    tick_clock_.Advance(Milliseconds(1));
    wake_up_queue_ = std::make_unique<MockWakeUpQueue>();
    task_queue_ = std::make_unique<TaskQueueImplForTest>(
        nullptr, wake_up_queue_.get(), TaskQueue::Spec(QueueName::TEST_TQ));
  }

  void TearDown() final {
    if (task_queue_)
      task_queue_->UnregisterTaskQueue();
  }

  std::unique_ptr<MockWakeUpQueue> wake_up_queue_;
  std::unique_ptr<TaskQueueImplForTest> task_queue_;
  SimpleTestTickClock tick_clock_;
};

TEST_F(WakeUpQueueTest, ScheduleWakeUpForQueue) {
  TimeTicks now = tick_clock_.NowTicks();
  TimeDelta delay = Milliseconds(10);
  TimeTicks delayed_runtime = now + delay;
  EXPECT_TRUE(wake_up_queue_->empty());
  EXPECT_CALL(*wake_up_queue_.get(),
              OnNextWakeUpChanged_TimeTicks(delayed_runtime));
  LazyNow lazy_now(now);
  task_queue_->SetNextWakeUp(&lazy_now, WakeUp{delayed_runtime});

  EXPECT_FALSE(wake_up_queue_->empty());
  EXPECT_EQ(delayed_runtime, wake_up_queue_->NextScheduledRunTime());

  EXPECT_EQ(task_queue_.get(), wake_up_queue_->NextScheduledTaskQueue());
  Mock::VerifyAndClearExpectations(wake_up_queue_.get());

  EXPECT_CALL(*wake_up_queue_.get(),
              OnNextWakeUpChanged_TimeTicks(TimeTicks::Max()))
      .Times(AnyNumber());
}

TEST_F(WakeUpQueueTest, ScheduleWakeUpForQueueSupersedesPreviousWakeUp) {
  TimeTicks now = tick_clock_.NowTicks();
  TimeDelta delay1 = Milliseconds(10);
  TimeDelta delay2 = Milliseconds(100);
  TimeTicks delayed_runtime1 = now + delay1;
  TimeTicks delayed_runtime2 = now + delay2;
  EXPECT_CALL(*wake_up_queue_.get(),
              OnNextWakeUpChanged_TimeTicks(delayed_runtime1));
  LazyNow lazy_now(now);
  task_queue_->SetNextWakeUp(&lazy_now, WakeUp{delayed_runtime1});

  EXPECT_EQ(delayed_runtime1, wake_up_queue_->NextScheduledRunTime());

  Mock::VerifyAndClearExpectations(wake_up_queue_.get());

  // Now schedule a later wake_up, which should replace the previously
  // requested one.
  EXPECT_CALL(*wake_up_queue_.get(),
              OnNextWakeUpChanged_TimeTicks(delayed_runtime2));
  task_queue_->SetNextWakeUp(&lazy_now, WakeUp{delayed_runtime2});

  EXPECT_EQ(delayed_runtime2, wake_up_queue_->NextScheduledRunTime());
  Mock::VerifyAndClearExpectations(wake_up_queue_.get());

  EXPECT_CALL(*wake_up_queue_.get(),
              OnNextWakeUpChanged_TimeTicks(TimeTicks::Max()))
      .Times(AnyNumber());
}

TEST_F(
    WakeUpQueueTest,
    ScheduleFlexibleNoSoonerWakeUpForQueueSupersedesPreviousWakeUpWithLeeway) {
  TimeTicks now = tick_clock_.NowTicks();
  TimeDelta delay1 = Milliseconds(10);
  TimeDelta delay2 = Milliseconds(15);
  TimeTicks delayed_runtime1 = now + delay1;
  TimeTicks delayed_runtime2 = now + delay2;
  EXPECT_CALL(*wake_up_queue_.get(),
              OnNextWakeUpChanged_TimeTicks(delayed_runtime1));
  LazyNow lazy_now(now);
  task_queue_->SetNextWakeUp(
      &lazy_now,
      WakeUp{delayed_runtime1, Milliseconds(10), WakeUpResolution::kLow,
             subtle::DelayPolicy::kFlexibleNoSooner});

  EXPECT_EQ((WakeUp{delayed_runtime1, Milliseconds(10), WakeUpResolution::kLow,
                    subtle::DelayPolicy::kFlexibleNoSooner}),
            wake_up_queue_->GetNextDelayedWakeUp());

  Mock::VerifyAndClearExpectations(wake_up_queue_.get());

  // Now schedule a later wake_up, which should replace the previously
  // requested one.
  EXPECT_CALL(*wake_up_queue_.get(),
              OnNextWakeUpChanged_TimeTicks(delayed_runtime2));
  task_queue_->SetNextWakeUp(
      &lazy_now, WakeUp{delayed_runtime2, TimeDelta(), WakeUpResolution::kLow,
                        subtle::DelayPolicy::kPrecise});

  EXPECT_EQ((WakeUp{delayed_runtime2, TimeDelta(), WakeUpResolution::kLow,
                    subtle::DelayPolicy::kPrecise}),
            wake_up_queue_->GetNextDelayedWakeUp());
  Mock::VerifyAndClearExpectations(wake_up_queue_.get());

  EXPECT_CALL(*wake_up_queue_.get(),
              OnNextWakeUpChanged_TimeTicks(TimeTicks::Max()))
      .Times(AnyNumber());
}

TEST_F(WakeUpQueueTest,
       ScheduleFlexiblePreferEarlyWakeUpForQueueSupersedesPreviousWakeUp) {
  TimeTicks now = tick_clock_.NowTicks();
  TimeDelta delay1 = Milliseconds(10);
  TimeDelta delay2 = Milliseconds(15);
  TimeTicks delayed_runtime1 = now + delay1;
  TimeTicks delayed_runtime2 = now + delay2;
  EXPECT_CALL(*wake_up_queue_.get(),
              OnNextWakeUpChanged_TimeTicks(delayed_runtime1));
  LazyNow lazy_now(now);
  task_queue_->SetNextWakeUp(
      &lazy_now, WakeUp{delayed_runtime1, TimeDelta(), WakeUpResolution::kLow,
                        subtle::DelayPolicy::kPrecise});

  EXPECT_EQ((WakeUp{delayed_runtime1, TimeDelta(), WakeUpResolution::kLow,
                    subtle::DelayPolicy::kPrecise}),
            wake_up_queue_->GetNextDelayedWakeUp());

  Mock::VerifyAndClearExpectations(wake_up_queue_.get());

  // Now schedule a later wake_up, which should replace the previously
  // requested one.
  EXPECT_CALL(*wake_up_queue_.get(),
              OnNextWakeUpChanged_TimeTicks(delayed_runtime2));
  task_queue_->SetNextWakeUp(
      &lazy_now,
      WakeUp{delayed_runtime2, Milliseconds(10), WakeUpResolution::kLow,
             subtle::DelayPolicy::kFlexiblePreferEarly});

  EXPECT_EQ((WakeUp{delayed_runtime2, Milliseconds(10), WakeUpResolution::kLow,
                    subtle::DelayPolicy::kFlexiblePreferEarly}),
            wake_up_queue_->GetNextDelayedWakeUp());
  Mock::VerifyAndClearExpectations(wake_up_queue_.get());

  EXPECT_CALL(*wake_up_queue_.get(),
              OnNextWakeUpChanged_TimeTicks(TimeTicks::Max()))
      .Times(AnyNumber());
}

TEST_F(WakeUpQueueTest, SetNextDelayedDoWork_OnlyCalledForEarlierTasks) {
  std::unique_ptr<TaskQueueImplForTest> task_queue2 =
      std::make_unique<TaskQueueImplForTest>(
          nullptr, wake_up_queue_.get(), TaskQueue::Spec(QueueName::TEST_TQ));

  std::unique_ptr<TaskQueueImplForTest> task_queue3 =
      std::make_unique<TaskQueueImplForTest>(
          nullptr, wake_up_queue_.get(), TaskQueue::Spec(QueueName::TEST_TQ));

  std::unique_ptr<TaskQueueImplForTest> task_queue4 =
      std::make_unique<TaskQueueImplForTest>(
          nullptr, wake_up_queue_.get(), TaskQueue::Spec(QueueName::TEST_TQ));

  TimeDelta delay1 = Milliseconds(10);
  TimeDelta delay2 = Milliseconds(20);
  TimeDelta delay3 = Milliseconds(30);
  TimeDelta delay4 = Milliseconds(1);

  // SetNextDelayedDoWork should always be called if there are no other
  // wake-ups.
  TimeTicks now = tick_clock_.NowTicks();
  LazyNow lazy_now(now);
  EXPECT_CALL(*wake_up_queue_.get(),
              OnNextWakeUpChanged_TimeTicks(now + delay1));
  task_queue_->SetNextWakeUp(&lazy_now, WakeUp{now + delay1});

  Mock::VerifyAndClearExpectations(wake_up_queue_.get());

  // SetNextDelayedDoWork should not be called when scheduling later tasks.
  EXPECT_CALL(*wake_up_queue_.get(), OnNextWakeUpChanged_TimeTicks(_)).Times(0);
  task_queue2->SetNextWakeUp(&lazy_now, WakeUp{now + delay2});
  task_queue3->SetNextWakeUp(&lazy_now, WakeUp{now + delay3});

  // SetNextDelayedDoWork should be called when scheduling earlier tasks.
  Mock::VerifyAndClearExpectations(wake_up_queue_.get());
  EXPECT_CALL(*wake_up_queue_.get(),
              OnNextWakeUpChanged_TimeTicks(now + delay4));
  task_queue4->SetNextWakeUp(&lazy_now, WakeUp{now + delay4});

  Mock::VerifyAndClearExpectations(wake_up_queue_.get());

  EXPECT_CALL(*wake_up_queue_.get(), OnNextWakeUpChanged_TimeTicks(_)).Times(2);
  task_queue2->UnregisterTaskQueue();
  task_queue3->UnregisterTaskQueue();
  task_queue4->UnregisterTaskQueue();
}

TEST_F(WakeUpQueueTest, UnregisterQueue) {
  std::unique_ptr<TaskQueueImplForTest> task_queue2 =
      std::make_unique<TaskQueueImplForTest>(
          nullptr, wake_up_queue_.get(), TaskQueue::Spec(QueueName::TEST_TQ));
  EXPECT_TRUE(wake_up_queue_->empty());

  TimeTicks now = tick_clock_.NowTicks();
  LazyNow lazy_now(now);
  TimeTicks wake_up1 = now + Milliseconds(10);
  EXPECT_CALL(*wake_up_queue_.get(), OnNextWakeUpChanged_TimeTicks(wake_up1))
      .Times(1);
  task_queue_->SetNextWakeUp(&lazy_now, WakeUp{wake_up1});
  TimeTicks wake_up2 = now + Milliseconds(100);
  task_queue2->SetNextWakeUp(&lazy_now, WakeUp{wake_up2});
  EXPECT_FALSE(wake_up_queue_->empty());

  EXPECT_EQ(task_queue_.get(), wake_up_queue_->NextScheduledTaskQueue());

  testing::Mock::VerifyAndClearExpectations(wake_up_queue_.get());

  EXPECT_CALL(*wake_up_queue_.get(), OnNextWakeUpChanged_TimeTicks(wake_up2))
      .Times(1);

  wake_up_queue_->UnregisterQueue(task_queue_.get());
  EXPECT_EQ(task_queue2.get(), wake_up_queue_->NextScheduledTaskQueue());

  task_queue_->UnregisterTaskQueue();
  task_queue_ = nullptr;

  EXPECT_FALSE(wake_up_queue_->empty());
  testing::Mock::VerifyAndClearExpectations(wake_up_queue_.get());

  EXPECT_CALL(*wake_up_queue_.get(),
              OnNextWakeUpChanged_TimeTicks(TimeTicks::Max()))
      .Times(1);

  wake_up_queue_->UnregisterQueue(task_queue2.get());
  EXPECT_FALSE(wake_up_queue_->NextScheduledTaskQueue());

  task_queue2->UnregisterTaskQueue();
  task_queue2 = nullptr;
  EXPECT_TRUE(wake_up_queue_->empty());
}

TEST_F(WakeUpQueueTest, MoveReadyDelayedTasksToWorkQueues) {
  TimeDelta delay = Milliseconds(50);
  TimeTicks now = tick_clock_.NowTicks();
  LazyNow lazy_now_1(now);
  TimeTicks delayed_runtime = now + delay;
  EXPECT_CALL(*wake_up_queue_.get(),
              OnNextWakeUpChanged_TimeTicks(delayed_runtime));
  task_queue_->SetNextWakeUp(&lazy_now_1, WakeUp{delayed_runtime});

  EXPECT_EQ(delayed_runtime, wake_up_queue_->NextScheduledRunTime());

  wake_up_queue_->MoveReadyDelayedTasksToWorkQueues(&lazy_now_1,
                                                    EnqueueOrder());
  EXPECT_EQ(delayed_runtime, wake_up_queue_->NextScheduledRunTime());

  EXPECT_CALL(*wake_up_queue_.get(),
              OnNextWakeUpChanged_TimeTicks(TimeTicks::Max()));
  tick_clock_.SetNowTicks(delayed_runtime);
  LazyNow lazy_now_2(&tick_clock_);
  wake_up_queue_->MoveReadyDelayedTasksToWorkQueues(&lazy_now_2,
                                                    EnqueueOrder());
  ASSERT_TRUE(wake_up_queue_->NextScheduledRunTime().is_max());
}

TEST_F(WakeUpQueueTest, MoveReadyDelayedTasksToWorkQueuesWithLeeway) {
  std::unique_ptr<TaskQueueImplForTest> task_queue2 =
      std::make_unique<TaskQueueImplForTest>(
          nullptr, wake_up_queue_.get(), TaskQueue::Spec(QueueName::TEST_TQ));

  TimeTicks now = tick_clock_.NowTicks();
  LazyNow lazy_now_1(now);

  EXPECT_CALL(*wake_up_queue_.get(),
              OnNextWakeUpChanged_TimeTicks(now + Milliseconds(10)));
  task_queue2->SetNextWakeUp(&lazy_now_1,
                             WakeUp{now + Milliseconds(10), Milliseconds(4)});
  EXPECT_CALL(*wake_up_queue_.get(),
              OnNextWakeUpChanged_TimeTicks(now + Milliseconds(11)));
  task_queue_->SetNextWakeUp(&lazy_now_1,
                             WakeUp{now + Milliseconds(11), TimeDelta()});

  EXPECT_EQ(now + Milliseconds(11), wake_up_queue_->NextScheduledRunTime());

  wake_up_queue_->MoveReadyDelayedTasksToWorkQueues(&lazy_now_1,
                                                    EnqueueOrder());
  EXPECT_EQ(now + Milliseconds(11), wake_up_queue_->NextScheduledRunTime());

  tick_clock_.SetNowTicks(now + Milliseconds(10));
  LazyNow lazy_now_2(&tick_clock_);
  wake_up_queue_->MoveReadyDelayedTasksToWorkQueues(&lazy_now_2,
                                                    EnqueueOrder());
  EXPECT_EQ(now + Milliseconds(11), wake_up_queue_->NextScheduledRunTime());

  Mock::VerifyAndClearExpectations(wake_up_queue_.get());
  EXPECT_CALL(*wake_up_queue_.get(), OnNextWakeUpChanged_TimeTicks(_))
      .Times(AnyNumber());
  // Tidy up.
  task_queue2->UnregisterTaskQueue();
}

TEST_F(WakeUpQueueTest, CancelDelayedWork) {
  TimeTicks now = tick_clock_.NowTicks();
  LazyNow lazy_now(now);
  TimeTicks run_time = now + Milliseconds(20);

  EXPECT_CALL(*wake_up_queue_.get(), OnNextWakeUpChanged_TimeTicks(run_time));
  task_queue_->SetNextWakeUp(&lazy_now, WakeUp{run_time});

  EXPECT_EQ(task_queue_.get(), wake_up_queue_->NextScheduledTaskQueue());

  EXPECT_CALL(*wake_up_queue_.get(),
              OnNextWakeUpChanged_TimeTicks(TimeTicks::Max()));
  task_queue_->SetNextWakeUp(&lazy_now, std::nullopt);
  EXPECT_FALSE(wake_up_queue_->NextScheduledTaskQueue());
}

TEST_F(WakeUpQueueTest, CancelDelayedWork_TwoQueues) {
  std::unique_ptr<TaskQueueImplForTest> task_queue2 =
      std::make_unique<TaskQueueImplForTest>(
          nullptr, wake_up_queue_.get(), TaskQueue::Spec(QueueName::TEST_TQ));

  TimeTicks now = tick_clock_.NowTicks();
  LazyNow lazy_now(now);
  TimeTicks run_time1 = now + Milliseconds(20);
  TimeTicks run_time2 = now + Milliseconds(40);
  EXPECT_CALL(*wake_up_queue_.get(), OnNextWakeUpChanged_TimeTicks(run_time1));
  task_queue_->SetNextWakeUp(&lazy_now, WakeUp{run_time1});
  Mock::VerifyAndClearExpectations(wake_up_queue_.get());

  EXPECT_CALL(*wake_up_queue_.get(), OnNextWakeUpChanged_TimeTicks(_)).Times(0);
  task_queue2->SetNextWakeUp(&lazy_now, WakeUp{run_time2});
  Mock::VerifyAndClearExpectations(wake_up_queue_.get());

  EXPECT_EQ(task_queue_.get(), wake_up_queue_->NextScheduledTaskQueue());

  EXPECT_EQ(run_time1, wake_up_queue_->NextScheduledRunTime());

  EXPECT_CALL(*wake_up_queue_.get(), OnNextWakeUpChanged_TimeTicks(run_time2));
  task_queue_->SetNextWakeUp(&lazy_now, std::nullopt);
  EXPECT_EQ(task_queue2.get(), wake_up_queue_->NextScheduledTaskQueue());

  EXPECT_EQ(run_time2, wake_up_queue_->NextScheduledRunTime());

  Mock::VerifyAndClearExpectations(wake_up_queue_.get());
  EXPECT_CALL(*wake_up_queue_.get(), OnNextWakeUpChanged_TimeTicks(_))
      .Times(AnyNumber());

  // Tidy up.
  task_queue2->UnregisterTaskQueue();
}

TEST_F(WakeUpQueueTest, HighResolutionWakeUps) {
  TimeTicks now = tick_clock_.NowTicks();
  LazyNow lazy_now(now);
  TimeTicks run_time1 = now + Milliseconds(20);
  TimeTicks run_time2 = now + Milliseconds(40);
  TaskQueueImplForTest q1(nullptr, wake_up_queue_.get(),
                          TaskQueue::Spec(QueueName::TEST_TQ));
  TaskQueueImplForTest q2(nullptr, wake_up_queue_.get(),
                          TaskQueue::Spec(QueueName::TEST_TQ));

  // Add two high resolution wake-ups.
  EXPECT_FALSE(wake_up_queue_->has_pending_high_resolution_tasks());
  wake_up_queue_->SetNextWakeUpForQueue(
      &q1, &lazy_now, WakeUp{run_time1, TimeDelta(), WakeUpResolution::kHigh});
  EXPECT_TRUE(wake_up_queue_->has_pending_high_resolution_tasks());
  wake_up_queue_->SetNextWakeUpForQueue(
      &q2, &lazy_now, WakeUp{run_time2, TimeDelta(), WakeUpResolution::kHigh});
  EXPECT_TRUE(wake_up_queue_->has_pending_high_resolution_tasks());

  // Remove one of the wake-ups.
  wake_up_queue_->SetNextWakeUpForQueue(&q1, &lazy_now, std::nullopt);
  EXPECT_TRUE(wake_up_queue_->has_pending_high_resolution_tasks());

  // Remove the second one too.
  wake_up_queue_->SetNextWakeUpForQueue(&q2, &lazy_now, std::nullopt);
  EXPECT_FALSE(wake_up_queue_->has_pending_high_resolution_tasks());

  // Change a low resolution wake-up to a high resolution one.
  wake_up_queue_->SetNextWakeUpForQueue(
      &q1, &lazy_now, WakeUp{run_time1, TimeDelta(), WakeUpResolution::kLow});
  EXPECT_FALSE(wake_up_queue_->has_pending_high_resolution_tasks());
  wake_up_queue_->SetNextWakeUpForQueue(
      &q1, &lazy_now, WakeUp{run_time1, TimeDelta(), WakeUpResolution::kHigh});
  EXPECT_TRUE(wake_up_queue_->has_pending_high_resolution_tasks());

  // Move a high resolution wake-up in time.
  wake_up_queue_->SetNextWakeUpForQueue(
      &q1, &lazy_now, WakeUp{run_time2, TimeDelta(), WakeUpResolution::kHigh});
  EXPECT_TRUE(wake_up_queue_->has_pending_high_resolution_tasks());

  // Cancel the wake-up twice.
  wake_up_queue_->SetNextWakeUpForQueue(&q1, &lazy_now, std::nullopt);
  wake_up_queue_->SetNextWakeUpForQueue(&q1, &lazy_now, std::nullopt);
  EXPECT_FALSE(wake_up_queue_->has_pending_high_resolution_tasks());

  // Tidy up.
  q1.UnregisterTaskQueue();
  q2.UnregisterTaskQueue();
}

TEST_F(WakeUpQueueTest, SetNextWakeUpForQueueInThePast) {
  constexpr auto kType = MessagePumpType::DEFAULT;
  constexpr auto kDelay = Milliseconds(20);
  constexpr TaskQueue::QueuePriority kHighestPriority = 0;
  constexpr TaskQueue::QueuePriority kDefaultPriority = 1;
  constexpr TaskQueue::QueuePriority kLowestPriority = 2;
  constexpr TaskQueue::QueuePriority kPriorityCount = 3;
  auto sequence_manager = sequence_manager::CreateUnboundSequenceManager(
      SequenceManager::Settings::Builder()
          .SetMessagePumpType(kType)
          .SetTickClock(&tick_clock_)
          .SetPrioritySettings(SequenceManager::PrioritySettings(
              kPriorityCount, kDefaultPriority))
          .Build());
  sequence_manager->BindToMessagePump(MessagePump::Create(kType));
  auto high_prio_queue =
      sequence_manager->CreateTaskQueue(TaskQueue::Spec(QueueName::TEST_TQ));
  high_prio_queue->SetQueuePriority(kHighestPriority);
  auto high_prio_runner = high_prio_queue->CreateTaskRunner(kTaskTypeNone);
  auto low_prio_queue =
      sequence_manager->CreateTaskQueue(TaskQueue::Spec(QueueName::TEST2_TQ));
  low_prio_queue->SetQueuePriority(kLowestPriority);
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
                          base::Unretained(&tick_clock_), kDelay * 2));
  RunLoop().RunUntilIdle();
}

}  // namespace internal
}  // namespace sequence_manager
}  // namespace base
