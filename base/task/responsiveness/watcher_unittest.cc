// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/responsiveness/watcher.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/pending_task.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/responsiveness/calculator.h"
#include "base/task/responsiveness/responsiveness_calculator_delegate.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace responsiveness {

namespace {

struct TaskTiming {
  base::TimeTicks queue_time;
  base::TimeTicks execution_start_time;
  base::TimeTicks execution_finish_time;
};

class FakeCalculator : public Calculator {
 public:
  using Calculator::Calculator;

  void TaskOrEventFinishedOnMainThread(
      base::TimeTicks queue_time,
      base::TimeTicks execution_start_time,
      base::TimeTicks execution_finish_time) override {
    execution_times_main_.push_back(
        {queue_time, execution_start_time, execution_finish_time});
  }

  void OnFirstIdle() override {
    first_idle_called_ = true;
    Calculator::OnFirstIdle();
  }

  bool FirstIdleCalled() const { return first_idle_called_; }

  int NumTasksOnMainThread() const {
    return static_cast<int>(execution_times_main_.size());
  }
  const std::vector<TaskTiming>& ExecutionTimesMainThread() const {
    return execution_times_main_;
  }

 private:
  bool first_idle_called_ = false;
  std::vector<TaskTiming> execution_times_main_;
};

class FakeWatcher : public Watcher {
 public:
  std::unique_ptr<Calculator> CreateCalculator() override {
    auto calculator = std::make_unique<FakeCalculator>(
        nullptr, "Browser",
        Calculator::Config{.consume_secondary_sources =
                               consume_secondary_sources()});
    fake_calculator_ = calculator.get();
    return calculator;
  }

  bool ShouldRegisterMessageLoopObserver() const override {
    return register_message_loop_observer_;
  }

  explicit FakeWatcher(bool register_message_loop_observer,
                       std::string_view histogram_prefix = "Browser",
                       Config config = {})
      : Watcher(histogram_prefix, std::move(config)),
        register_message_loop_observer_(register_message_loop_observer) {}
  ~FakeWatcher() override {
    // Clear `fake_calculator_` before `~Watcher()` destroys `calculator_`.
    fake_calculator_ = nullptr;
  }

  bool FirstIdleCalled() const { return fake_calculator_->FirstIdleCalled(); }
  int NumTasksOnMainThread() const {
    return fake_calculator_->NumTasksOnMainThread();
  }
  const std::vector<TaskTiming>& ExecutionTimesMainThread() const {
    return fake_calculator_->ExecutionTimesMainThread();
  }

 private:
  raw_ptr<FakeCalculator> fake_calculator_ = nullptr;
  bool register_message_loop_observer_ = false;
};

void RunTaskForDuration(
    base::Thread& thread,
    base::test::SingleThreadTaskEnvironment& task_environment,
    base::TimeDelta duration) {
  base::RunLoop run_loop;
  WaitableEvent task_started;
  WaitableEvent clock_advanced;
  thread.task_runner()->PostTaskAndReply(FROM_HERE, BindLambdaForTesting([&]() {
                                           task_started.Signal();
                                           clock_advanced.Wait();
                                         }),
                                         run_loop.QuitClosure());
  task_started.Wait();
  task_environment.AdvanceClock(duration);
  clock_advanced.Signal();
  run_loop.Run();
  // `PostTaskAndReply` posts its reply before `TaskObserver::DidProcessTask`
  // runs on `thread`; flush `thread` to ensure `DidRunTask` has completed.
  thread.FlushForTesting();
}

}  // namespace

class ResponsivenessWatcherTest : public testing::Test {
 public:
  void SetUp() override {
    watcher_ = std::make_unique<FakeWatcher>(
        /*register_message_loop_observer=*/false);
    watcher_->SetUp();
  }

  void TearDown() override { watcher_.reset(); }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  std::unique_ptr<FakeWatcher> watcher_;
};

// Test that tasks are forwarded to calculator.
TEST_F(ResponsivenessWatcherTest, TaskForwarding) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(watcher_->main_sequence_checker_);
  for (int i = 0; i < 3; ++i) {
    base::PendingTask task(FROM_HERE, base::OnceClosure(),
                           /*queue_time=*/base::TimeTicks::Now(),
                           /*delayed_run_time=*/base::TimeTicks());
    watcher_->main_thread_observer_->WillRunTask(
        &task, /*was_blocked_or_low_priority=*/false);
    watcher_->main_thread_observer_->DidRunTask(&task);
  }
  EXPECT_EQ(3, watcher_->NumTasksOnMainThread());
}

// Test that nested tasks are not forwarded to the calculator.
TEST_F(ResponsivenessWatcherTest, TaskNesting) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(watcher_->main_sequence_checker_);
  base::PendingTask task1(FROM_HERE, base::OnceClosure(),
                          /*queue_time=*/base::TimeTicks::Now(),
                          /*delayed_run_time=*/base::TimeTicks());
  base::PendingTask task2(FROM_HERE, base::OnceClosure(),
                          /*queue_time=*/base::TimeTicks::Now(),
                          /*delayed_run_time=*/base::TimeTicks());
  task_environment_.FastForwardBy(base::Milliseconds(1));
  base::PendingTask task3(FROM_HERE, base::OnceClosure(),
                          /*queue_time=*/base::TimeTicks::Now(),
                          /*delayed_run_time=*/base::TimeTicks());

  const base::TimeTicks task_1_execution_start_time = base::TimeTicks::Now();
  watcher_->main_thread_observer_->WillRunTask(
      &task1, /*was_blocked_or_low_priority=*/false);
  watcher_->main_thread_observer_->WillRunTask(
      &task2, /*was_blocked_or_low_priority=*/false);

  task_environment_.FastForwardBy(base::Milliseconds(1));
  const base::TimeTicks task_3_execution_start_time = base::TimeTicks::Now();
  EXPECT_EQ(task_1_execution_start_time + base::Milliseconds(1),
            task_3_execution_start_time);
  watcher_->main_thread_observer_->WillRunTask(
      &task3, /*was_blocked_or_low_priority=*/false);

  task_environment_.FastForwardBy(base::Milliseconds(1));
  const base::TimeTicks task_3_execution_finish_time = base::TimeTicks::Now();
  watcher_->main_thread_observer_->DidRunTask(&task3);
  task_environment_.FastForwardBy(base::Milliseconds(1));
  watcher_->main_thread_observer_->DidRunTask(&task2);
  watcher_->main_thread_observer_->DidRunTask(&task1);

  ASSERT_EQ(1, watcher_->NumTasksOnMainThread());

  // The innermost task should be the one that is passed through, as it didn't
  // cause reentrancy.
  const TaskTiming task_timing = watcher_->ExecutionTimesMainThread()[0];
  EXPECT_EQ(task3.queue_time, task_timing.queue_time);
  EXPECT_EQ(task_3_execution_start_time, task_timing.execution_start_time);
  EXPECT_EQ(task_3_execution_finish_time, task_timing.execution_finish_time);
}

// Test that native events use execution time instead of queue + execution time.
TEST_F(ResponsivenessWatcherTest, NativeEvents) {
  const base::TimeTicks start_time = base::TimeTicks::Now();

  uintptr_t opaque_identifier = 0x1234;
  watcher_->WillRunEventOnMainThread(opaque_identifier);

  task_environment_.FastForwardBy(base::Milliseconds(1));
  const base::TimeTicks finish_time = base::TimeTicks::Now();
  watcher_->DidRunEventOnMainThread(opaque_identifier);

  ASSERT_EQ(1, watcher_->NumTasksOnMainThread());
  const TaskTiming task_timing = watcher_->ExecutionTimesMainThread()[0];
  EXPECT_EQ(task_timing.queue_time, start_time);
  EXPECT_EQ(task_timing.execution_start_time, start_time);
  EXPECT_EQ(task_timing.execution_finish_time, finish_time);
}

// Test that the queue duration of a blocked or low priority task is zero.
TEST_F(ResponsivenessWatcherTest, BlockedOrLowPriorityTask) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(watcher_->main_sequence_checker_);
  base::PendingTask task(FROM_HERE, base::OnceClosure(),
                         /*queue_time=*/base::TimeTicks::Now(),
                         /*delayed_run_time=*/base::TimeTicks());
  task_environment_.FastForwardBy(base::Seconds(1));

  const base::TimeTicks execution_start_time = base::TimeTicks::Now();
  watcher_->main_thread_observer_->WillRunTask(
      &task, /*was_blocked_or_low_priority=*/true);
  task_environment_.FastForwardBy(base::Milliseconds(1));
  const base::TimeTicks execution_finish_time = base::TimeTicks::Now();
  watcher_->main_thread_observer_->DidRunTask(&task);

  ASSERT_EQ(1, watcher_->NumTasksOnMainThread());
  const TaskTiming task_timing = watcher_->ExecutionTimesMainThread()[0];
  // The queue time should be equal to the execution start time, to simulate
  // that the task did not spend any time in a queue. This is the desired
  // behavior for a task that was blocked or low priority.
  EXPECT_EQ(task_timing.queue_time, execution_start_time);
  EXPECT_EQ(task_timing.execution_start_time, execution_start_time);
  EXPECT_EQ(task_timing.execution_finish_time, execution_finish_time);
}

// Test that the queue duration of a delayed task is zero.
TEST_F(ResponsivenessWatcherTest, DelayedTask) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(watcher_->main_sequence_checker_);
  base::PendingTask task(FROM_HERE, base::OnceClosure(),
                         /*queue_time=*/base::TimeTicks::Now(),
                         /*delayed_run_time=*/base::TimeTicks::Now());
  task_environment_.FastForwardBy(base::Seconds(1));

  const base::TimeTicks execution_start_time = base::TimeTicks::Now();
  watcher_->main_thread_observer_->WillRunTask(
      &task, /*was_blocked_or_low_priority=*/false);
  task_environment_.FastForwardBy(base::Milliseconds(1));
  const base::TimeTicks execution_finish_time = base::TimeTicks::Now();
  watcher_->main_thread_observer_->DidRunTask(&task);

  ASSERT_EQ(1, watcher_->NumTasksOnMainThread());
  const TaskTiming task_timing = watcher_->ExecutionTimesMainThread()[0];
  // The queue time should be equal to the execution start time, to simulate
  // that the task did not spend any time in a queue. This is the desired
  // behavior for a delayed task.
  EXPECT_EQ(task_timing.queue_time, execution_start_time);
  EXPECT_EQ(task_timing.execution_start_time, execution_start_time);
  EXPECT_EQ(task_timing.execution_finish_time, execution_finish_time);
}

class WatchedThread : public base::Thread {
 public:
  explicit WatchedThread(std::string_view source_name = "")
      : base::Thread("WatchedThread"), source_name_(source_name) {}
  ~WatchedThread() override { Stop(); }

 private:
  void Init() override {
    registration_ = Watcher::RegisterCurrentThread(source_name_);
  }
  void CleanUp() override { registration_.RunAndReset(); }

  const std::string source_name_;
  ScopedClosureRunner registration_;
};

class ResponsivenessWatcherRealIOThreadTest : public testing::Test {
 public:
  void SetUp() override {
    // Start and register the IO thread *before* creating `watcher_`, matching
    // Browser startup (`BrowserProcessIOThread` starts before
    // `BrowserMainLoop::PreMainMessageLoopRun()`).
    ASSERT_TRUE(io_thread_.StartAndWaitForTesting());

    // Congested tasks running on the IO thread before `watcher_` attaches
    // should be ignored.
    RunTaskForDuration(io_thread_, task_environment_, Milliseconds(250));

    watcher_ = std::make_unique<FakeWatcher>(
        /*register_message_loop_observer=*/true, "Browser",
        Watcher::Config{.consume_secondary_sources = true});
    watcher_->SetUp();
  }

  void TearDown() override {
    io_thread_.Stop();
    watcher_.reset();
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  SecondarySources secondary_sources_;
  WatchedThread io_thread_;

  std::unique_ptr<FakeWatcher> watcher_;
};

TEST_F(ResponsivenessWatcherRealIOThreadTest, MessageLoopObserver) {
  RunTaskForDuration(io_thread_, task_environment_, Milliseconds(250));

  ASSERT_GE(watcher_->NumTasksOnMainThread(), 1);
  const TaskTiming main_task_timing = watcher_->ExecutionTimesMainThread()[0];
  EXPECT_FALSE(main_task_timing.queue_time.is_null());
  EXPECT_FALSE(main_task_timing.execution_start_time.is_null());
  EXPECT_FALSE(main_task_timing.execution_finish_time.is_null());

  std::vector<Calculator::SourceCongestions> sources;
  secondary_sources_.TakeCongestionsOlderThanTime(sources,
                                                  base::TimeTicks::Max());
  ASSERT_EQ(sources.size(), 1u);
  EXPECT_EQ(sources[0].name, "");
  ASSERT_EQ(sources[0].congestion.size(), 1u);
  EXPECT_EQ(
      sources[0].congestion[0].end_time - sources[0].congestion[0].start_time,
      Milliseconds(250));
  ASSERT_EQ(sources[0].execution_congestion.size(), 1u);
}

TEST(ResponsivenessWatcherSingleThreadTest, SingleThreadWatcher) {
  base::test::SingleThreadTaskEnvironment task_environment;
  FakeWatcher single_thread_watcher(/*register_message_loop_observer=*/true,
                                    "NetworkService");
  single_thread_watcher.SetUp();
  EXPECT_FALSE(single_thread_watcher.FirstIdleCalled());

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::DoNothing());
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return single_thread_watcher.FirstIdleCalled(); }));

  ASSERT_GE(single_thread_watcher.NumTasksOnMainThread(), 1);
  const TaskTiming main_task_timing =
      single_thread_watcher.ExecutionTimesMainThread()[0];
  EXPECT_FALSE(main_task_timing.queue_time.is_null());
  EXPECT_FALSE(main_task_timing.execution_start_time.is_null());
  EXPECT_FALSE(main_task_timing.execution_finish_time.is_null());
}

TEST(ResponsivenessWatcherSingleThreadTest, RegisterCurrentThread) {
  base::test::SingleThreadTaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  auto secondary_sources = std::make_unique<SecondarySources>();

  auto watcher = std::make_unique<Watcher>(
      "Renderer", Watcher::Config{.main_source_name = "Main",
                                  .consume_secondary_sources = true});
  watcher->SetUp();

  WatchedThread secondary_thread("Compositor");
  ASSERT_TRUE(secondary_thread.StartAndWaitForTesting());

  RunTaskForDuration(secondary_thread, task_environment, Milliseconds(250));

  std::vector<Calculator::SourceCongestions> sources;
  secondary_sources->TakeCongestionsOlderThanTime(sources,
                                                  base::TimeTicks::Max());
  ASSERT_EQ(sources.size(), 1u);
  EXPECT_EQ(sources[0].name, "Compositor");
  ASSERT_EQ(sources[0].congestion.size(), 1u);
  ASSERT_EQ(sources[0].execution_congestion.size(), 1u);

  // Also verify `Watcher` can be destroyed *before* the secondary thread
  // unregisters without dangling pointers, and tasks after detach are ignored.
  watcher.reset();
  RunTaskForDuration(secondary_thread, task_environment, Milliseconds(250));
  sources.clear();
  secondary_sources->TakeCongestionsOlderThanTime(sources,
                                                  base::TimeTicks::Max());
  ASSERT_EQ(sources.size(), 1u);
  EXPECT_TRUE(sources[0].congestion.empty());

  secondary_thread.Stop();
}

}  // namespace responsiveness
}  // namespace base
