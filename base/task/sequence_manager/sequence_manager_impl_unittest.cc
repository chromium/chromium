// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/sequence_manager/sequence_manager_impl.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "base/auto_reset.h"
#include "base/cancelable_callback.h"
#include "base/debug/stack_trace.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/message_loop/message_pump_default.h"
#include "base/message_loop/message_pump_type.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/sequence_checker_impl.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/current_thread.h"
#include "base/task/sequence_manager/sequence_manager.h"
#include "base/task/sequence_manager/task_queue.h"
#include "base/task/sequence_manager/task_queue_impl.h"
#include "base/task/sequence_manager/task_queue_selector.h"
#include "base/task/sequence_manager/tasks.h"
#include "base/task/sequence_manager/test/mock_time_domain.h"
#include "base/task/sequence_manager/test/mock_time_message_pump.h"
#include "base/task/sequence_manager/test/sequence_manager_for_test.h"
#include "base/task/sequence_manager/test/test_task_time_observer.h"
#include "base/task/sequence_manager/thread_controller_with_message_pump_impl.h"
#include "base/task/sequence_manager/work_queue.h"
#include "base/task/sequence_manager/work_queue_sets.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_features.h"
#include "base/task/thread_pool.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/null_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/task_environment.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/test/test_simple_task_runner.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/threading/sequence_local_storage_slot.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "base/trace_event/base_tracing.h"
#include "base/tracing_buildflags.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"

#if BUILDFLAG(ENABLE_BASE_TRACING)
#include <optional>

#include "base/test/trace_event_analyzer.h"
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

enum class RunnerType {
  kMockTaskRunner,
  kMessagePump,
};

enum class WakeUpType {
  kDefault,
  kAlign,
};

// Expresses whether metrics subsampling in ThreadController should always or
// never sample which affects the count of calls to Now().
enum class MetricsSampling {
  kMetricsOn,
  kMetricsOff,
};

enum class TestQueuePriority : TaskQueue::QueuePriority {
  kControlPriority = 0,
  kHighestPriority = 1,
  kVeryHighPriority = 2,
  kHighPriority = 3,

  kNormalPriority = 4,
  kDefaultPriority = kNormalPriority,

  kLowPriority = 5,
  kBestEffortPriority = 6,
  kQueuePriorityCount = 7,
  kFirstQueuePriority = kControlPriority,
};

std::string ToString(RunnerType type) {
  switch (type) {
    case RunnerType::kMockTaskRunner:
      return "kMockTaskRunner";
    case RunnerType::kMessagePump:
      return "kMessagePump";
  }
}

std::string ToString(WakeUpType type) {
  switch (type) {
    case WakeUpType::kDefault:
      return "";
    case WakeUpType::kAlign:
      return "AlignedWakeUps";
  }
}

std::string ToString(MetricsSampling sampling) {
  switch (sampling) {
    case MetricsSampling::kMetricsOn:
      return "MetricsOn";
    case MetricsSampling::kMetricsOff:
      return "MetricsOff";
  }
}

std::string GetTestNameSuffix(
    const testing::TestParamInfo<
        std::tuple<RunnerType, WakeUpType, MetricsSampling>>& info) {
  return StrCat({"With", ToString(std::get<0>(info.param)).substr(1),
                 ToString(std::get<1>(info.param)),
                 ToString(std::get<2>(info.param))});
}

TaskQueueImpl* GetTaskQueueImpl(TaskQueue* task_queue) {
  return static_cast<TaskQueueImpl*>(task_queue);
}

constexpr TimeDelta kLeeway = kDefaultLeeway;

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
  virtual TimeTicks NextPendingTaskTime() const = 0;
  // Keeps advancing time as needed to run tasks up to the specified limit.
  virtual void FastForwardBy(TimeDelta delta) = 0;
  // Keeps advancing time as needed to run tasks until no more tasks are
  // available.
  virtual void FastForwardUntilNoTasksRemain() = 0;
  virtual void RunDoWorkOnce() = 0;
  virtual SequenceManagerForTest* sequence_manager() const = 0;
  virtual void DestroySequenceManager() = 0;
  virtual int GetNowTicksCallCount() = 0;
  virtual TimeTicks FromStartAligned(TimeDelta delta) const = 0;
};

class CallCountingTickClock : public TickClock {
 public:
  explicit CallCountingTickClock(RepeatingCallback<TimeTicks()> now_callback)
      : now_callback_(std::move(now_callback)) {}
  explicit CallCountingTickClock(TickClock* clock)
      : CallCountingTickClock(
            BindLambdaForTesting([clock] { return clock->NowTicks(); })) {}

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
            SingleThreadTaskRunner::GetCurrentDefault(),
            mock_tick_clock(),
            SequenceManager::Settings::Builder()
                .SetMessagePumpType(MessagePumpType::DEFAULT)
                .SetRandomisedSamplingEnabled(false)
                .SetTickClock(mock_tick_clock())
                .SetPrioritySettings(SequenceManager::PrioritySettings(
                    TestQueuePriority::kQueuePriorityCount,
                    TestQueuePriority::kDefaultPriority))
                .Build())) {
    // A null clock triggers some assertions.
    AdvanceMockTickClock(Milliseconds(1));
    start_time_ = test_task_runner_->NowTicks();

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

  TimeTicks NextPendingTaskTime() const override {
    return test_task_runner_->NowTicks() +
           test_task_runner_->NextPendingTaskDelay();
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

  TimeTicks FromStartAligned(TimeDelta delta) const override {
    return start_time_ + delta;
  }

 private:
  scoped_refptr<TestMockTimeTaskRunner> test_task_runner_;
  CallCountingTickClock call_counting_clock_;
  std::unique_ptr<SequenceManagerForTest> sequence_manager_;
  TimeTicks start_time_;
};

class FixtureWithMockMessagePump : public Fixture {
 public:
  explicit FixtureWithMockMessagePump(WakeUpType wake_up_type)
      : call_counting_clock_(&mock_clock_), wake_up_type_(wake_up_type) {
    if (wake_up_type_ == WakeUpType::kAlign) {
      feature_list_.InitWithFeatures(
          {kAlignWakeUps, kExplicitHighResolutionTimerWin}, {});
    } else {
      feature_list_.InitWithFeatures(
          {}, {kAlignWakeUps, kExplicitHighResolutionTimerWin});
    }
    // A null clock triggers some assertions.
    mock_clock_.Advance(Milliseconds(1));

    auto pump = std::make_unique<MockTimeMessagePump>(&mock_clock_);
    pump_ = pump.get();
    auto settings = SequenceManager::Settings::Builder()
                        .SetMessagePumpType(MessagePumpType::DEFAULT)
                        .SetRandomisedSamplingEnabled(false)
                        .SetTickClock(mock_tick_clock())
                        .SetPrioritySettings(SequenceManager::PrioritySettings(
                            TestQueuePriority::kQueuePriorityCount,
                            TestQueuePriority::kDefaultPriority))
                        .Build();
    auto thread_controller =
        std::make_unique<ThreadControllerWithMessagePumpImpl>(std::move(pump),
                                                              settings);
    MessagePump::InitializeFeatures();
    ThreadControllerWithMessagePumpImpl::InitializeFeatures();
    sequence_manager_ = SequenceManagerForTest::Create(
        std::move(thread_controller), std::move(settings));
    sequence_manager_->SetDefaultTaskRunner(MakeRefCounted<NullTaskRunner>());
    start_time_ = mock_clock_.NowTicks();

    // The SequenceManager constructor calls Now() once for setting up
    // housekeeping.
    EXPECT_EQ(1, GetNowTicksCallCount());
    call_counting_clock_.Reset();
  }
  ~FixtureWithMockMessagePump() override {
    ThreadControllerWithMessagePumpImpl::ResetFeatures();
  }

  void AdvanceMockTickClock(TimeDelta delta) override {
    mock_clock_.Advance(delta);
  }

  const TickClock* mock_tick_clock() const override {
    return &call_counting_clock_;
  }

  TimeTicks NextPendingTaskTime() const override {
    return pump_->next_wake_up_time();
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

  TimeTicks FromStartAligned(TimeDelta delta) const override {
    if (wake_up_type_ == WakeUpType::kAlign) {
      return (start_time_ + delta).SnappedToNextTick(TimeTicks(), kLeeway);
    }
    return start_time_ + delta;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  SimpleTestTickClock mock_clock_;
  CallCountingTickClock call_counting_clock_;

  // Must outlive `pump_`.
  std::unique_ptr<SequenceManagerForTest> sequence_manager_;

  raw_ptr<MockTimeMessagePump> pump_ = nullptr;
  WakeUpType wake_up_type_;
  TimeTicks start_time_;
};

// Convenience wrapper around the fixtures so that we can use parametrized tests
// instead of templated ones. The latter would be more verbose as all method
// calls to the fixture would need to be like this->method()
class SequenceManagerTest
    : public testing::TestWithParam<
          std::tuple<RunnerType, WakeUpType, MetricsSampling>>,
      public Fixture {
 public:
  SequenceManagerTest() {
    switch (GetUnderlyingRunnerType()) {
      case RunnerType::kMockTaskRunner:
        fixture_ = std::make_unique<FixtureWithMockTaskRunner>();
        break;
      case RunnerType::kMessagePump:
        fixture_ =
            std::make_unique<FixtureWithMockMessagePump>(GetWakeUpType());
        break;
    }

    if (GetSampling() == MetricsSampling::kMetricsOn) {
      always_sample_scoper_.emplace();
    } else {
      never_sample_scoper_.emplace();
    }
  }

  // Accounts for the extra calls to Now() that come when sampling is enabled.
  int GetExtraNowSampleCount() {
    // When no extra metrics are sampled there are no extra Now() calls.
    if (GetSampling() == MetricsSampling::kMetricsOff) {
      return 0;
    }

    // In both cases when sampling metrics there is a new call to Now() when
    // ThreadController goes idle and the LazyNow instance used
    // has no value. There is an equivalent use of LazyNow upon becoming active.
    // In the case of RunnerType::kMessagePump the LazyNow has no value but it
    // does when using RunnerType::kMockTaskRunner since it was already
    // populated on entering OnWorkStarted().
    switch (GetUnderlyingRunnerType()) {
      case RunnerType::kMockTaskRunner:
        return 1;
      case RunnerType::kMessagePump:
        return 2;
    }
  }

  TaskQueue::Handle CreateTaskQueue(
      TaskQueue::Spec spec = TaskQueue::Spec(QueueName::TEST_TQ)) {
    return sequence_manager()->CreateTaskQueue(spec);
  }

  std::vector<TaskQueue::Handle> CreateTaskQueues(size_t num_queues) {
    std::vector<TaskQueue::Handle> queues;
    for (size_t i = 0; i < num_queues; i++)
      queues.push_back(CreateTaskQueue());
    return queues;
  }

  void RunUntilManagerIsIdle(RepeatingClosure per_run_time_callback) {
    for (;;) {
      // Advance time if we've run out of immediate work to do.
      if (!sequence_manager()->HasImmediateWork()) {
        LazyNow lazy_now(mock_tick_clock());
        auto wake_up = sequence_manager()->GetNextDelayedWakeUp();
        if (wake_up.has_value()) {
          AdvanceMockTickClock(wake_up->time - lazy_now.Now());
          per_run_time_callback.Run();
        } else {
          break;
        }
      }
      RunLoop().RunUntilIdle();
    }
  }

  debug::CrashKeyString* dummy_key() { return &dummy_key_; }

  void AdvanceMockTickClock(TimeDelta delta) override {
    fixture_->AdvanceMockTickClock(delta);
  }

  const TickClock* mock_tick_clock() const override {
    return fixture_->mock_tick_clock();
  }

  TimeTicks NextPendingTaskTime() const override {
    return fixture_->NextPendingTaskTime();
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

  RunnerType GetUnderlyingRunnerType() { return std::get<0>(GetParam()); }
  WakeUpType GetWakeUpType() { return std::get<1>(GetParam()); }
  MetricsSampling GetSampling() { return std::get<2>(GetParam()); }

  TimeTicks FromStartAligned(TimeDelta delta) const override {
    return fixture_->FromStartAligned(delta);
  }

 private:
  std::optional<base::MetricsSubSampler::ScopedAlwaysSampleForTesting>
      always_sample_scoper_;
  std::optional<base::MetricsSubSampler::ScopedNeverSampleForTesting>
      never_sample_scoper_;
  debug::CrashKeyString dummy_key_{"dummy", debug::CrashKeySize::Size64};
  std::unique_ptr<Fixture> fixture_;
};

auto GetTestTypes() {
  return testing::Values(
      std::make_tuple(RunnerType::kMessagePump, WakeUpType::kDefault,
                      MetricsSampling::kMetricsOn),
      std::make_tuple(RunnerType::kMessagePump, WakeUpType::kDefault,
                      MetricsSampling::kMetricsOff),
#if !BUILDFLAG(IS_WIN)
      std::make_tuple(RunnerType::kMessagePump, WakeUpType::kAlign,
                      MetricsSampling::kMetricsOn),
      std::make_tuple(RunnerType::kMessagePump, WakeUpType::kAlign,
                      MetricsSampling::kMetricsOff),
#endif
      std::make_tuple(RunnerType::kMockTaskRunner, WakeUpType::kDefault,
                      MetricsSampling::kMetricsOn),
      std::make_tuple(RunnerType::kMockTaskRunner, WakeUpType::kDefault,
                      MetricsSampling::kMetricsOff));
}

INSTANTIATE_TEST_SUITE_P(All,
                         SequenceManagerTest,
                         GetTestTypes(),
                         GetTestNameSuffix);

void PostFromNestedRunloop(scoped_refptr<SingleThreadTaskRunner> runner,
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
  TestCountUsesTimeSource(const TestCountUsesTimeSource&) = delete;
  TestCountUsesTimeSource& operator=(const TestCountUsesTimeSource&) = delete;
  ~TestCountUsesTimeSource() override = default;

  TimeTicks NowTicks() const override {
    now_calls_count_++;
    // Don't return 0, as it triggers some assertions.
    return TimeTicks() + Seconds(1);
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
      FROM_HERE, BindLambdaForTesting([&] {
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

  // In the absence of calls to Now() for TimeObserver the only calls will
  // come from metrics. There will be one call when the ThreadController
  // becomes active and one when it becomes idle.
  int extra_call_count = 0;
  if (GetSampling() == MetricsSampling::kMetricsOn) {
    extra_call_count = 2;
  }
  EXPECT_EQ(0 + extra_call_count, GetNowTicksCallCount());
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
  // Now is called when we start work and then for each task when it's
  // completed. 1 + 6  = 7 calls.
  EXPECT_EQ(7 + GetExtraNowSampleCount(), GetNowTicksCallCount());
  sequence_manager()->RemoveTaskTimeObserver(&time_observer);
}

TEST_P(SequenceManagerTest,
       NowCalledMinimumNumberOfTimesToComputeTaskDurationsDelayedFenceAllowed) {
  TestTaskTimeObserver time_observer;
  sequence_manager()->SetWorkBatchSize(6);
  sequence_manager()->AddTaskTimeObserver(&time_observer);

  std::vector<TaskQueue::Handle> queues;
  for (size_t i = 0; i < 3; i++) {
    queues.push_back(CreateTaskQueue(
        TaskQueue::Spec(QueueName::TEST_TQ).SetDelayedFencesAllowed(true)));
  }

  queues[0]->task_runner()->PostTask(FROM_HERE, BindOnce(&NopTask));
  queues[0]->task_runner()->PostTask(FROM_HERE, BindOnce(&NopTask));
  queues[1]->task_runner()->PostTask(FROM_HERE, BindOnce(&NopTask));
  queues[1]->task_runner()->PostTask(FROM_HERE, BindOnce(&NopTask));
  queues[2]->task_runner()->PostTask(FROM_HERE, BindOnce(&NopTask));
  queues[2]->task_runner()->PostTask(FROM_HERE, BindOnce(&NopTask));

  RunLoop().RunUntilIdle();
  // Now is called each time a task is queued, when first task is started
  // running, and when a task is completed. 1 + 6 * 2 = 13 calls.
  EXPECT_EQ(13 + GetExtraNowSampleCount(), GetNowTicksCallCount());
  sequence_manager()->RemoveTaskTimeObserver(&time_observer);
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
  if (GetUnderlyingRunnerType() == RunnerType::kMockTaskRunner)
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
      FROM_HERE, BindOnce(&PostFromNestedRunloop, queue->task_runner(),
                          Unretained(&tasks_to_post_from_nested_loop)));

  RunLoop().RunUntilIdle();
  // Note we expect tasks 3 & 4 to run last because they're non-nestable.
  EXPECT_THAT(run_order, ElementsAre(1u, 2u, 5u, 6u, 3u, 4u));
}

TEST_P(SequenceManagerTest, NonNestableTasksShutdownQueue) {
  // TestMockTimeTaskRunner doesn't support nested loops.
  if (GetUnderlyingRunnerType() == RunnerType::kMockTaskRunner) {
    return;
  }
  auto queue = CreateTaskQueue();

  std::vector<EnqueueOrder> run_order;

  std::vector<std::pair<OnceClosure, bool>> tasks_to_post_from_nested_loop;
  tasks_to_post_from_nested_loop.emplace_back(
      BindOnce(&TestTask, 1, &run_order), false);
  tasks_to_post_from_nested_loop.emplace_back(
      BindOnce(&TestTask, 2, &run_order), true);
  tasks_to_post_from_nested_loop.emplace_back(
      BindLambdaForTesting([&queue] { queue.reset(); }), true);

  queue->task_runner()->PostTask(
      FROM_HERE, BindOnce(&PostFromNestedRunloop, queue->task_runner(),
                          Unretained(&tasks_to_post_from_nested_loop)));

  RunLoop().RunUntilIdle();
  // We don't expect task 1 to run because the queue was shutdown.
  EXPECT_THAT(run_order, ElementsAre(2u));
}

TEST_P(SequenceManagerTest, NonNestableTaskQueueTimeShiftsToEndOfNestedLoop) {
  // TestMockTimeTaskRunner doesn't support nested loops.
  if (GetUnderlyingRunnerType() == RunnerType::kMockTaskRunner)
    return;

  auto queue = CreateTaskQueue();

  QueueTimeTaskObserver observer;
  sequence_manager()->AddTaskObserver(&observer);
  sequence_manager()->SetAddQueueTimeToTasks(true);

  RunLoop nested_run_loop(RunLoop::Type::kNestableTasksAllowed);

  const TimeTicks start_time = mock_tick_clock()->NowTicks();

  constexpr auto kTimeSpentInNestedLoop = Seconds(1);
  constexpr auto kTimeInTaskAfterNestedLoop = Seconds(3);

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
  queue->task_runner()->PostTask(FROM_HERE, BindLambdaForTesting([&] {
                                   TestTask(2, &run_order);
                                   nested_run_loop.Run();
                                 }));
  queue->task_runner()->PostTask(FROM_HERE, BindOnce(&TestTask, 3, &run_order));
  queue->task_runner()->PostNonNestableTask(FROM_HERE,
                                            BindOnce(&TestTask, 7, &run_order));
  queue->task_runner()->PostTask(FROM_HERE, BindLambdaForTesting([&] {
                                   TestTask(4, &run_order);
                                   AdvanceMockTickClock(kTimeSpentInNestedLoop);
                                 }));
  queue->task_runner()->PostNonNestableTask(
      FROM_HERE, BindLambdaForTesting([&] {
        TestTask(8, &run_order);
        AdvanceMockTickClock(kTimeInTaskAfterNestedLoop);
      }));
  queue->task_runner()->PostTask(FROM_HERE, BindOnce(&TestTask, 5, &run_order));
  queue->task_runner()->PostNonNestableTask(FROM_HERE,
                                            BindOnce(&TestTask, 9, &run_order));
  queue->task_runner()->PostTask(FROM_HERE, BindLambdaForTesting([&] {
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
                                TaskQueue* task_queue,
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
  if (GetUnderlyingRunnerType() == RunnerType::kMockTaskRunner)
    return;
  auto queue = CreateTaskQueue();
  std::vector<EnqueueOrder> run_order;

  std::vector<std::pair<OnceClosure, bool>> tasks_to_post_from_nested_loop;

  tasks_to_post_from_nested_loop.push_back(
      std::make_pair(BindOnce(&TestTask, 1, &run_order), false));
  tasks_to_post_from_nested_loop.push_back(
      std::make_pair(BindOnce(&InsertFenceAndPostTestTask, 2, &run_order,
                              queue.get(), sequence_manager()),
                     true));

  queue->task_runner()->PostTask(
      FROM_HERE, BindOnce(&PostFromNestedRunloop, queue->task_runner(),
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
  EXPECT_TRUE(GetTaskQueueImpl(queue.get())->immediate_work_queue()->Empty());
  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter =
      queue->CreateQueueEnabledVoter();
  voter->SetVoteToEnable(false);
  RunLoop().RunUntilIdle();
  EXPECT_FALSE(GetTaskQueueImpl(queue.get())->immediate_work_queue()->Empty());
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
  TimeDelta delay(Milliseconds(10));
  queue->task_runner()->PostDelayedTask(
      FROM_HERE, BindOnce(&TestTask, 1, &run_order), delay);
  EXPECT_FALSE(queue->HasTaskToRunImmediatelyOrReadyDelayedTask());
  AdvanceMockTickClock(delay);
  EXPECT_TRUE(queue->HasTaskToRunImmediatelyOrReadyDelayedTask());

  // Move the task into the |delayed_work_queue|.
  LazyNow lazy_now(mock_tick_clock());
  sequence_manager()->MoveReadyDelayedTasksToWorkQueues(&lazy_now);
  sequence_manager()->ScheduleWork();
  EXPECT_FALSE(GetTaskQueueImpl(queue.get())->delayed_work_queue()->Empty());
  EXPECT_TRUE(queue->HasTaskToRunImmediatelyOrReadyDelayedTask());

  // Run the task, making the queue empty.
  RunLoop().RunUntilIdle();
  EXPECT_TRUE(GetTaskQueueImpl(queue.get())->delayed_work_queue()->Empty());
}

TEST_P(SequenceManagerTest, DelayedTaskPosting) {
  auto queue = CreateTaskQueue();

  std::vector<EnqueueOrder> run_order;
  TimeDelta delay(Milliseconds(10));
  queue->task_runner()->PostDelayedTask(
      FROM_HERE, BindOnce(&TestTask, 1, &run_order), delay);
  EXPECT_EQ(FromStartAligned(Milliseconds(10)), NextPendingTaskTime());
  EXPECT_FALSE(queue->HasTaskToRunImmediatelyOrReadyDelayedTask());
  EXPECT_TRUE(run_order.empty());

  // The task doesn't run before the delay has completed.
  FastForwardBy(Milliseconds(9));
  EXPECT_TRUE(run_order.empty());

  // After the delay has completed, the task runs normally.
  FastForwardBy(Milliseconds(1));
  EXPECT_THAT(run_order, ElementsAre(1u));
  EXPECT_FALSE(queue->HasTaskToRunImmediatelyOrReadyDelayedTask());
}

TEST_P(SequenceManagerTest, DelayedTaskAtPosting) {
  auto queue = CreateTaskQueue();

  std::vector<EnqueueOrder> run_order;
  constexpr TimeDelta kDelay(Milliseconds(10));
  auto handle = queue->task_runner()->PostCancelableDelayedTaskAt(
      subtle::PostDelayedTaskPassKeyForTesting(), FROM_HERE,
      BindOnce(&TestTask, 1, &run_order),
      sequence_manager()->NowTicks() + kDelay,
      subtle::DelayPolicy::kFlexibleNoSooner);
  EXPECT_EQ(FromStartAligned(kDelay), NextPendingTaskTime());
  EXPECT_FALSE(queue->HasTaskToRunImmediatelyOrReadyDelayedTask());
  EXPECT_TRUE(run_order.empty());

  // The task doesn't run before the delay has completed.
  FastForwardBy(kDelay - Milliseconds(1));
  EXPECT_TRUE(run_order.empty());

  // After the delay has completed, the task runs normally.
  FastForwardBy(Milliseconds(1));
  EXPECT_THAT(run_order, ElementsAre(1u));
  EXPECT_FALSE(queue->HasTaskToRunImmediatelyOrReadyDelayedTask());
}

TEST_P(SequenceManagerTest, DelayedTaskAtPosting_FlexiblePreferEarly) {
  auto queue = CreateTaskQueue();

  TimeTicks start_time = sequence_manager()->NowTicks();
  std::vector<EnqueueOrder> run_order;
  constexpr TimeDelta kDelay(Milliseconds(20));
  auto handle = queue->task_runner()->PostCancelableDelayedTaskAt(
      subtle::PostDelayedTaskPassKeyForTesting(), FROM_HERE,
      BindOnce(&TestTask, 1, &run_order),
      sequence_manager()->NowTicks() + kDelay,
      subtle::DelayPolicy::kFlexiblePreferEarly);
  TimeTicks expected_run_time = start_time + kDelay;
  if (GetWakeUpType() == WakeUpType::kAlign) {
    expected_run_time =
        (start_time + kDelay - kLeeway).SnappedToNextTick(TimeTicks(), kLeeway);
  }
  EXPECT_EQ(expected_run_time, NextPendingTaskTime());
  EXPECT_FALSE(queue->HasTaskToRunImmediatelyOrReadyDelayedTask());
  EXPECT_TRUE(run_order.empty());
  LazyNow lazy_now(mock_tick_clock());
  EXPECT_EQ((WakeUp{start_time + kDelay, kLeeway, WakeUpResolution::kLow,
                    subtle::DelayPolicy::kFlexiblePreferEarly}),
            sequence_manager()->GetPendingWakeUp(&lazy_now));

  // The task doesn't run before the delay has completed.
  FastForwardBy(kDelay - kLeeway - Milliseconds(1));
  EXPECT_TRUE(run_order.empty());

  // After the delay has completed, the task runs normally.
  FastForwardBy(kLeeway + Milliseconds(1));
  EXPECT_THAT(run_order, ElementsAre(1u));
  EXPECT_FALSE(queue->HasTaskToRunImmediatelyOrReadyDelayedTask());
}

TEST_P(SequenceManagerTest, DelayedTaskAtPosting_MixedDelayPolicy) {
  auto queue = CreateTaskQueue();

  TimeTicks start_time = sequence_manager()->NowTicks();
  std::vector<EnqueueOrder> run_order;
  auto handle1 = queue->task_runner()->PostCancelableDelayedTaskAt(
      subtle::PostDelayedTaskPassKeyForTesting(), FROM_HERE,
      BindOnce(&TestTask, 2, &run_order),
      sequence_manager()->NowTicks() + Milliseconds(8),
      subtle::DelayPolicy::kFlexibleNoSooner);
  auto handle2 = queue->task_runner()->PostCancelableDelayedTaskAt(
      subtle::PostDelayedTaskPassKeyForTesting(), FROM_HERE,
      BindOnce(&TestTask, 1, &run_order),
      sequence_manager()->NowTicks() + Milliseconds(10),
      subtle::DelayPolicy::kPrecise);
  EXPECT_EQ(start_time + Milliseconds(10), NextPendingTaskTime());
  EXPECT_FALSE(queue->HasTaskToRunImmediatelyOrReadyDelayedTask());
  EXPECT_TRUE(run_order.empty());
  LazyNow lazy_now(mock_tick_clock());
  EXPECT_EQ((WakeUp{start_time + Milliseconds(10), kLeeway,
                    WakeUpResolution::kLow, subtle::DelayPolicy::kPrecise}),
            sequence_manager()->GetPendingWakeUp(&lazy_now));

  // The task doesn't run before the delay has completed.
  FastForwardBy(Milliseconds(10) - Milliseconds(1));
  EXPECT_TRUE(run_order.empty());

  // After the delay has completed, the task runs normally.
  FastForwardBy(Milliseconds(1));
  EXPECT_THAT(run_order, ElementsAre(1u, 2u));
  EXPECT_FALSE(queue->HasTaskToRunImmediatelyOrReadyDelayedTask());
}

TEST_P(SequenceManagerTest, DelayedTaskAtPosting_Immediate) {
  auto queue = CreateTaskQueue();

  std::vector<EnqueueOrder> run_order;
  auto handle = queue->task_runner()->PostCancelableDelayedTaskAt(
      subtle::PostDelayedTaskPassKeyForTesting(), FROM_HERE,
      BindOnce(&TestTask, 1, &run_order), TimeTicks(),
      subtle::DelayPolicy::kFlexibleNoSooner);
  EXPECT_TRUE(GetTaskQueueImpl(queue.get())->HasTaskToRunImmediately());
  EXPECT_TRUE(run_order.empty());

  // The task runs immediately.
  RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1u));
  EXPECT_FALSE(queue->HasTaskToRunImmediatelyOrReadyDelayedTask());
}

TEST(SequenceManagerTestWithMockTaskRunner,
     DelayedTaskExecutedInOneMessageLoopTask) {
  FixtureWithMockTaskRunner fixture;
  auto queue = fixture.sequence_manager()->CreateTaskQueue(
      TaskQueue::Spec(QueueName::TEST_TQ));

  queue->task_runner()->PostDelayedTask(FROM_HERE, BindOnce(&NopTask),
                                        Milliseconds(10));
  RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, fixture.test_task_runner()->GetPendingTaskCount());
  fixture.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(0u, fixture.test_task_runner()->GetPendingTaskCount());
}

TEST_P(SequenceManagerTest, DelayedTaskPosting_MultipleTasks_DecendingOrder) {
  auto queue = CreateTaskQueue();

  std::vector<EnqueueOrder> run_order;
  queue->task_runner()->PostDelayedTask(
      FROM_HERE, BindOnce(&TestTask, 1, &run_order), Milliseconds(10));

  queue->task_runner()->PostDelayedTask(
      FROM_HERE, BindOnce(&TestTask, 2, &run_order), Milliseconds(8));

  queue->task_runner()->PostDelayedTask(
      FROM_HERE, BindOnce(&TestTask, 3, &run_order), Milliseconds(5));

  EXPECT_EQ(FromStartAligned(Milliseconds(5)), NextPendingTaskTime());

  FastForwardBy(Milliseconds(5));
  EXPECT_THAT(run_order, ElementsAre(3u));
  EXPECT_EQ(FromStartAligned(Milliseconds(8)), NextPendingTaskTime());

  FastForwardBy(Milliseconds(3));
  EXPECT_THAT(run_order, ElementsAre(3u, 2u));
  EXPECT_EQ(FromStartAligned(Milliseconds(10)), NextPendingTaskTime());

  FastForwardBy(Milliseconds(2));
  EXPECT_THAT(run_order, ElementsAre(3u, 2u, 1u));
}

TEST_P(SequenceManagerTest,
       DelayedTaskAtPosting_MultipleTasks_DescendingOrder) {
  auto queue = CreateTaskQueue();

  std::vector<EnqueueOrder> run_order;
  auto handle1 = queue->task_runner()->PostCancelableDelayedTaskAt(
      subtle::PostDelayedTaskPassKeyForTesting(), FROM_HERE,
      BindOnce(&TestTask, 1, &run_order),
      sequence_manager()->NowTicks() + Milliseconds(10),
      subtle::DelayPolicy::kFlexibleNoSooner);

  queue->task_runner()->PostDelayedTask(
      FROM_HERE, BindOnce(&TestTask, 2, &run_order), Milliseconds(8));

  auto handle2 = queue->task_runner()->PostCancelableDelayedTaskAt(
      subtle::PostDelayedTaskPassKeyForTesting(), FROM_HERE,
      BindOnce(&TestTask, 3, &run_order),
      sequence_manager()->NowTicks() + Milliseconds(5),
      subtle::DelayPolicy::kFlexibleNoSooner);

  EXPECT_EQ(FromStartAligned(Milliseconds(5)), NextPendingTaskTime());

  FastForwardBy(Milliseconds(5));
  EXPECT_THAT(run_order, ElementsAre(3u));
  EXPECT_EQ(FromStartAligned(Milliseconds(8)), NextPendingTaskTime());

  FastForwardBy(Milliseconds(3));
  EXPECT_THAT(run_order, ElementsAre(3u, 2u));
  EXPECT_EQ(FromStartAligned(Milliseconds(10)), NextPendingTaskTime());

  FastForwardBy(Milliseconds(2));
  EXPECT_THAT(run_order, ElementsAre(3u, 2u, 1u));
}

TEST_P(SequenceManagerTest, DelayedTaskPosting_MultipleTasks_AscendingOrder) {
  auto queue = CreateTaskQueue();

  std::vector<EnqueueOrder> run_order;
  queue->task_runner()->PostDelayedTask(
      FROM_HERE, BindOnce(&TestTask, 1, &run_order), Milliseconds(1));

  queue->task_runner()->PostDelayedTask(
      FROM_HERE, BindOnce(&TestTask, 2, &run_order), Milliseconds(5));

  queue->task_runner()->PostDelayedTask(
      FROM_HERE, BindOnce(&TestTask, 3, &run_order), Milliseconds(10));

  EXPECT_EQ(FromStartAligned(Milliseconds(1)), NextPendingTaskTime());

  FastForwardBy(Milliseconds(1));
  EXPECT_THAT(run_order, ElementsAre(1u));
  EXPECT_EQ(FromStartAligned(Milliseconds(5)), NextPendingTaskTime());

  FastForwardBy(Milliseconds(4));
  EXPECT_THAT(run_order, ElementsAre(1u, 2u));
  EXPECT_EQ(FromStartAligned(Milliseconds(10)), NextPendingTaskTime());

  FastForwardBy(Milliseconds(5));
  EXPECT_THAT(run_order, ElementsAre(1u, 2u, 3u));
}

TEST_P(SequenceManagerTest, DelayedTaskAtPosting_MultipleTasks_AscendingOrder) {
  auto queue = CreateTaskQueue();

  std::vector<EnqueueOrder> run_order;
  auto handle1 = queue->task_runner()->PostCancelableDelayedTaskAt(
      subtle::PostDelayedTaskPassKeyForTesting(), FROM_HERE,
      BindOnce(&TestTask, 1, &run_order),
      sequence_manager()->NowTicks() + Milliseconds(1),
      subtle::DelayPolicy::kFlexibleNoSooner);

  queue->task_runner()->PostDelayedTask(
      FROM_HERE, BindOnce(&TestTask, 2, &run_order), Milliseconds(5));

  auto handle2 = queue->task_runner()->PostCancelableDelayedTaskAt(
      subtle::PostDelayedTaskPassKeyForTesting(), FROM_HERE,
      BindOnce(&TestTask, 3, &run_order),
      sequence_manager()->NowTicks() + Milliseconds(10),
      subtle::DelayPolicy::kFlexibleNoSooner);

  EXPECT_EQ(FromStartAligned(Milliseconds(1)), NextPendingTaskTime());

  FastForwardBy(Milliseconds(1));
  EXPECT_THAT(run_order, ElementsAre(1u));
  EXPECT_EQ(FromStartAligned(Milliseconds(5)), NextPendingTaskTime());

  FastForwardBy(Milliseconds(4));
  EXPECT_THAT(run_order, ElementsAre(1u, 2u));
  EXPECT_EQ(FromStartAligned(Milliseconds(10)), NextPendingTaskTime());

  FastForwardBy(Milliseconds(5));
  EXPECT_THAT(run_order, ElementsAre(1u, 2u, 3u));
}

TEST(SequenceManagerTestWithMockTaskRunner,
     PostDelayedTask_SharesUnderlyingDelayedTasks) {
  FixtureWithMockTaskRunner fixture;
  auto queue = fixture.sequence_manager()->CreateTaskQueue(
      TaskQueue::Spec(QueueName::TEST_TQ));

  std::vector<EnqueueOrder> run_order;
  TimeDelta delay(Milliseconds(10));
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
  auto queue = fixture.sequence_manager()->CreateTaskQueue(
      TaskQueue::Spec(QueueName::TEST_TQ));
  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter =
      queue->CreateQueueEnabledVoter();
  voter->SetVoteToEnable(false);

  WaitableEvent done_event;
  Thread thread("TestThread");
  thread.Start();
  thread.task_runner()->PostTask(FROM_HERE, BindLambdaForTesting([&] {
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
  auto queue = fixture.sequence_manager()->CreateTaskQueue(
      TaskQueue::Spec(QueueName::TEST_TQ));
  queue->InsertFence(TaskQueue::InsertFencePosition::kNow);

  WaitableEvent done_event;
  Thread thread("TestThread");
  thread.Start();
  thread.task_runner()->PostTask(FROM_HERE, BindLambdaForTesting([&] {
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

  TimeDelta delay(Milliseconds(10));
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
  TimeDelta kDelay = Milliseconds(10);
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
  TimeDelta delay1(Milliseconds(1));
  TimeDelta delay2(Milliseconds(10));
  TimeDelta delay3(Milliseconds(20));
  queue->task_runner()->PostDelayedTask(
      FROM_HERE, BindOnce(&TestTask, 1, &run_order), delay1);
  queue->task_runner()->PostDelayedTask(
      FROM_HERE, BindOnce(&TestTask, 2, &run_order), delay2);
  queue->task_runner()->PostDelayedTask(
      FROM_HERE, BindOnce(&TestTask, 3, &run_order), delay3);

  AdvanceMockTickClock(Milliseconds(15));
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
  TimeDelta delay(Milliseconds(10));
  queue->task_runner()->PostDelayedTask(
      FROM_HERE, BindOnce(&TestTask, 1, &run_order), delay);

  FastForwardBy(Milliseconds(10));
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
    std::vector<std::pair<TaskQueue*, TimeTicks>>* run_times,
    TaskQueue* task_queue,
    const TickClock* clock) {
  run_times->emplace_back(task_queue, clock->NowTicks());
}

}  // namespace

TEST_P(SequenceManagerTest, DelayedFence_DelayedTasks) {
  TaskQueue::Handle queue = CreateTaskQueue(
      TaskQueue::Spec(QueueName::TEST_TQ).SetDelayedFencesAllowed(true));

  std::vector<TimeTicks> run_times;
  queue->task_runner()->PostDelayedTask(
      FROM_HERE, BindOnce(&RecordTimeTask, &run_times, mock_tick_clock()),
      Milliseconds(100));
  queue->task_runner()->PostDelayedTask(
      FROM_HERE, BindOnce(&RecordTimeTask, &run_times, mock_tick_clock()),
      Milliseconds(200));
  queue->task_runner()->PostDelayedTask(
      FROM_HERE, BindOnce(&RecordTimeTask, &run_times, mock_tick_clock()),
      Milliseconds(300));

  queue->InsertFenceAt(mock_tick_clock()->NowTicks() + Milliseconds(250));
  EXPECT_FALSE(queue->HasActiveFence());

  FastForwardUntilNoTasksRemain();

  EXPECT_TRUE(queue->HasActiveFence());
  EXPECT_THAT(run_times, ElementsAre(FromStartAligned(Milliseconds(100)),
                                     FromStartAligned(Milliseconds(200))));
  run_times.clear();

  queue->RemoveFence();

  FastForwardUntilNoTasksRemain();

  EXPECT_FALSE(queue->HasActiveFence());
  EXPECT_THAT(run_times, ElementsAre(FromStartAligned(Milliseconds(300))));
}

TEST_P(SequenceManagerTest, DelayedFence_ImmediateTasks) {
  const auto kStartTime = mock_tick_clock()->NowTicks();
  TaskQueue::Handle queue = CreateTaskQueue(
      TaskQueue::Spec(QueueName::TEST_TQ).SetDelayedFencesAllowed(true));

  std::vector<TimeTicks> run_times;
  queue->InsertFenceAt(mock_tick_clock()->NowTicks() + Milliseconds(250));

  for (int i = 0; i < 5; ++i) {
    queue->task_runner()->PostTask(
        FROM_HERE, BindOnce(&RecordTimeTask, &run_times, mock_tick_clock()));
    FastForwardBy(Milliseconds(100));
    if (i < 2) {
      EXPECT_FALSE(queue->HasActiveFence());
    } else {
      EXPECT_TRUE(queue->HasActiveFence());
    }
  }

  EXPECT_THAT(run_times, ElementsAre(kStartTime, kStartTime + Milliseconds(100),
                                     kStartTime + Milliseconds(200)));
  run_times.clear();

  queue->RemoveFence();
  FastForwardUntilNoTasksRemain();

  EXPECT_THAT(run_times, ElementsAre(kStartTime + Milliseconds(500),
                                     kStartTime + Milliseconds(500)));
}

TEST_P(SequenceManagerTest, DelayedFence_RemovedFenceDoesNotActivate) {
  const auto kStartTime = mock_tick_clock()->NowTicks();
  TaskQueue::Handle queue = CreateTaskQueue(
      TaskQueue::Spec(QueueName::TEST_TQ).SetDelayedFencesAllowed(true));

  std::vector<TimeTicks> run_times;
  queue->InsertFenceAt(mock_tick_clock()->NowTicks() + Milliseconds(250));

  for (int i = 0; i < 3; ++i) {
    queue->task_runner()->PostTask(
        FROM_HERE, BindOnce(&RecordTimeTask, &run_times, mock_tick_clock()));
    EXPECT_FALSE(queue->HasActiveFence());
    FastForwardBy(Milliseconds(100));
  }

  EXPECT_TRUE(queue->HasActiveFence());
  queue->RemoveFence();

  for (int i = 0; i < 2; ++i) {
    queue->task_runner()->PostTask(
        FROM_HERE, BindOnce(&RecordTimeTask, &run_times, mock_tick_clock()));
    FastForwardBy(Milliseconds(100));
    EXPECT_FALSE(queue->HasActiveFence());
  }

  EXPECT_THAT(run_times, ElementsAre(kStartTime, kStartTime + Milliseconds(100),
                                     kStartTime + Milliseconds(200),
                                     kStartTime + Milliseconds(300),
                                     kStartTime + Milliseconds(400)));
}

TEST_P(SequenceManagerTest, DelayedFence_TakeIncomingImmediateQueue) {
  // This test checks that everything works correctly when a work queue
  // is swapped with an immediate incoming queue and a delayed fence
  // is activated, forcing a different queue to become active.
  const auto kStartTime = mock_tick_clock()->NowTicks();
  TaskQueue::Handle queue1 = CreateTaskQueue(
      TaskQueue::Spec(QueueName::TEST_TQ).SetDelayedFencesAllowed(true));
  TaskQueue::Handle queue2 = CreateTaskQueue(
      TaskQueue::Spec(QueueName::TEST2_TQ).SetDelayedFencesAllowed(true));

  std::vector<std::pair<TaskQueue*, TimeTicks>> run_times;

  // Fence ensures that the task posted after advancing time is blocked.
  queue1->InsertFenceAt(mock_tick_clock()->NowTicks() + Milliseconds(250));

  // This task should not be blocked and should run immediately after
  // advancing time at 301ms.
  queue1->task_runner()->PostTask(
      FROM_HERE, BindOnce(&RecordTimeAndQueueTask, &run_times,
                          Unretained(queue1.get()), mock_tick_clock()));
  // Force reload of immediate work queue. In real life the same effect can be
  // achieved with cross-thread posting.
  sequence_manager()->ReloadEmptyWorkQueues();

  AdvanceMockTickClock(Milliseconds(300));

  // This task should be blocked.
  queue1->task_runner()->PostTask(
      FROM_HERE, BindOnce(&RecordTimeAndQueueTask, &run_times,
                          Unretained(queue1.get()), mock_tick_clock()));
  // This task on a different runner should run as expected.
  queue2->task_runner()->PostTask(
      FROM_HERE, BindOnce(&RecordTimeAndQueueTask, &run_times,
                          Unretained(queue2.get()), mock_tick_clock()));

  FastForwardUntilNoTasksRemain();

  EXPECT_THAT(
      run_times,
      ElementsAre(
          std::make_pair(queue1.get(), kStartTime + Milliseconds(300)),
          std::make_pair(queue2.get(), kStartTime + Milliseconds(300))));
}

namespace {

void ReentrantTestTask(TaskQueue* runner,
                       int countdown,
                       std::vector<EnqueueOrder>* out_result) {
  out_result->push_back(EnqueueOrder::FromIntForTesting(countdown));
  if (--countdown) {
    runner->task_runner()->PostTask(
        FROM_HERE, BindOnce(&ReentrantTestTask, Unretained(runner), countdown,
                            out_result));
  }
}

}  // namespace

TEST_P(SequenceManagerTest, ReentrantPosting) {
  auto queue = CreateTaskQueue();

  std::vector<EnqueueOrder> run_order;
  queue->task_runner()->PostTask(
      FROM_HERE,
      BindOnce(&ReentrantTestTask, Unretained(queue.get()), 3, &run_order));

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

  if (GetUnderlyingRunnerType() != RunnerType::kMessagePump) {
    RunLoop().RunUntilIdle();
  }

  EXPECT_FALSE(counter.HasReferences());
}

void PostTaskToRunner(TaskQueue* runner, std::vector<EnqueueOrder>* run_order) {
  runner->task_runner()->PostTask(FROM_HERE, BindOnce(&TestTask, 1, run_order));
}

TEST_P(SequenceManagerTest, PostFromThread) {
  auto queue = CreateTaskQueue();

  std::vector<EnqueueOrder> run_order;
  Thread thread("TestThread");
  thread.Start();
  thread.task_runner()->PostTask(
      FROM_HERE,
      BindOnce(&PostTaskToRunner, Unretained(queue.get()), &run_order));
  thread.Stop();

  RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1u));
}

void RePostingTestTask(TaskQueue* runner, int* run_count) {
  (*run_count)++;
  runner->task_runner()->PostTask(
      FROM_HERE, BindOnce(&RePostingTestTask, Unretained(runner), run_count));
}

TEST_P(SequenceManagerTest, DoWorkCantPostItselfMultipleTimes) {
  auto queue = CreateTaskQueue();

  int run_count = 0;
  queue->task_runner()->PostTask(
      FROM_HERE,
      BindOnce(&RePostingTestTask, Unretained(queue.get()), &run_count));

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
      FROM_HERE, BindOnce(&PostFromNestedRunloop, queue->task_runner(),
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

void RemoveQueueObserverTask(TaskQueue* queue, TaskObserver* observer) {
  queue->RemoveTaskObserver(observer);
}

TEST_P(SequenceManagerTest, QueueTaskObserverRemovingInsideTask) {
  auto queue = CreateTaskQueue();
  MockTaskObserver observer;
  queue->AddTaskObserver(&observer);

  queue->task_runner()->PostTask(
      FROM_HERE,
      BindOnce(&RemoveQueueObserverTask, Unretained(queue.get()), &observer));

  EXPECT_CALL(observer,
              WillProcessTask(_, /*was_blocked_or_low_priority=*/false))
      .Times(1);
  EXPECT_CALL(observer, DidProcessTask(_)).Times(0);
  RunLoop().RunUntilIdle();
}

TEST_P(SequenceManagerTest, CancelHandleInsideTaskObserver) {
  class CancelingTaskObserver : public TaskObserver {
   public:
    DelayedTaskHandle handle;
    bool will_run_task_called = false;
    bool did_process_task_called = false;
    explicit CancelingTaskObserver(DelayedTaskHandle handle_in)
        : handle(std::move(handle_in)) {
      EXPECT_TRUE(handle.IsValid());
    }

    ~CancelingTaskObserver() override {
      EXPECT_FALSE(handle.IsValid());
      EXPECT_TRUE(will_run_task_called);
      EXPECT_TRUE(did_process_task_called);
    }

    void DidProcessTask(const PendingTask& task) override {
      did_process_task_called = true;
    }
    void WillProcessTask(const PendingTask& task,
                         bool was_blocked_or_low_priority) override {
      handle.CancelTask();
      will_run_task_called = true;
    }
  };

  auto queue = CreateTaskQueue();

  auto handle = queue->task_runner()->PostCancelableDelayedTask(
      subtle::PostDelayedTaskPassKeyForTesting(), FROM_HERE,
      BindLambdaForTesting([] { FAIL(); }), base::TimeDelta());

  CancelingTaskObserver observer(std::move(handle));
  queue->AddTaskObserver(&observer);

  RunLoop().RunUntilIdle();
}

TEST_P(SequenceManagerTest, ThreadCheckAfterTermination) {
  auto queue = CreateTaskQueue();
  EXPECT_TRUE(queue->task_runner()->RunsTasksInCurrentSequence());
  DestroySequenceManager();
  EXPECT_TRUE(queue->task_runner()->RunsTasksInCurrentSequence());
}

TEST_P(SequenceManagerTest, GetNextDelayedWakeUp) {
  auto queues = CreateTaskQueues(2u);
  AdvanceMockTickClock(Microseconds(10000));
  LazyNow lazy_now_1(mock_tick_clock());

  // With no delayed tasks.
  EXPECT_FALSE(sequence_manager()->GetNextDelayedWakeUp());

  // With a non-delayed task.
  queues[0]->task_runner()->PostTask(FROM_HERE, BindOnce(&NopTask));
  EXPECT_FALSE(sequence_manager()->GetNextDelayedWakeUp());

  // With a delayed task.
  TimeDelta expected_delay = Milliseconds(50);
  queues[0]->task_runner()->PostDelayedTask(FROM_HERE, BindOnce(&NopTask),
                                            expected_delay);
  EXPECT_EQ(lazy_now_1.Now() + expected_delay,
            sequence_manager()->GetNextDelayedWakeUp()->time);

  // With another delayed task in the same queue with a longer delay.
  queues[0]->task_runner()->PostDelayedTask(FROM_HERE, BindOnce(&NopTask),
                                            Milliseconds(100));
  EXPECT_EQ(lazy_now_1.Now() + expected_delay,
            sequence_manager()->GetNextDelayedWakeUp()->time);

  // With another delayed task in the same queue with a shorter delay.
  expected_delay = Milliseconds(20);
  queues[0]->task_runner()->PostDelayedTask(FROM_HERE, BindOnce(&NopTask),
                                            expected_delay);
  EXPECT_EQ(lazy_now_1.Now() + expected_delay,
            sequence_manager()->GetNextDelayedWakeUp()->time);

  // With another delayed task in a different queue with a shorter delay.
  expected_delay = Milliseconds(10);
  queues[1]->task_runner()->PostDelayedTask(FROM_HERE, BindOnce(&NopTask),
                                            expected_delay);
  EXPECT_EQ(lazy_now_1.Now() + expected_delay,
            sequence_manager()->GetNextDelayedWakeUp()->time);
}

TEST_P(SequenceManagerTest, GetNextDelayedWakeUp_MultipleQueues) {
  auto queues = CreateTaskQueues(3u);

  TimeDelta delay1 = Milliseconds(50);
  TimeDelta delay2 = Milliseconds(5);
  TimeDelta delay3 = Milliseconds(10);
  queues[0]->task_runner()->PostDelayedTask(FROM_HERE, BindOnce(&NopTask),
                                            delay1);
  queues[1]->task_runner()->PostDelayedTask(FROM_HERE, BindOnce(&NopTask),
                                            delay2);
  queues[2]->task_runner()->PostDelayedTask(FROM_HERE, BindOnce(&NopTask),
                                            delay3);
  queues[0]->task_runner()->PostTask(FROM_HERE, BindOnce(&NopTask));

  LazyNow lazy_now(mock_tick_clock());
  EXPECT_EQ(lazy_now.Now() + delay2,
            sequence_manager()->GetNextDelayedWakeUp()->time);
}

TEST(SequenceManagerWithTaskRunnerTest, DeleteSequenceManagerInsideATask) {
  FixtureWithMockTaskRunner fixture;
  auto queue = fixture.sequence_manager()->CreateTaskQueue(
      TaskQueue::Spec(QueueName::TEST_TQ));

  queue->task_runner()->PostTask(FROM_HERE, BindLambdaForTesting([&] {
                                   fixture.DestroySequenceManager();
                                 }));

  // This should not crash, assuming DoWork detects the SequenceManager has
  // been deleted.
  RunLoop().RunUntilIdle();
}

TEST_P(SequenceManagerTest, GetAndClearSystemIsQuiescentBit) {
  auto queues = CreateTaskQueues(3u);

  TaskQueue::Handle queue0 = CreateTaskQueue(
      TaskQueue::Spec(QueueName::TEST_TQ).SetShouldMonitorQuiescence(true));
  TaskQueue::Handle queue1 = CreateTaskQueue(
      TaskQueue::Spec(QueueName::TEST2_TQ).SetShouldMonitorQuiescence(true));
  TaskQueue::Handle queue2 = CreateTaskQueue();

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
                                        Milliseconds(12));
  EXPECT_FALSE(queue->HasTaskToRunImmediatelyOrReadyDelayedTask());

  // Move time forwards until just before the delayed task should run.
  AdvanceMockTickClock(Milliseconds(10));
  LazyNow lazy_now_1(mock_tick_clock());
  sequence_manager()->MoveReadyDelayedTasksToWorkQueues(&lazy_now_1);
  EXPECT_FALSE(queue->HasTaskToRunImmediatelyOrReadyDelayedTask());

  // Force the delayed task onto the work queue.
  AdvanceMockTickClock(Milliseconds(2));
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
  constexpr auto kDelay = Milliseconds(10);

  // By posting the immediate tasks from a delayed one we make sure that the
  // delayed tasks we post afterwards have a lower enqueue_order than the
  // immediate ones. Thus all the delayed ones would run before the immediate
  // ones if it weren't for the anti-starvation feature we are testing here.
  queue->task_runner()->PostDelayedTask(
      FROM_HERE, BindLambdaForTesting([&] {
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

  FastForwardBy(Milliseconds(10));

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
  TimeDelta delay = Milliseconds(10);
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
  TimeDelta delay = Milliseconds(10);
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
  TimeDelta delay1 = Milliseconds(10);
  TimeDelta delay2 = Milliseconds(5);
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
                                  TaskQueue* runner,
                                  bool* was_nested) {
  runner->task_runner()->PostTask(FROM_HERE, run_loop->QuitClosure());
  runner->task_runner()->PostTask(FROM_HERE,
                                  BindOnce(&CheckIsNested, was_nested));
  run_loop->Run();
}

}  // namespace

TEST_P(SequenceManagerTest, QuitWhileNested) {
  if (GetUnderlyingRunnerType() == RunnerType::kMockTaskRunner)
    return;
  // This test makes sure we don't continue running a work batch after a nested
  // run loop has been exited in the middle of the batch.
  auto queue = CreateTaskQueue();
  sequence_manager()->SetWorkBatchSize(2);

  bool was_nested = true;
  RunLoop run_loop(RunLoop::Type::kNestableTasksAllowed);
  queue->task_runner()->PostTask(
      FROM_HERE, BindOnce(&PostAndQuitFromNestedRunloop, Unretained(&run_loop),
                          queue.get(), Unretained(&was_nested)));

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
  queue->task_runner()->PostDelayedTask(
      FROM_HERE, BindOnce(&TestTask, 1, &run_order), Milliseconds(30));
  queue->task_runner()->PostDelayedTask(
      FROM_HERE, BindOnce(&TestTask, 2, &run_order), Milliseconds(20));
  queue->task_runner()->PostDelayedTask(
      FROM_HERE, BindOnce(&TestTask, 3, &run_order), Milliseconds(10));
  queue->task_runner()->PostTask(FROM_HERE, BindOnce(&TestTask, 4, &run_order));

  FastForwardBy(Milliseconds(40));
  ASSERT_THAT(run_order, ElementsAre(4u, 3u, 2u, 1u));

  // The sequence numbers are a one-based monotonically incrememting counter
  // which should be set when the task is posted rather than when it's enqueued
  // onto the Incoming queue. This counter starts with 2.
  EXPECT_THAT(observer.sequence_numbers(), ElementsAre(5, 4, 3, 2));

  sequence_manager()->RemoveTaskObserver(&observer);
}

TEST_P(SequenceManagerTest, NewTaskQueues) {
  auto queue = CreateTaskQueue();

  TaskQueue::Handle queue1 = CreateTaskQueue();
  TaskQueue::Handle queue2 = CreateTaskQueue();
  TaskQueue::Handle queue3 = CreateTaskQueue();

  ASSERT_NE(queue1.get(), queue2.get());
  ASSERT_NE(queue1.get(), queue3.get());
  ASSERT_NE(queue2.get(), queue3.get());

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
  TaskQueue::Handle queue = CreateTaskQueue();

  scoped_refptr<SingleThreadTaskRunner> runner1 = queue->task_runner();
  scoped_refptr<SingleThreadTaskRunner> runner2 = queue->CreateTaskRunner(1);

  std::vector<EnqueueOrder> run_order;
  EXPECT_TRUE(runner1->PostTask(FROM_HERE, BindOnce(&TestTask, 1, &run_order)));
  EXPECT_TRUE(runner2->PostTask(FROM_HERE, BindOnce(&TestTask, 2, &run_order)));
  queue.reset();
  EXPECT_FALSE(
      runner1->PostTask(FROM_HERE, BindOnce(&TestTask, 3, &run_order)));
  EXPECT_FALSE(
      runner2->PostTask(FROM_HERE, BindOnce(&TestTask, 4, &run_order)));

  RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre());
}

TEST_P(SequenceManagerTest, ShutdownTaskQueue) {
  auto queue = CreateTaskQueue();

  TaskQueue::Handle queue1 = CreateTaskQueue();
  TaskQueue::Handle queue2 = CreateTaskQueue();
  TaskQueue::Handle queue3 = CreateTaskQueue();

  ASSERT_NE(queue1.get(), queue2.get());
  ASSERT_NE(queue1.get(), queue3.get());
  ASSERT_NE(queue2.get(), queue3.get());

  std::vector<EnqueueOrder> run_order;
  queue1->task_runner()->PostTask(FROM_HERE,
                                  BindOnce(&TestTask, 1, &run_order));
  queue2->task_runner()->PostTask(FROM_HERE,
                                  BindOnce(&TestTask, 2, &run_order));
  queue3->task_runner()->PostTask(FROM_HERE,
                                  BindOnce(&TestTask, 3, &run_order));
  queue2.reset();
  RunLoop().RunUntilIdle();

  EXPECT_THAT(run_order, ElementsAre(1u, 3u));
}

TEST_P(SequenceManagerTest, ShutdownTaskQueue_WithDelayedTasks) {
  auto queues = CreateTaskQueues(2u);

  // Register three delayed tasks
  std::vector<EnqueueOrder> run_order;
  queues[0]->task_runner()->PostDelayedTask(
      FROM_HERE, BindOnce(&TestTask, 1, &run_order), Milliseconds(10));
  queues[1]->task_runner()->PostDelayedTask(
      FROM_HERE, BindOnce(&TestTask, 2, &run_order), Milliseconds(20));
  queues[0]->task_runner()->PostDelayedTask(
      FROM_HERE, BindOnce(&TestTask, 3, &run_order), Milliseconds(30));

  queues[1].reset();
  RunLoop().RunUntilIdle();

  FastForwardBy(Milliseconds(40));
  ASSERT_THAT(run_order, ElementsAre(1u, 3u));
}

namespace {
void ShutdownQueue(TaskQueue::Handle queue) {}
}  // namespace

TEST_P(SequenceManagerTest, ShutdownTaskQueue_InTasks) {
  auto queues = CreateTaskQueues(3u);
  auto runner1 = queues[1]->task_runner();
  auto runner2 = queues[2]->task_runner();

  std::vector<EnqueueOrder> run_order;
  queues[0]->task_runner()->PostTask(FROM_HERE,
                                     BindOnce(&TestTask, 1, &run_order));
  queues[0]->task_runner()->PostTask(
      FROM_HERE, BindOnce(&ShutdownQueue, std::move(queues[1])));
  queues[0]->task_runner()->PostTask(
      FROM_HERE, BindOnce(&ShutdownQueue, std::move(queues[2])));
  runner1->PostTask(FROM_HERE, BindOnce(&TestTask, 2, &run_order));
  runner2->PostTask(FROM_HERE, BindOnce(&TestTask, 3, &run_order));

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
  TaskQueue::Handle queue_to_delete = CreateTaskQueue();

  std::vector<bool> log;
  std::vector<std::pair<OnceClosure, bool>> tasks_to_post_from_nested_loop;

  // Inside a nested run loop, delete `queue_to_delete`, bookended by Nop tasks.
  tasks_to_post_from_nested_loop.push_back(
      std::make_pair(BindOnce(&NopTask), true));
  tasks_to_post_from_nested_loop.push_back(std::make_pair(
      BindLambdaForTesting([&] { queue_to_delete.reset(); }), true));
  tasks_to_post_from_nested_loop.push_back(
      std::make_pair(BindOnce(&NopTask), true));
  queue->task_runner()->PostTask(
      FROM_HERE, BindOnce(&PostFromNestedRunloop, queue->task_runner(),
                          Unretained(&tasks_to_post_from_nested_loop)));
  RunLoop().RunUntilIdle();

  // Just make sure that we don't crash.
}

TEST_P(SequenceManagerTest, TimeDomainMigrationWithIncomingImmediateTasks) {
  auto queue = CreateTaskQueue();

  TimeTicks start_time_ticks = sequence_manager()->NowTicks();
  std::unique_ptr<MockTimeDomain> domain_a =
      std::make_unique<MockTimeDomain>(start_time_ticks);
  std::unique_ptr<MockTimeDomain> domain_b =
      std::make_unique<MockTimeDomain>(start_time_ticks);

  sequence_manager()->SetTimeDomain(domain_a.get());
  std::vector<EnqueueOrder> run_order;
  queue->task_runner()->PostTask(FROM_HERE, BindOnce(&TestTask, 1, &run_order));
  sequence_manager()->ResetTimeDomain();
  sequence_manager()->SetTimeDomain(domain_b.get());

  RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1u));

  sequence_manager()->ResetTimeDomain();
}

// Test that no wake up is scheduled for a delayed task in the future when a
// time domain is present.
TEST_P(SequenceManagerTest, TimeDomainDoesNotCauseWakeUp) {
  auto queue = CreateTaskQueue();

  std::unique_ptr<MockTimeDomain> domain =
      std::make_unique<MockTimeDomain>(sequence_manager()->NowTicks());
  sequence_manager()->SetTimeDomain(domain.get());

  std::vector<EnqueueOrder> run_order;
  queue->task_runner()->PostDelayedTask(
      FROM_HERE, BindOnce(&TestTask, 1, &run_order), Milliseconds(10));
  LazyNow lazy_now1(domain.get());
  EXPECT_EQ(std::nullopt, sequence_manager()->GetPendingWakeUp(&lazy_now1));
  EXPECT_EQ(TimeTicks::Max(), NextPendingTaskTime());

  domain->SetNowTicks(sequence_manager()->NowTicks() + Milliseconds(10));
  LazyNow lazy_now2(domain.get());
  EXPECT_EQ(WakeUp{}, sequence_manager()->GetPendingWakeUp(&lazy_now2));

  sequence_manager()->ResetTimeDomain();
}

TEST_P(SequenceManagerTest,
       PostDelayedTasksReverseOrderAlternatingTimeDomains) {
  auto queue = CreateTaskQueue();

  std::vector<EnqueueOrder> run_order;

  std::unique_ptr<sequence_manager::MockTimeDomain> domain =
      std::make_unique<sequence_manager::MockTimeDomain>(
          mock_tick_clock()->NowTicks());

  sequence_manager()->SetTimeDomain(domain.get());
  queue->task_runner()->PostDelayedTask(
      FROM_HERE, BindOnce(&TestTask, 1, &run_order), Milliseconds(400));

  sequence_manager()->ResetTimeDomain();
  queue->task_runner()->PostDelayedTask(
      FROM_HERE, BindOnce(&TestTask, 2, &run_order), Milliseconds(300));

  sequence_manager()->SetTimeDomain(domain.get());
  queue->task_runner()->PostDelayedTask(
      FROM_HERE, BindOnce(&TestTask, 3, &run_order), Milliseconds(200));

  sequence_manager()->ResetTimeDomain();
  queue->task_runner()->PostDelayedTask(
      FROM_HERE, BindOnce(&TestTask, 4, &run_order), Milliseconds(100));

  FastForwardBy(Milliseconds(400));
  EXPECT_THAT(run_order, ElementsAre(4u, 3u, 2u, 1u));

  sequence_manager()->ResetTimeDomain();
}

namespace {

class MockTaskQueueThrottler : public TaskQueue::Throttler {
 public:
  MockTaskQueueThrottler() = default;
  ~MockTaskQueueThrottler() = default;

  MOCK_METHOD1(OnWakeUp, void(LazyNow*));
  MOCK_METHOD0(OnHasImmediateTask, void());
  MOCK_METHOD1(GetNextAllowedWakeUp_DesiredWakeUpTime, void(TimeTicks));

  std::optional<WakeUp> GetNextAllowedWakeUp(
      LazyNow* lazy_now,
      std::optional<WakeUp> next_desired_wake_up,
      bool has_immediate_work) override {
    if (next_desired_wake_up)
      GetNextAllowedWakeUp_DesiredWakeUpTime(next_desired_wake_up->time);
    if (next_allowed_wake_up_)
      return next_allowed_wake_up_;
    return next_desired_wake_up;
  }

  void SetNextAllowedWakeUp(std::optional<WakeUp> next_allowed_wake_up) {
    next_allowed_wake_up_ = next_allowed_wake_up;
  }

 private:
  std::optional<WakeUp> next_allowed_wake_up_;
};

}  // namespace

TEST_P(SequenceManagerTest, TaskQueueThrottler_ImmediateTask) {
  StrictMock<MockTaskQueueThrottler> throttler;
  auto queue = CreateTaskQueue();
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
  LazyNow lazy_now(mock_tick_clock());
  sequence_manager()->SelectNextTask(lazy_now);
  sequence_manager()->DidRunTask(lazy_now);
  sequence_manager()->SelectNextTask(lazy_now);
  sequence_manager()->DidRunTask(lazy_now);
  EXPECT_CALL(throttler, OnHasImmediateTask());
  queue->task_runner()->PostTask(FROM_HERE, BindOnce(&NopTask));
  sequence_manager()->ReloadEmptyWorkQueues();
  Mock::VerifyAndClearExpectations(&throttler);
}

TEST_P(SequenceManagerTest, TaskQueueThrottler_DelayedTask) {
  StrictMock<MockTaskQueueThrottler> throttler;
  auto queue = CreateTaskQueue();
  queue->SetThrottler(&throttler);

  TimeTicks start_time = sequence_manager()->NowTicks();
  TimeDelta delay10s(Seconds(10));
  TimeDelta delay100s(Seconds(100));
  TimeDelta delay1s(Seconds(1));

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
}

TEST_P(SequenceManagerTest, TaskQueueThrottler_OnWakeUp) {
  StrictMock<MockTaskQueueThrottler> throttler;
  auto queue = CreateTaskQueue();
  queue->SetThrottler(&throttler);

  TimeTicks start_time = sequence_manager()->NowTicks();
  TimeDelta delay(Seconds(1));

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
}

TEST_P(SequenceManagerTest, TaskQueueThrottler_ResetThrottler) {
  StrictMock<MockTaskQueueThrottler> throttler;
  auto queue = CreateTaskQueue();
  queue->SetThrottler(&throttler);

  TimeTicks start_time = sequence_manager()->NowTicks();
  TimeDelta delay10s(Seconds(10));
  TimeDelta delay1s(Seconds(1));

  EXPECT_FALSE(queue->GetNextDesiredWakeUp());

  // GetNextAllowedWakeUp should be called when a delayed task is posted on an
  // empty queue.
  throttler.SetNextAllowedWakeUp(
      base::sequence_manager::WakeUp{start_time + delay10s});
  EXPECT_CALL(throttler,
              GetNextAllowedWakeUp_DesiredWakeUpTime(start_time + delay1s));
  queue->task_runner()->PostDelayedTask(FROM_HERE, BindOnce(&NopTask), delay1s);
  Mock::VerifyAndClearExpectations(&throttler);
  // Expect throttled wake up.
  LazyNow lazy_now(mock_tick_clock());
  WakeUp expected_wake_up{start_time + delay10s};
  EXPECT_EQ(expected_wake_up, sequence_manager()->GetPendingWakeUp(&lazy_now));

  queue->ResetThrottler();
  // Next wake up should be back to normal.
  EXPECT_EQ((WakeUp{start_time + delay1s, kLeeway}),
            sequence_manager()->GetPendingWakeUp(&lazy_now));
}

TEST_P(SequenceManagerTest, TaskQueueThrottler_DelayedTaskMultipleQueues) {
  StrictMock<MockTaskQueueThrottler> throttler0;
  StrictMock<MockTaskQueueThrottler> throttler1;
  auto queues = CreateTaskQueues(2u);
  queues[0]->SetThrottler(&throttler0);
  queues[1]->SetThrottler(&throttler1);

  TimeTicks start_time = sequence_manager()->NowTicks();
  TimeDelta delay1s(Seconds(1));
  TimeDelta delay10s(Seconds(10));

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
}

TEST_P(SequenceManagerTest, TaskQueueThrottler_DelayedWorkWhichCanRunNow) {
  // This test checks that when delayed work becomes available the notification
  // still fires. This usually happens when time advances and task becomes
  // available in the middle of the scheduling code. For this test we force
  // notification dispatching by calling UpdateWakeUp() explicitly.

  StrictMock<MockTaskQueueThrottler> throttler;
  auto queue = CreateTaskQueue();
  queue->SetThrottler(&throttler);

  TimeDelta delay1s(Seconds(1));

  // GetNextAllowedWakeUp should be called when a delayed task is posted on an
  // empty queue.
  EXPECT_CALL(throttler, GetNextAllowedWakeUp_DesiredWakeUpTime(_));
  queue->task_runner()->PostDelayedTask(FROM_HERE, BindOnce(&NopTask), delay1s);
  Mock::VerifyAndClearExpectations(&throttler);

  AdvanceMockTickClock(Seconds(10));

  EXPECT_CALL(throttler, GetNextAllowedWakeUp_DesiredWakeUpTime(_));
  LazyNow lazy_now(mock_tick_clock());
  queue->UpdateWakeUp(&lazy_now);
  Mock::VerifyAndClearExpectations(&throttler);
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

  raw_ptr<const TickClock> clock_;
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
  StrictMock<MockTaskQueueThrottler> throttler;
  auto queue = CreateTaskQueue();
  queue->SetThrottler(&throttler);

  TimeTicks start_time = sequence_manager()->NowTicks();
  TimeDelta delay1(Seconds(5));
  TimeDelta delay2(Seconds(10));

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
      base::Seconds(1));

  // Make sure sweeping away the last task in the queue doesn't end up accessing
  // invalid iterators.
  task.weak_factory_.InvalidateWeakPtrs();
  sequence_manager()->ReclaimMemory();
}

TEST_P(SequenceManagerTest, CancelledTaskPostAnother_ReclaimMemory) {
  // This check ensures that a task whose destruction causes another task to be
  // posted as a side-effect doesn't cause us to access invalid iterators while
  // sweeping away cancelled tasks.
  auto queue = CreateTaskQueue();
  bool did_destroy = false;
  auto on_destroy = BindLambdaForTesting([&] {
    queue->task_runner()->PostDelayedTask(
        FROM_HERE, BindLambdaForTesting([] {}), base::Seconds(1));
    did_destroy = true;
  });

  DestructionCallback destruction_observer(std::move(on_destroy));
  CancelableTask task(mock_tick_clock());
  queue->task_runner()->PostDelayedTask(
      FROM_HERE,
      BindOnce(&CancelableTask::FailTask<DestructionCallback>,
               task.weak_factory_.GetWeakPtr(),
               std::move(destruction_observer)),
      base::Seconds(1));

  task.weak_factory_.InvalidateWeakPtrs();
  EXPECT_FALSE(did_destroy);
  sequence_manager()->ReclaimMemory();
  EXPECT_TRUE(did_destroy);
}

// Regression test to ensure that posting a new task from the destructor of a
// canceled task doesn't crash.
TEST_P(SequenceManagerTest,
       CancelledTaskPostAnother_MoveReadyDelayedTasksToWorkQueues) {
  // This check ensures that a task whose destruction causes another task to be
  // posted as a side-effect doesn't cause us to access invalid iterators while
  // sweeping away cancelled tasks.
  auto queue = CreateTaskQueue();
  bool did_destroy = false;
  auto on_destroy = BindLambdaForTesting([&] {
    queue->task_runner()->PostDelayedTask(
        FROM_HERE, BindLambdaForTesting([] {}), base::Seconds(1));
    did_destroy = true;
  });

  DestructionCallback destruction_observer(std::move(on_destroy));
  CancelableTask task(mock_tick_clock());
  queue->task_runner()->PostDelayedTask(
      FROM_HERE,
      BindOnce(&CancelableTask::FailTask<DestructionCallback>,
               task.weak_factory_.GetWeakPtr(),
               std::move(destruction_observer)),
      base::Seconds(1));

  AdvanceMockTickClock(base::Seconds(1));

  task.weak_factory_.InvalidateWeakPtrs();
  EXPECT_FALSE(did_destroy);
  LazyNow lazy_now(mock_tick_clock());
  sequence_manager()->MoveReadyDelayedTasksToWorkQueues(&lazy_now);
  EXPECT_TRUE(did_destroy);
}

TEST_P(SequenceManagerTest,
       CancelledTaskPostAnother_RemoveAllCanceledDelayedTasksFromFront) {
  // This check ensures that a task whose destruction causes another task to be
  // posted as a side-effect doesn't cause us to access invalid iterators while
  // removing canceled tasks from the front of the queues.
  auto queue = CreateTaskQueue();
  bool did_destroy = false;
  auto on_destroy = BindLambdaForTesting([&] {
    queue->task_runner()->PostDelayedTask(
        FROM_HERE, BindLambdaForTesting([] {}), base::Seconds(1));
    did_destroy = true;
  });

  DestructionCallback destruction_observer(std::move(on_destroy));
  CancelableTask task(mock_tick_clock());
  queue->task_runner()->PostDelayedTask(
      FROM_HERE,
      BindOnce(&CancelableTask::FailTask<DestructionCallback>,
               task.weak_factory_.GetWeakPtr(),
               std::move(destruction_observer)),
      base::Seconds(1));

  task.weak_factory_.InvalidateWeakPtrs();
  EXPECT_FALSE(did_destroy);
  LazyNow lazy_now(mock_tick_clock());
  // This removes canceled delayed tasks from the front of the queue.
  sequence_manager()->GetPendingWakeUp(&lazy_now);
  EXPECT_TRUE(did_destroy);
}

TEST_P(SequenceManagerTest, CancelledImmediateTaskShutsDownQueue) {
  // This check ensures that an immediate task whose destruction causes the
  // owning task queue to be shut down doesn't cause us to access freed memory.
  auto queue = CreateTaskQueue();
  bool did_shutdown = false;
  auto on_destroy = BindLambdaForTesting([&] {
    queue.reset();
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
    queue.reset();
    did_shutdown = true;
  });

  DestructionCallback destruction_observer(std::move(on_destroy));
  CancelableTask task(mock_tick_clock());
  queue->task_runner()->PostDelayedTask(
      FROM_HERE,
      BindOnce(&CancelableTask::FailTask<DestructionCallback>,
               task.weak_factory_.GetWeakPtr(),
               std::move(destruction_observer)),
      base::Seconds(1));

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
  auto queue = fixture.sequence_manager()->CreateTaskQueue(
      TaskQueue::Spec(QueueName::TEST_TQ));

  // NOTE because tasks posted to the chromiumrun loop are not cancellable, we
  // will end up with a lot more tasks posted if the delayed tasks were posted
  // in the reverse order.
  // TODO(alexclarke): Consider talking to the message pump directly.
  for (int i = 1; i < 100; i++) {
    queue->task_runner()->PostDelayedTask(
        FROM_HERE,
        BindOnce(&ChromiumRunloopInspectionTask, fixture.test_task_runner()),
        Milliseconds(i));
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
    fixture_->AdvanceMockTickClock(Milliseconds(5));
  }

  int Count() const { return count_; }

 private:
  int count_;
  scoped_refptr<TaskRunner> task_runner_;
  TimeDelta delay_;
  raw_ptr<Fixture> fixture_;
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
    fixture_->AdvanceMockTickClock(Milliseconds(5));
  }

  int Count() const { return count_; }

 private:
  int count_;
  scoped_refptr<TaskRunner> task_runner_;
  TimeDelta delay_;
  raw_ptr<Fixture> fixture_;
  RepeatingCallback<bool()> should_exit_;
};

bool ShouldExit(QuadraticTask* quadratic_task, LinearTask* linear_task) {
  return quadratic_task->Count() == 1000 || linear_task->Count() == 1000;
}

}  // namespace

TEST_P(SequenceManagerTest,
       DelayedTasksDontBadlyStarveNonDelayedWork_SameQueue) {
  auto queue = CreateTaskQueue();

  QuadraticTask quadratic_delayed_task(queue->task_runner(), Milliseconds(10),
                                       this);
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
  LinearTask linear_delayed_task(queue->task_runner(), Milliseconds(10), this);
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
                                       Milliseconds(10), this);
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
  LinearTask linear_delayed_task(queues[1]->task_runner(), Milliseconds(10),
                                 this);
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

  TaskQueue* queue0 = queues[0].get();
  TaskQueue* queue1 = queues[1].get();

  std::vector<internal::TaskQueueImpl*> task_sources;
  queue0->task_runner()->PostTask(FROM_HERE,
                                  BindOnce(&CurrentlyExecutingTaskQueueTestTask,
                                           sequence_manager(), &task_sources));
  queue1->task_runner()->PostTask(FROM_HERE,
                                  BindOnce(&CurrentlyExecutingTaskQueueTestTask,
                                           sequence_manager(), &task_sources));
  RunLoop().RunUntilIdle();

  EXPECT_THAT(task_sources,
              ElementsAre(GetTaskQueueImpl(queue0), GetTaskQueueImpl(queue1)));
  EXPECT_EQ(nullptr, sequence_manager()->currently_executing_task_queue());
}

namespace {
void RunloopCurrentlyExecutingTaskQueueTestTask(
    SequenceManagerImpl* sequence_manager,
    std::vector<internal::TaskQueueImpl*>* task_sources,
    std::vector<std::pair<OnceClosure, TaskQueue*>>* tasks) {
  task_sources->push_back(sequence_manager->currently_executing_task_queue());

  for (std::pair<OnceClosure, TaskQueue*>& pair : *tasks) {
    pair.second->task_runner()->PostTask(FROM_HERE, std::move(pair.first));
  }

  RunLoop(RunLoop::Type::kNestableTasksAllowed).RunUntilIdle();
  task_sources->push_back(sequence_manager->currently_executing_task_queue());
}
}  // namespace

TEST_P(SequenceManagerTest, CurrentlyExecutingTaskQueue_NestedLoop) {
  auto queues = CreateTaskQueues(3u);

  TaskQueue* queue0 = queues[0].get();
  TaskQueue* queue1 = queues[1].get();
  TaskQueue* queue2 = queues[2].get();

  std::vector<internal::TaskQueueImpl*> task_sources;
  std::vector<std::pair<OnceClosure, TaskQueue*>>
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
  EXPECT_THAT(
      task_sources,
      UnorderedElementsAre(GetTaskQueueImpl(queue0), GetTaskQueueImpl(queue1),
                           GetTaskQueueImpl(queue2), GetTaskQueueImpl(queue0)));
  EXPECT_EQ(nullptr, sequence_manager()->currently_executing_task_queue());
}

TEST_P(SequenceManagerTest, NoWakeUpsForCanceledDelayedTasks) {
  auto queue = CreateTaskQueue();

  TimeTicks start_time = sequence_manager()->NowTicks();

  CancelableTask task1(mock_tick_clock());
  CancelableTask task2(mock_tick_clock());
  CancelableTask task3(mock_tick_clock());
  CancelableTask task4(mock_tick_clock());
  TimeDelta delay1(Seconds(5));
  TimeDelta delay2(Seconds(10));
  TimeDelta delay3(Seconds(15));
  TimeDelta delay4(Seconds(30));
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
  TimeDelta delay1(Seconds(5));
  TimeDelta delay2(Seconds(10));
  TimeDelta delay3(Seconds(15));
  TimeDelta delay4(Seconds(30));
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
  TimeDelta delay1(Seconds(5));
  TimeDelta delay2(Seconds(10));
  TimeDelta delay3(Seconds(15));
  TimeDelta delay4(Seconds(30));
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
  TimeDelta delay1(Seconds(5));
  TimeDelta delay2(Seconds(10));
  TimeDelta delay3(Seconds(15));
  TimeDelta delay4(Seconds(30));
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

  constexpr const int kNumTasks = 100;

  std::vector<std::unique_ptr<CancelableTask>> tasks(100);
  std::vector<TimeTicks> run_times;
  for (int i = 0; i < kNumTasks; i++) {
    tasks[i] = std::make_unique<CancelableTask>(mock_tick_clock());
    queue->task_runner()->PostDelayedTask(
        FROM_HERE,
        BindOnce(&CancelableTask::RecordTimeTask,
                 tasks[i]->weak_factory_.GetWeakPtr(), &run_times),
        Seconds(i + 1));
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
    TimeTicks expected_run_time = FromStartAligned(Seconds(2 * i + 1));
    EXPECT_EQ(run_times[i], expected_run_time);
  }
}

TEST_P(SequenceManagerTest, DelayedTasksNotSelected) {
  auto queue = CreateTaskQueue();
  constexpr TimeDelta kDelay(Milliseconds(10));
  LazyNow lazy_now(mock_tick_clock());
  EXPECT_EQ(std::nullopt, sequence_manager()->GetPendingWakeUp(&lazy_now));
  EXPECT_EQ(
      std::nullopt,
      sequence_manager()->GetPendingWakeUp(
          &lazy_now, SequencedTaskSource::SelectTaskOption::kSkipDelayedTask));

  queue->task_runner()->PostDelayedTask(FROM_HERE, BindOnce(&NopTask), kDelay);

  // No task should be ready to execute.
  EXPECT_FALSE(sequence_manager()->SelectNextTask(
      lazy_now, SequencedTaskSource::SelectTaskOption::kDefault));
  EXPECT_FALSE(sequence_manager()->SelectNextTask(
      lazy_now, SequencedTaskSource::SelectTaskOption::kSkipDelayedTask));

  EXPECT_EQ((WakeUp{lazy_now.Now() + kDelay, kLeeway}),
            sequence_manager()->GetPendingWakeUp(&lazy_now));
  EXPECT_EQ(
      std::nullopt,
      sequence_manager()->GetPendingWakeUp(
          &lazy_now, SequencedTaskSource::SelectTaskOption::kSkipDelayedTask));

  AdvanceMockTickClock(kDelay);
  LazyNow lazy_now2(mock_tick_clock());

  // Delayed task is ready to be executed. Consider it only if not in power
  // suspend state.
  EXPECT_FALSE(sequence_manager()->SelectNextTask(
      lazy_now2, SequencedTaskSource::SelectTaskOption::kSkipDelayedTask));
  EXPECT_EQ(
      std::nullopt,
      sequence_manager()->GetPendingWakeUp(
          &lazy_now2, SequencedTaskSource::SelectTaskOption::kSkipDelayedTask));

  // Execute the delayed task.
  EXPECT_TRUE(sequence_manager()->SelectNextTask(
      lazy_now2, SequencedTaskSource::SelectTaskOption::kDefault));
  sequence_manager()->DidRunTask(lazy_now2);
  EXPECT_EQ(std::nullopt, sequence_manager()->GetPendingWakeUp(&lazy_now2));
}

TEST_P(SequenceManagerTest, DelayedTasksNotSelectedWithImmediateTask) {
  auto queue = CreateTaskQueue();
  constexpr TimeDelta kDelay(Milliseconds(10));
  LazyNow lazy_now(mock_tick_clock());

  EXPECT_EQ(std::nullopt, sequence_manager()->GetPendingWakeUp(&lazy_now));
  EXPECT_EQ(
      std::nullopt,
      sequence_manager()->GetPendingWakeUp(
          &lazy_now, SequencedTaskSource::SelectTaskOption::kSkipDelayedTask));

  // Post an immediate task.
  queue->task_runner()->PostTask(FROM_HERE, BindOnce(&NopTask));
  queue->task_runner()->PostDelayedTask(FROM_HERE, BindOnce(&NopTask), kDelay);

  EXPECT_EQ(WakeUp{}, sequence_manager()->GetPendingWakeUp(&lazy_now));
  EXPECT_EQ(
      WakeUp{},
      sequence_manager()->GetPendingWakeUp(
          &lazy_now, SequencedTaskSource::SelectTaskOption::kSkipDelayedTask));

  AdvanceMockTickClock(kDelay);
  LazyNow lazy_now2(mock_tick_clock());

  // An immediate task is present, even if we skip the delayed tasks.
  EXPECT_EQ(
      WakeUp{},
      sequence_manager()->GetPendingWakeUp(
          &lazy_now2, SequencedTaskSource::SelectTaskOption::kSkipDelayedTask));

  // Immediate task should be ready to execute, execute it.
  EXPECT_TRUE(sequence_manager()->SelectNextTask(
      lazy_now2, SequencedTaskSource::SelectTaskOption::kSkipDelayedTask));
  sequence_manager()->DidRunTask(lazy_now);

  // Delayed task is ready to be executed. Consider it only if not in power
  // suspend state. This test differs from
  // SequenceManagerTest.DelayedTasksNotSelected as it confirms that delayed
  // tasks are ignored even if they're already in the ready queue (per having
  // performed task selection already before running the immediate task above).
  EXPECT_FALSE(sequence_manager()->SelectNextTask(
      lazy_now2, SequencedTaskSource::SelectTaskOption::kSkipDelayedTask));
  EXPECT_EQ(
      std::nullopt,
      sequence_manager()->GetPendingWakeUp(
          &lazy_now2, SequencedTaskSource::SelectTaskOption::kSkipDelayedTask));

  // Execute the delayed task.
  EXPECT_TRUE(sequence_manager()->SelectNextTask(
      lazy_now2, SequencedTaskSource::SelectTaskOption::kDefault));
  EXPECT_EQ(
      std::nullopt,
      sequence_manager()->GetPendingWakeUp(
          &lazy_now2, SequencedTaskSource::SelectTaskOption::kSkipDelayedTask));
  sequence_manager()->DidRunTask(lazy_now2);
}

TEST_P(SequenceManagerTest,
       DelayedTasksNotSelectedWithImmediateTaskWithPriority) {
  auto queues = CreateTaskQueues(4u);
  queues[0]->SetQueuePriority(TestQueuePriority::kLowPriority);
  queues[1]->SetQueuePriority(TestQueuePriority::kNormalPriority);
  queues[2]->SetQueuePriority(TestQueuePriority::kHighPriority);
  queues[3]->SetQueuePriority(TestQueuePriority::kVeryHighPriority);

  // Post immediate tasks.
  queues[0]->task_runner()->PostTask(FROM_HERE, BindOnce(&NopTask));
  queues[2]->task_runner()->PostTask(FROM_HERE, BindOnce(&NopTask));

  // Post delayed tasks.
  constexpr TimeDelta kDelay(Milliseconds(10));
  queues[1]->task_runner()->PostDelayedTask(FROM_HERE, BindOnce(&NopTask),
                                            kDelay);
  queues[3]->task_runner()->PostDelayedTask(FROM_HERE, BindOnce(&NopTask),
                                            kDelay);

  LazyNow lazy_now(mock_tick_clock());

  EXPECT_EQ(
      WakeUp{},
      sequence_manager()->GetPendingWakeUp(
          &lazy_now, SequencedTaskSource::SelectTaskOption::kSkipDelayedTask));

  AdvanceMockTickClock(kDelay);
  LazyNow lazy_now2(mock_tick_clock());

  EXPECT_EQ(
      WakeUp{},
      sequence_manager()->GetPendingWakeUp(
          &lazy_now2, SequencedTaskSource::SelectTaskOption::kSkipDelayedTask));

  // Immediate tasks should be ready to execute, execute them.
  EXPECT_TRUE(sequence_manager()->SelectNextTask(
      lazy_now2, SequencedTaskSource::SelectTaskOption::kSkipDelayedTask));
  sequence_manager()->DidRunTask(lazy_now2);
  EXPECT_TRUE(sequence_manager()->SelectNextTask(
      lazy_now2, SequencedTaskSource::SelectTaskOption::kSkipDelayedTask));
  sequence_manager()->DidRunTask(lazy_now2);

  // No immediate tasks can be executed anymore.
  EXPECT_FALSE(sequence_manager()->SelectNextTask(
      lazy_now2, SequencedTaskSource::SelectTaskOption::kSkipDelayedTask));
  EXPECT_EQ(
      std::nullopt,
      sequence_manager()->GetPendingWakeUp(
          &lazy_now2, SequencedTaskSource::SelectTaskOption::kSkipDelayedTask));

  // Execute delayed tasks.
  EXPECT_TRUE(sequence_manager()->SelectNextTask(lazy_now2));
  sequence_manager()->DidRunTask(lazy_now2);
  EXPECT_TRUE(sequence_manager()->SelectNextTask(lazy_now2));
  sequence_manager()->DidRunTask(lazy_now2);

  // No delayed tasks can be executed anymore.
  EXPECT_FALSE(sequence_manager()->SelectNextTask(lazy_now2));
  EXPECT_EQ(std::nullopt, sequence_manager()->GetPendingWakeUp(&lazy_now2));
}

TEST_P(SequenceManagerTest, GetPendingWakeUp) {
  auto queues = CreateTaskQueues(2u);

  LazyNow lazy_now(mock_tick_clock());
  EXPECT_EQ(std::nullopt, sequence_manager()->GetPendingWakeUp(&lazy_now));

  queues[0]->task_runner()->PostDelayedTask(FROM_HERE, BindOnce(&NopTask),
                                            Seconds(10));

  EXPECT_EQ((WakeUp{lazy_now.Now() + Seconds(10), kLeeway}),
            sequence_manager()->GetPendingWakeUp(&lazy_now));

  queues[1]->task_runner()->PostDelayedTask(FROM_HERE, BindOnce(&NopTask),
                                            Seconds(15));

  EXPECT_EQ((WakeUp{lazy_now.Now() + Seconds(10), kLeeway}),
            sequence_manager()->GetPendingWakeUp(&lazy_now));

  queues[1]->task_runner()->PostDelayedTask(FROM_HERE, BindOnce(&NopTask),
                                            Seconds(5));

  EXPECT_EQ((WakeUp{lazy_now.Now() + Seconds(5), kLeeway}),
            sequence_manager()->GetPendingWakeUp(&lazy_now));

  queues[0]->task_runner()->PostTask(FROM_HERE, BindOnce(&NopTask));

  EXPECT_EQ(WakeUp{}, sequence_manager()->GetPendingWakeUp(&lazy_now));
}

TEST_P(SequenceManagerTest, GetPendingWakeUp_Disabled) {
  auto queue = CreateTaskQueue();

  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter =
      queue->CreateQueueEnabledVoter();
  voter->SetVoteToEnable(false);
  queue->task_runner()->PostTask(FROM_HERE, BindOnce(&NopTask));

  LazyNow lazy_now(mock_tick_clock());
  EXPECT_EQ(std::nullopt, sequence_manager()->GetPendingWakeUp(&lazy_now));
}

TEST_P(SequenceManagerTest, GetPendingWakeUp_Fence) {
  auto queue = CreateTaskQueue();

  queue->InsertFence(TaskQueue::InsertFencePosition::kNow);
  queue->task_runner()->PostTask(FROM_HERE, BindOnce(&NopTask));

  LazyNow lazy_now(mock_tick_clock());
  EXPECT_EQ(std::nullopt, sequence_manager()->GetPendingWakeUp(&lazy_now));
}

TEST_P(SequenceManagerTest, GetPendingWakeUp_FenceUnblocking) {
  auto queue = CreateTaskQueue();

  queue->InsertFence(TaskQueue::InsertFencePosition::kNow);
  queue->task_runner()->PostTask(FROM_HERE, BindOnce(&NopTask));
  queue->InsertFence(TaskQueue::InsertFencePosition::kNow);

  LazyNow lazy_now(mock_tick_clock());
  EXPECT_EQ(WakeUp{}, sequence_manager()->GetPendingWakeUp(&lazy_now));
}

TEST_P(SequenceManagerTest, GetPendingWakeUp_DelayedTaskReady) {
  auto queue = CreateTaskQueue();

  queue->task_runner()->PostDelayedTask(FROM_HERE, BindOnce(&NopTask),
                                        Seconds(1));

  AdvanceMockTickClock(Seconds(10));

  LazyNow lazy_now(mock_tick_clock());
  EXPECT_EQ(WakeUp{}, sequence_manager()->GetPendingWakeUp(&lazy_now));
}

TEST_P(SequenceManagerTest, RemoveAllCanceledDelayedTasksFromFront) {
  auto queue = CreateTaskQueue();

  // Posts a cancelable task.
  CancelableOnceClosure cancelable_closure(base::BindOnce(&NopTask));
  constexpr TimeDelta kDelay = Seconds(1);
  queue->task_runner()->PostDelayedTask(FROM_HERE,
                                        cancelable_closure.callback(), kDelay);

  // Ensure it is picked to calculate the next task time.
  LazyNow lazy_now(mock_tick_clock());
  EXPECT_EQ((WakeUp{lazy_now.Now() + kDelay, kLeeway}),
            sequence_manager()->GetPendingWakeUp(&lazy_now));

  // Canceling the task is not sufficient to ensure it is not considered for the
  // next task time.
  cancelable_closure.Cancel();
  EXPECT_EQ(std::nullopt, sequence_manager()->GetPendingWakeUp(&lazy_now));
}

TEST_P(SequenceManagerTest,
       RemoveAllCanceledDelayedTasksFromFront_MultipleQueues) {
  auto queues = CreateTaskQueues(2u);

  // Post a task in each queue such that they would be executed in order
  // according to their delay.
  CancelableOnceClosure cancelable_closure_1(base::BindOnce(&NopTask));
  constexpr TimeDelta kDelay1 = Seconds(1);
  queues[0]->task_runner()->PostDelayedTask(
      FROM_HERE, cancelable_closure_1.callback(), kDelay1);

  CancelableOnceClosure cancelable_closure_2(base::BindOnce(&NopTask));
  constexpr TimeDelta kDelay2 = Seconds(2);
  queues[1]->task_runner()->PostDelayedTask(
      FROM_HERE, cancelable_closure_2.callback(), kDelay2);

  // The task from the first queue is picked to calculate the next task time.
  LazyNow lazy_now(mock_tick_clock());
  EXPECT_EQ((WakeUp{lazy_now.Now() + kDelay1, kLeeway}),
            sequence_manager()->GetPendingWakeUp(&lazy_now));

  // Test that calling `GetPendingWakeUp()` works when no task is canceled.
  EXPECT_EQ((WakeUp{lazy_now.Now() + kDelay1, kLeeway}),
            sequence_manager()->GetPendingWakeUp(&lazy_now));

  // Canceling the first task which comes from the first queue.
  cancelable_closure_1.Cancel();

  // Now the only task remaining is the one from the second queue.
  EXPECT_EQ((WakeUp{lazy_now.Now() + kDelay2, kLeeway}),
            sequence_manager()->GetPendingWakeUp(&lazy_now));

  // Cancel the remaining task.
  cancelable_closure_2.Cancel();

  // No more valid tasks in any queues.
  EXPECT_EQ(std::nullopt, sequence_manager()->GetPendingWakeUp(&lazy_now));

  // Test that calling `GetPendingWakeUp()` works when no task is canceled.
  EXPECT_EQ(std::nullopt, sequence_manager()->GetPendingWakeUp(&lazy_now));
}

namespace {
void MessageLoopTaskWithDelayedQuit(Fixture* fixture, TaskQueue* task_queue) {
  RunLoop run_loop(RunLoop::Type::kNestableTasksAllowed);
  task_queue->task_runner()->PostDelayedTask(FROM_HERE, run_loop.QuitClosure(),
                                             Milliseconds(100));
  fixture->AdvanceMockTickClock(Milliseconds(200));
  run_loop.Run();
}
}  // namespace

TEST_P(SequenceManagerTest, DelayedTaskRunsInNestedMessageLoop) {
  if (GetUnderlyingRunnerType() == RunnerType::kMockTaskRunner)
    return;
  auto queue = CreateTaskQueue();
  RunLoop run_loop;
  queue->task_runner()->PostTask(
      FROM_HERE,
      BindOnce(&MessageLoopTaskWithDelayedQuit, this, Unretained(queue.get())));
  run_loop.RunUntilIdle();
}

namespace {
void MessageLoopTaskWithImmediateQuit(OnceClosure non_nested_quit_closure,
                                      TaskQueue* task_queue) {
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
  if (GetUnderlyingRunnerType() == RunnerType::kMockTaskRunner)
    return;
  auto queue = CreateTaskQueue();
  RunLoop run_loop;
  queue->task_runner()->PostDelayedTask(
      FROM_HERE,
      BindOnce(&MessageLoopTaskWithImmediateQuit, run_loop.QuitClosure(),
               Unretained(queue.get())),
      Milliseconds(100));

  AdvanceMockTickClock(Milliseconds(200));
  run_loop.Run();
}

TEST_P(SequenceManagerTest, CouldTaskRun_DisableAndReenable) {
  auto queue = CreateTaskQueue();

  EnqueueOrder enqueue_order = sequence_manager()->GetNextSequenceNumber();
  EXPECT_TRUE(GetTaskQueueImpl(queue.get())->CouldTaskRun(enqueue_order));

  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter =
      queue->CreateQueueEnabledVoter();
  voter->SetVoteToEnable(false);
  EXPECT_FALSE(GetTaskQueueImpl(queue.get())->CouldTaskRun(enqueue_order));

  voter->SetVoteToEnable(true);
  EXPECT_TRUE(GetTaskQueueImpl(queue.get())->CouldTaskRun(enqueue_order));
}

TEST_P(SequenceManagerTest, CouldTaskRun_Fence) {
  auto queue = CreateTaskQueue();

  EnqueueOrder enqueue_order = sequence_manager()->GetNextSequenceNumber();
  EXPECT_TRUE(GetTaskQueueImpl(queue.get())->CouldTaskRun(enqueue_order));

  queue->InsertFence(TaskQueue::InsertFencePosition::kNow);
  EXPECT_TRUE(GetTaskQueueImpl(queue.get())->CouldTaskRun(enqueue_order));

  queue->InsertFence(TaskQueue::InsertFencePosition::kBeginningOfTime);
  EXPECT_FALSE(GetTaskQueueImpl(queue.get())->CouldTaskRun(enqueue_order));

  queue->RemoveFence();
  EXPECT_TRUE(GetTaskQueueImpl(queue.get())->CouldTaskRun(enqueue_order));
}

TEST_P(SequenceManagerTest, CouldTaskRun_FenceBeforeThenAfter) {
  auto queue = CreateTaskQueue();

  queue->InsertFence(TaskQueue::InsertFencePosition::kNow);

  EnqueueOrder enqueue_order = sequence_manager()->GetNextSequenceNumber();
  EXPECT_FALSE(GetTaskQueueImpl(queue.get())->CouldTaskRun(enqueue_order));

  queue->InsertFence(TaskQueue::InsertFencePosition::kNow);
  EXPECT_TRUE(GetTaskQueueImpl(queue.get())->CouldTaskRun(enqueue_order));
}

TEST_P(SequenceManagerTest, DelayedDoWorkNotPostedForDisabledQueue) {
  auto queue = CreateTaskQueue();

  queue->task_runner()->PostDelayedTask(FROM_HERE, BindOnce(&NopTask),
                                        Milliseconds(1));
  EXPECT_EQ(FromStartAligned(Milliseconds(1)), NextPendingTaskTime());

  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter =
      queue->CreateQueueEnabledVoter();
  voter->SetVoteToEnable(false);

  EXPECT_EQ(TimeTicks::Max(), NextPendingTaskTime());

  voter->SetVoteToEnable(true);
  EXPECT_EQ(FromStartAligned(Milliseconds(1)), NextPendingTaskTime());
}

TEST_P(SequenceManagerTest, DisablingQueuesChangesDelayTillNextDoWork) {
  auto queues = CreateTaskQueues(3u);
  queues[0]->task_runner()->PostDelayedTask(FROM_HERE, BindOnce(&NopTask),
                                            Milliseconds(1));
  queues[1]->task_runner()->PostDelayedTask(FROM_HERE, BindOnce(&NopTask),
                                            Milliseconds(10));
  queues[2]->task_runner()->PostDelayedTask(FROM_HERE, BindOnce(&NopTask),
                                            Milliseconds(100));

  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter0 =
      queues[0]->CreateQueueEnabledVoter();
  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter1 =
      queues[1]->CreateQueueEnabledVoter();
  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter2 =
      queues[2]->CreateQueueEnabledVoter();

  EXPECT_EQ(FromStartAligned(Milliseconds(1)), NextPendingTaskTime());

  voter0->SetVoteToEnable(false);
  EXPECT_EQ(FromStartAligned(Milliseconds(10)), NextPendingTaskTime());

  voter1->SetVoteToEnable(false);
  EXPECT_EQ(FromStartAligned(Milliseconds(100)), NextPendingTaskTime());

  voter2->SetVoteToEnable(false);
  EXPECT_EQ(TimeTicks::Max(), NextPendingTaskTime());
}

TEST_P(SequenceManagerTest, GetNextDesiredWakeUp) {
  auto queue = CreateTaskQueue();

  EXPECT_EQ(std::nullopt, queue->GetNextDesiredWakeUp());

  TimeTicks start_time = sequence_manager()->NowTicks();
  TimeDelta delay1 = Milliseconds(10);
  TimeDelta delay2 = Milliseconds(2);

  queue->task_runner()->PostDelayedTask(FROM_HERE, BindOnce(&NopTask), delay1);
  EXPECT_EQ(start_time + delay1, queue->GetNextDesiredWakeUp()->time);

  queue->task_runner()->PostDelayedTask(FROM_HERE, BindOnce(&NopTask), delay2);
  EXPECT_EQ(start_time + delay2, queue->GetNextDesiredWakeUp()->time);

  // We don't have wake-ups scheduled for disabled queues.
  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter =
      queue->CreateQueueEnabledVoter();
  voter->SetVoteToEnable(false);
  EXPECT_EQ(std::nullopt, queue->GetNextDesiredWakeUp());

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
  StrictMock<MockTaskQueueThrottler> throttler;
  auto queue = CreateTaskQueue();
  queue->SetThrottler(&throttler);

  TimeTicks start_time = sequence_manager()->NowTicks();
  TimeDelta delay(Milliseconds(1));

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
  sequence_manager()->SetTimeDomain(domain.get());

  // Tidy up.
  queue.reset();
  sequence_manager()->ResetTimeDomain();
}

namespace {
void SetOnTaskHandlers(TaskQueue* task_queue,
                       int* start_counter,
                       int* complete_counter) {
  GetTaskQueueImpl(task_queue)
      ->SetOnTaskStartedHandler(BindRepeating(
          [](int* counter, const Task& task,
             const TaskQueue::TaskTiming& task_timing) { ++(*counter); },
          start_counter));
  GetTaskQueueImpl(task_queue)
      ->SetOnTaskCompletedHandler(BindRepeating(
          [](int* counter, const Task& task, TaskQueue::TaskTiming* task_timing,
             LazyNow* lazy_now) { ++(*counter); },
          complete_counter));
}

void UnsetOnTaskHandlers(TaskQueue* task_queue) {
  GetTaskQueueImpl(task_queue)
      ->SetOnTaskStartedHandler(
          internal::TaskQueueImpl::OnTaskStartedHandler());
  GetTaskQueueImpl(task_queue)
      ->SetOnTaskCompletedHandler(
          internal::TaskQueueImpl::OnTaskCompletedHandler());
}
}  // namespace

TEST_P(SequenceManagerTest, ProcessTasksWithoutTaskTimeObservers) {
  auto queue = CreateTaskQueue();
  int start_counter = 0;
  int complete_counter = 0;
  std::vector<EnqueueOrder> run_order;
  SetOnTaskHandlers(queue.get(), &start_counter, &complete_counter);
  EXPECT_TRUE(GetTaskQueueImpl(queue.get())->RequiresTaskTiming());
  queue->task_runner()->PostTask(FROM_HERE, BindOnce(&TestTask, 1, &run_order));
  queue->task_runner()->PostTask(FROM_HERE, BindOnce(&TestTask, 2, &run_order));
  queue->task_runner()->PostTask(FROM_HERE, BindOnce(&TestTask, 3, &run_order));

  RunLoop().RunUntilIdle();
  EXPECT_EQ(start_counter, 3);
  EXPECT_EQ(complete_counter, 3);
  EXPECT_THAT(run_order, ElementsAre(1u, 2u, 3u));

  UnsetOnTaskHandlers(queue.get());
  EXPECT_FALSE(GetTaskQueueImpl(queue.get())->RequiresTaskTiming());
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
  SetOnTaskHandlers(queue.get(), &start_counter, &complete_counter);
  EXPECT_TRUE(GetTaskQueueImpl(queue.get())->RequiresTaskTiming());
  std::vector<EnqueueOrder> run_order;
  queue->task_runner()->PostTask(FROM_HERE, BindOnce(&TestTask, 1, &run_order));
  queue->task_runner()->PostTask(FROM_HERE, BindOnce(&TestTask, 2, &run_order));

  RunLoop().RunUntilIdle();
  EXPECT_EQ(start_counter, 2);
  EXPECT_EQ(complete_counter, 2);
  EXPECT_THAT(run_order, ElementsAre(1u, 2u));

  UnsetOnTaskHandlers(queue.get());
  EXPECT_FALSE(GetTaskQueueImpl(queue.get())->RequiresTaskTiming());
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
  EXPECT_FALSE(GetTaskQueueImpl(queue.get())->RequiresTaskTiming());
  EXPECT_THAT(run_order, ElementsAre(1u, 2u, 3u, 4u, 5u, 6u));

  SetOnTaskHandlers(queue.get(), &start_counter, &complete_counter);
  queue->task_runner()->PostTask(FROM_HERE, BindOnce(&TestTask, 7, &run_order));
  queue->task_runner()->PostTask(FROM_HERE, BindOnce(&TestTask, 8, &run_order));

  RunLoop().RunUntilIdle();
  EXPECT_EQ(start_counter, 4);
  EXPECT_EQ(complete_counter, 4);
  EXPECT_TRUE(GetTaskQueueImpl(queue.get())->RequiresTaskTiming());
  EXPECT_THAT(run_order, ElementsAre(1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u));
  UnsetOnTaskHandlers(queue.get());
  sequence_manager()->RemoveTaskTimeObserver(&test_task_time_observer);
}

TEST_P(SequenceManagerTest, ObserverNotFiredAfterTaskQueueDestructed) {
  StrictMock<MockTaskQueueThrottler> throttler;
  auto queue = CreateTaskQueue();
  queue->SetThrottler(&throttler);

  // We don't expect the throttler to be notified if the TaskQueue gets
  // destructed.
  auto task_runner = queue->task_runner();
  queue.reset();
  task_runner->PostTask(FROM_HERE, BindOnce(&NopTask));

  FastForwardUntilNoTasksRemain();
}

TEST_P(SequenceManagerTest,
       OnQueueNextWakeUpChangedNotFiredForDisabledQueuePostTask) {
  StrictMock<MockTaskQueueThrottler> throttler;
  auto queue = CreateTaskQueue();
  queue->SetThrottler(&throttler);

  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter =
      queue->CreateQueueEnabledVoter();
  voter->SetVoteToEnable(false);

  // We don't expect the OnHasImmediateTask to be called if the TaskQueue gets
  // disabled.
  auto task_runner = queue->task_runner();
  task_runner->PostTask(FROM_HERE, BindOnce(&NopTask));

  FastForwardUntilNoTasksRemain();
  // When |voter| goes out of scope the queue will become enabled and the
  // observer will fire. We're not interested in testing that however.
  Mock::VerifyAndClearExpectations(&throttler);
}

TEST_P(SequenceManagerTest,
       OnQueueNextWakeUpChangedNotFiredForCrossThreadDisabledQueuePostTask) {
  StrictMock<MockTaskQueueThrottler> throttler;
  auto queue = CreateTaskQueue();
  queue->SetThrottler(&throttler);

  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter =
      queue->CreateQueueEnabledVoter();
  voter->SetVoteToEnable(false);

  // We don't expect OnHasImmediateTask to be called if the TaskQueue gets
  // blocked.
  auto task_runner = queue->task_runner();
  WaitableEvent done_event;
  Thread thread("TestThread");
  thread.Start();
  thread.task_runner()->PostTask(FROM_HERE, BindLambdaForTesting([&] {
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

TEST_P(SequenceManagerTest, GracefulShutdown_ManagerDeletedInFlight) {
  std::vector<TimeTicks> run_times;
  TaskQueue::Handle control_tq = CreateTaskQueue();
  std::vector<TaskQueue::Handle> main_tqs;

  // There might be a race condition - async task queues should be unregistered
  // first. Increase the number of task queues to surely detect that.
  // The problem is that pointers are compared in a set and generally for
  // a small number of allocations value of the pointers increases
  // monotonically. 100 is large enough to force allocations from different
  // pages.
  const int N = 100;
  for (int i = 0; i < N; ++i) {
    main_tqs.push_back(CreateTaskQueue());
  }

  for (int i = 1; i <= 5; ++i) {
    main_tqs[0]->task_runner()->PostDelayedTask(
        FROM_HERE, BindOnce(&RecordTimeTask, &run_times, mock_tick_clock()),
        Milliseconds(i * 100));
  }
  FastForwardBy(Milliseconds(250));

  main_tqs.clear();

  // No leaks should occur when TQM was destroyed before processing
  // shutdown task and TaskQueueImpl should be safely deleted on a correct
  // thread.
  DestroySequenceManager();

  if (GetUnderlyingRunnerType() != RunnerType::kMessagePump) {
    FastForwardUntilNoTasksRemain();
  }

  EXPECT_THAT(run_times, ElementsAre(FromStartAligned(Milliseconds(100)),
                                     FromStartAligned(Milliseconds(200))));
}

TEST_P(SequenceManagerTest, SequenceManagerDeletedWithQueuesToDelete) {
  std::vector<TimeTicks> run_times;
  TaskQueue::Handle main_tq = CreateTaskQueue();
  RefCountedCallbackFactory counter;

  EXPECT_EQ(1u, sequence_manager()->ActiveQueuesCount());
  EXPECT_EQ(0u, sequence_manager()->QueuesToDeleteCount());

  for (int i = 1; i <= 5; ++i) {
    main_tq->task_runner()->PostDelayedTask(
        FROM_HERE,
        counter.WrapCallback(
            BindOnce(&RecordTimeTask, &run_times, mock_tick_clock())),
        Milliseconds(i * 100));
  }
  FastForwardBy(Milliseconds(250));

  main_tq.reset();

  EXPECT_EQ(0u, sequence_manager()->ActiveQueuesCount());
  EXPECT_EQ(1u, sequence_manager()->QueuesToDeleteCount());

  // Ensure that all queues-to-gracefully-shutdown are properly unregistered.
  DestroySequenceManager();

  if (GetUnderlyingRunnerType() != RunnerType::kMessagePump) {
    FastForwardUntilNoTasksRemain();
  }

  EXPECT_THAT(run_times, ElementsAre(FromStartAligned(Milliseconds(100)),
                                     FromStartAligned(Milliseconds(200))));
  EXPECT_FALSE(counter.HasReferences());
}

TEST(SequenceManagerBasicTest, DefaultTaskRunnerSupport) {
  auto base_sequence_manager =
      sequence_manager::CreateSequenceManagerOnCurrentThreadWithPump(
          MessagePump::Create(MessagePumpType::DEFAULT));
  auto queue = base_sequence_manager->CreateTaskQueue(
      sequence_manager::TaskQueue::Spec(QueueName::DEFAULT_TQ));
  base_sequence_manager->SetDefaultTaskRunner(queue->task_runner());

  scoped_refptr<SingleThreadTaskRunner> original_task_runner =
      SingleThreadTaskRunner::GetCurrentDefault();
  scoped_refptr<SingleThreadTaskRunner> custom_task_runner =
      MakeRefCounted<TestSimpleTaskRunner>();
  {
    std::unique_ptr<SequenceManager> manager =
        CreateSequenceManagerOnCurrentThread(SequenceManager::Settings());

    manager->SetDefaultTaskRunner(custom_task_runner);
    DCHECK_EQ(custom_task_runner, SingleThreadTaskRunner::GetCurrentDefault());
  }
  DCHECK_EQ(original_task_runner, SingleThreadTaskRunner::GetCurrentDefault());
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

TEST_P(SequenceManagerTest, TaskQueueDeleted) {
  std::vector<TimeTicks> run_times;
  TaskQueue::Handle main_tq = CreateTaskQueue();
  scoped_refptr<TaskRunner> main_task_runner =
      main_tq->CreateTaskRunner(kTaskTypeNone);

  TaskQueue::Handle other_tq = CreateTaskQueue();
  scoped_refptr<TaskRunner> other_task_runner =
      other_tq->CreateTaskRunner(kTaskTypeNone);

  int start_counter = 0;
  int complete_counter = 0;
  SetOnTaskHandlers(main_tq.get(), &start_counter, &complete_counter);

  EXPECT_EQ(2u, sequence_manager()->ActiveQueuesCount());
  EXPECT_EQ(0u, sequence_manager()->QueuesToDeleteCount());

  for (int i = 1; i <= 5; ++i) {
    main_task_runner->PostDelayedTask(
        FROM_HERE, BindOnce(&RecordTimeTask, &run_times, mock_tick_clock()),
        Milliseconds(i * 100));
  }

  other_task_runner->PostDelayedTask(
      FROM_HERE, BindOnce(&RecordTimeTask, &run_times, mock_tick_clock()),
      Milliseconds(600));

  // TODO(altimin): do not do this after switching to weak pointer-based
  // task handlers.
  UnsetOnTaskHandlers(main_tq.get());

  main_tq.reset();

  EXPECT_EQ(1u, sequence_manager()->ActiveQueuesCount());
  EXPECT_EQ(1u, sequence_manager()->QueuesToDeleteCount());

  FastForwardUntilNoTasksRemain();

  // Only tasks on `other_tq` will run, which will also trigger deleting the
  // `main_tq`'s impl.
  EXPECT_THAT(run_times, ElementsAre(FromStartAligned(Milliseconds(600))));

  EXPECT_EQ(1u, sequence_manager()->ActiveQueuesCount());
  EXPECT_EQ(0u, sequence_manager()->QueuesToDeleteCount());
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

base::OnceClosure PostOnDestruction(TaskQueue* task_queue,
                                    base::OnceClosure task) {
  return RunOnDestruction(base::BindOnce(
      [](base::OnceClosure task, TaskQueue* task_queue) {
        task_queue->task_runner()->PostTask(FROM_HERE, std::move(task));
      },
      std::move(task), Unretained(task_queue)));
}

}  // namespace

TEST_P(SequenceManagerTest, TaskQueueUsedInTaskDestructorAfterShutdown) {
  // This test checks that when a task is posted to a shutdown queue and
  // destroyed, it can try to post a task to the same queue without deadlocks.
  TaskQueue::Handle main_tq = CreateTaskQueue();

  WaitableEvent test_executed(WaitableEvent::ResetPolicy::MANUAL,
                              WaitableEvent::InitialState::NOT_SIGNALED);
  std::unique_ptr<Thread> thread = std::make_unique<Thread>("test thread");
  thread->StartAndWaitForTesting();

  DestroySequenceManager();

  thread->task_runner()->PostTask(
      FROM_HERE, BindOnce(
                     [](TaskQueue* task_queue, WaitableEvent* test_executed) {
                       task_queue->task_runner()->PostTask(
                           FROM_HERE, PostOnDestruction(task_queue,
                                                        base::BindOnce([] {})));
                       test_executed->Signal();
                     },
                     Unretained(main_tq.get()), &test_executed));
  test_executed.Wait();
}

TEST_P(SequenceManagerTest, TaskQueueTaskRunnerDetach) {
  scoped_refptr<SingleThreadTaskRunner> task_runner;
  {
    TaskQueue::Handle queue1 = CreateTaskQueue();
    task_runner = queue1->task_runner();
    EXPECT_TRUE(task_runner->PostTask(FROM_HERE, BindOnce(&NopTask)));
  }
  EXPECT_FALSE(task_runner->PostTask(FROM_HERE, BindOnce(&NopTask)));

  // Create without a sequence manager.
  std::unique_ptr<TaskQueueImpl> queue2 = std::make_unique<TaskQueueImpl>(
      nullptr, nullptr, TaskQueue::Spec(QueueName::TEST_TQ));
  scoped_refptr<SingleThreadTaskRunner> task_runner2 =
      queue2->CreateTaskRunner(0);
  EXPECT_FALSE(task_runner2->PostTask(FROM_HERE, BindOnce(&NopTask)));

  // Tidy up.
  queue2->UnregisterTaskQueue();
}

TEST_P(SequenceManagerTest, DestructorPostChainDuringShutdown) {
  // Checks that a chain of closures which post other closures on destruction do
  // thing on shutdown.
  TaskQueue::Handle task_queue = CreateTaskQueue();
  bool run = false;
  task_queue->task_runner()->PostTask(
      FROM_HERE,
      PostOnDestruction(
          task_queue.get(),
          PostOnDestruction(task_queue.get(),
                            RunOnDestruction(base::BindOnce(
                                [](bool* run) { *run = true; }, &run)))));

  DestroySequenceManager();

  EXPECT_TRUE(run);
}

TEST_P(SequenceManagerTest, DestructorPostsViaTaskRunnerHandleDuringShutdown) {
  TaskQueue::Handle task_queue = CreateTaskQueue();
  bool run = false;
  task_queue->task_runner()->PostTask(
      FROM_HERE, RunOnDestruction(BindLambdaForTesting([&] {
        SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(&NopTask));
        run = true;
      })));

  // Should not DCHECK when SingleThreadTaskRunner::GetCurrentDefault() is
  // invoked.
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
#if BUILDFLAG(IS_WIN)
  supports_high_res = true;
#endif

  // Only the third task needs high resolution timing.
  EXPECT_FALSE(sequence_manager()->HasPendingHighResolutionTasks());
  queue->task_runner()->PostTask(FROM_HERE, BindOnce(&NopTask));
  EXPECT_FALSE(sequence_manager()->HasPendingHighResolutionTasks());
  queue->task_runner()->PostDelayedTask(FROM_HERE, BindOnce(&NopTask),
                                        Milliseconds(100));
  EXPECT_FALSE(sequence_manager()->HasPendingHighResolutionTasks());
  queue->task_runner()->PostDelayedTaskAt(
      subtle::PostDelayedTaskPassKeyForTesting(), FROM_HERE, BindOnce(&NopTask),
      sequence_manager()->NowTicks() + Milliseconds(10),
      subtle::DelayPolicy::kPrecise);
  EXPECT_EQ(sequence_manager()->HasPendingHighResolutionTasks(),
            supports_high_res);

  // Running immediate tasks doesn't affect pending high resolution tasks.
  RunLoop().RunUntilIdle();
  EXPECT_EQ(sequence_manager()->HasPendingHighResolutionTasks(),
            supports_high_res);

  // Advancing to just before a pending low resolution task doesn't mean that we
  // have pending high resolution work.
  AdvanceMockTickClock(Milliseconds(99));
  RunLoop().RunUntilIdle();
  EXPECT_FALSE(sequence_manager()->HasPendingHighResolutionTasks());

  AdvanceMockTickClock(Milliseconds(100));
  RunLoop().RunUntilIdle();
  EXPECT_FALSE(sequence_manager()->HasPendingHighResolutionTasks());
}

TEST_P(SequenceManagerTest, HasPendingHighResolutionTasksLowPriority) {
  auto queue = CreateTaskQueue();
  queue->SetQueuePriority(TestQueuePriority::kLowPriority);
  bool supports_high_res = false;
#if BUILDFLAG(IS_WIN)
  supports_high_res = true;
#endif

  // No task should be considered high resolution in a low priority queue.
  EXPECT_FALSE(sequence_manager()->HasPendingHighResolutionTasks());
  queue->task_runner()->PostTask(FROM_HERE, BindOnce(&NopTask));
  EXPECT_FALSE(sequence_manager()->HasPendingHighResolutionTasks());
  queue->task_runner()->PostDelayedTask(FROM_HERE, BindOnce(&NopTask),
                                        Milliseconds(100));
  EXPECT_FALSE(sequence_manager()->HasPendingHighResolutionTasks());
  queue->task_runner()->PostDelayedTaskAt(
      subtle::PostDelayedTaskPassKeyForTesting(), FROM_HERE, BindOnce(&NopTask),
      sequence_manager()->NowTicks() + Milliseconds(10),
      subtle::DelayPolicy::kPrecise);
  EXPECT_FALSE(sequence_manager()->HasPendingHighResolutionTasks());

  // Increasing queue priority should enable high resolution timer.
  queue->SetQueuePriority(TestQueuePriority::kNormalPriority);
  EXPECT_EQ(sequence_manager()->HasPendingHighResolutionTasks(),
            supports_high_res);
  queue->SetQueuePriority(TestQueuePriority::kLowPriority);
  EXPECT_FALSE(sequence_manager()->HasPendingHighResolutionTasks());

  // Running immediate tasks doesn't affect pending high resolution tasks.
  RunLoop().RunUntilIdle();
  EXPECT_FALSE(sequence_manager()->HasPendingHighResolutionTasks());

  // Advancing to just before a pending low resolution task doesn't mean that we
  // have pending high resolution work.
  AdvanceMockTickClock(Milliseconds(99));
  RunLoop().RunUntilIdle();
  EXPECT_FALSE(sequence_manager()->HasPendingHighResolutionTasks());

  AdvanceMockTickClock(Milliseconds(100));
  RunLoop().RunUntilIdle();
  EXPECT_FALSE(sequence_manager()->HasPendingHighResolutionTasks());
}

TEST_P(SequenceManagerTest,
       HasPendingHighResolutionTasksLowAndNormalPriorityQueues) {
  auto queueLow = CreateTaskQueue();
  queueLow->SetQueuePriority(TestQueuePriority::kLowPriority);
  auto queueNormal = CreateTaskQueue();
  queueNormal->SetQueuePriority(TestQueuePriority::kNormalPriority);
  bool supports_high_res = false;
#if BUILDFLAG(IS_WIN)
  supports_high_res = true;
#endif

  // No task should be considered high resolution in a low priority queue.
  EXPECT_FALSE(sequence_manager()->HasPendingHighResolutionTasks());
  queueLow->task_runner()->PostDelayedTaskAt(
      subtle::PostDelayedTaskPassKeyForTesting(), FROM_HERE, BindOnce(&NopTask),
      sequence_manager()->NowTicks() + Milliseconds(10),
      subtle::DelayPolicy::kPrecise);
  EXPECT_FALSE(sequence_manager()->HasPendingHighResolutionTasks());
  queueNormal->task_runner()->PostDelayedTask(FROM_HERE, BindOnce(&NopTask),
                                              Milliseconds(100));
  EXPECT_FALSE(sequence_manager()->HasPendingHighResolutionTasks());

  // Increasing queue priority should enable high resolution timer.
  queueLow->SetQueuePriority(TestQueuePriority::kNormalPriority);
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
    CHECK(tasks_alive_->find(full_name()) != tasks_alive_->end(),
          base::NotFatalUntil::M125);
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
  raw_ptr<std::set<std::string>> tasks_alive_;
  raw_ptr<std::vector<std::string>> tasks_deleted_;
};

void CallbackWithDestructor(std::unique_ptr<PostTaskWhenDeleted> object) {}

}  // namespace

TEST_P(SequenceManagerTest, DoesNotRecordQueueTimeIfSettingFalse) {
  auto queue = CreateTaskQueue();

  QueueTimeTaskObserver observer;
  sequence_manager()->AddTaskObserver(&observer);

  // We do not record task queue time when the setting is false.
  sequence_manager()->SetAddQueueTimeToTasks(false);
  AdvanceMockTickClock(Milliseconds(99));
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
  AdvanceMockTickClock(Milliseconds(99));
  queue->task_runner()->PostTask(FROM_HERE, BindOnce(&NopTask));
  RunLoop().RunUntilIdle();
  EXPECT_THAT(observer.queue_times(),
              ElementsAre(kStartTime + Milliseconds(99)));

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

  raw_ptr<bool> task_destroyed_;
  raw_ptr<bool> destruction_observer_called_;
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
  raw_ptr<bool> task_destroyed_;
  raw_ptr<bool> destruction_observer_called_;
  bool task_destroyed_before_message_loop_;
};

}  // namespace

TEST_P(SequenceManagerTest, DestructionObserverTest) {
  auto queue = CreateTaskQueue();

  // Verify that the destruction observer gets called at the very end (after
  // all the pending tasks have been destroyed).
  const TimeDelta kDelay = Milliseconds(100);

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
    case RunnerType::kMessagePump:
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

  // TickClock:
  TimeTicks NowTicks() const override { return now_; }

  // TimeDomain:
  bool MaybeFastForwardToWakeUp(std::optional<WakeUp> wakeup,
                                bool quit_when_idle_requested) override {
    return MaybeFastForwardToWakeUp(quit_when_idle_requested);
  }

  MOCK_METHOD1(MaybeFastForwardToWakeUp, bool(bool quit_when_idle_requested));

  const char* GetName() const override { return "Test"; }

 private:
  TimeTicks now_;
};

}  // namespace

TEST_P(SequenceManagerTest, OnIdleTimeDomainNotification) {
  if (GetUnderlyingRunnerType() != RunnerType::kMessagePump)
    return;

  auto queue = CreateTaskQueue();

  // If we call OnIdle, we expect registered TimeDomains to receive a call to
  // MaybeFastForwardToWakeUp.  If no run loop has requested quit on idle, the
  // parameter passed in should be false.
  StrictMock<MockTimeDomain> mock_time_domain;
  sequence_manager()->SetTimeDomain(&mock_time_domain);
  EXPECT_CALL(mock_time_domain, MaybeFastForwardToWakeUp(false))
      .WillOnce(Return(false));
  sequence_manager()->OnIdle();
  sequence_manager()->ResetTimeDomain();
  Mock::VerifyAndClearExpectations(&mock_time_domain);

  // However if RunUntilIdle is called it should be true.
  queue->task_runner()->PostTask(
      FROM_HERE, BindLambdaForTesting([&] {
        StrictMock<MockTimeDomain> mock_time_domain;
        EXPECT_CALL(mock_time_domain, MaybeFastForwardToWakeUp(true))
            .WillOnce(Return(false));
        sequence_manager()->SetTimeDomain(&mock_time_domain);
        sequence_manager()->OnIdle();
        sequence_manager()->ResetTimeDomain();
      }));

  RunLoop().RunUntilIdle();
}

TEST_P(SequenceManagerTest, CreateTaskQueue) {
  TaskQueue::Handle task_queue =
      sequence_manager()->CreateTaskQueue(TaskQueue::Spec(QueueName::TEST_TQ));
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
                                            Milliseconds(10));
  EXPECT_EQ(6u, sequence_manager()->GetPendingTaskCountForTesting());

  queues[2]->task_runner()->PostDelayedTask(FROM_HERE, BindOnce(&NopTask),
                                            Milliseconds(20));
  EXPECT_EQ(7u, sequence_manager()->GetPendingTaskCountForTesting());

  RunLoop().RunUntilIdle();
  EXPECT_EQ(2u, sequence_manager()->GetPendingTaskCountForTesting());

  AdvanceMockTickClock(Milliseconds(10));
  RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, sequence_manager()->GetPendingTaskCountForTesting());

  AdvanceMockTickClock(Milliseconds(10));
  RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, sequence_manager()->GetPendingTaskCountForTesting());
}

TEST_P(SequenceManagerTest, PostDelayedTaskFromOtherThread) {
  TaskQueue::Handle main_tq = CreateTaskQueue();
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
                       task_runner->PostDelayedTask(FROM_HERE,
                                                    BindOnce(&NopTask),
                                                    base::Milliseconds(10));
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
                               base::Milliseconds(10));
}

void PostTaskB(scoped_refptr<TaskRunner> task_runner) {
  task_runner->PostTask(FROM_HERE, BindOnce(&NopTask));
  task_runner->PostDelayedTask(FROM_HERE, BindOnce(&NopTask),
                               base::Milliseconds(20));
}

void PostTaskC(scoped_refptr<TaskRunner> task_runner) {
  task_runner->PostTask(FROM_HERE, BindOnce(&NopTask));
  task_runner->PostDelayedTask(FROM_HERE, BindOnce(&NopTask),
                               base::Milliseconds(30));
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
  auto queues = CreateTaskQueues(
      static_cast<size_t>(TestQueuePriority::kQueuePriorityCount));

  for (uint8_t priority = 0;
       priority < static_cast<uint8_t>(TestQueuePriority::kQueuePriorityCount);
       priority++) {
    if (priority != static_cast<uint8_t>(TestQueuePriority::kNormalPriority)) {
      queues[priority]->SetQueuePriority(
          static_cast<TaskQueue::QueuePriority>(priority));
    }
  }

  std::string order;
  for (int i = 0; i < 60; i++) {
    for (uint8_t priority = 0;
         priority <
         static_cast<uint8_t>(TestQueuePriority::kQueuePriorityCount);
         priority++) {
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
  CancelableTaskWithDestructionObserver() = default;

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
                   BindLambdaForTesting([&] { task1_deleted = true; }))),
      Hours(1));

  queue->task_runner()->PostDelayedTask(
      FROM_HERE,
      BindOnce(&CancelableTaskWithDestructionObserver::Task,
               task2.weak_factory_.GetWeakPtr(),
               std::make_unique<ScopedClosureRunner>(
                   BindLambdaForTesting([&] { task2_deleted = true; }))),
      Hours(2));

  queue->task_runner()->PostDelayedTask(
      FROM_HERE,
      BindOnce(&CancelableTaskWithDestructionObserver::Task,
               task3.weak_factory_.GetWeakPtr(),
               std::make_unique<ScopedClosureRunner>(
                   BindLambdaForTesting([&] { task3_deleted = true; }))),
      Hours(3));

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
  MOCK_METHOD2(Set, void(debug::CrashKeyString*, std::string_view));
  MOCK_METHOD1(Clear, void(debug::CrashKeyString*));
  MOCK_METHOD1(OutputCrashKeysToStream, void(std::ostream&));
};

}  // namespace

TEST_P(SequenceManagerTest, CrossQueueTaskPostingWhenQueueDeleted) {
  MockTask task;
  auto queue_1 = CreateTaskQueue();
  auto queue_2 = CreateTaskQueue();

  EXPECT_CALL(task, Run).Times(1);

  queue_1->task_runner()->PostDelayedTask(
      FROM_HERE, PostOnDestruction(queue_2.get(), task.Get()), Minutes(1));

  queue_1.reset();

  FastForwardUntilNoTasksRemain();
}

TEST_P(SequenceManagerTest, UnregisterTaskQueueTriggersScheduleWork) {
  constexpr auto kDelay = Minutes(1);
  auto queue_1 = CreateTaskQueue();
  auto queue_2 = CreateTaskQueue();

  MockTask task;
  EXPECT_CALL(task, Run).Times(1);

  queue_1->task_runner()->PostDelayedTask(FROM_HERE, task.Get(), kDelay);
  queue_2->task_runner()->PostDelayedTask(FROM_HERE, task.Get(), kDelay * 2);

  AdvanceMockTickClock(kDelay * 2);

  // Wakeup time needs to be adjusted to kDelay * 2 when the queue is
  // unregistered from the TimeDomain
  queue_1.reset();

  RunLoop().RunUntilIdle();
}

TEST_P(SequenceManagerTest, ReclaimMemoryRemovesCorrectQueueFromSet) {
  auto queue1 = CreateTaskQueue();
  auto queue2 = CreateTaskQueue();
  auto queue3 = CreateTaskQueue();
  auto queue4 = CreateTaskQueue();

  std::vector<int> order;

  CancelableRepeatingClosure cancelable_closure1(
      BindLambdaForTesting([&] { order.push_back(10); }));
  CancelableRepeatingClosure cancelable_closure2(
      BindLambdaForTesting([&] { order.push_back(11); }));
  queue1->task_runner()->PostTask(FROM_HERE, BindLambdaForTesting([&] {
                                    order.push_back(1);
                                    cancelable_closure1.Cancel();
                                    cancelable_closure2.Cancel();
                                    // This should remove |queue4| from the work
                                    // queue set,
                                    sequence_manager()->ReclaimMemory();
                                  }));
  queue2->task_runner()->PostTask(
      FROM_HERE, BindLambdaForTesting([&] { order.push_back(2); }));
  queue3->task_runner()->PostTask(
      FROM_HERE, BindLambdaForTesting([&] { order.push_back(3); }));
  queue4->task_runner()->PostTask(FROM_HERE, cancelable_closure1.callback());
  queue4->task_runner()->PostTask(FROM_HERE, cancelable_closure2.callback());

  RunLoop().RunUntilIdle();

  // Make sure ReclaimMemory didn't prevent the task from |queue2| from running.
  EXPECT_THAT(order, ElementsAre(1, 2, 3));
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
      FROM_HERE, BindLambdaForTesting([&] { run_loop.Quit(); }));
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
  GetTaskQueueImpl(queue.get())->SetQueueEnabled(false);
  GetTaskQueueImpl(queue.get())->SetQueueEnabled(true);

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
  queue->InsertFence(TaskQueue::InsertFencePosition::kBeginningOfTime);
  queue->RemoveFence();

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
  queue->InsertFence(TaskQueue::InsertFencePosition::kNow);
  queue->RemoveFence();

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

  queue->InsertFence(TaskQueue::InsertFencePosition::kNow);
  queue->task_runner()->PostTask(FROM_HERE, DoNothing());
  queue->RemoveFence();

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
  queue->SetQueuePriority(TestQueuePriority::kLowPriority);
  queue->SetQueuePriority(TestQueuePriority::kNormalPriority);

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

  queue->SetQueuePriority(TestQueuePriority::kLowPriority);
  queue->task_runner()->PostTask(FROM_HERE, DoNothing());
  queue->SetQueuePriority(TestQueuePriority::kNormalPriority);

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
  queue->SetQueuePriority(TestQueuePriority::kLowPriority);

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

  queue->SetQueuePriority(TestQueuePriority::kLowPriority);
  queue->task_runner()->PostTask(FROM_HERE, DoNothing());
  queue->InsertFence(TaskQueue::InsertFencePosition::kBeginningOfTime);
  queue->task_runner()->PostTask(FROM_HERE, DoNothing());
  queue->RemoveFence();
  queue->task_runner()->PostTask(FROM_HERE, DoNothing());
  queue->SetQueuePriority(TestQueuePriority::kNormalPriority);
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

  queue->SetQueuePriority(TestQueuePriority::kLowPriority);
  queue->task_runner()->PostTask(FROM_HERE, DoNothing());
  queue->InsertFence(TaskQueue::InsertFencePosition::kBeginningOfTime);
  queue->task_runner()->PostTask(FROM_HERE, DoNothing());
  queue->RemoveFence();
  queue->task_runner()->PostTask(FROM_HERE, DoNothing());

  EXPECT_CALL(observer,
              WillProcessTask(_, /*was_blocked_or_low_priority=*/true))
      .Times(3);
  EXPECT_CALL(observer, DidProcessTask(_)).Times(3);
  RunLoop().RunUntilIdle();
  testing::Mock::VerifyAndClear(&observer);

  queue->SetQueuePriority(TestQueuePriority::kNormalPriority);
  queue->task_runner()->PostTask(FROM_HERE, DoNothing());
  EXPECT_CALL(observer,
              WillProcessTask(_, /*was_blocked_or_low_priority=*/false));
  EXPECT_CALL(observer, DidProcessTask(_));
  RunLoop().RunUntilIdle();

  sequence_manager()->RemoveTaskObserver(&observer);
}

TEST_P(SequenceManagerTest, DelayedTaskOrderFromMultipleQueues) {
  // Regression test for crbug.com/1249857. The 4th task posted below should run
  // 4th despite being in queues[0].
  std::vector<EnqueueOrder> run_order;
  auto queues = CreateTaskQueues(3u);

  queues[0]->task_runner()->PostDelayedTask(
      FROM_HERE, BindOnce(&TestTask, 1, &run_order), Milliseconds(9));
  queues[1]->task_runner()->PostDelayedTask(
      FROM_HERE, BindOnce(&TestTask, 2, &run_order), Milliseconds(10));
  queues[2]->task_runner()->PostDelayedTask(
      FROM_HERE, BindOnce(&TestTask, 3, &run_order), Milliseconds(10));
  queues[0]->task_runner()->PostDelayedTask(
      FROM_HERE, BindOnce(&TestTask, 4, &run_order), Milliseconds(100));

  // All delayed tasks are now ready, but none have run.
  AdvanceMockTickClock(Milliseconds(100));
  RunLoop().RunUntilIdle();

  EXPECT_THAT(run_order, ElementsAre(1u, 2u, 3u, 4u));
}

TEST_P(SequenceManagerTest, OnTaskPostedCallbacks) {
  int counter1 = 0;
  int counter2 = 0;

  auto queue = CreateTaskQueue();

  std::unique_ptr<TaskQueue::OnTaskPostedCallbackHandle> handle1 =
      queue->AddOnTaskPostedHandler(BindRepeating(
          [](int* counter, const Task& task) { ++(*counter); }, &counter1));

  queue->task_runner()->PostTask(FROM_HERE, BindOnce(NullTask));
  EXPECT_EQ(1, counter1);
  EXPECT_EQ(0, counter2);

  std::unique_ptr<TaskQueue::OnTaskPostedCallbackHandle> handle2 =
      queue->AddOnTaskPostedHandler(BindRepeating(
          [](int* counter, const Task& task) { ++(*counter); }, &counter2));

  queue->task_runner()->PostTask(FROM_HERE, BindOnce(NullTask));
  EXPECT_EQ(2, counter1);
  EXPECT_EQ(1, counter2);

  handle1.reset();

  queue->task_runner()->PostTask(FROM_HERE, BindOnce(NullTask));
  EXPECT_EQ(2, counter1);
  EXPECT_EQ(2, counter2);

  handle2.reset();

  queue->task_runner()->PostTask(FROM_HERE, BindOnce(NullTask));
  EXPECT_EQ(2, counter1);
  EXPECT_EQ(2, counter2);
}

// `RunOrPostTask` is tightly integrated with `ThreadControllerWithMessagePump`
// and `RunLoop` so its tests can't use `SequenceManagerTest`.
class SequenceManagerRunOrPostTaskTest : public testing::Test {
 public:
  SequenceManagerRunOrPostTaskTest() {
    auto settings = SequenceManager::Settings::Builder().Build();
    auto thread_controller =
        std::make_unique<ThreadControllerWithMessagePumpImpl>(
            std::make_unique<MessagePumpDefault>(), settings);
    sequence_manager_ = SequenceManagerForTest::Create(
        std::move(thread_controller), std::move(settings));
    queue_ =
        sequence_manager_->CreateTaskQueue(TaskQueue::Spec(QueueName::TEST_TQ));
    other_queue_ =
        sequence_manager_->CreateTaskQueue(TaskQueue::Spec(QueueName::TEST_TQ));
    sequence_manager_->SetDefaultTaskRunner(queue_->task_runner());

    thread_.Start();
  }

  SequenceManagerImpl* sequence_manager() { return sequence_manager_.get(); }
  TaskQueue* queue() { return queue_.get(); }
  TaskQueue* other_queue() { return other_queue_.get(); }
  SingleThreadTaskRunner* task_runner() { return queue_->task_runner().get(); }
  SingleThreadTaskRunner* other_task_runner() {
    return other_queue_->task_runner().get();
  }
  SingleThreadTaskRunner* other_thread_task_runner() {
    return thread_.task_runner().get();
  }

  void FlushOtherThread() { thread_.FlushForTesting(); }

  // Allow tasks to run synchronously. This imitates being inside a `RunLoop`,
  // but allows the test's body to keep running.
  void SimulateInsideRunLoop() {
    sequence_manager_->SetRunTaskSynchronouslyAllowed(true);
  }

 private:
  std::unique_ptr<SequenceManagerForTest> sequence_manager_;
  TaskQueue::Handle queue_;
  TaskQueue::Handle other_queue_;
  Thread thread_{"OtherThread"};
};

// Verify that `RunOrPostTask` from the bound thread does not run the task
// synchronously if there is no active `RunLoop`.
TEST_F(SequenceManagerRunOrPostTaskTest, FromBoundThreadOutsideRunLoop) {
  bool did_run = false;
  EXPECT_TRUE(task_runner()->RunOrPostTask(
      subtle::RunOrPostTaskPassKeyForTesting(), FROM_HERE,
      BindLambdaForTesting([&] { did_run = true; })));
  EXPECT_FALSE(did_run);
  RunLoop().RunUntilIdle();
  EXPECT_TRUE(did_run);
}

// Verify that `RunOrPostTask` from another thread does not run the task
// synchronously if there is no active `RunLoop`.
TEST_F(SequenceManagerRunOrPostTaskTest, FromOtherThreadOutsideRunLoop) {
  bool did_run = false;
  other_thread_task_runner()->PostTask(
      FROM_HERE, BindLambdaForTesting([&] {
        EXPECT_TRUE(task_runner()->RunOrPostTask(
            subtle::RunOrPostTaskPassKeyForTesting(), FROM_HERE,
            BindLambdaForTesting([&] { did_run = true; })));
      }));

  FlushOtherThread();
  EXPECT_FALSE(did_run);
  RunLoop().RunUntilIdle();
  EXPECT_TRUE(did_run);
}

// Verify that `RunOrPostTask` from a task running on the bound thread does not
// run the task synchronously.
TEST_F(SequenceManagerRunOrPostTaskTest, FromInsideTask) {
  bool did_run = false;
  EXPECT_TRUE(task_runner()->PostTask(
      FROM_HERE, BindLambdaForTesting([&] {
        EXPECT_TRUE(task_runner()->RunOrPostTask(
            subtle::RunOrPostTaskPassKeyForTesting(), FROM_HERE,
            BindLambdaForTesting([&] { did_run = true; })));
        EXPECT_FALSE(did_run);
      })));
  RunLoop().RunUntilIdle();
  EXPECT_TRUE(did_run);
}

// Verify that `RunOrPostTask` from another thread does not run the task
// synchronously if there is a queued task.
TEST_F(SequenceManagerRunOrPostTaskTest, FromOtherThreadWithQueuedTask) {
  SimulateInsideRunLoop();
  EXPECT_TRUE(task_runner()->PostTask(FROM_HERE, DoNothing()));

  bool did_run = false;
  EXPECT_TRUE(other_thread_task_runner()->PostTask(
      FROM_HERE, BindLambdaForTesting([&] {
        EXPECT_TRUE(task_runner()->RunOrPostTask(
            subtle::RunOrPostTaskPassKeyForTesting(), FROM_HERE,
            BindLambdaForTesting([&] { did_run = true; })));
        EXPECT_FALSE(did_run);
      })));

  FlushOtherThread();
  EXPECT_FALSE(did_run);
  RunLoop().RunUntilIdle();
  EXPECT_TRUE(did_run);
}

// Verify that `RunOrPostTask` from another thread does not run the task
// synchronously if there is a queued task in a different queue from the same
// `SequenceManager`.
TEST_F(SequenceManagerRunOrPostTaskTest,
       FromOtherThreadWithQueuedTaskOtherQueue) {
  SimulateInsideRunLoop();
  EXPECT_TRUE(other_task_runner()->PostTask(FROM_HERE, DoNothing()));

  bool did_run = false;
  EXPECT_TRUE(other_thread_task_runner()->PostTask(
      FROM_HERE, BindLambdaForTesting([&] {
        EXPECT_TRUE(task_runner()->RunOrPostTask(
            subtle::RunOrPostTaskPassKeyForTesting(), FROM_HERE,
            BindLambdaForTesting([&] { did_run = true; })));
        EXPECT_FALSE(did_run);
      })));

  FlushOtherThread();
  EXPECT_FALSE(did_run);
  RunLoop().RunUntilIdle();
  EXPECT_TRUE(did_run);
}

// Verify that `RunOrPostTask` from another thread runs the task synchronously
// if there is no queued or running task.
TEST_F(SequenceManagerRunOrPostTaskTest, FromOtherThreadNoQueuedOrRunningTask) {
  SimulateInsideRunLoop();

  bool did_run = false;
  EXPECT_TRUE(other_thread_task_runner()->PostTask(
      FROM_HERE, BindLambdaForTesting([&] {
        EXPECT_TRUE(task_runner()->RunOrPostTask(
            subtle::RunOrPostTaskPassKeyForTesting(), FROM_HERE,
            BindLambdaForTesting([&] { did_run = true; })));
        EXPECT_TRUE(did_run);
      })));

  FlushOtherThread();
  EXPECT_TRUE(did_run);
}

// Verify that `RunOrPostTask` from another thread does not run the task
// synchronously when "internal work" is simulated.
TEST_F(SequenceManagerRunOrPostTaskTest, FromOtherThreadInternalWork) {
  SimulateInsideRunLoop();

  // Simulate internal work execution in the message pump.
  sequence_manager()->OnBeginWork();

  bool did_run = false;
  EXPECT_TRUE(other_thread_task_runner()->PostTask(
      FROM_HERE, BindLambdaForTesting([&] {
        EXPECT_TRUE(task_runner()->RunOrPostTask(
            subtle::RunOrPostTaskPassKeyForTesting(), FROM_HERE,
            BindLambdaForTesting([&] { did_run = true; })));
        EXPECT_FALSE(did_run);
      })));
  FlushOtherThread();
  EXPECT_FALSE(did_run);
  RunLoop().RunUntilIdle();
  EXPECT_TRUE(did_run);
}

// Verify that `RunOrPostTask` from another thread runs the task synchronously
// if there is no running task and the only queued task is in a different queue
// which is disabled.
TEST_F(SequenceManagerRunOrPostTaskTest, FromOtherThreadDisabledQueue) {
  auto voter = queue()->CreateQueueEnabledVoter();
  voter->SetVoteToEnable(false);
  // Reload empty work queues (tasks can't run synchronously when there are
  // pending requests to reload empty work queues).
  RunLoop().RunUntilIdle();
  SimulateInsideRunLoop();

  bool did_run = false;
  EXPECT_TRUE(other_thread_task_runner()->PostTask(
      FROM_HERE, BindLambdaForTesting([&] {
        EXPECT_TRUE(task_runner()->RunOrPostTask(
            subtle::RunOrPostTaskPassKeyForTesting(), FROM_HERE,
            BindLambdaForTesting([&] { did_run = true; })));
        EXPECT_FALSE(did_run);
      })));

  FlushOtherThread();
  EXPECT_FALSE(did_run);
  RunLoop().RunUntilIdle();
  EXPECT_FALSE(did_run);
  voter.reset();
  RunLoop().RunUntilIdle();
  EXPECT_TRUE(did_run);
}

// Verify that `RunOrPostTask` from another thread runs the task synchronously
// if there is no running task and the only queued task is in a different queue
// which is disabled.
TEST_F(SequenceManagerRunOrPostTaskTest,
       FromOtherThreadQueuedTaskInDisabledOtherQueue) {
  bool did_run_other_task = false;
  EXPECT_TRUE(other_task_runner()->PostTask(
      FROM_HERE, BindLambdaForTesting([&] { did_run_other_task = true; })));
  auto voter = other_queue()->CreateQueueEnabledVoter();
  voter->SetVoteToEnable(false);
  // Reload empty work queues (tasks can't run synchronously when there are
  // pending requests to reload empty work queues).
  RunLoop().RunUntilIdle();
  EXPECT_FALSE(did_run_other_task);
  SimulateInsideRunLoop();

  bool did_run = false;
  EXPECT_TRUE(other_thread_task_runner()->PostTask(
      FROM_HERE, BindLambdaForTesting([&] {
        EXPECT_TRUE(task_runner()->RunOrPostTask(
            subtle::RunOrPostTaskPassKeyForTesting(), FROM_HERE,
            BindLambdaForTesting([&] { did_run = true; })));
        EXPECT_TRUE(did_run);
      })));

  FlushOtherThread();
  EXPECT_TRUE(did_run);
}

// Verify that `RunOrPostTask` from another thread does not run the task
// synchronously if there is a running task.
TEST_F(SequenceManagerRunOrPostTaskTest, FromOtherThreadWithRunningTask) {
  WaitableEvent main_thread_task_running;
  WaitableEvent run_or_post_task_done;
  task_runner()->PostTask(FROM_HERE, BindLambdaForTesting([&] {
                            main_thread_task_running.Signal();
                            run_or_post_task_done.Wait();
                          }));

  bool did_run = false;
  EXPECT_TRUE(other_thread_task_runner()->PostTask(
      FROM_HERE, BindLambdaForTesting([&] {
        main_thread_task_running.Wait();
        EXPECT_TRUE(task_runner()->RunOrPostTask(
            subtle::RunOrPostTaskPassKeyForTesting(), FROM_HERE,
            BindLambdaForTesting([&] { did_run = true; })));
        EXPECT_FALSE(did_run);
        run_or_post_task_done.Signal();
      })));

  RunLoop().RunUntilIdle();
  EXPECT_TRUE(did_run);
}

// Verify that `RunOrPostTask` from another thread does not run the task
// synchronously if there is a running task blocked in a nested loop.
TEST_F(SequenceManagerRunOrPostTaskTest,
       FromOtherThreadWithRunningTaskInNestedLoop) {
  WaitableEvent main_thread_task_running;
  RunLoop nested_run_loop(RunLoop::Type::kNestableTasksAllowed);
  auto nested_run_loop_quit_closure = nested_run_loop.QuitClosure();
  task_runner()->PostTask(FROM_HERE, BindLambdaForTesting([&] {
                            main_thread_task_running.Signal();
                            nested_run_loop.Run();
                          }));

  thread_local bool is_main_thread = false;
  is_main_thread = true;

  EXPECT_TRUE(other_thread_task_runner()->PostTask(
      FROM_HERE, BindLambdaForTesting([&] {
        EXPECT_FALSE(is_main_thread);
        main_thread_task_running.Wait();
        // Wait to increase chances of posting while the main thread task is in
        // a nested `RunLoop`.
        PlatformThread::Sleep(TestTimeouts::tiny_timeout());
        EXPECT_TRUE(task_runner()->RunOrPostTask(
            subtle::RunOrPostTaskPassKeyForTesting(), FROM_HERE,
            BindLambdaForTesting([&] {
              // Should not run synchronously on the other thread.
              EXPECT_TRUE(is_main_thread);
              std::move(nested_run_loop_quit_closure).Run();
            })));
      })));

  RunLoop().RunUntilIdle();
  FlushOtherThread();
}

// Verify that `RunOrPostTask` from another thread does not run the task
// synchronously if there is a running task in a different queue from the same
// `SequenceManager`.
TEST_F(SequenceManagerRunOrPostTaskTest,
       FromOtherThreadWithRunningTaskOtherQueue) {
  WaitableEvent main_thread_task_running;
  WaitableEvent run_or_post_task_done;
  other_task_runner()->PostTask(FROM_HERE, BindLambdaForTesting([&] {
                                  main_thread_task_running.Signal();
                                  run_or_post_task_done.Wait();
                                }));

  bool did_run = false;
  EXPECT_TRUE(other_thread_task_runner()->PostTask(
      FROM_HERE, BindLambdaForTesting([&] {
        main_thread_task_running.Wait();
        EXPECT_TRUE(task_runner()->RunOrPostTask(
            subtle::RunOrPostTaskPassKeyForTesting(), FROM_HERE,
            BindLambdaForTesting([&] { did_run = true; })));
        EXPECT_FALSE(did_run);
        run_or_post_task_done.Signal();
      })));

  RunLoop().RunUntilIdle();
  EXPECT_TRUE(did_run);
}

// Verify that a task run synchronously inside `RunOrPostTask` prevents another
// task from starting on the bound thread.
TEST_F(SequenceManagerRunOrPostTaskTest,
       MainThreadCantStartTaskDuringRunOrPostTask) {
  RunLoop run_loop;

  bool sync_task_done = true;

  task_runner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&] {
        EXPECT_TRUE(other_thread_task_runner()->PostTask(
            FROM_HERE, BindLambdaForTesting([&] {
              EXPECT_TRUE(task_runner()->RunOrPostTask(
                  subtle::RunOrPostTaskPassKeyForTesting(), FROM_HERE,
                  BindLambdaForTesting([&] {
                    task_runner()->PostTask(FROM_HERE,
                                            BindLambdaForTesting([&] {
                                              // Must deterministically run
                                              // after the sync task is
                                              // complete.
                                              EXPECT_TRUE(sync_task_done);
                                              run_loop.Quit();
                                            }));

                    // Wait to increase chances that the main thread will
                    // attempt to schedule its task.
                    PlatformThread::Sleep(TestTimeouts::tiny_timeout());
                    sync_task_done = true;
                  })));
              EXPECT_TRUE(sync_task_done);
            })));
      }));

  run_loop.Run();
  FlushOtherThread();
}

// Verify that when `RunOrPostTask` is called concurrently from multiple
// threads, only one can execute its task synchronously.
TEST_F(SequenceManagerRunOrPostTaskTest, ConcurrentCalls) {
  SimulateInsideRunLoop();

  WaitableEvent did_start_task_1;
  WaitableEvent did_post_task_2;

  EXPECT_TRUE(other_thread_task_runner()->PostTask(
      FROM_HERE, BindLambdaForTesting([&] {
        EXPECT_TRUE(task_runner()->RunOrPostTask(
            subtle::RunOrPostTaskPassKeyForTesting(), FROM_HERE,
            BindLambdaForTesting([&] {
              did_start_task_1.Signal();
              did_post_task_2.Wait();
            })));
      })));

  Thread other_thread_2{"OtherThread2"};
  other_thread_2.Start();

  bool did_complete_task_2 = false;
  EXPECT_TRUE(other_thread_2.task_runner()->PostTask(
      FROM_HERE, BindLambdaForTesting([&] {
        did_start_task_1.Wait();
        EXPECT_TRUE(task_runner()->RunOrPostTask(
            subtle::RunOrPostTaskPassKeyForTesting(), FROM_HERE,
            BindLambdaForTesting([&] { did_complete_task_2 = true; })));
        EXPECT_FALSE(did_complete_task_2);
        did_post_task_2.Signal();
      })));

  FlushOtherThread();
  EXPECT_FALSE(did_complete_task_2);
  RunLoop().RunUntilIdle();
  EXPECT_TRUE(did_complete_task_2);
}

// Verify the behavior of `SequenceCheckerImpl` and `ThreadCheckerImpl` in a
// callback that runs synchronously in `RunOrPostTask` on another thread.
TEST_F(SequenceManagerRunOrPostTaskTest, SequenceAndThreadChecker) {
  SimulateInsideRunLoop();

  SequenceCheckerImpl sequence_checker;
  ThreadCheckerImpl thread_checker;
  std::optional<SequenceCheckerImpl> sequence_checker_bound_in_task;
  std::optional<ThreadCheckerImpl> thread_checker_bound_in_task;

  EXPECT_TRUE(other_thread_task_runner()->PostTask(
      FROM_HERE, BindLambdaForTesting([&] {
        bool did_run = false;

        task_runner()->RunOrPostTask(
            subtle::RunOrPostTaskPassKeyForTesting(), FROM_HERE,
            BindLambdaForTesting([&] {
              EXPECT_TRUE(sequence_checker.CalledOnValidSequence());
              EXPECT_FALSE(thread_checker.CalledOnValidThread());
              sequence_checker_bound_in_task.emplace();
              thread_checker_bound_in_task.emplace();
              EXPECT_TRUE(
                  sequence_checker_bound_in_task->CalledOnValidSequence());
              EXPECT_TRUE(thread_checker_bound_in_task->CalledOnValidThread());
              did_run = true;
            }));
        EXPECT_TRUE(did_run);
        EXPECT_FALSE(sequence_checker_bound_in_task->CalledOnValidSequence());
        EXPECT_FALSE(thread_checker_bound_in_task->CalledOnValidThread());
      })));

  FlushOtherThread();
  EXPECT_TRUE(sequence_checker_bound_in_task->CalledOnValidSequence());
  EXPECT_FALSE(thread_checker_bound_in_task->CalledOnValidThread());
}

// Same as SequenceManagerRunOrPostTaskTest.SequenceAndThreadChecker, but
// `RunOrPostTask()` is invoked from a `ThreadPool` task (i.e. within a
// `TaskScope`).
TEST_F(SequenceManagerRunOrPostTaskTest,
       SequenceAndThreadCheckerFromThreadPool) {
  SimulateInsideRunLoop();

  SequenceCheckerImpl sequence_checker;
  ThreadCheckerImpl thread_checker;
  std::optional<SequenceCheckerImpl> sequence_checker_bound_in_task;
  std::optional<ThreadCheckerImpl> thread_checker_bound_in_task;

  ThreadPoolInstance::Create("TestPool");
  ThreadPoolInstance::Get()->Start({/* max_num_foreground_threads_in=*/1});

  EXPECT_TRUE(ThreadPool::PostTask(
      FROM_HERE, BindLambdaForTesting([&] {
        bool did_run = false;
        task_runner()->RunOrPostTask(
            subtle::RunOrPostTaskPassKeyForTesting(), FROM_HERE,
            BindLambdaForTesting([&] {
              EXPECT_TRUE(sequence_checker.CalledOnValidSequence());
              EXPECT_FALSE(thread_checker.CalledOnValidThread());
              sequence_checker_bound_in_task.emplace();
              thread_checker_bound_in_task.emplace();
              EXPECT_TRUE(
                  sequence_checker_bound_in_task->CalledOnValidSequence());
              EXPECT_TRUE(thread_checker_bound_in_task->CalledOnValidThread());
              did_run = true;
            }));
        EXPECT_TRUE(did_run);
        EXPECT_FALSE(sequence_checker_bound_in_task->CalledOnValidSequence());
        EXPECT_FALSE(thread_checker_bound_in_task->CalledOnValidThread());
      })));

  ThreadPoolInstance::Get()->FlushForTesting();
  ThreadPoolInstance::Get()->JoinForTesting();
  ThreadPoolInstance::Set(nullptr);

  EXPECT_TRUE(sequence_checker_bound_in_task->CalledOnValidSequence());
  EXPECT_FALSE(thread_checker_bound_in_task->CalledOnValidThread());
}

TEST_F(SequenceManagerRunOrPostTaskTest, CurrentDefaultTaskRunner) {
  SimulateInsideRunLoop();

  EXPECT_TRUE(task_runner()->RunsTasksInCurrentSequence());
  EXPECT_TRUE(task_runner()->BelongsToCurrentThread());
  EXPECT_TRUE(other_task_runner()->RunsTasksInCurrentSequence());
  EXPECT_TRUE(other_task_runner()->BelongsToCurrentThread());

  EXPECT_TRUE(other_thread_task_runner()->PostTask(
      FROM_HERE, BindLambdaForTesting([&] {
        EXPECT_TRUE(SingleThreadTaskRunner::HasCurrentDefault());
        EXPECT_EQ(other_thread_task_runner(),
                  SingleThreadTaskRunner::GetCurrentDefault());
        EXPECT_TRUE(SequencedTaskRunner::HasCurrentDefault());
        EXPECT_EQ(other_thread_task_runner(),
                  SequencedTaskRunner::GetCurrentDefault());

        EXPECT_FALSE(task_runner()->RunsTasksInCurrentSequence());
        EXPECT_FALSE(task_runner()->BelongsToCurrentThread());
        EXPECT_FALSE(other_task_runner()->RunsTasksInCurrentSequence());
        EXPECT_FALSE(other_task_runner()->BelongsToCurrentThread());

        for (auto* tested_task_runner : {task_runner(), other_task_runner()}) {
          bool did_run = false;

          // The "current default" `SequencedTaskRunner` is the
          // `SequenceManager`'s default task runner, irrespective of the task
          // runner on which `RunOrPostTask` is called.
          tested_task_runner->RunOrPostTask(
              subtle::RunOrPostTaskPassKeyForTesting(), FROM_HERE,
              BindLambdaForTesting([&] {
                did_run = true;
                EXPECT_FALSE(SingleThreadTaskRunner::HasCurrentDefault());
                EXPECT_TRUE(SequencedTaskRunner::HasCurrentDefault());
                EXPECT_EQ(task_runner(),
                          SequencedTaskRunner::GetCurrentDefault());

                EXPECT_TRUE(task_runner()->RunsTasksInCurrentSequence());
                EXPECT_FALSE(task_runner()->BelongsToCurrentThread());
                EXPECT_TRUE(other_task_runner()->RunsTasksInCurrentSequence());
                EXPECT_FALSE(other_task_runner()->BelongsToCurrentThread());
              }));
          EXPECT_TRUE(did_run);
        }

        EXPECT_TRUE(SingleThreadTaskRunner::HasCurrentDefault());
        EXPECT_EQ(other_thread_task_runner(),
                  SingleThreadTaskRunner::GetCurrentDefault());
        EXPECT_TRUE(SequencedTaskRunner::HasCurrentDefault());
        EXPECT_EQ(other_thread_task_runner(),
                  SequencedTaskRunner::GetCurrentDefault());
      })));

  FlushOtherThread();
}

TEST(
    SequenceManagerTest,
    CanAccessSingleThreadTaskRunnerCurrentDefaultHandleHandleDuringSequenceLocalStorageSlotDestruction) {
  auto sequence_manager =
      sequence_manager::CreateSequenceManagerOnCurrentThreadWithPump(
          MessagePump::Create(MessagePumpType::DEFAULT));
  auto queue = sequence_manager->CreateTaskQueue(
      sequence_manager::TaskQueue::Spec(QueueName::DEFAULT_TQ));
  sequence_manager->SetDefaultTaskRunner(queue->task_runner());

  scoped_refptr<SingleThreadTaskRunner> expected_task_runner =
      SingleThreadTaskRunner::GetCurrentDefault();

  StrictMock<MockCallback<base::OnceCallback<void()>>> cb;
  EXPECT_CALL(cb, Run).WillOnce(testing::Invoke([expected_task_runner] {
    EXPECT_EQ(SingleThreadTaskRunner::GetCurrentDefault(),
              expected_task_runner);
  }));

  static base::SequenceLocalStorageSlot<std::unique_ptr<DestructionCallback>>
      storage_slot;
  storage_slot.GetOrCreateValue() =
      std::make_unique<DestructionCallback>(cb.Get());

  queue.reset();
  sequence_manager.reset();
}

TEST(SequenceManagerTest, BindOnDifferentThreadWithActiveVoters) {
  auto sequence_manager = CreateUnboundSequenceManager();
  auto queue =
      sequence_manager->CreateTaskQueue(TaskQueue::Spec(QueueName::TEST_TQ));
  auto voter = queue->CreateQueueEnabledVoter();
  {
    // Create a second voter that gets destroyed while unbound.
    auto voter2 = queue->CreateQueueEnabledVoter();
  }

  voter->SetVoteToEnable(false);
  EXPECT_FALSE(queue->IsQueueEnabled());

  std::vector<bool> results;
  WaitableEvent done_event;
  Thread thread("TestThread");
  thread.Start();
  auto task = BindLambdaForTesting([&] {
    // Move `voter` so it gets destroyed on the bound thread.
    auto scoped_voter = std::move(voter);
    // Bind `sequence_manager` to this thread.
    auto scoped_mgr = std::move(sequence_manager);
    scoped_mgr->BindToCurrentThread();

    results.push_back(queue->IsQueueEnabled());
    scoped_voter->SetVoteToEnable(true);
    results.push_back(queue->IsQueueEnabled());
    done_event.Signal();
  });
  thread.task_runner()->PostTask(FROM_HERE, std::move(task));
  done_event.Wait();
  thread.Stop();
  EXPECT_THAT(results, ElementsAre(false, true));
}

}  // namespace internal
}  // namespace sequence_manager
}  // namespace base
