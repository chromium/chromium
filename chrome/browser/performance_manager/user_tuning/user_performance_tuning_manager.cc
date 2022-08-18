// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/user_tuning/user_performance_tuning_manager.h"

#include "base/feature_list.h"
#include "base/power_monitor/power_monitor.h"
#include "base/values.h"
#include "chrome/browser/performance_manager/policies/high_efficiency_mode_policy.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/frame_rate_throttling.h"

namespace performance_manager::user_tuning {
namespace {

UserPerformanceTuningManager* g_user_performance_tuning_manager = nullptr;

class FrameThrottlingDelegateImpl
    : public performance_manager::user_tuning::UserPerformanceTuningManager::
          FrameThrottlingDelegate {
 public:
  void StartThrottlingAllFrameSinks() override {
    content::StartThrottlingAllFrameSinks(base::Hertz(30));
  }

  void StopThrottlingAllFrameSinks() override {
    content::StopThrottlingAllFrameSinks();
  }

  ~FrameThrottlingDelegateImpl() override = default;
};

class HighEfficiencyModeToggleDelegateImpl
    : public performance_manager::user_tuning::UserPerformanceTuningManager::
          HighEfficiencyModeToggleDelegate {
 public:
  void ToggleHighEfficiencyMode(bool enabled) override {
    performance_manager::PerformanceManager::CallOnGraph(
        FROM_HERE, base::BindOnce(
                       [](bool enabled, performance_manager::Graph* graph) {
                         policies::HighEfficiencyModePolicy::GetInstance()
                             ->OnHighEfficiencyModeChanged(enabled);
                       },
                       enabled));
  }

  ~HighEfficiencyModeToggleDelegateImpl() override = default;
};

}  // namespace

// static
UserPerformanceTuningManager* UserPerformanceTuningManager::GetInstance() {
  DCHECK(g_user_performance_tuning_manager);
  return g_user_performance_tuning_manager;
}

UserPerformanceTuningManager::~UserPerformanceTuningManager() {
  DCHECK_EQ(this, g_user_performance_tuning_manager);
  g_user_performance_tuning_manager = nullptr;

  base::PowerMonitor::RemovePowerStateObserver(this);
}

void UserPerformanceTuningManager::AddObserver(Observer* o) {
  observers_.AddObserver(o);
}

void UserPerformanceTuningManager::RemoveObserver(Observer* o) {
  observers_.RemoveObserver(o);
}

bool UserPerformanceTuningManager::DeviceHasBattery() const {
  // TODO(crbug.com/1348590): Check platform-specific APIs to return whether
  // this device has a battery.
  return true;
}

void UserPerformanceTuningManager::SetTemporaryBatterySaverDisabledForSession(
    bool disabled) {
  // Setting the temporary mode to its current state is a no-op.
  if (battery_saver_mode_disabled_for_session_ == disabled)
    return;

  battery_saver_mode_disabled_for_session_ = disabled;
  UpdateBatterySaverModeState();
}

bool UserPerformanceTuningManager::IsBatterySaverModeDisabledForSession()
    const {
  return battery_saver_mode_disabled_for_session_;
}

bool UserPerformanceTuningManager::IsBatterySaverActive() const {
  return battery_saver_mode_enabled_;
}

UserPerformanceTuningManager::UserPerformanceTuningManager(
    PrefService* local_state,
    std::unique_ptr<FrameThrottlingDelegate> frame_throttling_delegate,
    std::unique_ptr<HighEfficiencyModeToggleDelegate>
        high_efficiency_mode_toggle_delegate)
    : frame_throttling_delegate_(
          frame_throttling_delegate
              ? std::move(frame_throttling_delegate)
              : std::make_unique<FrameThrottlingDelegateImpl>()),
      high_efficiency_mode_toggle_delegate_(
          high_efficiency_mode_toggle_delegate
              ? std::move(high_efficiency_mode_toggle_delegate)
              : std::make_unique<HighEfficiencyModeToggleDelegateImpl>()) {
  DCHECK(!g_user_performance_tuning_manager);
  g_user_performance_tuning_manager = this;

  if (base::FeatureList::IsEnabled(
          performance_manager::features::kHighEfficiencyModeAvailable)) {
    // If the HEM pref is still the default (it wasn't configured by the user),
    // look up what that default value should be in Finch and set it here.
    // This is called in PostCreateThreads, which ensures the pref is in the
    // correct state when views are created.
    const PrefService::Preference* pref = local_state->FindPreference(
        performance_manager::user_tuning::prefs::kHighEfficiencyModeEnabled);
    if (pref->IsDefaultValue()) {
      local_state->SetDefaultPrefValue(
          performance_manager::user_tuning::prefs::kHighEfficiencyModeEnabled,
          base::Value(
              performance_manager::features::kHighEfficiencyModeDefaultState
                  .Get()));
    }
  }

  pref_change_registrar_.Init(local_state);
}

void UserPerformanceTuningManager::Start() {
  was_started_ = true;

  if (base::FeatureList::IsEnabled(
          performance_manager::features::kHighEfficiencyModeAvailable)) {
    pref_change_registrar_.Add(
        performance_manager::user_tuning::prefs::kHighEfficiencyModeEnabled,
        base::BindRepeating(
            &UserPerformanceTuningManager::OnHighEfficiencyModePrefChanged,
            base::Unretained(this)));
    // Make sure the initial state of the pref is passed on to the policy.
    OnHighEfficiencyModePrefChanged();
  }

  if (base::FeatureList::IsEnabled(
          performance_manager::features::kBatterySaverModeAvailable)) {
    pref_change_registrar_.Add(
        performance_manager::user_tuning::prefs::kBatterySaverModeState,
        base::BindRepeating(
            &UserPerformanceTuningManager::OnBatterySaverModePrefChanged,
            base::Unretained(this)));

    on_battery_power_ =
        base::PowerMonitor::AddPowerStateObserverAndReturnOnBatteryState(this);

    OnBatterySaverModePrefChanged();
  }
}

void UserPerformanceTuningManager::OnHighEfficiencyModePrefChanged() {
  bool enabled = pref_change_registrar_.prefs()->GetBoolean(
      performance_manager::user_tuning::prefs::kHighEfficiencyModeEnabled);
  high_efficiency_mode_toggle_delegate_->ToggleHighEfficiencyMode(enabled);
}

void UserPerformanceTuningManager::OnBatterySaverModePrefChanged() {
  battery_saver_mode_disabled_for_session_ = false;
  UpdateBatterySaverModeState();
}

void UserPerformanceTuningManager::UpdateBatterySaverModeState() {
  DCHECK(was_started_);

  using BatterySaverModeState =
      performance_manager::user_tuning::prefs::BatterySaverModeState;
  performance_manager::user_tuning::prefs::BatterySaverModeState state =
      performance_manager::user_tuning::prefs::GetCurrentBatterySaverModeState(
          pref_change_registrar_.prefs());

  bool previously_enabled = battery_saver_mode_enabled_;

  battery_saver_mode_enabled_ =
      !battery_saver_mode_disabled_for_session_ &&
      (state == BatterySaverModeState::kEnabled ||
       (state == BatterySaverModeState::kEnabledOnBattery &&
        on_battery_power_));

  // Don't change throttling or notify observers if the mode didn't change.
  if (previously_enabled == battery_saver_mode_enabled_)
    return;

  if (battery_saver_mode_enabled_) {
    frame_throttling_delegate_->StartThrottlingAllFrameSinks();
  } else {
    frame_throttling_delegate_->StopThrottlingAllFrameSinks();
  }

  for (auto& obs : observers_) {
    obs.OnBatterySaverModeChanged(battery_saver_mode_enabled_);
  }
}

void UserPerformanceTuningManager::OnPowerStateChange(bool on_battery_power) {
  on_battery_power_ = on_battery_power;

  for (auto& obs : observers_) {
    obs.OnExternalPowerConnectedChanged(on_battery_power);
  }

  UpdateBatterySaverModeState();
}

}  // namespace performance_manager::user_tuning
