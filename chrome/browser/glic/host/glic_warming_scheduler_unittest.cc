// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_warming_scheduler.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "components/performance_manager/scenario_api/performance_scenario_test_support.h"
#include "components/performance_manager/scenario_api/performance_scenarios.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {
namespace {

using performance_scenarios::InputScenario;
using performance_scenarios::LoadingScenario;
using performance_scenarios::PerformanceScenarioTestHelper;
using performance_scenarios::ScenarioScope;

// Mirrors SchedulingMethod enum in glic_warming_scheduler.cc for metric
// verification.
enum class SchedulingMethod {
  kFixedDelay = 0,
  kPerformanceManagerImmediateIdle = 1,
  kPerformanceManagerObserved = 2,
  kPerformanceManagerMissingObserverListFallback = 3,
};

class GlicWarmingSchedulerTest : public ::testing::Test {
 public:
  GlicWarmingSchedulerTest()
      : task_environment_(
            content::BrowserTaskEnvironment::TimeSource::MOCK_TIME) {}

 protected:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(GlicWarmingSchedulerTest, FixedDelayTriggersAfterDelay) {
  base::HistogramTester histogram_tester;
  GlicWarmingScheduler::Options options{
      .use_performance_manager = false,
      .delay = base::Seconds(20),
  };
  GlicWarmingScheduler scheduler(options);

  base::test::TestFuture<void> future;
  scheduler.Schedule(future.GetCallback());
  EXPECT_TRUE(scheduler.IsScheduled());
  EXPECT_FALSE(future.IsReady());

  histogram_tester.ExpectUniqueSample("Glic.WarmingScheduler.SchedulingMethod",
                                      SchedulingMethod::kFixedDelay, 1);

  task_environment_.FastForwardBy(base::Seconds(19));
  EXPECT_FALSE(future.IsReady());
  EXPECT_TRUE(scheduler.IsScheduled());

  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_TRUE(future.Wait());
  EXPECT_FALSE(scheduler.IsScheduled());
}

TEST_F(GlicWarmingSchedulerTest, FixedDelayZeroTriggersAsynchronously) {
  base::HistogramTester histogram_tester;
  GlicWarmingScheduler::Options options{
      .use_performance_manager = false,
      .delay = base::TimeDelta(),
  };
  GlicWarmingScheduler scheduler(options);

  base::test::TestFuture<void> future;
  scheduler.Schedule(future.GetCallback());
  EXPECT_TRUE(scheduler.IsScheduled());
  EXPECT_FALSE(future.IsReady());

  histogram_tester.ExpectUniqueSample("Glic.WarmingScheduler.SchedulingMethod",
                                      SchedulingMethod::kFixedDelay, 1);

  EXPECT_TRUE(future.Wait());
  EXPECT_FALSE(scheduler.IsScheduled());
}

TEST_F(GlicWarmingSchedulerTest, CancelStopsPendingExecution) {
  GlicWarmingScheduler::Options options{
      .use_performance_manager = false,
      .delay = base::Seconds(20),
  };
  GlicWarmingScheduler scheduler(options);

  base::test::TestFuture<void> future;
  scheduler.Schedule(future.GetCallback());
  EXPECT_TRUE(scheduler.IsScheduled());

  scheduler.Cancel();
  EXPECT_FALSE(scheduler.IsScheduled());

  task_environment_.FastForwardBy(base::Seconds(30));
  EXPECT_FALSE(future.IsReady());
}

TEST_F(GlicWarmingSchedulerTest,
       PerformanceManagerTriggersImmediatelyIfAlreadyIdle) {
  base::HistogramTester histogram_tester;
  auto test_helper = PerformanceScenarioTestHelper::Create();
  ASSERT_TRUE(test_helper);
  test_helper->SetLoadingScenario(ScenarioScope::kGlobal,
                                  LoadingScenario::kNoPageLoading);
  test_helper->SetInputScenario(ScenarioScope::kGlobal,
                                InputScenario::kNoInput);

  GlicWarmingScheduler::Options options{
      .use_performance_manager = true,
      .delay = base::Seconds(20),
  };
  GlicWarmingScheduler scheduler(options);

  base::test::TestFuture<void> future;
  scheduler.Schedule(future.GetCallback());
  EXPECT_TRUE(scheduler.IsScheduled());
  EXPECT_FALSE(future.IsReady());

  histogram_tester.ExpectUniqueSample(
      "Glic.WarmingScheduler.SchedulingMethod",
      SchedulingMethod::kPerformanceManagerImmediateIdle, 1);
  histogram_tester.ExpectTotalCount("Glic.WarmingScheduler.TimeToIdle", 1);

  EXPECT_TRUE(future.Wait());
  EXPECT_FALSE(scheduler.IsScheduled());
}

TEST_F(GlicWarmingSchedulerTest,
       PerformanceManagerTriggersWhenScenarioBecomesIdle) {
  base::HistogramTester histogram_tester;
  auto test_helper = PerformanceScenarioTestHelper::Create();
  ASSERT_TRUE(test_helper);
  test_helper->SetLoadingScenario(ScenarioScope::kGlobal,
                                  LoadingScenario::kFocusedPageLoading);
  test_helper->SetInputScenario(ScenarioScope::kGlobal,
                                InputScenario::kNoInput);

  GlicWarmingScheduler::Options options{
      .use_performance_manager = true,
      .delay = base::Seconds(20),
  };
  GlicWarmingScheduler scheduler(options);

  base::test::TestFuture<void> future;
  scheduler.Schedule(future.GetCallback());
  EXPECT_TRUE(scheduler.IsScheduled());
  EXPECT_FALSE(future.IsReady());

  histogram_tester.ExpectUniqueSample(
      "Glic.WarmingScheduler.SchedulingMethod",
      SchedulingMethod::kPerformanceManagerObserved, 1);
  histogram_tester.ExpectTotalCount("Glic.WarmingScheduler.TimeToIdle", 0);

  // Fast forwarding time past the delay does NOT trigger execution because
  // Performance Manager observation is trusted without a racing timer.
  task_environment_.FastForwardBy(base::Seconds(30));
  EXPECT_FALSE(future.IsReady());
  EXPECT_TRUE(scheduler.IsScheduled());
  histogram_tester.ExpectTotalCount("Glic.WarmingScheduler.TimeToIdle", 0);

  // Transition to idle.
  test_helper->SetLoadingScenario(ScenarioScope::kGlobal,
                                  LoadingScenario::kNoPageLoading);
  EXPECT_TRUE(future.Wait());
  EXPECT_FALSE(scheduler.IsScheduled());
  histogram_tester.ExpectTotalCount("Glic.WarmingScheduler.TimeToIdle", 1);
}

TEST_F(GlicWarmingSchedulerTest,
       PerformanceManagerFallbackWhenObserverListUnavailable) {
  base::HistogramTester histogram_tester;
  // Note: No PerformanceScenarioTestHelper is created, so
  // PerformanceScenarioObserverList::GetForScope() returns nullptr.
  ASSERT_EQ(performance_scenarios::PerformanceScenarioObserverList::GetForScope(
                ScenarioScope::kGlobal),
            nullptr);

  GlicWarmingScheduler::Options options{
      .use_performance_manager = true,
      .delay = base::Seconds(20),
  };
  GlicWarmingScheduler scheduler(options);

  base::test::TestFuture<void> future;
  scheduler.Schedule(future.GetCallback());
  EXPECT_TRUE(scheduler.IsScheduled());
  EXPECT_FALSE(future.IsReady());

  histogram_tester.ExpectUniqueSample(
      "Glic.WarmingScheduler.SchedulingMethod",
      SchedulingMethod::kPerformanceManagerMissingObserverListFallback, 1);
  histogram_tester.ExpectTotalCount("Glic.WarmingScheduler.TimeToIdle", 0);

  task_environment_.FastForwardBy(base::Seconds(19));
  EXPECT_FALSE(future.IsReady());
  EXPECT_TRUE(scheduler.IsScheduled());

  // Delay timer fires fallback after configured delay.
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_TRUE(future.Wait());
  EXPECT_FALSE(scheduler.IsScheduled());
  histogram_tester.ExpectTotalCount("Glic.WarmingScheduler.TimeToIdle", 0);
}

TEST_F(GlicWarmingSchedulerTest, DelayTooLongDisablesWarmingFixedDelay) {
  GlicWarmingScheduler::Options options{
      .use_performance_manager = false,
      .delay = base::Days(7),
  };
  GlicWarmingScheduler scheduler(options);

  base::test::TestFuture<void> future;
  scheduler.Schedule(future.GetCallback());
  EXPECT_FALSE(scheduler.IsScheduled());

  task_environment_.FastForwardBy(base::Days(8));
  EXPECT_FALSE(future.IsReady());
}

TEST_F(GlicWarmingSchedulerTest,
       DelayTooLongDisablesWarmingPerformanceManager) {
  auto test_helper = PerformanceScenarioTestHelper::Create();
  ASSERT_TRUE(test_helper);
  test_helper->SetLoadingScenario(ScenarioScope::kGlobal,
                                  LoadingScenario::kNoPageLoading);
  test_helper->SetInputScenario(ScenarioScope::kGlobal,
                                InputScenario::kNoInput);

  GlicWarmingScheduler::Options options{
      .use_performance_manager = true,
      .delay = base::Days(7),
  };
  GlicWarmingScheduler scheduler(options);

  base::test::TestFuture<void> future;
  scheduler.Schedule(future.GetCallback());
  EXPECT_FALSE(scheduler.IsScheduled());

  task_environment_.FastForwardBy(base::Days(8));
  EXPECT_FALSE(future.IsReady());
}

}  // namespace
}  // namespace glic
