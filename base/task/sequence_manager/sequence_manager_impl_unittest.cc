// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/sequence_manager/sequence_manager_impl.h"

#include <stddef.h>
#include <memory>
#include <utility>

#include "base/location.h"
#include "base/memory/ref_counted_memory.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_current.h"
#include "base/message_loop/message_pump_default.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequence_manager/real_time_domain.h"
#include "base/task/sequence_manager/task_queue_impl.h"
#include "base/task/sequence_manager/task_queue_selector.h"
#include "base/task/sequence_manager/test/mock_time_domain.h"
#include "base/task/sequence_manager/test/sequence_manager_for_test.h"
#include "base/task/sequence_manager/test/test_task_queue.h"
#include "base/task/sequence_manager/test/test_task_time_observer.h"
#include "base/task/sequence_manager/thread_controller_with_message_pump_impl.h"
#include "base/task/sequence_manager/work_queue.h"
#include "base/task/sequence_manager/work_queue_sets.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/test/test_simple_task_runner.h"
#include "base/test/trace_event_analyzer.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/blame_context.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::AnyNumber;
using testing::Contains;
using testing::ElementsAre;
using testing::ElementsAreArray;
using testing::Mock;
using testing::Not;
using testing::_;
using base::sequence_manager::internal::EnqueueOrder;

namespace base {
namespace sequence_manager {
namespace internal {
// To avoid symbol collisions in jumbo builds.
namespace sequence_manager_impl_unittest {

enum class TestType : int {
  kCustom = 0,
  kUseMockTaskRunner = 1,
  kUseMessageLoop = 2,
  kUseMessagePump = 3,
};

class SequenceManagerTestBase : public testing::TestWithParam<TestType> {
 protected:
  void TearDown() override {
    // SequenceManager should be deleted before an underlying task runner.
    manager_.reset();
  }

  scoped_refptr<TestTaskQueue> CreateTaskQueue(
      TaskQueue::Spec spec = TaskQueue::Spec("test")) {
    return manager_->CreateTaskQueue<TestTaskQueue>(spec);
  }

  void CreateTaskQueues(size_t num_queues) {
    for (size_t i = 0; i < num_queues; i++)
      runners_.push_back(CreateTaskQueue());
  }

  std::unique_ptr<SequenceManagerForTest> manager_;
  std::vector<scoped_refptr<TestTaskQueue>> runners_;
  TimeTicks start_time_;
  TestTaskTimeObserver test_task_time_observer_;
};

// SequenceManagerImpl uses TestMockTimeTaskRunner which controls
// both task execution and mock clock.
// TODO(kraynov): Make this class to support all TestTypes.
// It will allow us to re-run tests in various environments before we'll
// eventually move to MessagePump and remove current ThreadControllerImpl.
class SequenceManagerTest : public SequenceManagerTestBase {
 public:
  void DeleteSequenceManagerTask() { manager_.reset(); }

 protected:
  void SetUp() override {
    ASSERT_EQ(GetParam(), TestType::kUseMockTaskRunner);
    test_task_runner_ = WrapRefCounted(new TestMockTimeTaskRunner(
        TestMockTimeTaskRunner::Type::kBoundToThread));
    // A null clock triggers some assertions.
    test_task_runner_->AdvanceMockTickClock(TimeDelta::FromMilliseconds(1));
    start_time_ = GetTickClock()->NowTicks();

    manager_ =
        SequenceManagerForTest::Create(nullptr, ThreadTaskRunnerHandle::Get(),
                                       test_task_runner_->GetMockTickClock());
  }

  const TickClock* GetTickClock() {
    return test_task_runner_->GetMockTickClock();
  }

  void RunPendingTasks() {
    // We should only run tasks already posted by that moment.
    RunLoop run_loop;
    test_task_runner_->PostTask(FROM_HERE, run_loop.QuitClosure());
    // TestMockTimeTaskRunner will fast-forward mock clock if necessary.
    run_loop.Run();
  }

  // Runs all immediate tasks until there is no more work to do and advances
  // time if there is a pending delayed task. |per_run_time_callback| is called
  // when the clock advances.
  // The only difference to FastForwardUntilNoTasksRemain is that time
  // advancing isn't driven by the test task runner, but uses time domain's
  // next scheduled run time instead. It allows us to double-check consistency
  // and allows to count such bursts of doing work, which is a test subject.
  void RunUntilManagerIsIdle(RepeatingClosure per_run_time_callback) {
    for (;;) {
      // Advance time if we've run out of immediate work to do.
      if (!manager_->HasImmediateWork()) {
        LazyNow lazy_now(GetTickClock());
        Optional<TimeDelta> delay =
            manager_->GetRealTimeDomain()->DelayTillNextTask(&lazy_now);
        if (delay) {
          test_task_runner_->AdvanceMockTickClock(*delay);
          per_run_time_callback.Run();
        } else {
          break;
        }
      }
      RunPendingTasks();
    }
  }

  scoped_refptr<TestMockTimeTaskRunner> test_task_runner_;
};

// SequenceManagerImpl is being initialized with real MessageLoop
// at cost of less control over a task runner.
// It also runs a version with experimental MessagePump support.
// TODO(kraynov): Generalize as many tests as possible to run it
// in all supported environments.
class SequenceManagerTestWithMessageLoop : public SequenceManagerTestBase {
 protected:
  void SetUp() override {
    switch (GetParam()) {
      case TestType::kUseMessageLoop:
        SetUpWithMessageLoop();
        break;
      case TestType::kUseMessagePump:
        SetUpWithMessagePump();
        break;
      default:
        FAIL();
    }
  }

  void SetUpWithMessageLoop() {
    message_loop_.reset(new MessageLoop());
    // A null clock triggers some assertions.
    mock_clock_.Advance(TimeDelta::FromMilliseconds(1));
    start_time_ = mock_clock_.NowTicks();

    manager_ = SequenceManagerForTest::Create(
        message_loop_.get(), ThreadTaskRunnerHandle::Get(), &mock_clock_);
  }

  void SetUpWithMessagePump() {
    mock_clock_.Advance(TimeDelta::FromMilliseconds(1));
    start_time_ = mock_clock_.NowTicks();
    manager_ = SequenceManagerForTest::Create(
        std::make_unique<ThreadControllerWithMessagePumpImpl>(
            std::make_unique<MessagePumpDefault>(), &mock_clock_));
    // ThreadControllerWithMessagePumpImpl doesn't provide
    // a default task runner.
    scoped_refptr<TestTaskQueue> default_task_queue =
        manager_->CreateTaskQueue<TestTaskQueue>(TaskQueue::Spec("default"));
    manager_->SetDefaultTaskRunner(default_task_queue->task_runner());
  }

  const TickClock* GetTickClock() { return &mock_clock_; }

  std::unique_ptr<MessageLoop> message_loop_;
  SimpleTestTickClock mock_clock_;
};

class SequenceManagerTestWithCustomInitialization
    : public SequenceManagerTestWithMessageLoop {
 protected:
  void SetUp() override { ASSERT_EQ(GetParam(), TestType::kCustom); }
};

INSTANTIATE_TEST_CASE_P(,
                        SequenceManagerTest,
                        testing::Values(TestType::kUseMockTaskRunner));

INSTANTIATE_TEST_CASE_P(,
                        SequenceManagerTestWithMessageLoop,
                        testing::Values(TestType::kUseMessageLoop,
                                        TestType::kUseMessagePump));

INSTANTIATE_TEST_CASE_P(,
                        SequenceManagerTestWithCustomInitialization,
                        testing::Values(TestType::kCustom));

void PostFromNestedRunloop(scoped_refptr<TestTaskQueue> runner,
                           std::vector<std::pair<OnceClosure, bool>>* tasks) {
  for (std::pair<OnceClosure, bool>& pair : *tasks) {
    if (pair.second) {
      runner->PostTask(FROM_HERE, std::move(pair.first));
    } else {
      runner->PostNonNestableTask(FROM_HERE, std::move(pair.first));
    }
  }
  RunLoop(RunLoop::Type::kNestableTasksAllowed).RunUntilIdle();
}

void NopTask() {}

class TestCountUsesTimeSource : public TickClock {
 public:
  TestCountUsesTimeSource() = default;
  ~TestCountUsesTimeSource() override = default;

  TimeTicks NowTicks() const override {
    now_calls_count_++;
    // Don't return 0, as it triggers some assertions.
    return TimeTicks() + TimeDelta::FromSeconds(1);
  }

  int now_calls_count() const { return now_calls_count_; }

 private:
  mutable int now_calls_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestCountUsesTimeSource);
};

TEST_P(SequenceManagerTestWithCustomInitialization,
       NowCalledMinimumNumberOfTimesToComputeTaskDurations) {
  message_loop_.reset(new MessageLoop());
  // This memory is managed by the SequenceManager, but we need to hold a
  // pointer to this object to read out how many times Now was called.
  TestCountUsesTimeSource test_count_uses_time_source;

  manager_ = SequenceManagerForTest::Create(
      nullptr, ThreadTaskRunnerHandle::Get(), &test_count_uses_time_source);
  manager_->SetWorkBatchSize(6);
  manager_->AddTaskTimeObserver(&test_task_time_observer_);

  for (size_t i = 0; i < 3; i++)
    runners_.push_back(CreateTaskQueue());

  runners_[0]->PostTask(FROM_HERE, BindOnce(&NopTask));
  runners_[0]->PostTask(FROM_HERE, BindOnce(&NopTask));
  runners_[1]->PostTask(FROM_HERE, BindOnce(&NopTask));
  runners_[1]->PostTask(FROM_HERE, BindOnce(&NopTask));
  runners_[2]->PostTask(FROM_HERE, BindOnce(&NopTask));
  runners_[2]->PostTask(FROM_HERE, BindOnce(&NopTask));

  RunLoop().RunUntilIdle();
  // Now is called each time a task is queued, when first task is started
  // running, and when a task is completed. 6 * 3 = 18 calls.
  EXPECT_EQ(18, test_count_uses_time_source.now_calls_count());
}

void NullTask() {}

void TestTask(uint64_t value, std::vector<EnqueueOrder>* out_result) {
  out_result->push_back(EnqueueOrder::FromIntForTesting(value));
}

void DisableQueueTestTask(uint64_t value,
                          std::vector<EnqueueOrder>* out_result,
                          TaskQueue::QueueEnabledVoter* voter) {
  out_result->push_back(EnqueueOrder::FromIntForTesting(value));
  voter->SetQueueEnabled(false);
}

TEST_P(SequenceManagerTest, SingleQueuePosting) {
  CreateTaskQueues(1u);

  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostTask(FROM_HERE, BindOnce(&TestTask, 1, &run_order));
  runners_[0]->PostTask(FROM_HERE, BindOnce(&TestTask, 2, &run_order));
  runners_[0]->PostTask(FROM_HERE, BindOnce(&TestTask, 3, &run_order));

  RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1u, 2u, 3u));
}

TEST_P(SequenceManagerTest, MultiQueuePosting) {
  CreateTaskQueues(3u);

  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostTask(FROM_HERE, BindOnce(&TestTask, 1, &run_order));
  runners_[0]->PostTask(FROM_HERE, BindOnce(&TestTask, 2, &run_order));
  runners_[1]->PostTask(FROM_HERE, BindOnce(&TestTask, 3, &run_order));
  runners_[1]->PostTask(FROM_HERE, BindOnce(&TestTask, 4, &run_order));
  runners_[2]->PostTask(FROM_HERE, BindOnce(&TestTask, 5, &run_order));
  runners_[2]->PostTask(FROM_HERE, BindOnce(&TestTask, 6, &run_order));

  RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1u, 2u, 3u, 4u, 5u, 6u));
}

TEST_P(SequenceManagerTestWithMessageLoop, NonNestableTaskPosting) {
  CreateTaskQueues(1u);

  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostNonNestableTask(FROM_HERE,
                                   BindOnce(&TestTask, 1, &run_order));

  RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1u));
}

TEST_P(SequenceManagerTestWithMessageLoop,
       NonNestableTaskExecutesInExpectedOrder) {
  CreateTaskQueues(1u);

  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostTask(FROM_HERE, BindOnce(&TestTask, 1, &run_order));
  runners_[0]->PostTask(FROM_HERE, BindOnce(&TestTask, 2, &run_order));
  runners_[0]->PostTask(FROM_HERE, BindOnce(&TestTask, 3, &run_order));
  runners_[0]->PostTask(FROM_HERE, BindOnce(&TestTask, 4, &run_order));
  runners_[0]->PostNonNestableTask(FROM_HERE,
                                   BindOnce(&TestTask, 5, &run_order));

  RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1u, 2u, 3u, 4u, 5u));
}

TEST_P(SequenceManagerTestWithMessageLoop,
       NonNestableTasksDoesntExecuteInNestedLoop) {
  CreateTaskQueues(1u);

  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostTask(FROM_HERE, BindOnce(&TestTask, 1, &run_order));
  runners_[0]->PostTask(FROM_HERE, BindOnce(&TestTask, 2, &run_order));

  std::vector<std::pair<OnceClosure, bool>> tasks_to_post_from_nested_loop;
  tasks_to_post_from_nested_loop.push_back(
      std::make_pair(BindOnce(&TestTask, 3, &run_order), false));
  tasks_to_post_from_nested_loop.push_back(
      std::make_pair(BindOnce(&TestTask, 4, &run_order), false));
  tasks_to_post_from_nested_loop.push_back(
      std::make_pair(BindOnce(&TestTask, 5, &run_order), true));
  tasks_to_post_from_nested_loop.push_back(
      std::make_pair(BindOnce(&TestTask, 6, &run_order), true));

  runners_[0]->PostTask(FROM_HERE,
                        BindOnce(&PostFromNestedRunloop, runners_[0],
                                 Unretained(&tasks_to_post_from_nested_loop)));

  RunLoop().RunUntilIdle();
  // Note we expect tasks 3 & 4 to run last because they're non-nestable.
  EXPECT_THAT(run_order, ElementsAre(1u, 2u, 5u, 6u, 3u, 4u));
}

namespace {

void InsertFenceAndPostTestTask(int id,
                                std::vector<EnqueueOrder>* run_order,
                                scoped_refptr<TestTaskQueue> task_queue) {
  run_order->push_back(EnqueueOrder::FromIntForTesting(id));
  task_queue->InsertFence(TaskQueue::InsertFencePosition::kNow);
  task_queue->PostTask(FROM_HERE, BindOnce(&TestTask, id + 1, run_order));

  // Force reload of immediate work queue. In real life the same effect can be
  // achieved with cross-thread posting.
  task_queue->GetTaskQueueImpl()->ReloadImmediateWorkQueueIfEmpty();
}

}  // namespace

TEST_P(SequenceManagerTestWithMessageLoop, TaskQueueDisabledFromNestedLoop) {
  CreateTaskQueues(1u);
  std::vector<EnqueueOrder> run_order;

  std::vector<std::pair<OnceClosure, bool>> tasks_to_post_from_nested_loop;

  tasks_to_post_from_nested_loop.push_back(
      std::make_pair(BindOnce(&TestTask, 1, &run_order), false));
  tasks_to_post_from_nested_loop.push_back(std::make_pair(
      BindOnce(&InsertFenceAndPostTestTask, 2, &run_order, runners_[0]), true));

  runners_[0]->PostTask(FROM_HERE,
                        BindOnce(&PostFromNestedRunloop, runners_[0],
                                 Unretained(&tasks_to_post_from_nested_loop)));
  RunLoop().RunUntilIdle();

  // Task 1 shouldn't run first due to it being non-nestable and queue gets
  // blocked after task 2. Task 1 runs after existing nested message loop
  // due to being posted before inserting a fence.
  // This test checks that breaks when nestable task is pushed into a redo
  // queue.
  EXPECT_THAT(run_order, ElementsAre(2u, 1u));

  runners_[0]->RemoveFence();
  RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(2u, 1u, 3u));
}

TEST_P(SequenceManagerTest, HasPendingImmediateWork_ImmediateTask) {
  CreateTaskQueues(1u);

  std::vector<EnqueueOrder> run_order;
  EXPECT_FALSE(runners_[0]->HasTaskToRunImmediately());
  runners_[0]->PostTask(FROM_HERE, BindOnce(&TestTask, 1, &run_order));
  EXPECT_TRUE(runners_[0]->HasTaskToRunImmediately());

  // Move the task into the |immediate_work_queue|.
  EXPECT_TRUE(runners_[0]->GetTaskQueueImpl()->immediate_work_queue()->Empty());
  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter =
      runners_[0]->CreateQueueEnabledVoter();
  voter->SetQueueEnabled(false);
  RunLoop().RunUntilIdle();
  EXPECT_FALSE(
      runners_[0]->GetTaskQueueImpl()->immediate_work_queue()->Empty());
  EXPECT_TRUE(runners_[0]->HasTaskToRunImmediately());

  // Run the task, making the queue empty.
  voter->SetQueueEnabled(true);
  RunLoop().RunUntilIdle();
  EXPECT_FALSE(runners_[0]->HasTaskToRunImmediately());
}

TEST_P(SequenceManagerTest, HasPendingImmediateWork_DelayedTask) {
  CreateTaskQueues(1u);

  std::vector<EnqueueOrder> run_order;
  TimeDelta delay(TimeDelta::FromMilliseconds(10));
  runners_[0]->PostDelayedTask(FROM_HERE, BindOnce(&TestTask, 1, &run_order),
                               delay);
  EXPECT_FALSE(runners_[0]->HasTaskToRunImmediately());
  test_task_runner_->AdvanceMockTickClock(delay);
  EXPECT_TRUE(runners_[0]->HasTaskToRunImmediately());

  // Move the task into the |delayed_work_queue|.
  LazyNow lazy_now(GetTickClock());
  manager_->WakeUpReadyDelayedQueues(&lazy_now);
  EXPECT_FALSE(runners_[0]->GetTaskQueueImpl()->delayed_work_queue()->Empty());
  EXPECT_TRUE(runners_[0]->HasTaskToRunImmediately());

  // Run the task, making the queue empty.
  RunLoop().RunUntilIdle();
  EXPECT_FALSE(runners_[0]->HasTaskToRunImmediately());
}

TEST_P(SequenceManagerTest, DelayedTaskPosting) {
  CreateTaskQueues(1u);

  std::vector<EnqueueOrder> run_order;
  TimeDelta delay(TimeDelta::FromMilliseconds(10));
  runners_[0]->PostDelayedTask(FROM_HERE, BindOnce(&TestTask, 1, &run_order),
                               delay);
  EXPECT_EQ(TimeDelta::FromMilliseconds(10),
            test_task_runner_->NextPendingTaskDelay());
  EXPECT_FALSE(runners_[0]->HasTaskToRunImmediately());
  EXPECT_TRUE(run_order.empty());

  // The task doesn't run before the delay has completed.
  test_task_runner_->FastForwardBy(TimeDelta::FromMilliseconds(9));
  EXPECT_TRUE(run_order.empty());

  // After the delay has completed, the task runs normally.
  test_task_runner_->FastForwardBy(TimeDelta::FromMilliseconds(1));
  EXPECT_THAT(run_order, ElementsAre(1u));
  EXPECT_FALSE(runners_[0]->HasTaskToRunImmediately());
}

TEST_P(SequenceManagerTest, DelayedTaskExecutedInOneMessageLoopTask) {
  CreateTaskQueues(1u);

  runners_[0]->PostDelayedTask(FROM_HERE, BindOnce(&NopTask),
                               TimeDelta::FromMilliseconds(10));
  RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, test_task_runner_->GetPendingTaskCount());
  test_task_runner_->FastForwardUntilNoTasksRemain();
  EXPECT_EQ(0u, test_task_runner_->GetPendingTaskCount());
}

TEST_P(SequenceManagerTest, DelayedTaskPosting_MultipleTasks_DecendingOrder) {
  CreateTaskQueues(1u);

  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostDelayedTask(FROM_HERE, BindOnce(&TestTask, 1, &run_order),
                               TimeDelta::FromMilliseconds(10));

  runners_[0]->PostDelayedTask(FROM_HERE, BindOnce(&TestTask, 2, &run_order),
                               TimeDelta::FromMilliseconds(8));

  runners_[0]->PostDelayedTask(FROM_HERE, BindOnce(&TestTask, 3, &run_order),
                               TimeDelta::FromMilliseconds(5));

  EXPECT_EQ(TimeDelta::FromMilliseconds(5),
            test_task_runner_->NextPendingTaskDelay());

  test_task_runner_->FastForwardBy(TimeDelta::FromMilliseconds(5));
  EXPECT_THAT(run_order, ElementsAre(3u));
  EXPECT_EQ(TimeDelta::FromMilliseconds(3),
            test_task_runner_->NextPendingTaskDelay());

  test_task_runner_->FastForwardBy(TimeDelta::FromMilliseconds(3));
  EXPECT_THAT(run_order, ElementsAre(3u, 2u));
  EXPECT_EQ(TimeDelta::FromMilliseconds(2),
            test_task_runner_->NextPendingTaskDelay());

  test_task_runner_->FastForwardBy(TimeDelta::FromMilliseconds(2));
  EXPECT_THAT(run_order, ElementsAre(3u, 2u, 1u));
}

TEST_P(SequenceManagerTest, DelayedTaskPosting_MultipleTasks_AscendingOrder) {
  CreateTaskQueues(1u);

  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostDelayedTask(FROM_HERE, BindOnce(&TestTask, 1, &run_order),
                               TimeDelta::FromMilliseconds(1));

  runners_[0]->PostDelayedTask(FROM_HERE, BindOnce(&TestTask, 2, &run_order),
                               TimeDelta::FromMilliseconds(5));

  runners_[0]->PostDelayedTask(FROM_HERE, BindOnce(&TestTask, 3, &run_order),
                               TimeDelta::FromMilliseconds(10));

  EXPECT_EQ(TimeDelta::FromMilliseconds(1),
            test_task_runner_->NextPendingTaskDelay());

  test_task_runner_->FastForwardBy(TimeDelta::FromMilliseconds(1));
  EXPECT_THAT(run_order, ElementsAre(1u));
  EXPECT_EQ(TimeDelta::FromMilliseconds(4),
            test_task_runner_->NextPendingTaskDelay());

  test_task_runner_->FastForwardBy(TimeDelta::FromMilliseconds(4));
  EXPECT_THAT(run_order, ElementsAre(1u, 2u));
  EXPECT_EQ(TimeDelta::FromMilliseconds(5),
            test_task_runner_->NextPendingTaskDelay());

  test_task_runner_->FastForwardBy(TimeDelta::FromMilliseconds(5));
  EXPECT_THAT(run_order, ElementsAre(1u, 2u, 3u));
}

TEST_P(SequenceManagerTest, PostDelayedTask_SharesUnderlyingDelayedTasks) {
  CreateTaskQueues(1u);

  std::vector<EnqueueOrder> run_order;
  TimeDelta delay(TimeDelta::FromMilliseconds(10));
  runners_[0]->PostDelayedTask(FROM_HERE, BindOnce(&TestTask, 1, &run_order),
                               delay);
  runners_[0]->PostDelayedTask(FROM_HERE, BindOnce(&TestTask, 2, &run_order),
                               delay);
  runners_[0]->PostDelayedTask(FROM_HERE, BindOnce(&TestTask, 3, &run_order),
                               delay);

  EXPECT_EQ(1u, test_task_runner_->GetPendingTaskCount());
}

class TestObject {
 public:
  ~TestObject() { destructor_count__++; }

  void Run() { FAIL() << "TestObject::Run should not be called"; }

  static int destructor_count__;
};

int TestObject::destructor_count__ = 0;

TEST_P(SequenceManagerTest, PendingDelayedTasksRemovedOnShutdown) {
  CreateTaskQueues(1u);

  TestObject::destructor_count__ = 0;

  TimeDelta delay(TimeDelta::FromMilliseconds(10));
  runners_[0]->PostDelayedTask(
      FROM_HERE, BindOnce(&TestObject::Run, Owned(new TestObject())), delay);
  runners_[0]->PostTask(FROM_HERE,
                        BindOnce(&TestObject::Run, Owned(new TestObject())));

  manager_.reset();

  EXPECT_EQ(2, TestObject::destructor_count__);
}

TEST_P(SequenceManagerTest, InsertAndRemoveFence) {
  CreateTaskQueues(1u);
  runners_[0]->InsertFence(TaskQueue::InsertFencePosition::kNow);

  std::vector<EnqueueOrder> run_order;
  // Posting a task when pumping is disabled doesn't result in work getting
  // posted.
  runners_[0]->PostTask(FROM_HERE, BindOnce(&TestTask, 1, &run_order));
  EXPECT_FALSE(test_task_runner_->HasPendingTask());

  // However polling still works.
  EXPECT_TRUE(runners_[0]->HasTaskToRunImmediately());

  // After removing the fence the task runs normally.
  runners_[0]->RemoveFence();
  EXPECT_TRUE(test_task_runner_->HasPendingTask());
  RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1u));
}

TEST_P(SequenceManagerTest, RemovingFenceForDisabledQueueDoesNotPostDoWork) {
  CreateTaskQueues(1u);

  std::vector<EnqueueOrder> run_order;
  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter =
      runners_[0]->CreateQueueEnabledVoter();
  voter->SetQueueEnabled(false);
  runners_[0]->InsertFence(TaskQueue::InsertFencePosition::kNow);
  runners_[0]->PostTask(FROM_HERE, BindOnce(&TestTask, 1, &run_order));

  runners_[0]->RemoveFence();
  EXPECT_FALSE(test_task_runner_->HasPendingTask());
}

TEST_P(SequenceManagerTest, EnablingFencedQueueDoesNotPostDoWork) {
  CreateTaskQueues(1u);

  std::vector<EnqueueOrder> run_order;
  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter =
      runners_[0]->CreateQueueEnabledVoter();
  voter->SetQueueEnabled(false);
  runners_[0]->InsertFence(TaskQueue::InsertFencePosition::kNow);
  runners_[0]->PostTask(FROM_HERE, BindOnce(&TestTask, 1, &run_order));

  voter->SetQueueEnabled(true);
  EXPECT_FALSE(test_task_runner_->HasPendingTask());
}

TEST_P(SequenceManagerTest, DenyRunning_BeforePosting) {
  CreateTaskQueues(1u);

  std::vector<EnqueueOrder> run_order;
  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter =
      runners_[0]->CreateQueueEnabledVoter();
  voter->SetQueueEnabled(false);
  runners_[0]->PostTask(FROM_HERE, BindOnce(&TestTask, 1, &run_order));
  EXPECT_FALSE(test_task_runner_->HasPendingTask());

  RunLoop().RunUntilIdle();
  EXPECT_TRUE(run_order.empty());

  voter->SetQueueEnabled(true);
  RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1u));
}

TEST_P(SequenceManagerTest, DenyRunning_AfterPosting) {
  CreateTaskQueues(1u);

  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostTask(FROM_HERE, BindOnce(&TestTask, 1, &run_order));
  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter =
      runners_[0]->CreateQueueEnabledVoter();
  EXPECT_TRUE(test_task_runner_->HasPendingTask());
  voter->SetQueueEnabled(false);

  RunLoop().RunUntilIdle();
  EXPECT_TRUE(run_order.empty());

  voter->SetQueueEnabled(true);
  RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1u));
}

TEST_P(SequenceManagerTest, DenyRunning_AfterRemovingFence) {
  CreateTaskQueues(1u);

  std::vector<EnqueueOrder> run_order;
  runners_[0]->InsertFence(TaskQueue::InsertFencePosition::kNow);
  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter =
      runners_[0]->CreateQueueEnabledVoter();
  voter->SetQueueEnabled(false);
  runners_[0]->PostTask(FROM_HERE, BindOnce(&TestTask, 1, &run_order));

  RunLoop().RunUntilIdle();
  EXPECT_TRUE(run_order.empty());

  runners_[0]->RemoveFence();
  voter->SetQueueEnabled(true);
  RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1u));
}

TEST_P(SequenceManagerTest, RemovingFenceWithDelayedTask) {
  CreateTaskQueues(1u);
  runners_[0]->InsertFence(TaskQueue::InsertFencePosition::kNow);

  std::vector<EnqueueOrder> run_order;
  // Posting a delayed task when fenced will apply the delay, but won't cause
  // work to executed afterwards.
  TimeDelta delay(TimeDelta::FromMilliseconds(10));
  runners_[0]->PostDelayedTask(FROM_HERE, BindOnce(&TestTask, 1, &run_order),
                               delay);

  // The task does not run even though it's delay is up.
  test_task_runner_->FastForwardBy(TimeDelta::FromMilliseconds(10));
  EXPECT_TRUE(run_order.empty());

  // Removing the fence causes the task to run.
  runners_[0]->RemoveFence();
  EXPECT_TRUE(test_task_runner_->HasPendingTask());
  RunPendingTasks();
  EXPECT_THAT(run_order, ElementsAre(1u));
}

TEST_P(SequenceManagerTest, RemovingFenceWithMultipleDelayedTasks) {
  CreateTaskQueues(1u);
  runners_[0]->InsertFence(TaskQueue::InsertFencePosition::kNow);

  std::vector<EnqueueOrder> run_order;
  // Posting a delayed task when fenced will apply the delay, but won't cause
  // work to executed afterwards.
  TimeDelta delay1(TimeDelta::FromMilliseconds(1));
  TimeDelta delay2(TimeDelta::FromMilliseconds(10));
  TimeDelta delay3(TimeDelta::FromMilliseconds(20));
  runners_[0]->PostDelayedTask(FROM_HERE, BindOnce(&TestTask, 1, &run_order),
                               delay1);
  runners_[0]->PostDelayedTask(FROM_HERE, BindOnce(&TestTask, 2, &run_order),
                               delay2);
  runners_[0]->PostDelayedTask(FROM_HERE, BindOnce(&TestTask, 3, &run_order),
                               delay3);

  test_task_runner_->AdvanceMockTickClock(TimeDelta::FromMilliseconds(15));
  RunLoop().RunUntilIdle();
  EXPECT_TRUE(run_order.empty());

  // Removing the fence causes the ready tasks to run.
  runners_[0]->RemoveFence();
  RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1u, 2u));
}

TEST_P(SequenceManagerTest, InsertFencePreventsDelayedTasksFromRunning) {
  CreateTaskQueues(1u);
  runners_[0]->InsertFence(TaskQueue::InsertFencePosition::kNow);

  std::vector<EnqueueOrder> run_order;
  TimeDelta delay(TimeDelta::FromMilliseconds(10));
  runners_[0]->PostDelayedTask(FROM_HERE, BindOnce(&TestTask, 1, &run_order),
                               delay);

  test_task_runner_->FastForwardBy(TimeDelta::FromMilliseconds(10));
  EXPECT_TRUE(run_order.empty());
}

TEST_P(SequenceManagerTest, MultipleFences) {
  CreateTaskQueues(1u);

  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostTask(FROM_HERE, BindOnce(&TestTask, 1, &run_order));
  runners_[0]->PostTask(FROM_HERE, BindOnce(&TestTask, 2, &run_order));
  runners_[0]->InsertFence(TaskQueue::InsertFencePosition::kNow);

  runners_[0]->PostTask(FROM_HERE, BindOnce(&TestTask, 3, &run_order));
  RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1u, 2u));

  runners_[0]->InsertFence(TaskQueue::InsertFencePosition::kNow);
  // Subsequent tasks should be blocked.
  runners_[0]->PostTask(FROM_HERE, BindOnce(&TestTask, 4, &run_order));
  RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1u, 2u, 3u));
}

TEST_P(SequenceManagerTest, InsertFenceThenImmediatlyRemoveDoesNotBlock) {
  CreateTaskQueues(1u);
  runners_[0]->InsertFence(TaskQueue::InsertFencePosition::kNow);
  runners_[0]->RemoveFence();

  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostTask(FROM_HERE, BindOnce(&TestTask, 1, &run_order));
  runners_[0]->PostTask(FROM_HERE, BindOnce(&TestTask, 2, &run_order));

  RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1u, 2u));
}

TEST_P(SequenceManagerTest, InsertFencePostThenRemoveDoesNotBlock) {
  CreateTaskQueues(1u);
  runners_[0]->InsertFence(TaskQueue::InsertFencePosition::kNow);

  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostTask(FROM_HERE, BindOnce(&TestTask, 1, &run_order));
  runners_[0]->PostTask(FROM_HERE, BindOnce(&TestTask, 2, &run_order));
  runners_[0]->RemoveFence();

  RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1u, 2u));
}

TEST_P(SequenceManagerTest, MultipleFencesWithInitiallyEmptyQueue) {
  CreateTaskQueues(1u);
  runners_[0]->InsertFence(TaskQueue::InsertFencePosition::kNow);

  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostTask(FROM_HERE, BindOnce(&TestTask, 1, &run_order));
  runners_[0]->InsertFence(TaskQueue::InsertFencePosition::kNow);
  runners_[0]->PostTask(FROM_HERE, BindOnce(&TestTask, 2, &run_order));

  RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1u));
}

TEST_P(SequenceManagerTest, BlockedByFence) {
  CreateTaskQueues(1u);
  EXPECT_FALSE(runners_[0]->BlockedByFence());

  runners_[0]->InsertFence(TaskQueue::InsertFencePosition::kNow);
  EXPECT_TRUE(runners_[0]->BlockedByFence());

  runners_[0]->RemoveFence();
  EXPECT_FALSE(runners_[0]->BlockedByFence());

  runners_[0]->PostTask(FROM_HERE, BindOnce(&NopTask));
  runners_[0]->InsertFence(TaskQueue::InsertFencePosition::kNow);
  EXPECT_FALSE(runners_[0]->BlockedByFence());

  RunLoop().RunUntilIdle();
  EXPECT_TRUE(runners_[0]->BlockedByFence());

  runners_[0]->RemoveFence();
  EXPECT_FALSE(runners_[0]->BlockedByFence());
}

TEST_P(SequenceManagerTest, BlockedByFence_BothTypesOfFence) {
  CreateTaskQueues(1u);

  runners_[0]->PostTask(FROM_HERE, BindOnce(&NopTask));

  runners_[0]->InsertFence(TaskQueue::InsertFencePosition::kNow);
  EXPECT_FALSE(runners_[0]->BlockedByFence());

  runners_[0]->InsertFence(TaskQueue::InsertFencePosition::kBeginningOfTime);
  EXPECT_TRUE(runners_[0]->BlockedByFence());
}

namespace {

void RecordTimeTask(std::vector<TimeTicks>* run_times, const TickClock* clock) {
  run_times->push_back(clock->NowTicks());
}

void RecordTimeAndQueueTask(
    std::vector<std::pair<scoped_refptr<TestTaskQueue>, TimeTicks>>* run_times,
    scoped_refptr<TestTaskQueue> task_queue,
    const TickClock* clock) {
  run_times->emplace_back(task_queue, clock->NowTicks());
}

}  // namespace

TEST_P(SequenceManagerTest, DelayedFence_DelayedTasks) {
  CreateTaskQueues(1u);

  std::vector<TimeTicks> run_times;
  runners_[0]->PostDelayedTask(
      FROM_HERE, BindOnce(&RecordTimeTask, &run_times, GetTickClock()),
      TimeDelta::FromMilliseconds(100));
  runners_[0]->PostDelayedTask(
      FROM_HERE, BindOnce(&RecordTimeTask, &run_times, GetTickClock()),
      TimeDelta::FromMilliseconds(200));
  runners_[0]->PostDelayedTask(
      FROM_HERE, BindOnce(&RecordTimeTask, &run_times, GetTickClock()),
      TimeDelta::FromMilliseconds(300));

  runners_[0]->InsertFenceAt(GetTickClock()->NowTicks() +
                             TimeDelta::FromMilliseconds(250));
  EXPECT_FALSE(runners_[0]->HasActiveFence());

  test_task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_TRUE(runners_[0]->HasActiveFence());
  EXPECT_THAT(run_times,
              ElementsAre(start_time_ + TimeDelta::FromMilliseconds(100),
                          start_time_ + TimeDelta::FromMilliseconds(200)));
  run_times.clear();

  runners_[0]->RemoveFence();

  test_task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_FALSE(runners_[0]->HasActiveFence());
  EXPECT_THAT(run_times,
              ElementsAre(start_time_ + TimeDelta::FromMilliseconds(300)));
}

TEST_P(SequenceManagerTest, DelayedFence_ImmediateTasks) {
  CreateTaskQueues(1u);

  std::vector<TimeTicks> run_times;
  runners_[0]->InsertFenceAt(GetTickClock()->NowTicks() +
                             TimeDelta::FromMilliseconds(250));

  for (int i = 0; i < 5; ++i) {
    runners_[0]->PostTask(
        FROM_HERE, BindOnce(&RecordTimeTask, &run_times, GetTickClock()));
    test_task_runner_->FastForwardBy(TimeDelta::FromMilliseconds(100));
    if (i < 2) {
      EXPECT_FALSE(runners_[0]->HasActiveFence());
    } else {
      EXPECT_TRUE(runners_[0]->HasActiveFence());
    }
  }

  EXPECT_THAT(
      run_times,
      ElementsAre(start_time_, start_time_ + TimeDelta::FromMilliseconds(100),
                  start_time_ + TimeDelta::FromMilliseconds(200)));
  run_times.clear();

  runners_[0]->RemoveFence();
  test_task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_THAT(run_times,
              ElementsAre(start_time_ + TimeDelta::FromMilliseconds(500),
                          start_time_ + TimeDelta::FromMilliseconds(500)));
}

TEST_P(SequenceManagerTest, DelayedFence_RemovedFenceDoesNotActivate) {
  CreateTaskQueues(1u);

  std::vector<TimeTicks> run_times;
  runners_[0]->InsertFenceAt(GetTickClock()->NowTicks() +
                             TimeDelta::FromMilliseconds(250));

  for (int i = 0; i < 3; ++i) {
    runners_[0]->PostTask(
        FROM_HERE, BindOnce(&RecordTimeTask, &run_times, GetTickClock()));
    EXPECT_FALSE(runners_[0]->HasActiveFence());
    test_task_runner_->FastForwardBy(TimeDelta::FromMilliseconds(100));
  }

  EXPECT_TRUE(runners_[0]->HasActiveFence());
  runners_[0]->RemoveFence();

  for (int i = 0; i < 2; ++i) {
    runners_[0]->PostTask(
        FROM_HERE, BindOnce(&RecordTimeTask, &run_times, GetTickClock()));
    test_task_runner_->FastForwardBy(TimeDelta::FromMilliseconds(100));
    EXPECT_FALSE(runners_[0]->HasActiveFence());
  }

  EXPECT_THAT(
      run_times,
      ElementsAre(start_time_, start_time_ + TimeDelta::FromMilliseconds(100),
                  start_time_ + TimeDelta::FromMilliseconds(200),
                  start_time_ + TimeDelta::FromMilliseconds(300),
                  start_time_ + TimeDelta::FromMilliseconds(400)));
}

TEST_P(SequenceManagerTest, DelayedFence_TakeIncomingImmediateQueue) {
  // This test checks that everything works correctly when a work queue
  // is swapped with an immediate incoming queue and a delayed fence
  // is activated, forcing a different queue to become active.
  CreateTaskQueues(2u);

  scoped_refptr<TestTaskQueue> queue1 = runners_[0];
  scoped_refptr<TestTaskQueue> queue2 = runners_[1];

  std::vector<std::pair<scoped_refptr<TestTaskQueue>, TimeTicks>> run_times;

  // Fence ensures that the task posted after advancing time is blocked.
  queue1->InsertFenceAt(GetTickClock()->NowTicks() +
                        TimeDelta::FromMilliseconds(250));

  // This task should not be blocked and should run immediately after
  // advancing time at 301ms.
  queue1->PostTask(FROM_HERE, BindOnce(&RecordTimeAndQueueTask, &run_times,
                                       queue1, GetTickClock()));
  // Force reload of immediate work queue. In real life the same effect can be
  // achieved with cross-thread posting.
  queue1->GetTaskQueueImpl()->ReloadImmediateWorkQueueIfEmpty();

  test_task_runner_->AdvanceMockTickClock(TimeDelta::FromMilliseconds(300));

  // This task should be blocked.
  queue1->PostTask(FROM_HERE, BindOnce(&RecordTimeAndQueueTask, &run_times,
                                       queue1, GetTickClock()));
  // This task on a different runner should run as expected.
  queue2->PostTask(FROM_HERE, BindOnce(&RecordTimeAndQueueTask, &run_times,
                                       queue2, GetTickClock()));

  test_task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_THAT(
      run_times,
      ElementsAre(std::make_pair(
                      queue1, start_time_ + TimeDelta::FromMilliseconds(300)),
                  std::make_pair(
                      queue2, start_time_ + TimeDelta::FromMilliseconds(300))));
}

namespace {

void ReentrantTestTask(scoped_refptr<TestTaskQueue> runner,
                       int countdown,
                       std::vector<EnqueueOrder>* out_result) {
  out_result->push_back(EnqueueOrder::FromIntForTesting(countdown));
  if (--countdown) {
    runner->PostTask(
        FROM_HERE, BindOnce(&ReentrantTestTask, runner, countdown, out_result));
  }
}

}  // namespace

TEST_P(SequenceManagerTest, ReentrantPosting) {
  CreateTaskQueues(1u);

  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostTask(
      FROM_HERE, BindOnce(&ReentrantTestTask, runners_[0], 3, &run_order));

  RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(3u, 2u, 1u));
}

TEST_P(SequenceManagerTest, NoTasksAfterShutdown) {
  CreateTaskQueues(1u);

  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostTask(FROM_HERE, BindOnce(&TestTask, 1, &run_order));
  manager_.reset();
  runners_[0]->PostTask(FROM_HERE, BindOnce(&TestTask, 1, &run_order));

  RunLoop().RunUntilIdle();
  EXPECT_TRUE(run_order.empty());
}

void PostTaskToRunner(scoped_refptr<TestTaskQueue> runner,
                      std::vector<EnqueueOrder>* run_order) {
  runner->PostTask(FROM_HERE, BindOnce(&TestTask, 1, run_order));
}

TEST_P(SequenceManagerTestWithMessageLoop, PostFromThread) {
  CreateTaskQueues(1u);

  std::vector<EnqueueOrder> run_order;
  Thread thread("TestThread");
  thread.Start();
  thread.task_runner()->PostTask(
      FROM_HERE, BindOnce(&PostTaskToRunner, runners_[0], &run_order));
  thread.Stop();

  RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1u));
}

void RePostingTestTask(scoped_refptr<TestTaskQueue> runner, int* run_count) {
  (*run_count)++;
  runner->PostTask(FROM_HERE, BindOnce(&RePostingTestTask,
                                       Unretained(runner.get()), run_count));
}

TEST_P(SequenceManagerTest, DoWorkCantPostItselfMultipleTimes) {
  CreateTaskQueues(1u);

  int run_count = 0;
  runners_[0]->PostTask(FROM_HERE,
                        BindOnce(&RePostingTestTask, runners_[0], &run_count));

  RunPendingTasks();
  EXPECT_EQ(1u, test_task_runner_->GetPendingTaskCount());
  EXPECT_EQ(1, run_count);
}

TEST_P(SequenceManagerTestWithMessageLoop, PostFromNestedRunloop) {
  CreateTaskQueues(1u);

  std::vector<EnqueueOrder> run_order;
  std::vector<std::pair<OnceClosure, bool>> tasks_to_post_from_nested_loop;
  tasks_to_post_from_nested_loop.push_back(
      std::make_pair(BindOnce(&TestTask, 1, &run_order), true));

  runners_[0]->PostTask(FROM_HERE, BindOnce(&TestTask, 0, &run_order));
  runners_[0]->PostTask(FROM_HERE,
                        BindOnce(&PostFromNestedRunloop, runners_[0],
                                 Unretained(&tasks_to_post_from_nested_loop)));
  runners_[0]->PostTask(FROM_HERE, BindOnce(&TestTask, 2, &run_order));

  RunLoop().RunUntilIdle();

  EXPECT_THAT(run_order, ElementsAre(0u, 2u, 1u));
}

TEST_P(SequenceManagerTest, WorkBatching) {
  CreateTaskQueues(1u);

  manager_->SetWorkBatchSize(2);

  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostTask(FROM_HERE, BindOnce(&TestTask, 1, &run_order));
  runners_[0]->PostTask(FROM_HERE, BindOnce(&TestTask, 2, &run_order));
  runners_[0]->PostTask(FROM_HERE, BindOnce(&TestTask, 3, &run_order));
  runners_[0]->PostTask(FROM_HERE, BindOnce(&TestTask, 4, &run_order));

  // Running one task in the host message loop should cause two posted tasks to
  // get executed.
  EXPECT_EQ(1u, test_task_runner_->GetPendingTaskCount());
  RunPendingTasks();
  EXPECT_THAT(run_order, ElementsAre(1u, 2u));

  // The second task runs the remaining two posted tasks.
  EXPECT_EQ(1u, test_task_runner_->GetPendingTaskCount());
  RunPendingTasks();
  EXPECT_THAT(run_order, ElementsAre(1u, 2u, 3u, 4u));
}

class MockTaskObserver : public MessageLoop::TaskObserver {
 public:
  MOCK_METHOD1(DidProcessTask, void(const PendingTask& task));
  MOCK_METHOD1(WillProcessTask, void(const PendingTask& task));
};

TEST_P(SequenceManagerTestWithMessageLoop, TaskObserverAdding) {
  CreateTaskQueues(1u);
  MockTaskObserver observer;

  manager_->SetWorkBatchSize(2);
  manager_->AddTaskObserver(&observer);

  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostTask(FROM_HERE, BindOnce(&TestTask, 1, &run_order));
  runners_[0]->PostTask(FROM_HERE, BindOnce(&TestTask, 2, &run_order));

  EXPECT_CALL(observer, WillProcessTask(_)).Times(2);
  EXPECT_CALL(observer, DidProcessTask(_)).Times(2);
  RunLoop().RunUntilIdle();
}

TEST_P(SequenceManagerTestWithMessageLoop, TaskObserverRemoving) {
  CreateTaskQueues(1u);
  MockTaskObserver observer;
  manager_->SetWorkBatchSize(2);
  manager_->AddTaskObserver(&observer);
  manager_->RemoveTaskObserver(&observer);

  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostTask(FROM_HERE, BindOnce(&TestTask, 1, &run_order));

  EXPECT_CALL(observer, WillProcessTask(_)).Times(0);
  EXPECT_CALL(observer, DidProcessTask(_)).Times(0);
  RunLoop().RunUntilIdle();
}

void RemoveObserverTask(SequenceManagerImpl* manager,
                        MessageLoop::TaskObserver* observer) {
  manager->RemoveTaskObserver(observer);
}

TEST_P(SequenceManagerTestWithMessageLoop, TaskObserverRemovingInsideTask) {
  CreateTaskQueues(1u);
  MockTaskObserver observer;
  manager_->SetWorkBatchSize(3);
  manager_->AddTaskObserver(&observer);

  runners_[0]->PostTask(
      FROM_HERE, BindOnce(&RemoveObserverTask, manager_.get(), &observer));

  EXPECT_CALL(observer, WillProcessTask(_)).Times(1);
  EXPECT_CALL(observer, DidProcessTask(_)).Times(0);
  RunLoop().RunUntilIdle();
}

TEST_P(SequenceManagerTestWithMessageLoop, QueueTaskObserverAdding) {
  CreateTaskQueues(2u);
  MockTaskObserver observer;

  manager_->SetWorkBatchSize(2);
  runners_[0]->AddTaskObserver(&observer);

  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostTask(FROM_HERE, BindOnce(&TestTask, 1, &run_order));
  runners_[1]->PostTask(FROM_HERE, BindOnce(&TestTask, 2, &run_order));

  EXPECT_CALL(observer, WillProcessTask(_)).Times(1);
  EXPECT_CALL(observer, DidProcessTask(_)).Times(1);
  RunLoop().RunUntilIdle();
}

TEST_P(SequenceManagerTestWithMessageLoop, QueueTaskObserverRemoving) {
  CreateTaskQueues(1u);
  MockTaskObserver observer;
  manager_->SetWorkBatchSize(2);
  runners_[0]->AddTaskObserver(&observer);
  runners_[0]->RemoveTaskObserver(&observer);

  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostTask(FROM_HERE, BindOnce(&TestTask, 1, &run_order));

  EXPECT_CALL(observer, WillProcessTask(_)).Times(0);
  EXPECT_CALL(observer, DidProcessTask(_)).Times(0);

  RunLoop().RunUntilIdle();
}

void RemoveQueueObserverTask(scoped_refptr<TestTaskQueue> queue,
                             MessageLoop::TaskObserver* observer) {
  queue->RemoveTaskObserver(observer);
}

TEST_P(SequenceManagerTestWithMessageLoop,
       QueueTaskObserverRemovingInsideTask) {
  CreateTaskQueues(1u);
  MockTaskObserver observer;
  runners_[0]->AddTaskObserver(&observer);

  runners_[0]->PostTask(
      FROM_HERE, BindOnce(&RemoveQueueObserverTask, runners_[0], &observer));

  EXPECT_CALL(observer, WillProcessTask(_)).Times(1);
  EXPECT_CALL(observer, DidProcessTask(_)).Times(0);
  RunLoop().RunUntilIdle();
}

TEST_P(SequenceManagerTest, ThreadCheckAfterTermination) {
  CreateTaskQueues(1u);
  EXPECT_TRUE(runners_[0]->RunsTasksInCurrentSequence());
  manager_.reset();
  EXPECT_TRUE(runners_[0]->RunsTasksInCurrentSequence());
}

TEST_P(SequenceManagerTest, TimeDomain_NextScheduledRunTime) {
  CreateTaskQueues(2u);
  test_task_runner_->AdvanceMockTickClock(TimeDelta::FromMicroseconds(10000));
  LazyNow lazy_now_1(GetTickClock());

  // With no delayed tasks.
  EXPECT_FALSE(manager_->GetRealTimeDomain()->DelayTillNextTask(&lazy_now_1));

  // With a non-delayed task.
  runners_[0]->PostTask(FROM_HERE, BindOnce(&NopTask));
  EXPECT_FALSE(manager_->GetRealTimeDomain()->DelayTillNextTask(&lazy_now_1));

  // With a delayed task.
  TimeDelta expected_delay = TimeDelta::FromMilliseconds(50);
  runners_[0]->PostDelayedTask(FROM_HERE, BindOnce(&NopTask), expected_delay);
  EXPECT_EQ(expected_delay,
            manager_->GetRealTimeDomain()->DelayTillNextTask(&lazy_now_1));

  // With another delayed task in the same queue with a longer delay.
  runners_[0]->PostDelayedTask(FROM_HERE, BindOnce(&NopTask),
                               TimeDelta::FromMilliseconds(100));
  EXPECT_EQ(expected_delay,
            manager_->GetRealTimeDomain()->DelayTillNextTask(&lazy_now_1));

  // With another delayed task in the same queue with a shorter delay.
  expected_delay = TimeDelta::FromMilliseconds(20);
  runners_[0]->PostDelayedTask(FROM_HERE, BindOnce(&NopTask), expected_delay);
  EXPECT_EQ(expected_delay,
            manager_->GetRealTimeDomain()->DelayTillNextTask(&lazy_now_1));

  // With another delayed task in a different queue with a shorter delay.
  expected_delay = TimeDelta::FromMilliseconds(10);
  runners_[1]->PostDelayedTask(FROM_HERE, BindOnce(&NopTask), expected_delay);
  EXPECT_EQ(expected_delay,
            manager_->GetRealTimeDomain()->DelayTillNextTask(&lazy_now_1));

  // Test it updates as time progresses
  test_task_runner_->AdvanceMockTickClock(expected_delay);
  LazyNow lazy_now_2(GetTickClock());
  EXPECT_EQ(TimeDelta(),
            manager_->GetRealTimeDomain()->DelayTillNextTask(&lazy_now_2));
}

TEST_P(SequenceManagerTest, TimeDomain_NextScheduledRunTime_MultipleQueues) {
  CreateTaskQueues(3u);

  TimeDelta delay1 = TimeDelta::FromMilliseconds(50);
  TimeDelta delay2 = TimeDelta::FromMilliseconds(5);
  TimeDelta delay3 = TimeDelta::FromMilliseconds(10);
  runners_[0]->PostDelayedTask(FROM_HERE, BindOnce(&NopTask), delay1);
  runners_[1]->PostDelayedTask(FROM_HERE, BindOnce(&NopTask), delay2);
  runners_[2]->PostDelayedTask(FROM_HERE, BindOnce(&NopTask), delay3);
  runners_[0]->PostTask(FROM_HERE, BindOnce(&NopTask));

  LazyNow lazy_now(GetTickClock());
  EXPECT_EQ(delay2,
            manager_->GetRealTimeDomain()->DelayTillNextTask(&lazy_now));
}

TEST_P(SequenceManagerTest, DeleteSequenceManagerInsideATask) {
  CreateTaskQueues(1u);

  runners_[0]->PostTask(
      FROM_HERE, BindOnce(&SequenceManagerTest::DeleteSequenceManagerTask,
                          Unretained(this)));

  // This should not crash, assuming DoWork detects the SequenceManager has
  // been deleted.
  RunLoop().RunUntilIdle();
}

TEST_P(SequenceManagerTest, GetAndClearSystemIsQuiescentBit) {
  CreateTaskQueues(3u);

  scoped_refptr<TestTaskQueue> queue0 =
      CreateTaskQueue(TaskQueue::Spec("test").SetShouldMonitorQuiescence(true));
  scoped_refptr<TestTaskQueue> queue1 =
      CreateTaskQueue(TaskQueue::Spec("test").SetShouldMonitorQuiescence(true));
  scoped_refptr<TestTaskQueue> queue2 = CreateTaskQueue();

  EXPECT_TRUE(manager_->GetAndClearSystemIsQuiescentBit());

  queue0->PostTask(FROM_HERE, BindOnce(&NopTask));
  RunLoop().RunUntilIdle();
  EXPECT_FALSE(manager_->GetAndClearSystemIsQuiescentBit());
  EXPECT_TRUE(manager_->GetAndClearSystemIsQuiescentBit());

  queue1->PostTask(FROM_HERE, BindOnce(&NopTask));
  RunLoop().RunUntilIdle();
  EXPECT_FALSE(manager_->GetAndClearSystemIsQuiescentBit());
  EXPECT_TRUE(manager_->GetAndClearSystemIsQuiescentBit());

  queue2->PostTask(FROM_HERE, BindOnce(&NopTask));
  RunLoop().RunUntilIdle();
  EXPECT_TRUE(manager_->GetAndClearSystemIsQuiescentBit());

  queue0->PostTask(FROM_HERE, BindOnce(&NopTask));
  queue1->PostTask(FROM_HERE, BindOnce(&NopTask));
  RunLoop().RunUntilIdle();
  EXPECT_FALSE(manager_->GetAndClearSystemIsQuiescentBit());
  EXPECT_TRUE(manager_->GetAndClearSystemIsQuiescentBit());
}

TEST_P(SequenceManagerTest, HasPendingImmediateWork) {
  CreateTaskQueues(1u);

  EXPECT_FALSE(runners_[0]->HasTaskToRunImmediately());
  runners_[0]->PostTask(FROM_HERE, BindOnce(NullTask));
  EXPECT_TRUE(runners_[0]->HasTaskToRunImmediately());

  RunLoop().RunUntilIdle();
  EXPECT_FALSE(runners_[0]->HasTaskToRunImmediately());
}

TEST_P(SequenceManagerTest, HasPendingImmediateWork_DelayedTasks) {
  CreateTaskQueues(1u);

  EXPECT_FALSE(runners_[0]->HasTaskToRunImmediately());
  runners_[0]->PostDelayedTask(FROM_HERE, BindOnce(NullTask),
                               TimeDelta::FromMilliseconds(12));
  EXPECT_FALSE(runners_[0]->HasTaskToRunImmediately());

  // Move time forwards until just before the delayed task should run.
  test_task_runner_->AdvanceMockTickClock(TimeDelta::FromMilliseconds(10));
  LazyNow lazy_now_1(GetTickClock());
  manager_->WakeUpReadyDelayedQueues(&lazy_now_1);
  EXPECT_FALSE(runners_[0]->HasTaskToRunImmediately());

  // Force the delayed task onto the work queue.
  test_task_runner_->AdvanceMockTickClock(TimeDelta::FromMilliseconds(2));
  LazyNow lazy_now_2(GetTickClock());
  manager_->WakeUpReadyDelayedQueues(&lazy_now_2);
  EXPECT_TRUE(runners_[0]->HasTaskToRunImmediately());

  RunLoop().RunUntilIdle();
  EXPECT_FALSE(runners_[0]->HasTaskToRunImmediately());
}

void ExpensiveTestTask(int value,
                       scoped_refptr<TestMockTimeTaskRunner> test_task_runner,
                       std::vector<EnqueueOrder>* out_result) {
  out_result->push_back(EnqueueOrder::FromIntForTesting(value));
  test_task_runner->FastForwardBy(TimeDelta::FromMilliseconds(1));
}

TEST_P(SequenceManagerTest, ImmediateAndDelayedTaskInterleaving) {
  CreateTaskQueues(1u);

  std::vector<EnqueueOrder> run_order;
  TimeDelta delay = TimeDelta::FromMilliseconds(10);
  for (int i = 10; i < 19; i++) {
    runners_[0]->PostDelayedTask(
        FROM_HERE,
        BindOnce(&ExpensiveTestTask, i, test_task_runner_, &run_order), delay);
  }

  test_task_runner_->FastForwardBy(TimeDelta::FromMilliseconds(10));

  for (int i = 0; i < 9; i++) {
    runners_[0]->PostTask(FROM_HERE, BindOnce(&ExpensiveTestTask, i,
                                              test_task_runner_, &run_order));
  }

  test_task_runner_->FastForwardUntilNoTasksRemain();

  // Delayed tasks are not allowed to starve out immediate work which is why
  // some of the immediate tasks run out of order.
  uint64_t expected_run_order[] = {10u, 11u, 12u, 13u, 0u, 14u, 15u, 16u, 1u,
                                   17u, 18u, 2u,  3u,  4u, 5u,  6u,  7u,  8u};
  EXPECT_THAT(run_order, ElementsAreArray(expected_run_order));
}

TEST_P(SequenceManagerTest,
       DelayedTaskDoesNotSkipAHeadOfNonDelayedTask_SameQueue) {
  CreateTaskQueues(1u);

  std::vector<EnqueueOrder> run_order;
  TimeDelta delay = TimeDelta::FromMilliseconds(10);
  runners_[0]->PostTask(FROM_HERE, BindOnce(&TestTask, 2, &run_order));
  runners_[0]->PostTask(FROM_HERE, BindOnce(&TestTask, 3, &run_order));
  runners_[0]->PostDelayedTask(FROM_HERE, BindOnce(&TestTask, 1, &run_order),
                               delay);

  test_task_runner_->AdvanceMockTickClock(delay * 2);
  RunLoop().RunUntilIdle();

  EXPECT_THAT(run_order, ElementsAre(2u, 3u, 1u));
}

TEST_P(SequenceManagerTest,
       DelayedTaskDoesNotSkipAHeadOfNonDelayedTask_DifferentQueues) {
  CreateTaskQueues(2u);

  std::vector<EnqueueOrder> run_order;
  TimeDelta delay = TimeDelta::FromMilliseconds(10);
  runners_[1]->PostTask(FROM_HERE, BindOnce(&TestTask, 2, &run_order));
  runners_[1]->PostTask(FROM_HERE, BindOnce(&TestTask, 3, &run_order));
  runners_[0]->PostDelayedTask(FROM_HERE, BindOnce(&TestTask, 1, &run_order),
                               delay);

  test_task_runner_->AdvanceMockTickClock(delay * 2);
  RunLoop().RunUntilIdle();

  EXPECT_THAT(run_order, ElementsAre(2u, 3u, 1u));
}

TEST_P(SequenceManagerTest, DelayedTaskDoesNotSkipAHeadOfShorterDelayedTask) {
  CreateTaskQueues(2u);

  std::vector<EnqueueOrder> run_order;
  TimeDelta delay1 = TimeDelta::FromMilliseconds(10);
  TimeDelta delay2 = TimeDelta::FromMilliseconds(5);
  runners_[0]->PostDelayedTask(FROM_HERE, BindOnce(&TestTask, 1, &run_order),
                               delay1);
  runners_[1]->PostDelayedTask(FROM_HERE, BindOnce(&TestTask, 2, &run_order),
                               delay2);

  test_task_runner_->AdvanceMockTickClock(delay1 * 2);
  RunLoop().RunUntilIdle();

  EXPECT_THAT(run_order, ElementsAre(2u, 1u));
}

void CheckIsNested(bool* is_nested) {
  *is_nested = RunLoop::IsNestedOnCurrentThread();
}

void PostAndQuitFromNestedRunloop(RunLoop* run_loop,
                                  scoped_refptr<TestTaskQueue> runner,
                                  bool* was_nested) {
  runner->PostTask(FROM_HERE, run_loop->QuitClosure());
  runner->PostTask(FROM_HERE, BindOnce(&CheckIsNested, was_nested));
  run_loop->Run();
}

TEST_P(SequenceManagerTestWithMessageLoop, QuitWhileNested) {
  // This test makes sure we don't continue running a work batch after a nested
  // run loop has been exited in the middle of the batch.
  CreateTaskQueues(1u);
  manager_->SetWorkBatchSize(2);

  bool was_nested = true;
  RunLoop run_loop(RunLoop::Type::kNestableTasksAllowed);
  runners_[0]->PostTask(
      FROM_HERE, BindOnce(&PostAndQuitFromNestedRunloop, Unretained(&run_loop),
                          runners_[0], Unretained(&was_nested)));

  RunLoop().RunUntilIdle();
  EXPECT_FALSE(was_nested);
}

class SequenceNumberCapturingTaskObserver : public MessageLoop::TaskObserver {
 public:
  // MessageLoop::TaskObserver overrides.
  void WillProcessTask(const PendingTask& pending_task) override {}
  void DidProcessTask(const PendingTask& pending_task) override {
    sequence_numbers_.push_back(pending_task.sequence_num);
  }

  const std::vector<int>& sequence_numbers() const { return sequence_numbers_; }

 private:
  std::vector<int> sequence_numbers_;
};

TEST_P(SequenceManagerTest, SequenceNumSetWhenTaskIsPosted) {
  CreateTaskQueues(1u);

  SequenceNumberCapturingTaskObserver observer;
  manager_->AddTaskObserver(&observer);

  // Register four tasks that will run in reverse order.
  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostDelayedTask(FROM_HERE, BindOnce(&TestTask, 1, &run_order),
                               TimeDelta::FromMilliseconds(30));
  runners_[0]->PostDelayedTask(FROM_HERE, BindOnce(&TestTask, 2, &run_order),
                               TimeDelta::FromMilliseconds(20));
  runners_[0]->PostDelayedTask(FROM_HERE, BindOnce(&TestTask, 3, &run_order),
                               TimeDelta::FromMilliseconds(10));
  runners_[0]->PostTask(FROM_HERE, BindOnce(&TestTask, 4, &run_order));

  test_task_runner_->FastForwardBy(TimeDelta::FromMilliseconds(40));
  ASSERT_THAT(run_order, ElementsAre(4u, 3u, 2u, 1u));

  // The sequence numbers are a one-based monotonically incrememting counter
  // which should be set when the task is posted rather than when it's enqueued
  // onto the Incoming queue. This counter starts with 2.
  EXPECT_THAT(observer.sequence_numbers(), ElementsAre(5, 4, 3, 2));

  manager_->RemoveTaskObserver(&observer);
}

TEST_P(SequenceManagerTest, NewTaskQueues) {
  CreateTaskQueues(1u);

  scoped_refptr<TestTaskQueue> queue1 = CreateTaskQueue();
  scoped_refptr<TestTaskQueue> queue2 = CreateTaskQueue();
  scoped_refptr<TestTaskQueue> queue3 = CreateTaskQueue();

  ASSERT_NE(queue1, queue2);
  ASSERT_NE(queue1, queue3);
  ASSERT_NE(queue2, queue3);

  std::vector<EnqueueOrder> run_order;
  queue1->PostTask(FROM_HERE, BindOnce(&TestTask, 1, &run_order));
  queue2->PostTask(FROM_HERE, BindOnce(&TestTask, 2, &run_order));
  queue3->PostTask(FROM_HERE, BindOnce(&TestTask, 3, &run_order));
  RunLoop().RunUntilIdle();

  EXPECT_THAT(run_order, ElementsAre(1u, 2u, 3u));
}

TEST_P(SequenceManagerTest, ShutdownTaskQueue_TaskRunnersDetaching) {
  scoped_refptr<TestTaskQueue> queue = CreateTaskQueue();

  scoped_refptr<SingleThreadTaskRunner> runner1 = queue->task_runner();
  scoped_refptr<SingleThreadTaskRunner> runner2 = queue->CreateTaskRunner(1);

  std::vector<EnqueueOrder> run_order;
  EXPECT_TRUE(runner1->PostTask(FROM_HERE, BindOnce(&TestTask, 1, &run_order)));
  EXPECT_TRUE(runner2->PostTask(FROM_HERE, BindOnce(&TestTask, 2, &run_order)));
  queue->ShutdownTaskQueue();
  EXPECT_FALSE(
      runner1->PostTask(FROM_HERE, BindOnce(&TestTask, 3, &run_order)));
  EXPECT_FALSE(
      runner2->PostTask(FROM_HERE, BindOnce(&TestTask, 4, &run_order)));

  RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre());
}

TEST_P(SequenceManagerTest, ShutdownTaskQueue) {
  CreateTaskQueues(1u);

  scoped_refptr<TestTaskQueue> queue1 = CreateTaskQueue();
  scoped_refptr<TestTaskQueue> queue2 = CreateTaskQueue();
  scoped_refptr<TestTaskQueue> queue3 = CreateTaskQueue();

  ASSERT_NE(queue1, queue2);
  ASSERT_NE(queue1, queue3);
  ASSERT_NE(queue2, queue3);

  std::vector<EnqueueOrder> run_order;
  queue1->PostTask(FROM_HERE, BindOnce(&TestTask, 1, &run_order));
  queue2->PostTask(FROM_HERE, BindOnce(&TestTask, 2, &run_order));
  queue3->PostTask(FROM_HERE, BindOnce(&TestTask, 3, &run_order));
  queue2->ShutdownTaskQueue();
  RunLoop().RunUntilIdle();

  EXPECT_THAT(run_order, ElementsAre(1u, 3u));
}

TEST_P(SequenceManagerTest, ShutdownTaskQueue_WithDelayedTasks) {
  CreateTaskQueues(2u);

  // Register three delayed tasks
  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostDelayedTask(FROM_HERE, BindOnce(&TestTask, 1, &run_order),
                               TimeDelta::FromMilliseconds(10));
  runners_[1]->PostDelayedTask(FROM_HERE, BindOnce(&TestTask, 2, &run_order),
                               TimeDelta::FromMilliseconds(20));
  runners_[0]->PostDelayedTask(FROM_HERE, BindOnce(&TestTask, 3, &run_order),
                               TimeDelta::FromMilliseconds(30));

  runners_[1]->ShutdownTaskQueue();
  RunLoop().RunUntilIdle();

  test_task_runner_->FastForwardBy(TimeDelta::FromMilliseconds(40));
  ASSERT_THAT(run_order, ElementsAre(1u, 3u));
}

namespace {
void ShutdownQueue(scoped_refptr<TestTaskQueue> queue) {
  queue->ShutdownTaskQueue();
}
}  // namespace

TEST_P(SequenceManagerTest, ShutdownTaskQueue_InTasks) {
  CreateTaskQueues(3u);

  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostTask(FROM_HERE, BindOnce(&TestTask, 1, &run_order));
  runners_[0]->PostTask(FROM_HERE, BindOnce(&ShutdownQueue, runners_[1]));
  runners_[0]->PostTask(FROM_HERE, BindOnce(&ShutdownQueue, runners_[2]));
  runners_[1]->PostTask(FROM_HERE, BindOnce(&TestTask, 2, &run_order));
  runners_[2]->PostTask(FROM_HERE, BindOnce(&TestTask, 3, &run_order));

  RunLoop().RunUntilIdle();
  ASSERT_THAT(run_order, ElementsAre(1u));
}

namespace {

class MockObserver : public SequenceManager::Observer {
 public:
  MOCK_METHOD0(OnTriedToExecuteBlockedTask, void());
  MOCK_METHOD0(OnBeginNestedRunLoop, void());
  MOCK_METHOD0(OnExitNestedRunLoop, void());
};

}  // namespace

TEST_P(SequenceManagerTestWithMessageLoop, ShutdownTaskQueueInNestedLoop) {
  CreateTaskQueues(1u);

  // We retain a reference to the task queue even when the manager has deleted
  // its reference.
  scoped_refptr<TestTaskQueue> task_queue = CreateTaskQueue();

  std::vector<bool> log;
  std::vector<std::pair<OnceClosure, bool>> tasks_to_post_from_nested_loop;

  // Inside a nested run loop, call task_queue->ShutdownTaskQueue, bookended
  // by calls to HasOneRefTask to make sure the manager doesn't release its
  // reference until the nested run loop exits.
  // NB: This first HasOneRefTask is a sanity check.
  tasks_to_post_from_nested_loop.push_back(
      std::make_pair(BindOnce(&NopTask), true));
  tasks_to_post_from_nested_loop.push_back(std::make_pair(
      BindOnce(&TaskQueue::ShutdownTaskQueue, Unretained(task_queue.get())),
      true));
  tasks_to_post_from_nested_loop.push_back(
      std::make_pair(BindOnce(&NopTask), true));
  runners_[0]->PostTask(FROM_HERE,
                        BindOnce(&PostFromNestedRunloop, runners_[0],
                                 Unretained(&tasks_to_post_from_nested_loop)));
  RunLoop().RunUntilIdle();

  // Just make sure that we don't crash.
}

TEST_P(SequenceManagerTest, TimeDomainsAreIndependant) {
  CreateTaskQueues(2u);

  TimeTicks start_time_ticks = manager_->NowTicks();
  std::unique_ptr<MockTimeDomain> domain_a =
      std::make_unique<MockTimeDomain>(start_time_ticks);
  std::unique_ptr<MockTimeDomain> domain_b =
      std::make_unique<MockTimeDomain>(start_time_ticks);
  manager_->RegisterTimeDomain(domain_a.get());
  manager_->RegisterTimeDomain(domain_b.get());
  runners_[0]->SetTimeDomain(domain_a.get());
  runners_[1]->SetTimeDomain(domain_b.get());

  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostDelayedTask(FROM_HERE, BindOnce(&TestTask, 1, &run_order),
                               TimeDelta::FromMilliseconds(10));
  runners_[0]->PostDelayedTask(FROM_HERE, BindOnce(&TestTask, 2, &run_order),
                               TimeDelta::FromMilliseconds(20));
  runners_[0]->PostDelayedTask(FROM_HERE, BindOnce(&TestTask, 3, &run_order),
                               TimeDelta::FromMilliseconds(30));

  runners_[1]->PostDelayedTask(FROM_HERE, BindOnce(&TestTask, 4, &run_order),
                               TimeDelta::FromMilliseconds(10));
  runners_[1]->PostDelayedTask(FROM_HERE, BindOnce(&TestTask, 5, &run_order),
                               TimeDelta::FromMilliseconds(20));
  runners_[1]->PostDelayedTask(FROM_HERE, BindOnce(&TestTask, 6, &run_order),
                               TimeDelta::FromMilliseconds(30));

  domain_b->SetNowTicks(start_time_ticks + TimeDelta::FromMilliseconds(50));
  manager_->MaybeScheduleImmediateWork(FROM_HERE);

  RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(4u, 5u, 6u));

  domain_a->SetNowTicks(start_time_ticks + TimeDelta::FromMilliseconds(50));
  manager_->MaybeScheduleImmediateWork(FROM_HERE);

  RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(4u, 5u, 6u, 1u, 2u, 3u));

  runners_[0]->ShutdownTaskQueue();
  runners_[1]->ShutdownTaskQueue();

  manager_->UnregisterTimeDomain(domain_a.get());
  manager_->UnregisterTimeDomain(domain_b.get());
}

TEST_P(SequenceManagerTest, TimeDomainMigration) {
  CreateTaskQueues(1u);

  TimeTicks start_time_ticks = manager_->NowTicks();
  std::unique_ptr<MockTimeDomain> domain_a =
      std::make_unique<MockTimeDomain>(start_time_ticks);
  manager_->RegisterTimeDomain(domain_a.get());
  runners_[0]->SetTimeDomain(domain_a.get());

  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostDelayedTask(FROM_HERE, BindOnce(&TestTask, 1, &run_order),
                               TimeDelta::FromMilliseconds(10));
  runners_[0]->PostDelayedTask(FROM_HERE, BindOnce(&TestTask, 2, &run_order),
                               TimeDelta::FromMilliseconds(20));
  runners_[0]->PostDelayedTask(FROM_HERE, BindOnce(&TestTask, 3, &run_order),
                               TimeDelta::FromMilliseconds(30));
  runners_[0]->PostDelayedTask(FROM_HERE, BindOnce(&TestTask, 4, &run_order),
                               TimeDelta::FromMilliseconds(40));

  domain_a->SetNowTicks(start_time_ticks + TimeDelta::FromMilliseconds(20));
  manager_->MaybeScheduleImmediateWork(FROM_HERE);
  RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1u, 2u));

  std::unique_ptr<MockTimeDomain> domain_b =
      std::make_unique<MockTimeDomain>(start_time_ticks);
  manager_->RegisterTimeDomain(domain_b.get());
  runners_[0]->SetTimeDomain(domain_b.get());

  domain_b->SetNowTicks(start_time_ticks + TimeDelta::FromMilliseconds(50));
  manager_->MaybeScheduleImmediateWork(FROM_HERE);

  RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1u, 2u, 3u, 4u));

  runners_[0]->ShutdownTaskQueue();

  manager_->UnregisterTimeDomain(domain_a.get());
  manager_->UnregisterTimeDomain(domain_b.get());
}

TEST_P(SequenceManagerTest, TimeDomainMigrationWithIncomingImmediateTasks) {
  CreateTaskQueues(1u);

  TimeTicks start_time_ticks = manager_->NowTicks();
  std::unique_ptr<MockTimeDomain> domain_a =
      std::make_unique<MockTimeDomain>(start_time_ticks);
  std::unique_ptr<MockTimeDomain> domain_b =
      std::make_unique<MockTimeDomain>(start_time_ticks);
  manager_->RegisterTimeDomain(domain_a.get());
  manager_->RegisterTimeDomain(domain_b.get());

  runners_[0]->SetTimeDomain(domain_a.get());
  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostTask(FROM_HERE, BindOnce(&TestTask, 1, &run_order));
  runners_[0]->SetTimeDomain(domain_b.get());

  RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1u));

  runners_[0]->ShutdownTaskQueue();

  manager_->UnregisterTimeDomain(domain_a.get());
  manager_->UnregisterTimeDomain(domain_b.get());
}

TEST_P(SequenceManagerTest,
       PostDelayedTasksReverseOrderAlternatingTimeDomains) {
  CreateTaskQueues(1u);

  std::vector<EnqueueOrder> run_order;

  std::unique_ptr<internal::RealTimeDomain> domain_a =
      std::make_unique<internal::RealTimeDomain>();
  std::unique_ptr<internal::RealTimeDomain> domain_b =
      std::make_unique<internal::RealTimeDomain>();
  manager_->RegisterTimeDomain(domain_a.get());
  manager_->RegisterTimeDomain(domain_b.get());

  runners_[0]->SetTimeDomain(domain_a.get());
  runners_[0]->PostDelayedTask(FROM_HERE, BindOnce(&TestTask, 1, &run_order),
                               TimeDelta::FromMilliseconds(40));

  runners_[0]->SetTimeDomain(domain_b.get());
  runners_[0]->PostDelayedTask(FROM_HERE, BindOnce(&TestTask, 2, &run_order),
                               TimeDelta::FromMilliseconds(30));

  runners_[0]->SetTimeDomain(domain_a.get());
  runners_[0]->PostDelayedTask(FROM_HERE, BindOnce(&TestTask, 3, &run_order),
                               TimeDelta::FromMilliseconds(20));

  runners_[0]->SetTimeDomain(domain_b.get());
  runners_[0]->PostDelayedTask(FROM_HERE, BindOnce(&TestTask, 4, &run_order),
                               TimeDelta::FromMilliseconds(10));

  test_task_runner_->FastForwardBy(TimeDelta::FromMilliseconds(40));
  EXPECT_THAT(run_order, ElementsAre(4u, 3u, 2u, 1u));

  runners_[0]->ShutdownTaskQueue();

  manager_->UnregisterTimeDomain(domain_a.get());
  manager_->UnregisterTimeDomain(domain_b.get());
}

namespace {

class MockTaskQueueObserver : public TaskQueue::Observer {
 public:
  ~MockTaskQueueObserver() override = default;

  MOCK_METHOD2(OnQueueNextWakeUpChanged, void(TaskQueue*, TimeTicks));
};

}  // namespace

TEST_P(SequenceManagerTest, TaskQueueObserver_ImmediateTask) {
  CreateTaskQueues(1u);

  MockTaskQueueObserver observer;
  runners_[0]->SetObserver(&observer);

  // We should get a notification when a task is posted on an empty queue.
  EXPECT_CALL(observer, OnQueueNextWakeUpChanged(runners_[0].get(), _));
  runners_[0]->PostTask(FROM_HERE, BindOnce(&NopTask));
  Mock::VerifyAndClearExpectations(&observer);

  // But not subsequently.
  EXPECT_CALL(observer, OnQueueNextWakeUpChanged(_, _)).Times(0);
  runners_[0]->PostTask(FROM_HERE, BindOnce(&NopTask));
  Mock::VerifyAndClearExpectations(&observer);

  // Unless the immediate work queue is emptied.
  runners_[0]->GetTaskQueueImpl()->ReloadImmediateWorkQueueIfEmpty();
  EXPECT_CALL(observer, OnQueueNextWakeUpChanged(runners_[0].get(), _));
  runners_[0]->PostTask(FROM_HERE, BindOnce(&NopTask));

  // Tidy up.
  runners_[0]->ShutdownTaskQueue();
}

TEST_P(SequenceManagerTest, TaskQueueObserver_DelayedTask) {
  CreateTaskQueues(1u);

  TimeTicks start_time = manager_->NowTicks();
  TimeDelta delay10s(TimeDelta::FromSeconds(10));
  TimeDelta delay100s(TimeDelta::FromSeconds(100));
  TimeDelta delay1s(TimeDelta::FromSeconds(1));

  MockTaskQueueObserver observer;
  runners_[0]->SetObserver(&observer);

  // We should get a notification when a delayed task is posted on an empty
  // queue.
  EXPECT_CALL(observer, OnQueueNextWakeUpChanged(runners_[0].get(),
                                                 start_time + delay10s));
  runners_[0]->PostDelayedTask(FROM_HERE, BindOnce(&NopTask), delay10s);
  Mock::VerifyAndClearExpectations(&observer);

  // We should not get a notification for a longer delay.
  EXPECT_CALL(observer, OnQueueNextWakeUpChanged(_, _)).Times(0);
  runners_[0]->PostDelayedTask(FROM_HERE, BindOnce(&NopTask), delay100s);
  Mock::VerifyAndClearExpectations(&observer);

  // We should get a notification for a shorter delay.
  EXPECT_CALL(observer, OnQueueNextWakeUpChanged(runners_[0].get(),
                                                 start_time + delay1s));
  runners_[0]->PostDelayedTask(FROM_HERE, BindOnce(&NopTask), delay1s);
  Mock::VerifyAndClearExpectations(&observer);

  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter =
      runners_[0]->CreateQueueEnabledVoter();
  voter->SetQueueEnabled(false);
  Mock::VerifyAndClearExpectations(&observer);

  // When a queue has been enabled, we may get a notification if the
  // TimeDomain's next scheduled wake-up has changed.
  EXPECT_CALL(observer, OnQueueNextWakeUpChanged(runners_[0].get(),
                                                 start_time + delay1s));
  voter->SetQueueEnabled(true);
  Mock::VerifyAndClearExpectations(&observer);

  // Tidy up.
  runners_[0]->ShutdownTaskQueue();
}

TEST_P(SequenceManagerTest, TaskQueueObserver_DelayedTaskMultipleQueues) {
  CreateTaskQueues(2u);

  MockTaskQueueObserver observer;
  runners_[0]->SetObserver(&observer);
  runners_[1]->SetObserver(&observer);

  TimeTicks start_time = manager_->NowTicks();
  TimeDelta delay1s(TimeDelta::FromSeconds(1));
  TimeDelta delay10s(TimeDelta::FromSeconds(10));

  EXPECT_CALL(observer,
              OnQueueNextWakeUpChanged(runners_[0].get(), start_time + delay1s))
      .Times(1);
  EXPECT_CALL(observer, OnQueueNextWakeUpChanged(runners_[1].get(),
                                                 start_time + delay10s))
      .Times(1);
  runners_[0]->PostDelayedTask(FROM_HERE, BindOnce(&NopTask), delay1s);
  runners_[1]->PostDelayedTask(FROM_HERE, BindOnce(&NopTask), delay10s);
  testing::Mock::VerifyAndClearExpectations(&observer);

  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter0 =
      runners_[0]->CreateQueueEnabledVoter();
  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter1 =
      runners_[1]->CreateQueueEnabledVoter();

  // Disabling a queue should not trigger a notification.
  EXPECT_CALL(observer, OnQueueNextWakeUpChanged(_, _)).Times(0);
  voter0->SetQueueEnabled(false);
  Mock::VerifyAndClearExpectations(&observer);

  // Re-enabling it should should also trigger a notification.
  EXPECT_CALL(observer, OnQueueNextWakeUpChanged(runners_[0].get(),
                                                 start_time + delay1s));
  voter0->SetQueueEnabled(true);
  Mock::VerifyAndClearExpectations(&observer);

  // Disabling a queue should not trigger a notification.
  EXPECT_CALL(observer, OnQueueNextWakeUpChanged(_, _)).Times(0);
  voter1->SetQueueEnabled(false);
  Mock::VerifyAndClearExpectations(&observer);

  // Re-enabling it should should trigger a notification.
  EXPECT_CALL(observer, OnQueueNextWakeUpChanged(runners_[1].get(),
                                                 start_time + delay10s));
  voter1->SetQueueEnabled(true);
  Mock::VerifyAndClearExpectations(&observer);

  // Tidy up.
  EXPECT_CALL(observer, OnQueueNextWakeUpChanged(_, _)).Times(AnyNumber());
  runners_[0]->ShutdownTaskQueue();
  runners_[1]->ShutdownTaskQueue();
}

TEST_P(SequenceManagerTest, TaskQueueObserver_DelayedWorkWhichCanRunNow) {
  // This test checks that when delayed work becomes available
  // the notification still fires. This usually happens when time advances
  // and task becomes available in the middle of the scheduling code.
  // For this test we rely on the fact that notification dispatching code
  // is the same in all conditions and just change a time domain to
  // trigger notification.

  CreateTaskQueues(1u);

  TimeDelta delay1s(TimeDelta::FromSeconds(1));
  TimeDelta delay10s(TimeDelta::FromSeconds(10));

  MockTaskQueueObserver observer;
  runners_[0]->SetObserver(&observer);

  // We should get a notification when a delayed task is posted on an empty
  // queue.
  EXPECT_CALL(observer, OnQueueNextWakeUpChanged(_, _));
  runners_[0]->PostDelayedTask(FROM_HERE, BindOnce(&NopTask), delay1s);
  Mock::VerifyAndClearExpectations(&observer);

  std::unique_ptr<TimeDomain> mock_time_domain =
      std::make_unique<internal::RealTimeDomain>();
  manager_->RegisterTimeDomain(mock_time_domain.get());

  test_task_runner_->AdvanceMockTickClock(delay10s);

  EXPECT_CALL(observer, OnQueueNextWakeUpChanged(_, _));
  runners_[0]->SetTimeDomain(mock_time_domain.get());
  Mock::VerifyAndClearExpectations(&observer);

  // Tidy up.
  runners_[0]->ShutdownTaskQueue();
}

class CancelableTask {
 public:
  explicit CancelableTask(const TickClock* clock)
      : clock_(clock), weak_factory_(this) {}

  void RecordTimeTask(std::vector<TimeTicks>* run_times) {
    run_times->push_back(clock_->NowTicks());
  }

  const TickClock* clock_;
  WeakPtrFactory<CancelableTask> weak_factory_;
};

TEST_P(SequenceManagerTest, TaskQueueObserver_SweepCanceledDelayedTasks) {
  CreateTaskQueues(1u);

  MockTaskQueueObserver observer;
  runners_[0]->SetObserver(&observer);

  TimeTicks start_time = manager_->NowTicks();
  TimeDelta delay1(TimeDelta::FromSeconds(5));
  TimeDelta delay2(TimeDelta::FromSeconds(10));

  EXPECT_CALL(observer,
              OnQueueNextWakeUpChanged(runners_[0].get(), start_time + delay1))
      .Times(1);

  CancelableTask task1(GetTickClock());
  CancelableTask task2(GetTickClock());
  std::vector<TimeTicks> run_times;
  runners_[0]->PostDelayedTask(
      FROM_HERE,
      BindOnce(&CancelableTask::RecordTimeTask,
               task1.weak_factory_.GetWeakPtr(), &run_times),
      delay1);
  runners_[0]->PostDelayedTask(
      FROM_HERE,
      BindOnce(&CancelableTask::RecordTimeTask,
               task2.weak_factory_.GetWeakPtr(), &run_times),
      delay2);

  task1.weak_factory_.InvalidateWeakPtrs();

  // Sweeping away canceled delayed tasks should trigger a notification.
  EXPECT_CALL(observer,
              OnQueueNextWakeUpChanged(runners_[0].get(), start_time + delay2))
      .Times(1);
  manager_->SweepCanceledDelayedTasks();
}

namespace {
void ChromiumRunloopInspectionTask(
    scoped_refptr<TestMockTimeTaskRunner> test_task_runner) {
  // We don't expect more than 1 pending task at any time.
  EXPECT_GE(1u, test_task_runner->GetPendingTaskCount());
}
}  // namespace

TEST_P(SequenceManagerTest, NumberOfPendingTasksOnChromiumRunLoop) {
  CreateTaskQueues(1u);

  // NOTE because tasks posted to the chromiumrun loop are not cancellable, we
  // will end up with a lot more tasks posted if the delayed tasks were posted
  // in the reverse order.
  // TODO(alexclarke): Consider talking to the message pump directly.
  for (int i = 1; i < 100; i++) {
    runners_[0]->PostDelayedTask(
        FROM_HERE, BindOnce(&ChromiumRunloopInspectionTask, test_task_runner_),
        TimeDelta::FromMilliseconds(i));
  }
  test_task_runner_->FastForwardUntilNoTasksRemain();
}

namespace {

class QuadraticTask {
 public:
  QuadraticTask(scoped_refptr<TestTaskQueue> task_queue,
                TimeDelta delay,
                scoped_refptr<TestMockTimeTaskRunner> test_task_runner)
      : count_(0),
        task_queue_(task_queue),
        delay_(delay),
        test_task_runner_(test_task_runner) {}

  void SetShouldExit(RepeatingCallback<bool()> should_exit) {
    should_exit_ = should_exit;
  }

  void Run() {
    if (should_exit_.Run())
      return;
    count_++;
    task_queue_->PostDelayedTask(
        FROM_HERE, BindOnce(&QuadraticTask::Run, Unretained(this)), delay_);
    task_queue_->PostDelayedTask(
        FROM_HERE, BindOnce(&QuadraticTask::Run, Unretained(this)), delay_);
    test_task_runner_->FastForwardBy(TimeDelta::FromMilliseconds(5));
  }

  int Count() const { return count_; }

 private:
  int count_;
  scoped_refptr<TestTaskQueue> task_queue_;
  TimeDelta delay_;
  RepeatingCallback<bool()> should_exit_;
  scoped_refptr<TestMockTimeTaskRunner> test_task_runner_;
};

class LinearTask {
 public:
  LinearTask(scoped_refptr<TestTaskQueue> task_queue,
             TimeDelta delay,
             scoped_refptr<TestMockTimeTaskRunner> test_task_runner)
      : count_(0),
        task_queue_(task_queue),
        delay_(delay),
        test_task_runner_(test_task_runner) {}

  void SetShouldExit(RepeatingCallback<bool()> should_exit) {
    should_exit_ = should_exit;
  }

  void Run() {
    if (should_exit_.Run())
      return;
    count_++;
    task_queue_->PostDelayedTask(
        FROM_HERE, BindOnce(&LinearTask::Run, Unretained(this)), delay_);
    test_task_runner_->FastForwardBy(TimeDelta::FromMilliseconds(5));
  }

  int Count() const { return count_; }

 private:
  int count_;
  scoped_refptr<TestTaskQueue> task_queue_;
  TimeDelta delay_;
  RepeatingCallback<bool()> should_exit_;
  scoped_refptr<TestMockTimeTaskRunner> test_task_runner_;
};

bool ShouldExit(QuadraticTask* quadratic_task, LinearTask* linear_task) {
  return quadratic_task->Count() == 1000 || linear_task->Count() == 1000;
}

}  // namespace

TEST_P(SequenceManagerTest,
       DelayedTasksDontBadlyStarveNonDelayedWork_SameQueue) {
  CreateTaskQueues(1u);

  QuadraticTask quadratic_delayed_task(
      runners_[0], TimeDelta::FromMilliseconds(10), test_task_runner_);
  LinearTask linear_immediate_task(runners_[0], TimeDelta(), test_task_runner_);
  RepeatingCallback<bool()> should_exit = BindRepeating(
      ShouldExit, &quadratic_delayed_task, &linear_immediate_task);
  quadratic_delayed_task.SetShouldExit(should_exit);
  linear_immediate_task.SetShouldExit(should_exit);

  quadratic_delayed_task.Run();
  linear_immediate_task.Run();

  test_task_runner_->FastForwardUntilNoTasksRemain();

  double ratio = static_cast<double>(linear_immediate_task.Count()) /
                 static_cast<double>(quadratic_delayed_task.Count());

  EXPECT_GT(ratio, 0.333);
  EXPECT_LT(ratio, 1.1);
}

TEST_P(SequenceManagerTest, ImmediateWorkCanStarveDelayedTasks_SameQueue) {
  CreateTaskQueues(1u);

  QuadraticTask quadratic_immediate_task(runners_[0], TimeDelta(),
                                         test_task_runner_);
  LinearTask linear_delayed_task(runners_[0], TimeDelta::FromMilliseconds(10),
                                 test_task_runner_);
  RepeatingCallback<bool()> should_exit = BindRepeating(
      &ShouldExit, &quadratic_immediate_task, &linear_delayed_task);

  quadratic_immediate_task.SetShouldExit(should_exit);
  linear_delayed_task.SetShouldExit(should_exit);

  quadratic_immediate_task.Run();
  linear_delayed_task.Run();

  test_task_runner_->FastForwardUntilNoTasksRemain();

  double ratio = static_cast<double>(linear_delayed_task.Count()) /
                 static_cast<double>(quadratic_immediate_task.Count());

  // This is by design, we want to enforce a strict ordering in task execution
  // where by delayed tasks can not skip ahead of non-delayed work.
  EXPECT_GT(ratio, 0.0);
  EXPECT_LT(ratio, 0.1);
}

TEST_P(SequenceManagerTest,
       DelayedTasksDontBadlyStarveNonDelayedWork_DifferentQueue) {
  CreateTaskQueues(2u);

  QuadraticTask quadratic_delayed_task(
      runners_[0], TimeDelta::FromMilliseconds(10), test_task_runner_);
  LinearTask linear_immediate_task(runners_[1], TimeDelta(), test_task_runner_);
  RepeatingCallback<bool()> should_exit = BindRepeating(
      ShouldExit, &quadratic_delayed_task, &linear_immediate_task);
  quadratic_delayed_task.SetShouldExit(should_exit);
  linear_immediate_task.SetShouldExit(should_exit);

  quadratic_delayed_task.Run();
  linear_immediate_task.Run();

  test_task_runner_->FastForwardUntilNoTasksRemain();

  double ratio = static_cast<double>(linear_immediate_task.Count()) /
                 static_cast<double>(quadratic_delayed_task.Count());

  EXPECT_GT(ratio, 0.333);
  EXPECT_LT(ratio, 1.1);
}

TEST_P(SequenceManagerTest, ImmediateWorkCanStarveDelayedTasks_DifferentQueue) {
  CreateTaskQueues(2u);

  QuadraticTask quadratic_immediate_task(runners_[0], TimeDelta(),
                                         test_task_runner_);
  LinearTask linear_delayed_task(runners_[1], TimeDelta::FromMilliseconds(10),
                                 test_task_runner_);
  RepeatingCallback<bool()> should_exit = BindRepeating(
      &ShouldExit, &quadratic_immediate_task, &linear_delayed_task);

  quadratic_immediate_task.SetShouldExit(should_exit);
  linear_delayed_task.SetShouldExit(should_exit);

  quadratic_immediate_task.Run();
  linear_delayed_task.Run();

  test_task_runner_->FastForwardUntilNoTasksRemain();

  double ratio = static_cast<double>(linear_delayed_task.Count()) /
                 static_cast<double>(quadratic_immediate_task.Count());

  // This is by design, we want to enforce a strict ordering in task execution
  // where by delayed tasks can not skip ahead of non-delayed work.
  EXPECT_GT(ratio, 0.0);
  EXPECT_LT(ratio, 0.1);
}

TEST_P(SequenceManagerTest, CurrentlyExecutingTaskQueue_NoTaskRunning) {
  CreateTaskQueues(1u);

  EXPECT_EQ(nullptr, manager_->currently_executing_task_queue());
}

namespace {
void CurrentlyExecutingTaskQueueTestTask(
    SequenceManagerImpl* sequence_manager,
    std::vector<internal::TaskQueueImpl*>* task_sources) {
  task_sources->push_back(sequence_manager->currently_executing_task_queue());
}
}  // namespace

TEST_P(SequenceManagerTest, CurrentlyExecutingTaskQueue_TaskRunning) {
  CreateTaskQueues(2u);

  TestTaskQueue* queue0 = runners_[0].get();
  TestTaskQueue* queue1 = runners_[1].get();

  std::vector<internal::TaskQueueImpl*> task_sources;
  queue0->PostTask(FROM_HERE, BindOnce(&CurrentlyExecutingTaskQueueTestTask,
                                       manager_.get(), &task_sources));
  queue1->PostTask(FROM_HERE, BindOnce(&CurrentlyExecutingTaskQueueTestTask,
                                       manager_.get(), &task_sources));
  RunLoop().RunUntilIdle();

  EXPECT_THAT(task_sources, ElementsAre(queue0->GetTaskQueueImpl(),
                                        queue1->GetTaskQueueImpl()));
  EXPECT_EQ(nullptr, manager_->currently_executing_task_queue());
}

namespace {
void RunloopCurrentlyExecutingTaskQueueTestTask(
    SequenceManagerImpl* sequence_manager,
    std::vector<internal::TaskQueueImpl*>* task_sources,
    std::vector<std::pair<OnceClosure, TestTaskQueue*>>* tasks) {
  task_sources->push_back(sequence_manager->currently_executing_task_queue());

  for (std::pair<OnceClosure, TestTaskQueue*>& pair : *tasks) {
    pair.second->PostTask(FROM_HERE, std::move(pair.first));
  }

  RunLoop(RunLoop::Type::kNestableTasksAllowed).RunUntilIdle();
  task_sources->push_back(sequence_manager->currently_executing_task_queue());
}
}  // namespace

TEST_P(SequenceManagerTestWithMessageLoop,
       CurrentlyExecutingTaskQueue_NestedLoop) {
  CreateTaskQueues(3u);

  TestTaskQueue* queue0 = runners_[0].get();
  TestTaskQueue* queue1 = runners_[1].get();
  TestTaskQueue* queue2 = runners_[2].get();

  std::vector<internal::TaskQueueImpl*> task_sources;
  std::vector<std::pair<OnceClosure, TestTaskQueue*>>
      tasks_to_post_from_nested_loop;
  tasks_to_post_from_nested_loop.push_back(
      std::make_pair(BindOnce(&CurrentlyExecutingTaskQueueTestTask,
                              manager_.get(), &task_sources),
                     queue1));
  tasks_to_post_from_nested_loop.push_back(
      std::make_pair(BindOnce(&CurrentlyExecutingTaskQueueTestTask,
                              manager_.get(), &task_sources),
                     queue2));

  queue0->PostTask(
      FROM_HERE,
      BindOnce(&RunloopCurrentlyExecutingTaskQueueTestTask, manager_.get(),
               &task_sources, &tasks_to_post_from_nested_loop));

  RunLoop().RunUntilIdle();
  EXPECT_THAT(
      task_sources,
      ElementsAre(queue0->GetTaskQueueImpl(), queue1->GetTaskQueueImpl(),
                  queue2->GetTaskQueueImpl(), queue0->GetTaskQueueImpl()));
  EXPECT_EQ(nullptr, manager_->currently_executing_task_queue());
}

TEST_P(SequenceManagerTestWithMessageLoop, BlameContextAttribution) {
  using trace_analyzer::Query;

  CreateTaskQueues(1u);
  TestTaskQueue* queue = runners_[0].get();

  trace_analyzer::Start("*");
  {
    trace_event::BlameContext blame_context("cat", "name", "type", "scope", 0,
                                            nullptr);
    blame_context.Initialize();
    queue->SetBlameContext(&blame_context);
    queue->PostTask(FROM_HERE, BindOnce(&NopTask));
    RunLoop().RunUntilIdle();
  }
  auto analyzer = trace_analyzer::Stop();

  trace_analyzer::TraceEventVector events;
  Query q = Query::EventPhaseIs(TRACE_EVENT_PHASE_ENTER_CONTEXT) ||
            Query::EventPhaseIs(TRACE_EVENT_PHASE_LEAVE_CONTEXT);
  analyzer->FindEvents(q, &events);

  EXPECT_EQ(2u, events.size());
}

TEST_P(SequenceManagerTest, NoWakeUpsForCanceledDelayedTasks) {
  CreateTaskQueues(1u);

  TimeTicks start_time = manager_->NowTicks();

  CancelableTask task1(GetTickClock());
  CancelableTask task2(GetTickClock());
  CancelableTask task3(GetTickClock());
  CancelableTask task4(GetTickClock());
  TimeDelta delay1(TimeDelta::FromSeconds(5));
  TimeDelta delay2(TimeDelta::FromSeconds(10));
  TimeDelta delay3(TimeDelta::FromSeconds(15));
  TimeDelta delay4(TimeDelta::FromSeconds(30));
  std::vector<TimeTicks> run_times;
  runners_[0]->PostDelayedTask(
      FROM_HERE,
      BindOnce(&CancelableTask::RecordTimeTask,
               task1.weak_factory_.GetWeakPtr(), &run_times),
      delay1);
  runners_[0]->PostDelayedTask(
      FROM_HERE,
      BindOnce(&CancelableTask::RecordTimeTask,
               task2.weak_factory_.GetWeakPtr(), &run_times),
      delay2);
  runners_[0]->PostDelayedTask(
      FROM_HERE,
      BindOnce(&CancelableTask::RecordTimeTask,
               task3.weak_factory_.GetWeakPtr(), &run_times),
      delay3);
  runners_[0]->PostDelayedTask(
      FROM_HERE,
      BindOnce(&CancelableTask::RecordTimeTask,
               task4.weak_factory_.GetWeakPtr(), &run_times),
      delay4);

  task2.weak_factory_.InvalidateWeakPtrs();
  task3.weak_factory_.InvalidateWeakPtrs();

  std::set<TimeTicks> wake_up_times;

  RunUntilManagerIsIdle(BindRepeating(
      [](std::set<TimeTicks>* wake_up_times, const TickClock* clock) {
        wake_up_times->insert(clock->NowTicks());
      },
      &wake_up_times, GetTickClock()));

  EXPECT_THAT(wake_up_times,
              ElementsAre(start_time + delay1, start_time + delay4));
  EXPECT_THAT(run_times, ElementsAre(start_time + delay1, start_time + delay4));
}

TEST_P(SequenceManagerTest, NoWakeUpsForCanceledDelayedTasksReversePostOrder) {
  CreateTaskQueues(1u);

  TimeTicks start_time = manager_->NowTicks();

  CancelableTask task1(GetTickClock());
  CancelableTask task2(GetTickClock());
  CancelableTask task3(GetTickClock());
  CancelableTask task4(GetTickClock());
  TimeDelta delay1(TimeDelta::FromSeconds(5));
  TimeDelta delay2(TimeDelta::FromSeconds(10));
  TimeDelta delay3(TimeDelta::FromSeconds(15));
  TimeDelta delay4(TimeDelta::FromSeconds(30));
  std::vector<TimeTicks> run_times;
  runners_[0]->PostDelayedTask(
      FROM_HERE,
      BindOnce(&CancelableTask::RecordTimeTask,
               task4.weak_factory_.GetWeakPtr(), &run_times),
      delay4);
  runners_[0]->PostDelayedTask(
      FROM_HERE,
      BindOnce(&CancelableTask::RecordTimeTask,
               task3.weak_factory_.GetWeakPtr(), &run_times),
      delay3);
  runners_[0]->PostDelayedTask(
      FROM_HERE,
      BindOnce(&CancelableTask::RecordTimeTask,
               task2.weak_factory_.GetWeakPtr(), &run_times),
      delay2);
  runners_[0]->PostDelayedTask(
      FROM_HERE,
      BindOnce(&CancelableTask::RecordTimeTask,
               task1.weak_factory_.GetWeakPtr(), &run_times),
      delay1);

  task2.weak_factory_.InvalidateWeakPtrs();
  task3.weak_factory_.InvalidateWeakPtrs();

  std::set<TimeTicks> wake_up_times;

  RunUntilManagerIsIdle(BindRepeating(
      [](std::set<TimeTicks>* wake_up_times, const TickClock* clock) {
        wake_up_times->insert(clock->NowTicks());
      },
      &wake_up_times, GetTickClock()));

  EXPECT_THAT(wake_up_times,
              ElementsAre(start_time + delay1, start_time + delay4));
  EXPECT_THAT(run_times, ElementsAre(start_time + delay1, start_time + delay4));
}

TEST_P(SequenceManagerTest, TimeDomainWakeUpOnlyCancelledIfAllUsesCancelled) {
  CreateTaskQueues(1u);

  TimeTicks start_time = manager_->NowTicks();

  CancelableTask task1(GetTickClock());
  CancelableTask task2(GetTickClock());
  CancelableTask task3(GetTickClock());
  CancelableTask task4(GetTickClock());
  TimeDelta delay1(TimeDelta::FromSeconds(5));
  TimeDelta delay2(TimeDelta::FromSeconds(10));
  TimeDelta delay3(TimeDelta::FromSeconds(15));
  TimeDelta delay4(TimeDelta::FromSeconds(30));
  std::vector<TimeTicks> run_times;
  runners_[0]->PostDelayedTask(
      FROM_HERE,
      BindOnce(&CancelableTask::RecordTimeTask,
               task1.weak_factory_.GetWeakPtr(), &run_times),
      delay1);
  runners_[0]->PostDelayedTask(
      FROM_HERE,
      BindOnce(&CancelableTask::RecordTimeTask,
               task2.weak_factory_.GetWeakPtr(), &run_times),
      delay2);
  runners_[0]->PostDelayedTask(
      FROM_HERE,
      BindOnce(&CancelableTask::RecordTimeTask,
               task3.weak_factory_.GetWeakPtr(), &run_times),
      delay3);
  runners_[0]->PostDelayedTask(
      FROM_HERE,
      BindOnce(&CancelableTask::RecordTimeTask,
               task4.weak_factory_.GetWeakPtr(), &run_times),
      delay4);

  // Post a non-canceled task with |delay3|. So we should still get a wake-up at
  // |delay3| even though we cancel |task3|.
  runners_[0]->PostDelayedTask(
      FROM_HERE,
      BindOnce(&CancelableTask::RecordTimeTask, Unretained(&task3), &run_times),
      delay3);

  task2.weak_factory_.InvalidateWeakPtrs();
  task3.weak_factory_.InvalidateWeakPtrs();
  task1.weak_factory_.InvalidateWeakPtrs();

  std::set<TimeTicks> wake_up_times;

  RunUntilManagerIsIdle(BindRepeating(
      [](std::set<TimeTicks>* wake_up_times, const TickClock* clock) {
        wake_up_times->insert(clock->NowTicks());
      },
      &wake_up_times, GetTickClock()));

  EXPECT_THAT(wake_up_times,
              ElementsAre(start_time + delay1, start_time + delay3,
                          start_time + delay4));

  EXPECT_THAT(run_times, ElementsAre(start_time + delay3, start_time + delay4));
}

TEST_P(SequenceManagerTest, TaskQueueVoters) {
  CreateTaskQueues(1u);

  // The task queue should be initially enabled.
  EXPECT_TRUE(runners_[0]->IsQueueEnabled());

  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter1 =
      runners_[0]->CreateQueueEnabledVoter();
  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter2 =
      runners_[0]->CreateQueueEnabledVoter();
  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter3 =
      runners_[0]->CreateQueueEnabledVoter();
  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter4 =
      runners_[0]->CreateQueueEnabledVoter();

  // Voters should initially vote for the queue to be enabled.
  EXPECT_TRUE(runners_[0]->IsQueueEnabled());

  // If any voter wants to disable, the queue is disabled.
  voter1->SetQueueEnabled(false);
  EXPECT_FALSE(runners_[0]->IsQueueEnabled());

  // If the voter is deleted then the queue should be re-enabled.
  voter1.reset();
  EXPECT_TRUE(runners_[0]->IsQueueEnabled());

  // If any of the remaining voters wants to disable, the queue should be
  // disabled.
  voter2->SetQueueEnabled(false);
  EXPECT_FALSE(runners_[0]->IsQueueEnabled());

  // If another queue votes to disable, nothing happens because it's already
  // disabled.
  voter3->SetQueueEnabled(false);
  EXPECT_FALSE(runners_[0]->IsQueueEnabled());

  // There are two votes to disable, so one of them voting to enable does
  // nothing.
  voter2->SetQueueEnabled(true);
  EXPECT_FALSE(runners_[0]->IsQueueEnabled());

  // IF all queues vote to enable then the queue is enabled.
  voter3->SetQueueEnabled(true);
  EXPECT_TRUE(runners_[0]->IsQueueEnabled());
}

TEST_P(SequenceManagerTest, ShutdownQueueBeforeEnabledVoterDeleted) {
  CreateTaskQueues(1u);

  scoped_refptr<TestTaskQueue> queue = CreateTaskQueue();

  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter =
      queue->CreateQueueEnabledVoter();

  voter->SetQueueEnabled(true);  // NOP
  queue->ShutdownTaskQueue();

  // This should complete without DCHECKing.
  voter.reset();
}

TEST_P(SequenceManagerTest, ShutdownQueueBeforeDisabledVoterDeleted) {
  CreateTaskQueues(1u);

  scoped_refptr<TestTaskQueue> queue = CreateTaskQueue();

  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter =
      queue->CreateQueueEnabledVoter();

  voter->SetQueueEnabled(false);
  queue->ShutdownTaskQueue();

  // This should complete without DCHECKing.
  voter.reset();
}

TEST_P(SequenceManagerTest, SweepCanceledDelayedTasks) {
  CreateTaskQueues(1u);

  CancelableTask task1(GetTickClock());
  CancelableTask task2(GetTickClock());
  CancelableTask task3(GetTickClock());
  CancelableTask task4(GetTickClock());
  TimeDelta delay1(TimeDelta::FromSeconds(5));
  TimeDelta delay2(TimeDelta::FromSeconds(10));
  TimeDelta delay3(TimeDelta::FromSeconds(15));
  TimeDelta delay4(TimeDelta::FromSeconds(30));
  std::vector<TimeTicks> run_times;
  runners_[0]->PostDelayedTask(
      FROM_HERE,
      BindOnce(&CancelableTask::RecordTimeTask,
               task1.weak_factory_.GetWeakPtr(), &run_times),
      delay1);
  runners_[0]->PostDelayedTask(
      FROM_HERE,
      BindOnce(&CancelableTask::RecordTimeTask,
               task2.weak_factory_.GetWeakPtr(), &run_times),
      delay2);
  runners_[0]->PostDelayedTask(
      FROM_HERE,
      BindOnce(&CancelableTask::RecordTimeTask,
               task3.weak_factory_.GetWeakPtr(), &run_times),
      delay3);
  runners_[0]->PostDelayedTask(
      FROM_HERE,
      BindOnce(&CancelableTask::RecordTimeTask,
               task4.weak_factory_.GetWeakPtr(), &run_times),
      delay4);

  EXPECT_EQ(4u, runners_[0]->GetNumberOfPendingTasks());
  task2.weak_factory_.InvalidateWeakPtrs();
  task3.weak_factory_.InvalidateWeakPtrs();
  EXPECT_EQ(4u, runners_[0]->GetNumberOfPendingTasks());

  manager_->SweepCanceledDelayedTasks();
  EXPECT_EQ(2u, runners_[0]->GetNumberOfPendingTasks());

  task1.weak_factory_.InvalidateWeakPtrs();
  task4.weak_factory_.InvalidateWeakPtrs();

  manager_->SweepCanceledDelayedTasks();
  EXPECT_EQ(0u, runners_[0]->GetNumberOfPendingTasks());
}

TEST_P(SequenceManagerTest, DelayTillNextTask) {
  CreateTaskQueues(2u);

  LazyNow lazy_now(GetTickClock());
  EXPECT_EQ(TimeDelta::Max(), manager_->DelayTillNextTask(&lazy_now));

  runners_[0]->PostDelayedTask(FROM_HERE, BindOnce(&NopTask),
                               TimeDelta::FromSeconds(10));

  EXPECT_EQ(TimeDelta::FromSeconds(10), manager_->DelayTillNextTask(&lazy_now));

  runners_[1]->PostDelayedTask(FROM_HERE, BindOnce(&NopTask),
                               TimeDelta::FromSeconds(15));

  EXPECT_EQ(TimeDelta::FromSeconds(10), manager_->DelayTillNextTask(&lazy_now));

  runners_[1]->PostDelayedTask(FROM_HERE, BindOnce(&NopTask),
                               TimeDelta::FromSeconds(5));

  EXPECT_EQ(TimeDelta::FromSeconds(5), manager_->DelayTillNextTask(&lazy_now));

  runners_[0]->PostTask(FROM_HERE, BindOnce(&NopTask));

  EXPECT_EQ(TimeDelta(), manager_->DelayTillNextTask(&lazy_now));
}

TEST_P(SequenceManagerTest, DelayTillNextTask_Disabled) {
  CreateTaskQueues(1u);

  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter =
      runners_[0]->CreateQueueEnabledVoter();
  voter->SetQueueEnabled(false);
  runners_[0]->PostTask(FROM_HERE, BindOnce(&NopTask));

  LazyNow lazy_now(GetTickClock());
  EXPECT_EQ(TimeDelta::Max(), manager_->DelayTillNextTask(&lazy_now));
}

TEST_P(SequenceManagerTest, DelayTillNextTask_Fence) {
  CreateTaskQueues(1u);

  runners_[0]->InsertFence(TaskQueue::InsertFencePosition::kNow);
  runners_[0]->PostTask(FROM_HERE, BindOnce(&NopTask));

  LazyNow lazy_now(GetTickClock());
  EXPECT_EQ(TimeDelta::Max(), manager_->DelayTillNextTask(&lazy_now));
}

TEST_P(SequenceManagerTest, DelayTillNextTask_FenceUnblocking) {
  CreateTaskQueues(1u);

  runners_[0]->InsertFence(TaskQueue::InsertFencePosition::kNow);
  runners_[0]->PostTask(FROM_HERE, BindOnce(&NopTask));
  runners_[0]->InsertFence(TaskQueue::InsertFencePosition::kNow);

  LazyNow lazy_now(GetTickClock());
  EXPECT_EQ(TimeDelta(), manager_->DelayTillNextTask(&lazy_now));
}

TEST_P(SequenceManagerTest, DelayTillNextTask_DelayedTaskReady) {
  CreateTaskQueues(1u);

  runners_[0]->PostDelayedTask(FROM_HERE, BindOnce(&NopTask),
                               TimeDelta::FromSeconds(1));

  test_task_runner_->AdvanceMockTickClock(TimeDelta::FromSeconds(10));

  LazyNow lazy_now(GetTickClock());
  EXPECT_EQ(TimeDelta(), manager_->DelayTillNextTask(&lazy_now));
}

namespace {
void MessageLoopTaskWithDelayedQuit(SimpleTestTickClock* now_src,
                                    scoped_refptr<TestTaskQueue> task_queue) {
  RunLoop run_loop(RunLoop::Type::kNestableTasksAllowed);
  task_queue->PostDelayedTask(FROM_HERE, run_loop.QuitClosure(),
                              TimeDelta::FromMilliseconds(100));
  now_src->Advance(TimeDelta::FromMilliseconds(200));
  run_loop.Run();
}
}  // namespace

TEST_P(SequenceManagerTestWithMessageLoop, DelayedTaskRunsInNestedMessageLoop) {
  CreateTaskQueues(1u);
  RunLoop run_loop;
  runners_[0]->PostTask(FROM_HERE,
                        BindOnce(&MessageLoopTaskWithDelayedQuit, &mock_clock_,
                                 RetainedRef(runners_[0])));
  run_loop.RunUntilIdle();
}

namespace {
void MessageLoopTaskWithImmediateQuit(OnceClosure non_nested_quit_closure,
                                      scoped_refptr<TestTaskQueue> task_queue) {
  RunLoop run_loop(RunLoop::Type::kNestableTasksAllowed);
  // Needed because entering the nested run loop causes a DoWork to get
  // posted.
  task_queue->PostTask(FROM_HERE, BindOnce(&NopTask));
  task_queue->PostTask(FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
  std::move(non_nested_quit_closure).Run();
}
}  // namespace

TEST_P(SequenceManagerTestWithMessageLoop,
       DelayedNestedMessageLoopDoesntPreventTasksRunning) {
  CreateTaskQueues(1u);
  RunLoop run_loop;
  runners_[0]->PostDelayedTask(
      FROM_HERE,
      BindOnce(&MessageLoopTaskWithImmediateQuit, run_loop.QuitClosure(),
               RetainedRef(runners_[0])),
      TimeDelta::FromMilliseconds(100));

  mock_clock_.Advance(TimeDelta::FromMilliseconds(200));
  run_loop.Run();
}

TEST_P(SequenceManagerTest, CouldTaskRun_DisableAndReenable) {
  CreateTaskQueues(1u);

  EnqueueOrder enqueue_order = manager_->GetNextSequenceNumber();
  EXPECT_TRUE(runners_[0]->GetTaskQueueImpl()->CouldTaskRun(enqueue_order));

  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter =
      runners_[0]->CreateQueueEnabledVoter();
  voter->SetQueueEnabled(false);
  EXPECT_FALSE(runners_[0]->GetTaskQueueImpl()->CouldTaskRun(enqueue_order));

  voter->SetQueueEnabled(true);
  EXPECT_TRUE(runners_[0]->GetTaskQueueImpl()->CouldTaskRun(enqueue_order));
}

TEST_P(SequenceManagerTest, CouldTaskRun_Fence) {
  CreateTaskQueues(1u);

  EnqueueOrder enqueue_order = manager_->GetNextSequenceNumber();
  EXPECT_TRUE(runners_[0]->GetTaskQueueImpl()->CouldTaskRun(enqueue_order));

  runners_[0]->InsertFence(TaskQueue::InsertFencePosition::kNow);
  EXPECT_TRUE(runners_[0]->GetTaskQueueImpl()->CouldTaskRun(enqueue_order));

  runners_[0]->InsertFence(TaskQueue::InsertFencePosition::kBeginningOfTime);
  EXPECT_FALSE(runners_[0]->GetTaskQueueImpl()->CouldTaskRun(enqueue_order));

  runners_[0]->RemoveFence();
  EXPECT_TRUE(runners_[0]->GetTaskQueueImpl()->CouldTaskRun(enqueue_order));
}

TEST_P(SequenceManagerTest, CouldTaskRun_FenceBeforeThenAfter) {
  CreateTaskQueues(1u);

  runners_[0]->InsertFence(TaskQueue::InsertFencePosition::kNow);

  EnqueueOrder enqueue_order = manager_->GetNextSequenceNumber();
  EXPECT_FALSE(runners_[0]->GetTaskQueueImpl()->CouldTaskRun(enqueue_order));

  runners_[0]->InsertFence(TaskQueue::InsertFencePosition::kNow);
  EXPECT_TRUE(runners_[0]->GetTaskQueueImpl()->CouldTaskRun(enqueue_order));
}

TEST_P(SequenceManagerTest, DelayedDoWorkNotPostedForDisabledQueue) {
  CreateTaskQueues(1u);

  runners_[0]->PostDelayedTask(FROM_HERE, BindOnce(&NopTask),
                               TimeDelta::FromMilliseconds(1));
  ASSERT_TRUE(test_task_runner_->HasPendingTask());
  EXPECT_EQ(TimeDelta::FromMilliseconds(1),
            test_task_runner_->NextPendingTaskDelay());

  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter =
      runners_[0]->CreateQueueEnabledVoter();
  voter->SetQueueEnabled(false);
  EXPECT_FALSE(test_task_runner_->HasPendingTask());

  voter->SetQueueEnabled(true);
  ASSERT_TRUE(test_task_runner_->HasPendingTask());
  EXPECT_EQ(TimeDelta::FromMilliseconds(1),
            test_task_runner_->NextPendingTaskDelay());
}

TEST_P(SequenceManagerTest, DisablingQueuesChangesDelayTillNextDoWork) {
  CreateTaskQueues(3u);
  runners_[0]->PostDelayedTask(FROM_HERE, BindOnce(&NopTask),
                               TimeDelta::FromMilliseconds(1));
  runners_[1]->PostDelayedTask(FROM_HERE, BindOnce(&NopTask),
                               TimeDelta::FromMilliseconds(10));
  runners_[2]->PostDelayedTask(FROM_HERE, BindOnce(&NopTask),
                               TimeDelta::FromMilliseconds(100));

  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter0 =
      runners_[0]->CreateQueueEnabledVoter();
  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter1 =
      runners_[1]->CreateQueueEnabledVoter();
  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter2 =
      runners_[2]->CreateQueueEnabledVoter();

  ASSERT_TRUE(test_task_runner_->HasPendingTask());
  EXPECT_EQ(TimeDelta::FromMilliseconds(1),
            test_task_runner_->NextPendingTaskDelay());

  voter0->SetQueueEnabled(false);
  ASSERT_TRUE(test_task_runner_->HasPendingTask());
  EXPECT_EQ(TimeDelta::FromMilliseconds(10),
            test_task_runner_->NextPendingTaskDelay());

  voter1->SetQueueEnabled(false);
  ASSERT_TRUE(test_task_runner_->HasPendingTask());
  EXPECT_EQ(TimeDelta::FromMilliseconds(100),
            test_task_runner_->NextPendingTaskDelay());

  voter2->SetQueueEnabled(false);
  EXPECT_FALSE(test_task_runner_->HasPendingTask());
}

TEST_P(SequenceManagerTest, GetNextScheduledWakeUp) {
  CreateTaskQueues(1u);

  EXPECT_EQ(nullopt, runners_[0]->GetNextScheduledWakeUp());

  TimeTicks start_time = manager_->NowTicks();
  TimeDelta delay1 = TimeDelta::FromMilliseconds(10);
  TimeDelta delay2 = TimeDelta::FromMilliseconds(2);

  runners_[0]->PostDelayedTask(FROM_HERE, BindOnce(&NopTask), delay1);
  EXPECT_EQ(start_time + delay1, runners_[0]->GetNextScheduledWakeUp());

  runners_[0]->PostDelayedTask(FROM_HERE, BindOnce(&NopTask), delay2);
  EXPECT_EQ(start_time + delay2, runners_[0]->GetNextScheduledWakeUp());

  // We don't have wake-ups scheduled for disabled queues.
  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter =
      runners_[0]->CreateQueueEnabledVoter();
  voter->SetQueueEnabled(false);
  EXPECT_EQ(nullopt, runners_[0]->GetNextScheduledWakeUp());

  voter->SetQueueEnabled(true);
  EXPECT_EQ(start_time + delay2, runners_[0]->GetNextScheduledWakeUp());

  // Immediate tasks shouldn't make any difference.
  runners_[0]->PostTask(FROM_HERE, BindOnce(&NopTask));
  EXPECT_EQ(start_time + delay2, runners_[0]->GetNextScheduledWakeUp());

  // Neither should fences.
  runners_[0]->InsertFence(TaskQueue::InsertFencePosition::kBeginningOfTime);
  EXPECT_EQ(start_time + delay2, runners_[0]->GetNextScheduledWakeUp());
}

TEST_P(SequenceManagerTest, SetTimeDomainForDisabledQueue) {
  CreateTaskQueues(1u);

  MockTaskQueueObserver observer;
  runners_[0]->SetObserver(&observer);

  runners_[0]->PostDelayedTask(FROM_HERE, BindOnce(&NopTask),
                               TimeDelta::FromMilliseconds(1));

  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter =
      runners_[0]->CreateQueueEnabledVoter();
  voter->SetQueueEnabled(false);

  // We should not get a notification for a disabled queue.
  EXPECT_CALL(observer, OnQueueNextWakeUpChanged(_, _)).Times(0);

  std::unique_ptr<MockTimeDomain> domain =
      std::make_unique<MockTimeDomain>(manager_->NowTicks());
  manager_->RegisterTimeDomain(domain.get());
  runners_[0]->SetTimeDomain(domain.get());

  // Tidy up.
  runners_[0]->ShutdownTaskQueue();
  manager_->UnregisterTimeDomain(domain.get());
}

namespace {
void SetOnTaskHandlers(scoped_refptr<TestTaskQueue> task_queue,
                       int* start_counter,
                       int* complete_counter) {
  task_queue->GetTaskQueueImpl()->SetOnTaskStartedHandler(BindRepeating(
      [](int* counter, const Task& task,
         const TaskQueue::TaskTiming& task_timing) { ++(*counter); },
      start_counter));
  task_queue->GetTaskQueueImpl()->SetOnTaskCompletedHandler(BindRepeating(
      [](int* counter, const Task& task,
         const TaskQueue::TaskTiming& task_timing) { ++(*counter); },
      complete_counter));
}

void UnsetOnTaskHandlers(scoped_refptr<TestTaskQueue> task_queue) {
  task_queue->GetTaskQueueImpl()->SetOnTaskStartedHandler(
      internal::TaskQueueImpl::OnTaskStartedHandler());
  task_queue->GetTaskQueueImpl()->SetOnTaskCompletedHandler(
      internal::TaskQueueImpl::OnTaskStartedHandler());
}
}  // namespace

TEST_P(SequenceManagerTest, ProcessTasksWithoutTaskTimeObservers) {
  CreateTaskQueues(1u);
  int start_counter = 0;
  int complete_counter = 0;
  std::vector<EnqueueOrder> run_order;
  SetOnTaskHandlers(runners_[0], &start_counter, &complete_counter);
  EXPECT_TRUE(runners_[0]->GetTaskQueueImpl()->RequiresTaskTiming());
  runners_[0]->PostTask(FROM_HERE, BindOnce(&TestTask, 1, &run_order));
  runners_[0]->PostTask(FROM_HERE, BindOnce(&TestTask, 2, &run_order));
  runners_[0]->PostTask(FROM_HERE, BindOnce(&TestTask, 3, &run_order));

  RunLoop().RunUntilIdle();
  EXPECT_EQ(start_counter, 3);
  EXPECT_EQ(complete_counter, 3);
  EXPECT_THAT(run_order, ElementsAre(1u, 2u, 3u));

  UnsetOnTaskHandlers(runners_[0]);
  EXPECT_FALSE(runners_[0]->GetTaskQueueImpl()->RequiresTaskTiming());
  runners_[0]->PostTask(FROM_HERE, BindOnce(&TestTask, 4, &run_order));
  runners_[0]->PostTask(FROM_HERE, BindOnce(&TestTask, 5, &run_order));
  runners_[0]->PostTask(FROM_HERE, BindOnce(&TestTask, 6, &run_order));

  RunLoop().RunUntilIdle();
  EXPECT_EQ(start_counter, 3);
  EXPECT_EQ(complete_counter, 3);
  EXPECT_THAT(run_order, ElementsAre(1u, 2u, 3u, 4u, 5u, 6u));
}

TEST_P(SequenceManagerTest, ProcessTasksWithTaskTimeObservers) {
  CreateTaskQueues(1u);
  int start_counter = 0;
  int complete_counter = 0;

  manager_->AddTaskTimeObserver(&test_task_time_observer_);
  SetOnTaskHandlers(runners_[0], &start_counter, &complete_counter);
  EXPECT_TRUE(runners_[0]->GetTaskQueueImpl()->RequiresTaskTiming());
  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostTask(FROM_HERE, BindOnce(&TestTask, 1, &run_order));
  runners_[0]->PostTask(FROM_HERE, BindOnce(&TestTask, 2, &run_order));

  RunLoop().RunUntilIdle();
  EXPECT_EQ(start_counter, 2);
  EXPECT_EQ(complete_counter, 2);
  EXPECT_THAT(run_order, ElementsAre(1u, 2u));

  UnsetOnTaskHandlers(runners_[0]);
  EXPECT_FALSE(runners_[0]->GetTaskQueueImpl()->RequiresTaskTiming());
  runners_[0]->PostTask(FROM_HERE, BindOnce(&TestTask, 3, &run_order));
  runners_[0]->PostTask(FROM_HERE, BindOnce(&TestTask, 4, &run_order));

  RunLoop().RunUntilIdle();
  EXPECT_EQ(start_counter, 2);
  EXPECT_EQ(complete_counter, 2);
  EXPECT_THAT(run_order, ElementsAre(1u, 2u, 3u, 4u));

  manager_->RemoveTaskTimeObserver(&test_task_time_observer_);
  runners_[0]->PostTask(FROM_HERE, BindOnce(&TestTask, 5, &run_order));
  runners_[0]->PostTask(FROM_HERE, BindOnce(&TestTask, 6, &run_order));

  RunLoop().RunUntilIdle();
  EXPECT_EQ(start_counter, 2);
  EXPECT_EQ(complete_counter, 2);
  EXPECT_FALSE(runners_[0]->GetTaskQueueImpl()->RequiresTaskTiming());
  EXPECT_THAT(run_order, ElementsAre(1u, 2u, 3u, 4u, 5u, 6u));

  SetOnTaskHandlers(runners_[0], &start_counter, &complete_counter);
  runners_[0]->PostTask(FROM_HERE, BindOnce(&TestTask, 7, &run_order));
  runners_[0]->PostTask(FROM_HERE, BindOnce(&TestTask, 8, &run_order));

  RunLoop().RunUntilIdle();
  EXPECT_EQ(start_counter, 4);
  EXPECT_EQ(complete_counter, 4);
  EXPECT_TRUE(runners_[0]->GetTaskQueueImpl()->RequiresTaskTiming());
  EXPECT_THAT(run_order, ElementsAre(1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u));
  UnsetOnTaskHandlers(runners_[0]);
}

TEST_P(SequenceManagerTest, GracefulShutdown) {
  std::vector<TimeTicks> run_times;
  scoped_refptr<TestTaskQueue> main_tq = CreateTaskQueue();
  WeakPtr<TestTaskQueue> main_tq_weak_ptr = main_tq->GetWeakPtr();

  EXPECT_EQ(1u, manager_->ActiveQueuesCount());
  EXPECT_EQ(0u, manager_->QueuesToShutdownCount());
  EXPECT_EQ(0u, manager_->QueuesToDeleteCount());

  for (int i = 1; i <= 5; ++i) {
    main_tq->PostDelayedTask(
        FROM_HERE, BindOnce(&RecordTimeTask, &run_times, GetTickClock()),
        TimeDelta::FromMilliseconds(i * 100));
  }
  test_task_runner_->FastForwardBy(TimeDelta::FromMilliseconds(250));

  main_tq = nullptr;
  // Ensure that task queue went away.
  EXPECT_FALSE(main_tq_weak_ptr.get());

  test_task_runner_->FastForwardBy(TimeDelta::FromMilliseconds(1));

  EXPECT_EQ(1u, manager_->ActiveQueuesCount());
  EXPECT_EQ(1u, manager_->QueuesToShutdownCount());
  EXPECT_EQ(0u, manager_->QueuesToDeleteCount());

  test_task_runner_->FastForwardUntilNoTasksRemain();

  // Even with TaskQueue gone, tasks are executed.
  EXPECT_THAT(run_times,
              ElementsAre(start_time_ + TimeDelta::FromMilliseconds(100),
                          start_time_ + TimeDelta::FromMilliseconds(200),
                          start_time_ + TimeDelta::FromMilliseconds(300),
                          start_time_ + TimeDelta::FromMilliseconds(400),
                          start_time_ + TimeDelta::FromMilliseconds(500)));

  EXPECT_EQ(0u, manager_->ActiveQueuesCount());
  EXPECT_EQ(0u, manager_->QueuesToShutdownCount());
  EXPECT_EQ(0u, manager_->QueuesToDeleteCount());
}

TEST_P(SequenceManagerTest, GracefulShutdown_ManagerDeletedInFlight) {
  std::vector<TimeTicks> run_times;
  scoped_refptr<TestTaskQueue> control_tq = CreateTaskQueue();
  std::vector<scoped_refptr<TestTaskQueue>> main_tqs;
  std::vector<WeakPtr<TestTaskQueue>> main_tq_weak_ptrs;

  // There might be a race condition - async task queues should be unregistered
  // first. Increase the number of task queues to surely detect that.
  // The problem is that pointers are compared in a set and generally for
  // a small number of allocations value of the pointers increases
  // monotonically. 100 is large enough to force allocations from different
  // pages.
  const int N = 100;
  for (int i = 0; i < N; ++i) {
    scoped_refptr<TestTaskQueue> tq = CreateTaskQueue();
    main_tq_weak_ptrs.push_back(tq->GetWeakPtr());
    main_tqs.push_back(std::move(tq));
  }

  for (int i = 1; i <= 5; ++i) {
    main_tqs[0]->PostDelayedTask(
        FROM_HERE, BindOnce(&RecordTimeTask, &run_times, GetTickClock()),
        TimeDelta::FromMilliseconds(i * 100));
  }
  test_task_runner_->FastForwardBy(TimeDelta::FromMilliseconds(250));

  main_tqs.clear();
  // Ensure that task queues went away.
  for (int i = 0; i < N; ++i) {
    EXPECT_FALSE(main_tq_weak_ptrs[i].get());
  }

  // No leaks should occur when TQM was destroyed before processing
  // shutdown task and TaskQueueImpl should be safely deleted on a correct
  // thread.
  manager_.reset();

  test_task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_THAT(run_times,
              ElementsAre(start_time_ + TimeDelta::FromMilliseconds(100),
                          start_time_ + TimeDelta::FromMilliseconds(200)));
}

TEST_P(SequenceManagerTest,
       GracefulShutdown_ManagerDeletedWithQueuesToShutdown) {
  std::vector<TimeTicks> run_times;
  scoped_refptr<TestTaskQueue> main_tq = CreateTaskQueue();
  WeakPtr<TestTaskQueue> main_tq_weak_ptr = main_tq->GetWeakPtr();

  EXPECT_EQ(1u, manager_->ActiveQueuesCount());
  EXPECT_EQ(0u, manager_->QueuesToShutdownCount());
  EXPECT_EQ(0u, manager_->QueuesToDeleteCount());

  for (int i = 1; i <= 5; ++i) {
    main_tq->PostDelayedTask(
        FROM_HERE, BindOnce(&RecordTimeTask, &run_times, GetTickClock()),
        TimeDelta::FromMilliseconds(i * 100));
  }
  test_task_runner_->FastForwardBy(TimeDelta::FromMilliseconds(250));

  main_tq = nullptr;
  // Ensure that task queue went away.
  EXPECT_FALSE(main_tq_weak_ptr.get());

  test_task_runner_->FastForwardBy(TimeDelta::FromMilliseconds(1));

  EXPECT_EQ(1u, manager_->ActiveQueuesCount());
  EXPECT_EQ(1u, manager_->QueuesToShutdownCount());
  EXPECT_EQ(0u, manager_->QueuesToDeleteCount());

  // Ensure that all queues-to-gracefully-shutdown are properly unregistered.
  manager_.reset();

  test_task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_THAT(run_times,
              ElementsAre(start_time_ + TimeDelta::FromMilliseconds(100),
                          start_time_ + TimeDelta::FromMilliseconds(200)));
}

TEST_P(SequenceManagerTestWithCustomInitialization, DefaultTaskRunnerSupport) {
  MessageLoop message_loop;
  scoped_refptr<SingleThreadTaskRunner> original_task_runner =
      message_loop.task_runner();
  scoped_refptr<SingleThreadTaskRunner> custom_task_runner =
      MakeRefCounted<TestSimpleTaskRunner>();
  {
    std::unique_ptr<SequenceManagerForTest> manager =
        SequenceManagerForTest::Create(&message_loop,
                                       message_loop.task_runner(), nullptr);
    manager->SetDefaultTaskRunner(custom_task_runner);
    DCHECK_EQ(custom_task_runner, message_loop.task_runner());
  }
  DCHECK_EQ(original_task_runner, message_loop.task_runner());
}

TEST_P(SequenceManagerTest, CanceledTasksInQueueCantMakeOtherTasksSkipAhead) {
  CreateTaskQueues(2u);

  CancelableTask task1(GetTickClock());
  CancelableTask task2(GetTickClock());
  std::vector<TimeTicks> run_times;

  runners_[0]->PostTask(FROM_HERE,
                        BindOnce(&CancelableTask::RecordTimeTask,
                                 task1.weak_factory_.GetWeakPtr(), &run_times));
  runners_[0]->PostTask(FROM_HERE,
                        BindOnce(&CancelableTask::RecordTimeTask,
                                 task2.weak_factory_.GetWeakPtr(), &run_times));

  std::vector<EnqueueOrder> run_order;
  runners_[1]->PostTask(FROM_HERE, BindOnce(&TestTask, 1, &run_order));

  runners_[0]->PostTask(FROM_HERE, BindOnce(&TestTask, 2, &run_order));

  task1.weak_factory_.InvalidateWeakPtrs();
  task2.weak_factory_.InvalidateWeakPtrs();
  RunLoop().RunUntilIdle();

  EXPECT_THAT(run_order, ElementsAre(1u, 2u));
}

TEST_P(SequenceManagerTest, TaskRunnerDeletedOnAnotherThread) {
  std::vector<TimeTicks> run_times;
  scoped_refptr<TestTaskQueue> main_tq = CreateTaskQueue();
  scoped_refptr<TaskRunner> task_runner =
      main_tq->CreateTaskRunner(kTaskTypeNone);

  int start_counter = 0;
  int complete_counter = 0;
  SetOnTaskHandlers(main_tq, &start_counter, &complete_counter);

  EXPECT_EQ(1u, manager_->ActiveQueuesCount());
  EXPECT_EQ(0u, manager_->QueuesToShutdownCount());
  EXPECT_EQ(0u, manager_->QueuesToDeleteCount());

  for (int i = 1; i <= 5; ++i) {
    task_runner->PostDelayedTask(
        FROM_HERE, BindOnce(&RecordTimeTask, &run_times, GetTickClock()),
        TimeDelta::FromMilliseconds(i * 100));
  }

  // TODO(altimin): do not do this after switching to weak pointer-based
  // task handlers.
  UnsetOnTaskHandlers(main_tq);

  // Make |task_runner| the only reference to |main_tq|.
  main_tq = nullptr;

  WaitableEvent task_queue_deleted(WaitableEvent::ResetPolicy::MANUAL,
                                   WaitableEvent::InitialState::NOT_SIGNALED);
  std::unique_ptr<Thread> thread = std::make_unique<Thread>("test thread");
  thread->StartAndWaitForTesting();

  thread->task_runner()->PostTask(
      FROM_HERE, BindOnce(
                     [](scoped_refptr<TaskRunner> task_runner,
                        WaitableEvent* task_queue_deleted) {
                       task_runner = nullptr;
                       task_queue_deleted->Signal();
                     },
                     std::move(task_runner), &task_queue_deleted));
  task_queue_deleted.Wait();

  EXPECT_EQ(1u, manager_->ActiveQueuesCount());
  EXPECT_EQ(1u, manager_->QueuesToShutdownCount());
  EXPECT_EQ(0u, manager_->QueuesToDeleteCount());

  test_task_runner_->FastForwardUntilNoTasksRemain();

  // Even with TaskQueue gone, tasks are executed.
  EXPECT_THAT(run_times,
              ElementsAre(start_time_ + TimeDelta::FromMilliseconds(100),
                          start_time_ + TimeDelta::FromMilliseconds(200),
                          start_time_ + TimeDelta::FromMilliseconds(300),
                          start_time_ + TimeDelta::FromMilliseconds(400),
                          start_time_ + TimeDelta::FromMilliseconds(500)));

  EXPECT_EQ(0u, manager_->ActiveQueuesCount());
  EXPECT_EQ(0u, manager_->QueuesToShutdownCount());
  EXPECT_EQ(0u, manager_->QueuesToDeleteCount());

  thread->Stop();
}

namespace {

class RunOnDestructionHelper {
 public:
  explicit RunOnDestructionHelper(base::OnceClosure task)
      : task_(std::move(task)) {}

  ~RunOnDestructionHelper() { std::move(task_).Run(); }

 private:
  base::OnceClosure task_;
};

base::OnceClosure RunOnDestruction(base::OnceClosure task) {
  return base::BindOnce(
      [](std::unique_ptr<RunOnDestructionHelper>) {},
      base::Passed(std::make_unique<RunOnDestructionHelper>(std::move(task))));
}

base::OnceClosure PostOnDestructon(scoped_refptr<TestTaskQueue> task_queue,
                                   base::OnceClosure task) {
  return RunOnDestruction(base::BindOnce(
      [](base::OnceClosure task, scoped_refptr<TestTaskQueue> task_queue) {
        task_queue->PostTask(FROM_HERE, std::move(task));
      },
      base::Passed(std::move(task)), task_queue));
}

}  // namespace

TEST_P(SequenceManagerTest, TaskQueueUsedInTaskDestructorAfterShutdown) {
  // This test checks that when a task is posted to a shutdown queue and
  // destroyed, it can try to post a task to the same queue without deadlocks.
  scoped_refptr<TestTaskQueue> main_tq = CreateTaskQueue();

  WaitableEvent test_executed(WaitableEvent::ResetPolicy::MANUAL,
                              WaitableEvent::InitialState::NOT_SIGNALED);
  std::unique_ptr<Thread> thread = std::make_unique<Thread>("test thread");
  thread->StartAndWaitForTesting();

  manager_.reset();

  thread->task_runner()->PostTask(
      FROM_HERE, BindOnce(
                     [](scoped_refptr<TestTaskQueue> task_queue,
                        WaitableEvent* test_executed) {
                       task_queue->PostTask(
                           FROM_HERE, PostOnDestructon(
                                          task_queue, base::BindOnce([]() {})));
                       test_executed->Signal();
                     },
                     main_tq, &test_executed));
  test_executed.Wait();
}

TEST_P(SequenceManagerTest, TaskQueueTaskRunnerDetach) {
  scoped_refptr<TestTaskQueue> queue1 = CreateTaskQueue();
  EXPECT_TRUE(queue1->task_runner()->PostTask(FROM_HERE, BindOnce(&NopTask)));
  queue1->ShutdownTaskQueue();
  EXPECT_FALSE(queue1->task_runner()->PostTask(FROM_HERE, BindOnce(&NopTask)));

  // Create without a sequence manager.
  std::unique_ptr<TimeDomain> time_domain =
      std::make_unique<internal::RealTimeDomain>();
  std::unique_ptr<TaskQueueImpl> queue2 = std::make_unique<TaskQueueImpl>(
      nullptr, time_domain.get(), TaskQueue::Spec("stub"));
  scoped_refptr<SingleThreadTaskRunner> task_runner2 =
      queue2->CreateTaskRunner(0);
  EXPECT_FALSE(task_runner2->PostTask(FROM_HERE, BindOnce(&NopTask)));
}

TEST_P(SequenceManagerTest, DestructorPostChainDuringShutdown) {
  // Checks that a chain of closures which post other closures on destruction do
  // thing on shutdown.
  scoped_refptr<TestTaskQueue> task_queue = CreateTaskQueue();
  bool run = false;
  task_queue->PostTask(
      FROM_HERE,
      PostOnDestructon(
          task_queue,
          PostOnDestructon(task_queue,
                           RunOnDestruction(base::BindOnce(
                               [](bool* run) { *run = true; }, &run)))));

  manager_.reset();

  EXPECT_TRUE(run);
}

class ThreadForOffThreadInitializationTest : public Thread {
 public:
  ThreadForOffThreadInitializationTest()
      : base::Thread("ThreadForOffThreadInitializationTest") {}

  void SequenceManagerCreated(
      base::sequence_manager::SequenceManager* sequence_manager) {
    // This executes on the creating thread.
    DCHECK_CALLED_ON_VALID_SEQUENCE(creating_sequence_checker_);

    queue_ = sequence_manager->CreateTaskQueue<TestTaskQueue>(
        TaskQueue::Spec("default"));

    // TaskQueue should not run tasks on the creating thread.
    EXPECT_FALSE(queue_->RunsTasksInCurrentSequence());

    // Override the default task runner before the thread is started.
    sequence_manager->SetDefaultTaskRunner(queue_->task_runner());
    EXPECT_EQ(queue_->task_runner(), message_loop()->task_runner());

    // Post a task to the queue.
    message_loop()->task_runner()->PostTask(
        FROM_HERE,
        Bind([](bool* did_run_task) { *did_run_task = true; }, &did_run_task_));
  }

 private:
  void Init() override {
    // Queue should already be bound to this thread.
    EXPECT_TRUE(queue_->RunsTasksInCurrentSequence());
    EXPECT_EQ(queue_->task_runner(), ThreadTaskRunnerHandle::Get());
  }

  void Run(base::RunLoop* run_loop) override {
    // Run the posted task.
    Thread::Run(run_loop);
    EXPECT_TRUE(did_run_task_);

    // The |queue_| should be destructed on the creating thread.
    queue_ = nullptr;
  }

  scoped_refptr<SingleThreadTaskRunner> original_task_runner_;
  scoped_refptr<TestTaskQueue> queue_;
  bool did_run_task_ = false;

  SEQUENCE_CHECKER(creating_sequence_checker_);
};

// Verifies the integration of off-thread SequenceManager and MessageLoop
// initialization when starting a base::Thread.
TEST_P(SequenceManagerTestWithCustomInitialization, OffThreadInitialization) {
  ThreadForOffThreadInitializationTest thread;

  base::Thread::Options options;
  options.message_loop_type = base::MessageLoop::TYPE_DEFAULT;
  options.on_sequence_manager_created = base::BindRepeating(
      &ThreadForOffThreadInitializationTest::SequenceManagerCreated,
      base::Unretained(&thread));
  ASSERT_TRUE(thread.StartWithOptions(options));

  // Waits for the thread to complete execution.
  thread.Stop();
}

TEST_P(SequenceManagerTestWithCustomInitialization,
       SequenceManagerCreatedBeforeMessageLoop) {
  std::unique_ptr<SequenceManager> manager =
      CreateUnboundSequenceManager(nullptr);
  manager->BindToCurrentThread();
  scoped_refptr<TaskQueue> default_task_queue =
      manager->CreateTaskQueue<TestTaskQueue>(TaskQueue::Spec("default"));
  EXPECT_THAT(default_task_queue.get(), testing::NotNull());

  std::unique_ptr<MessageLoop> message_loop(new MessageLoop());
  manager->BindToMessageLoop(message_loop.get());

  // Check that task posting works.
  std::vector<EnqueueOrder> run_order;
  default_task_queue->task_runner()->PostTask(
      FROM_HERE, BindOnce(&TestTask, 1, &run_order));
  default_task_queue->task_runner()->PostTask(
      FROM_HERE, BindOnce(&TestTask, 2, &run_order));
  default_task_queue->task_runner()->PostTask(
      FROM_HERE, BindOnce(&TestTask, 3, &run_order));
  RunLoop().RunUntilIdle();

  EXPECT_THAT(run_order, ElementsAre(1u, 2u, 3u));

  // We must release the SequenceManager before the MessageLoop because
  // SequenceManager assumes the MessageLoop outlives it.
  manager.reset();
}

TEST_P(SequenceManagerTestWithCustomInitialization,
       CreateUnboundSequenceManagerWhichIsNeverBound) {
  // This should not crash.
  CreateUnboundSequenceManager(nullptr);
}

TEST_P(SequenceManagerTest, HasPendingHighResolutionTasks) {
  CreateTaskQueues(1u);
  bool supports_high_res = false;
#if defined(OS_WIN)
  supports_high_res = true;
#endif

  // Only the third task needs high resolution timing.
  EXPECT_FALSE(manager_->HasPendingHighResolutionTasks());
  runners_[0]->PostTask(FROM_HERE, BindOnce(&NopTask));
  EXPECT_FALSE(manager_->HasPendingHighResolutionTasks());
  runners_[0]->PostDelayedTask(FROM_HERE, BindOnce(&NopTask),
                               TimeDelta::FromMilliseconds(100));
  EXPECT_FALSE(manager_->HasPendingHighResolutionTasks());
  runners_[0]->PostDelayedTask(FROM_HERE, BindOnce(&NopTask),
                               TimeDelta::FromMilliseconds(10));
  EXPECT_EQ(manager_->HasPendingHighResolutionTasks(), supports_high_res);

  // Running immediate tasks doesn't affect pending high resolution tasks.
  RunLoop().RunUntilIdle();
  EXPECT_EQ(manager_->HasPendingHighResolutionTasks(), supports_high_res);

  // Advancing to just before a pending low resolution task doesn't mean that we
  // have pending high resolution work.
  test_task_runner_->AdvanceMockTickClock(TimeDelta::FromMilliseconds(99));
  RunLoop().RunUntilIdle();
  EXPECT_FALSE(manager_->HasPendingHighResolutionTasks());

  test_task_runner_->AdvanceMockTickClock(TimeDelta::FromMilliseconds(100));
  RunLoop().RunUntilIdle();
  EXPECT_FALSE(manager_->HasPendingHighResolutionTasks());
}

}  // namespace sequence_manager_impl_unittest
}  // namespace internal
}  // namespace sequence_manager
}  // namespace base
