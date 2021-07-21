// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/sequence_manager/sequence_manager_impl.h"

#include <stddef.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/cancelable_callback.h"
#include "base/location.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/message_loop/message_pump_default.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/current_thread.h"
#include "base/task/sequence_manager/real_time_domain.h"
#include "base/task/sequence_manager/sequence_manager.h"
#include "base/task/sequence_manager/task_queue.h"
#include "base/task/sequence_manager/task_queue_impl.h"
#include "base/task/sequence_manager/task_queue_selector.h"
#include "base/task/sequence_manager/tasks.h"
#include "base/task/sequence_manager/test/mock_time_domain.h"
#include "base/task/sequence_manager/test/mock_time_message_pump.h"
#include "base/task/sequence_manager/test/sequence_manager_for_test.h"
#include "base/task/sequence_manager/test/test_task_queue.h"
#include "base/task/sequence_manager/test/test_task_time_observer.h"
#include "base/task/sequence_manager/thread_controller_with_message_pump_impl.h"
#include "base/task/sequence_manager/work_queue.h"
#include "base/task/sequence_manager/work_queue_sets.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/null_task_runner.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/task_environment.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/trace_event/base_tracing.h"
#include "base/tracing_buildflags.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"

#if BUILDFLAG(ENABLE_BASE_TRACING)
#include "base/test/trace_event_analyzer.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#endif  // BUILDFLAG(ENABLE_BASE_TRACING)

using base::sequence_manager::EnqueueOrder;
using testing::_;
using testing::AnyNumber;
using testing::Contains;
using testing::ElementsAre;
using testing::ElementsAreArray;
using testing::HasSubstr;
using testing::Mock;
using testing::Not;
using testing::Return;
using testing::StrictMock;
using testing::UnorderedElementsAre;

namespace base {
namespace sequence_manager {
namespace internal {

namespace {

enum class TestType {
  kMockTaskRunner,
  kMessagePump,
};

std::string ToString(TestType type) {
  switch (type) {
    case TestType::kMockTaskRunner:
      return "kMockTaskRunner";
    case TestType::kMessagePump:
      return "kMessagePump";
  }
}

std::string GetTestNameSuffix(const testing::TestParamInfo<TestType>& info) {
  return StrCat({"With", ToString(info.param).substr(1)});
}

void PrintTo(const TestType type, std::ostream* os) {
  *os << ToString(type);
}

using MockTask = MockCallback<base::RepeatingCallback<void()>>;

// This class abstracts the details of how the SequenceManager runs tasks.
// Subclasses will use a MockTaskRunner, a MessageLoop or a MockMessagePump. We
// can then have common tests for all the scenarios by just using this
// interface.
class Fixture {
 public:
  virtual ~Fixture() = default;
  virtual void AdvanceMockTickClock(TimeDelta delta) = 0;
  virtual const TickClock* mock_tick_clock() const = 0;
  virtual TimeDelta NextPendingTaskDelay() const = 0;
  // Keeps advancing time as needed to run tasks up to the specified limit.
  virtual void FastForwardBy(TimeDelta delta) = 0;
  // Keeps advancing time as needed to run tasks until no more tasks are
  // available.
  virtual void FastForwardUntilNoTasksRemain() = 0;
  virtual void RunDoWorkOnce() = 0;
  virtual SequenceManagerForTest* sequence_manager() const = 0;
  virtual void DestroySequenceManager() = 0;
  virtual int GetNowTicksCallCount() = 0;
};

class CallCountingTickClock : public TickClock {
 public:
  explicit CallCountingTickClock(RepeatingCallback<TimeTicks()> now_callback)
      : now_callback_(std::move(now_callback)) {}
  explicit CallCountingTickClock(TickClock* clock)
      : CallCountingTickClock(
            BindLambdaForTesting([clock]() { return clock->NowTicks(); })) {}

  ~CallCountingTickClock() override = default;

  TimeTicks NowTicks() const override {
    ++now_call_count_;
    return now_callback_.Run();
  }

  void Reset() { now_call_count_.store(0); }

  int now_call_count() const { return now_call_count_; }

 private:
  const RepeatingCallback<TimeTicks()> now_callback_;
  mutable std::atomic<int> now_call_count_{0};
};

class FixtureWithMockTaskRunner final : public Fixture {
 public:
  FixtureWithMockTaskRunner()
      : test_task_runner_(MakeRefCounted<TestMockTimeTaskRunner>(
            TestMockTimeTaskRunner::Type::kBoundToThread)),
        call_counting_clock_(BindRepeating(&TestMockTimeTaskRunner::NowTicks,
                                           test_task_runner_)),
        sequence_manager_(SequenceManagerForTest::Create(
            nullptr,
            ThreadTaskRunnerHandle::Get(),
            mock_tick_clock(),
            SequenceManager::Settings::Builder()
                .SetMessagePumpType(MessagePumpType::DEFAULT)
                .SetRandomisedSamplingEnabled(false)
                .SetTickClock(mock_tick_clock())
                .Build())) {
    // A null clock triggers some assertions.
    AdvanceMockTickClock(TimeDelta::FromMilliseconds(1));

    // The SequenceManager constructor calls Now() once for setting up
    // housekeeping.
    EXPECT_EQ(1, GetNowTicksCallCount());
    call_counting_clock_.Reset();
  }

  void AdvanceMockTickClock(TimeDelta delta) override {
    test_task_runner_->AdvanceMockTickClock(delta);
  }

  const TickClock* mock_tick_clock() const override {
    return &call_counting_clock_;
  }

  TimeDelta NextPendingTaskDelay() const override {
    return test_task_runner_->NextPendingTaskDelay();
  }

  void FastForwardBy(TimeDelta delta) override {
    test_task_runner_->FastForwardBy(delta);
  }

  void FastForwardUntilNoTasksRemain() override {
    test_task_runner_->FastForwardUntilNoTasksRemain();
  }

  void RunDoWorkOnce() override {
    EXPECT_EQ(test_task_runner_->GetPendingTaskCount(), 1u);
    // We should only run tasks already posted by that moment.
    RunLoop run_loop;
    test_task_runner_->PostTask(FROM_HERE, run_loop.QuitClosure());
    // TestMockTimeTaskRunner will fast-forward mock clock if necessary.
    run_loop.Run();
  }

  scoped_refptr<TestMockTimeTaskRunner> test_task_runner() const {
    return test_task_runner_;
  }

  SequenceManagerForTest* sequence_manager() const override {
    return sequence_manager_.get();
  }

  void DestroySequenceManager() override { sequence_manager_.reset(); }

  int GetNowTicksCallCount() override {
    return call_counting_clock_.now_call_count();
  }

 private:
  scoped_refptr<TestMockTimeTaskRunner> test_task_runner_;
  CallCountingTickClock call_counting_clock_;
  std::unique_ptr<SequenceManagerForTest> sequence_manager_;
};

class FixtureWithMockMessagePump : public Fixture {
 public:
  FixtureWithMockMessagePump() : call_counting_clock_(&mock_clock_) {
    // A null clock triggers some assertions.
    mock_clock_.Advance(TimeDelta::FromMilliseconds(1));

    auto pump = std::make_unique<MockTimeMessagePump>(&mock_clock_);
    pump_ = pump.get();
    auto settings = SequenceManager::Settings::Builder()
                        .SetMessagePumpType(MessagePumpType::DEFAULT)
                        .SetRandomisedSamplingEnabled(false)
                        .SetTickClock(mock_tick_clock())
                        .Build();
    auto thread_controller =
        std::make_unique<ThreadControllerWithMessagePumpImpl>(std::move(pump),
                                                              settings);
    sequence_manager_ = SequenceManagerForTest::Create(
        std::move(thread_controller), std::move(settings));
    sequence_manager_->SetDefaultTaskRunner(MakeRefCounted<NullTaskRunner>());

    // The SequenceManager constructor calls Now() once for setting up
    // housekeeping.
    EXPECT_EQ(1, GetNowTicksCallCount());
    call_counting_clock_.Reset();
  }

  void AdvanceMockTickClock(TimeDelta delta) override {
    mock_clock_.Advance(delta);
  }

  const TickClock* mock_tick_clock() const override {
    return &call_counting_clock_;
  }

  TimeDelta NextPendingTaskDelay() const override {
    return pump_->next_wake_up_time() - mock_tick_clock()->NowTicks();
  }

  void FastForwardBy(TimeDelta delta) override {
    pump_->SetAllowTimeToAutoAdvanceUntil(mock_tick_clock()->NowTicks() +
                                          delta);
    pump_->SetStopWhenMessagePumpIsIdle(true);
    RunLoop().Run();
    pump_->SetStopWhenMessagePumpIsIdle(false);
  }

  void FastForwardUntilNoTasksRemain() override {
    pump_->SetAllowTimeToAutoAdvanceUntil(TimeTicks::Max());
    pump_->SetStopWhenMessagePumpIsIdle(true);
    RunLoop().Run();
    pump_->SetStopWhenMessagePumpIsIdle(false);
    pump_->SetAllowTimeToAutoAdvanceUntil(mock_tick_clock()->NowTicks());
  }

  void RunDoWorkOnce() override {
    pump_->SetQuitAfterDoWork(true);
    RunLoop().Run();
    pump_->SetQuitAfterDoWork(false);
  }

  SequenceManagerForTest* sequence_manager() const override {
    return sequence_manager_.get();
  }

  void DestroySequenceManager() override {
    pump_ = nullptr;
    sequence_manager_.reset();
  }

  int GetNowTicksCallCount() override {
    return call_counting_clock_.now_call_count();
  }

 private:
  MockTimeMessagePump* pump_ = nullptr;
  SimpleTestTickClock mock_clock_;
  CallCountingTickClock call_counting_clock_;
  std::unique_ptr<SequenceManagerForTest> sequence_manager_;
};

// Convenience wrapper around the fixtures so that we can use parametrized tests
// instead of templated ones. The latter would be more verbose as all method
// calls to the fixture would need to be like this->method()
class SequenceManagerTest : public testing::TestWithParam<TestType>,
                            public Fixture {
 public:
  SequenceManagerTest() {
    switch (GetUnderlyingRunnerType()) {
      case TestType::kMockTaskRunner:
        fixture_ = std::make_unique<FixtureWithMockTaskRunner>();
        break;
      case TestType::kMessagePump:
        fixture_ = std::make_unique<FixtureWithMockMessagePump>();
        break;
      default:
        NOTREACHED();
    }
  }

  scoped_refptr<TestTaskQueue> CreateTaskQueue(
      TaskQueue::Spec spec = TaskQueue::Spec("test")) {
    return sequence_manager()->CreateTaskQueueWithType<TestTaskQueue>(spec);
  }

  std::vector<scoped_refptr<TestTaskQueue>> CreateTaskQueues(
      size_t num_queues) {
    std::vector<scoped_refptr<TestTaskQueue>> queues;
    for (size_t i = 0; i < num_queues; i++)
      queues.push_back(CreateTaskQueue());
    return queues;
  }

  void RunUntilManagerIsIdle(RepeatingClosure per_run_time_callback) {
    for (;;) {
      // Advance time if we've run out of immediate work to do.
      if (!sequence_manager()->HasImmediateWork()) {
        LazyNow lazy_now(mock_tick_clock());
        absl::optional<TimeDelta> delay =
            sequence_manager()->GetRealTimeDomain()->DelayTillNextTask(
                &lazy_now);
        if (delay) {
          AdvanceMockTickClock(*delay);
          per_run_time_callback.Run();
        } else {
          break;
        }
      }
      RunLoop().RunUntilIdle();
    }
  }

  void AdvanceMockTickClock(TimeDelta delta) override {
    fixture_->AdvanceMockTickClock(delta);
  }

  const TickClock* mock_tick_clock() const override {
    return fixture_->mock_tick_clock();
  }

  TimeDelta NextPendingTaskDelay() const override {
    return fixture_->NextPendingTaskDelay();
  }

  void FastForwardBy(TimeDelta delta) override {
    fixture_->FastForwardBy(delta);
  }

  void FastForwardUntilNoTasksRemain() override {
    fixture_->FastForwardUntilNoTasksRemain();
  }

  void RunDoWorkOnce() override { fixture_->RunDoWorkOnce(); }

  SequenceManagerForTest* sequence_manager() const override {
    return fixture_->sequence_manager();
  }

  void DestroySequenceManager() override { fixture_->DestroySequenceManager(); }

  int GetNowTicksCallCount() override {
    return fixture_->GetNowTicksCallCount();
  }

  TestType GetUnderlyingRunnerType() { return GetParam(); }

 private:
  std::unique_ptr<Fixture> fixture_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         SequenceManagerTest,
                         testing::Values(TestType::kMockTaskRunner,
                                         TestType::kMessagePump),
                         GetTestNameSuffix);

void PostFromNestedRunloop(scoped_refptr<TestTaskQueue> runner,
                           std::vector<std::pair<OnceClosure, bool>>* tasks) {
  for (std::pair<OnceClosure, bool>& pair : *tasks) {
    if (pair.second) {
      runner->task_runner()->PostTask(FROM_HERE, std::move(pair.first));
    } else {
      runner->task_runner()->PostNonNestableTask(FROM_HERE,
                                                 std::move(pair.first));
    }
  }
  RunLoop(RunLoop::Type::kNestableTasksAllowed).RunUntilIdle();
}

void NopTask() {}

class TestCountUsesTimeSource : public TickClock {
 public:
  TestCountUsesTimeSource() = default;
  TestCountUsesTimeSource(const TestCountUsesTimeSource&) = delete;
  TestCountUsesTimeSource& operator=(const TestCountUsesTimeSource&) = delete;
  ~TestCountUsesTimeSource() override = default;

  TimeTicks NowTicks() const override {
    now_calls_count_++;
    // Don't return 0, as it triggers some assertions.
    return TimeTicks() + TimeDelta::FromSeconds(1);
  }

  int now_calls_count() const { return now_calls_count_; }

 private:
  mutable int now_calls_count_ = 0;
};

class QueueTimeTaskObserver : public TaskObserver {
 public:
  void WillProcessTask(const PendingTask& pending_task,
                       bool was_blocked_or_low_priority) override {
    queue_times_.push_back(pending_task.queue_time);
  }
  void DidProcessTask(const PendingTask& pending_task) override {}
  std::vector<TimeTicks> queue_times() const { return queue_times_; }

 private:
  std::vector<TimeTicks> queue_times_;
};

}  // namespace

TEST_P(SequenceManagerTest, GetCorrectTaskRunnerForCurrentTask) {
  auto queue = CreateTaskQueue();

  queue->task_runner()->PostTask(
      FROM_HERE, BindLambdaForTesting([&]() {
        EXPECT_EQ(queue->task_runner(),
                  sequence_manager()->GetTaskRunnerForCurrentTask());
      }));

  RunLoop().RunUntilIdle();
}

TEST_P(SequenceManagerTest, NowNotCalledIfUnneeded) {
  sequence_manager()->SetWorkBatchSize(6);

  auto queues = CreateTaskQueues(3u);

  queues[0]->task_runner()->PostTask(FROM_HERE, BindOnce(&NopTask));
  queues[0]->task_runner()->PostTask(FROM_HERE, BindOnce(&NopTask));
  queues[1]->task_runner()->PostTask(FROM_HERE, BindOnce(&NopTask));
  queues[1]->task_runner()->PostTask(FROM_HERE, BindOnce(&NopTask));
  queues[2]->task_runner()->PostTask(FROM_HERE, BindOnce(&NopTask));
  queues[2]->task_runner()->PostTask(FROM_HERE, BindOnce(&NopTask));

  RunLoop().RunUntilIdle();

  EXPECT_EQ(0, GetNowTicksCallCount());
}

TEST_P(SequenceManagerTest,
       NowCalledMinimumNumberOfTimesToComputeTaskDurations) {
  TestTaskTimeObserver time_observer;
  sequence_manager()->SetWorkBatchSize(6);
  sequence_manager()->AddTaskTimeObserver(&time_observer);

  auto queues = CreateTaskQueues(3u);

  queues[0]->task_runner()->PostTask(FROM_HERE, BindOnce(&NopTask));
  queues[0]->task_runner()->PostTask(FROM_HERE, BindOnce(&NopTask));
  queues[1]->task_runner()->PostTask(FROM_HERE, BindOnce(&NopTask));
  queues[1]->task_runner()->PostTask(FROM_HERE, BindOnce(&NopTask));
  queues[2]->task_runner()->PostTask(FROM_HERE, BindOnce(&NopTask));
  queues[2]->task_runner()->PostTask(FROM_HERE, BindOnce(&NopTask));

  RunLoop().RunUntilIdle();
  // Now is called when each task starts running and when its completed.
  // 6 * 2 = 12 calls.
  EXPECT_EQ(12, GetNowTicksCallCount());
}

TEST_P(SequenceManagerTest,
       NowCalledMinimumNumberOfTimesToComputeTaskDurationsDelayedFenceAllowed) {
  TestTaskTimeObserver time_observer;
  sequence_manager()->SetWorkBatchSize(6);
  sequence_manager()->AddTaskTimeObserver(&time_observer);

  std::vector<scoped_refptr<TestTaskQueue>> queues;
  for (size_t i = 0; i < 3; i++) {
    queues.push_back(
        CreateTaskQueue(TaskQueue::Spec("test").SetDelayedFencesAllowed(true)));
  }

  queues[0]->task_runner()->PostTask(FROM_HERE, BindOnce(&NopTask));
  queues[0]->task_runner()->PostTask(FROM_HERE, BindOnce(&NopTask));
  queues[1]->task_runner()->PostTask(FROM_HERE, BindOnce(&NopTask));
  queues[1]->task_runner()->PostTask(FROM_HERE, BindOnce(&NopTask));
  queues[2]->task_runner()->PostTask(FROM_HERE, BindOnce(&NopTask));
  queues[2]->task_runner()->PostTask(FROM_HERE, BindOnce(&NopTask));

  RunLoop().RunUntilIdle();
  // Now is called each time a task is queued, when first task is started
  // running, and when a task is completed. 6 * 3 = 18 calls.
  EXPECT_EQ(18, GetNowTicksCallCount());
}

void NullTask() {}

void TestTask(uint64_t value, std::vector<EnqueueOrder>* out_result) {
  out_result->push_back(EnqueueOrder::FromIntForTesting(value));
}

void DisableQueueTestTask(uint64_t value,
                          std::vector<EnqueueOrder>* out_result,
                          TaskQueue::QueueEnabledVoter* voter) {
  out_result->push_back(EnqueueOrder::FromIntForTesting(value));
  voter->SetVoteToEnable(false);
}

TEST_P(SequenceManagerTest, SingleQueuePosting) {
  auto queue = CreateTaskQueue();

  std::vector<EnqueueOrder> run_order;
  queue->task_runner()->PostTask(FROM_HERE, BindOnce(&TestTask, 1, &run_order));
  queue->task_runner()->PostTask(FROM_HERE, BindOnce(&TestTask, 2, &run_order));
  queue->task_runner()->PostTask(FROM_HERE, BindOnce(&TestTask, 3, &run_order));

  RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1u, 2u, 3u));
}

TEST_P(SequenceManagerTest, MultiQueuePosting) {
  auto queues = CreateTaskQueues(3u);

  std::vector<EnqueueOrder> run_order;
  queues[0]->task_runner()->PostTask(FROM_HERE,
                                     BindOnce(&TestTask, 1, &run_order));
  queues[0]->task_runner()->PostTask(FROM_HERE,
                                     BindOnce(&TestTask, 2, &run_order));
  queues[1]->task_runner()->PostTask(FROM_HERE,
                                     BindOnce(&TestTask, 3, &run_order));
  queues[1]->task_runner()->PostTask(FROM_HERE,
                                     BindOnce(&TestTask, 4, &run_order));
  queues[2]->task_runner()->PostTask(FROM_HERE,
                                     BindOnce(&TestTask, 5, &run_order));
  queues[2]->task_runner()->PostTask(FROM_HERE,
                                     BindOnce(&TestTask, 6, &run_order));

  RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1u, 2u, 3u, 4u, 5u, 6u));
}

TEST_P(SequenceManagerTest, NonNestableTaskPosting) {
  auto queue = CreateTaskQueue();

  std::vector<EnqueueOrder> run_order;
  queue->task_runner()->PostNonNestableTask(FROM_HERE,
                                            BindOnce(&TestTask, 1, &run_order));

  RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1u));
}

TEST_P(SequenceManagerTest, NonNestableTaskExecutesInExpectedOrder) {
  auto queue = CreateTaskQueue();

  std::vector<EnqueueOrder> run_order;
  queue->task_runner()->PostTask(FROM_HERE, BindOnce(&TestTask, 1, &run_order));
  queue->task_runner()->PostTask(FROM_HERE, BindOnce(&TestTask, 2, &run_order));
  queue->task_runner()->PostTask(FROM_HERE, BindOnce(&TestTask, 3, &run_order));
  queue->task_runner()->PostTask(FROM_HERE, BindOnce(&TestTask, 4, &run_order));
  queue->task_runner()->PostNonNestableTask(FROM_HERE,
                                            BindOnce(&TestTask, 5, &run_order));

  RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1u, 2u, 3u, 4u, 5u));
}

TEST_P(SequenceManagerTest, NonNestableTasksDoesntExecuteInNestedLoop) {
  // TestMockTimeTaskRunner doesn't support nested loops.
  if (GetUnderlyingRunnerType() == TestType::kMockTaskRunner)
    return;
  auto queue = CreateTaskQueue();

  std::vector<EnqueueOrder> run_order;
  queue->task_runner()->PostTask(FROM_HERE, BindOnce(&TestTask, 1, &run_order));
  queue->task_runner()->PostTask(FROM_HERE, BindOnce(&TestTask, 2, &run_order));

  std::vector<std::pair<OnceClosure, bool>> tasks_to_post_from_nested_loop;
  tasks_to_post_from_nested_loop.push_back(
      std::make_pair(BindOnce(&TestTask, 3, &run_order), false));
  tasks_to_post_from_nested_loop.push_back(
      std::make_pair(BindOnce(&TestTask, 4, &run_order), false));
  tasks_to_post_from_nested_loop.push_back(
      std::make_pair(BindOnce(&TestTask, 5, &run_order), true));
  tasks_to_post_from_nested_loop.push_back(
      std::make_pair(BindOnce(&TestTask, 6, &run_order), true));

  queue->task_runner()->PostTask(
      FROM_HERE, BindOnce(&PostFromNestedRunloop, queue,
                          Unretained(&tasks_to_post_from_nested_loop)));

  RunLoop().RunUntilIdle();
  // Note we expect tasks 3 & 4 to run last because they're non-nestable.
  EXPECT_THAT(run_order, ElementsAre(1u, 2u, 5u, 6u, 3u, 4u));
}

TEST_P(SequenceManagerTest, NonNestableTaskQueueTimeShiftsToEndOfNestedLoop) {
  // TestMockTimeTaskRunner doesn't support nested loops.
  if (GetUnderlyingRunnerType() == TestType::kMockTaskRunner)
    return;

  auto queue = CreateTaskQueue();

  QueueTimeTaskObserver observer;
  sequence_manager()->AddTaskObserver(&observer);
  sequence_manager()->SetAddQueueTimeToTasks(true);

  RunLoop nested_run_loop(RunLoop::Type::kNestableTasksAllowed);

  const TimeTicks start_time = mock_tick_clock()->NowTicks();

  constexpr auto kTimeSpentInNestedLoop = TimeDelta::FromSeconds(1);
  constexpr auto kTimeInTaskAfterNestedLoop = TimeDelta::FromSeconds(3);

  // 1) Run task 1
  // 2) Enter a nested loop
  // 3) Run task 3
  // 4) Advance time by 1 second
  // 5) Run task 5
  // 6) Exit nested loop
  // 7) Run task 7 (non-nestable)
  // 8) Advance time by 3 seconds (non-nestable)
  // 9) Run task 9 (non-nestable)
  // Steps 7-9 are expected to run last and have had their queue time adjusted
  // to 6 (task 8 shouldn't affect task 9's queue time).
  std::vector<EnqueueOrder> run_order;
  queue->task_runner()->PostTask(FROM_HERE, BindOnce(&TestTask, 1, &run_order));
  queue->task_runner()->PostTask(FROM_HERE, BindLambdaForTesting([&]() {
                                   TestTask(2, &run_order);
                                   nested_run_loop.Run();
                                 }));
  queue->task_runner()->PostTask(FROM_HERE, BindOnce(&TestTask, 3, &run_order));
  queue->task_runner()->PostNonNestableTask(FROM_HERE,
                                            BindOnce(&TestTask, 7, &run_order));
  queue->task_runner()->PostTask(FROM_HERE, BindLambdaForTesting([&]() {
                                   TestTask(4, &run_order);
                                   AdvanceMockTickClock(kTimeSpentInNestedLoop);
                                 }));
  queue->task_runner()->PostNonNestableTask(
      FROM_HERE, BindLambdaForTesting([&]() {
        TestTask(8, &run_order);
        AdvanceMockTickClock(kTimeInTaskAfterNestedLoop);
      }));
  queue->task_runner()->PostTask(FROM_HERE, BindOnce(&TestTask, 5, &run_order));
  queue->task_runner()->PostNonNestableTask(FROM_HERE,
                                            BindOnce(&TestTask, 9, &run_order));
  queue->task_runner()->PostTask(FROM_HERE, BindLambdaForTesting([&]() {
                                   TestTask(6, &run_order);
                                   nested_run_loop.Quit();
                                 }));

  RunLoop().RunUntilIdle();

  EXPECT_THAT(run_order, ElementsAre(1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u));

  const TimeTicks expected_adjusted_queueing_time =
      start_time + kTimeSpentInNestedLoop;
  EXPECT_THAT(
      observer.queue_times(),
      ElementsAre(start_time, start_time, start_time, start_time, start_time,
                  start_time, expected_adjusted_queueing_time,
                  expected_adjusted_queueing_time,
                  expected_adjusted_queueing_time));

  sequence_manager()->RemoveTaskObserver(&observer);
}

namespace {

void InsertFenceAndPostTestTask(int id,
                                std::vector<EnqueueOrder>* run_order,
                                scoped_refptr<TestTaskQueue> task_queue,
                                SequenceManagerForTest* manager) {
  run_order->push_back(EnqueueOrder::FromIntForTesting(id));
  task_queue->InsertFence(TaskQueue::InsertFencePosition::kNow);
  task_queue->task_runner()->PostTask(FROM_HERE,
                                      BindOnce(&TestTask, id + 1, run_order));

  // Force reload of immediate work queue. In real life the same effect can be
  // achieved with cross-thread posting.
  manager->ReloadEmptyWorkQueues();
}

}  // namespace

TEST_P(SequenceManagerTest, TaskQueueDisabledFromNestedLoop) {
  if (GetUnderlyingRunnerType() == TestType::kMockTaskRunner)
    return;
  auto queue = CreateTaskQueue();
  std::vector<EnqueueOrder> run_order;

  std::vector<std::pair<OnceClosure, bool>> tasks_to_post_from_nested_loop;

  tasks_to_post_from_nested_loop.push_back(
      std::make_pair(BindOnce(&TestTask, 1, &run_order), false));
  tasks_to_post_from_nested_loop.push_back(
      std::make_pair(BindOnce(&InsertFenceAndPostTestTask, 2, &run_order, queue,
                              sequence_manager()),
                     true));

  queue->task_runner()->PostTask(
      FROM_HERE, BindOnce(&PostFromNestedRunloop, queue,
                          Unretained(&tasks_to_post_from_nested_loop)));
  RunLoop().RunUntilIdle();

  // Task 1 shouldn't run first due to it being non-nestable and queue gets
  // blocked after task 2. Task 1 runs after existing nested message loop
  // due to being posted before inserting a fence.
  // This test checks that breaks when nestable task is pushed into a redo
  // queue.
  EXPECT_THAT(run_order, ElementsAre(2u, 1u));

  queue->RemoveFence();
  RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(2u, 1u, 3u));
}

TEST_P(SequenceManagerTest,
       HasTaskToRunImmediatelyOrReadyDelayedTask_ImmediateTask) {
  auto queue = CreateTaskQueue();

  std::vector<EnqueueOrder> run_order;
  EXPECT_FALSE(queue->HasTaskToRunImmediatelyOrReadyDelayedTask());
  queue->task_runner()->PostTask(FROM_HERE, BindOnce(&TestTask, 1, &run_order));
  EXPECT_TRUE(queue->HasTaskToRunImmediatelyOrReadyDelayedTask());

  // Move the task into the |immediate_work_queue|.
  EXPECT_TRUE(queue->GetTaskQueueImpl()->immediate_work_queue()->Empty());
  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter =
      queue->CreateQueueEnabledVoter();
  voter->SetVoteToEnable(false);
  RunLoop().RunUntilIdle();
  EXPECT_FALSE(queue->GetTaskQueueImpl()->immediate_work_queue()->Empty());
  EXPECT_TRUE(queue->HasTaskToRunImmediatelyOrReadyDelayedTask());

  // Run the task, making the queue empty.
  voter->SetVoteToEnable(true);
  RunLoop().RunUntilIdle();
  EXPECT_FALSE(queue->HasTaskToRunImmediatelyOrReadyDelayedTask());
}

TEST_P(SequenceManagerTest,
       HasTaskToRunImmediatelyOrReadyDelayedTask_DelayedTask) {
  auto queue = CreateTaskQueue();

  std::vector<EnqueueOrder> run_order;
  TimeDelta delay(TimeDelta::FromMilliseconds(10));
  queue->task_runner()->PostDelayedTask(
      FROM_HERE, BindOnce(&TestTask, 1, &run_order), delay);
  EXPECT_FALSE(queue->HasTaskToRunImmediatelyOrReadyDelayedTask());
  AdvanceMockTickClock(delay);
  EXPECT_TRUE(queue->HasTaskToRunImmediatelyOrReadyDelayedTask());

  // Move the task into the |delayed_work_queue|.
  LazyNow lazy_now(mock_tick_clock());
  sequence_manager()->MoveReadyDelayedTasksToWorkQueues(&lazy_now);
  sequence_manager()->ScheduleWork();
  EXPECT_FALSE(queue->GetTaskQueueImpl()->delayed_work_queue()->Empty());
  EXPECT_TRUE(queue->HasTaskToRunImmediatelyOrReadyDelayedTask());

  // Run the task, making the queue empty.
  RunLoop().RunUntilIdle();
  EXPECT_TRUE(queue->GetTaskQueueImpl()->delayed_work_queue()->Empty());
}

TEST_P(SequenceManagerTest, DelayedTaskPosting) {
  auto queue = CreateTaskQueue();

  std::vector<EnqueueOrder> run_order;
  TimeDelta delay(TimeDelta::FromMilliseconds(10));
  queue->task_runner()->PostDelayedTask(
      FROM_HERE, BindOnce(&TestTask, 1, &run_order), delay);
  EXPECT_EQ(TimeDelta::FromMilliseconds(10), NextPendingTaskDelay());
  EXPECT_FALSE(queue->HasTaskToRunImmediatelyOrReadyDelayedTask());
  EXPECT_TRUE(run_order.empty());

  // The task doesn't run before the delay has completed.
  FastForwardBy(TimeDelta::FromMilliseconds(9));
  EXPECT_TRUE(run_order.empty());

  // After the delay has completed, the task runs normally.
  FastForwardBy(TimeDelta::FromMilliseconds(1));
  EXPECT_THAT(run_order, ElementsAre(1u));
  EXPECT_FALSE(queue->HasTaskToRunImmediatelyOrReadyDelayedTask());
}

TEST(SequenceManagerTestWithMockTaskRunner,
     DelayedTaskExecutedInOneMessageLoopTask) {
  FixtureWithMockTaskRunner fixture;
  auto queue =
      fixture.sequence_manager()->CreateTaskQueue(TaskQueue::Spec("test"));

  queue->task_runner()->PostDelayedTask(FROM_HERE, BindOnce(&NopTask),
                                        TimeDelta::FromMilliseconds(10));
  RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, fixture.test_task_runner()->GetPendingTaskCount());
  fixture.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(0u, fixture.test_task_runner()->GetPendingTaskCount());
}

TEST_P(SequenceManagerTest, DelayedTaskPosting_MultipleTasks_DecendingOrder) {
  auto queue = CreateTaskQueue();

  std::vector<EnqueueOrder> run_order;
  queue->task_runner()->PostDelayedTask(FROM_HERE,
                                        BindOnce(&TestTask, 1, &run_order),
                                        TimeDelta::FromMilliseconds(10));

  queue->task_runner()->PostDelayedTask(FROM_HERE,
                                        BindOnce(&TestTask, 2, &run_order),
                                        TimeDelta::FromMilliseconds(8));

  queue->task_runner()->PostDelayedTask(FROM_HERE,
                                        BindOnce(&TestTask, 3, &run_order),
                                        TimeDelta::FromMilliseconds(5));

  EXPECT_EQ(TimeDelta::FromMilliseconds(5), NextPendingTaskDelay());

  FastForwardBy(TimeDelta::FromMilliseconds(5));
  EXPECT_THAT(run_order, ElementsAre(3u));
  EXPECT_EQ(TimeDelta::FromMilliseconds(3), NextPendingTaskDelay());

  FastForwardBy(TimeDelta::FromMilliseconds(3));
  EXPECT_THAT(run_order, ElementsAre(3u, 2u));
  EXPECT_EQ(TimeDelta::FromMilliseconds(2), NextPendingTaskDelay());

  FastForwardBy(TimeDelta::FromMilliseconds(2));
  EXPECT_THAT(run_order, ElementsAre(3u, 2u, 1u));
}

TEST_P(SequenceManagerTest, DelayedTaskPosting_MultipleTasks_AscendingOrder) {
  auto queue = CreateTaskQueue();

  std::vector<EnqueueOrder> run_order;
  queue->task_runner()->PostDelayedTask(FROM_HERE,
                                        BindOnce(&TestTask, 1, &run_order),
                                        TimeDelta::FromMilliseconds(1));

  queue->task_runner()->PostDelayedTask(FROM_HERE,
                                        BindOnce(&TestTask, 2, &run_order),
                                        TimeDelta::FromMilliseconds(5));

  queue->task_runner()->PostDelayedTask(FROM_HERE,
                                        BindOnce(&TestTask, 3, &run_order),
                                        TimeDelta::FromMilliseconds(10));

  EXPECT_EQ(TimeDelta::FromMilliseconds(1), NextPendingTaskDelay());

  FastForwardBy(TimeDelta::FromMilliseconds(1));
  EXPECT_THAT(run_order, ElementsAre(1u));
  EXPECT_EQ(TimeDelta::FromMilliseconds(4), NextPendingTaskDelay());

  FastForwardBy(TimeDelta::FromMilliseconds(4));
  EXPECT_THAT(run_order, ElementsAre(1u, 2u));
  EXPECT_EQ(TimeDelta::FromMilliseconds(5), NextPendingTaskDelay());

  FastForwardBy(TimeDelta::FromMilliseconds(5));
  EXPECT_THAT(run_order, ElementsAre(1u, 2u, 3u));
}

TEST(SequenceManagerTestWithMockTaskRunner,
     PostDelayedTask_SharesUnderlyingDelayedTasks) {
  FixtureWithMockTaskRunner fixture;
  auto queue =
      fixture.sequence_manager()->CreateTaskQueue(TaskQueue::Spec("test"));

  std::vector<EnqueueOrder> run_order;
  TimeDelta delay(TimeDelta::FromMilliseconds(10));
  queue->task_runner()->PostDelayedTask(
      FROM_HERE, BindOnce(&TestTask, 1, &run_order), delay);
  queue->task_runner()->PostDelayedTask(
      FROM_HERE, BindOnce(&TestTask, 2, &run_order), delay);
  queue->task_runner()->PostDelayedTask(
      FROM_HERE, BindOnce(&TestTask, 3, &run_order), delay);

  EXPECT_EQ(1u, fixture.test_task_runner()->GetPendingTaskCount());
}

TEST(SequenceManagerTestWithMockTaskRunner,
     CrossThreadTaskPostingToDisabledQueueDoesntScheduleWork) {
  FixtureWithMockTaskRunner fixture;
  auto queue =
      fixture.sequence_manager()->CreateTaskQueue(TaskQueue::Spec("test"));
  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter =
      queue->CreateQueueEnabledVoter();
  voter->SetVoteToEnable(false);

  WaitableEvent done_event;
  Thread thread("TestThread");
  thread.Start();
  thread.task_runner()->PostTask(FROM_HERE, BindLambdaForTesting([&]() {
                                   // Should not schedule a DoWork.
                                   queue->task_runner()->PostTask(
                                       FROM_HERE, BindOnce(&NopTask));
                                   done_event.Signal();
                                 }));
  done_event.Wait();
  thread.Stop();

  EXPECT_EQ(0u, fixture.test_task_runner()->GetPendingTaskCount());

  // But if the queue becomes re-enabled it does schedule work.
  voter->SetVoteToEnable(true);
  EXPECT_EQ(1u, fixture.test_task_runner()->GetPendingTaskCount());
}

TEST(SequenceManagerTestWithMockTaskRunner,
     CrossThreadTaskPostingToBlockedQueueDoesntScheduleWork) {
  FixtureWithMockTaskRunner fixture;
  auto queue =
      fixture.sequence_manager()->CreateTaskQueue(TaskQueue::Spec("test"));
  queue->InsertFence(TaskQueue::InsertFencePosition::kNow);

  WaitableEvent done_event;
  Thread thread("TestThread");
  thread.Start();
  thread.task_runner()->PostTask(FROM_HERE, BindLambdaForTesting([&]() {
                                   // Should not schedule a DoWork.
                                   queue->task_runner()->PostTask(
                                       FROM_HERE, BindOnce(&NopTask));
                                   done_event.Signal();
                                 }));
  done_event.Wait();
  thread.Stop();

  EXPECT_EQ(0u, fixture.test_task_runner()->GetPendingTaskCount());

  // But if the queue becomes unblocked it does schedule work.
  queue->RemoveFence();
  EXPECT_EQ(1u, fixture.test_task_runner()->GetPendingTaskCount());
}

namespace {

class TestObject {
 public:
  ~TestObject() { destructor_count__++; }

  void Run() { FAIL() << "TestObject::Run should not be called"; }

  static int destructor_count__;
};

int TestObject::destructor_count__ = 0;

}  // namespace

TEST_P(SequenceManagerTest, PendingDelayedTasksRemovedOnShutdown) {
  auto queue = CreateTaskQueue();

  TestObject::destructor_count__ = 0;

  TimeDelta delay(TimeDelta::FromMilliseconds(10));
  queue->task_runner()->PostDelayedTask(
      FROM_HERE, BindOnce(&TestObject::Run, Owned(new TestObject())), delay);
  queue->task_runner()->PostTask(
      FROM_HERE, BindOnce(&TestObject::Run, Owned(new TestObject())));

  DestroySequenceManager();

  EXPECT_EQ(2, TestObject::destructor_count__);
}

TEST_P(SequenceManagerTest, InsertAndRemoveFence) {
  auto queue = CreateTaskQueue();
  StrictMock<MockTask> task;

  // Posting a task when pumping is disabled doesn't result in work getting
  // posted.
  queue->InsertFence(TaskQueue::InsertFencePosition::kNow);
  queue->task_runner()->PostTask(FROM_HERE, task.Get());
  EXPECT_CALL(task, Run).Times(0);
  RunLoop().RunUntilIdle();

  // However polling still works.
  EXPECT_TRUE(queue->HasTaskToRunImmediatelyOrReadyDelayedTask());

  // After removing the fence the task runs normally.
  queue->RemoveFence();
  EXPECT_CALL(task, Run);
  RunLoop().RunUntilIdle();
}

TEST_P(SequenceManagerTest, RemovingFenceForDisabledQueueDoesNotPostDoWork) {
  auto queue = CreateTaskQueue();
  StrictMock<MockTask> task;

  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter =
      queue->CreateQueueEnabledVoter();
  voter->SetVoteToEnable(false);
  queue->InsertFence(TaskQueue::InsertFencePosition::kNow);
  queue->task_runner()->PostTask(FROM_HERE, task.Get());

  queue->RemoveFence();
  EXPECT_CALL(task, Run).Times(0);
  RunLoop().RunUntilIdle();
}

TEST_P(SequenceManagerTest, EnablingFencedQueueDoesNotPostDoWork) {
  auto queue = CreateTaskQueue();
  StrictMock<MockTask> task;

  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter =
      queue->CreateQueueEnabledVoter();
  voter->SetVoteToEnable(false);
  queue->InsertFence(TaskQueue::InsertFencePosition::kNow);
  queue->task_runner()->PostTask(FROM_HERE, task.Get());
  voter->SetVoteToEnable(true);

  EXPECT_CALL(task, Run).Times(0);
  RunLoop().RunUntilIdle();
}

TEST_P(SequenceManagerTest, DenyRunning_BeforePosting) {
  auto queue = CreateTaskQueue();
  StrictMock<MockTask> task;

  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter =
      queue->CreateQueueEnabledVoter();
  voter->SetVoteToEnable(false);
  queue->task_runner()->PostTask(FROM_HERE, task.Get());

  EXPECT_CALL(task, Run).Times(0);
  RunLoop().RunUntilIdle();

  voter->SetVoteToEnable(true);
  EXPECT_CALL(task, Run);
  RunLoop().RunUntilIdle();
}

TEST_P(SequenceManagerTest, DenyRunning_AfterPosting) {
  auto queue = CreateTaskQueue();
  StrictMock<MockTask> task;

  queue->task_runner()->PostTask(FROM_HERE, task.Get());
  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter =
      queue->CreateQueueEnabledVoter();
  voter->SetVoteToEnable(false);

  EXPECT_CALL(task, Run).Times(0);
  RunLoop().RunUntilIdle();

  voter->SetVoteToEnable(true);
  EXPECT_CALL(task, Run);
  RunLoop().RunUntilIdle();
}

TEST_P(SequenceManagerTest, DenyRunning_AfterRemovingFence) {
  auto queue = CreateTaskQueue();

  std::vector<EnqueueOrder> run_order;
  queue->InsertFence(TaskQueue::InsertFencePosition::kNow);
  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter =
      queue->CreateQueueEnabledVoter();
  voter->SetVoteToEnable(false);
  queue->task_runner()->PostTask(FROM_HERE, BindOnce(&TestTask, 1, &run_order));

  RunLoop().RunUntilIdle();
  EXPECT_TRUE(run_order.empty());

  queue->RemoveFence();
  voter->SetVoteToEnable(true);
  RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1u));
}

TEST_P(SequenceManagerTest, RemovingFenceWithDelayedTask) {
  TimeDelta kDelay = TimeDelta::FromMilliseconds(10);
  auto queue = CreateTaskQueue();
  StrictMock<MockTask> task;

  // Posting a delayed task when fenced will apply the delay, but won't cause
  // work to executed afterwards.
  queue->InsertFence(TaskQueue::InsertFencePosition::kNow);

  queue->task_runner()->PostDelayedTask(FROM_HERE, task.Get(), kDelay);

  // The task does not run even though it's delay is up.
  EXPECT_CALL(task, Run).Times(0);
  FastForwardBy(kDelay);

  // Removing the fence causes the task to run.
  queue->RemoveFence();
  EXPECT_CALL(task, Run);
  RunLoop().RunUntilIdle();
}

TEST_P(SequenceManagerTest, RemovingFenceWithMultipleDelayedTasks) {
  auto queue = CreateTaskQueue();
  queue->InsertFence(TaskQueue::InsertFencePosition::kNow);

  std::vector<EnqueueOrder> run_order;
  // Posting a delayed task when fenced will apply the delay, but won't cause
  // work to executed afterwards.
  TimeDelta delay1(TimeDelta::FromMilliseconds(1));
  TimeDelta delay2(TimeDelta::FromMilliseconds(10));
  TimeDelta delay3(TimeDelta::FromMilliseconds(20));
  queue->task_runner()->PostDelayedTask(
      FROM_HERE, BindOnce(&TestTask, 1, &run_order), delay1);
  queue->task_runner()->PostDelayedTask(
      FROM_HERE, BindOnce(&TestTask, 2, &run_order), delay2);
  queue->task_runner()->PostDelayedTask(
      FROM_HERE, BindOnce(&TestTask, 3, &run_order), delay3);

  AdvanceMockTickClock(TimeDelta::FromMilliseconds(15));
  RunLoop().RunUntilIdle();
  EXPECT_TRUE(run_order.empty());

  // Removing the fence causes the ready tasks to run.
  queue->RemoveFence();
  RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1u, 2u));
}

TEST_P(SequenceManagerTest, InsertFencePreventsDelayedTasksFromRunning) {
  auto queue = CreateTaskQueue();
  queue->InsertFence(TaskQueue::InsertFencePosition::kNow);

  std::vector<EnqueueOrder> run_order;
  TimeDelta delay(TimeDelta::FromMilliseconds(10));
  queue->task_runner()->PostDelayedTask(
      FROM_HERE, BindOnce(&TestTask, 1, &run_order), delay);

  FastForwardBy(TimeDelta::FromMilliseconds(10));
  EXPECT_TRUE(run_order.empty());
}

TEST_P(SequenceManagerTest, MultipleFences) {
  auto queue = CreateTaskQueue();

  std::vector<EnqueueOrder> run_order;
  queue->task_runner()->PostTask(FROM_HERE, BindOnce(&TestTask, 1, &run_order));
  queue->task_runner()->PostTask(FROM_HERE, BindOnce(&TestTask, 2, &run_order));
  queue->InsertFence(TaskQueue::InsertFencePosition::kNow);

  queue->task_runner()->PostTask(FROM_HERE, BindOnce(&TestTask, 3, &run_order));
  RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1u, 2u));

  queue->InsertFence(TaskQueue::InsertFencePosition::kNow);
  // Subsequent tasks should be blocked.
  queue->task_runner()->PostTask(FROM_HERE, BindOnce(&TestTask, 4, &run_order));
  RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1u, 2u, 3u));
}

TEST_P(SequenceManagerTest, InsertFenceThenImmediatlyRemoveDoesNotBlock) {
  auto queue = CreateTaskQueue();
  queue->InsertFence(TaskQueue::InsertFencePosition::kNow);
  queue->RemoveFence();

  std::vector<EnqueueOrder> run_order;
  queue->task_runner()->PostTask(FROM_HERE, BindOnce(&TestTask, 1, &run_order));
  queue->task_runner()->PostTask(FROM_HERE, BindOnce(&TestTask, 2, &run_order));

  RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1u, 2u));
}

TEST_P(SequenceManagerTest, InsertFencePostThenRemoveDoesNotBlock) {
  auto queue = CreateTaskQueue();
  queue->InsertFence(TaskQueue::InsertFencePosition::kNow);

  std::vector<EnqueueOrder> run_order;
  queue->task_runner()->PostTask(FROM_HERE, BindOnce(&TestTask, 1, &run_order));
  queue->task_runner()->PostTask(FROM_HERE, BindOnce(&TestTask, 2, &run_order));
  queue->RemoveFence();

  RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1u, 2u));
}

TEST_P(SequenceManagerTest, MultipleFencesWithInitiallyEmptyQueue) {
  auto queue = CreateTaskQueue();
  queue->InsertFence(TaskQueue::InsertFencePosition::kNow);

  std::vector<EnqueueOrder> run_order;
  queue->task_runner()->PostTask(FROM_HERE, BindOnce(&TestTask, 1, &run_order));
  queue->InsertFence(TaskQueue::InsertFencePosition::kNow);
  queue->task_runner()->PostTask(FROM_HERE, BindOnce(&TestTask, 2, &run_order));

  RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1u));
}

TEST_P(SequenceManagerTest, BlockedByFence) {
  auto queue = CreateTaskQueue();
  EXPECT_FALSE(queue->BlockedByFence());

  queue->InsertFence(TaskQueue::InsertFencePosition::kNow);
  EXPECT_TRUE(queue->BlockedByFence());

  queue->RemoveFence();
  EXPECT_FALSE(queue->BlockedByFence());

  queue->task_runner()->PostTask(FROM_HERE, BindOnce(&NopTask));
  queue->InsertFence(TaskQueue::InsertFencePosition::kNow);
  EXPECT_FALSE(queue->BlockedByFence());

  RunLoop().RunUntilIdle();
  EXPECT_TRUE(queue->BlockedByFence());

  queue->RemoveFence();
  EXPECT_FALSE(queue->BlockedByFence());
}

TEST_P(SequenceManagerTest, BlockedByFence_BothTypesOfFence) {
  auto queue = CreateTaskQueue();

  queue->task_runner()->PostTask(FROM_HERE, BindOnce(&NopTask));

  queue->InsertFence(TaskQueue::InsertFencePosition::kNow);
  EXPECT_FALSE(queue->BlockedByFence());

  queue->InsertFence(TaskQueue::InsertFencePosition::kBeginningOfTime);
  EXPECT_TRUE(queue->BlockedByFence());
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
  const auto kStartTime = mock_tick_clock()->NowTicks();
  scoped_refptr<TestTaskQueue> queue =
      CreateTaskQueue(TaskQueue::Spec("test").SetDelayedFencesAllowed(true));

  std::vector<TimeTicks> run_times;
  queue->task_runner()->PostDelayedTask(
      FROM_HERE, BindOnce(&RecordTimeTask, &run_times, mock_tick_clock()),
      TimeDelta::FromMilliseconds(100));
  queue->task_runner()->PostDelayedTask(
      FROM_HERE, BindOnce(&RecordTimeTask, &run_times, mock_tick_clock()),
      TimeDelta::FromMilliseconds(200));
  queue->task_runner()->PostDelayedTask(
      FROM_HERE, BindOnce(&RecordTimeTask, &run_times, mock_tick_clock()),
      TimeDelta::FromMilliseconds(300));

  queue->InsertFenceAt(mock_tick_clock()->NowTicks() +
                       TimeDelta::FromMilliseconds(250));
  EXPECT_FALSE(queue->HasActiveFence());

  FastForwardUntilNoTasksRemain();

  EXPECT_TRUE(queue->HasActiveFence());
  EXPECT_THAT(run_times,
              ElementsAre(kStartTime + TimeDelta::FromMilliseconds(100),
                          kStartTime + TimeDelta::FromMilliseconds(200)));
  run_times.clear();

  queue->RemoveFence();

  FastForwardUntilNoTasksRemain();

  EXPECT_FALSE(queue->HasActiveFence());
  EXPECT_THAT(run_times,
              ElementsAre(kStartTime + TimeDelta::FromMilliseconds(300)));
}

TEST_P(SequenceManagerTest, DelayedFence_ImmediateTasks) {
  const auto kStartTime = mock_tick_clock()->NowTicks();
  scoped_refptr<TestTaskQueue> queue =
      CreateTaskQueue(TaskQueue::Spec("test").SetDelayedFencesAllowed(true));

  std::vector<TimeTicks> run_times;
  queue->InsertFenceAt(mock_tick_clock()->NowTicks() +
                       TimeDelta::FromMilliseconds(250));

  for (int i = 0; i < 5; ++i) {
    queue->task_runner()->PostTask(
        FROM_HERE, BindOnce(&RecordTimeTask, &run_times, mock_tick_clock()));
    FastForwardBy(TimeDelta::FromMilliseconds(100));
    if (i < 2) {
      EXPECT_FALSE(queue->HasActiveFence());
    } else {
      EXPECT_TRUE(queue->HasActiveFence());
    }
  }

  EXPECT_THAT(
      run_times,
      ElementsAre(kStartTime, kStartTime + TimeDelta::FromMilliseconds(100),
                  kStartTime + TimeDelta::FromMilliseconds(200)));
  run_times.clear();

  queue->RemoveFence();
  FastForwardUntilNoTasksRemain();

  EXPECT_THAT(run_times,
              ElementsAre(kStartTime + TimeDelta::FromMilliseconds(500),
                          kStartTime + TimeDelta::FromMilliseconds(500)));
}

TEST_P(SequenceManagerTest, DelayedFence_RemovedFenceDoesNotActivate) {
  const auto kStartTime = mock_tick_clock()->NowTicks();
  scoped_refptr<TestTaskQueue> queue =
      CreateTaskQueue(TaskQueue::Spec("test").SetDelayedFencesAllowed(true));

  std::vector<TimeTicks> run_times;
  queue->InsertFenceAt(mock_tick_clock()->NowTicks() +
                       TimeDelta::FromMilliseconds(250));

  for (int i = 0; i < 3; ++i) {
    queue->task_runner()->PostTask(
        FROM_HERE, BindOnce(&RecordTimeTask, &run_times, mock_tick_clock()));
    EXPECT_FALSE(queue->HasActiveFence());
    FastForwardBy(TimeDelta::FromMilliseconds(100));
  }

  EXPECT_TRUE(queue->HasActiveFence());
  queue->RemoveFence();

  for (int i = 0; i < 2; ++i) {
    queue->task_runner()->PostTask(
        FROM_HERE, BindOnce(&RecordTimeTask, &run_times, mock_tick_clock()));
    FastForwardBy(TimeDelta::FromMilliseconds(100));
    EXPECT_FALSE(queue->HasActiveFence());
  }

  EXPECT_THAT(
      run_times,
      ElementsAre(kStartTime, kStartTime + TimeDelta::FromMilliseconds(100),
                  kStartTime + TimeDelta::FromMilliseconds(200),
                  kStartTime + TimeDelta::FromMilliseconds(300),
                  kStartTime + TimeDelta::FromMilliseconds(400)));
}

TEST_P(SequenceManagerTest, DelayedFence_TakeIncomingImmediateQueue) {
  // This test checks that everything works correctly when a work queue
  // is swapped with an immediate incoming queue and a delayed fence
  // is activated, forcing a different queue to become active.
  const auto kStartTime = mock_tick_clock()->NowTicks();
  scoped_refptr<TestTaskQueue> queue1 =
      CreateTaskQueue(TaskQueue::Spec("test").SetDelayedFencesAllowed(true));
  scoped_refptr<TestTaskQueue> queue2 =
      CreateTaskQueue(TaskQueue::Spec("test").SetDelayedFencesAllowed(true));

  std::vector<std::pair<scoped_refptr<TestTaskQueue>, TimeTicks>> run_times;

  // Fence ensures that the task posted after advancing time is blocked.
  queue1->InsertFenceAt(mock_tick_clock()->NowTicks() +
                        TimeDelta::FromMilliseconds(250));

  // This task should not be blocked and should run immediately after
  // advancing time at 301ms.
  queue1->task_runner()->PostTask(
      FROM_HERE,
      BindOnce(&RecordTimeAndQueueTask, &run_times, queue1, mock_tick_clock()));
  // Force reload of immediate work queue. In real life the same effect can be
  // achieved with cross-thread posting.
  sequence_manager()->ReloadEmptyWorkQueues();

  AdvanceMockTickClock(TimeDelta::FromMilliseconds(300));

  // This task should be blocked.
  queue1->task_runner()->PostTask(
      FROM_HERE,
      BindOnce(&RecordTimeAndQueueTask, &run_times, queue1, mock_tick_clock()));
  // This task on a different runner should run as expected.
  queue2->task_runner()->PostTask(
      FROM_HERE,
      BindOnce(&RecordTimeAndQueueTask, &run_times, queue2, mock_tick_clock()));

  FastForwardUntilNoTasksRemain();

  EXPECT_THAT(
      run_times,
      ElementsAre(
          std::make_pair(queue1, kStartTime + TimeDelta::FromMilliseconds(300)),
          std::make_pair(queue2,
                         kStartTime + TimeDelta::FromMilliseconds(300))));
}

namespace {

void ReentrantTestTask(scoped_refptr<TestTaskQueue> runner,
                       int countdown,
                       std::vector<EnqueueOrder>* out_result) {
  out_result->push_back(EnqueueOrder::FromIntForTesting(countdown));
  if (--countdown) {
    runner->task_runner()->PostTask(
        FROM_HERE, BindOnce(&ReentrantTestTask, runner, countdown, out_result));
  }
}

}  // namespace

TEST_P(SequenceManagerTest, ReentrantPosting) {
  auto queue = CreateTaskQueue();

  std::vector<EnqueueOrder> run_order;
  queue->task_runner()->PostTask(
      FROM_HERE, BindOnce(&ReentrantTestTask, queue, 3, &run_order));

  RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(3u, 2u, 1u));
}

namespace {

class RefCountedCallbackFactory {
 public:
  OnceCallback<void()> WrapCallback(OnceCallback<void()> cb) {
    return BindOnce(
        [](OnceCallback<void()> cb, WeakPtr<bool>) { std::move(cb).Run(); },
        std::move(cb), task_references_.GetWeakPtr());
  }

  bool HasReferences() const { return task_references_.HasWeakPtrs(); }

 private:
  bool dummy_;
  WeakPtrFactory<bool> task_references_{&dummy_};
};

}  // namespace

TEST_P(SequenceManagerTest, NoTasksAfterShutdown) {
  auto queue = CreateTaskQueue();
  StrictMock<MockTask> task;
  RefCountedCallbackFactory counter;

  EXPECT_CALL(task, Run).Times(0);
  queue->task_runner()->PostTask(FROM_HERE, counter.WrapCallback(task.Get()));
  DestroySequenceManager();
  queue->task_runner()->PostTask(FROM_HERE, counter.WrapCallback(task.Get()));

  if (GetUnderlyingRunnerType() != TestType::kMessagePump) {
    RunLoop().RunUntilIdle();
  }

  EXPECT_FALSE(counter.HasReferences());
}

void PostTaskToRunner(scoped_refptr<TestTaskQueue> runner,
                      std::vector<EnqueueOrder>* run_order) {
  runner->task_runner()->PostTask(FROM_HERE, BindOnce(&TestTask, 1, run_order));
}

TEST_P(SequenceManagerTest, PostFromThread) {
  auto queue = CreateTaskQueue();

  std::vector<EnqueueOrder> run_order;
  Thread thread("TestThread");
  thread.Start();
  thread.task_runner()->PostTask(
      FROM_HERE, BindOnce(&PostTaskToRunner, queue, &run_order));
  thread.Stop();

  RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1u));
}

void RePostingTestTask(scoped_refptr<TestTaskQueue> runner, int* run_count) {
  (*run_count)++;
  runner->task_runner()->PostTask(
      FROM_HERE,
      BindOnce(&RePostingTestTask, Unretained(runner.get()), run_count));
}

TEST_P(SequenceManagerTest, DoWorkCantPostItselfMultipleTimes) {
  auto queue = CreateTaskQueue();

  int run_count = 0;
  queue->task_runner()->PostTask(
      FROM_HERE, BindOnce(&RePostingTestTask, queue, &run_count));

  RunDoWorkOnce();
  EXPECT_EQ(1u, sequence_manager()->GetPendingTaskCountForTesting());
  EXPECT_EQ(1, run_count);
}

TEST_P(SequenceManagerTest, PostFromNestedRunloop) {
  auto queue = CreateTaskQueue();

  std::vector<EnqueueOrder> run_order;
  std::vector<std::pair<OnceClosure, bool>> tasks_to_post_from_nested_loop;
  tasks_to_post_from_nested_loop.push_back(
      std::make_pair(BindOnce(&TestTask, 1, &run_order), true));

  queue->task_runner()->PostTask(FROM_HERE, BindOnce(&TestTask, 0, &run_order));
  queue->task_runner()->PostTask(
      FROM_HERE, BindOnce(&PostFromNestedRunloop, queue,
                          Unretained(&tasks_to_post_from_nested_loop)));
  queue->task_runner()->PostTask(FROM_HERE, BindOnce(&TestTask, 2, &run_order));

  RunLoop().RunUntilIdle();

  EXPECT_THAT(run_order, ElementsAre(0u, 2u, 1u));
}

TEST_P(SequenceManagerTest, WorkBatching) {
  auto queue = CreateTaskQueue();
  sequence_manager()->SetWorkBatchSize(2);

  std::vector<EnqueueOrder> run_order;
  for (int i = 0; i < 4; ++i) {
    queue->task_runner()->PostTask(FROM_HERE,
                                   BindOnce(&TestTask, i, &run_order));
  }

  // Running one task in the host message loop should cause two posted tasks
  // to get executed.
  RunDoWorkOnce();
  EXPECT_THAT(run_order, ElementsAre(0u, 1u));

  // The second task runs the remaining two posted tasks.
  RunDoWorkOnce();
  EXPECT_THAT(run_order, ElementsAre(0u, 1u, 2u, 3u));
}

namespace {

class MockTaskObserver : public TaskObserver {
 public:
  MOCK_METHOD1(DidProcessTask, void(const PendingTask& task));
  MOCK_METHOD2(WillProcessTask,
               void(const PendingTask& task, bool was_blocked_or_low_priority));
};

}  // namespace

TEST_P(SequenceManagerTest, TaskObserverAdding) {
  auto queue = CreateTaskQueue();
  MockTaskObserver observer;

  sequence_manager()->SetWorkBatchSize(2);
  sequence_manager()->AddTaskObserver(&observer);

  std::vector<EnqueueOrder> run_order;
  queue->task_runner()->PostTask(FROM_HERE, BindOnce(&TestTask, 1, &run_order));
  queue->task_runner()->PostTask(FROM_HERE, BindOnce(&TestTask, 2, &run_order));

  EXPECT_CALL(observer,
              WillProcessTask(_, /*was_blocked_or_low_priority=*/false))
      .Times(2);
  EXPECT_CALL(observer, DidProcessTask(_)).Times(2);
  RunLoop().RunUntilIdle();
}

TEST_P(SequenceManagerTest, TaskObserverRemoving) {
  auto queue = CreateTaskQueue();
  MockTaskObserver observer;
  sequence_manager()->SetWorkBatchSize(2);
  sequence_manager()->AddTaskObserver(&observer);
  sequence_manager()->RemoveTaskObserver(&observer);

  std::vector<EnqueueOrder> run_order;
  queue->task_runner()->PostTask(FROM_HERE, BindOnce(&TestTask, 1, &run_order));

  EXPECT_CALL(observer, WillProcessTask(_, _)).Times(0);
  EXPECT_CALL(observer, DidProcessTask(_)).Times(0);
  RunLoop().RunUntilIdle();
}

void RemoveObserverTask(SequenceManagerImpl* manager, TaskObserver* observer) {
  manager->RemoveTaskObserver(observer);
}

TEST_P(SequenceManagerTest, TaskObserverRemovingInsideTask) {
  auto queue = CreateTaskQueue();
  MockTaskObserver observer;
  sequence_manager()->SetWorkBatchSize(3);
  sequence_manager()->AddTaskObserver(&observer);

  queue->task_runner()->PostTask(
      FROM_HERE, BindOnce(&RemoveObserverTask, sequence_manager(), &observer));

  EXPECT_CALL(observer,
              WillProcessTask(_, /*was_blocked_or_low_priority=*/false))
      .Times(1);
  EXPECT_CALL(observer, DidProcessTask(_)).Times(0);
  RunLoop().RunUntilIdle();
}

TEST_P(SequenceManagerTest, QueueTaskObserverAdding) {
  auto queues = CreateTaskQueues(2);
  MockTaskObserver observer;

  sequence_manager()->SetWorkBatchSize(2);
  queues[0]->AddTaskObserver(&observer);

  std::vector<EnqueueOrder> run_order;
  queues[0]->task_runner()->PostTask(FROM_HERE,
                                     BindOnce(&TestTask, 1, &run_order));
  queues[1]->task_runner()->PostTask(FROM_HERE,
                                     BindOnce(&TestTask, 2, &run_order));

  EXPECT_CALL(observer,
              WillProcessTask(_, /*was_blocked_or_low_priority=*/false))
      .Times(1);
  EXPECT_CALL(observer, DidProcessTask(_)).Times(1);
  RunLoop().RunUntilIdle();
}

TEST_P(SequenceManagerTest, QueueTaskObserverRemoving) {
  auto queue = CreateTaskQueue();
  MockTaskObserver observer;
  sequence_manager()->SetWorkBatchSize(2);
  queue->AddTaskObserver(&observer);
  queue->RemoveTaskObserver(&observer);

  std::vector<EnqueueOrder> run_order;
  queue->task_runner()->PostTask(FROM_HERE, BindOnce(&TestTask, 1, &run_order));

  EXPECT_CALL(observer,
              WillProcessTask(_, /*was_blocked_or_low_priority=*/false))
      .Times(0);
  EXPECT_CALL(observer, DidProcessTask(_)).Times(0);

  RunLoop().RunUntilIdle();
}

void RemoveQueueObserverTask(scoped_refptr<TestTaskQueue> queue,
                             TaskObserver* observer) {
  queue->RemoveTaskObserver(observer);
}

TEST_P(SequenceManagerTest, QueueTaskObserverRemovingInsideTask) {
  auto queue = CreateTaskQueue();
  MockTaskObserver observer;
  queue->AddTaskObserver(&observer);

  queue->task_runner()->PostTask(
      FROM_HERE, BindOnce(&RemoveQueueObserverTask, queue, &observer));

  EXPECT_CALL(observer,
              WillProcessTask(_, /*was_blocked_or_low_priority=*/false))
      .Times(1);
  EXPECT_CALL(observer, DidProcessTask(_)).Times(0);
  RunLoop().RunUntilIdle();
}

TEST_P(SequenceManagerTest, ThreadCheckAfterTermination) {
  auto queue = CreateTaskQueue();
  EXPECT_TRUE(queue->task_runner()->RunsTasksInCurrentSequence());
  DestroySequenceManager();
  EXPECT_TRUE(queue->task_runner()->RunsTasksInCurrentSequence());
}

TEST_P(SequenceManagerTest, TimeDomain_NextScheduledRunTime) {
  auto queues = CreateTaskQueues(2u);
  AdvanceMockTickClock(TimeDelta::FromMicroseconds(10000));
  LazyNow lazy_now_1(mock_tick_clock());

  // With no delayed tasks.
  EXPECT_FALSE(
      sequence_manager()->GetRealTimeDomain()->DelayTillNextTask(&lazy_now_1));

  // With a non-delayed task.
  queues[0]->task_runner()->PostTask(FROM_HERE, BindOnce(&NopTask));
  EXPECT_FALSE(
      sequence_manager()->GetRealTimeDomain()->DelayTillNextTask(&lazy_now_1));

  // With a delayed task.
  TimeDelta expected_delay = TimeDelta::FromMilliseconds(50);
  queues[0]->task_runner()->PostDelayedTask(FROM_HERE, BindOnce(&NopTask),
                                            expected_delay);
  EXPECT_EQ(
      expected_delay,
      sequence_manager()->GetRealTimeDomain()->DelayTillNextTask(&lazy_now_1));

  // With another delayed task in the same queue with a longer delay.
  queues[0]->task_runner()->PostDelayedTask(FROM_HERE, BindOnce(&NopTask),
                                            TimeDelta::FromMilliseconds(100));
  EXPECT_EQ(
      expected_delay,
      sequence_manager()->GetRealTimeDomain()->DelayTillNextTask(&lazy_now_1));

  // With another delayed task in the same queue with a shorter delay.
  expected_delay = TimeDelta::FromMilliseconds(20);
  queues[0]->task_runner()->PostDelayedTask(FROM_HERE, BindOnce(&NopTask),
                                            expected_delay);
  EXPECT_EQ(
      expected_delay,
      sequence_manager()->GetRealTimeDomain()->DelayTillNextTask(&lazy_now_1));

  // With another delayed task in a different queue with a shorter delay.
  expected_delay = TimeDelta::FromMilliseconds(10);
  queues[1]->task_runner()->PostDelayedTask(FROM_HERE, BindOnce(&NopTask),
                                            expected_delay);
  EXPECT_EQ(
      expected_delay,
      sequence_manager()->GetRealTimeDomain()->DelayTillNextTask(&lazy_now_1));

  // Test it updates as time progresses
  AdvanceMockTickClock(expected_delay);
  LazyNow lazy_now_2(mock_tick_clock());
  EXPECT_EQ(
      TimeDelta(),
      sequence_manager()->GetRealTimeDomain()->DelayTillNextTask(&lazy_now_2));
}

TEST_P(SequenceManagerTest, TimeDomain_NextScheduledRunTime_MultipleQueues) {
  auto queues = CreateTaskQueues(3u);

  TimeDelta delay1 = TimeDelta::FromMilliseconds(50);
  TimeDelta delay2 = TimeDelta::FromMilliseconds(5);
  TimeDelta delay3 = TimeDelta::FromMilliseconds(10);
  queues[0]->task_runner()->PostDelayedTask(FROM_HERE, BindOnce(&NopTask),
                                            delay1);
  queues[1]->task_runner()->PostDelayedTask(FROM_HERE, BindOnce(&NopTask),
                                            delay2);
  queues[2]->task_runner()->PostDelayedTask(FROM_HERE, BindOnce(&NopTask),
                                            delay3);
  queues[0]->task_runner()->PostTask(FROM_HERE, BindOnce(&NopTask));

  LazyNow lazy_now(mock_tick_clock());
  EXPECT_EQ(delay2, sequence_manager()->GetRealTimeDomain()->DelayTillNextTask(
                        &lazy_now));
}

TEST(SequenceManagerWithTaskRunnerTest, DeleteSequenceManagerInsideATask) {
  FixtureWithMockTaskRunner fixture;
  auto queue =
      fixture.sequence_manager()->CreateTaskQueue(TaskQueue::Spec("test"));

  queue->task_runner()->PostTask(FROM_HERE, BindLambdaForTesting([&]() {
                                   fixture.DestroySequenceManager();
                                 }));

  // This should not crash, assuming DoWork detects the SequenceManager has
  // been deleted.
  RunLoop().RunUntilIdle();
}

TEST_P(SequenceManagerTest, GetAndClearSystemIsQuiescentBit) {
  auto queues = CreateTaskQueues(3u);

  scoped_refptr<TestTaskQueue> queue0 =
      CreateTaskQueue(TaskQueue::Spec("test").SetShouldMonitorQuiescence(true));
  scoped_refptr<TestTaskQueue> queue1 =
      CreateTaskQueue(TaskQueue::Spec("test").SetShouldMonitorQuiescence(true));
  scoped_refptr<TestTaskQueue> queue2 = CreateTaskQueue();

  EXPECT_TRUE(sequence_manager()->GetAndClearSystemIsQuiescentBit());

  queue0->task_runner()->PostTask(FROM_HERE, BindOnce(&NopTask));
  RunLoop().RunUntilIdle();
  EXPECT_FALSE(sequence_manager()->GetAndClearSystemIsQuiescentBit());
  EXPECT_TRUE(sequence_manager()->GetAndClearSystemIsQuiescentBit());

  queue1->task_runner()->PostTask(FROM_HERE, BindOnce(&NopTask));
  RunLoop().RunUntilIdle();
  EXPECT_FALSE(sequence_manager()->GetAndClearSystemIsQuiescentBit());
  EXPECT_TRUE(sequence_manager()->GetAndClearSystemIsQuiescentBit());

  queue2->task_runner()->PostTask(FROM_HERE, BindOnce(&NopTask));
  RunLoop().RunUntilIdle();
  EXPECT_TRUE(sequence_manager()->GetAndClearSystemIsQuiescentBit());

  queue0->task_runner()->PostTask(FROM_HERE, BindOnce(&NopTask));
  queue1->task_runner()->PostTask(FROM_HERE, BindOnce(&NopTask));
  RunLoop().RunUntilIdle();
  EXPECT_FALSE(sequence_manager()->GetAndClearSystemIsQuiescentBit());
  EXPECT_TRUE(sequence_manager()->GetAndClearSystemIsQuiescentBit());
}

TEST_P(SequenceManagerTest, HasTaskToRunImmediatelyOrReadyDelayedTask) {
  auto queue = CreateTaskQueue();

  EXPECT_FALSE(queue->HasTaskToRunImmediatelyOrReadyDelayedTask());
  queue->task_runner()->PostTask(FROM_HERE, BindOnce(NullTask));
  EXPECT_TRUE(queue->HasTaskToRunImmediatelyOrReadyDelayedTask());

  RunLoop().RunUntilIdle();
  EXPECT_FALSE(queue->HasTaskToRunImmediatelyOrReadyDelayedTask());
}

TEST_P(SequenceManagerTest,
       HasTaskToRunImmediatelyOrReadyDelayedTask_DelayedTasks) {
  auto queue = CreateTaskQueue();

  EXPECT_FALSE(queue->HasTaskToRunImmediatelyOrReadyDelayedTask());
  queue->task_runner()->PostDelayedTask(FROM_HERE, BindOnce(NullTask),
                                        TimeDelta::FromMilliseconds(12));
  EXPECT_FALSE(queue->HasTaskToRunImmediatelyOrReadyDelayedTask());

  // Move time forwards until just before the delayed task should run.
  AdvanceMockTickClock(TimeDelta::FromMilliseconds(10));
  LazyNow lazy_now_1(mock_tick_clock());
  sequence_manager()->MoveReadyDelayedTasksToWorkQueues(&lazy_now_1);
  EXPECT_FALSE(queue->HasTaskToRunImmediatelyOrReadyDelayedTask());

  // Force the delayed task onto the work queue.
  AdvanceMockTickClock(TimeDelta::FromMilliseconds(2));
  EXPECT_TRUE(queue->HasTaskToRunImmediatelyOrReadyDelayedTask());

  LazyNow lazy_now_2(mock_tick_clock());
  sequence_manager()->MoveReadyDelayedTasksToWorkQueues(&lazy_now_2);
  EXPECT_TRUE(queue->HasTaskToRunImmediatelyOrReadyDelayedTask());

  sequence_manager()->ScheduleWork();
  RunLoop().RunUntilIdle();
  EXPECT_FALSE(queue->HasTaskToRunImmediatelyOrReadyDelayedTask());
}

TEST_P(SequenceManagerTest, ImmediateTasksAreNotStarvedByDelayedTasks) {
  auto queue = CreateTaskQueue();
  std::vector<EnqueueOrder> run_order;
  constexpr auto kDelay = TimeDelta::FromMilliseconds(10);

  // By posting the immediate tasks from a delayed one we make sure that the
  // delayed tasks we post afterwards have a lower enqueue_order than the
  // immediate ones. Thus all the delayed ones would run before the immediate
  // ones if it weren't for the anti-starvation feature we are testing here.
  queue->task_runner()->PostDelayedTask(
      FROM_HERE, BindLambdaForTesting([&]() {
        for (int i = 0; i < 9; i++) {
          queue->task_runner()->PostTask(FROM_HERE,
                                         BindOnce(&TestTask, i, &run_order));
        }
      }),
      kDelay);

  for (int i = 10; i < 19; i++) {
    queue->task_runner()->PostDelayedTask(
        FROM_HERE, BindOnce(&TestTask, i, &run_order), kDelay);
  }

  FastForwardBy(TimeDelta::FromMilliseconds(10));

  // Delayed tasks are not allowed to starve out immediate work which is why
  // some of the immediate tasks run out of order.
  uint64_t expected_run_order[] = {10, 11, 12, 0, 13, 14, 15, 1, 16,
                                   17, 18, 2,  3, 4,  5,  6,  7, 8};
  EXPECT_THAT(run_order, ElementsAreArray(expected_run_order));
}

TEST_P(SequenceManagerTest,
       DelayedTaskDoesNotSkipAHeadOfNonDelayedTask_SameQueue) {
  auto queue = CreateTaskQueue();

  std::vector<EnqueueOrder> run_order;
  TimeDelta delay = TimeDelta::FromMilliseconds(10);
  queue->task_runner()->PostTask(FROM_HERE, BindOnce(&TestTask, 2, &run_order));
  queue->task_runner()->PostTask(FROM_HERE, BindOnce(&TestTask, 3, &run_order));
  queue->task_runner()->PostDelayedTask(
      FROM_HERE, BindOnce(&TestTask, 1, &run_order), delay);

  AdvanceMockTickClock(delay * 2);
  RunLoop().RunUntilIdle();

  EXPECT_THAT(run_order, ElementsAre(2u, 3u, 1u));
}

TEST_P(SequenceManagerTest,
       DelayedTaskDoesNotSkipAHeadOfNonDelayedTask_DifferentQueues) {
  auto queues = CreateTaskQueues(2u);

  std::vector<EnqueueOrder> run_order;
  TimeDelta delay = TimeDelta::FromMilliseconds(10);
  queues[1]->task_runner()->PostTask(FROM_HERE,
                                     BindOnce(&TestTask, 2, &run_order));
  queues[1]->task_runner()->PostTask(FROM_HERE,
                                     BindOnce(&TestTask, 3, &run_order));
  queues[0]->task_runner()->PostDelayedTask(
      FROM_HERE, BindOnce(&TestTask, 1, &run_order), delay);

  AdvanceMockTickClock(delay * 2);
  RunLoop().RunUntilIdle();

  EXPECT_THAT(run_order, ElementsAre(2u, 3u, 1u));
}

TEST_P(SequenceManagerTest, DelayedTaskDoesNotSkipAHeadOfShorterDelayedTask) {
  auto queues = CreateTaskQueues(2u);

  std::vector<EnqueueOrder> run_order;
  TimeDelta delay1 = TimeDelta::FromMilliseconds(10);
  TimeDelta delay2 = TimeDelta::FromMilliseconds(5);
  queues[0]->task_runner()->PostDelayedTask(
      FROM_HERE, BindOnce(&TestTask, 1, &run_order), delay1);
  queues[1]->task_runner()->PostDelayedTask(
      FROM_HERE, BindOnce(&TestTask, 2, &run_order), delay2);

  AdvanceMockTickClock(delay1 * 2);
  RunLoop().RunUntilIdle();

  EXPECT_THAT(run_order, ElementsAre(2u, 1u));
}

namespace {

void CheckIsNested(bool* is_nested) {
  *is_nested = RunLoop::IsNestedOnCurrentThread();
}

void PostAndQuitFromNestedRunloop(RunLoop* run_loop,
                                  scoped_refptr<TestTaskQueue> runner,
                                  bool* was_nested) {
  runner->task_runner()->PostTask(FROM_HERE, run_loop->QuitClosure());
  runner->task_runner()->PostTask(FROM_HERE,
                                  BindOnce(&CheckIsNested, was_nested));
  run_loop->Run();
}

}  // namespace

TEST_P(SequenceManagerTest, QuitWhileNested) {
  if (GetUnderlyingRunnerType() == TestType::kMockTaskRunner)
    return;
  // This test makes sure we don't continue running a work batch after a nested
  // run loop has been exited in the middle of the batch.
  auto queue = CreateTaskQueue();
  sequence_manager()->SetWorkBatchSize(2);

  bool was_nested = true;
  RunLoop run_loop(RunLoop::Type::kNestableTasksAllowed);
  queue->task_runner()->PostTask(
      FROM_HERE, BindOnce(&PostAndQuitFromNestedRunloop, Unretained(&run_loop),
                          queue, Unretained(&was_nested)));

  RunLoop().RunUntilIdle();
  EXPECT_FALSE(was_nested);
}

namespace {

class SequenceNumberCapturingTaskObserver : public TaskObserver {
 public:
  // TaskObserver overrides.
  void WillProcessTask(const PendingTask& pending_task,
                       bool was_blocked_or_low_priority) override {}
  void DidProcessTask(const PendingTask& pending_task) override {
    sequence_numbers_.push_back(pending_task.sequence_num);
  }

  const std::vector<int>& sequence_numbers() const { return sequence_numbers_; }

 private:
  std::vector<int> sequence_numbers_;
};

}  // namespace

TEST_P(SequenceManagerTest, SequenceNumSetWhenTaskIsPosted) {
  auto queue = CreateTaskQueue();

  SequenceNumberCapturingTaskObserver observer;
  sequence_manager()->AddTaskObserver(&observer);

  // Register four tasks that will run in reverse order.
  std::vector<EnqueueOrder> run_order;
  queue->task_runner()->PostDelayedTask(FROM_HERE,
                                        BindOnce(&TestTask, 1, &run_order),
                                        TimeDelta::FromMilliseconds(30));
  queue->task_runner()->PostDelayedTask(FROM_HERE,
                                        BindOnce(&TestTask, 2, &run_order),
                                        TimeDelta::FromMilliseconds(20));
  queue->task_runner()->PostDelayedTask(FROM_HERE,
                                        BindOnce(&TestTask, 3, &run_order),
                                        TimeDelta::FromMilliseconds(10));
  queue->task_runner()->PostTask(FROM_HERE, BindOnce(&TestTask, 4, &run_order));

  FastForwardBy(TimeDelta::FromMilliseconds(40));
  ASSERT_THAT(run_order, ElementsAre(4u, 3u, 2u, 1u));

  // The sequence numbers are a one-based monotonically incrememting counter
  // which should be set when the task is posted rather than when it's enqueued
  // onto the Incoming queue. This counter starts with 2.
  EXPECT_THAT(observer.sequence_numbers(), ElementsAre(5, 4, 3, 2));

  sequence_manager()->RemoveTaskObserver(&observer);
}

TEST_P(SequenceManagerTest, NewTaskQueues) {
  auto queue = CreateTaskQueue();

  scoped_refptr<TestTaskQueue> queue1 = CreateTaskQueue();
  scoped_refptr<TestTaskQueue> queue2 = CreateTaskQueue();
  scoped_refptr<TestTaskQueue> queue3 = CreateTaskQueue();

  ASSERT_NE(queue1, queue2);
  ASSERT_NE(queue1, queue3);
  ASSERT_NE(queue2, queue3);

  std::vector<EnqueueOrder> run_order;
  queue1->task_runner()->PostTask(FROM_HERE,
                                  BindOnce(&TestTask, 1, &run_order));
  queue2->task_runner()->PostTask(FROM_HERE,
                                  BindOnce(&TestTask, 2, &run_order));
  queue3->task_runner()->PostTask(FROM_HERE,
                                  BindOnce(&TestTask, 3, &run_order));
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
  auto queue = CreateTaskQueue();

  scoped_refptr<TestTaskQueue> queue1 = CreateTaskQueue();
  scoped_refptr<TestTaskQueue> queue2 = CreateTaskQueue();
  scoped_refptr<TestTaskQueue> queue3 = CreateTaskQueue();

  ASSERT_NE(queue1, queue2);
  ASSERT_NE(queue1, queue3);
  ASSERT_NE(queue2, queue3);

  std::vector<EnqueueOrder> run_order;
  queue1->task_runner()->PostTask(FROM_HERE,
                                  BindOnce(&TestTask, 1, &run_order));
  queue2->task_runner()->PostTask(FROM_HERE,
                                  BindOnce(&TestTask, 2, &run_order));
  queue3->task_runner()->PostTask(FROM_HERE,
                                  BindOnce(&TestTask, 3, &run_order));
  queue2->ShutdownTaskQueue();
  RunLoop().RunUntilIdle();

  EXPECT_THAT(run_order, ElementsAre(1u, 3u));
}

TEST_P(SequenceManagerTest, ShutdownTaskQueue_WithDelayedTasks) {
  auto queues = CreateTaskQueues(2u);

  // Register three delayed tasks
  std::vector<EnqueueOrder> run_order;
  queues[0]->task_runner()->PostDelayedTask(FROM_HERE,
                                            BindOnce(&TestTask, 1, &run_order),
                                            TimeDelta::FromMilliseconds(10));
  queues[1]->task_runner()->PostDelayedTask(FROM_HERE,
                                            BindOnce(&TestTask, 2, &run_order),
                                            TimeDelta::FromMilliseconds(20));
  queues[0]->task_runner()->PostDelayedTask(FROM_HERE,
                                            BindOnce(&TestTask, 3, &run_order),
                                            TimeDelta::FromMilliseconds(30));

  queues[1]->ShutdownTaskQueue();
  RunLoop().RunUntilIdle();

  FastForwardBy(TimeDelta::FromMilliseconds(40));
  ASSERT_THAT(run_order, ElementsAre(1u, 3u));
}

namespace {
void ShutdownQueue(scoped_refptr<TestTaskQueue> queue) {
  queue->ShutdownTaskQueue();
}
}  // namespace

TEST_P(SequenceManagerTest, ShutdownTaskQueue_InTasks) {
  auto queues = CreateTaskQueues(3u);

  std::vector<EnqueueOrder> run_order;
  queues[0]->task_runner()->PostTask(FROM_HERE,
                                     BindOnce(&TestTask, 1, &run_order));
  queues[0]->task_runner()->PostTask(FROM_HERE,
                                     BindOnce(&ShutdownQueue, queues[1]));
  queues[0]->task_runner()->PostTask(FROM_HERE,
                                     BindOnce(&ShutdownQueue, queues[2]));
  queues[1]->task_runner()->PostTask(FROM_HERE,
                                     BindOnce(&TestTask, 2, &run_order));
  queues[2]->task_runner()->PostTask(FROM_HERE,
                                     BindOnce(&TestTask, 3, &run_order));

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

TEST_P(SequenceManagerTest, ShutdownTaskQueueInNestedLoop) {
  auto queue = CreateTaskQueue();

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
  queue->task_runner()->PostTask(
      FROM_HERE, BindOnce(&PostFromNestedRunloop, queue,
                          Unretained(&tasks_to_post_from_nested_loop)));
  RunLoop().RunUntilIdle();

  // Just make sure that we don't crash.
}

TEST_P(SequenceManagerTest, TimeDomainsAreIndependant) {
  auto queues = CreateTaskQueues(2u);

  TimeTicks start_time_ticks = sequence_manager()->NowTicks();
  std::unique_ptr<MockTimeDomain> domain_a =
      std::make_unique<MockTimeDomain>(start_time_ticks);
  std::unique_ptr<MockTimeDomain> domain_b =
      std::make_unique<MockTimeDomain>(start_time_ticks);
  sequence_manager()->RegisterTimeDomain(domain_a.get());
  sequence_manager()->RegisterTimeDomain(domain_b.get());
  queues[0]->SetTimeDomain(domain_a.get());
  queues[1]->SetTimeDomain(domain_b.get());

  std::vector<EnqueueOrder> run_order;
  queues[0]->task_runner()->PostDelayedTask(FROM_HERE,
                                            BindOnce(&TestTask, 1, &run_order),
                                            TimeDelta::FromMilliseconds(10));
  queues[0]->task_runner()->PostDelayedTask(FROM_HERE,
                                            BindOnce(&TestTask, 2, &run_order),
                                            TimeDelta::FromMilliseconds(20));
  queues[0]->task_runner()->PostDelayedTask(FROM_HERE,
                                            BindOnce(&TestTask, 3, &run_order),
                                            TimeDelta::FromMilliseconds(30));

  queues[1]->task_runner()->PostDelayedTask(FROM_HERE,
                                            BindOnce(&TestTask, 4, &run_order),
                                            TimeDelta::FromMilliseconds(10));
  queues[1]->task_runner()->PostDelayedTask(FROM_HERE,
                                            BindOnce(&TestTask, 5, &run_order),
                                            TimeDelta::FromMilliseconds(20));
  queues[1]->task_runner()->PostDelayedTask(FROM_HERE,
                                            BindOnce(&TestTask, 6, &run_order),
                                            TimeDelta::FromMilliseconds(30));

  domain_b->SetNowTicks(start_time_ticks + TimeDelta::FromMilliseconds(50));
  sequence_manager()->ScheduleWork();

  RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(4u, 5u, 6u));

  domain_a->SetNowTicks(start_time_ticks + TimeDelta::FromMilliseconds(50));
  sequence_manager()->ScheduleWork();

  RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(4u, 5u, 6u, 1u, 2u, 3u));

  queues[0]->ShutdownTaskQueue();
  queues[1]->ShutdownTaskQueue();

  sequence_manager()->UnregisterTimeDomain(domain_a.get());
  sequence_manager()->UnregisterTimeDomain(domain_b.get());
}

TEST_P(SequenceManagerTest, TimeDomainMigration) {
  auto queue = CreateTaskQueue();

  TimeTicks start_time_ticks = sequence_manager()->NowTicks();
  std::unique_ptr<MockTimeDomain> domain_a =
      std::make_unique<MockTimeDomain>(start_time_ticks);
  sequence_manager()->RegisterTimeDomain(domain_a.get());
  queue->SetTimeDomain(domain_a.get());

  std::vector<EnqueueOrder> run_order;
  queue->task_runner()->PostDelayedTask(FROM_HERE,
                                        BindOnce(&TestTask, 1, &run_order),
                                        TimeDelta::FromMilliseconds(10));
  queue->task_runner()->PostDelayedTask(FROM_HERE,
                                        BindOnce(&TestTask, 2, &run_order),
                                        TimeDelta::FromMilliseconds(20));
  queue->task_runner()->PostDelayedTask(FROM_HERE,
                                        BindOnce(&TestTask, 3, &run_order),
                                        TimeDelta::FromMilliseconds(30));
  queue->task_runner()->PostDelayedTask(FROM_HERE,
                                        BindOnce(&TestTask, 4, &run_order),
                                        TimeDelta::FromMilliseconds(40));

  domain_a->SetNowTicks(start_time_ticks + TimeDelta::FromMilliseconds(20));
  sequence_manager()->ScheduleWork();
  RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1u, 2u));

  std::unique_ptr<MockTimeDomain> domain_b =
      std::make_unique<MockTimeDomain>(start_time_ticks);
  sequence_manager()->RegisterTimeDomain(domain_b.get());
  queue->SetTimeDomain(domain_b.get());

  domain_b->SetNowTicks(start_time_ticks + TimeDelta::FromMilliseconds(50));
  sequence_manager()->ScheduleWork();

  RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1u, 2u, 3u, 4u));

  queue->ShutdownTaskQueue();

  sequence_manager()->UnregisterTimeDomain(domain_a.get());
  sequence_manager()->UnregisterTimeDomain(domain_b.get());
}

TEST_P(SequenceManagerTest, TimeDomainMigrationWithIncomingImmediateTasks) {
  auto queue = CreateTaskQueue();

  TimeTicks start_time_ticks = sequence_manager()->NowTicks();
  std::unique_ptr<MockTimeDomain> domain_a =
      std::make_unique<MockTimeDomain>(start_time_ticks);
  std::unique_ptr<MockTimeDomain> domain_b =
      std::make_unique<MockTimeDomain>(start_time_ticks);
  sequence_manager()->RegisterTimeDomain(domain_a.get());
  sequence_manager()->RegisterTimeDomain(domain_b.get());

  queue->SetTimeDomain(domain_a.get());
  std::vector<EnqueueOrder> run_order;
  queue->task_runner()->PostTask(FROM_HERE, BindOnce(&TestTask, 1, &run_order));
  queue->SetTimeDomain(domain_b.get());

  RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1u));

  queue->ShutdownTaskQueue();

  sequence_manager()->UnregisterTimeDomain(domain_a.get());
  sequence_manager()->UnregisterTimeDomain(domain_b.get());
}

TEST_P(SequenceManagerTest,
       PostDelayedTasksReverseOrderAlternatingTimeDomains) {
  auto queue = CreateTaskQueue();

  std::vector<EnqueueOrder> run_order;

  std::unique_ptr<internal::RealTimeDomain> domain_a =
      std::make_unique<internal::RealTimeDomain>();
  std::unique_ptr<internal::RealTimeDomain> domain_b =
      std::make_unique<internal::RealTimeDomain>();
  sequence_manager()->RegisterTimeDomain(domain_a.get());
  sequence_manager()->RegisterTimeDomain(domain_b.get());

  queue->SetTimeDomain(domain_a.get());
  queue->task_runner()->PostDelayedTask(FROM_HERE,
                                        BindOnce(&TestTask, 1, &run_order),
                                        TimeDelta::FromMilliseconds(40));

  queue->SetTimeDomain(domain_b.get());
  queue->task_runner()->PostDelayedTask(FROM_HERE,
                                        BindOnce(&TestTask, 2, &run_order),
                                        TimeDelta::FromMilliseconds(30));

  queue->SetTimeDomain(domain_a.get());
  queue->task_runner()->PostDelayedTask(FROM_HERE,
                                        BindOnce(&TestTask, 3, &run_order),
                                        TimeDelta::FromMilliseconds(20));

  queue->SetTimeDomain(domain_b.get());
  queue->task_runner()->PostDelayedTask(FROM_HERE,
                                        BindOnce(&TestTask, 4, &run_order),
                                        TimeDelta::FromMilliseconds(10));

  FastForwardBy(TimeDelta::FromMilliseconds(40));
  EXPECT_THAT(run_order, ElementsAre(4u, 3u, 2u, 1u));

  queue->ShutdownTaskQueue();

  sequence_manager()->UnregisterTimeDomain(domain_a.get());
  sequence_manager()->UnregisterTimeDomain(domain_b.get());
}

namespace {

class MockTaskQueueThrottler : public TaskQueue::Throttler {
 public:
  TaskQueue* task_queue;

  explicit MockTaskQueueThrottler(TaskQueue* task_queue)
      : task_queue(task_queue) {}
  ~MockTaskQueueThrottler() = default;

  MOCK_METHOD1(OnWakeUp, void(LazyNow*));
  MOCK_METHOD0(OnHasImmediateTask, void());

  MOCK_METHOD1(GetNextAllowedWakeUp_DesiredWakeUpTime, void(TimeTicks));

  absl::optional<DelayedWakeUp> GetNextAllowedWakeUp(
      LazyNow* lazy_now,
      absl::optional<DelayedWakeUp> next_desired_wake_up,
      bool has_immediate_work) override {
    if (next_desired_wake_up)
      GetNextAllowedWakeUp_DesiredWakeUpTime(next_desired_wake_up->time);
    if (next_allowed_wake_up_)
      return next_allowed_wake_up_;
    return next_desired_wake_up;
  }

  void SetNextAllowedWakeUp(
      absl::optional<DelayedWakeUp> next_allowed_wake_up) {
    next_allowed_wake_up_ = next_allowed_wake_up;
  }

 private:
  absl::optional<DelayedWakeUp> next_allowed_wake_up_;
};

}  // namespace

TEST_P(SequenceManagerTest, TaskQueueThrottler_ImmediateTask) {
  auto queue = CreateTaskQueue();

  StrictMock<MockTaskQueueThrottler> throttler(queue.get());
  queue->SetThrottler(&throttler);

  // OnHasImmediateTask should be called when a task is posted on an empty
  // queue.
  EXPECT_CALL(throttler, OnHasImmediateTask());
  queue->task_runner()->PostTask(FROM_HERE, BindOnce(&NopTask));
  sequence_manager()->ReloadEmptyWorkQueues();
  Mock::VerifyAndClearExpectations(&throttler);

  // But not subsequently.
  queue->task_runner()->PostTask(FROM_HERE, BindOnce(&NopTask));
  sequence_manager()->ReloadEmptyWorkQueues();
  Mock::VerifyAndClearExpectations(&throttler);

  // Unless the immediate work queue is emptied.
  sequence_manager()->SelectNextTask();
  sequence_manager()->DidRunTask();
  sequence_manager()->SelectNextTask();
  sequence_manager()->DidRunTask();
  EXPECT_CALL(throttler, OnHasImmediateTask());
  queue->task_runner()->PostTask(FROM_HERE, BindOnce(&NopTask));
  sequence_manager()->ReloadEmptyWorkQueues();
  Mock::VerifyAndClearExpectations(&throttler);

  // Tidy up.
  queue->ShutdownTaskQueue();
}

TEST_P(SequenceManagerTest, TaskQueueThrottler_DelayedTask) {
  auto queue = CreateTaskQueue();

  TimeTicks start_time = sequence_manager()->NowTicks();
  TimeDelta delay10s(TimeDelta::FromSeconds(10));
  TimeDelta delay100s(TimeDelta::FromSeconds(100));
  TimeDelta delay1s(TimeDelta::FromSeconds(1));

  StrictMock<MockTaskQueueThrottler> throttler(queue.get());
  queue->SetThrottler(&throttler);

  // GetNextAllowedWakeUp should be called when a delayed task is posted on an
  // empty queue.
  EXPECT_CALL(throttler,
              GetNextAllowedWakeUp_DesiredWakeUpTime(start_time + delay10s));
  queue->task_runner()->PostDelayedTask(FROM_HERE, BindOnce(&NopTask),
                                        delay10s);
  Mock::VerifyAndClearExpectations(&throttler);

  // GetNextAllowedWakeUp should be given the same delay when a longer delay
  // task is posted.
  EXPECT_CALL(throttler,
              GetNextAllowedWakeUp_DesiredWakeUpTime(start_time + delay10s));
  queue->task_runner()->PostDelayedTask(FROM_HERE, BindOnce(&NopTask),
                                        delay100s);
  Mock::VerifyAndClearExpectations(&throttler);

  // GetNextAllowedWakeUp should be given the new delay when a task is posted
  // with a shorter delay.
  EXPECT_CALL(throttler,
              GetNextAllowedWakeUp_DesiredWakeUpTime(start_time + delay1s));
  queue->task_runner()->PostDelayedTask(FROM_HERE, BindOnce(&NopTask), delay1s);
  Mock::VerifyAndClearExpectations(&throttler);

  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter =
      queue->CreateQueueEnabledVoter();
  voter->SetVoteToEnable(false);
  Mock::VerifyAndClearExpectations(&throttler);

  // When a queue has been enabled, we may get a notification if the
  // TimeDomain's next scheduled wake-up has changed.
  EXPECT_CALL(throttler,
              GetNextAllowedWakeUp_DesiredWakeUpTime(start_time + delay1s));
  voter->SetVoteToEnable(true);
  Mock::VerifyAndClearExpectations(&throttler);

  // Tidy up.
  queue->ShutdownTaskQueue();
}

TEST_P(SequenceManagerTest, TaskQueueThrottler_OnWakeUp) {
  auto queue = CreateTaskQueue();

  TimeTicks start_time = sequence_manager()->NowTicks();
  TimeDelta delay(TimeDelta::FromSeconds(1));

  StrictMock<MockTaskQueueThrottler> throttler(queue.get());
  queue->SetThrottler(&throttler);

  EXPECT_CALL(throttler,
              GetNextAllowedWakeUp_DesiredWakeUpTime(start_time + delay));
  queue->task_runner()->PostDelayedTask(FROM_HERE, BindOnce(&NopTask), delay);
  Mock::VerifyAndClearExpectations(&throttler);

  AdvanceMockTickClock(delay);

  // OnWakeUp should be called when the queue has a scheduler wake up.
  EXPECT_CALL(throttler, OnWakeUp(_));
  // Move the task into the |delayed_work_queue|.
  LazyNow lazy_now(mock_tick_clock());
  sequence_manager()->MoveReadyDelayedTasksToWorkQueues(&lazy_now);
  Mock::VerifyAndClearExpectations(&throttler);

  // Tidy up.
  queue->ShutdownTaskQueue();
}

TEST_P(SequenceManagerTest, TaskQueueThrottler_ResetThrottler) {
  auto queue = CreateTaskQueue();

  TimeTicks start_time = sequence_manager()->NowTicks();
  TimeDelta delay10s(TimeDelta::FromSeconds(10));
  TimeDelta delay1s(TimeDelta::FromSeconds(1));

  StrictMock<MockTaskQueueThrottler> throttler(queue.get());
  queue->SetThrottler(&throttler);
  EXPECT_FALSE(queue->GetNextDesiredWakeUp());

  // GetNextAllowedWakeUp should be called when a delayed task is posted on an
  // empty queue.
  throttler.SetNextAllowedWakeUp(
      base::sequence_manager::DelayedWakeUp{start_time + delay10s});
  EXPECT_CALL(throttler,
              GetNextAllowedWakeUp_DesiredWakeUpTime(start_time + delay1s));
  queue->task_runner()->PostDelayedTask(FROM_HERE, BindOnce(&NopTask), delay1s);
  Mock::VerifyAndClearExpectations(&throttler);
  // Expect throttled wake up.
  LazyNow lazy_now(mock_tick_clock());
  EXPECT_EQ(
      delay10s,
      sequence_manager()->GetRealTimeDomain()->DelayTillNextTask(&lazy_now));

  queue->ResetThrottler();
  // Next wake up should be back to normal.
  EXPECT_EQ(delay1s, sequence_manager()->GetRealTimeDomain()->DelayTillNextTask(
                         &lazy_now));

  // Tidy up.
  queue->ShutdownTaskQueue();
}

TEST_P(SequenceManagerTest, TaskQueueThrottler_DelayedTaskMultipleQueues) {
  auto queues = CreateTaskQueues(2u);

  StrictMock<MockTaskQueueThrottler> throttler0(queues[0].get());
  StrictMock<MockTaskQueueThrottler> throttler1(queues[1].get());
  queues[0]->SetThrottler(&throttler0);
  queues[1]->SetThrottler(&throttler1);

  TimeTicks start_time = sequence_manager()->NowTicks();
  TimeDelta delay1s(TimeDelta::FromSeconds(1));
  TimeDelta delay10s(TimeDelta::FromSeconds(10));

  EXPECT_CALL(throttler0,
              GetNextAllowedWakeUp_DesiredWakeUpTime(start_time + delay1s))
      .Times(1);
  EXPECT_CALL(throttler1,
              GetNextAllowedWakeUp_DesiredWakeUpTime(start_time + delay10s))
      .Times(1);
  queues[0]->task_runner()->PostDelayedTask(FROM_HERE, BindOnce(&NopTask),
                                            delay1s);
  queues[1]->task_runner()->PostDelayedTask(FROM_HERE, BindOnce(&NopTask),
                                            delay10s);
  testing::Mock::VerifyAndClearExpectations(&throttler0);
  testing::Mock::VerifyAndClearExpectations(&throttler1);

  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter0 =
      queues[0]->CreateQueueEnabledVoter();
  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter1 =
      queues[1]->CreateQueueEnabledVoter();

  // Disabling a queue should not trigger a notification.
  voter0->SetVoteToEnable(false);
  Mock::VerifyAndClearExpectations(&throttler0);

  // But re-enabling it should should trigger an GetNextAllowedWakeUp
  // notification.
  EXPECT_CALL(throttler0,
              GetNextAllowedWakeUp_DesiredWakeUpTime(start_time + delay1s));
  voter0->SetVoteToEnable(true);
  Mock::VerifyAndClearExpectations(&throttler0);

  // Disabling a queue should not trigger a notification.
  voter1->SetVoteToEnable(false);
  Mock::VerifyAndClearExpectations(&throttler0);

  // But re-enabling it should should trigger a notification.
  EXPECT_CALL(throttler1,
              GetNextAllowedWakeUp_DesiredWakeUpTime(start_time + delay10s));
  voter1->SetVoteToEnable(true);
  Mock::VerifyAndClearExpectations(&throttler1);

  // Tidy up.
  queues[0]->ShutdownTaskQueue();
  queues[1]->ShutdownTaskQueue();
}

TEST_P(SequenceManagerTest, TaskQueueThrottler_DelayedWorkWhichCanRunNow) {
  // This test checks that when delayed work becomes available
  // the notification still fires. This usually happens when time advances
  // and task becomes available in the middle of the scheduling code.
  // For this test we rely on the fact that notification dispatching code
  // is the same in all conditions and just change a time domain to
  // trigger notification.

  auto queue = CreateTaskQueue();

  TimeDelta delay1s(TimeDelta::FromSeconds(1));
  TimeDelta delay10s(TimeDelta::FromSeconds(10));

  StrictMock<MockTaskQueueThrottler> throttler(queue.get());
  queue->SetThrottler(&throttler);

  // GetNextAllowedWakeUp should be called when a delayed task is posted on an
  // empty queue.
  EXPECT_CALL(throttler, GetNextAllowedWakeUp_DesiredWakeUpTime(_));
  queue->task_runner()->PostDelayedTask(FROM_HERE, BindOnce(&NopTask), delay1s);
  Mock::VerifyAndClearExpectations(&throttler);

  std::unique_ptr<TimeDomain> mock_time_domain =
      std::make_unique<internal::RealTimeDomain>();
  sequence_manager()->RegisterTimeDomain(mock_time_domain.get());

  AdvanceMockTickClock(delay10s);

  EXPECT_CALL(throttler, GetNextAllowedWakeUp_DesiredWakeUpTime(_));
  queue->SetTimeDomain(mock_time_domain.get());
  Mock::VerifyAndClearExpectations(&throttler);

  // Tidy up.
  queue->ShutdownTaskQueue();
}

namespace {

class CancelableTask {
 public:
  explicit CancelableTask(const TickClock* clock) : clock_(clock) {}

  void RecordTimeTask(std::vector<TimeTicks>* run_times) {
    run_times->push_back(clock_->NowTicks());
  }

  template <typename... Args>
  void FailTask(Args...) {
    FAIL();
  }

  const TickClock* clock_;
  WeakPtrFactory<CancelableTask> weak_factory_{this};
};

class DestructionCallback {
 public:
  explicit DestructionCallback(OnceCallback<void()> on_destroy)
      : on_destroy_(std::move(on_destroy)) {}
  ~DestructionCallback() {
    if (on_destroy_)
      std::move(on_destroy_).Run();
  }
  DestructionCallback(const DestructionCallback&) = delete;
  DestructionCallback& operator=(const DestructionCallback&) = delete;
  DestructionCallback(DestructionCallback&&) = default;

 private:
  OnceCallback<void()> on_destroy_;
};

}  // namespace

TEST_P(SequenceManagerTest, TaskQueueThrottler_SweepCanceledDelayedTasks) {
  auto queue = CreateTaskQueue();

  StrictMock<MockTaskQueueThrottler> throttler(queue.get());
  queue->SetThrottler(&throttler);

  TimeTicks start_time = sequence_manager()->NowTicks();
  TimeDelta delay1(TimeDelta::FromSeconds(5));
  TimeDelta delay2(TimeDelta::FromSeconds(10));

  EXPECT_CALL(throttler,
              GetNextAllowedWakeUp_DesiredWakeUpTime(start_time + delay1))
      .Times(2);

  CancelableTask task1(mock_tick_clock());
  CancelableTask task2(mock_tick_clock());
  std::vector<TimeTicks> run_times;
  queue->task_runner()->PostDelayedTask(
      FROM_HERE,
      BindOnce(&CancelableTask::RecordTimeTask,
               task1.weak_factory_.GetWeakPtr(), &run_times),
      delay1);
  queue->task_runner()->PostDelayedTask(
      FROM_HERE,
      BindOnce(&CancelableTask::RecordTimeTask,
               task2.weak_factory_.GetWeakPtr(), &run_times),
      delay2);

  task1.weak_factory_.InvalidateWeakPtrs();

  // Sweeping away canceled delayed tasks should trigger a notification.
  EXPECT_CALL(throttler,
              GetNextAllowedWakeUp_DesiredWakeUpTime(start_time + delay2))
      .Times(1);
  sequence_manager()->ReclaimMemory();
}

TEST_P(SequenceManagerTest, SweepLastTaskInQueue) {
  auto queue = CreateTaskQueue();
  CancelableTask task(mock_tick_clock());
  queue->task_runner()->PostDelayedTask(
      FROM_HERE,
      BindOnce(&CancelableTask::FailTask<>, task.weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromSeconds(1));

  // Make sure sweeping away the last task in the queue doesn't end up accessing
  // invalid iterators.
  task.weak_factory_.InvalidateWeakPtrs();
  sequence_manager()->ReclaimMemory();
}

TEST_P(SequenceManagerTest, CancelledTaskPostAnother) {
  // This check ensures that a task whose destruction causes another task to be
  // posted as a side-effect doesn't cause us to access invalid iterators while
  // sweeping away cancelled tasks.
  auto queue = CreateTaskQueue();
  bool did_post = false;
  auto on_destroy = BindLambdaForTesting([&] {
    queue->task_runner()->PostDelayedTask(FROM_HERE,
                                          BindLambdaForTesting([] {}),
                                          base::TimeDelta::FromSeconds(1));
    did_post = true;
  });

  DestructionCallback destruction_observer(std::move(on_destroy));
  CancelableTask task(mock_tick_clock());
  queue->task_runner()->PostDelayedTask(
      FROM_HERE,
      BindOnce(&CancelableTask::FailTask<DestructionCallback>,
               task.weak_factory_.GetWeakPtr(),
               std::move(destruction_observer)),
      base::TimeDelta::FromSeconds(1));

  task.weak_factory_.InvalidateWeakPtrs();
  EXPECT_FALSE(did_post);
  sequence_manager()->ReclaimMemory();
  EXPECT_TRUE(did_post);
}

TEST_P(SequenceManagerTest, CancelledImmediateTaskShutsDownQueue) {
  // This check ensures that an immediate task whose destruction causes the
  // owning task queue to be shut down doesn't cause us to access freed memory.
  auto queue = CreateTaskQueue();
  bool did_shutdown = false;
  auto on_destroy = BindLambdaForTesting([&] {
    queue->ShutdownTaskQueue();
    did_shutdown = true;
  });

  DestructionCallback destruction_observer(std::move(on_destroy));
  CancelableTask task(mock_tick_clock());
  queue->task_runner()->PostTask(
      FROM_HERE, BindOnce(&CancelableTask::FailTask<DestructionCallback>,
                          task.weak_factory_.GetWeakPtr(),
                          std::move(destruction_observer)));

  task.weak_factory_.InvalidateWeakPtrs();
  EXPECT_FALSE(did_shutdown);
  RunLoop().RunUntilIdle();
  EXPECT_TRUE(did_shutdown);
}

TEST_P(SequenceManagerTest, CancelledDelayedTaskShutsDownQueue) {
  // This check ensures that a delayed task whose destruction causes the owning
  // task queue to be shut down doesn't cause us to access freed memory.
  auto queue = CreateTaskQueue();
  bool did_shutdown = false;
  auto on_destroy = BindLambdaForTesting([&] {
    queue->ShutdownTaskQueue();
    did_shutdown = true;
  });

  DestructionCallback destruction_observer(std::move(on_destroy));
  CancelableTask task(mock_tick_clock());
  queue->task_runner()->PostDelayedTask(
      FROM_HERE,
      BindOnce(&CancelableTask::FailTask<DestructionCallback>,
               task.weak_factory_.GetWeakPtr(),
               std::move(destruction_observer)),
      base::TimeDelta::FromSeconds(1));

  task.weak_factory_.InvalidateWeakPtrs();
  EXPECT_FALSE(did_shutdown);
  sequence_manager()->ReclaimMemory();
  EXPECT_TRUE(did_shutdown);
}

namespace {

void ChromiumRunloopInspectionTask(
    scoped_refptr<TestMockTimeTaskRunner> test_task_runner) {
  // We don't expect more than 1 pending task at any time.
  EXPECT_GE(1u, test_task_runner->GetPendingTaskCount());
}

}  // namespace

TEST(SequenceManagerTestWithMockTaskRunner,
     NumberOfPendingTasksOnChromiumRunLoop) {
  FixtureWithMockTaskRunner fixture;
  auto queue =
      fixture.sequence_manager()->CreateTaskQueue(TaskQueue::Spec("test"));

  // NOTE because tasks posted to the chromiumrun loop are not cancellable, we
  // will end up with a lot more tasks posted if the delayed tasks were posted
  // in the reverse order.
  // TODO(alexclarke): Consider talking to the message pump directly.
  for (int i = 1; i < 100; i++) {
    queue->task_runner()->PostDelayedTask(
        FROM_HERE,
        BindOnce(&ChromiumRunloopInspectionTask, fixture.test_task_runner()),
        TimeDelta::FromMilliseconds(i));
  }
  fixture.FastForwardUntilNoTasksRemain();
}

namespace {

class QuadraticTask {
 public:
  QuadraticTask(scoped_refptr<TaskRunner> task_runner,
                TimeDelta delay,
                Fixture* fixture)
      : count_(0),
        task_runner_(task_runner),
        delay_(delay),
        fixture_(fixture) {}

  void SetShouldExit(RepeatingCallback<bool()> should_exit) {
    should_exit_ = should_exit;
  }

  void Run() {
    if (should_exit_.Run())
      return;
    count_++;
    task_runner_->PostDelayedTask(
        FROM_HERE, BindOnce(&QuadraticTask::Run, Unretained(this)), delay_);
    task_runner_->PostDelayedTask(
        FROM_HERE, BindOnce(&QuadraticTask::Run, Unretained(this)), delay_);
    fixture_->AdvanceMockTickClock(TimeDelta::FromMilliseconds(5));
  }

  int Count() const { return count_; }

 private:
  int count_;
  scoped_refptr<TaskRunner> task_runner_;
  TimeDelta delay_;
  Fixture* fixture_;
  RepeatingCallback<bool()> should_exit_;
};

class LinearTask {
 public:
  LinearTask(scoped_refptr<TaskRunner> task_runner,
             TimeDelta delay,
             Fixture* fixture)
      : count_(0),
        task_runner_(task_runner),
        delay_(delay),
        fixture_(fixture) {}

  void SetShouldExit(RepeatingCallback<bool()> should_exit) {
    should_exit_ = should_exit;
  }

  void Run() {
    if (should_exit_.Run())
      return;
    count_++;
    task_runner_->PostDelayedTask(
        FROM_HERE, BindOnce(&LinearTask::Run, Unretained(this)), delay_);
    fixture_->AdvanceMockTickClock(TimeDelta::FromMilliseconds(5));
  }

  int Count() const { return count_; }

 private:
  int count_;
  scoped_refptr<TaskRunner> task_runner_;
  TimeDelta delay_;
  Fixture* fixture_;
  RepeatingCallback<bool()> should_exit_;
};

bool ShouldExit(QuadraticTask* quadratic_task, LinearTask* linear_task) {
  return quadratic_task->Count() == 1000 || linear_task->Count() == 1000;
}

}  // namespace

TEST_P(SequenceManagerTest,
       DelayedTasksDontBadlyStarveNonDelayedWork_SameQueue) {
  auto queue = CreateTaskQueue();

  QuadraticTask quadratic_delayed_task(queue->task_runner(),
                                       TimeDelta::FromMilliseconds(10), this);
  LinearTask linear_immediate_task(queue->task_runner(), TimeDelta(), this);
  RepeatingCallback<bool()> should_exit = BindRepeating(
      ShouldExit, &quadratic_delayed_task, &linear_immediate_task);
  quadratic_delayed_task.SetShouldExit(should_exit);
  linear_immediate_task.SetShouldExit(should_exit);

  quadratic_delayed_task.Run();
  linear_immediate_task.Run();

  FastForwardUntilNoTasksRemain();

  double ratio = static_cast<double>(linear_immediate_task.Count()) /
                 static_cast<double>(quadratic_delayed_task.Count());

  EXPECT_GT(ratio, 0.333);
  EXPECT_LT(ratio, 1.1);
}

TEST_P(SequenceManagerTest, ImmediateWorkCanStarveDelayedTasks_SameQueue) {
  auto queue = CreateTaskQueue();

  QuadraticTask quadratic_immediate_task(queue->task_runner(), TimeDelta(),
                                         this);
  LinearTask linear_delayed_task(queue->task_runner(),
                                 TimeDelta::FromMilliseconds(10), this);
  RepeatingCallback<bool()> should_exit = BindRepeating(
      &ShouldExit, &quadratic_immediate_task, &linear_delayed_task);

  quadratic_immediate_task.SetShouldExit(should_exit);
  linear_delayed_task.SetShouldExit(should_exit);

  quadratic_immediate_task.Run();
  linear_delayed_task.Run();

  FastForwardUntilNoTasksRemain();

  double ratio = static_cast<double>(linear_delayed_task.Count()) /
                 static_cast<double>(quadratic_immediate_task.Count());

  // This is by design, we want to enforce a strict ordering in task execution
  // where by delayed tasks can not skip ahead of non-delayed work.
  EXPECT_GT(ratio, 0.0);
  EXPECT_LT(ratio, 0.1);
}

TEST_P(SequenceManagerTest,
       DelayedTasksDontBadlyStarveNonDelayedWork_DifferentQueue) {
  auto queues = CreateTaskQueues(2u);

  QuadraticTask quadratic_delayed_task(queues[0]->task_runner(),
                                       TimeDelta::FromMilliseconds(10), this);
  LinearTask linear_immediate_task(queues[1]->task_runner(), TimeDelta(), this);
  RepeatingCallback<bool()> should_exit = BindRepeating(
      ShouldExit, &quadratic_delayed_task, &linear_immediate_task);
  quadratic_delayed_task.SetShouldExit(should_exit);
  linear_immediate_task.SetShouldExit(should_exit);

  quadratic_delayed_task.Run();
  linear_immediate_task.Run();

  FastForwardUntilNoTasksRemain();

  double ratio = static_cast<double>(linear_immediate_task.Count()) /
                 static_cast<double>(quadratic_delayed_task.Count());

  EXPECT_GT(ratio, 0.333);
  EXPECT_LT(ratio, 1.1);
}

TEST_P(SequenceManagerTest, ImmediateWorkCanStarveDelayedTasks_DifferentQueue) {
  auto queues = CreateTaskQueues(2u);

  QuadraticTask quadratic_immediate_task(queues[0]->task_runner(), TimeDelta(),
                                         this);
  LinearTask linear_delayed_task(queues[1]->task_runner(),
                                 TimeDelta::FromMilliseconds(10), this);
  RepeatingCallback<bool()> should_exit = BindRepeating(
      &ShouldExit, &quadratic_immediate_task, &linear_delayed_task);

  quadratic_immediate_task.SetShouldExit(should_exit);
  linear_delayed_task.SetShouldExit(should_exit);

  quadratic_immediate_task.Run();
  linear_delayed_task.Run();

  FastForwardUntilNoTasksRemain();

  double ratio = static_cast<double>(linear_delayed_task.Count()) /
                 static_cast<double>(quadratic_immediate_task.Count());

  // This is by design, we want to enforce a strict ordering in task execution
  // where by delayed tasks can not skip ahead of non-delayed work.
  EXPECT_GT(ratio, 0.0);
  EXPECT_LT(ratio, 0.1);
}

TEST_P(SequenceManagerTest, CurrentlyExecutingTaskQueue_NoTaskRunning) {
  auto queue = CreateTaskQueue();

  EXPECT_EQ(nullptr, sequence_manager()->currently_executing_task_queue());
}

namespace {
void CurrentlyExecutingTaskQueueTestTask(
    SequenceManagerImpl* sequence_manager,
    std::vector<internal::TaskQueueImpl*>* task_sources) {
  task_sources->push_back(sequence_manager->currently_executing_task_queue());
}
}  // namespace

TEST_P(SequenceManagerTest, CurrentlyExecutingTaskQueue_TaskRunning) {
  auto queues = CreateTaskQueues(2u);

  TestTaskQueue* queue0 = queues[0].get();
  TestTaskQueue* queue1 = queues[1].get();

  std::vector<internal::TaskQueueImpl*> task_sources;
  queue0->task_runner()->PostTask(FROM_HERE,
                                  BindOnce(&CurrentlyExecutingTaskQueueTestTask,
                                           sequence_manager(), &task_sources));
  queue1->task_runner()->PostTask(FROM_HERE,
                                  BindOnce(&CurrentlyExecutingTaskQueueTestTask,
                                           sequence_manager(), &task_sources));
  RunLoop().RunUntilIdle();

  EXPECT_THAT(task_sources, ElementsAre(queue0->GetTaskQueueImpl(),
                                        queue1->GetTaskQueueImpl()));
  EXPECT_EQ(nullptr, sequence_manager()->currently_executing_task_queue());
}

namespace {
void RunloopCurrentlyExecutingTaskQueueTestTask(
    SequenceManagerImpl* sequence_manager,
    std::vector<internal::TaskQueueImpl*>* task_sources,
    std::vector<std::pair<OnceClosure, TestTaskQueue*>>* tasks) {
  task_sources->push_back(sequence_manager->currently_executing_task_queue());

  for (std::pair<OnceClosure, TestTaskQueue*>& pair : *tasks) {
    pair.second->task_runner()->PostTask(FROM_HERE, std::move(pair.first));
  }

  RunLoop(RunLoop::Type::kNestableTasksAllowed).RunUntilIdle();
  task_sources->push_back(sequence_manager->currently_executing_task_queue());
}
}  // namespace

TEST_P(SequenceManagerTest, CurrentlyExecutingTaskQueue_NestedLoop) {
  auto queues = CreateTaskQueues(3u);

  TestTaskQueue* queue0 = queues[0].get();
  TestTaskQueue* queue1 = queues[1].get();
  TestTaskQueue* queue2 = queues[2].get();

  std::vector<internal::TaskQueueImpl*> task_sources;
  std::vector<std::pair<OnceClosure, TestTaskQueue*>>
      tasks_to_post_from_nested_loop;
  tasks_to_post_from_nested_loop.push_back(
      std::make_pair(BindOnce(&CurrentlyExecutingTaskQueueTestTask,
                              sequence_manager(), &task_sources),
                     queue1));
  tasks_to_post_from_nested_loop.push_back(
      std::make_pair(BindOnce(&CurrentlyExecutingTaskQueueTestTask,
                              sequence_manager(), &task_sources),
                     queue2));

  queue0->task_runner()->PostTask(
      FROM_HERE,
      BindOnce(&RunloopCurrentlyExecutingTaskQueueTestTask, sequence_manager(),
               &task_sources, &tasks_to_post_from_nested_loop));

  RunLoop().RunUntilIdle();
  EXPECT_THAT(task_sources, UnorderedElementsAre(queue0->GetTaskQueueImpl(),
                                                 queue1->GetTaskQueueImpl(),
                                                 queue2->GetTaskQueueImpl(),
                                                 queue0->GetTaskQueueImpl()));
  EXPECT_EQ(nullptr, sequence_manager()->currently_executing_task_queue());
}

#if BUILDFLAG(ENABLE_BASE_TRACING)
TEST_P(SequenceManagerTest, BlameContextAttribution) {
  if (GetUnderlyingRunnerType() == TestType::kMessagePump)
    return;
  using trace_analyzer::Query;

  auto queue = CreateTaskQueue();

  trace_analyzer::Start("*");
  {
    trace_event::BlameContext blame_context("base", "name", "type", "scope", 0,
                                            nullptr);
    blame_context.Initialize();
    queue->SetBlameContext(&blame_context);
    queue->task_runner()->PostTask(FROM_HERE, BindOnce(&NopTask));
    RunLoop().RunUntilIdle();
  }
  auto analyzer = trace_analyzer::Stop();

  trace_analyzer::TraceEventVector events;
  Query q = Query::EventPhaseIs(TRACE_EVENT_PHASE_ENTER_CONTEXT) ||
            Query::EventPhaseIs(TRACE_EVENT_PHASE_LEAVE_CONTEXT);
  analyzer->FindEvents(q, &events);

  EXPECT_EQ(2u, events.size());
}
#endif  // BUILDFLAG(ENABLE_BASE_TRACING)

TEST_P(SequenceManagerTest, NoWakeUpsForCanceledDelayedTasks) {
  auto queue = CreateTaskQueue();

  TimeTicks start_time = sequence_manager()->NowTicks();

  CancelableTask task1(mock_tick_clock());
  CancelableTask task2(mock_tick_clock());
  CancelableTask task3(mock_tick_clock());
  CancelableTask task4(mock_tick_clock());
  TimeDelta delay1(TimeDelta::FromSeconds(5));
  TimeDelta delay2(TimeDelta::FromSeconds(10));
  TimeDelta delay3(TimeDelta::FromSeconds(15));
  TimeDelta delay4(TimeDelta::FromSeconds(30));
  std::vector<TimeTicks> run_times;
  queue->task_runner()->PostDelayedTask(
      FROM_HERE,
      BindOnce(&CancelableTask::RecordTimeTask,
               task1.weak_factory_.GetWeakPtr(), &run_times),
      delay1);
  queue->task_runner()->PostDelayedTask(
      FROM_HERE,
      BindOnce(&CancelableTask::RecordTimeTask,
               task2.weak_factory_.GetWeakPtr(), &run_times),
      delay2);
  queue->task_runner()->PostDelayedTask(
      FROM_HERE,
      BindOnce(&CancelableTask::RecordTimeTask,
               task3.weak_factory_.GetWeakPtr(), &run_times),
      delay3);
  queue->task_runner()->PostDelayedTask(
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
      &wake_up_times, mock_tick_clock()));

  EXPECT_THAT(wake_up_times,
              ElementsAre(start_time + delay1, start_time + delay4));
  EXPECT_THAT(run_times, ElementsAre(start_time + delay1, start_time + delay4));
}

TEST_P(SequenceManagerTest, NoWakeUpsForCanceledDelayedTasksReversePostOrder) {
  auto queue = CreateTaskQueue();

  TimeTicks start_time = sequence_manager()->NowTicks();

  CancelableTask task1(mock_tick_clock());
  CancelableTask task2(mock_tick_clock());
  CancelableTask task3(mock_tick_clock());
  CancelableTask task4(mock_tick_clock());
  TimeDelta delay1(TimeDelta::FromSeconds(5));
  TimeDelta delay2(TimeDelta::FromSeconds(10));
  TimeDelta delay3(TimeDelta::FromSeconds(15));
  TimeDelta delay4(TimeDelta::FromSeconds(30));
  std::vector<TimeTicks> run_times;
  queue->task_runner()->PostDelayedTask(
      FROM_HERE,
      BindOnce(&CancelableTask::RecordTimeTask,
               task4.weak_factory_.GetWeakPtr(), &run_times),
      delay4);
  queue->task_runner()->PostDelayedTask(
      FROM_HERE,
      BindOnce(&CancelableTask::RecordTimeTask,
               task3.weak_factory_.GetWeakPtr(), &run_times),
      delay3);
  queue->task_runner()->PostDelayedTask(
      FROM_HERE,
      BindOnce(&CancelableTask::RecordTimeTask,
               task2.weak_factory_.GetWeakPtr(), &run_times),
      delay2);
  queue->task_runner()->PostDelayedTask(
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
      &wake_up_times, mock_tick_clock()));

  EXPECT_THAT(wake_up_times,
              ElementsAre(start_time + delay1, start_time + delay4));
  EXPECT_THAT(run_times, ElementsAre(start_time + delay1, start_time + delay4));
}

TEST_P(SequenceManagerTest, TimeDomainWakeUpOnlyCancelledIfAllUsesCancelled) {
  auto queue = CreateTaskQueue();

  TimeTicks start_time = sequence_manager()->NowTicks();

  CancelableTask task1(mock_tick_clock());
  CancelableTask task2(mock_tick_clock());
  CancelableTask task3(mock_tick_clock());
  CancelableTask task4(mock_tick_clock());
  TimeDelta delay1(TimeDelta::FromSeconds(5));
  TimeDelta delay2(TimeDelta::FromSeconds(10));
  TimeDelta delay3(TimeDelta::FromSeconds(15));
  TimeDelta delay4(TimeDelta::FromSeconds(30));
  std::vector<TimeTicks> run_times;
  queue->task_runner()->PostDelayedTask(
      FROM_HERE,
      BindOnce(&CancelableTask::RecordTimeTask,
               task1.weak_factory_.GetWeakPtr(), &run_times),
      delay1);
  queue->task_runner()->PostDelayedTask(
      FROM_HERE,
      BindOnce(&CancelableTask::RecordTimeTask,
               task2.weak_factory_.GetWeakPtr(), &run_times),
      delay2);
  queue->task_runner()->PostDelayedTask(
      FROM_HERE,
      BindOnce(&CancelableTask::RecordTimeTask,
               task3.weak_factory_.GetWeakPtr(), &run_times),
      delay3);
  queue->task_runner()->PostDelayedTask(
      FROM_HERE,
      BindOnce(&CancelableTask::RecordTimeTask,
               task4.weak_factory_.GetWeakPtr(), &run_times),
      delay4);

  // Post a non-canceled task with |delay3|. So we should still get a wake-up at
  // |delay3| even though we cancel |task3|.
  queue->task_runner()->PostDelayedTask(
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
      &wake_up_times, mock_tick_clock()));

  EXPECT_THAT(wake_up_times,
              ElementsAre(start_time + delay1, start_time + delay3,
                          start_time + delay4));

  EXPECT_THAT(run_times, ElementsAre(start_time + delay3, start_time + delay4));
}

TEST_P(SequenceManagerTest, SweepCanceledDelayedTasks) {
  auto queue = CreateTaskQueue();

  CancelableTask task1(mock_tick_clock());
  CancelableTask task2(mock_tick_clock());
  CancelableTask task3(mock_tick_clock());
  CancelableTask task4(mock_tick_clock());
  TimeDelta delay1(TimeDelta::FromSeconds(5));
  TimeDelta delay2(TimeDelta::FromSeconds(10));
  TimeDelta delay3(TimeDelta::FromSeconds(15));
  TimeDelta delay4(TimeDelta::FromSeconds(30));
  std::vector<TimeTicks> run_times;
  queue->task_runner()->PostDelayedTask(
      FROM_HERE,
      BindOnce(&CancelableTask::RecordTimeTask,
               task1.weak_factory_.GetWeakPtr(), &run_times),
      delay1);
  queue->task_runner()->PostDelayedTask(
      FROM_HERE,
      BindOnce(&CancelableTask::RecordTimeTask,
               task2.weak_factory_.GetWeakPtr(), &run_times),
      delay2);
  queue->task_runner()->PostDelayedTask(
      FROM_HERE,
      BindOnce(&CancelableTask::RecordTimeTask,
               task3.weak_factory_.GetWeakPtr(), &run_times),
      delay3);
  queue->task_runner()->PostDelayedTask(
      FROM_HERE,
      BindOnce(&CancelableTask::RecordTimeTask,
               task4.weak_factory_.GetWeakPtr(), &run_times),
      delay4);

  EXPECT_EQ(4u, queue->GetNumberOfPendingTasks());
  task2.weak_factory_.InvalidateWeakPtrs();
  task3.weak_factory_.InvalidateWeakPtrs();
  EXPECT_EQ(4u, queue->GetNumberOfPendingTasks());

  sequence_manager()->ReclaimMemory();
  EXPECT_EQ(2u, queue->GetNumberOfPendingTasks());

  task1.weak_factory_.InvalidateWeakPtrs();
  task4.weak_factory_.InvalidateWeakPtrs();

  sequence_manager()->ReclaimMemory();
  EXPECT_EQ(0u, queue->GetNumberOfPendingTasks());
}

TEST_P(SequenceManagerTest, SweepCanceledDelayedTasks_ManyTasks) {
  auto queue = CreateTaskQueue();

  TimeTicks start_time = sequence_manager()->NowTicks();

  constexpr const int kNumTasks = 100;

  std::vector<std::unique_ptr<CancelableTask>> tasks(100);
  std::vector<TimeTicks> run_times;
  for (int i = 0; i < kNumTasks; i++) {
    tasks[i] = std::make_unique<CancelableTask>(mock_tick_clock());
    queue->task_runner()->PostDelayedTask(
        FROM_HERE,
        BindOnce(&CancelableTask::RecordTimeTask,
                 tasks[i]->weak_factory_.GetWeakPtr(), &run_times),
        TimeDelta::FromSeconds(i + 1));
  }

  // Invalidate ever other timer.
  for (int i = 0; i < kNumTasks; i++) {
    if (i % 2)
      tasks[i]->weak_factory_.InvalidateWeakPtrs();
  }

  sequence_manager()->ReclaimMemory();
  EXPECT_EQ(50u, queue->GetNumberOfPendingTasks());

  // Make sure the priority queue still operates as expected.
  FastForwardUntilNoTasksRemain();
  ASSERT_EQ(50u, run_times.size());
  for (int i = 0; i < 50; i++) {
    TimeTicks expected_run_time =
        start_time + TimeDelta::FromSeconds(2 * i + 1);
    EXPECT_EQ(run_times[i], expected_run_time);
  }
}

TEST_P(SequenceManagerTest, DelayedTasksNotSelected) {
  auto queue = CreateTaskQueue();
  constexpr TimeDelta kDelay(TimeDelta::FromMilliseconds(10));
  LazyNow lazy_now(mock_tick_clock());
  EXPECT_EQ(TimeDelta::Max(), sequence_manager()->DelayTillNextTask(&lazy_now));
  EXPECT_EQ(
      TimeDelta::Max(),
      sequence_manager()->DelayTillNextTask(
          &lazy_now, SequencedTaskSource::SelectTaskOption::kSkipDelayedTask));

  queue->task_runner()->PostDelayedTask(FROM_HERE, BindOnce(&NopTask), kDelay);

  // No task should be ready to execute.
  EXPECT_FALSE(sequence_manager()->SelectNextTask(
      SequencedTaskSource::SelectTaskOption::kDefault));
  EXPECT_FALSE(sequence_manager()->SelectNextTask(
      SequencedTaskSource::SelectTaskOption::kSkipDelayedTask));

  EXPECT_EQ(kDelay, sequence_manager()->DelayTillNextTask(&lazy_now));
  EXPECT_EQ(
      TimeDelta::Max(),
      sequence_manager()->DelayTillNextTask(
          &lazy_now, SequencedTaskSource::SelectTaskOption::kSkipDelayedTask));

  AdvanceMockTickClock(kDelay);
  LazyNow lazy_now2(mock_tick_clock());

  // Delayed task is ready to be executed. Consider it only if not in power
  // suspend state.
  EXPECT_FALSE(sequence_manager()->SelectNextTask(
      SequencedTaskSource::SelectTaskOption::kSkipDelayedTask));
  EXPECT_EQ(
      TimeDelta::Max(),
      sequence_manager()->DelayTillNextTask(
          &lazy_now2, SequencedTaskSource::SelectTaskOption::kSkipDelayedTask));

  // Execute the delayed task.
  EXPECT_TRUE(sequence_manager()->SelectNextTask(
      SequencedTaskSource::SelectTaskOption::kDefault));
  sequence_manager()->DidRunTask();
  EXPECT_EQ(TimeDelta::Max(),
            sequence_manager()->DelayTillNextTask(&lazy_now2));

  // Tidy up.
  queue->ShutdownTaskQueue();
}

TEST_P(SequenceManagerTest, DelayedTasksNotSelectedWithImmediateTask) {
  auto queue = CreateTaskQueue();
  constexpr TimeDelta kDelay(TimeDelta::FromMilliseconds(10));
  LazyNow lazy_now(mock_tick_clock());

  EXPECT_EQ(TimeDelta::Max(), sequence_manager()->DelayTillNextTask(&lazy_now));
  EXPECT_EQ(
      TimeDelta::Max(),
      sequence_manager()->DelayTillNextTask(
          &lazy_now, SequencedTaskSource::SelectTaskOption::kSkipDelayedTask));

  // Post an immediate task.
  queue->task_runner()->PostTask(FROM_HERE, BindOnce(&NopTask));
  queue->task_runner()->PostDelayedTask(FROM_HERE, BindOnce(&NopTask), kDelay);

  EXPECT_EQ(TimeDelta(), sequence_manager()->DelayTillNextTask(&lazy_now));
  EXPECT_EQ(
      TimeDelta(),
      sequence_manager()->DelayTillNextTask(
          &lazy_now, SequencedTaskSource::SelectTaskOption::kSkipDelayedTask));

  AdvanceMockTickClock(kDelay);
  LazyNow lazy_now2(mock_tick_clock());

  // An immediate task is present, even if we skip the delayed tasks.
  EXPECT_EQ(
      TimeDelta(),
      sequence_manager()->DelayTillNextTask(
          &lazy_now2, SequencedTaskSource::SelectTaskOption::kSkipDelayedTask));

  // Immediate task should be ready to execute, execute it.
  EXPECT_TRUE(sequence_manager()->SelectNextTask(
      SequencedTaskSource::SelectTaskOption::kSkipDelayedTask));
  sequence_manager()->DidRunTask();

  // Delayed task is ready to be executed. Consider it only if not in power
  // suspend state. This test differs from
  // SequenceManagerTest.DelayedTasksNotSelected as it confirms that delayed
  // tasks are ignored even if they're already in the ready queue (per having
  // performed task selection already before running the immediate task above).
  EXPECT_FALSE(sequence_manager()->SelectNextTask(
      SequencedTaskSource::SelectTaskOption::kSkipDelayedTask));
  EXPECT_EQ(
      TimeDelta::Max(),
      sequence_manager()->DelayTillNextTask(
          &lazy_now2, SequencedTaskSource::SelectTaskOption::kSkipDelayedTask));

  // Execute the delayed task.
  EXPECT_TRUE(sequence_manager()->SelectNextTask(
      SequencedTaskSource::SelectTaskOption::kDefault));
  EXPECT_EQ(
      TimeDelta::Max(),
      sequence_manager()->DelayTillNextTask(
          &lazy_now2, SequencedTaskSource::SelectTaskOption::kSkipDelayedTask));
  sequence_manager()->DidRunTask();

  // Tidy up.
  queue->ShutdownTaskQueue();
}

TEST_P(SequenceManagerTest,
       DelayedTasksNotSelectedWithImmediateTaskWithPriority) {
  auto queues = CreateTaskQueues(4u);
  queues[0]->SetQueuePriority(TaskQueue::QueuePriority::kLowPriority);
  queues[1]->SetQueuePriority(TaskQueue::QueuePriority::kNormalPriority);
  queues[2]->SetQueuePriority(TaskQueue::QueuePriority::kHighPriority);
  queues[3]->SetQueuePriority(TaskQueue::QueuePriority::kVeryHighPriority);

  // Post immediate tasks.
  queues[0]->task_runner()->PostTask(FROM_HERE, BindOnce(&NopTask));
  queues[2]->task_runner()->PostTask(FROM_HERE, BindOnce(&NopTask));

  // Post delayed tasks.
  constexpr TimeDelta kDelay(TimeDelta::FromMilliseconds(10));
  queues[1]->task_runner()->PostDelayedTask(FROM_HERE, BindOnce(&NopTask),
                                            kDelay);
  queues[3]->task_runner()->PostDelayedTask(FROM_HERE, BindOnce(&NopTask),
                                            kDelay);

  LazyNow lazy_now(mock_tick_clock());

  EXPECT_EQ(
      TimeDelta(),
      sequence_manager()->DelayTillNextTask(
          &lazy_now, SequencedTaskSource::SelectTaskOption::kSkipDelayedTask));

  AdvanceMockTickClock(kDelay);
  LazyNow lazy_now2(mock_tick_clock());

  EXPECT_EQ(
      TimeDelta(),
      sequence_manager()->DelayTillNextTask(
          &lazy_now2, SequencedTaskSource::SelectTaskOption::kSkipDelayedTask));

  // Immediate tasks should be ready to execute, execute them.
  EXPECT_TRUE(sequence_manager()->SelectNextTask(
      SequencedTaskSource::SelectTaskOption::kSkipDelayedTask));
  sequence_manager()->DidRunTask();
  EXPECT_TRUE(sequence_manager()->SelectNextTask(
      SequencedTaskSource::SelectTaskOption::kSkipDelayedTask));
  sequence_manager()->DidRunTask();

  // No immediate tasks can be executed anymore.
  EXPECT_FALSE(sequence_manager()->SelectNextTask(
      SequencedTaskSource::SelectTaskOption::kSkipDelayedTask));
  EXPECT_EQ(
      TimeDelta::Max(),
      sequence_manager()->DelayTillNextTask(
          &lazy_now2, SequencedTaskSource::SelectTaskOption::kSkipDelayedTask));

  // Execute delayed tasks.
  EXPECT_TRUE(sequence_manager()->SelectNextTask());
  sequence_manager()->DidRunTask();
  EXPECT_TRUE(sequence_manager()->SelectNextTask());
  sequence_manager()->DidRunTask();

  // No delayed tasks can be executed anymore.
  EXPECT_FALSE(sequence_manager()->SelectNextTask());
  EXPECT_EQ(TimeDelta::Max(),
            sequence_manager()->DelayTillNextTask(&lazy_now2));

  // Tidy up.
  queues[0]->ShutdownTaskQueue();
  queues[1]->ShutdownTaskQueue();
  queues[2]->ShutdownTaskQueue();
  queues[3]->ShutdownTaskQueue();
}

TEST_P(SequenceManagerTest, DelayTillNextTask) {
  auto queues = CreateTaskQueues(2u);

  LazyNow lazy_now(mock_tick_clock());
  EXPECT_EQ(TimeDelta::Max(), sequence_manager()->DelayTillNextTask(&lazy_now));

  queues[0]->task_runner()->PostDelayedTask(FROM_HERE, BindOnce(&NopTask),
                                            TimeDelta::FromSeconds(10));

  EXPECT_EQ(TimeDelta::FromSeconds(10),
            sequence_manager()->DelayTillNextTask(&lazy_now));

  queues[1]->task_runner()->PostDelayedTask(FROM_HERE, BindOnce(&NopTask),
                                            TimeDelta::FromSeconds(15));

  EXPECT_EQ(TimeDelta::FromSeconds(10),
            sequence_manager()->DelayTillNextTask(&lazy_now));

  queues[1]->task_runner()->PostDelayedTask(FROM_HERE, BindOnce(&NopTask),
                                            TimeDelta::FromSeconds(5));

  EXPECT_EQ(TimeDelta::FromSeconds(5),
            sequence_manager()->DelayTillNextTask(&lazy_now));

  queues[0]->task_runner()->PostTask(FROM_HERE, BindOnce(&NopTask));

  EXPECT_EQ(TimeDelta(), sequence_manager()->DelayTillNextTask(&lazy_now));
}

TEST_P(SequenceManagerTest, DelayTillNextTask_Disabled) {
  auto queue = CreateTaskQueue();

  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter =
      queue->CreateQueueEnabledVoter();
  voter->SetVoteToEnable(false);
  queue->task_runner()->PostTask(FROM_HERE, BindOnce(&NopTask));

  LazyNow lazy_now(mock_tick_clock());
  EXPECT_EQ(TimeDelta::Max(), sequence_manager()->DelayTillNextTask(&lazy_now));
}

TEST_P(SequenceManagerTest, DelayTillNextTask_Fence) {
  auto queue = CreateTaskQueue();

  queue->InsertFence(TaskQueue::InsertFencePosition::kNow);
  queue->task_runner()->PostTask(FROM_HERE, BindOnce(&NopTask));

  LazyNow lazy_now(mock_tick_clock());
  EXPECT_EQ(TimeDelta::Max(), sequence_manager()->DelayTillNextTask(&lazy_now));
}

TEST_P(SequenceManagerTest, DelayTillNextTask_FenceUnblocking) {
  auto queue = CreateTaskQueue();

  queue->InsertFence(TaskQueue::InsertFencePosition::kNow);
  queue->task_runner()->PostTask(FROM_HERE, BindOnce(&NopTask));
  queue->InsertFence(TaskQueue::InsertFencePosition::kNow);

  LazyNow lazy_now(mock_tick_clock());
  EXPECT_EQ(TimeDelta(), sequence_manager()->DelayTillNextTask(&lazy_now));
}

TEST_P(SequenceManagerTest, DelayTillNextTask_DelayedTaskReady) {
  auto queue = CreateTaskQueue();

  queue->task_runner()->PostDelayedTask(FROM_HERE, BindOnce(&NopTask),
                                        TimeDelta::FromSeconds(1));

  AdvanceMockTickClock(TimeDelta::FromSeconds(10));

  LazyNow lazy_now(mock_tick_clock());
  EXPECT_EQ(TimeDelta(), sequence_manager()->DelayTillNextTask(&lazy_now));
}

namespace {
void MessageLoopTaskWithDelayedQuit(Fixture* fixture,
                                    scoped_refptr<TestTaskQueue> task_queue) {
  RunLoop run_loop(RunLoop::Type::kNestableTasksAllowed);
  task_queue->task_runner()->PostDelayedTask(FROM_HERE, run_loop.QuitClosure(),
                                             TimeDelta::FromMilliseconds(100));
  fixture->AdvanceMockTickClock(TimeDelta::FromMilliseconds(200));
  run_loop.Run();
}
}  // namespace

TEST_P(SequenceManagerTest, DelayedTaskRunsInNestedMessageLoop) {
  if (GetUnderlyingRunnerType() == TestType::kMockTaskRunner)
    return;
  auto queue = CreateTaskQueue();
  RunLoop run_loop;
  queue->task_runner()->PostTask(
      FROM_HERE,
      BindOnce(&MessageLoopTaskWithDelayedQuit, this, RetainedRef(queue)));
  run_loop.RunUntilIdle();
}

namespace {
void MessageLoopTaskWithImmediateQuit(OnceClosure non_nested_quit_closure,
                                      scoped_refptr<TestTaskQueue> task_queue) {
  RunLoop run_loop(RunLoop::Type::kNestableTasksAllowed);
  // Needed because entering the nested run loop causes a DoWork to get
  // posted.
  task_queue->task_runner()->PostTask(FROM_HERE, BindOnce(&NopTask));
  task_queue->task_runner()->PostTask(FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
  std::move(non_nested_quit_closure).Run();
}
}  // namespace

TEST_P(SequenceManagerTest, DelayedNestedMessageLoopDoesntPreventTasksRunning) {
  if (GetUnderlyingRunnerType() == TestType::kMockTaskRunner)
    return;
  auto queue = CreateTaskQueue();
  RunLoop run_loop;
  queue->task_runner()->PostDelayedTask(
      FROM_HERE,
      BindOnce(&MessageLoopTaskWithImmediateQuit, run_loop.QuitClosure(),
               RetainedRef(queue)),
      TimeDelta::FromMilliseconds(100));

  AdvanceMockTickClock(TimeDelta::FromMilliseconds(200));
  run_loop.Run();
}

TEST_P(SequenceManagerTest, CouldTaskRun_DisableAndReenable) {
  auto queue = CreateTaskQueue();

  EnqueueOrder enqueue_order = sequence_manager()->GetNextSequenceNumber();
  EXPECT_TRUE(queue->GetTaskQueueImpl()->CouldTaskRun(enqueue_order));

  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter =
      queue->CreateQueueEnabledVoter();
  voter->SetVoteToEnable(false);
  EXPECT_FALSE(queue->GetTaskQueueImpl()->CouldTaskRun(enqueue_order));

  voter->SetVoteToEnable(true);
  EXPECT_TRUE(queue->GetTaskQueueImpl()->CouldTaskRun(enqueue_order));
}

TEST_P(SequenceManagerTest, CouldTaskRun_Fence) {
  auto queue = CreateTaskQueue();

  EnqueueOrder enqueue_order = sequence_manager()->GetNextSequenceNumber();
  EXPECT_TRUE(queue->GetTaskQueueImpl()->CouldTaskRun(enqueue_order));

  queue->InsertFence(TaskQueue::InsertFencePosition::kNow);
  EXPECT_TRUE(queue->GetTaskQueueImpl()->CouldTaskRun(enqueue_order));

  queue->InsertFence(TaskQueue::InsertFencePosition::kBeginningOfTime);
  EXPECT_FALSE(queue->GetTaskQueueImpl()->CouldTaskRun(enqueue_order));

  queue->RemoveFence();
  EXPECT_TRUE(queue->GetTaskQueueImpl()->CouldTaskRun(enqueue_order));
}

TEST_P(SequenceManagerTest, CouldTaskRun_FenceBeforeThenAfter) {
  auto queue = CreateTaskQueue();

  queue->InsertFence(TaskQueue::InsertFencePosition::kNow);

  EnqueueOrder enqueue_order = sequence_manager()->GetNextSequenceNumber();
  EXPECT_FALSE(queue->GetTaskQueueImpl()->CouldTaskRun(enqueue_order));

  queue->InsertFence(TaskQueue::InsertFencePosition::kNow);
  EXPECT_TRUE(queue->GetTaskQueueImpl()->CouldTaskRun(enqueue_order));
}

TEST_P(SequenceManagerTest, DelayedDoWorkNotPostedForDisabledQueue) {
  auto queue = CreateTaskQueue();

  queue->task_runner()->PostDelayedTask(FROM_HERE, BindOnce(&NopTask),
                                        TimeDelta::FromMilliseconds(1));
  EXPECT_EQ(TimeDelta::FromMilliseconds(1), NextPendingTaskDelay());

  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter =
      queue->CreateQueueEnabledVoter();
  voter->SetVoteToEnable(false);

  switch (GetUnderlyingRunnerType()) {
    case TestType::kMessagePump:
      EXPECT_EQ(TimeDelta::FromDays(1), NextPendingTaskDelay());
      break;

    case TestType::kMockTaskRunner:
      EXPECT_EQ(TimeDelta::Max(), NextPendingTaskDelay());
      break;

    default:
      NOTREACHED();
  }

  voter->SetVoteToEnable(true);
  EXPECT_EQ(TimeDelta::FromMilliseconds(1), NextPendingTaskDelay());
}

TEST_P(SequenceManagerTest, DisablingQueuesChangesDelayTillNextDoWork) {
  auto queues = CreateTaskQueues(3u);
  queues[0]->task_runner()->PostDelayedTask(FROM_HERE, BindOnce(&NopTask),
                                            TimeDelta::FromMilliseconds(1));
  queues[1]->task_runner()->PostDelayedTask(FROM_HERE, BindOnce(&NopTask),
                                            TimeDelta::FromMilliseconds(10));
  queues[2]->task_runner()->PostDelayedTask(FROM_HERE, BindOnce(&NopTask),
                                            TimeDelta::FromMilliseconds(100));

  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter0 =
      queues[0]->CreateQueueEnabledVoter();
  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter1 =
      queues[1]->CreateQueueEnabledVoter();
  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter2 =
      queues[2]->CreateQueueEnabledVoter();

  EXPECT_EQ(TimeDelta::FromMilliseconds(1), NextPendingTaskDelay());

  voter0->SetVoteToEnable(false);
  EXPECT_EQ(TimeDelta::FromMilliseconds(10), NextPendingTaskDelay());

  voter1->SetVoteToEnable(false);
  EXPECT_EQ(TimeDelta::FromMilliseconds(100), NextPendingTaskDelay());

  voter2->SetVoteToEnable(false);
  switch (GetUnderlyingRunnerType()) {
    case TestType::kMessagePump:
      EXPECT_EQ(TimeDelta::FromDays(1), NextPendingTaskDelay());
      break;

    case TestType::kMockTaskRunner:
      EXPECT_EQ(TimeDelta::Max(), NextPendingTaskDelay());
      break;

    default:
      NOTREACHED();
  }
}

TEST_P(SequenceManagerTest, GetNextDesiredWakeUp) {
  auto queue = CreateTaskQueue();

  EXPECT_EQ(absl::nullopt, queue->GetNextDesiredWakeUp());

  TimeTicks start_time = sequence_manager()->NowTicks();
  TimeDelta delay1 = TimeDelta::FromMilliseconds(10);
  TimeDelta delay2 = TimeDelta::FromMilliseconds(2);

  queue->task_runner()->PostDelayedTask(FROM_HERE, BindOnce(&NopTask), delay1);
  EXPECT_EQ(start_time + delay1, queue->GetNextDesiredWakeUp()->time);

  queue->task_runner()->PostDelayedTask(FROM_HERE, BindOnce(&NopTask), delay2);
  EXPECT_EQ(start_time + delay2, queue->GetNextDesiredWakeUp()->time);

  // We don't have wake-ups scheduled for disabled queues.
  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter =
      queue->CreateQueueEnabledVoter();
  voter->SetVoteToEnable(false);
  EXPECT_EQ(absl::nullopt, queue->GetNextDesiredWakeUp());

  voter->SetVoteToEnable(true);
  EXPECT_EQ(start_time + delay2, queue->GetNextDesiredWakeUp()->time);

  // Immediate tasks shouldn't make any difference.
  queue->task_runner()->PostTask(FROM_HERE, BindOnce(&NopTask));
  EXPECT_EQ(start_time + delay2, queue->GetNextDesiredWakeUp()->time);

  // Neither should fences.
  queue->InsertFence(TaskQueue::InsertFencePosition::kBeginningOfTime);
  EXPECT_EQ(start_time + delay2, queue->GetNextDesiredWakeUp()->time);
}

TEST_P(SequenceManagerTest, SetTimeDomainForDisabledQueue) {
  auto queue = CreateTaskQueue();

  TimeTicks start_time = sequence_manager()->NowTicks();
  TimeDelta delay(TimeDelta::FromMilliseconds(1));

  StrictMock<MockTaskQueueThrottler> throttler(queue.get());
  queue->SetThrottler(&throttler);

  EXPECT_CALL(throttler,
              GetNextAllowedWakeUp_DesiredWakeUpTime(start_time + delay));
  queue->task_runner()->PostDelayedTask(FROM_HERE, BindOnce(&NopTask), delay);
  Mock::VerifyAndClearExpectations(&throttler);

  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter =
      queue->CreateQueueEnabledVoter();
  voter->SetVoteToEnable(false);

  // We should not get a notification for a disabled queue.
  std::unique_ptr<MockTimeDomain> domain =
      std::make_unique<MockTimeDomain>(sequence_manager()->NowTicks());
  sequence_manager()->RegisterTimeDomain(domain.get());
  queue->SetTimeDomain(domain.get());

  // Tidy up.
  queue->ShutdownTaskQueue();
  sequence_manager()->UnregisterTimeDomain(domain.get());
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
      [](int* counter, const Task& task, TaskQueue::TaskTiming* task_timing,
         LazyNow* lazy_now) { ++(*counter); },
      complete_counter));
  task_queue->GetTaskQueueImpl()->SetOnTaskPostedHandler(
      internal::TaskQueueImpl::OnTaskPostedHandler());
}

void UnsetOnTaskHandlers(scoped_refptr<TestTaskQueue> task_queue) {
  task_queue->GetTaskQueueImpl()->SetOnTaskStartedHandler(
      internal::TaskQueueImpl::OnTaskStartedHandler());
  task_queue->GetTaskQueueImpl()->SetOnTaskCompletedHandler(
      internal::TaskQueueImpl::OnTaskCompletedHandler());
  task_queue->GetTaskQueueImpl()->SetOnTaskPostedHandler(
      internal::TaskQueueImpl::OnTaskPostedHandler());
}
}  // namespace

TEST_P(SequenceManagerTest, ProcessTasksWithoutTaskTimeObservers) {
  auto queue = CreateTaskQueue();
  int start_counter = 0;
  int complete_counter = 0;
  std::vector<EnqueueOrder> run_order;
  SetOnTaskHandlers(queue, &start_counter, &complete_counter);
  EXPECT_TRUE(queue->GetTaskQueueImpl()->RequiresTaskTiming());
  queue->task_runner()->PostTask(FROM_HERE, BindOnce(&TestTask, 1, &run_order));
  queue->task_runner()->PostTask(FROM_HERE, BindOnce(&TestTask, 2, &run_order));
  queue->task_runner()->PostTask(FROM_HERE, BindOnce(&TestTask, 3, &run_order));

  RunLoop().RunUntilIdle();
  EXPECT_EQ(start_counter, 3);
  EXPECT_EQ(complete_counter, 3);
  EXPECT_THAT(run_order, ElementsAre(1u, 2u, 3u));

  UnsetOnTaskHandlers(queue);
  EXPECT_FALSE(queue->GetTaskQueueImpl()->RequiresTaskTiming());
  queue->task_runner()->PostTask(FROM_HERE, BindOnce(&TestTask, 4, &run_order));
  queue->task_runner()->PostTask(FROM_HERE, BindOnce(&TestTask, 5, &run_order));
  queue->task_runner()->PostTask(FROM_HERE, BindOnce(&TestTask, 6, &run_order));

  RunLoop().RunUntilIdle();
  EXPECT_EQ(start_counter, 3);
  EXPECT_EQ(complete_counter, 3);
  EXPECT_THAT(run_order, ElementsAre(1u, 2u, 3u, 4u, 5u, 6u));
}

TEST_P(SequenceManagerTest, ProcessTasksWithTaskTimeObservers) {
  TestTaskTimeObserver test_task_time_observer;
  auto queue = CreateTaskQueue();
  int start_counter = 0;
  int complete_counter = 0;

  sequence_manager()->AddTaskTimeObserver(&test_task_time_observer);
  SetOnTaskHandlers(queue, &start_counter, &complete_counter);
  EXPECT_TRUE(queue->GetTaskQueueImpl()->RequiresTaskTiming());
  std::vector<EnqueueOrder> run_order;
  queue->task_runner()->PostTask(FROM_HERE, BindOnce(&TestTask, 1, &run_order));
  queue->task_runner()->PostTask(FROM_HERE, BindOnce(&TestTask, 2, &run_order));

  RunLoop().RunUntilIdle();
  EXPECT_EQ(start_counter, 2);
  EXPECT_EQ(complete_counter, 2);
  EXPECT_THAT(run_order, ElementsAre(1u, 2u));

  UnsetOnTaskHandlers(queue);
  EXPECT_FALSE(queue->GetTaskQueueImpl()->RequiresTaskTiming());
  queue->task_runner()->PostTask(FROM_HERE, BindOnce(&TestTask, 3, &run_order));
  queue->task_runner()->PostTask(FROM_HERE, BindOnce(&TestTask, 4, &run_order));

  RunLoop().RunUntilIdle();
  EXPECT_EQ(start_counter, 2);
  EXPECT_EQ(complete_counter, 2);
  EXPECT_THAT(run_order, ElementsAre(1u, 2u, 3u, 4u));

  sequence_manager()->RemoveTaskTimeObserver(&test_task_time_observer);
  queue->task_runner()->PostTask(FROM_HERE, BindOnce(&TestTask, 5, &run_order));
  queue->task_runner()->PostTask(FROM_HERE, BindOnce(&TestTask, 6, &run_order));

  RunLoop().RunUntilIdle();
  EXPECT_EQ(start_counter, 2);
  EXPECT_EQ(complete_counter, 2);
  EXPECT_FALSE(queue->GetTaskQueueImpl()->RequiresTaskTiming());
  EXPECT_THAT(run_order, ElementsAre(1u, 2u, 3u, 4u, 5u, 6u));

  SetOnTaskHandlers(queue, &start_counter, &complete_counter);
  queue->task_runner()->PostTask(FROM_HERE, BindOnce(&TestTask, 7, &run_order));
  queue->task_runner()->PostTask(FROM_HERE, BindOnce(&TestTask, 8, &run_order));

  RunLoop().RunUntilIdle();
  EXPECT_EQ(start_counter, 4);
  EXPECT_EQ(complete_counter, 4);
  EXPECT_TRUE(queue->GetTaskQueueImpl()->RequiresTaskTiming());
  EXPECT_THAT(run_order, ElementsAre(1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u));
  UnsetOnTaskHandlers(queue);
}

TEST_P(SequenceManagerTest, ObserverNotFiredAfterTaskQueueDestructed) {
  scoped_refptr<TestTaskQueue> main_tq = CreateTaskQueue();

  StrictMock<MockTaskQueueThrottler> throttler(main_tq.get());
  main_tq->SetThrottler(&throttler);

  // We don't expect the throttler to be notified if the TaskQueue gets
  // destructed.
  auto task_runner = main_tq->task_runner();
  main_tq = nullptr;
  task_runner->PostTask(FROM_HERE, BindOnce(&NopTask));

  FastForwardUntilNoTasksRemain();
}

TEST_P(SequenceManagerTest,
       OnQueueNextWakeUpChangedNotFiredForDisabledQueuePostTask) {
  scoped_refptr<TestTaskQueue> main_tq = CreateTaskQueue();
  auto task_runner = main_tq->task_runner();

  StrictMock<MockTaskQueueThrottler> throttler(main_tq.get());
  main_tq->SetThrottler(&throttler);

  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter =
      main_tq->CreateQueueEnabledVoter();
  voter->SetVoteToEnable(false);

  // We don't expect the OnHasImmediateTask to be called if the TaskQueue gets
  // disabled.
  task_runner->PostTask(FROM_HERE, BindOnce(&NopTask));

  FastForwardUntilNoTasksRemain();
  // When |voter| goes out of scope the queue will become enabled and the
  // observer will fire. We're not interested in testing that however.
  Mock::VerifyAndClearExpectations(&throttler);
}

TEST_P(SequenceManagerTest,
       OnQueueNextWakeUpChangedNotFiredForCrossThreadDisabledQueuePostTask) {
  scoped_refptr<TestTaskQueue> main_tq = CreateTaskQueue();
  auto task_runner = main_tq->task_runner();

  StrictMock<MockTaskQueueThrottler> throttler(main_tq.get());
  main_tq->SetThrottler(&throttler);

  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter =
      main_tq->CreateQueueEnabledVoter();
  voter->SetVoteToEnable(false);

  // We don't expect OnHasImmediateTask to be called if the TaskQueue gets
  // blocked.
  WaitableEvent done_event;
  Thread thread("TestThread");
  thread.Start();
  thread.task_runner()->PostTask(FROM_HERE, BindLambdaForTesting([&]() {
                                   // Should not fire the observer.
                                   task_runner->PostTask(FROM_HERE,
                                                         BindOnce(&NopTask));
                                   done_event.Signal();
                                 }));
  done_event.Wait();
  thread.Stop();

  FastForwardUntilNoTasksRemain();
  // When |voter| goes out of scope the queue will become enabled and the
  // observer will fire. We're not interested in testing that however.
  Mock::VerifyAndClearExpectations(&throttler);
}

TEST_P(SequenceManagerTest, GracefulShutdown) {
  const auto kStartTime = mock_tick_clock()->NowTicks();
  std::vector<TimeTicks> run_times;
  scoped_refptr<TestTaskQueue> main_tq = CreateTaskQueue();
  WeakPtr<TestTaskQueue> main_tq_weak_ptr = main_tq->GetWeakPtr();

  EXPECT_EQ(1u, sequence_manager()->ActiveQueuesCount());
  EXPECT_EQ(0u, sequence_manager()->QueuesToShutdownCount());
  EXPECT_EQ(0u, sequence_manager()->QueuesToDeleteCount());

  for (int i = 1; i <= 5; ++i) {
    main_tq->task_runner()->PostDelayedTask(
        FROM_HERE, BindOnce(&RecordTimeTask, &run_times, mock_tick_clock()),
        TimeDelta::FromMilliseconds(i * 100));
  }
  FastForwardBy(TimeDelta::FromMilliseconds(250));

  main_tq = nullptr;
  // Ensure that task queue went away.
  EXPECT_FALSE(main_tq_weak_ptr.get());

  FastForwardBy(TimeDelta::FromMilliseconds(1));

  EXPECT_EQ(1u, sequence_manager()->ActiveQueuesCount());
  EXPECT_EQ(1u, sequence_manager()->QueuesToShutdownCount());
  EXPECT_EQ(0u, sequence_manager()->QueuesToDeleteCount());

  FastForwardUntilNoTasksRemain();

  // Even with TaskQueue gone, tasks are executed.
  EXPECT_THAT(run_times,
              ElementsAre(kStartTime + TimeDelta::FromMilliseconds(100),
                          kStartTime + TimeDelta::FromMilliseconds(200),
                          kStartTime + TimeDelta::FromMilliseconds(300),
                          kStartTime + TimeDelta::FromMilliseconds(400),
                          kStartTime + TimeDelta::FromMilliseconds(500)));

  EXPECT_EQ(0u, sequence_manager()->ActiveQueuesCount());
  EXPECT_EQ(0u, sequence_manager()->QueuesToShutdownCount());
  EXPECT_EQ(0u, sequence_manager()->QueuesToDeleteCount());
}

TEST_P(SequenceManagerTest, GracefulShutdown_ManagerDeletedInFlight) {
  const auto kStartTime = mock_tick_clock()->NowTicks();
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
    main_tqs[0]->task_runner()->PostDelayedTask(
        FROM_HERE, BindOnce(&RecordTimeTask, &run_times, mock_tick_clock()),
        TimeDelta::FromMilliseconds(i * 100));
  }
  FastForwardBy(TimeDelta::FromMilliseconds(250));

  main_tqs.clear();
  // Ensure that task queues went away.
  for (int i = 0; i < N; ++i) {
    EXPECT_FALSE(main_tq_weak_ptrs[i].get());
  }

  // No leaks should occur when TQM was destroyed before processing
  // shutdown task and TaskQueueImpl should be safely deleted on a correct
  // thread.
  DestroySequenceManager();

  if (GetUnderlyingRunnerType() != TestType::kMessagePump) {
    FastForwardUntilNoTasksRemain();
  }

  EXPECT_THAT(run_times,
              ElementsAre(kStartTime + TimeDelta::FromMilliseconds(100),
                          kStartTime + TimeDelta::FromMilliseconds(200)));
}

TEST_P(SequenceManagerTest,
       GracefulShutdown_ManagerDeletedWithQueuesToShutdown) {
  const auto kStartTime = mock_tick_clock()->NowTicks();
  std::vector<TimeTicks> run_times;
  scoped_refptr<TestTaskQueue> main_tq = CreateTaskQueue();
  WeakPtr<TestTaskQueue> main_tq_weak_ptr = main_tq->GetWeakPtr();
  RefCountedCallbackFactory counter;

  EXPECT_EQ(1u, sequence_manager()->ActiveQueuesCount());
  EXPECT_EQ(0u, sequence_manager()->QueuesToShutdownCount());
  EXPECT_EQ(0u, sequence_manager()->QueuesToDeleteCount());

  for (int i = 1; i <= 5; ++i) {
    main_tq->task_runner()->PostDelayedTask(
        FROM_HERE,
        counter.WrapCallback(
            BindOnce(&RecordTimeTask, &run_times, mock_tick_clock())),
        TimeDelta::FromMilliseconds(i * 100));
  }
  FastForwardBy(TimeDelta::FromMilliseconds(250));

  main_tq = nullptr;
  // Ensure that task queue went away.
  EXPECT_FALSE(main_tq_weak_ptr.get());

  FastForwardBy(TimeDelta::FromMilliseconds(1));

  EXPECT_EQ(1u, sequence_manager()->ActiveQueuesCount());
  EXPECT_EQ(1u, sequence_manager()->QueuesToShutdownCount());
  EXPECT_EQ(0u, sequence_manager()->QueuesToDeleteCount());

  // Ensure that all queues-to-gracefully-shutdown are properly unregistered.
  DestroySequenceManager();

  if (GetUnderlyingRunnerType() != TestType::kMessagePump) {
    FastForwardUntilNoTasksRemain();
  }

  EXPECT_THAT(run_times,
              ElementsAre(kStartTime + TimeDelta::FromMilliseconds(100),
                          kStartTime + TimeDelta::FromMilliseconds(200)));
  EXPECT_FALSE(counter.HasReferences());
}

TEST(SequenceManagerBasicTest, DefaultTaskRunnerSupport) {
  auto base_sequence_manager =
      sequence_manager::CreateSequenceManagerOnCurrentThreadWithPump(
          MessagePump::Create(MessagePumpType::DEFAULT));
  auto queue = base_sequence_manager->CreateTaskQueue(
      sequence_manager::TaskQueue::Spec("default_tq"));
  base_sequence_manager->SetDefaultTaskRunner(queue->task_runner());

  scoped_refptr<SingleThreadTaskRunner> original_task_runner =
      ThreadTaskRunnerHandle::Get();
  scoped_refptr<SingleThreadTaskRunner> custom_task_runner =
      MakeRefCounted<TestSimpleTaskRunner>();
  {
    std::unique_ptr<SequenceManager> manager =
        CreateSequenceManagerOnCurrentThread(SequenceManager::Settings());

    manager->SetDefaultTaskRunner(custom_task_runner);
    DCHECK_EQ(custom_task_runner, ThreadTaskRunnerHandle::Get());
  }
  DCHECK_EQ(original_task_runner, ThreadTaskRunnerHandle::Get());
}

TEST_P(SequenceManagerTest, CanceledTasksInQueueCantMakeOtherTasksSkipAhead) {
  auto queues = CreateTaskQueues(2u);

  CancelableTask task1(mock_tick_clock());
  CancelableTask task2(mock_tick_clock());
  std::vector<TimeTicks> run_times;

  queues[0]->task_runner()->PostTask(
      FROM_HERE, BindOnce(&CancelableTask::RecordTimeTask,
                          task1.weak_factory_.GetWeakPtr(), &run_times));
  queues[0]->task_runner()->PostTask(
      FROM_HERE, BindOnce(&CancelableTask::RecordTimeTask,
                          task2.weak_factory_.GetWeakPtr(), &run_times));

  std::vector<EnqueueOrder> run_order;
  queues[1]->task_runner()->PostTask(FROM_HERE,
                                     BindOnce(&TestTask, 1, &run_order));

  queues[0]->task_runner()->PostTask(FROM_HERE,
                                     BindOnce(&TestTask, 2, &run_order));

  task1.weak_factory_.InvalidateWeakPtrs();
  task2.weak_factory_.InvalidateWeakPtrs();
  RunLoop().RunUntilIdle();

  EXPECT_THAT(run_order, ElementsAre(1u, 2u));
}

TEST_P(SequenceManagerTest, TaskRunnerDeletedOnAnotherThread) {
  const auto kStartTime = mock_tick_clock()->NowTicks();
  std::vector<TimeTicks> run_times;
  scoped_refptr<TestTaskQueue> main_tq = CreateTaskQueue();
  scoped_refptr<TaskRunner> task_runner =
      main_tq->CreateTaskRunner(kTaskTypeNone);

  int start_counter = 0;
  int complete_counter = 0;
  SetOnTaskHandlers(main_tq, &start_counter, &complete_counter);

  EXPECT_EQ(1u, sequence_manager()->ActiveQueuesCount());
  EXPECT_EQ(0u, sequence_manager()->QueuesToShutdownCount());
  EXPECT_EQ(0u, sequence_manager()->QueuesToDeleteCount());

  for (int i = 1; i <= 5; ++i) {
    task_runner->PostDelayedTask(
        FROM_HERE, BindOnce(&RecordTimeTask, &run_times, mock_tick_clock()),
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

  EXPECT_EQ(1u, sequence_manager()->ActiveQueuesCount());
  EXPECT_EQ(1u, sequence_manager()->QueuesToShutdownCount());
  EXPECT_EQ(0u, sequence_manager()->QueuesToDeleteCount());

  FastForwardUntilNoTasksRemain();

  // Even with TaskQueue gone, tasks are executed.
  EXPECT_THAT(run_times,
              ElementsAre(kStartTime + TimeDelta::FromMilliseconds(100),
                          kStartTime + TimeDelta::FromMilliseconds(200),
                          kStartTime + TimeDelta::FromMilliseconds(300),
                          kStartTime + TimeDelta::FromMilliseconds(400),
                          kStartTime + TimeDelta::FromMilliseconds(500)));

  EXPECT_EQ(0u, sequence_manager()->ActiveQueuesCount());
  EXPECT_EQ(0u, sequence_manager()->QueuesToShutdownCount());
  EXPECT_EQ(0u, sequence_manager()->QueuesToDeleteCount());

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
      std::make_unique<RunOnDestructionHelper>(std::move(task)));
}

base::OnceClosure PostOnDestruction(scoped_refptr<TestTaskQueue> task_queue,
                                    base::OnceClosure task) {
  return RunOnDestruction(base::BindOnce(
      [](base::OnceClosure task, scoped_refptr<TestTaskQueue> task_queue) {
        task_queue->task_runner()->PostTask(FROM_HERE, std::move(task));
      },
      std::move(task), task_queue));
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

  DestroySequenceManager();

  thread->task_runner()->PostTask(
      FROM_HERE, BindOnce(
                     [](scoped_refptr<TestTaskQueue> task_queue,
                        WaitableEvent* test_executed) {
                       task_queue->task_runner()->PostTask(
                           FROM_HERE, PostOnDestruction(
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
      std::make_unique<MockTimeDomain>(TimeTicks());
  std::unique_ptr<TaskQueueImpl> queue2 = std::make_unique<TaskQueueImpl>(
      nullptr, time_domain.get(), TaskQueue::Spec("stub"));
  scoped_refptr<SingleThreadTaskRunner> task_runner2 =
      queue2->CreateTaskRunner(0);
  EXPECT_FALSE(task_runner2->PostTask(FROM_HERE, BindOnce(&NopTask)));

  // Tidy up.
  queue2->UnregisterTaskQueue();
}

TEST_P(SequenceManagerTest, DestructorPostChainDuringShutdown) {
  // Checks that a chain of closures which post other closures on destruction do
  // thing on shutdown.
  scoped_refptr<TestTaskQueue> task_queue = CreateTaskQueue();
  bool run = false;
  task_queue->task_runner()->PostTask(
      FROM_HERE,
      PostOnDestruction(
          task_queue,
          PostOnDestruction(task_queue,
                            RunOnDestruction(base::BindOnce(
                                [](bool* run) { *run = true; }, &run)))));

  DestroySequenceManager();

  EXPECT_TRUE(run);
}

TEST_P(SequenceManagerTest, DestructorPostsViaTaskRunnerHandleDuringShutdown) {
  scoped_refptr<TestTaskQueue> task_queue = CreateTaskQueue();
  bool run = false;
  task_queue->task_runner()->PostTask(
      FROM_HERE, RunOnDestruction(BindLambdaForTesting([&]() {
        ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                base::BindOnce(&NopTask));
        run = true;
      })));

  // Should not DCHECK when ThreadTaskRunnerHandle::Get() is invoked.
  DestroySequenceManager();
  EXPECT_TRUE(run);
}

TEST_P(SequenceManagerTest, CreateUnboundSequenceManagerWhichIsNeverBound) {
  // This should not crash.
  CreateUnboundSequenceManager();
}

TEST_P(SequenceManagerTest, HasPendingHighResolutionTasks) {
  auto queue = CreateTaskQueue();
  bool supports_high_res = false;
#if defined(OS_WIN)
  supports_high_res = true;
#endif

  // Only the third task needs high resolution timing.
  EXPECT_FALSE(sequence_manager()->HasPendingHighResolutionTasks());
  queue->task_runner()->PostTask(FROM_HERE, BindOnce(&NopTask));
  EXPECT_FALSE(sequence_manager()->HasPendingHighResolutionTasks());
  queue->task_runner()->PostDelayedTask(FROM_HERE, BindOnce(&NopTask),
                                        TimeDelta::FromMilliseconds(100));
  EXPECT_FALSE(sequence_manager()->HasPendingHighResolutionTasks());
  queue->task_runner()->PostDelayedTask(FROM_HERE, BindOnce(&NopTask),
                                        TimeDelta::FromMilliseconds(10));
  EXPECT_EQ(sequence_manager()->HasPendingHighResolutionTasks(),
            supports_high_res);

  // Running immediate tasks doesn't affect pending high resolution tasks.
  RunLoop().RunUntilIdle();
  EXPECT_EQ(sequence_manager()->HasPendingHighResolutionTasks(),
            supports_high_res);

  // Advancing to just before a pending low resolution task doesn't mean that we
  // have pending high resolution work.
  AdvanceMockTickClock(TimeDelta::FromMilliseconds(99));
  RunLoop().RunUntilIdle();
  EXPECT_FALSE(sequence_manager()->HasPendingHighResolutionTasks());

  AdvanceMockTickClock(TimeDelta::FromMilliseconds(100));
  RunLoop().RunUntilIdle();
  EXPECT_FALSE(sequence_manager()->HasPendingHighResolutionTasks());
}

TEST_P(SequenceManagerTest, HasPendingHighResolutionTasksLowPriority) {
  auto queue = CreateTaskQueue();
  queue->SetQueuePriority(TaskQueue::QueuePriority::kLowPriority);
  bool supports_high_res = false;
#if defined(OS_WIN)
  supports_high_res = true;
#endif

  // No task should be considered high resolution in a low priority queue.
  EXPECT_FALSE(sequence_manager()->HasPendingHighResolutionTasks());
  queue->task_runner()->PostTask(FROM_HERE, BindOnce(&NopTask));
  EXPECT_FALSE(sequence_manager()->HasPendingHighResolutionTasks());
  queue->task_runner()->PostDelayedTask(FROM_HERE, BindOnce(&NopTask),
                                        TimeDelta::FromMilliseconds(100));
  EXPECT_FALSE(sequence_manager()->HasPendingHighResolutionTasks());
  queue->task_runner()->PostDelayedTask(FROM_HERE, BindOnce(&NopTask),
                                        TimeDelta::FromMilliseconds(10));
  EXPECT_FALSE(sequence_manager()->HasPendingHighResolutionTasks());

  // Increasing queue priority should enable high resolution timer.
  queue->SetQueuePriority(TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(sequence_manager()->HasPendingHighResolutionTasks(),
            supports_high_res);
  queue->SetQueuePriority(TaskQueue::QueuePriority::kLowPriority);
  EXPECT_FALSE(sequence_manager()->HasPendingHighResolutionTasks());

  // Running immediate tasks doesn't affect pending high resolution tasks.
  RunLoop().RunUntilIdle();
  EXPECT_FALSE(sequence_manager()->HasPendingHighResolutionTasks());

  // Advancing to just before a pending low resolution task doesn't mean that we
  // have pending high resolution work.
  AdvanceMockTickClock(TimeDelta::FromMilliseconds(99));
  RunLoop().RunUntilIdle();
  EXPECT_FALSE(sequence_manager()->HasPendingHighResolutionTasks());

  AdvanceMockTickClock(TimeDelta::FromMilliseconds(100));
  RunLoop().RunUntilIdle();
  EXPECT_FALSE(sequence_manager()->HasPendingHighResolutionTasks());
}

TEST_P(SequenceManagerTest,
       HasPendingHighResolutionTasksLowAndNormalPriorityQueues) {
  auto queueLow = CreateTaskQueue();
  queueLow->SetQueuePriority(TaskQueue::QueuePriority::kLowPriority);
  auto queueNormal = CreateTaskQueue();
  queueNormal->SetQueuePriority(TaskQueue::QueuePriority::kNormalPriority);
  bool supports_high_res = false;
#if defined(OS_WIN)
  supports_high_res = true;
#endif

  // No task should be considered high resolution in a low priority queue.
  EXPECT_FALSE(sequence_manager()->HasPendingHighResolutionTasks());
  queueLow->task_runner()->PostDelayedTask(FROM_HERE, BindOnce(&NopTask),
                                           TimeDelta::FromMilliseconds(10));
  EXPECT_FALSE(sequence_manager()->HasPendingHighResolutionTasks());
  queueNormal->task_runner()->PostDelayedTask(FROM_HERE, BindOnce(&NopTask),
                                              TimeDelta::FromMilliseconds(100));
  EXPECT_FALSE(sequence_manager()->HasPendingHighResolutionTasks());

  // Increasing queue priority should enable high resolution timer.
  queueLow->SetQueuePriority(TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(sequence_manager()->HasPendingHighResolutionTasks(),
            supports_high_res);
}

namespace {

class PostTaskWhenDeleted;
void CallbackWithDestructor(std::unique_ptr<PostTaskWhenDeleted>);

class PostTaskWhenDeleted {
 public:
  PostTaskWhenDeleted(std::string name,
                      scoped_refptr<SingleThreadTaskRunner> task_runner,
                      size_t depth,
                      std::set<std::string>* tasks_alive,
                      std::vector<std::string>* tasks_deleted)
      : name_(name),
        task_runner_(std::move(task_runner)),
        depth_(depth),
        tasks_alive_(tasks_alive),
        tasks_deleted_(tasks_deleted) {
    tasks_alive_->insert(full_name());
  }

  ~PostTaskWhenDeleted() {
    DCHECK(tasks_alive_->find(full_name()) != tasks_alive_->end());
    tasks_alive_->erase(full_name());
    tasks_deleted_->push_back(full_name());

    if (depth_ > 0) {
      task_runner_->PostTask(
          FROM_HERE, base::BindOnce(&CallbackWithDestructor,
                                    std::make_unique<PostTaskWhenDeleted>(
                                        name_, task_runner_, depth_ - 1,
                                        tasks_alive_, tasks_deleted_)));
    }
  }

 private:
  std::string full_name() { return name_ + " " + NumberToString(depth_); }

  std::string name_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  int depth_;
  std::set<std::string>* tasks_alive_;
  std::vector<std::string>* tasks_deleted_;
};

void CallbackWithDestructor(std::unique_ptr<PostTaskWhenDeleted> object) {}

}  // namespace

TEST_P(SequenceManagerTest, DoesNotRecordQueueTimeIfSettingFalse) {
  auto queue = CreateTaskQueue();

  QueueTimeTaskObserver observer;
  sequence_manager()->AddTaskObserver(&observer);

  // We do not record task queue time when the setting is false.
  sequence_manager()->SetAddQueueTimeToTasks(false);
  AdvanceMockTickClock(TimeDelta::FromMilliseconds(99));
  queue->task_runner()->PostTask(FROM_HERE, BindOnce(&NopTask));
  RunLoop().RunUntilIdle();
  EXPECT_THAT(observer.queue_times(), ElementsAre(TimeTicks()));

  sequence_manager()->RemoveTaskObserver(&observer);
}

TEST_P(SequenceManagerTest, RecordsQueueTimeIfSettingTrue) {
  const auto kStartTime = mock_tick_clock()->NowTicks();
  auto queue = CreateTaskQueue();

  QueueTimeTaskObserver observer;
  sequence_manager()->AddTaskObserver(&observer);

  // We correctly record task queue time when the setting is true.
  sequence_manager()->SetAddQueueTimeToTasks(true);
  AdvanceMockTickClock(TimeDelta::FromMilliseconds(99));
  queue->task_runner()->PostTask(FROM_HERE, BindOnce(&NopTask));
  RunLoop().RunUntilIdle();
  EXPECT_THAT(observer.queue_times(),
              ElementsAre(kStartTime + TimeDelta::FromMilliseconds(99)));

  sequence_manager()->RemoveTaskObserver(&observer);
}

namespace {

// Inject a test point for recording the destructor calls for OnceClosure
// objects sent to PostTask(). It is awkward usage since we are trying to hook
// the actual destruction, which is not a common operation.
class DestructionObserverProbe : public RefCounted<DestructionObserverProbe> {
 public:
  DestructionObserverProbe(bool* task_destroyed,
                           bool* destruction_observer_called)
      : task_destroyed_(task_destroyed),
        destruction_observer_called_(destruction_observer_called) {}
  virtual void Run() {
    // This task should never run.
    ADD_FAILURE();
  }

 private:
  friend class RefCounted<DestructionObserverProbe>;

  virtual ~DestructionObserverProbe() {
    EXPECT_FALSE(*destruction_observer_called_);
    *task_destroyed_ = true;
  }

  bool* task_destroyed_;
  bool* destruction_observer_called_;
};

class SMDestructionObserver : public CurrentThread::DestructionObserver {
 public:
  SMDestructionObserver(bool* task_destroyed, bool* destruction_observer_called)
      : task_destroyed_(task_destroyed),
        destruction_observer_called_(destruction_observer_called),
        task_destroyed_before_message_loop_(false) {}
  void WillDestroyCurrentMessageLoop() override {
    task_destroyed_before_message_loop_ = *task_destroyed_;
    *destruction_observer_called_ = true;
  }
  bool task_destroyed_before_message_loop() const {
    return task_destroyed_before_message_loop_;
  }

 private:
  bool* task_destroyed_;
  bool* destruction_observer_called_;
  bool task_destroyed_before_message_loop_;
};

}  // namespace

TEST_P(SequenceManagerTest, DestructionObserverTest) {
  auto queue = CreateTaskQueue();

  // Verify that the destruction observer gets called at the very end (after
  // all the pending tasks have been destroyed).
  const TimeDelta kDelay = TimeDelta::FromMilliseconds(100);

  bool task_destroyed = false;
  bool destruction_observer_called = false;

  SMDestructionObserver observer(&task_destroyed, &destruction_observer_called);
  sequence_manager()->AddDestructionObserver(&observer);
  queue->task_runner()->PostDelayedTask(
      FROM_HERE,
      BindOnce(&DestructionObserverProbe::Run,
               base::MakeRefCounted<DestructionObserverProbe>(
                   &task_destroyed, &destruction_observer_called)),
      kDelay);

  DestroySequenceManager();

  EXPECT_TRUE(observer.task_destroyed_before_message_loop());
  // The task should have been destroyed when we deleted the loop.
  EXPECT_TRUE(task_destroyed);
  EXPECT_TRUE(destruction_observer_called);
}

TEST_P(SequenceManagerTest, GetMessagePump) {
  switch (GetUnderlyingRunnerType()) {
    default:
      EXPECT_THAT(sequence_manager()->GetMessagePump(), testing::IsNull());
      break;
    case TestType::kMessagePump:
      EXPECT_THAT(sequence_manager()->GetMessagePump(), testing::NotNull());
      break;
  }
}

namespace {

class MockTimeDomain : public TimeDomain {
 public:
  MockTimeDomain() = default;
  MockTimeDomain(const MockTimeDomain&) = delete;
  MockTimeDomain& operator=(const MockTimeDomain&) = delete;
  ~MockTimeDomain() override = default;

  LazyNow CreateLazyNow() const override { return LazyNow(now_); }
  TimeTicks Now() const override { return now_; }

  absl::optional<TimeDelta> DelayTillNextTask(LazyNow* lazy_now) override {
    return absl::optional<TimeDelta>();
  }

  MOCK_METHOD1(MaybeFastForwardToNextTask, bool(bool quit_when_idle_requested));

  const char* GetName() const override { return "Test"; }

  void SetNextDelayedDoWork(LazyNow* lazy_now, TimeTicks run_time) override {}

 private:
  TimeTicks now_;
};

}  // namespace

TEST_P(SequenceManagerTest, OnSystemIdleTimeDomainNotification) {
  if (GetUnderlyingRunnerType() != TestType::kMessagePump)
    return;

  auto queue = CreateTaskQueue();

  // If we call OnSystemIdle, we expect registered TimeDomains to receive a call
  // to MaybeFastForwardToNextTask.  If no run loop has requested quit on idle,
  // the parameter passed in should be false.
  StrictMock<MockTimeDomain> mock_time_domain;
  sequence_manager()->RegisterTimeDomain(&mock_time_domain);
  EXPECT_CALL(mock_time_domain, MaybeFastForwardToNextTask(false))
      .WillOnce(Return(false));
  sequence_manager()->OnSystemIdle();
  sequence_manager()->UnregisterTimeDomain(&mock_time_domain);
  Mock::VerifyAndClearExpectations(&mock_time_domain);

  // However if RunUntilIdle is called it should be true.
  queue->task_runner()->PostTask(
      FROM_HERE, BindLambdaForTesting([&]() {
        StrictMock<MockTimeDomain> mock_time_domain;
        EXPECT_CALL(mock_time_domain, MaybeFastForwardToNextTask(true))
            .WillOnce(Return(false));
        sequence_manager()->RegisterTimeDomain(&mock_time_domain);
        sequence_manager()->OnSystemIdle();
        sequence_manager()->UnregisterTimeDomain(&mock_time_domain);
      }));

  RunLoop().RunUntilIdle();
}

TEST_P(SequenceManagerTest, CreateTaskQueue) {
  scoped_refptr<TaskQueue> task_queue =
      sequence_manager()->CreateTaskQueue(TaskQueue::Spec("test"));
  EXPECT_THAT(task_queue.get(), testing::NotNull());

  task_queue->task_runner()->PostTask(FROM_HERE, BindOnce(&NopTask));
  EXPECT_EQ(1u, sequence_manager()->GetPendingTaskCountForTesting());
}

TEST_P(SequenceManagerTest, GetPendingTaskCountForTesting) {
  auto queues = CreateTaskQueues(3u);

  EXPECT_EQ(0u, sequence_manager()->GetPendingTaskCountForTesting());

  queues[0]->task_runner()->PostTask(FROM_HERE, BindOnce(&NopTask));
  EXPECT_EQ(1u, sequence_manager()->GetPendingTaskCountForTesting());

  queues[0]->task_runner()->PostTask(FROM_HERE, BindOnce(&NopTask));
  EXPECT_EQ(2u, sequence_manager()->GetPendingTaskCountForTesting());

  queues[0]->task_runner()->PostTask(FROM_HERE, BindOnce(&NopTask));
  EXPECT_EQ(3u, sequence_manager()->GetPendingTaskCountForTesting());

  queues[1]->task_runner()->PostTask(FROM_HERE, BindOnce(&NopTask));
  EXPECT_EQ(4u, sequence_manager()->GetPendingTaskCountForTesting());

  queues[2]->task_runner()->PostTask(FROM_HERE, BindOnce(&NopTask));
  EXPECT_EQ(5u, sequence_manager()->GetPendingTaskCountForTesting());

  queues[1]->task_runner()->PostDelayedTask(FROM_HERE, BindOnce(&NopTask),
                                            TimeDelta::FromMilliseconds(10));
  EXPECT_EQ(6u, sequence_manager()->GetPendingTaskCountForTesting());

  queues[2]->task_runner()->PostDelayedTask(FROM_HERE, BindOnce(&NopTask),
                                            TimeDelta::FromMilliseconds(20));
  EXPECT_EQ(7u, sequence_manager()->GetPendingTaskCountForTesting());

  RunLoop().RunUntilIdle();
  EXPECT_EQ(2u, sequence_manager()->GetPendingTaskCountForTesting());

  AdvanceMockTickClock(TimeDelta::FromMilliseconds(10));
  RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, sequence_manager()->GetPendingTaskCountForTesting());

  AdvanceMockTickClock(TimeDelta::FromMilliseconds(10));
  RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, sequence_manager()->GetPendingTaskCountForTesting());
}

TEST_P(SequenceManagerTest, PostDelayedTaskFromOtherThread) {
  scoped_refptr<TestTaskQueue> main_tq = CreateTaskQueue();
  scoped_refptr<TaskRunner> task_runner =
      main_tq->CreateTaskRunner(kTaskTypeNone);
  sequence_manager()->SetAddQueueTimeToTasks(true);

  Thread thread("test thread");
  thread.StartAndWaitForTesting();

  WaitableEvent task_posted(WaitableEvent::ResetPolicy::MANUAL,
                            WaitableEvent::InitialState::NOT_SIGNALED);
  thread.task_runner()->PostTask(
      FROM_HERE, BindOnce(
                     [](scoped_refptr<TaskRunner> task_runner,
                        WaitableEvent* task_posted) {
                       task_runner->PostDelayedTask(
                           FROM_HERE, BindOnce(&NopTask),
                           base::TimeDelta::FromMilliseconds(10));
                       task_posted->Signal();
                     },
                     std::move(task_runner), &task_posted));
  task_posted.Wait();
  FastForwardUntilNoTasksRemain();
  RunLoop().RunUntilIdle();
  thread.Stop();
}

namespace {

void PostTaskA(scoped_refptr<TaskRunner> task_runner) {
  task_runner->PostTask(FROM_HERE, BindOnce(&NopTask));
  task_runner->PostDelayedTask(FROM_HERE, BindOnce(&NopTask),
                               base::TimeDelta::FromMilliseconds(10));
}

void PostTaskB(scoped_refptr<TaskRunner> task_runner) {
  task_runner->PostTask(FROM_HERE, BindOnce(&NopTask));
  task_runner->PostDelayedTask(FROM_HERE, BindOnce(&NopTask),
                               base::TimeDelta::FromMilliseconds(20));
}

void PostTaskC(scoped_refptr<TaskRunner> task_runner) {
  task_runner->PostTask(FROM_HERE, BindOnce(&NopTask));
  task_runner->PostDelayedTask(FROM_HERE, BindOnce(&NopTask),
                               base::TimeDelta::FromMilliseconds(30));
}

}  // namespace

TEST_P(SequenceManagerTest, DescribeAllPendingTasks) {
  auto queues = CreateTaskQueues(3u);

  PostTaskA(queues[0]->task_runner());
  PostTaskB(queues[1]->task_runner());
  PostTaskC(queues[2]->task_runner());

  std::string description = sequence_manager()->DescribeAllPendingTasks();
  EXPECT_THAT(description, HasSubstr("PostTaskA@"));
  EXPECT_THAT(description, HasSubstr("PostTaskB@"));
  EXPECT_THAT(description, HasSubstr("PostTaskC@"));
}

TEST_P(SequenceManagerTest, TaskPriortyInterleaving) {
  auto queues = CreateTaskQueues(TaskQueue::QueuePriority::kQueuePriorityCount);

  for (uint8_t priority = 0;
       priority < TaskQueue::QueuePriority::kQueuePriorityCount; priority++) {
    if (priority != TaskQueue::QueuePriority::kNormalPriority) {
      queues[priority]->SetQueuePriority(
          static_cast<TaskQueue::QueuePriority>(priority));
    }
  }

  std::string order;
  for (int i = 0; i < 60; i++) {
    for (uint8_t priority = 0;
         priority < TaskQueue::QueuePriority::kQueuePriorityCount; priority++) {
      queues[priority]->task_runner()->PostTask(
          FROM_HERE,
          base::BindOnce([](std::string* str, char c) { str->push_back(c); },
                         &order, '0' + priority));
    }
  }

  RunLoop().RunUntilIdle();

  EXPECT_EQ(order,
            "000000000000000000000000000000000000000000000000000000000000"
            "111111111111111111111111111111111111111111111111111111111111"
            "222222222222222222222222222222222222222222222222222222222222"
            "333333333333333333333333333333333333333333333333333333333333"
            "444444444444444444444444444444444444444444444444444444444444"
            "555555555555555555555555555555555555555555555555555555555555"
            "666666666666666666666666666666666666666666666666666666666666");
}

namespace {

class CancelableTaskWithDestructionObserver {
 public:
  CancelableTaskWithDestructionObserver() {}

  void Task(std::unique_ptr<ScopedClosureRunner> destruction_observer) {
    destruction_observer_ = std::move(destruction_observer);
  }

  std::unique_ptr<ScopedClosureRunner> destruction_observer_;
  WeakPtrFactory<CancelableTaskWithDestructionObserver> weak_factory_{this};
};

}  // namespace

TEST_P(SequenceManagerTest, PeriodicHousekeeping) {
  auto queue = CreateTaskQueue();

  // Post a task that will trigger housekeeping.
  queue->task_runner()->PostDelayedTask(
      FROM_HERE, BindOnce(&NopTask),
      SequenceManagerImpl::kReclaimMemoryInterval);

  // Posts some tasks set to run long in the future and then cancel some of
  // them.
  bool task1_deleted = false;
  bool task2_deleted = false;
  bool task3_deleted = false;
  CancelableTaskWithDestructionObserver task1;
  CancelableTaskWithDestructionObserver task2;
  CancelableTaskWithDestructionObserver task3;

  queue->task_runner()->PostDelayedTask(
      FROM_HERE,
      BindOnce(&CancelableTaskWithDestructionObserver::Task,
               task1.weak_factory_.GetWeakPtr(),
               std::make_unique<ScopedClosureRunner>(
                   BindLambdaForTesting([&]() { task1_deleted = true; }))),
      TimeDelta::FromHours(1));

  queue->task_runner()->PostDelayedTask(
      FROM_HERE,
      BindOnce(&CancelableTaskWithDestructionObserver::Task,
               task2.weak_factory_.GetWeakPtr(),
               std::make_unique<ScopedClosureRunner>(
                   BindLambdaForTesting([&]() { task2_deleted = true; }))),
      TimeDelta::FromHours(2));

  queue->task_runner()->PostDelayedTask(
      FROM_HERE,
      BindOnce(&CancelableTaskWithDestructionObserver::Task,
               task3.weak_factory_.GetWeakPtr(),
               std::make_unique<ScopedClosureRunner>(
                   BindLambdaForTesting([&]() { task3_deleted = true; }))),
      TimeDelta::FromHours(3));

  task2.weak_factory_.InvalidateWeakPtrs();
  task3.weak_factory_.InvalidateWeakPtrs();

  EXPECT_FALSE(task1_deleted);
  EXPECT_FALSE(task2_deleted);
  EXPECT_FALSE(task3_deleted);

  // This should trigger housekeeping which will sweep away the canceled tasks.
  FastForwardBy(SequenceManagerImpl::kReclaimMemoryInterval);

  EXPECT_FALSE(task1_deleted);
  EXPECT_TRUE(task2_deleted);
  EXPECT_TRUE(task3_deleted);

  // Tidy up.
  FastForwardUntilNoTasksRemain();
}

namespace {

class MockCrashKeyImplementation : public debug::CrashKeyImplementation {
 public:
  MOCK_METHOD2(Allocate,
               debug::CrashKeyString*(const char name[], debug::CrashKeySize));
  MOCK_METHOD2(Set, void(debug::CrashKeyString*, StringPiece));
  MOCK_METHOD1(Clear, void(debug::CrashKeyString*));
};

}  // namespace

TEST_P(SequenceManagerTest, CrashKeys) {
  testing::InSequence sequence;
  auto queue = CreateTaskQueue();
  auto runner = queue->CreateTaskRunner(kTaskTypeNone);
  auto crash_key_impl = std::make_unique<MockCrashKeyImplementation>();
  RunLoop run_loop;

  MockCrashKeyImplementation* mock_impl = crash_key_impl.get();
  debug::SetCrashKeyImplementation(std::move(crash_key_impl));
  debug::CrashKeyString dummy_key("dummy", debug::CrashKeySize::Size64);

  // Parent task.
  auto parent_location = FROM_HERE;
  auto expected_stack1 = StringPrintf(
      "0x%zX 0x0",
      reinterpret_cast<uintptr_t>(parent_location.program_counter()));
  EXPECT_CALL(*mock_impl, Allocate(_, _)).WillRepeatedly(Return(&dummy_key));
  EXPECT_CALL(*mock_impl, Set(_, testing::Eq(expected_stack1)));

  // Child task.
  auto location = FROM_HERE;
  auto expected_stack2 = StringPrintf(
      "0x%zX 0x%zX", reinterpret_cast<uintptr_t>(location.program_counter()),
      reinterpret_cast<uintptr_t>(parent_location.program_counter()));
  EXPECT_CALL(*mock_impl, Set(_, testing::Eq(expected_stack2)));

  sequence_manager()->EnableCrashKeys("test-async-stack");

  // Run a task that posts another task to establish an asynchronous call stack.
  runner->PostTask(parent_location, BindLambdaForTesting([&]() {
                     runner->PostTask(location, run_loop.QuitClosure());
                   }));
  run_loop.Run();

  debug::SetCrashKeyImplementation(nullptr);
}

TEST_P(SequenceManagerTest, CrossQueueTaskPostingWhenQueueDeleted) {
  MockTask task;
  auto queue_1 = CreateTaskQueue();
  auto queue_2 = CreateTaskQueue();

  EXPECT_CALL(task, Run).Times(1);

  queue_1->task_runner()->PostDelayedTask(
      FROM_HERE, PostOnDestruction(queue_2, task.Get()),
      TimeDelta::FromMinutes(1));

  queue_1->ShutdownTaskQueue();

  FastForwardUntilNoTasksRemain();
}

TEST_P(SequenceManagerTest, UnregisterTaskQueueTriggersScheduleWork) {
  constexpr auto kDelay = TimeDelta::FromMinutes(1);
  auto queue_1 = CreateTaskQueue();
  auto queue_2 = CreateTaskQueue();

  MockTask task;
  EXPECT_CALL(task, Run).Times(1);

  queue_1->task_runner()->PostDelayedTask(FROM_HERE, task.Get(), kDelay);
  queue_2->task_runner()->PostDelayedTask(FROM_HERE, task.Get(), kDelay * 2);

  AdvanceMockTickClock(kDelay * 2);

  // Wakeup time needs to be adjusted to kDelay * 2 when the queue is
  // unregistered from the TimeDomain
  queue_1->ShutdownTaskQueue();

  RunLoop().RunUntilIdle();
}

TEST_P(SequenceManagerTest, ReclaimMemoryRemovesCorrectQueueFromSet) {
  auto queue1 = CreateTaskQueue();
  auto queue2 = CreateTaskQueue();
  auto queue3 = CreateTaskQueue();
  auto queue4 = CreateTaskQueue();

  std::vector<int> order;

  CancelableRepeatingClosure cancelable_closure1(
      BindLambdaForTesting([&]() { order.push_back(10); }));
  CancelableRepeatingClosure cancelable_closure2(
      BindLambdaForTesting([&]() { order.push_back(11); }));
  queue1->task_runner()->PostTask(FROM_HERE, BindLambdaForTesting([&]() {
                                    order.push_back(1);
                                    cancelable_closure1.Cancel();
                                    cancelable_closure2.Cancel();
                                    // This should remove |queue4| from the work
                                    // queue set,
                                    sequence_manager()->ReclaimMemory();
                                  }));
  queue2->task_runner()->PostTask(
      FROM_HERE, BindLambdaForTesting([&]() { order.push_back(2); }));
  queue3->task_runner()->PostTask(
      FROM_HERE, BindLambdaForTesting([&]() { order.push_back(3); }));
  queue4->task_runner()->PostTask(FROM_HERE, cancelable_closure1.callback());
  queue4->task_runner()->PostTask(FROM_HERE, cancelable_closure2.callback());

  RunLoop().RunUntilIdle();

  // Make sure ReclaimMemory didn't prevent the task from |queue2| from running.
  EXPECT_THAT(order, ElementsAre(1, 2, 3));
}

TEST_P(SequenceManagerTest, OnNativeWorkPending) {
  MockTask task;
  auto queue = CreateTaskQueue();
  queue->SetQueuePriority(TaskQueue::QueuePriority::kNormalPriority);

  auto CheckPostedTaskRan = [&](bool should_have_run) {
    EXPECT_CALL(task, Run).Times(should_have_run ? 1 : 0);
    RunLoop().RunUntilIdle();
    Mock::VerifyAndClearExpectations(&task);
  };

  // Scheduling native work with higher priority causes the posted task to be
  // deferred.
  auto native_work = sequence_manager()->OnNativeWorkPending(
      TaskQueue::QueuePriority::kHighPriority);
  queue->task_runner()->PostTask(FROM_HERE, task.Get());
  CheckPostedTaskRan(false);

  // Once the native work completes, the posted task is free to execute.
  native_work.reset();
  CheckPostedTaskRan(true);

  // Lower priority native work doesn't preempt posted tasks.
  native_work = sequence_manager()->OnNativeWorkPending(
      TaskQueue::QueuePriority::kLowPriority);
  queue->task_runner()->PostTask(FROM_HERE, task.Get());
  CheckPostedTaskRan(true);

  // Equal priority native work doesn't preempt posted tasks.
  native_work = sequence_manager()->OnNativeWorkPending(
      TaskQueue::QueuePriority::kNormalPriority);
  queue->task_runner()->PostTask(FROM_HERE, task.Get());
  CheckPostedTaskRan(true);

  // When there are multiple priorities of native work, only the highest
  // priority matters.
  native_work = sequence_manager()->OnNativeWorkPending(
      TaskQueue::QueuePriority::kNormalPriority);
  auto native_work_high = sequence_manager()->OnNativeWorkPending(
      TaskQueue::QueuePriority::kHighPriority);
  auto native_work_low = sequence_manager()->OnNativeWorkPending(
      TaskQueue::QueuePriority::kLowPriority);
  queue->task_runner()->PostTask(FROM_HERE, task.Get());
  CheckPostedTaskRan(false);
  native_work.reset();
  CheckPostedTaskRan(false);
  native_work_high.reset();
  CheckPostedTaskRan(true);
}

namespace {

class TaskObserverExpectingNoDelayedRunTime : public TaskObserver {
 public:
  TaskObserverExpectingNoDelayedRunTime() = default;
  ~TaskObserverExpectingNoDelayedRunTime() override = default;

  int num_will_process_task() const { return num_will_process_task_; }
  int num_did_process_task() const { return num_did_process_task_; }

 private:
  void WillProcessTask(const base::PendingTask& pending_task,
                       bool was_blocked_or_low_priority) override {
    EXPECT_TRUE(pending_task.delayed_run_time.is_null());
    ++num_will_process_task_;
  }
  void DidProcessTask(const base::PendingTask& pending_task) override {
    EXPECT_TRUE(pending_task.delayed_run_time.is_null());
    ++num_did_process_task_;
  }

  int num_will_process_task_ = 0;
  int num_did_process_task_ = 0;
};

}  // namespace

// The |delayed_run_time| must not be set for immediate tasks as that prevents
// external observers from correctly identifying delayed tasks.
// https://crbug.com/1029137
TEST_P(SequenceManagerTest, NoDelayedRunTimeForImmediateTask) {
  TaskObserverExpectingNoDelayedRunTime task_observer;
  sequence_manager()->SetAddQueueTimeToTasks(true);
  sequence_manager()->AddTaskObserver(&task_observer);
  auto queue = CreateTaskQueue();

  base::RunLoop run_loop;
  queue->task_runner()->PostTask(
      FROM_HERE, BindLambdaForTesting([&]() { run_loop.Quit(); }));
  run_loop.Run();

  EXPECT_EQ(1, task_observer.num_will_process_task());
  EXPECT_EQ(1, task_observer.num_did_process_task());

  sequence_manager()->RemoveTaskObserver(&task_observer);
}

TEST_P(SequenceManagerTest, TaskObserverBlockedOrLowPriority_QueueDisabled) {
  auto queue = CreateTaskQueue();
  testing::StrictMock<MockTaskObserver> observer;
  sequence_manager()->AddTaskObserver(&observer);

  queue->task_runner()->PostTask(FROM_HERE, DoNothing());
  queue->GetTaskQueueImpl()->SetQueueEnabled(false);
  queue->GetTaskQueueImpl()->SetQueueEnabled(true);

  EXPECT_CALL(observer,
              WillProcessTask(_, /*was_blocked_or_low_priority=*/true));
  EXPECT_CALL(observer, DidProcessTask(_));
  RunLoop().RunUntilIdle();

  sequence_manager()->RemoveTaskObserver(&observer);
}

TEST_P(SequenceManagerTest,
       TaskObserverBlockedOrLowPriority_FenceBeginningOfTime) {
  auto queue = CreateTaskQueue();
  testing::StrictMock<MockTaskObserver> observer;
  sequence_manager()->AddTaskObserver(&observer);

  queue->task_runner()->PostTask(FROM_HERE, DoNothing());
  queue->GetTaskQueueImpl()->InsertFence(
      TaskQueue::InsertFencePosition::kBeginningOfTime);
  queue->GetTaskQueueImpl()->RemoveFence();

  EXPECT_CALL(observer,
              WillProcessTask(_, /*was_blocked_or_low_priority=*/true));
  EXPECT_CALL(observer, DidProcessTask(_));
  RunLoop().RunUntilIdle();

  sequence_manager()->RemoveTaskObserver(&observer);
}

TEST_P(SequenceManagerTest,
       TaskObserverBlockedOrLowPriority_PostedBeforeFenceNow) {
  auto queue = CreateTaskQueue();
  testing::StrictMock<MockTaskObserver> observer;
  sequence_manager()->AddTaskObserver(&observer);

  queue->task_runner()->PostTask(FROM_HERE, DoNothing());
  queue->GetTaskQueueImpl()->InsertFence(TaskQueue::InsertFencePosition::kNow);
  queue->GetTaskQueueImpl()->RemoveFence();

  EXPECT_CALL(observer,
              WillProcessTask(_, /*was_blocked_or_low_priority=*/false));
  EXPECT_CALL(observer, DidProcessTask(_));
  RunLoop().RunUntilIdle();

  sequence_manager()->RemoveTaskObserver(&observer);
}

TEST_P(SequenceManagerTest,
       TaskObserverBlockedOrLowPriority_PostedAfterFenceNow) {
  auto queue = CreateTaskQueue();
  testing::StrictMock<MockTaskObserver> observer;
  sequence_manager()->AddTaskObserver(&observer);

  queue->GetTaskQueueImpl()->InsertFence(TaskQueue::InsertFencePosition::kNow);
  queue->task_runner()->PostTask(FROM_HERE, DoNothing());
  queue->GetTaskQueueImpl()->RemoveFence();

  EXPECT_CALL(observer,
              WillProcessTask(_, /*was_blocked_or_low_priority=*/true));
  EXPECT_CALL(observer, DidProcessTask(_));
  RunLoop().RunUntilIdle();

  sequence_manager()->RemoveTaskObserver(&observer);
}

TEST_P(SequenceManagerTest,
       TaskObserverBlockedOrLowPriority_LowerPriorityWhileQueued) {
  auto queue = CreateTaskQueue();
  testing::StrictMock<MockTaskObserver> observer;
  sequence_manager()->AddTaskObserver(&observer);

  queue->task_runner()->PostTask(FROM_HERE, DoNothing());
  queue->SetQueuePriority(TaskQueue::QueuePriority::kLowPriority);
  queue->SetQueuePriority(TaskQueue::QueuePriority::kNormalPriority);

  EXPECT_CALL(observer,
              WillProcessTask(_, /*was_blocked_or_low_priority=*/true));
  EXPECT_CALL(observer, DidProcessTask(_));
  RunLoop().RunUntilIdle();

  sequence_manager()->RemoveTaskObserver(&observer);
}

TEST_P(SequenceManagerTest,
       TaskObserverBlockedOrLowPriority_LowPriorityWhenQueueing) {
  auto queue = CreateTaskQueue();
  testing::StrictMock<MockTaskObserver> observer;
  sequence_manager()->AddTaskObserver(&observer);

  queue->SetQueuePriority(TaskQueue::QueuePriority::kLowPriority);
  queue->task_runner()->PostTask(FROM_HERE, DoNothing());
  queue->SetQueuePriority(TaskQueue::QueuePriority::kNormalPriority);

  EXPECT_CALL(observer,
              WillProcessTask(_, /*was_blocked_or_low_priority=*/true));
  EXPECT_CALL(observer, DidProcessTask(_));
  RunLoop().RunUntilIdle();

  sequence_manager()->RemoveTaskObserver(&observer);
}

TEST_P(SequenceManagerTest,
       TaskObserverBlockedOrLowPriority_LowPriorityWhenRunning) {
  auto queue = CreateTaskQueue();
  testing::StrictMock<MockTaskObserver> observer;
  sequence_manager()->AddTaskObserver(&observer);

  queue->task_runner()->PostTask(FROM_HERE, DoNothing());
  queue->SetQueuePriority(TaskQueue::QueuePriority::kLowPriority);

  EXPECT_CALL(observer,
              WillProcessTask(_, /*was_blocked_or_low_priority=*/true));
  EXPECT_CALL(observer, DidProcessTask(_));
  RunLoop().RunUntilIdle();

  sequence_manager()->RemoveTaskObserver(&observer);
}

TEST_P(SequenceManagerTest,
       TaskObserverBlockedOrLowPriority_TaskObserverUnblockedWithBacklog) {
  auto queue = CreateTaskQueue();
  testing::StrictMock<MockTaskObserver> observer;
  sequence_manager()->AddTaskObserver(&observer);

  queue->SetQueuePriority(TaskQueue::QueuePriority::kLowPriority);
  queue->task_runner()->PostTask(FROM_HERE, DoNothing());
  queue->GetTaskQueueImpl()->InsertFence(
      TaskQueue::InsertFencePosition::kBeginningOfTime);
  queue->task_runner()->PostTask(FROM_HERE, DoNothing());
  queue->GetTaskQueueImpl()->RemoveFence();
  queue->task_runner()->PostTask(FROM_HERE, DoNothing());
  queue->SetQueuePriority(TaskQueue::QueuePriority::kNormalPriority);
  // Post a task while the queue is kNormalPriority and unblocked, but has a
  // backlog of tasks that were blocked.
  queue->task_runner()->PostTask(FROM_HERE, DoNothing());

  EXPECT_CALL(observer,
              WillProcessTask(_, /*was_blocked_or_low_priority=*/true))
      .Times(3);
  EXPECT_CALL(observer, DidProcessTask(_)).Times(4);
  EXPECT_CALL(observer,
              WillProcessTask(_, /*was_blocked_or_low_priority=*/false));
  RunLoop().RunUntilIdle();
  testing::Mock::VerifyAndClear(&observer);

  sequence_manager()->RemoveTaskObserver(&observer);
}

TEST_P(SequenceManagerTest, TaskObserverBlockedOrLowPriority_Mix) {
  auto queue = CreateTaskQueue();
  testing::StrictMock<MockTaskObserver> observer;
  sequence_manager()->AddTaskObserver(&observer);

  queue->SetQueuePriority(TaskQueue::QueuePriority::kLowPriority);
  queue->task_runner()->PostTask(FROM_HERE, DoNothing());
  queue->GetTaskQueueImpl()->InsertFence(
      TaskQueue::InsertFencePosition::kBeginningOfTime);
  queue->task_runner()->PostTask(FROM_HERE, DoNothing());
  queue->GetTaskQueueImpl()->RemoveFence();
  queue->task_runner()->PostTask(FROM_HERE, DoNothing());

  EXPECT_CALL(observer,
              WillProcessTask(_, /*was_blocked_or_low_priority=*/true))
      .Times(3);
  EXPECT_CALL(observer, DidProcessTask(_)).Times(3);
  RunLoop().RunUntilIdle();
  testing::Mock::VerifyAndClear(&observer);

  queue->SetQueuePriority(TaskQueue::QueuePriority::kNormalPriority);
  queue->task_runner()->PostTask(FROM_HERE, DoNothing());
  EXPECT_CALL(observer,
              WillProcessTask(_, /*was_blocked_or_low_priority=*/false));
  EXPECT_CALL(observer, DidProcessTask(_));
  RunLoop().RunUntilIdle();

  sequence_manager()->RemoveTaskObserver(&observer);
}

}  // namespace internal
}  // namespace sequence_manager
}  // namespace base
