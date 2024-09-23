// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/pre_freeze_background_memory_trimmer.h"

#include <optional>

#include "base/task/thread_pool.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base::android {

namespace {

static int s_counter = 0;

void ResetGlobalCounter() {
  s_counter = 0;
}

void IncGlobalCounter() {
  s_counter++;
}

void DecGlobalCounter() {
  s_counter--;
}

void PostDelayedIncGlobal() {
  PreFreezeBackgroundMemoryTrimmer::PostDelayedBackgroundTask(
      SingleThreadTaskRunner::GetCurrentDefault(), FROM_HERE,
      base::BindRepeating(&IncGlobalCounter), base::Seconds(10));
}

class MockMetric : public PreFreezeBackgroundMemoryTrimmer::PreFreezeMetric {
 public:
  MockMetric() : PreFreezeBackgroundMemoryTrimmer::PreFreezeMetric("Mock") {
    count_++;
  }
  std::optional<uint64_t> Measure() const override { return 0; }
  static size_t count_;

  ~MockMetric() override { count_--; }
};

size_t MockMetric::count_ = 0;

}  // namespace

class PreFreezeBackgroundMemoryTrimmerTest : public testing::Test {
 public:
  PreFreezeBackgroundMemoryTrimmerTest() {
    fl_.InitAndEnableFeature(kOnPreFreezeMemoryTrim);
  }
  void SetUp() override {
    PreFreezeBackgroundMemoryTrimmer::SetSupportsModernTrimForTesting(true);
    PreFreezeBackgroundMemoryTrimmer::ClearMetricsForTesting();
    ResetGlobalCounter();
  }

 protected:
  size_t pending_task_count() {
    return PreFreezeBackgroundMemoryTrimmer::Instance()
        .GetNumberOfPendingBackgroundTasksForTesting();
  }

  bool did_register_tasks() {
    return PreFreezeBackgroundMemoryTrimmer::Instance()
        .DidRegisterTasksForTesting();
  }

  size_t measurements_count() {
    return PreFreezeBackgroundMemoryTrimmer::Instance()
        .GetNumberOfKnownMetricsForTesting();
  }

  size_t values_before_count() {
    return PreFreezeBackgroundMemoryTrimmer::Instance()
        .GetNumberOfValuesBeforeForTesting();
  }

  test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
 private:
  test::ScopedFeatureList fl_;
};

// We do not expect any tasks to be registered with
// PreFreezeBackgroundMemoryTrimmer on Android versions before U.
TEST_F(PreFreezeBackgroundMemoryTrimmerTest, PostTaskPreFreezeUnsupported) {
  PreFreezeBackgroundMemoryTrimmer::SetSupportsModernTrimForTesting(false);

  ASSERT_FALSE(did_register_tasks());

  PreFreezeBackgroundMemoryTrimmer::PostDelayedBackgroundTask(
      SingleThreadTaskRunner::GetCurrentDefault(), FROM_HERE,
      base::BindRepeating(&IncGlobalCounter), base::Seconds(30));

  ASSERT_EQ(pending_task_count(), 0u);
  ASSERT_FALSE(did_register_tasks());

  task_environment_.FastForwardBy(base::Seconds(30));

  ASSERT_EQ(pending_task_count(), 0u);
  EXPECT_EQ(s_counter, 1);
}

TEST_F(PreFreezeBackgroundMemoryTrimmerTest, PostTaskPreFreezeWithoutTrim) {
  test::ScopedFeatureList fl;
  fl.InitAndDisableFeature(kOnPreFreezeMemoryTrim);
  ASSERT_FALSE(did_register_tasks());

  PreFreezeBackgroundMemoryTrimmer::PostDelayedBackgroundTask(
      SingleThreadTaskRunner::GetCurrentDefault(), FROM_HERE,
      base::BindRepeating(&IncGlobalCounter), base::Seconds(30));

  ASSERT_EQ(pending_task_count(), 0u);
  ASSERT_TRUE(did_register_tasks());

  task_environment_.FastForwardBy(base::Seconds(30));

  ASSERT_EQ(pending_task_count(), 0u);
  EXPECT_EQ(s_counter, 1);
}

// TODO(thiabaud): Test that the histograms are recorded too.
TEST_F(PreFreezeBackgroundMemoryTrimmerTest, RegisterMetric) {
  ASSERT_EQ(measurements_count(), 0u);
  ASSERT_EQ(MockMetric::count_, 0u);

  {
    MockMetric mock_metric;

    PreFreezeBackgroundMemoryTrimmer::RegisterMemoryMetric(&mock_metric);

    EXPECT_EQ(MockMetric::count_, 1u);
    EXPECT_EQ(measurements_count(), 1u);

    PreFreezeBackgroundMemoryTrimmer::UnregisterMemoryMetric(&mock_metric);

    // Unregistering does not destroy the metric.
    EXPECT_EQ(MockMetric::count_, 1u);
    EXPECT_EQ(measurements_count(), 0u);
  }

  EXPECT_EQ(MockMetric::count_, 0u);
  EXPECT_EQ(measurements_count(), 0u);
}

TEST_F(PreFreezeBackgroundMemoryTrimmerTest, UnregisterDuringPreFreeze) {
  ASSERT_EQ(measurements_count(), 0u);
  ASSERT_EQ(MockMetric::count_, 0u);

  {
    MockMetric mock_metric;

    PreFreezeBackgroundMemoryTrimmer::RegisterMemoryMetric(&mock_metric);

    EXPECT_EQ(MockMetric::count_, 1u);
    EXPECT_EQ(measurements_count(), 1u);

    // This posts a metrics task.
    PreFreezeBackgroundMemoryTrimmer::OnPreFreezeForTesting();

    EXPECT_EQ(measurements_count(), 1u);
    EXPECT_EQ(values_before_count(), 1u);

    PreFreezeBackgroundMemoryTrimmer::UnregisterMemoryMetric(&mock_metric);

    // Unregistering does not destroy the metric, but does remove its value
    // from |before_values_|.
    EXPECT_EQ(MockMetric::count_, 1u);
    EXPECT_EQ(measurements_count(), 0u);
    EXPECT_EQ(values_before_count(), 0u);
  }

  EXPECT_EQ(MockMetric::count_, 0u);
  EXPECT_EQ(measurements_count(), 0u);
  EXPECT_EQ(values_before_count(), 0u);
}

TEST_F(PreFreezeBackgroundMemoryTrimmerTest, PostDelayedTaskSimple) {
  PreFreezeBackgroundMemoryTrimmer::PostDelayedBackgroundTask(
      SingleThreadTaskRunner::GetCurrentDefault(), FROM_HERE,
      base::BindRepeating(&IncGlobalCounter), base::Seconds(30));

  ASSERT_TRUE(did_register_tasks());
  ASSERT_EQ(pending_task_count(), 1u);

  task_environment_.FastForwardBy(base::Seconds(30));

  ASSERT_EQ(pending_task_count(), 0u);

  EXPECT_EQ(s_counter, 1);
}

TEST_F(PreFreezeBackgroundMemoryTrimmerTest, PostDelayedTaskMultiple) {
  PreFreezeBackgroundMemoryTrimmer::PostDelayedBackgroundTask(
      SingleThreadTaskRunner::GetCurrentDefault(), FROM_HERE,
      base::BindRepeating(&IncGlobalCounter), base::Seconds(40));

  PreFreezeBackgroundMemoryTrimmer::PostDelayedBackgroundTask(
      SingleThreadTaskRunner::GetCurrentDefault(), FROM_HERE,
      base::BindRepeating(&IncGlobalCounter), base::Seconds(30));

  ASSERT_EQ(pending_task_count(), 2u);

  task_environment_.FastForwardBy(base::Seconds(30));

  ASSERT_EQ(pending_task_count(), 1u);

  EXPECT_EQ(s_counter, 1);

  task_environment_.FastForwardBy(base::Seconds(10));

  ASSERT_EQ(pending_task_count(), 0u);

  EXPECT_EQ(s_counter, 2);
}

TEST_F(PreFreezeBackgroundMemoryTrimmerTest, PostDelayedTaskPreFreeze) {
  PreFreezeBackgroundMemoryTrimmer::PostDelayedBackgroundTask(
      SingleThreadTaskRunner::GetCurrentDefault(), FROM_HERE,
      base::BindRepeating(&IncGlobalCounter), base::Seconds(60));

  ASSERT_EQ(pending_task_count(), 1u);

  task_environment_.FastForwardBy(base::Seconds(30));

  PreFreezeBackgroundMemoryTrimmer::OnPreFreezeForTesting();

  ASSERT_EQ(pending_task_count(), 0u);

  EXPECT_EQ(s_counter, 1);
}

TEST_F(PreFreezeBackgroundMemoryTrimmerTest, PostDelayedTaskMultiThreaded) {
  base::WaitableEvent event1(base::WaitableEvent::ResetPolicy::MANUAL,
                             base::WaitableEvent::InitialState::NOT_SIGNALED);
  base::WaitableEvent event2(base::WaitableEvent::ResetPolicy::MANUAL,
                             base::WaitableEvent::InitialState::NOT_SIGNALED);
  auto task_runner =
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()});
  ASSERT_FALSE(task_runner->RunsTasksInCurrentSequence());

  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](scoped_refptr<base::SequencedTaskRunner> task_runner,
             base::WaitableEvent* event1, base::WaitableEvent* event2) {
            PreFreezeBackgroundMemoryTrimmer::PostDelayedBackgroundTask(
                task_runner, FROM_HERE,
                base::BindOnce(
                    [](base::WaitableEvent* event) {
                      IncGlobalCounter();
                      event->Signal();
                    },
                    base::Unretained(event2)),
                base::Seconds(30));
            event1->Signal();
          },
          task_runner, base::Unretained(&event1), base::Unretained(&event2)));

  task_environment_.FastForwardBy(base::Seconds(1));

  event1.Wait();

  ASSERT_EQ(pending_task_count(), 1u);

  PreFreezeBackgroundMemoryTrimmer::OnPreFreezeForTesting();

  event2.Wait();

  ASSERT_EQ(pending_task_count(), 0u);

  EXPECT_EQ(s_counter, 1);
}

TEST_F(PreFreezeBackgroundMemoryTrimmerTest,
       PostDelayedTaskBeforeAndAfterPreFreeze) {
  PreFreezeBackgroundMemoryTrimmer::PostDelayedBackgroundTask(
      SingleThreadTaskRunner::GetCurrentDefault(), FROM_HERE,
      base::BindRepeating(&IncGlobalCounter), base::Seconds(60));

  PreFreezeBackgroundMemoryTrimmer::PostDelayedBackgroundTask(
      SingleThreadTaskRunner::GetCurrentDefault(), FROM_HERE,
      base::BindRepeating(&IncGlobalCounter), base::Seconds(30));

  ASSERT_EQ(pending_task_count(), 2u);

  task_environment_.FastForwardBy(base::Seconds(30));

  ASSERT_EQ(pending_task_count(), 1u);

  EXPECT_EQ(s_counter, 1);

  PreFreezeBackgroundMemoryTrimmer::OnPreFreezeForTesting();

  ASSERT_EQ(pending_task_count(), 0u);

  EXPECT_EQ(s_counter, 2);
}

TEST_F(PreFreezeBackgroundMemoryTrimmerTest, AddDuringPreFreeze) {
  PreFreezeBackgroundMemoryTrimmer::PostDelayedBackgroundTask(
      SingleThreadTaskRunner::GetCurrentDefault(), FROM_HERE,
      base::BindRepeating(&PostDelayedIncGlobal), base::Seconds(10));

  ASSERT_EQ(pending_task_count(), 1u);

  PreFreezeBackgroundMemoryTrimmer::OnPreFreezeForTesting();

  ASSERT_EQ(pending_task_count(), 1u);
  EXPECT_EQ(s_counter, 0);

  // Fast forward to run the metrics task.
  task_environment_.FastForwardBy(base::Seconds(2));

  PreFreezeBackgroundMemoryTrimmer::OnPreFreezeForTesting();

  ASSERT_EQ(pending_task_count(), 0u);

  EXPECT_EQ(s_counter, 1);
}

TEST_F(PreFreezeBackgroundMemoryTrimmerTest, AddDuringPreFreezeRunNormally) {
  PreFreezeBackgroundMemoryTrimmer::PostDelayedBackgroundTask(
      SingleThreadTaskRunner::GetCurrentDefault(), FROM_HERE,
      base::BindRepeating(&PostDelayedIncGlobal), base::Seconds(10));

  ASSERT_EQ(pending_task_count(), 1u);

  PreFreezeBackgroundMemoryTrimmer::OnPreFreezeForTesting();

  ASSERT_EQ(pending_task_count(), 1u);
  EXPECT_EQ(s_counter, 0);

  task_environment_.FastForwardBy(base::Seconds(30));

  ASSERT_EQ(pending_task_count(), 0u);

  EXPECT_EQ(s_counter, 1);
}

TEST_F(PreFreezeBackgroundMemoryTrimmerTest, TimerNeverStarted) {
  OneShotDelayedBackgroundTimer timer;

  ASSERT_EQ(pending_task_count(), 0u);
  ASSERT_FALSE(timer.IsRunning());

  task_environment_.FastForwardBy(base::Seconds(30));

  ASSERT_EQ(pending_task_count(), 0u);
  ASSERT_FALSE(timer.IsRunning());

  ASSERT_FALSE(did_register_tasks());
  EXPECT_EQ(s_counter, 0);
}

TEST_F(PreFreezeBackgroundMemoryTrimmerTest, TimerFastForward) {
  OneShotDelayedBackgroundTimer timer;

  ASSERT_EQ(pending_task_count(), 0u);
  ASSERT_FALSE(timer.IsRunning());
  ASSERT_FALSE(did_register_tasks());

  timer.Start(FROM_HERE, base::Seconds(30), base::BindOnce(&IncGlobalCounter));

  ASSERT_EQ(pending_task_count(), 1u);
  ASSERT_TRUE(timer.IsRunning());
  ASSERT_TRUE(did_register_tasks());

  task_environment_.FastForwardBy(base::Seconds(30));

  ASSERT_EQ(pending_task_count(), 0u);
  ASSERT_FALSE(timer.IsRunning());

  EXPECT_EQ(s_counter, 1);
}

TEST_F(PreFreezeBackgroundMemoryTrimmerTest, TimerOnPreFreeze) {
  OneShotDelayedBackgroundTimer timer;

  ASSERT_EQ(pending_task_count(), 0u);
  ASSERT_FALSE(timer.IsRunning());
  ASSERT_FALSE(did_register_tasks());

  timer.Start(FROM_HERE, base::Seconds(30), base::BindOnce(&IncGlobalCounter));

  ASSERT_EQ(pending_task_count(), 1u);
  ASSERT_TRUE(timer.IsRunning());
  ASSERT_TRUE(did_register_tasks());

  PreFreezeBackgroundMemoryTrimmer::OnPreFreezeForTesting();

  ASSERT_EQ(pending_task_count(), 0u);
  ASSERT_FALSE(timer.IsRunning());

  EXPECT_EQ(s_counter, 1);
}

TEST_F(PreFreezeBackgroundMemoryTrimmerTest, TimerStopSingle) {
  OneShotDelayedBackgroundTimer timer;

  ASSERT_EQ(pending_task_count(), 0u);
  ASSERT_FALSE(timer.IsRunning());
  ASSERT_FALSE(did_register_tasks());

  timer.Start(FROM_HERE, base::Seconds(30), base::BindOnce(&IncGlobalCounter));

  ASSERT_EQ(pending_task_count(), 1u);
  ASSERT_TRUE(timer.IsRunning());
  ASSERT_TRUE(did_register_tasks());

  timer.Stop();
  PreFreezeBackgroundMemoryTrimmer::OnPreFreezeForTesting();

  ASSERT_EQ(pending_task_count(), 0u);
  ASSERT_FALSE(timer.IsRunning());

  EXPECT_EQ(s_counter, 0);
}

TEST_F(PreFreezeBackgroundMemoryTrimmerTest, TimerStopMultiple) {
  OneShotDelayedBackgroundTimer timer;

  ASSERT_EQ(pending_task_count(), 0u);
  ASSERT_FALSE(timer.IsRunning());
  ASSERT_FALSE(did_register_tasks());

  timer.Start(FROM_HERE, base::Seconds(30), base::BindOnce(&IncGlobalCounter));

  ASSERT_EQ(pending_task_count(), 1u);
  ASSERT_TRUE(timer.IsRunning());
  ASSERT_TRUE(did_register_tasks());

  timer.Stop();
  timer.Stop();

  PreFreezeBackgroundMemoryTrimmer::OnPreFreezeForTesting();

  ASSERT_EQ(pending_task_count(), 0u);
  ASSERT_FALSE(timer.IsRunning());

  EXPECT_EQ(s_counter, 0);
}

TEST_F(PreFreezeBackgroundMemoryTrimmerTest, TimerDestroyed) {
  // Add scope here to destroy timer.
  {
    OneShotDelayedBackgroundTimer timer;

    ASSERT_EQ(pending_task_count(), 0u);
    ASSERT_FALSE(timer.IsRunning());
    ASSERT_FALSE(did_register_tasks());

    timer.Start(FROM_HERE, base::Seconds(30),
                base::BindOnce(&IncGlobalCounter));

    ASSERT_EQ(pending_task_count(), 1u);
    ASSERT_TRUE(timer.IsRunning());
    ASSERT_TRUE(did_register_tasks());
  }

  ASSERT_EQ(pending_task_count(), 0u);

  PreFreezeBackgroundMemoryTrimmer::OnPreFreezeForTesting();

  ASSERT_EQ(pending_task_count(), 0u);

  EXPECT_EQ(s_counter, 0);
}

TEST_F(PreFreezeBackgroundMemoryTrimmerTest, TimerStartedWhileRunning) {
  IncGlobalCounter();
  ASSERT_EQ(s_counter, 1);

  OneShotDelayedBackgroundTimer timer;

  ASSERT_EQ(pending_task_count(), 0u);
  ASSERT_FALSE(timer.IsRunning());
  ASSERT_FALSE(did_register_tasks());

  timer.Start(FROM_HERE, base::Seconds(30), base::BindOnce(&IncGlobalCounter));

  ASSERT_EQ(pending_task_count(), 1u);
  ASSERT_TRUE(timer.IsRunning());
  ASSERT_TRUE(did_register_tasks());

  timer.Start(FROM_HERE, base::Seconds(10), base::BindOnce(&DecGlobalCounter));

  // Previous task was cancelled, so s_counter should still be 1.
  ASSERT_EQ(s_counter, 1);
  ASSERT_EQ(pending_task_count(), 1u);
  ASSERT_TRUE(timer.IsRunning());
  ASSERT_TRUE(did_register_tasks());

  PreFreezeBackgroundMemoryTrimmer::OnPreFreezeForTesting();

  ASSERT_EQ(pending_task_count(), 0u);
  ASSERT_FALSE(timer.IsRunning());
  ASSERT_TRUE(did_register_tasks());

  // Expect 0 here because we decremented it. The incrementing task was
  // cancelled when we restarted the experiment.
  EXPECT_EQ(s_counter, 0);
}

TEST_F(PreFreezeBackgroundMemoryTrimmerTest, BoolTaskRunDirectly) {
  std::optional<MemoryReductionTaskContext> called_task_type = std::nullopt;
  PreFreezeBackgroundMemoryTrimmer::PostDelayedBackgroundTask(
      SingleThreadTaskRunner::GetCurrentDefault(), FROM_HERE,
      base::BindOnce(
          [](std::optional<MemoryReductionTaskContext>& called_task_type,
             MemoryReductionTaskContext task_type) {
            called_task_type = task_type;
          },
          std::ref(called_task_type)),
      base::Seconds(30));

  ASSERT_FALSE(called_task_type.has_value());
  ASSERT_EQ(pending_task_count(), 1u);

  task_environment_.FastForwardBy(base::Seconds(30));

  ASSERT_EQ(pending_task_count(), 0u);
  EXPECT_EQ(called_task_type.value(),
            MemoryReductionTaskContext::kDelayExpired);
}

TEST_F(PreFreezeBackgroundMemoryTrimmerTest, BoolTaskRunFromPreFreeze) {
  std::optional<MemoryReductionTaskContext> called_task_type = std::nullopt;
  PreFreezeBackgroundMemoryTrimmer::PostDelayedBackgroundTask(
      SingleThreadTaskRunner::GetCurrentDefault(), FROM_HERE,
      base::BindOnce(
          [](std::optional<MemoryReductionTaskContext>& called_task_type,
             MemoryReductionTaskContext task_type) {
            called_task_type = task_type;
          },
          std::ref(called_task_type)),
      base::Seconds(30));

  ASSERT_FALSE(called_task_type.has_value());
  ASSERT_EQ(pending_task_count(), 1u);

  PreFreezeBackgroundMemoryTrimmer::OnPreFreezeForTesting();

  ASSERT_EQ(pending_task_count(), 0u);
  EXPECT_EQ(called_task_type.value(), MemoryReductionTaskContext::kProactive);
}

TEST_F(PreFreezeBackgroundMemoryTrimmerTest, TimerBoolTaskRunDirectly) {
  OneShotDelayedBackgroundTimer timer;
  std::optional<MemoryReductionTaskContext> called_task_type = std::nullopt;

  ASSERT_EQ(pending_task_count(), 0u);
  ASSERT_FALSE(timer.IsRunning());

  timer.Start(
      FROM_HERE, base::Seconds(30),
      base::BindOnce(
          [](std::optional<MemoryReductionTaskContext>& called_task_type,
             MemoryReductionTaskContext task_type) {
            called_task_type = task_type;
          },
          std::ref(called_task_type)));

  ASSERT_FALSE(called_task_type.has_value());
  ASSERT_EQ(pending_task_count(), 1u);

  task_environment_.FastForwardBy(base::Seconds(30));

  ASSERT_EQ(pending_task_count(), 0u);
  EXPECT_EQ(called_task_type.value(),
            MemoryReductionTaskContext::kDelayExpired);
}

TEST_F(PreFreezeBackgroundMemoryTrimmerTest, TimerBoolTaskRunFromPreFreeze) {
  OneShotDelayedBackgroundTimer timer;
  std::optional<MemoryReductionTaskContext> called_task_type = std::nullopt;

  ASSERT_EQ(pending_task_count(), 0u);
  ASSERT_FALSE(timer.IsRunning());

  timer.Start(
      FROM_HERE, base::Seconds(30),
      base::BindOnce(
          [](std::optional<MemoryReductionTaskContext>& called_task_type,
             MemoryReductionTaskContext task_type) {
            called_task_type = task_type;
          },
          std::ref(called_task_type)));

  ASSERT_FALSE(called_task_type.has_value());
  ASSERT_EQ(pending_task_count(), 1u);

  PreFreezeBackgroundMemoryTrimmer::OnPreFreezeForTesting();

  ASSERT_EQ(pending_task_count(), 0u);
  EXPECT_EQ(called_task_type.value(), MemoryReductionTaskContext::kProactive);
}

}  // namespace base::android
