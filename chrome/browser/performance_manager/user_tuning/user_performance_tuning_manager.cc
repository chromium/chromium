// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/user_tuning/user_performance_tuning_manager.h"

#include "base/feature_list.h"
#include "chrome/browser/performance_manager/policies/high_efficiency_mode_policy.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/frame_rate_throttling.h"

namespace performance_manager::user_tuning {
namespace {

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

}  // namespace

UserPerformanceTuningManager::UserPerformanceTuningManager(
    PrefService* local_state,
    std::unique_ptr<FrameThrottlingDelegate> frame_throttling_delegate)
    : frame_throttling_delegate_(
          frame_throttling_delegate
              ? std::move(frame_throttling_delegate)
              : std::make_unique<FrameThrottlingDelegateImpl>()) {
  pref_change_registrar_.Init(local_state);

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
        performance_manager::user_tuning::prefs::kBatterySaverModeEnabled,
        base::BindRepeating(
            &UserPerformanceTuningManager::OnBatterySaverModePrefChanged,
            base::Unretained(this)));
    OnBatterySaverModePrefChanged();
  }
}

UserPerformanceTuningManager::~UserPerformanceTuningManager() = default;

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

void UserPerformanceTuningManager::SetTemporaryBatterySaver(bool enabled) {
  // Setting the temporary mode to its current state is a no-op.
  if (temporary_battery_saver_enabled_ == enabled)
    return;

  temporary_battery_saver_enabled_ = enabled;
  UpdateBatterySaverModeState();
}

bool UserPerformanceTuningManager::IsBatterySaverActive() const {
  return battery_saver_mode_enabled_;
}

void UserPerformanceTuningManager::OnHighEfficiencyModePrefChanged() {
  bool enabled = pref_change_registrar_.prefs()->GetBoolean(
      performance_manager::user_tuning::prefs::kHighEfficiencyModeEnabled);
  performance_manager::PerformanceManager::CallOnGraph(
      FROM_HERE, base::BindOnce(
                     [](bool enabled, performance_manager::Graph* graph) {
                       policies::HighEfficiencyModePolicy::GetInstance()
                           ->OnHighEfficiencyModeChanged(enabled);
                     },
                     enabled));
}

void UserPerformanceTuningManager::OnBatterySaverModePrefChanged() {
  UpdateBatterySaverModeState();
}

void UserPerformanceTuningManager::UpdateBatterySaverModeState() {
  bool pref_enabled = pref_change_registrar_.prefs()->GetBoolean(
      performance_manager::user_tuning::prefs::kBatterySaverModeEnabled);

  bool previously_enabled = battery_saver_mode_enabled_;
  battery_saver_mode_enabled_ =
      pref_enabled || temporary_battery_saver_enabled_;

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

}  // namespace performance_manager::user_tuning
