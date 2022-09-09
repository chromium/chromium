// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/metrics/metrics_provider.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/performance_manager/public/user_tuning/user_performance_tuning_manager.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "components/prefs/pref_service.h"

namespace performance_manager {

namespace {

MetricsProvider* g_metrics_provider = nullptr;

}

// static
MetricsProvider* MetricsProvider::GetInstance() {
  DCHECK(g_metrics_provider);
  return g_metrics_provider;
}

MetricsProvider::~MetricsProvider() {
  DCHECK_EQ(this, g_metrics_provider);
  g_metrics_provider = nullptr;
}

void MetricsProvider::Initialize() {
  DCHECK(!initialized_);

  pref_change_registrar_.Init(local_state_);
  pref_change_registrar_.Add(
      performance_manager::user_tuning::prefs::kHighEfficiencyModeEnabled,
      base::BindRepeating(&MetricsProvider::OnTuningModesChanged,
                          base::Unretained(this)));
  performance_manager::user_tuning::UserPerformanceTuningManager::GetInstance()
      ->AddObserver(this);
  battery_saver_enabled_ = performance_manager::user_tuning::
                               UserPerformanceTuningManager::GetInstance()
                                   ->IsBatterySaverActive();

  initialized_ = true;
  current_mode_ = ComputeCurrentMode();
}

void MetricsProvider::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto) {
  // It's valid for this to be called when `initialized_` is false if the finch
  // features controlling battery saver and high efficiency are disabled.
  // TODO(crbug.com/1348590): CHECK(initialized_) when the features are enabled
  // and removed.
  base::UmaHistogramEnumeration("PerformanceManager.UserTuning.EfficiencyMode",
                                current_mode_);

  // Set `current_mode_` to represent the state of the modes as they are now, so
  // that this mode is what is adequately reported at the next report, unless it
  // changes in the meantime.
  current_mode_ = ComputeCurrentMode();
}

MetricsProvider::MetricsProvider(PrefService* local_state)
    : local_state_(local_state) {
  DCHECK(!g_metrics_provider);
  g_metrics_provider = this;
}

void MetricsProvider::OnBatterySaverModeChanged(bool is_active) {
  battery_saver_enabled_ = is_active;
  OnTuningModesChanged();
}

void MetricsProvider::OnTuningModesChanged() {
  EfficiencyMode new_mode = ComputeCurrentMode();

  // If the mode changes between UMA reports, mark it as Mixed for this
  // interval.
  if (current_mode_ != new_mode) {
    current_mode_ = EfficiencyMode::kMixed;
  }
}

MetricsProvider::EfficiencyMode MetricsProvider::ComputeCurrentMode() const {
  // It's valid for this to be uninitialized if the battery saver/high
  // efficiency modes are unavailable. In that case, the browser is running in
  // normal mode, so return kNormal.
  // TODO(crbug.com/1348590): Change this to a DCHECK when the features are
  // enabled and removed.
  if (!initialized_) {
    return EfficiencyMode::kNormal;
  }

  // It's possible for this function to be called during shutdown, after
  // UserPerformanceTuningManager is destroyed. Do not access UPTM directly from
  // here.

  bool high_efficiency_enabled = local_state_->GetBoolean(
      performance_manager::user_tuning::prefs::kHighEfficiencyModeEnabled);

  if (high_efficiency_enabled && battery_saver_enabled_) {
    return EfficiencyMode::kBoth;
  }

  if (high_efficiency_enabled) {
    return EfficiencyMode::kHighEfficiency;
  }

  if (battery_saver_enabled_) {
    return EfficiencyMode::kBatterySaver;
  }

  return EfficiencyMode::kNormal;
}

}  // namespace performance_manager
