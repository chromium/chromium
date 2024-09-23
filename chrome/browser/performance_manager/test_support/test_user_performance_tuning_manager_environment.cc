// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/test_support/test_user_performance_tuning_manager_environment.h"

#include "base/power_monitor/battery_state_sampler.h"
#include "base/run_loop.h"
#include "base/test/power_monitor_test_utils.h"
#include "chrome/browser/performance_manager/test_support/fake_child_process_tuning_delegate.h"
#include "chrome/browser/performance_manager/test_support/fake_frame_throttling_delegate.h"
#include "chrome/browser/performance_manager/test_support/fake_memory_saver_mode_delegate.h"
#include "chrome/browser/performance_manager/test_support/fake_power_monitor_source.h"
#include "components/prefs/pref_service.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power/power_manager_client.h"
#endif

namespace performance_manager::user_tuning {

namespace {

class QuitRunLoopOnBSMChangeObserver
    : public BatterySaverModeManager::Observer {
 public:
  explicit QuitRunLoopOnBSMChangeObserver(base::RepeatingClosure quit_closure)
      : quit_closure_(quit_closure) {}

  ~QuitRunLoopOnBSMChangeObserver() override = default;

  // BatterySaverModeManager::Observer implementation:
  void OnBatterySaverActiveChanged(bool) override { quit_closure_.Run(); }

 private:
  base::RepeatingClosure quit_closure_;
};

}  //  namespace

TestUserPerformanceTuningManagerEnvironment::
    TestUserPerformanceTuningManagerEnvironment() = default;

TestUserPerformanceTuningManagerEnvironment::
    ~TestUserPerformanceTuningManagerEnvironment() {
  DCHECK(!user_performance_tuning_manager_)
      << "TearDown must be invoked before destruction";
  DCHECK(!battery_saver_mode_manager_)
      << "TearDown must be invoked before destruction";
  DCHECK(!battery_sampler_) << "TearDown must be invoked before destruction";
}

void TestUserPerformanceTuningManagerEnvironment::SetUp(
    PrefService* local_state) {
  SetUp(local_state, nullptr, nullptr);
}

void TestUserPerformanceTuningManagerEnvironment::SetUp(
    PrefService* local_state,
    std::unique_ptr<base::SamplingEventSource> sampling_event_source,
    std::unique_ptr<base::BatteryLevelProvider> battery_level_provider) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!chromeos::PowerManagerClient::Get()) {
    tear_down_power_manager_client_ = true;
    chromeos::PowerManagerClient::InitializeFake();
  } else {
    // Check that it's FakePowerManagerClient.
    chromeos::FakePowerManagerClient::Get();
  }
#endif
  auto source = std::make_unique<FakePowerMonitorSource>();
  power_monitor_source_ = source.get();
  base::PowerMonitor::GetInstance()->Initialize(std::move(source));

  if (!sampling_event_source) {
    auto test_sampling_event_source =
        std::make_unique<base::test::TestSamplingEventSource>();
    sampling_source_ = test_sampling_event_source.get();
    sampling_event_source = std::move(test_sampling_event_source);
  } else {
    sampling_source_ = nullptr;
  }

  if (!battery_level_provider) {
    auto test_battery_level_provider =
        std::make_unique<base::test::TestBatteryLevelProvider>();
    battery_level_provider_ = test_battery_level_provider.get();
    battery_level_provider = std::move(test_battery_level_provider);
  } else {
    battery_level_provider_ = nullptr;
  }

  battery_sampler_ = std::make_unique<base::BatteryStateSampler>(
      std::move(sampling_event_source), std::move(battery_level_provider));

  user_performance_tuning_manager_.reset(
      new user_tuning::UserPerformanceTuningManager(
          local_state, nullptr,
          std::make_unique<FakeMemorySaverModeDelegate>()));
  battery_saver_mode_manager_.reset(new user_tuning::BatterySaverModeManager(
      local_state,
      std::make_unique<FakeFrameThrottlingDelegate>(&throttling_enabled_),
      std::make_unique<FakeChildProcessTuningDelegate>(
          &child_process_tuning_enabled_)));
  user_performance_tuning_manager_->Start();
  battery_saver_mode_manager_->Start();
}

void TestUserPerformanceTuningManagerEnvironment::TearDown() {
  sampling_source_ = nullptr;
  battery_level_provider_ = nullptr;
  user_performance_tuning_manager_.reset();
  battery_saver_mode_manager_.reset();
  battery_sampler_.reset();
  base::PowerMonitor::GetInstance()->ShutdownForTesting();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (tear_down_power_manager_client_) {
    chromeos::PowerManagerClient::Shutdown();
    tear_down_power_manager_client_ = false;
  }
#endif
}

// static
void TestUserPerformanceTuningManagerEnvironment::SetBatterySaverMode(
    PrefService* local_state,
    bool enabled) {
  auto mode = enabled ? performance_manager::user_tuning::prefs::
                            BatterySaverModeState::kEnabled
                      : performance_manager::user_tuning::prefs::
                            BatterySaverModeState::kDisabled;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (ash::features::IsBatterySaverAvailable()) {
    base::RunLoop run_loop;
    std::unique_ptr<QuitRunLoopOnBSMChangeObserver> observer =
        std::make_unique<QuitRunLoopOnBSMChangeObserver>(
            run_loop.QuitClosure());
    BatterySaverModeManager* manager = BatterySaverModeManager::GetInstance();
    manager->AddObserver(observer.get());
    power_manager::SetBatterySaverModeStateRequest request;
    request.set_enabled(enabled);
    chromeos::FakePowerManagerClient::Get()->SetBatterySaverModeState(request);
    run_loop.Run();
    manager->RemoveObserver(observer.get());
    return;
    // Fall through to the Chrome battery saver pref set code if the ChromeOS
    // battery saver feature is disabled.
  }
#endif
  local_state->SetInteger(
      performance_manager::user_tuning::prefs::kBatterySaverModeState,
      static_cast<int>(mode));
}

base::test::TestSamplingEventSource*
TestUserPerformanceTuningManagerEnvironment::sampling_source() {
  CHECK(sampling_source_) << "sampling_source unavailable when passed to SetUp";
  return sampling_source_;
}

base::test::TestBatteryLevelProvider*
TestUserPerformanceTuningManagerEnvironment::battery_level_provider() {
  CHECK(battery_level_provider_)
      << "battery_level_provider unavailable when passed to SetUp";
  return battery_level_provider_;
}

base::BatteryStateSampler*
TestUserPerformanceTuningManagerEnvironment::battery_state_sampler() {
  return battery_sampler_.get();
}

FakePowerMonitorSource*
TestUserPerformanceTuningManagerEnvironment::power_monitor_source() {
  return power_monitor_source_;
}

}  // namespace performance_manager::user_tuning
