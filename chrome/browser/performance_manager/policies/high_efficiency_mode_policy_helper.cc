// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/high_efficiency_mode_policy_helper.h"

#include "base/feature_list.h"
#include "chrome/browser/performance_manager/policies/high_efficiency_mode_policy.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/frame_rate_throttling.h"

namespace performance_manager::policies {

HighEfficiencyModePolicyHelper::HighEfficiencyModePolicyHelper(
    PrefService* local_state) {
  pref_change_registrar_.Init(local_state);

  if (base::FeatureList::IsEnabled(
          performance_manager::features::kHighEfficiencyModeAvailable)) {
    pref_change_registrar_.Add(
        performance_manager::user_tuning::prefs::kHighEfficiencyModeEnabled,
        base::BindRepeating(
            &HighEfficiencyModePolicyHelper::OnHighEfficiencyModeChanged,
            base::Unretained(this)));
    // Make sure the initial state of the pref is passed on to the policy.
    OnHighEfficiencyModeChanged();
  }

  if (base::FeatureList::IsEnabled(
          performance_manager::features::kBatterySaverModeAvailable)) {
    pref_change_registrar_.Add(
        performance_manager::user_tuning::prefs::kBatterySaverModeEnabled,
        base::BindRepeating(
            &HighEfficiencyModePolicyHelper::OnBatterySaverModeChanged,
            base::Unretained(this)));
    OnBatterySaverModeChanged();
  }
}

void HighEfficiencyModePolicyHelper::OnHighEfficiencyModeChanged() {
  bool enabled = pref_change_registrar_.prefs()->GetBoolean(
      performance_manager::user_tuning::prefs::kHighEfficiencyModeEnabled);
  performance_manager::PerformanceManager::CallOnGraph(
      FROM_HERE, base::BindOnce(
                     [](bool enabled, performance_manager::Graph* graph) {
                       HighEfficiencyModePolicy::GetInstance()
                           ->OnHighEfficiencyModeChanged(enabled);
                     },
                     enabled));
}

void HighEfficiencyModePolicyHelper::OnBatterySaverModeChanged() {
  bool enabled = pref_change_registrar_.prefs()->GetBoolean(
      performance_manager::user_tuning::prefs::kBatterySaverModeEnabled);

  if (enabled) {
    content::StartThrottlingAllFrameSinks(base::Hertz(30));
  } else {
    content::StopThrottlingAllFrameSinks();
  }
}

}  // namespace performance_manager::policies
