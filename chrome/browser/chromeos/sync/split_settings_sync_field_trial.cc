// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/sync/split_settings_sync_field_trial.h"

#include "ash/constants/ash_features.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/common/channel_info.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/variations/variations_associated_data.h"
#include "components/version_info/version_info.h"

namespace split_settings_sync_field_trial {
namespace {

// String local state preference with the name of the assigned trial group.
// Empty if no group has been assigned yet.
const char kTrialGroupPrefName[] = "split_settings_sync.trial_group";

// The field trial name.
const char kTrialName[] = "SplitSettingsSync";

// Group names for the trial.
const char kEnabledGroup[] = "Enabled";
const char kDisabledGroup[] = "Disabled";
const char kDefaultGroup[] = "Default";

// Probabilities for all field trial groups add up to kTotalProbability.
const base::FieldTrial::Probability kTotalProbability = 100;

// Creates the field trial.
scoped_refptr<base::FieldTrial> CreateFieldTrial() {
  return base::FieldTrialList::FactoryGetFieldTrial(
      kTrialName, kTotalProbability, kDefaultGroup,
      base::FieldTrial::ONE_TIME_RANDOMIZED,
      /*default_group_number=*/nullptr);
}

// Sets the feature state based on the trial group. Defaults to disabled.
void SetFeatureState(base::FeatureList* feature_list,
                     base::FieldTrial* trial,
                     const std::string& group_name) {
  base::FeatureList::OverrideState feature_state =
      group_name == kEnabledGroup ? base::FeatureList::OVERRIDE_ENABLE_FEATURE
                                  : base::FeatureList::OVERRIDE_DISABLE_FEATURE;
  feature_list->RegisterFieldTrialOverride(
      chromeos::features::kSplitSettingsSync.name, feature_state, trial);
}

// Creates a trial for the first run (when there is no variations seed) and
// enables the feature based on the randomly selected trial group. Returns the
// group name.
std::string CreateFirstRunTrial(base::FeatureList* feature_list) {
  int enabled_percent;
  int disabled_percent;
  int default_percent;
  switch (chrome::GetChannel()) {
    case version_info::Channel::UNKNOWN:
    case version_info::Channel::CANARY:
    case version_info::Channel::DEV:
    case version_info::Channel::BETA:
      // Field trial is disabled due to b/171471530.
      // TODO(khorimoto): Re-enable the trial once the underlying issue is
      // fixed.
      enabled_percent = 0;
      disabled_percent = 0;
      default_percent = 100;
      break;
    case version_info::Channel::STABLE:
      // Disabled on Stable pending approval (see https://crbug.com/1020731).
      // Note that this code is not currently accessed on Stable channel due to
      // the early return in Create() below.
      enabled_percent = 0;
      disabled_percent = 0;
      default_percent = 100;
      break;
  }
  DCHECK_EQ(kTotalProbability,
            enabled_percent + disabled_percent + default_percent);

  // Set up the trial and groups.
  scoped_refptr<base::FieldTrial> trial = CreateFieldTrial();
  trial->AppendGroup(kEnabledGroup, enabled_percent);
  trial->AppendGroup(kDisabledGroup, disabled_percent);
  trial->AppendGroup(kDefaultGroup, default_percent);

  // Finalize the group choice and set the feature state.
  const std::string& group_name = trial->GetGroupNameWithoutActivation();
  SetFeatureState(feature_list, trial.get(), group_name);
  return group_name;
}

// Creates a trial with a single group and sets the feature flag to the state
// for that group.
void CreateSubsequentRunTrial(base::FeatureList* feature_list,
                              const std::string& group_name) {
  scoped_refptr<base::FieldTrial> trial = CreateFieldTrial();
  trial->AppendGroup(group_name, kTotalProbability);
  SetFeatureState(feature_list, trial.get(), group_name);
}

}  // namespace

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(kTrialGroupPrefName, std::string());
}

void Create(base::FeatureList* feature_list, PrefService* local_state) {
  // This field trial is only intended to be run on Canary/Dev/Beta channels.
  // If the user is on Stable channel, return early so that they are not opted
  // into this experiment. Without this return, users who were opted into the
  // experiment on Canary/Dev/Beta, then changed to Stable, could still be in
  // the experiment. See https://crbug.com/1147325.
  if (chrome::GetChannel() == version_info::Channel::STABLE)
    return;

  std::string trial_group = local_state->GetString(kTrialGroupPrefName);
  if (trial_group.empty()) {
    // No group assigned, this is the first run.
    trial_group = CreateFirstRunTrial(feature_list);
    // Persist the assigned group for subsequent runs.
    local_state->SetString(kTrialGroupPrefName, trial_group);
  } else {
    // Group already assigned.

    // Field trial is disabled due to b/171471530. Override the existing trial
    // and use kDefaultGroup instead.
    // TODO(khorimoto): Remove the line below once the underlying issue from
    // b/171471530 is fixed.
    trial_group = kDefaultGroup;

    CreateSubsequentRunTrial(feature_list, trial_group);
  }
}

}  // namespace split_settings_sync_field_trial
