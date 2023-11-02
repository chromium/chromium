// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_CONSOLIDATED_CONSENT_FIELD_TRIAL_H_
#define CHROME_BROWSER_ASH_LOGIN_CONSOLIDATED_CONSENT_FIELD_TRIAL_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"

// TODO(crbug/1349811): Delete this file once feature is fully launched.
namespace ash::consolidated_consent_field_trial {

// String local state preference with the name of the assigned trial group.
// Empty if no group has been assigned yet.
inline constexpr char kTrialGroupPrefName[] = "per_user_metrics.trial_group";

// The field trial name.
inline constexpr char kTrialName[] = "ConsolidatedScreenAndPerUserMetrics";

// Group names for the trial.
inline constexpr char kEnabledGroup[] = "Enabled";
inline constexpr char kDisabledGroup[] = "Disabled";

// Registers the local state pref to hold the trial group.
//
// If the device does not belong to an experiment group, an experiment group
// will be assigned and the experiment group will be stored in local state. This
// is to keep states consistent across sessions even if the variation seed
// changes since the feature modifies the metrics opt-in/out model.
void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

// Enables client-side rollout for kOobeConsolidatedConsent and kPerUserMetrics
// features. These two features must be coupled together as they are dependent
// on each other.
//
// kOobeConsolidatedConsent is an OOBE feature and cannot depend on the
// variation seed being available. Thus, a client-side trial is created to
// control these two features.
//
// See crbug/1186354 for OOBE consolidated screen and crbug/1181504 for
// per-user metrics collection.
//
// The rollout plan for this feature is 50% for dev/beta.
void Create(const base::FieldTrial::EntropyProvider& entropy_provider,
            base::FeatureList* feature_list,
            PrefService* local_state);

// Whether trial should be enabled or not.
bool ShouldEnableTrial(version_info::Channel channel);

}  // namespace ash::consolidated_consent_field_trial

#endif  // CHROME_BROWSER_ASH_LOGIN_CONSOLIDATED_CONSENT_FIELD_TRIAL_H_
