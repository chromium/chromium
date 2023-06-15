// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/public/user_tuning/user_performance_tuning_manager.h"

#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/power_monitor/battery_state_sampler.h"
#include "base/power_monitor/power_monitor.h"
#include "base/power_monitor/power_monitor_source.h"
#include "base/run_loop.h"
#include "base/test/power_monitor_test_utils.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/performance_manager/test_support/fake_frame_throttling_delegate.h"
#include "chrome/browser/performance_manager/test_support/fake_high_efficiency_mode_delegate.h"
#include "chrome/browser/performance_manager/test_support/fake_power_monitor_source.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_object_proxy.h"
#include "dbus/object_path.h"
#endif

namespace performance_manager::user_tuning {
namespace {

using HighEfficiencyModeState = prefs::HighEfficiencyModeState;
using ::testing::Bool;
using ::testing::Combine;
using ::testing::Optional;
using ::testing::ValuesIn;

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

class MockObserver : public performance_manager::user_tuning::
                         UserPerformanceTuningManager::Observer {
 public:
  MOCK_METHOD0(OnBatteryThresholdReached, void());
  MOCK_METHOD1(OnDeviceHasBatteryChanged, void(bool));
};

base::BatteryLevelProvider::BatteryState CreateBatteryState(
    bool under_threshold) {
  return {
      .battery_count = 1,
      .is_external_power_connected = false,
      .current_capacity = (under_threshold ? 10 : 30),
      .full_charged_capacity = 100,
      .charge_unit = base::BatteryLevelProvider::BatteryLevelUnit::kRelative,
      .capture_time = base::TimeTicks::Now()};
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
class ScopedFakePowerManagerClientLifetime {
 public:
  ScopedFakePowerManagerClientLifetime() {
    chromeos::PowerManagerClient::InitializeFake();
  }

  ~ScopedFakePowerManagerClientLifetime() {
    chromeos::PowerManagerClient::Shutdown();
  }
};
#endif

}  // namespace

class UserPerformanceTuningManagerTest : public ::testing::Test {
 public:
  void SetUp() override {
    auto source = std::make_unique<FakePowerMonitorSource>();
    power_monitor_source_ = source.get();
    base::PowerMonitor::Initialize(std::move(source));

    performance_manager::user_tuning::prefs::RegisterLocalStatePrefs(
        local_state_.registry());
  }

  void StartManager() {
    auto test_sampling_event_source =
        std::make_unique<base::test::TestSamplingEventSource>();
    auto test_battery_level_provider =
        std::make_unique<base::test::TestBatteryLevelProvider>();
    auto fake_high_efficiency_mode_delegate =
        std::make_unique<FakeHighEfficiencyModeDelegate>();

    sampling_source_ = test_sampling_event_source.get();
    battery_level_provider_ = test_battery_level_provider.get();
    high_efficiency_mode_delegate_ = fake_high_efficiency_mode_delegate.get();

    battery_sampler_ = std::make_unique<base::BatteryStateSampler>(
        std::move(test_sampling_event_source),
        std::move(test_battery_level_provider));

    manager_.reset(new UserPerformanceTuningManager(
        &local_state_, nullptr,
        std::make_unique<FakeFrameThrottlingDelegate>(&throttling_enabled_),
        std::move(fake_high_efficiency_mode_delegate)));
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

  raw_ptr<base::test::TestSamplingEventSource, DanglingUntriaged>
      sampling_source_;
  raw_ptr<base::test::TestBatteryLevelProvider, DanglingUntriaged>
      battery_level_provider_;
  raw_ptr<FakeHighEfficiencyModeDelegate, DanglingUntriaged>
      high_efficiency_mode_delegate_;
  std::unique_ptr<base::BatteryStateSampler> battery_sampler_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ScopedFakePowerManagerClientLifetime fake_power_manager_client_lifetime_;
#endif
  raw_ptr<FakePowerMonitorSource, DanglingUntriaged> power_monitor_source_;
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
}

TEST_F(UserPerformanceTuningManagerTest,
       TemporaryBatterySaverTurnsOffWhenPlugged) {
  StartManager();
  EXPECT_FALSE(manager()->IsBatterySaverActive());
  EXPECT_FALSE(throttling_enabled());

  // Test the flag is cleared when the device is plugged in.
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
                           BatterySaverModeState::kEnabled));
  EXPECT_TRUE(manager()->IsBatterySaverActive());
  EXPECT_TRUE(throttling_enabled());

  manager()->SetTemporaryBatterySaverDisabledForSession(true);
  EXPECT_FALSE(manager()->IsBatterySaverActive());
  EXPECT_FALSE(throttling_enabled());

  {
    base::RunLoop run_loop;
    std::unique_ptr<QuitRunLoopOnPowerStateChangeObserver> observer =
        std::make_unique<QuitRunLoopOnPowerStateChangeObserver>(
            run_loop.QuitClosure());
    manager()->AddObserver(observer.get());
    power_monitor_source_->SetOnBatteryPower(false);
    run_loop.Run();
    manager()->RemoveObserver(observer.get());
  }
  EXPECT_FALSE(manager()->IsBatterySaverModeDisabledForSession());
  EXPECT_TRUE(manager()->IsBatterySaverActive());
  EXPECT_TRUE(throttling_enabled());
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

TEST_F(UserPerformanceTuningManagerTest, LowBatteryThresholdRaised) {
  local_state_.SetInteger(
      performance_manager::user_tuning::prefs::kBatterySaverModeState,
      static_cast<int>(performance_manager::user_tuning::prefs::
                           BatterySaverModeState::kDisabled));
  StartManager();
  EXPECT_FALSE(manager()->IsBatterySaverActive());
  EXPECT_FALSE(throttling_enabled());

  MockObserver obs;
  manager()->AddObserver(&obs);
  EXPECT_CALL(obs, OnBatteryThresholdReached()).Times(1);

  battery_level_provider_->SetBatteryState(
      CreateBatteryState(/*under_threshold=*/true));
  sampling_source_->SimulateEvent();

  // A new sample under the threshold won't trigger the event again
  sampling_source_->SimulateEvent();
}

TEST_F(UserPerformanceTuningManagerTest, BSMEnabledUnderThreshold) {
  local_state_.SetInteger(
      performance_manager::user_tuning::prefs::kBatterySaverModeState,
      static_cast<int>(performance_manager::user_tuning::prefs::
                           BatterySaverModeState::kEnabledBelowThreshold));
  StartManager();
  EXPECT_FALSE(manager()->IsBatterySaverActive());
  EXPECT_FALSE(throttling_enabled());

  // If the device is not on battery, getting a "below threshold" sample doesn't
  // enable BSM
  battery_level_provider_->SetBatteryState(
      CreateBatteryState(/*under_threshold=*/true));
  sampling_source_->SimulateEvent();
  EXPECT_FALSE(manager()->IsBatterySaverActive());
  EXPECT_FALSE(throttling_enabled());

  // We're below threshold and the device goes on battery, BSM is enabled
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

  EXPECT_TRUE(manager()->IsBatterySaverActive());
  EXPECT_TRUE(throttling_enabled());

  // The device is plugged in, BSM deactivates. Then it's charged above
  // threshold, unplugged, and the battery is drained below threshold, which
  // reactivates BSM.
  {
    base::RunLoop run_loop;
    std::unique_ptr<QuitRunLoopOnPowerStateChangeObserver> observer =
        std::make_unique<QuitRunLoopOnPowerStateChangeObserver>(
            run_loop.QuitClosure());
    manager()->AddObserver(observer.get());
    power_monitor_source_->SetOnBatteryPower(false);
    run_loop.Run();
    manager()->RemoveObserver(observer.get());
  }
  EXPECT_FALSE(manager()->IsBatterySaverActive());
  EXPECT_FALSE(throttling_enabled());

  battery_level_provider_->SetBatteryState(
      CreateBatteryState(/*under_threshold=*/false));
  sampling_source_->SimulateEvent();

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

  EXPECT_FALSE(manager()->IsBatterySaverActive());
  EXPECT_FALSE(throttling_enabled());

  battery_level_provider_->SetBatteryState(
      CreateBatteryState(/*under_threshold=*/true));
  sampling_source_->SimulateEvent();

  EXPECT_TRUE(manager()->IsBatterySaverActive());
  EXPECT_TRUE(throttling_enabled());
}

TEST_F(UserPerformanceTuningManagerTest, HasBatteryChanged) {
  local_state_.SetInteger(
      performance_manager::user_tuning::prefs::kBatterySaverModeState,
      static_cast<int>(performance_manager::user_tuning::prefs::
                           BatterySaverModeState::kEnabledBelowThreshold));
  StartManager();
  EXPECT_FALSE(manager()->DeviceHasBattery());

  MockObserver obs;
  manager()->AddObserver(&obs);

  // Expect OnDeviceHasBatteryChanged to be called only once if a battery state
  // without a battery is received, followed by a state with a battery.
  EXPECT_CALL(obs, OnDeviceHasBatteryChanged(true));
  battery_level_provider_->SetBatteryState(
      base::BatteryLevelProvider::BatteryState({
          .battery_count = 0,
      }));
  sampling_source_->SimulateEvent();
  EXPECT_FALSE(manager()->DeviceHasBattery());
  battery_level_provider_->SetBatteryState(
      base::BatteryLevelProvider::BatteryState({
          .battery_count = 1,
          .current_capacity = 100,
          .full_charged_capacity = 100,
      }));
  sampling_source_->SimulateEvent();
  EXPECT_TRUE(manager()->DeviceHasBattery());

  // Simulate the battery being disconnected, OnDeviceHasBatteryChanged should
  // be called once.
  EXPECT_CALL(obs, OnDeviceHasBatteryChanged(false));
  battery_level_provider_->SetBatteryState(
      base::BatteryLevelProvider::BatteryState({
          .battery_count = 0,
      }));
  sampling_source_->SimulateEvent();
  EXPECT_FALSE(manager()->DeviceHasBattery());
}

TEST_F(UserPerformanceTuningManagerTest,
       BatteryPercentageWithoutFullChargedCapacity) {
  local_state_.SetInteger(
      performance_manager::user_tuning::prefs::kBatterySaverModeState,
      static_cast<int>(performance_manager::user_tuning::prefs::
                           BatterySaverModeState::kEnabledBelowThreshold));
  StartManager();

  battery_level_provider_->SetBatteryState(
      base::BatteryLevelProvider::BatteryState({
          .battery_count = 0,
          .current_capacity = 100,
          .full_charged_capacity = 0,
      }));
  sampling_source_->SimulateEvent();
  EXPECT_EQ(100, manager()->SampledBatteryPercentage());
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(UserPerformanceTuningManagerTest, ManagedFromPowerManager) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(ash::features::kBatterySaver);

  StartManager();
  EXPECT_FALSE(manager()->IsBatterySaverActive());
  EXPECT_FALSE(throttling_enabled());

  base::RunLoop run_loop;
  std::unique_ptr<QuitRunLoopOnBSMChangeObserver> observer =
      std::make_unique<QuitRunLoopOnBSMChangeObserver>(run_loop.QuitClosure());
  manager()->AddObserver(observer.get());

  // Request to enable PowerManager's BSM
  power_manager::SetBatterySaverModeStateRequest proto;
  proto.set_enabled(true);
  chromeos::PowerManagerClient::Get()->SetBatterySaverModeState(proto);

  run_loop.Run();
  manager()->RemoveObserver(observer.get());

  EXPECT_TRUE(manager()->IsBatterySaverActive());
  EXPECT_TRUE(throttling_enabled());
}

TEST_F(UserPerformanceTuningManagerTest,
       StartsEnabledIfAlreadyEnabledInPowerManager) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(ash::features::kBatterySaver);

  // Request to enable PowerManager's BSM
  power_manager::SetBatterySaverModeStateRequest proto;
  proto.set_enabled(true);
  chromeos::PowerManagerClient::Get()->SetBatterySaverModeState(proto);

  StartManager();

  // It's fine to install the observer after the manager is created, as long as
  // it's done before the runloop runs
  base::RunLoop run_loop;
  std::unique_ptr<QuitRunLoopOnBSMChangeObserver> observer =
      std::make_unique<QuitRunLoopOnBSMChangeObserver>(run_loop.QuitClosure());
  manager()->AddObserver(observer.get());

  run_loop.Run();
  manager()->RemoveObserver(observer.get());

  EXPECT_TRUE(manager()->IsBatterySaverActive());
  EXPECT_TRUE(throttling_enabled());
}
#endif

struct PrefTestParams {
  // Whether the HeuristicMemorySaver feature is enabled.
  bool is_heuristic_memory_saver_enabled = false;

  // State to store in the kHighEfficiencyModeState pref.
  HighEfficiencyModeState pref_state = HighEfficiencyModeState::kDisabled;

  // Expected state passed to ToggleHighEfficiencyMode().
  HighEfficiencyModeState expected_state = HighEfficiencyModeState::kDisabled;

  // Expected state passed to ToggleHighEfficiencyMode() when
  // ForceHeuristicMemorySaver is enabled and not ignored.
  HighEfficiencyModeState expected_state_with_force =
      HighEfficiencyModeState::kDisabled;
};

class UserPerformanceTuningManagerPrefTest
    : public UserPerformanceTuningManagerTest,
      public ::testing::WithParamInterface<PrefTestParams> {
 protected:
  void InstallFeatures(bool is_force_heuristic_memory_saver_enabled,
                       bool is_multistate_enabled = false) {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;
    if (GetParam().is_heuristic_memory_saver_enabled) {
      enabled_features.push_back(features::kHeuristicMemorySaver);
    } else {
      disabled_features.push_back(features::kHeuristicMemorySaver);
    }
    if (is_force_heuristic_memory_saver_enabled) {
      enabled_features.push_back(features::kForceHeuristicMemorySaver);
    } else {
      disabled_features.push_back(features::kForceHeuristicMemorySaver);
    }
    if (is_multistate_enabled) {
      enabled_features.push_back(features::kHighEfficiencyMultistateMode);
    } else {
      disabled_features.push_back(features::kHighEfficiencyMultistateMode);
    }
    feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  base::Value ValueForPrefState() const {
    return base::Value(static_cast<int>(GetParam().pref_state));
  }

  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    UserPerformanceTuningManagerPrefTest,
    ::testing::Values(
        // With HeuristicMemorySaver disabled, the timer policy is used whenever
        // HighEfficiencyMode is enabled. ForceHeuristicMemorySaver forces
        // HighEfficiencyMode to OFF.
        PrefTestParams{
            .is_heuristic_memory_saver_enabled = false,
            .pref_state = HighEfficiencyModeState::kDisabled,
            .expected_state = HighEfficiencyModeState::kDisabled,
            .expected_state_with_force = HighEfficiencyModeState::kDisabled,
        },
        PrefTestParams{
            .is_heuristic_memory_saver_enabled = false,
            .pref_state = HighEfficiencyModeState::kEnabled,
            .expected_state = HighEfficiencyModeState::kEnabledOnTimer,
            .expected_state_with_force = HighEfficiencyModeState::kDisabled,
        },
        PrefTestParams{
            .is_heuristic_memory_saver_enabled = false,
            .pref_state = HighEfficiencyModeState::kEnabledOnTimer,
            .expected_state = HighEfficiencyModeState::kEnabledOnTimer,
            .expected_state_with_force = HighEfficiencyModeState::kDisabled,
        },
        // With HeuristicMemorySaver enabled, the heuristic policy is used
        // whenever HighEfficiencyMode is enabled. ForceHeuristicMemorySaver
        // forces HighEfficiencyMode to ON.
        PrefTestParams{
            .is_heuristic_memory_saver_enabled = true,
            .pref_state = HighEfficiencyModeState::kDisabled,
            .expected_state = HighEfficiencyModeState::kDisabled,
            .expected_state_with_force = HighEfficiencyModeState::kEnabled,
        },
        PrefTestParams{
            .is_heuristic_memory_saver_enabled = true,
            .pref_state = HighEfficiencyModeState::kEnabled,
            .expected_state = HighEfficiencyModeState::kEnabled,
            .expected_state_with_force = HighEfficiencyModeState::kEnabled,
        },
        PrefTestParams{
            .is_heuristic_memory_saver_enabled = true,
            .pref_state = HighEfficiencyModeState::kEnabledOnTimer,
            .expected_state = HighEfficiencyModeState::kEnabled,
            .expected_state_with_force = HighEfficiencyModeState::kEnabled,
        }));

TEST_P(UserPerformanceTuningManagerPrefTest, OnPrefChanged) {
  InstallFeatures(/*is_force_heuristic_memory_saver_enabled=*/false);
  StartManager();
  local_state_.SetUserPref(prefs::kHighEfficiencyModeState,
                           ValueForPrefState());
  EXPECT_THAT(high_efficiency_mode_delegate_->GetLastState(),
              Optional(GetParam().expected_state));
}

TEST_P(UserPerformanceTuningManagerPrefTest, OnPrefChangedWithForce) {
  InstallFeatures(/*is_force_heuristic_memory_saver_enabled=*/true);
  StartManager();
  local_state_.SetUserPref(prefs::kHighEfficiencyModeState,
                           ValueForPrefState());
  EXPECT_THAT(high_efficiency_mode_delegate_->GetLastState(),
              Optional(GetParam().expected_state_with_force));
}

TEST_P(UserPerformanceTuningManagerPrefTest, OnPrefChangedMultistate) {
  InstallFeatures(/*is_force_heuristic_memory_saver_enabled=*/false,
                  /*is_multistate_enabled=*/true);
  StartManager();

  // When the HighEfficiencyMultistateMode feature is enabled, all states should
  // be passed to ToggleHighEfficiencyMode() unchanged.
  local_state_.SetUserPref(prefs::kHighEfficiencyModeState,
                           ValueForPrefState());
  EXPECT_THAT(high_efficiency_mode_delegate_->GetLastState(),
              Optional(GetParam().pref_state));
}

TEST_P(UserPerformanceTuningManagerPrefTest, OnPrefChangedMultistateWithForce) {
  InstallFeatures(/*is_force_heuristic_memory_saver_enabled=*/true,
                  /*is_multistate_enabled=*/true);
  StartManager();

  // When the HighEfficiencyMultistateMode feature is enabled, all states should
  // be passed to ToggleHighEfficiencyMode() unchanged, even when
  // ForceHeuristicMemorySaver is enabled.
  local_state_.SetUserPref(prefs::kHighEfficiencyModeState,
                           ValueForPrefState());
  EXPECT_THAT(high_efficiency_mode_delegate_->GetLastState(),
              Optional(GetParam().pref_state));
}

TEST_P(UserPerformanceTuningManagerPrefTest, OnManagedPrefChanged) {
  InstallFeatures(/*is_force_heuristic_memory_saver_enabled=*/true);
  StartManager();

  // Since the pref is managed, ForceHeuristicMemorySaver is not allowed to
  // override it, so use the expectation without force.
  local_state_.SetManagedPref(prefs::kHighEfficiencyModeState,
                              ValueForPrefState());
  EXPECT_THAT(high_efficiency_mode_delegate_->GetLastState(),
              Optional(GetParam().expected_state));
}

}  // namespace performance_manager::user_tuning
