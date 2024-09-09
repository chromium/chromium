// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/public/user_tuning/battery_saver_mode_manager.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/power_monitor/battery_state_sampler.h"
#include "base/power_monitor/power_monitor.h"
#include "base/power_monitor/power_monitor_source.h"
#include "base/power_monitor/power_observer.h"
#include "base/run_loop.h"
#include "base/test/power_monitor_test_utils.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/performance_manager/test_support/fake_child_process_tuning_delegate.h"
#include "chrome/browser/performance_manager/test_support/fake_frame_throttling_delegate.h"
#include "chrome/browser/performance_manager/test_support/fake_freezing_delegate.h"
#include "chrome/browser/performance_manager/test_support/fake_power_monitor_source.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/common/content_features.h"
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

class QuitRunLoopObserverBase : public performance_manager::user_tuning::
                                    BatterySaverModeManager::Observer {
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

  // BatterySaverModeManager::Observer implementation:
  void OnBatterySaverActiveChanged(bool) override { Quit(); }
};

class QuitRunLoopOnPowerStateChangeObserver : public QuitRunLoopObserverBase {
 public:
  explicit QuitRunLoopOnPowerStateChangeObserver(
      base::RepeatingClosure quit_closure)
      : QuitRunLoopObserverBase(quit_closure) {}

  ~QuitRunLoopOnPowerStateChangeObserver() override = default;

  // BatterySaverModeManager::Observer implementation:
  void OnExternalPowerConnectedChanged(bool) override { Quit(); }
};

class MockObserver : public performance_manager::user_tuning::
                         BatterySaverModeManager::Observer {
 public:
  MOCK_METHOD0(OnBatteryThresholdReached, void());
  MOCK_METHOD1(OnDeviceHasBatteryChanged, void(bool));
};

#if !BUILDFLAG(IS_CHROMEOS_ASH)

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

#else  // BUILDFLAG(IS_CHROMEOS_ASH)

class ScopedFakePowerManagerClientLifetime {
 public:
  ScopedFakePowerManagerClientLifetime() {
    chromeos::PowerManagerClient::InitializeFake();
  }

  ~ScopedFakePowerManagerClientLifetime() {
    chromeos::PowerManagerClient::Shutdown();
  }
};

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace

class BatterySaverModeManagerTest : public ::testing::Test {
 public:
  void SetUp() override {
    auto source = std::make_unique<FakePowerMonitorSource>();
    power_monitor_source_ = source.get();
    base::PowerMonitor::GetInstance()->Initialize(std::move(source));

    performance_manager::user_tuning::prefs::RegisterLocalStatePrefs(
        local_state_.registry());
  }

  void StartManager() {
    auto test_sampling_event_source =
        std::make_unique<base::test::TestSamplingEventSource>();
    auto test_battery_level_provider =
        std::make_unique<base::test::TestBatteryLevelProvider>();

    sampling_source_ = test_sampling_event_source.get();
    battery_level_provider_ = test_battery_level_provider.get();

    battery_sampler_ = std::make_unique<base::BatteryStateSampler>(
        std::move(test_sampling_event_source),
        std::move(test_battery_level_provider));

    manager_.reset(new BatterySaverModeManager(
        &local_state_,
        std::make_unique<FakeFrameThrottlingDelegate>(&throttling_enabled_),
        std::make_unique<FakeChildProcessTuningDelegate>(
            &child_process_tuning_enabled_),
        std::make_unique<FakeFreezingDelegate>(&freezing_enabled_)));
    manager()->Start();
  }

  void TearDown() override {
    base::PowerMonitor::GetInstance()->ShutdownForTesting();
  }

  BatterySaverModeManager* manager() {
    return BatterySaverModeManager::GetInstance();
  }
  bool throttling_enabled() const { return throttling_enabled_; }
  bool child_process_tuning_enabled() const {
    return child_process_tuning_enabled_;
  }
  bool freezing_enabled() const { return freezing_enabled_; }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  TestingPrefServiceSimple local_state_;

  raw_ptr<base::test::TestSamplingEventSource, DanglingUntriaged>
      sampling_source_;
  raw_ptr<base::test::TestBatteryLevelProvider, DanglingUntriaged>
      battery_level_provider_;
  std::unique_ptr<base::BatteryStateSampler> battery_sampler_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ScopedFakePowerManagerClientLifetime fake_power_manager_client_lifetime_;
#endif
  raw_ptr<FakePowerMonitorSource, DanglingUntriaged> power_monitor_source_;
  bool throttling_enabled_ = false;
  bool child_process_tuning_enabled_ = false;
  bool freezing_enabled_ = false;
  std::unique_ptr<BatterySaverModeManager> manager_;
};

// Battery Saver is controlled by the OS on ChromeOS
#if !BUILDFLAG(IS_CHROMEOS_ASH)

TEST_F(BatterySaverModeManagerTest, TemporaryBatterySaver) {
  StartManager();
  EXPECT_FALSE(manager()->IsBatterySaverActive());
  EXPECT_FALSE(throttling_enabled());
  EXPECT_FALSE(child_process_tuning_enabled());
  EXPECT_FALSE(freezing_enabled());

  local_state_.SetInteger(
      performance_manager::user_tuning::prefs::kBatterySaverModeState,
      static_cast<int>(performance_manager::user_tuning::prefs::
                           BatterySaverModeState::kEnabled));

  EXPECT_TRUE(manager()->IsBatterySaverModeEnabled());
  EXPECT_TRUE(manager()->IsBatterySaverActive());
  EXPECT_TRUE(throttling_enabled());
  EXPECT_TRUE(child_process_tuning_enabled());
  EXPECT_TRUE(freezing_enabled());

  manager()->SetTemporaryBatterySaverDisabledForSession(true);
  EXPECT_TRUE(manager()->IsBatterySaverModeEnabled());
  EXPECT_FALSE(manager()->IsBatterySaverActive());
  EXPECT_FALSE(throttling_enabled());
  EXPECT_FALSE(child_process_tuning_enabled());
  EXPECT_FALSE(freezing_enabled());

  manager()->SetTemporaryBatterySaverDisabledForSession(false);
  EXPECT_TRUE(manager()->IsBatterySaverModeEnabled());
  EXPECT_TRUE(manager()->IsBatterySaverActive());
  EXPECT_TRUE(throttling_enabled());
  EXPECT_TRUE(child_process_tuning_enabled());
  EXPECT_TRUE(freezing_enabled());

  // Changing the pref resets the "disabled for session" flag.
  manager()->SetTemporaryBatterySaverDisabledForSession(true);
  local_state_.SetInteger(
      performance_manager::user_tuning::prefs::kBatterySaverModeState,
      static_cast<int>(performance_manager::user_tuning::prefs::
                           BatterySaverModeState::kEnabledOnBattery));
  EXPECT_FALSE(manager()->IsBatterySaverModeDisabledForSession());
}

TEST_F(BatterySaverModeManagerTest, TemporaryBatterySaverTurnsOffWhenPlugged) {
  StartManager();
  EXPECT_FALSE(manager()->IsBatterySaverActive());
  EXPECT_FALSE(throttling_enabled());
  EXPECT_FALSE(child_process_tuning_enabled());
  EXPECT_FALSE(freezing_enabled());

  // Test the flag is cleared when the device is plugged in.
  {
    base::RunLoop run_loop;
    std::unique_ptr<QuitRunLoopOnPowerStateChangeObserver> observer =
        std::make_unique<QuitRunLoopOnPowerStateChangeObserver>(
            run_loop.QuitClosure());
    manager()->AddObserver(observer.get());
    power_monitor_source_->SetBatteryPowerStatus(
        base::PowerStateObserver::BatteryPowerStatus::kBatteryPower);
    run_loop.Run();
    manager()->RemoveObserver(observer.get());
  }
  local_state_.SetInteger(
      performance_manager::user_tuning::prefs::kBatterySaverModeState,
      static_cast<int>(performance_manager::user_tuning::prefs::
                           BatterySaverModeState::kEnabled));
  EXPECT_TRUE(manager()->IsBatterySaverActive());
  EXPECT_TRUE(throttling_enabled());
  EXPECT_TRUE(child_process_tuning_enabled());
  EXPECT_TRUE(freezing_enabled());

  manager()->SetTemporaryBatterySaverDisabledForSession(true);
  EXPECT_FALSE(manager()->IsBatterySaverActive());
  EXPECT_FALSE(throttling_enabled());
  EXPECT_FALSE(child_process_tuning_enabled());
  EXPECT_FALSE(freezing_enabled());

  {
    base::RunLoop run_loop;
    std::unique_ptr<QuitRunLoopOnPowerStateChangeObserver> observer =
        std::make_unique<QuitRunLoopOnPowerStateChangeObserver>(
            run_loop.QuitClosure());
    manager()->AddObserver(observer.get());
    power_monitor_source_->SetBatteryPowerStatus(
        base::PowerStateObserver::BatteryPowerStatus::kExternalPower);
    run_loop.Run();
    manager()->RemoveObserver(observer.get());
  }
  EXPECT_FALSE(manager()->IsBatterySaverModeDisabledForSession());
  EXPECT_TRUE(manager()->IsBatterySaverActive());
  EXPECT_TRUE(throttling_enabled());
  EXPECT_TRUE(child_process_tuning_enabled());
  EXPECT_TRUE(freezing_enabled());
}

TEST_F(BatterySaverModeManagerTest, BatterySaverModePref) {
  StartManager();
  EXPECT_FALSE(manager()->IsBatterySaverActive());
  EXPECT_FALSE(throttling_enabled());
  EXPECT_FALSE(child_process_tuning_enabled());
  EXPECT_FALSE(freezing_enabled());

  local_state_.SetInteger(
      performance_manager::user_tuning::prefs::kBatterySaverModeState,
      static_cast<int>(performance_manager::user_tuning::prefs::
                           BatterySaverModeState::kEnabled));
  EXPECT_TRUE(manager()->IsBatterySaverModeEnabled());
  EXPECT_TRUE(manager()->IsBatterySaverActive());
  EXPECT_TRUE(throttling_enabled());
  EXPECT_TRUE(child_process_tuning_enabled());
  EXPECT_TRUE(freezing_enabled());

  local_state_.SetInteger(
      performance_manager::user_tuning::prefs::kBatterySaverModeState,
      static_cast<int>(performance_manager::user_tuning::prefs::
                           BatterySaverModeState::kDisabled));
  EXPECT_FALSE(manager()->IsBatterySaverModeEnabled());
  EXPECT_FALSE(manager()->IsBatterySaverActive());
  EXPECT_FALSE(throttling_enabled());
  EXPECT_FALSE(child_process_tuning_enabled());
  EXPECT_FALSE(freezing_enabled());
}

TEST_F(BatterySaverModeManagerTest, InvalidPrefInStore) {
  StartManager();
  local_state_.SetInteger(
      performance_manager::user_tuning::prefs::kBatterySaverModeState,
      static_cast<int>(performance_manager::user_tuning::prefs::
                           BatterySaverModeState::kEnabled));
  EXPECT_TRUE(manager()->IsBatterySaverActive());
  EXPECT_TRUE(throttling_enabled());
  EXPECT_TRUE(child_process_tuning_enabled());
  EXPECT_TRUE(freezing_enabled());

  local_state_.SetInteger(
      performance_manager::user_tuning::prefs::kBatterySaverModeState, -1);
  EXPECT_FALSE(manager()->IsBatterySaverActive());
  EXPECT_FALSE(throttling_enabled());
  EXPECT_FALSE(child_process_tuning_enabled());
  EXPECT_FALSE(freezing_enabled());

  local_state_.SetInteger(
      performance_manager::user_tuning::prefs::kBatterySaverModeState,
      static_cast<int>(performance_manager::user_tuning::prefs::
                           BatterySaverModeState::kDisabled) +
          1);
  EXPECT_FALSE(manager()->IsBatterySaverActive());
  EXPECT_FALSE(throttling_enabled());
  EXPECT_FALSE(child_process_tuning_enabled());
  EXPECT_FALSE(freezing_enabled());
}

TEST_F(BatterySaverModeManagerTest, EnabledOnBatteryPower) {
  StartManager();
  EXPECT_FALSE(manager()->IsBatterySaverActive());
  EXPECT_FALSE(throttling_enabled());
  EXPECT_FALSE(child_process_tuning_enabled());
  EXPECT_FALSE(freezing_enabled());

  local_state_.SetInteger(
      performance_manager::user_tuning::prefs::kBatterySaverModeState,
      static_cast<int>(performance_manager::user_tuning::prefs::
                           BatterySaverModeState::kEnabledOnBattery));
  EXPECT_FALSE(manager()->IsBatterySaverActive());
  EXPECT_FALSE(throttling_enabled());
  EXPECT_FALSE(child_process_tuning_enabled());
  EXPECT_FALSE(freezing_enabled());

  {
    base::RunLoop run_loop;
    std::unique_ptr<QuitRunLoopOnBSMChangeObserver> observer =
        std::make_unique<QuitRunLoopOnBSMChangeObserver>(
            run_loop.QuitClosure());
    manager()->AddObserver(observer.get());
    power_monitor_source_->SetBatteryPowerStatus(
        base::PowerStateObserver::BatteryPowerStatus::kBatteryPower);
    run_loop.Run();
    manager()->RemoveObserver(observer.get());
  }

  EXPECT_TRUE(manager()->IsBatterySaverActive());
  EXPECT_TRUE(throttling_enabled());
  EXPECT_TRUE(child_process_tuning_enabled());
  EXPECT_TRUE(freezing_enabled());

  {
    base::RunLoop run_loop;
    std::unique_ptr<QuitRunLoopOnBSMChangeObserver> observer =
        std::make_unique<QuitRunLoopOnBSMChangeObserver>(
            run_loop.QuitClosure());
    manager()->AddObserver(observer.get());
    power_monitor_source_->SetBatteryPowerStatus(
        base::PowerStateObserver::BatteryPowerStatus::kExternalPower);
    run_loop.Run();
    manager()->RemoveObserver(observer.get());
  }

  EXPECT_FALSE(manager()->IsBatterySaverActive());
  EXPECT_FALSE(throttling_enabled());
  EXPECT_FALSE(child_process_tuning_enabled());

  // Change mode, go back on battery power, then reswitch to kEnabledOnBattery.
  // BSM should activate right away.
  local_state_.SetInteger(
      performance_manager::user_tuning::prefs::kBatterySaverModeState,
      static_cast<int>(performance_manager::user_tuning::prefs::
                           BatterySaverModeState::kDisabled));
  EXPECT_FALSE(manager()->IsBatterySaverActive());
  EXPECT_FALSE(throttling_enabled());
  EXPECT_FALSE(child_process_tuning_enabled());
  EXPECT_FALSE(freezing_enabled());

  {
    base::RunLoop run_loop;
    std::unique_ptr<QuitRunLoopOnPowerStateChangeObserver> observer =
        std::make_unique<QuitRunLoopOnPowerStateChangeObserver>(
            run_loop.QuitClosure());
    manager()->AddObserver(observer.get());
    power_monitor_source_->SetBatteryPowerStatus(
        base::PowerStateObserver::BatteryPowerStatus::kBatteryPower);
    run_loop.Run();
    manager()->RemoveObserver(observer.get());
  }

  local_state_.SetInteger(
      performance_manager::user_tuning::prefs::kBatterySaverModeState,
      static_cast<int>(performance_manager::user_tuning::prefs::
                           BatterySaverModeState::kEnabledOnBattery));
  EXPECT_TRUE(manager()->IsBatterySaverActive());
  EXPECT_TRUE(throttling_enabled());
  EXPECT_TRUE(child_process_tuning_enabled());
  EXPECT_TRUE(freezing_enabled());
}

TEST_F(BatterySaverModeManagerTest, LowBatteryThresholdRaised) {
  local_state_.SetInteger(
      performance_manager::user_tuning::prefs::kBatterySaverModeState,
      static_cast<int>(performance_manager::user_tuning::prefs::
                           BatterySaverModeState::kDisabled));
  StartManager();
  EXPECT_FALSE(manager()->IsBatterySaverActive());
  EXPECT_FALSE(throttling_enabled());
  EXPECT_FALSE(child_process_tuning_enabled());
  EXPECT_FALSE(freezing_enabled());

  MockObserver obs;
  manager()->AddObserver(&obs);
  EXPECT_CALL(obs, OnBatteryThresholdReached()).Times(1);

  battery_level_provider_->SetBatteryState(
      CreateBatteryState(/*under_threshold=*/true));
  sampling_source_->SimulateEvent();

  // A new sample under the threshold won't trigger the event again
  sampling_source_->SimulateEvent();
}

TEST_F(BatterySaverModeManagerTest, BSMEnabledUnderThreshold) {
  local_state_.SetInteger(
      performance_manager::user_tuning::prefs::kBatterySaverModeState,
      static_cast<int>(performance_manager::user_tuning::prefs::
                           BatterySaverModeState::kEnabledBelowThreshold));
  StartManager();
  EXPECT_FALSE(manager()->IsBatterySaverActive());
  EXPECT_FALSE(throttling_enabled());
  EXPECT_FALSE(child_process_tuning_enabled());
  EXPECT_FALSE(freezing_enabled());

  // If the device is not on battery, getting a "below threshold" sample doesn't
  // enable BSM
  battery_level_provider_->SetBatteryState(
      CreateBatteryState(/*under_threshold=*/true));
  sampling_source_->SimulateEvent();
  EXPECT_FALSE(manager()->IsBatterySaverActive());
  EXPECT_FALSE(throttling_enabled());
  EXPECT_FALSE(child_process_tuning_enabled());
  EXPECT_FALSE(freezing_enabled());

  // We're below threshold and the device goes on battery, BSM is enabled
  {
    base::RunLoop run_loop;
    std::unique_ptr<QuitRunLoopOnPowerStateChangeObserver> observer =
        std::make_unique<QuitRunLoopOnPowerStateChangeObserver>(
            run_loop.QuitClosure());
    manager()->AddObserver(observer.get());
    power_monitor_source_->SetBatteryPowerStatus(
        base::PowerStateObserver::BatteryPowerStatus::kBatteryPower);
    run_loop.Run();
    manager()->RemoveObserver(observer.get());
  }

  EXPECT_TRUE(manager()->IsBatterySaverActive());
  EXPECT_TRUE(throttling_enabled());
  EXPECT_TRUE(child_process_tuning_enabled());
  EXPECT_TRUE(freezing_enabled());

  // The device is plugged in, BSM deactivates. Then it's charged above
  // threshold, unplugged, and the battery is drained below threshold, which
  // reactivates BSM.
  {
    base::RunLoop run_loop;
    std::unique_ptr<QuitRunLoopOnPowerStateChangeObserver> observer =
        std::make_unique<QuitRunLoopOnPowerStateChangeObserver>(
            run_loop.QuitClosure());
    manager()->AddObserver(observer.get());
    power_monitor_source_->SetBatteryPowerStatus(
        base::PowerStateObserver::BatteryPowerStatus::kExternalPower);
    run_loop.Run();
    manager()->RemoveObserver(observer.get());
  }
  EXPECT_FALSE(manager()->IsBatterySaverActive());
  EXPECT_FALSE(throttling_enabled());
  EXPECT_FALSE(child_process_tuning_enabled());
  EXPECT_FALSE(freezing_enabled());

  battery_level_provider_->SetBatteryState(
      CreateBatteryState(/*under_threshold=*/false));
  sampling_source_->SimulateEvent();

  {
    base::RunLoop run_loop;
    std::unique_ptr<QuitRunLoopOnPowerStateChangeObserver> observer =
        std::make_unique<QuitRunLoopOnPowerStateChangeObserver>(
            run_loop.QuitClosure());
    manager()->AddObserver(observer.get());
    power_monitor_source_->SetBatteryPowerStatus(
        base::PowerStateObserver::BatteryPowerStatus::kBatteryPower);
    run_loop.Run();
    manager()->RemoveObserver(observer.get());
  }

  EXPECT_FALSE(manager()->IsBatterySaverActive());
  EXPECT_FALSE(throttling_enabled());
  EXPECT_FALSE(child_process_tuning_enabled());
  EXPECT_FALSE(freezing_enabled());

  battery_level_provider_->SetBatteryState(
      CreateBatteryState(/*under_threshold=*/true));
  sampling_source_->SimulateEvent();

  EXPECT_TRUE(manager()->IsBatterySaverActive());
  EXPECT_TRUE(throttling_enabled());
  EXPECT_TRUE(child_process_tuning_enabled());
  EXPECT_TRUE(freezing_enabled());
}

TEST_F(BatterySaverModeManagerTest, HasBatteryChanged) {
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

TEST_F(BatterySaverModeManagerTest,
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

#else   // BUILDFLAG(IS_CHROMEOS_ASH)

TEST_F(BatterySaverModeManagerTest, ManagedFromPowerManager) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(ash::features::kBatterySaver);

  StartManager();
  EXPECT_FALSE(manager()->IsBatterySaverActive());
  EXPECT_FALSE(throttling_enabled());
  EXPECT_FALSE(child_process_tuning_enabled());
  EXPECT_FALSE(freezing_enabled());

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
  EXPECT_FALSE(manager()->IsBatterySaverModeEnabled());
  EXPECT_TRUE(throttling_enabled());
  EXPECT_TRUE(child_process_tuning_enabled());
  EXPECT_TRUE(freezing_enabled());
}

TEST_F(BatterySaverModeManagerTest,
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
  EXPECT_TRUE(child_process_tuning_enabled());
  EXPECT_TRUE(freezing_enabled());
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace performance_manager::user_tuning
