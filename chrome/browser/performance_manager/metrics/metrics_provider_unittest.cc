// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/metrics/metrics_provider.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/performance_manager/public/user_tuning/user_performance_tuning_manager.h"
#include "chrome/browser/performance_manager/test_support/fake_frame_throttling_delegate.h"
#include "chrome/browser/performance_manager/test_support/test_user_performance_tuning_manager_environment.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "base/logging.h"

class FakeHighEfficiencyModeToggleDelegate
    : public performance_manager::user_tuning::UserPerformanceTuningManager::
          HighEfficiencyModeToggleDelegate {
 public:
  void ToggleHighEfficiencyMode(bool enabled) override {}
  ~FakeHighEfficiencyModeToggleDelegate() override = default;
};

class PerformanceManagerMetricsProviderTest : public testing::Test {
 protected:
  PrefService* local_state() { return &local_state_; }

  void SetHighEfficiencyEnabled(bool enabled) {
    local_state()->SetBoolean(
        performance_manager::user_tuning::prefs::kHighEfficiencyModeEnabled,
        enabled);
  }

  void SetBatterySaverEnabled(bool enabled) {
    local_state()->SetInteger(
        performance_manager::user_tuning::prefs::kBatterySaverModeState,
        static_cast<int>(enabled ? performance_manager::user_tuning::prefs::
                                       BatterySaverModeState::kEnabled
                                 : performance_manager::user_tuning::prefs::
                                       BatterySaverModeState::kDisabled));
  }

  void ExpectSingleUniqueSample(
      const base::HistogramTester& tester,
      performance_manager::MetricsProvider::EfficiencyMode sample) {
    tester.ExpectUniqueSample("PerformanceManager.UserTuning.EfficiencyMode",
                              sample, 1);
  }

  void InitProvider() { provider_->Initialize(); }

  performance_manager::MetricsProvider* provider() { return provider_.get(); }

  void ShutdownUserPerformanceTuningManager() {
    user_performance_tuning_env_->TearDown();
    user_performance_tuning_env_.reset();
  }

 private:
  void SetUp() override {
    feature_list_.InitWithFeatures(
        {performance_manager::features::kHighEfficiencyModeAvailable,
         performance_manager::features::kBatterySaverModeAvailable},
        {});

    performance_manager::user_tuning::prefs::RegisterLocalStatePrefs(
        local_state_.registry());

    user_performance_tuning_env_ =
        std::make_unique<performance_manager::user_tuning::
                             TestUserPerformanceTuningManagerEnvironment>();
    user_performance_tuning_env_->SetUp(&local_state_);

    provider_.reset(new performance_manager::MetricsProvider(local_state()));
  }

  void TearDown() override {
    // Tests may teardown the environment before this is called to make some
    // assertions.
    if (user_performance_tuning_env_)
      user_performance_tuning_env_->TearDown();
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  TestingPrefServiceSimple local_state_;
  base::test::ScopedFeatureList feature_list_;

  std::unique_ptr<performance_manager::user_tuning::
                      TestUserPerformanceTuningManagerEnvironment>
      user_performance_tuning_env_;
  std::unique_ptr<performance_manager::MetricsProvider> provider_;
};

TEST_F(PerformanceManagerMetricsProviderTest, TestNormalMode) {
  InitProvider();
  base::HistogramTester tester;

  provider()->ProvideCurrentSessionData(nullptr);

  ExpectSingleUniqueSample(
      tester, performance_manager::MetricsProvider::EfficiencyMode::kNormal);
}

TEST_F(PerformanceManagerMetricsProviderTest, TestMixedMode) {
  InitProvider();
  {
    base::HistogramTester tester;
    // Start in normal mode
    provider()->ProvideCurrentSessionData(nullptr);
    ExpectSingleUniqueSample(
        tester, performance_manager::MetricsProvider::EfficiencyMode::kNormal);
  }

  {
    base::HistogramTester tester;
    // Enabled High-Efficiency Mode, the next reported value should be "mixed"
    // because we transitioned from normal to High-Efficiency during the
    // interval.
    SetHighEfficiencyEnabled(true);
    provider()->ProvideCurrentSessionData(nullptr);
    ExpectSingleUniqueSample(
        tester, performance_manager::MetricsProvider::EfficiencyMode::kMixed);
  }

  {
    base::HistogramTester tester;
    // If another UMA upload happens without mode changes, this one will report
    // High-Efficiency Mode.
    provider()->ProvideCurrentSessionData(nullptr);
    ExpectSingleUniqueSample(
        tester,
        performance_manager::MetricsProvider::EfficiencyMode::kHighEfficiency);
  }
}

TEST_F(PerformanceManagerMetricsProviderTest, TestBothModes) {
  SetHighEfficiencyEnabled(true);
  SetBatterySaverEnabled(true);

  InitProvider();

  {
    base::HistogramTester tester;
    // Start with both modes enabled (such as a Chrome startup after having
    // enabled both modes in a previous session).
    provider()->ProvideCurrentSessionData(nullptr);
    ExpectSingleUniqueSample(
        tester, performance_manager::MetricsProvider::EfficiencyMode::kBoth);
  }

  {
    base::HistogramTester tester;
    // Disabling High-Efficiency Mode will cause the next report to be "mixed".
    SetHighEfficiencyEnabled(false);
    provider()->ProvideCurrentSessionData(nullptr);
    ExpectSingleUniqueSample(
        tester, performance_manager::MetricsProvider::EfficiencyMode::kMixed);
  }

  {
    base::HistogramTester tester;
    // No changes until the following report, "Battery saver" is reported
    provider()->ProvideCurrentSessionData(nullptr);
    ExpectSingleUniqueSample(
        tester,
        performance_manager::MetricsProvider::EfficiencyMode::kBatterySaver);
  }

  {
    base::HistogramTester tester;
    // Re-enabling High-Efficiency Mode will cause the next report to indicate
    // "mixed".
    SetHighEfficiencyEnabled(true);
    provider()->ProvideCurrentSessionData(nullptr);
    ExpectSingleUniqueSample(
        tester, performance_manager::MetricsProvider::EfficiencyMode::kMixed);
  }

  {
    base::HistogramTester tester;
    // One more report with no changes, this one reports "both" again.
    provider()->ProvideCurrentSessionData(nullptr);
    ExpectSingleUniqueSample(
        tester, performance_manager::MetricsProvider::EfficiencyMode::kBoth);
  }
}

TEST_F(PerformanceManagerMetricsProviderTest,
       TestCorrectlyLoggedDuringShutdown) {
  SetBatterySaverEnabled(true);

  InitProvider();

  {
    base::HistogramTester tester;
    // No changes until the following report, "Battery saver" is reported
    provider()->ProvideCurrentSessionData(nullptr);
    ExpectSingleUniqueSample(
        tester,
        performance_manager::MetricsProvider::EfficiencyMode::kBatterySaver);
  }

  ShutdownUserPerformanceTuningManager();

  // During shutdown, the MetricsProvider will attempt to record session data
  // one last time. This happens after the UserPerformanceTuningManager is
  // destroyed, which can cause a crash if the manager is accessed to compute
  // the current mode.
  {
    base::HistogramTester tester;
    // No changes until the following report, "Battery saver" is reported
    provider()->ProvideCurrentSessionData(nullptr);
    ExpectSingleUniqueSample(
        tester,
        performance_manager::MetricsProvider::EfficiencyMode::kBatterySaver);
  }
}
