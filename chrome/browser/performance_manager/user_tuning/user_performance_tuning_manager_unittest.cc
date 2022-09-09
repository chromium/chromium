// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/public/user_tuning/user_performance_tuning_manager.h"

#include "base/power_monitor/power_monitor.h"
#include "base/power_monitor/power_monitor_source.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/performance_manager/test_support/fake_frame_throttling_delegate.h"
#include "chrome/browser/performance_manager/test_support/fake_high_efficiency_mode_toggle_delegate.h"
#include "chrome/browser/performance_manager/test_support/fake_power_monitor_source.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager::user_tuning {
namespace {

class QuitRunLoopObserverBase : public performance_manager::user_tuning::
                                    UserPerformanceTuningManager::Observer {
 public:
  explicit QuitRunLoopObserverBase(base::RepeatingClosure quit_closure)
      : quit_closure_(quit_closure) {}

  ~QuitRunLoopObserverBase() override = default;

  void Quit() { quit_closure_.Run(); }

 private:
  base::RepeatingClosure quit_closure_;
};

class QuitRunLoopOnBSMChangeObserver : public QuitRunLoopObserverBase {
 public:
  explicit QuitRunLoopOnBSMChangeObserver(base::RepeatingClosure quit_closure)
      : QuitRunLoopObserverBase(quit_closure) {}

  ~QuitRunLoopOnBSMChangeObserver() override = default;

  // UserPeformanceTuningManager::Observer implementation:
  void OnBatterySaverModeChanged(bool) override { Quit(); }
};

class QuitRunLoopOnPowerStateChangeObserver : public QuitRunLoopObserverBase {
 public:
  explicit QuitRunLoopOnPowerStateChangeObserver(
      base::RepeatingClosure quit_closure)
      : QuitRunLoopObserverBase(quit_closure) {}

  ~QuitRunLoopOnPowerStateChangeObserver() override = default;

  // UserPeformanceTuningManager::Observer implementation:
  void OnExternalPowerConnectedChanged(bool) override { Quit(); }
};

}  // namespace

class UserPerformanceTuningManagerTest : public testing::Test {
 public:
  void SetUp() override {
    auto source = std::make_unique<FakePowerMonitorSource>();
    power_monitor_source_ = source.get();
    base::PowerMonitor::Initialize(std::move(source));

    performance_manager::user_tuning::prefs::RegisterLocalStatePrefs(
        local_state_.registry());
  }

  void StartManager(
      std::vector<base::test::ScopedFeatureList::FeatureAndParams>
          features_and_params = {
              {performance_manager::features::kBatterySaverModeAvailable, {}},
              {performance_manager::features::kHighEfficiencyModeAvailable, {}},
          }) {
    feature_list_.InitWithFeaturesAndParameters(features_and_params, {});
    manager_.reset(new UserPerformanceTuningManager(
        &local_state_, nullptr,
        std::make_unique<FakeFrameThrottlingDelegate>(&throttling_enabled_),
        std::make_unique<FakeHighEfficiencyModeToggleDelegate>()));
    manager()->Start();
  }

  void TearDown() override { base::PowerMonitor::ShutdownForTesting(); }

  UserPerformanceTuningManager* manager() {
    return UserPerformanceTuningManager::GetInstance();
  }
  bool throttling_enabled() const { return throttling_enabled_; }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  TestingPrefServiceSimple local_state_;
  base::test::ScopedFeatureList feature_list_;

  FakePowerMonitorSource* power_monitor_source_;
  bool throttling_enabled_ = false;
  std::unique_ptr<UserPerformanceTuningManager> manager_;
};

TEST_F(UserPerformanceTuningManagerTest, TemporaryBatterySaver) {
  StartManager();
  EXPECT_FALSE(manager()->IsBatterySaverActive());
  EXPECT_FALSE(throttling_enabled());

  local_state_.SetInteger(
      performance_manager::user_tuning::prefs::kBatterySaverModeState,
      static_cast<int>(performance_manager::user_tuning::prefs::
                           BatterySaverModeState::kEnabled));

  EXPECT_TRUE(manager()->IsBatterySaverActive());
  EXPECT_TRUE(throttling_enabled());

  manager()->SetTemporaryBatterySaverDisabledForSession(true);
  EXPECT_FALSE(manager()->IsBatterySaverActive());
  EXPECT_FALSE(throttling_enabled());

  manager()->SetTemporaryBatterySaverDisabledForSession(false);
  EXPECT_TRUE(manager()->IsBatterySaverActive());
  EXPECT_TRUE(throttling_enabled());

  // Changing the pref resets the "disabled for session" flag.
  manager()->SetTemporaryBatterySaverDisabledForSession(true);
  local_state_.SetInteger(
      performance_manager::user_tuning::prefs::kBatterySaverModeState,
      static_cast<int>(performance_manager::user_tuning::prefs::
                           BatterySaverModeState::kEnabledOnBattery));
  EXPECT_FALSE(manager()->IsBatterySaverModeDisabledForSession());

  // TODO(anthonyvd): Test the flag is cleared when the device is plugged in
  // once that CL lands.
}

TEST_F(UserPerformanceTuningManagerTest, BatterySaverModePref) {
  StartManager();
  EXPECT_FALSE(manager()->IsBatterySaverActive());
  EXPECT_FALSE(throttling_enabled());

  local_state_.SetInteger(
      performance_manager::user_tuning::prefs::kBatterySaverModeState,
      static_cast<int>(performance_manager::user_tuning::prefs::
                           BatterySaverModeState::kEnabled));
  EXPECT_TRUE(manager()->IsBatterySaverActive());
  EXPECT_TRUE(throttling_enabled());

  local_state_.SetInteger(
      performance_manager::user_tuning::prefs::kBatterySaverModeState,
      static_cast<int>(performance_manager::user_tuning::prefs::
                           BatterySaverModeState::kDisabled));
  EXPECT_FALSE(manager()->IsBatterySaverActive());
  EXPECT_FALSE(throttling_enabled());
}

TEST_F(UserPerformanceTuningManagerTest, InvalidPrefInStore) {
  StartManager();
  local_state_.SetInteger(
      performance_manager::user_tuning::prefs::kBatterySaverModeState,
      static_cast<int>(performance_manager::user_tuning::prefs::
                           BatterySaverModeState::kEnabled));
  EXPECT_TRUE(manager()->IsBatterySaverActive());
  EXPECT_TRUE(throttling_enabled());

  local_state_.SetInteger(
      performance_manager::user_tuning::prefs::kBatterySaverModeState, -1);
  EXPECT_FALSE(manager()->IsBatterySaverActive());
  EXPECT_FALSE(throttling_enabled());

  local_state_.SetInteger(
      performance_manager::user_tuning::prefs::kBatterySaverModeState,
      static_cast<int>(performance_manager::user_tuning::prefs::
                           BatterySaverModeState::kDisabled) +
          1);
  EXPECT_FALSE(manager()->IsBatterySaverActive());
  EXPECT_FALSE(throttling_enabled());
}

TEST_F(UserPerformanceTuningManagerTest, HEMFinchDisabledByDefault) {
  StartManager({
      {performance_manager::features::kHighEfficiencyModeAvailable,
       {{"default_state", "false"}}},
  });

  EXPECT_FALSE(local_state_.GetBoolean(
      performance_manager::user_tuning::prefs::kHighEfficiencyModeEnabled));
}

TEST_F(UserPerformanceTuningManagerTest, HEMFinchEnabledByDefault) {
  StartManager({
      {performance_manager::features::kHighEfficiencyModeAvailable,
       {{"default_state", "true"}}},
  });

  EXPECT_TRUE(local_state_.GetBoolean(
      performance_manager::user_tuning::prefs::kHighEfficiencyModeEnabled));
}

TEST_F(UserPerformanceTuningManagerTest, EnabledOnBatteryPower) {
  StartManager();

  EXPECT_FALSE(manager()->IsBatterySaverActive());
  EXPECT_FALSE(throttling_enabled());

  local_state_.SetInteger(
      performance_manager::user_tuning::prefs::kBatterySaverModeState,
      static_cast<int>(performance_manager::user_tuning::prefs::
                           BatterySaverModeState::kEnabledOnBattery));
  EXPECT_FALSE(manager()->IsBatterySaverActive());
  EXPECT_FALSE(throttling_enabled());

  {
    base::RunLoop run_loop;
    std::unique_ptr<QuitRunLoopOnBSMChangeObserver> observer =
        std::make_unique<QuitRunLoopOnBSMChangeObserver>(
            run_loop.QuitClosure());
    manager()->AddObserver(observer.get());
    power_monitor_source_->SetOnBatteryPower(true);
    run_loop.Run();
    manager()->RemoveObserver(observer.get());
  }

  EXPECT_TRUE(manager()->IsBatterySaverActive());
  EXPECT_TRUE(throttling_enabled());

  {
    base::RunLoop run_loop;
    std::unique_ptr<QuitRunLoopOnBSMChangeObserver> observer =
        std::make_unique<QuitRunLoopOnBSMChangeObserver>(
            run_loop.QuitClosure());
    manager()->AddObserver(observer.get());
    power_monitor_source_->SetOnBatteryPower(false);
    run_loop.Run();
    manager()->RemoveObserver(observer.get());
  }

  EXPECT_FALSE(manager()->IsBatterySaverActive());
  EXPECT_FALSE(throttling_enabled());

  // Change mode, go back on battery power, then reswitch to kEnabledOnBattery.
  // BSM should activate right away.
  local_state_.SetInteger(
      performance_manager::user_tuning::prefs::kBatterySaverModeState,
      static_cast<int>(performance_manager::user_tuning::prefs::
                           BatterySaverModeState::kDisabled));
  EXPECT_FALSE(manager()->IsBatterySaverActive());
  EXPECT_FALSE(throttling_enabled());

  {
    base::RunLoop run_loop;
    std::unique_ptr<QuitRunLoopOnPowerStateChangeObserver> observer =
        std::make_unique<QuitRunLoopOnPowerStateChangeObserver>(
            run_loop.QuitClosure());
    manager()->AddObserver(observer.get());
    power_monitor_source_->SetOnBatteryPower(true);
    run_loop.Run();
    manager()->RemoveObserver(observer.get());
  }

  local_state_.SetInteger(
      performance_manager::user_tuning::prefs::kBatterySaverModeState,
      static_cast<int>(performance_manager::user_tuning::prefs::
                           BatterySaverModeState::kEnabledOnBattery));
  EXPECT_TRUE(manager()->IsBatterySaverActive());
  EXPECT_TRUE(throttling_enabled());
}

}  // namespace performance_manager::user_tuning
