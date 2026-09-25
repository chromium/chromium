// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/scudo_purge_coordinator.h"

#include <utility>
#include <vector>

#include "base/android/pre_freeze_background_memory_trimmer.h"
#include "base/android/scudo_features.h"
#include "base/functional/bind.h"
#include "base/synchronization/lock.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base::android {

class ScudoPurgeCoordinatorTest : public testing::Test {
 public:
  ScudoPurgeCoordinatorTest()
      : task_environment_(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME,
            base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED) {}

  void SetUp() override {
    PreFreezeBackgroundMemoryTrimmer::SetSupportsModernTrimForTesting(true);
    PreFreezeBackgroundMemoryTrimmer::ResetForTesting();
  }

  void TearDown() override {
    PreFreezeBackgroundMemoryTrimmer::ResetForTesting();
  }

  ScudoPurgeCoordinator::Configuration CreateDefaultTestConfig() {
    ScudoPurgeCoordinator::Configuration config;
    config.mallopt_fn_for_testing = base::BindRepeating(
        &ScudoPurgeCoordinatorTest::RecordMalloptCall, base::Unretained(this));
    return config;
  }

  bool RecordMalloptCall(int param, int /*value*/) {
    base::AutoLock auto_lock(lock_);
    mallopt_calls_.push_back(param);
    if (param == kScudoPurgeAll && fail_purge_all_) {
      return false;
    }
    return true;
  }

  std::vector<int> GetMalloptCalls() {
    base::AutoLock auto_lock(lock_);
    return mallopt_calls_;
  }

  void ClearMalloptCalls() {
    base::AutoLock auto_lock(lock_);
    mallopt_calls_.clear();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::Lock lock_;
  std::vector<int> mallopt_calls_ GUARDED_BY(lock_);
  bool fail_purge_all_ = false;
};

// InitialDelayRespected: Verify no purges occur before 60s; fires
// kScudoPurge after 60s.
TEST_F(ScudoPurgeCoordinatorTest, InitialDelayRespected) {
  ScudoPurgeCoordinator coordinator(CreateDefaultTestConfig());
  coordinator.Start();

  task_environment_.FastForwardBy(base::Seconds(59));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(GetMalloptCalls().empty());

  task_environment_.FastForwardBy(base::Seconds(1));
  task_environment_.RunUntilIdle();
  EXPECT_THAT(GetMalloptCalls(), testing::ElementsAre(kScudoPurge));
}

// PeriodicForegroundLoopEmitsOnlyPurge: Verify subsequent ticks only call
// kScudoPurge (-101) every 60s.
TEST_F(ScudoPurgeCoordinatorTest, PeriodicForegroundLoopEmitsOnlyPurge) {
  ScudoPurgeCoordinator coordinator(CreateDefaultTestConfig());
  coordinator.Start();

  // Initial purge at t=60s.
  task_environment_.FastForwardBy(base::Seconds(60));
  task_environment_.RunUntilIdle();
  EXPECT_THAT(GetMalloptCalls(), testing::ElementsAre(kScudoPurge));

  // Ticks every 60s emit kScudoPurge.
  task_environment_.FastForwardBy(base::Seconds(60));
  task_environment_.RunUntilIdle();
  EXPECT_THAT(GetMalloptCalls(),
              testing::ElementsAre(kScudoPurge, kScudoPurge));

  task_environment_.FastForwardBy(base::Seconds(60));
  task_environment_.RunUntilIdle();
  EXPECT_THAT(GetMalloptCalls(),
              testing::ElementsAre(kScudoPurge, kScudoPurge, kScudoPurge));
}

// BackgroundTransitionStopsPeriodicTicks: Verify periodic timer stops on
// OnBackgrounded().
TEST_F(ScudoPurgeCoordinatorTest, BackgroundTransitionStopsPeriodicTicks) {
  ScudoPurgeCoordinator coordinator(CreateDefaultTestConfig());
  coordinator.Start();

  task_environment_.FastForwardBy(base::Seconds(60));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(coordinator.is_foreground_periodic_running_for_testing());
  ClearMalloptCalls();

  coordinator.OnBackgrounded();
  EXPECT_FALSE(coordinator.is_in_foreground_for_testing());
  EXPECT_FALSE(coordinator.is_foreground_periodic_running_for_testing());

  // Let background purge fire at 10s and clear it.
  task_environment_.FastForwardBy(base::Seconds(10));
  task_environment_.RunUntilIdle();
  ClearMalloptCalls();

  // Fast forward past periodic intervals; no foreground ticks should occur.
  task_environment_.FastForwardBy(base::Seconds(120));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(GetMalloptCalls().empty());
}

// BackgroundTimerCancellationOnResume: App backgrounded at t=60s
// (10s timer). Resumed at t=65s. Fast-forward past 70s. Verify background timer
// was stopped, zero background purges executed.
TEST_F(ScudoPurgeCoordinatorTest, BackgroundTimerCancellationOnResume) {
  ScudoPurgeCoordinator coordinator(CreateDefaultTestConfig());
  coordinator.Start();

  task_environment_.FastForwardBy(base::Seconds(60));
  task_environment_.RunUntilIdle();
  ClearMalloptCalls();

  coordinator.OnBackgrounded();
  EXPECT_TRUE(coordinator.is_background_timer_running_for_testing());

  // Fast forward to t=65s and foreground.
  task_environment_.FastForwardBy(base::Seconds(5));
  coordinator.OnForegrounded();
  EXPECT_FALSE(coordinator.is_background_timer_running_for_testing());
  EXPECT_TRUE(coordinator.is_in_foreground_for_testing());

  // Fast forward past 70s.
  task_environment_.FastForwardBy(base::Seconds(10));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(GetMalloptCalls().empty());
}

// ResumptionInitialDelayRespected: Verifies that after OnForegrounded(),
// initial delay is respected before periodic ticks resume.
TEST_F(ScudoPurgeCoordinatorTest, ResumptionInitialDelayRespected) {
  ScudoPurgeCoordinator coordinator(CreateDefaultTestConfig());
  coordinator.Start();

  // Reach steady state and background.
  task_environment_.FastForwardBy(base::Seconds(60));
  task_environment_.RunUntilIdle();
  coordinator.OnBackgrounded();
  task_environment_.FastForwardBy(base::Seconds(10));
  task_environment_.RunUntilIdle();
  ClearMalloptCalls();

  // Foreground the app. Initial delay is 60s.
  coordinator.OnForegrounded();
  EXPECT_TRUE(coordinator.is_in_foreground_for_testing());
  EXPECT_FALSE(coordinator.is_foreground_periodic_running_for_testing());

  // Advance 59s: no ticks.
  task_environment_.FastForwardBy(base::Seconds(59));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(GetMalloptCalls().empty());

  // Advance 1s (total 60s): initial delay expires, purge runs, periodic starts.
  task_environment_.FastForwardBy(base::Seconds(1));
  task_environment_.RunUntilIdle();
  EXPECT_THAT(GetMalloptCalls(), testing::ElementsAre(kScudoPurge));
  EXPECT_TRUE(coordinator.is_foreground_periodic_running_for_testing());

  // Periodic ticks resume after periodic interval (60s).
  task_environment_.FastForwardBy(base::Seconds(60));
  task_environment_.RunUntilIdle();
  EXPECT_THAT(GetMalloptCalls(),
              testing::ElementsAre(kScudoPurge, kScudoPurge));
}

// BackgroundPurgeExecution: App backgrounded and stays in background for
// 10s. Verifies kScudoPurgeAll (-104) is called.
TEST_F(ScudoPurgeCoordinatorTest, BackgroundPurgeExecution) {
  ScudoPurgeCoordinator coordinator(CreateDefaultTestConfig());
  coordinator.Start();

  task_environment_.FastForwardBy(base::Seconds(60));
  task_environment_.RunUntilIdle();
  ClearMalloptCalls();

  coordinator.OnBackgrounded();
  EXPECT_TRUE(coordinator.is_background_timer_running_for_testing());

  task_environment_.FastForwardBy(base::Seconds(10));
  task_environment_.RunUntilIdle();
  EXPECT_THAT(GetMalloptCalls(), testing::ElementsAre(kScudoPurgeAll));
  EXPECT_FALSE(coordinator.is_background_timer_running_for_testing());
}

// FallbackWhenPurgeAllUnsupported: When mock returns false for kScudoPurgeAll,
// verify coordinator automatically calls kScudoPurge.
TEST_F(ScudoPurgeCoordinatorTest, FallbackWhenPurgeAllUnsupported) {
  ScudoPurgeCoordinator coordinator(CreateDefaultTestConfig());
  coordinator.Start();
  task_environment_.FastForwardBy(base::Seconds(60));
  task_environment_.RunUntilIdle();
  ClearMalloptCalls();

  fail_purge_all_ = true;

  coordinator.OnBackgrounded();
  task_environment_.FastForwardBy(base::Seconds(10));
  task_environment_.RunUntilIdle();

  EXPECT_THAT(GetMalloptCalls(),
              testing::ElementsAre(kScudoPurgeAll, kScudoPurge));
}

// ConfigurationFromFeatures: Verifies Configuration::FromFeatures() parses
// feature parameters properly.
TEST_F(ScudoPurgeCoordinatorTest, ConfigurationFromFeatures) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      base::features::kPeriodicScudoPurge,
      {{base::features::kScudoInitialPurgeDelay.name, "45s"},
       {base::features::kScudoPeriodicPurgeInterval.name, "90s"},
       {base::features::kScudoBackgroundPurgeDelay.name, "15s"},
       {base::features::kScudoEnableForegroundPeriodic.name, "true"},
       {base::features::kScudoEnableBackgroundPurge.name, "false"}});

  ScudoPurgeCoordinator::Configuration config =
      ScudoPurgeCoordinator::Configuration::FromFeatures();

  EXPECT_EQ(config.initial_delay, base::Seconds(45));
  EXPECT_EQ(config.periodic_interval, base::Seconds(90));
  EXPECT_EQ(config.background_delay, base::Seconds(15));
  EXPECT_TRUE(config.enable_foreground_periodic);
  EXPECT_FALSE(config.enable_background_purge);
}

// EmitsDurationHistograms: Verifies that duration metrics are recorded for
// both foreground and background purges in the browser process.
TEST_F(ScudoPurgeCoordinatorTest, EmitsDurationHistograms) {
  base::HistogramTester histogram_tester;
  ScudoPurgeCoordinator coordinator(CreateDefaultTestConfig());
  coordinator.Start();

  // Trigger foreground purge after initial delay in browser process.
  task_environment_.FastForwardBy(base::Seconds(60));
  task_environment_.RunUntilIdle();
  histogram_tester.ExpectTotalCount(
      "Memory.Experimental.ScudoPurge.Duration.Browser.Foreground", 1);
  histogram_tester.ExpectTotalCount(
      "Memory.Experimental.ScudoPurge.Duration.Browser.Background", 0);
  histogram_tester.ExpectTotalCount(
      "Memory.Experimental.ScudoPurge.Duration.GPU.Foreground", 0);

  // Transition to background and wait for background purge.
  coordinator.OnBackgrounded();
  task_environment_.FastForwardBy(base::Seconds(10));
  task_environment_.RunUntilIdle();
  histogram_tester.ExpectTotalCount(
      "Memory.Experimental.ScudoPurge.Duration.Browser.Foreground", 1);
  histogram_tester.ExpectTotalCount(
      "Memory.Experimental.ScudoPurge.Duration.Browser.Background", 1);
  histogram_tester.ExpectTotalCount(
      "Memory.Experimental.ScudoPurge.Duration.GPU.Background", 0);
}

// EmitsDurationHistogramsGpu: Verifies that duration metrics are recorded for
// both foreground and background purges in the GPU process.
TEST_F(ScudoPurgeCoordinatorTest, EmitsDurationHistogramsGpu) {
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII("type",
                                                                 "gpu-process");

  base::HistogramTester histogram_tester;
  ScudoPurgeCoordinator coordinator(CreateDefaultTestConfig());
  coordinator.Start();

  task_environment_.FastForwardBy(base::Seconds(60));
  task_environment_.RunUntilIdle();
  histogram_tester.ExpectTotalCount(
      "Memory.Experimental.ScudoPurge.Duration.GPU.Foreground", 1);
  histogram_tester.ExpectTotalCount(
      "Memory.Experimental.ScudoPurge.Duration.GPU.Background", 0);
  histogram_tester.ExpectTotalCount(
      "Memory.Experimental.ScudoPurge.Duration.Browser.Foreground", 0);

  coordinator.OnBackgrounded();
  task_environment_.FastForwardBy(base::Seconds(10));
  task_environment_.RunUntilIdle();
  histogram_tester.ExpectTotalCount(
      "Memory.Experimental.ScudoPurge.Duration.GPU.Foreground", 1);
  histogram_tester.ExpectTotalCount(
      "Memory.Experimental.ScudoPurge.Duration.GPU.Background", 1);
  histogram_tester.ExpectTotalCount(
      "Memory.Experimental.ScudoPurge.Duration.Browser.Background", 0);
}

// MalloptFailureDoesNotEmitHistograms: Verifies that when mallopt fails,
// duration histograms are not emitted.
TEST_F(ScudoPurgeCoordinatorTest, MalloptFailureDoesNotEmitHistograms) {
  ScudoPurgeCoordinator::Configuration config;
  config.mallopt_fn_for_testing =
      base::BindRepeating([](int, int) { return false; });
  base::HistogramTester histogram_tester;
  ScudoPurgeCoordinator coordinator(std::move(config));
  coordinator.Start();

  task_environment_.FastForwardBy(base::Seconds(60));
  task_environment_.RunUntilIdle();
  histogram_tester.ExpectTotalCount(
      "Memory.Experimental.ScudoPurge.Duration.Browser.Foreground", 0);

  coordinator.OnBackgrounded();
  task_environment_.FastForwardBy(base::Seconds(10));
  task_environment_.RunUntilIdle();
  histogram_tester.ExpectTotalCount(
      "Memory.Experimental.ScudoPurge.Duration.Browser.Background", 0);
}

}  // namespace base::android
