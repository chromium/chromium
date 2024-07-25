// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/metrics/metrics_provider_desktop.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/performance_manager/public/user_tuning/user_performance_tuning_manager.h"
#include "chrome/browser/performance_manager/test_support/fake_frame_throttling_delegate.h"
#include "chrome/browser/performance_manager/test_support/test_user_performance_tuning_manager_environment.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

class PerformanceManagerMetricsProviderDesktopTest : public testing::Test {
 protected:
  PrefService* local_state() { return &local_state_; }

  void SetMemorySaverEnabled(bool enabled) {
    local_state()->SetInteger(
        performance_manager::user_tuning::prefs::kMemorySaverModeState,
        static_cast<int>(enabled ? performance_manager::user_tuning::prefs::
                                       MemorySaverModeState::kEnabled
                                 : performance_manager::user_tuning::prefs::
                                       MemorySaverModeState::kDisabled));
  }

  void SetBatterySaverEnabled(bool enabled) {
    performance_manager::user_tuning::
        TestUserPerformanceTuningManagerEnvironment::SetBatterySaverMode(
            &local_state_, enabled);
  }

  void ExpectSingleUniqueSample(
      const base::HistogramTester& tester,
      performance_manager::MetricsProviderDesktop::EfficiencyMode sample,
      int battery_saver_percent,
      int memory_saver_percent) {
    tester.ExpectUniqueSample("PerformanceManager.UserTuning.EfficiencyMode",
                              sample, 1);
    tester.ExpectUniqueSample(
        "PerformanceManager.UserTuning.BatterySaverModeEnabledPercent",
        battery_saver_percent, 1);
    tester.ExpectUniqueSample(
        "PerformanceManager.UserTuning.MemorySaverModeEnabledPercent",
        memory_saver_percent, 1);
  }

  void InitProvider() { provider_->Initialize(); }

  performance_manager::MetricsProviderDesktop* provider() {
    return provider_.get();
  }

  void ShutdownUserPerformanceTuningManager() {
    user_performance_tuning_env_->TearDown();
    user_performance_tuning_env_.reset();
  }

  void FastForwardBy(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

 private:
  void SetUp() override {
    performance_manager::user_tuning::prefs::RegisterLocalStatePrefs(
        local_state_.registry());

    user_performance_tuning_env_ =
        std::make_unique<performance_manager::user_tuning::
                             TestUserPerformanceTuningManagerEnvironment>();
    user_performance_tuning_env_->SetUp(&local_state_);

    provider_.reset(
        new performance_manager::MetricsProviderDesktop(local_state()));
  }

  void TearDown() override {
    // Tests may teardown the environment before this is called to make some
    // assertions.
    if (user_performance_tuning_env_) {
      user_performance_tuning_env_->TearDown();
    }
  }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  TestingPrefServiceSimple local_state_;

  std::unique_ptr<performance_manager::user_tuning::
                      TestUserPerformanceTuningManagerEnvironment>
      user_performance_tuning_env_;
  std::unique_ptr<performance_manager::MetricsProviderDesktop> provider_;
};

TEST_F(PerformanceManagerMetricsProviderDesktopTest, TestNormalMode) {
  SetMemorySaverEnabled(false);

  InitProvider();
  base::HistogramTester tester;

  provider()->ProvideCurrentSessionData(nullptr);

  ExpectSingleUniqueSample(
      tester,
      performance_manager::MetricsProviderDesktop::EfficiencyMode::kNormal,
      /*battery_saver_percent=*/0, /*memory_saver_percent=*/0);
}

TEST_F(PerformanceManagerMetricsProviderDesktopTest, TestMixedMode) {
  // Start in normal mode
  SetMemorySaverEnabled(false);

  InitProvider();
  {
    base::HistogramTester tester;
    provider()->ProvideCurrentSessionData(nullptr);
    ExpectSingleUniqueSample(
        tester,
        performance_manager::MetricsProviderDesktop::EfficiencyMode::kNormal,
        /*battery_saver_percent=*/0, /*memory_saver_percent=*/0);
  }

  {
    base::HistogramTester tester;
    // Enable High-Efficiency Mode, the next reported value should be "mixed"
    // because we transitioned from normal to High-Efficiency during the
    // interval. Simulate 10 minutes elapsing before and after the change, which
    // should result in memory saver being reported as enabled for 50% of the
    // interval
    FastForwardBy(base::Minutes(10));
    SetMemorySaverEnabled(true);
    FastForwardBy(base::Minutes(10));
    provider()->ProvideCurrentSessionData(nullptr);
    ExpectSingleUniqueSample(
        tester,
        performance_manager::MetricsProviderDesktop::EfficiencyMode::kMixed,
        /*battery_saver_percent=*/0, /*memory_saver_percent=*/50);
  }

  {
    base::HistogramTester tester;
    // If another UMA upload happens without mode changes, this one will report
    // High-Efficiency Mode. Memory Saver is considered on for 100% of the
    // interval.
    provider()->ProvideCurrentSessionData(nullptr);
    ExpectSingleUniqueSample(tester,
                             performance_manager::MetricsProviderDesktop::
                                 EfficiencyMode::kMemorySaver,
                             /*battery_saver_percent=*/0,
                             /*memory_saver_percent=*/100);
  }

  {
    base::HistogramTester tester;
    // Advance time, change battery saver state, then advance time again. The
    // Battery Saver Percent histogram should report the correct percentage.
    FastForwardBy(base::Minutes(40));
    SetBatterySaverEnabled(true);
    FastForwardBy(base::Minutes(10));
    provider()->ProvideCurrentSessionData(nullptr);
    ExpectSingleUniqueSample(
        tester,
        performance_manager::MetricsProviderDesktop::EfficiencyMode::kMixed,
        /*battery_saver_percent=*/20, /*memory_saver_percent=*/100);
  }
}

TEST_F(PerformanceManagerMetricsProviderDesktopTest, TestBothModes) {
  SetMemorySaverEnabled(true);
  SetBatterySaverEnabled(true);

  InitProvider();

  {
    base::HistogramTester tester;
    // Start with both modes enabled (such as a Chrome startup after having
    // enabled both modes in a previous session).
    provider()->ProvideCurrentSessionData(nullptr);
    ExpectSingleUniqueSample(
        tester,
        performance_manager::MetricsProviderDesktop::EfficiencyMode::kBoth,
        /*battery_saver_percent=*/100, /*memory_saver_percent=*/100);
  }

  {
    base::HistogramTester tester;
    // Disabling High-Efficiency Mode will cause the next report to be "mixed".
    // Since the time didn't advance, memory saver was off for the entire
    // interval and battery saver was on for it.
    SetMemorySaverEnabled(false);
    provider()->ProvideCurrentSessionData(nullptr);
    ExpectSingleUniqueSample(
        tester,
        performance_manager::MetricsProviderDesktop::EfficiencyMode::kMixed,
        /*battery_saver_percent=*/100, /*memory_saver_percent=*/0);
  }

  {
    base::HistogramTester tester;
    // No changes until the following report, "Battery saver" is reported
    provider()->ProvideCurrentSessionData(nullptr);
    ExpectSingleUniqueSample(tester,
                             performance_manager::MetricsProviderDesktop::
                                 EfficiencyMode::kBatterySaver,
                             /*battery_saver_percent=*/100,
                             /*memory_saver_percent=*/0);
  }

  {
    base::HistogramTester tester;
    // Re-enabling High-Efficiency Mode will cause the next report to indicate
    // "mixed".
    FastForwardBy(base::Minutes(10));
    SetMemorySaverEnabled(true);
    FastForwardBy(base::Minutes(30));
    provider()->ProvideCurrentSessionData(nullptr);
    ExpectSingleUniqueSample(
        tester,
        performance_manager::MetricsProviderDesktop::EfficiencyMode::kMixed,
        /*battery_saver_percent=*/100, /*memory_saver_percent=*/75);
  }

  {
    base::HistogramTester tester;
    // One more report with no changes, this one reports "both" again.
    provider()->ProvideCurrentSessionData(nullptr);
    ExpectSingleUniqueSample(
        tester,
        performance_manager::MetricsProviderDesktop::EfficiencyMode::kBoth,
        /*battery_saver_percent=*/100, /*memory_saver_percent=*/100);
  }
}

TEST_F(PerformanceManagerMetricsProviderDesktopTest,
       TestCorrectlyLoggedDuringShutdown) {
  SetMemorySaverEnabled(false);
  SetBatterySaverEnabled(true);

  InitProvider();

  {
    base::HistogramTester tester;
    // No changes until the following report, "Battery saver" is reported
    provider()->ProvideCurrentSessionData(nullptr);
    ExpectSingleUniqueSample(tester,
                             performance_manager::MetricsProviderDesktop::
                                 EfficiencyMode::kBatterySaver,
                             /*battery_saver_percent=*/100,
                             /*memory_saver_percent=*/0);
  }

  ShutdownUserPerformanceTuningManager();

  // During shutdown, the MetricsProviderDesktop will attempt to record session
  // data one last time. This happens after the UserPerformanceTuningManager is
  // destroyed, which can cause a crash if the manager is accessed to compute
  // the current mode.
  {
    base::HistogramTester tester;
    // No changes until the following report, "Battery saver" is reported
    provider()->ProvideCurrentSessionData(nullptr);
    ExpectSingleUniqueSample(tester,
                             performance_manager::MetricsProviderDesktop::
                                 EfficiencyMode::kBatterySaver,
                             /*battery_saver_percent=*/100,
                             /*memory_saver_percent=*/0);
  }
}

TEST_F(PerformanceManagerMetricsProviderDesktopTest,
       TestCpuThrottlingMetricRecordedWhereAvailable) {
  InitProvider();
  base::HistogramTester tester;

  FastForwardBy(base::Minutes(5));
  tester.ExpectTotalCount(
      "CPU.Experimental.EstimatedFrequencyAsPercentOfMax.Performance",
      performance_manager::MetricsProviderDesktop::
              ShouldCollectCpuFrequencyMetrics()
          ? 1
          : 0);

  tester.ExpectTotalCount(
      "CPU.Experimental.EstimatedFrequencyAsPercentOfLimit.Performance",
      performance_manager::MetricsProviderDesktop::
              ShouldCollectCpuFrequencyMetrics()
          ? 1
          : 0);

  tester.ExpectTotalCount(
      "CPU.Experimental.CpuEstimationTaskQueuedTime.Performance",
      performance_manager::MetricsProviderDesktop::
              ShouldCollectCpuFrequencyMetrics()
          ? 1
          : 0);
  tester.ExpectTotalCount(
      "CPU.Experimental.CpuEstimationTaskTotalTime.Performance",
      performance_manager::MetricsProviderDesktop::
              ShouldCollectCpuFrequencyMetrics()
          ? 1
          : 0);
  tester.ExpectTotalCount(
      "CPU.Experimental.CpuEstimationTaskThreadTime.Performance",
      performance_manager::MetricsProviderDesktop::
              ShouldCollectCpuFrequencyMetrics()
          ? 1
          : 0);
  tester.ExpectTotalCount(
      "CPU.Experimental.CpuEstimationTaskWallTime.Performance",
      performance_manager::MetricsProviderDesktop::
              ShouldCollectCpuFrequencyMetrics()
          ? 1
          : 0);
}
