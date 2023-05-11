// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/public/user_tuning/user_performance_tuning_manager.h"

#include <tuple>
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
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
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

  raw_ptr<base::test::TestSamplingEventSource> sampling_source_;
  raw_ptr<base::test::TestBatteryLevelProvider> battery_level_provider_;
  raw_ptr<FakeHighEfficiencyModeDelegate> high_efficiency_mode_delegate_;
  std::unique_ptr<base::BatteryStateSampler> battery_sampler_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ScopedFakePowerManagerClientLifetime fake_power_manager_client_lifetime_;
#endif
  raw_ptr<FakePowerMonitorSource> power_monitor_source_;
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

TEST_F(UserPerformanceTuningManagerTest, SetDefaultTimeBeforeDiscardPref) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      performance_manager::user_tuning::UserPerformanceTuningManager::
          kTimeBeforeDiscardInMinutesSwitch,
      "5");
  StartManager();

  EXPECT_EQ(5, local_state_.GetInteger(
                   performance_manager::user_tuning::prefs::
                       kHighEfficiencyModeTimeBeforeDiscardInMinutes));
  EXPECT_THAT(high_efficiency_mode_delegate_->GetLastTimeBeforeDiscard(),
              Optional(base::Minutes(5)));
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
  feature_list.InitAndEnableFeature(
      performance_manager::features::kUseDeviceBatterySaverChromeOS);

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
  feature_list.InitAndEnableFeature(
      performance_manager::features::kUseDeviceBatterySaverChromeOS);

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

// States of the HeuristicMemorySaver and MultistateMode features to test, with
// expected outcomes.
struct PrefTestFeatureState {
  // Whether the MultistateMode feature is enabled.
  bool is_multistate_enabled;

  // Whether the HeuristicMemorySaver feature is enabled.
  bool is_heuristic_memory_saver_enabled;

  // The expected result when the pref is set to kEnabled with these params.
  HighEfficiencyModeState expected_enabled_state;

  // The expected result when the pref is set to kEnabledOnTimer with these
  // params.
  HighEfficiencyModeState expected_enabled_on_timer_state;
};

// List of feature states to test, not including the ForceHeuristicMemorySaver
// feature. These include the expected states that should be passed to
// ToggleHighEfficiencyMode when ForceHeuristicMemorySaver is disabled.
constexpr PrefTestFeatureState kNonForcedPrefTestFeatureStates[] = {
    // If multistate is off, both "enabled" states are controlled by the
    // HeuristicMemorySaver feature.
    {
        .is_multistate_enabled = false,
        .is_heuristic_memory_saver_enabled = false,
        .expected_enabled_state = HighEfficiencyModeState::kEnabledOnTimer,
        .expected_enabled_on_timer_state =
            HighEfficiencyModeState::kEnabledOnTimer,
    },
    {
        .is_multistate_enabled = false,
        .is_heuristic_memory_saver_enabled = true,
        .expected_enabled_state = HighEfficiencyModeState::kEnabled,
        .expected_enabled_on_timer_state = HighEfficiencyModeState::kEnabled,
    },
    // If multistate is on, the true "enabled" state is used regardless of the
    // HeuristicMemorySaver feature.
    {
        .is_multistate_enabled = true,
        .is_heuristic_memory_saver_enabled = false,
        .expected_enabled_state = HighEfficiencyModeState::kEnabled,
        .expected_enabled_on_timer_state =
            HighEfficiencyModeState::kEnabledOnTimer,
    },
    {
        .is_multistate_enabled = true,
        .is_heuristic_memory_saver_enabled = true,
        .expected_enabled_state = HighEfficiencyModeState::kEnabled,
        .expected_enabled_on_timer_state =
            HighEfficiencyModeState::kEnabledOnTimer,
    }};

// Test parameters are (feature_state, is_force_heuristic_memory_saver_enabled).
// Each feature state above is tested once with the ForceHeuristicMemorySaver
// feature enabled and once with it disabled.
class UserPerformanceTuningManagerPrefTest
    : public UserPerformanceTuningManagerTest,
      public ::testing::WithParamInterface<
          std::tuple<PrefTestFeatureState, bool>> {
 protected:
  void SetUp() override {
    UserPerformanceTuningManagerTest::SetUp();

    std::tie(feature_state_, is_force_heuristic_memory_saver_enabled_) =
        GetParam();

    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;
    if (feature_state_.is_multistate_enabled) {
      enabled_features.push_back(features::kHighEfficiencyMultistateMode);
    } else {
      disabled_features.push_back(features::kHighEfficiencyMultistateMode);
    }
    if (feature_state_.is_heuristic_memory_saver_enabled) {
      enabled_features.push_back(features::kHeuristicMemorySaver);
    } else {
      disabled_features.push_back(features::kHeuristicMemorySaver);
    }
    if (is_force_heuristic_memory_saver_enabled_) {
      enabled_features.push_back(features::kForceHeuristicMemorySaver);
    } else {
      disabled_features.push_back(features::kForceHeuristicMemorySaver);
    }
    feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  base::Value ValueForState(HighEfficiencyModeState state) {
    return base::Value(static_cast<int>(state));
  }

  PrefTestFeatureState feature_state_;
  bool is_force_heuristic_memory_saver_enabled_;
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         UserPerformanceTuningManagerPrefTest,
                         Combine(ValuesIn(kNonForcedPrefTestFeatureStates),
                                 Bool()));

TEST_P(UserPerformanceTuningManagerPrefTest, OnHighEfficiencyModePrefChanged) {
  StartManager();

  // If the ForceHeuristicMemorySaver feature is overriding the pref, the final
  // state should always be based on the HeuristicMemorySaver feature.
  absl::optional<HighEfficiencyModeState> expected_overridden_state;
  if (is_force_heuristic_memory_saver_enabled_) {
    expected_overridden_state = feature_state_.is_heuristic_memory_saver_enabled
                                    ? HighEfficiencyModeState::kEnabled
                                    : HighEfficiencyModeState::kDisabled;
  }

  local_state_.SetUserPref(prefs::kHighEfficiencyModeState,
                           ValueForState(HighEfficiencyModeState::kDisabled));
  EXPECT_THAT(high_efficiency_mode_delegate_->GetLastState(),
              Optional(expected_overridden_state.value_or(
                  HighEfficiencyModeState::kDisabled)));

  high_efficiency_mode_delegate_->ClearLastState();

  local_state_.SetUserPref(prefs::kHighEfficiencyModeState,
                           ValueForState(HighEfficiencyModeState::kEnabled));
  EXPECT_THAT(high_efficiency_mode_delegate_->GetLastState(),
              Optional(expected_overridden_state.value_or(
                  feature_state_.expected_enabled_state)));
  high_efficiency_mode_delegate_->ClearLastState();

  local_state_.SetUserPref(
      prefs::kHighEfficiencyModeState,
      ValueForState(HighEfficiencyModeState::kEnabledOnTimer));
  EXPECT_THAT(high_efficiency_mode_delegate_->GetLastState(),
              Optional(expected_overridden_state.value_or(
                  feature_state_.expected_enabled_on_timer_state)));
  high_efficiency_mode_delegate_->ClearLastState();
}

TEST_P(UserPerformanceTuningManagerPrefTest,
       OnHighEfficiencyModePrefChangedManaged) {
  StartManager();

  // Since the pref is managed, ForceHeuristicMemorySaver is not allowed to
  // override it, so ignore `is_force_heuristic_memory_saver_enabled_`.
  local_state_.SetManagedPref(
      prefs::kHighEfficiencyModeState,
      ValueForState(HighEfficiencyModeState::kDisabled));
  EXPECT_THAT(high_efficiency_mode_delegate_->GetLastState(),
              Optional(HighEfficiencyModeState::kDisabled));
  high_efficiency_mode_delegate_->ClearLastState();

  local_state_.SetManagedPref(prefs::kHighEfficiencyModeState,
                              ValueForState(HighEfficiencyModeState::kEnabled));
  EXPECT_THAT(high_efficiency_mode_delegate_->GetLastState(),
              Optional(feature_state_.expected_enabled_state));
  high_efficiency_mode_delegate_->ClearLastState();

  local_state_.SetManagedPref(
      prefs::kHighEfficiencyModeState,
      ValueForState(HighEfficiencyModeState::kEnabledOnTimer));
  EXPECT_THAT(high_efficiency_mode_delegate_->GetLastState(),
              Optional(feature_state_.expected_enabled_on_timer_state));
  high_efficiency_mode_delegate_->ClearLastState();
}

}  // namespace performance_manager::user_tuning
