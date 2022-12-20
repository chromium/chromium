// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/hid_detection_revamp_field_trial.h"

#include "ash/constants/ash_features.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/common/channel_info.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/variations/variations_associated_data.h"
#include "components/version_info/version_info.h"

namespace ash::hid_detection_revamp_field_trial {

namespace {

// Probabilities for all field trial groups add up to kTotalProbability.
const base::FieldTrial::Probability kTotalProbability = 100;

// Creates the field trial.
scoped_refptr<base::FieldTrial> CreateFieldTrial(
    const base::FieldTrial::EntropyProvider& entropy_provider) {
  return base::FieldTrialList::FactoryGetFieldTrial(
      kTrialName, kTotalProbability, kDefaultGroup, entropy_provider);
}

// Sets the feature state based on the trial group.
void SetFeatureState(base::FeatureList* feature_list,
                     base::FieldTrial* trial,
                     const std::string& group_name) {
  // Don't override the feature if the group is the default group.
  if (group_name == kDefaultGroup)
    return;

  const base::FeatureList::OverrideState feature_state =
      group_name == kEnabledGroup ? base::FeatureList::OVERRIDE_ENABLE_FEATURE
                                  : base::FeatureList::OVERRIDE_DISABLE_FEATURE;
  feature_list->RegisterFieldTrialOverride(
      features::kOobeHidDetectionRevamp.name, feature_state, trial);
}

// Creates a trial if there is no group name saved and enables the features
// based on the randomly selected trial group. Returns the group name.
std::string CreateFreshTrial(
    const base::FieldTrial::EntropyProvider& entropy_provider,
    base::FeatureList* feature_list) {
  int enabled_percent;
  switch (chrome::GetChannel()) {
    case version_info::Channel::UNKNOWN:
    case version_info::Channel::CANARY:
    case version_info::Channel::DEV:
    case version_info::Channel::BETA:
      enabled_percent = 50;
      break;
    case version_info::Channel::STABLE:
      enabled_percent = 0;
      break;
  }
  int disabled_percent = enabled_percent;
  int default_percent = kTotalProbability - enabled_percent - disabled_percent;
  DCHECK_EQ(kTotalProbability,
            enabled_percent + disabled_percent + default_percent);

  // Set up the trial and groups.
  scoped_refptr<base::FieldTrial> trial = CreateFieldTrial(entropy_provider);
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
void CreateSubsequentRunTrial(
    const base::FieldTrial::EntropyProvider& entropy_provider,
    base::FeatureList* feature_list,
    const std::string& group_name) {
  scoped_refptr<base::FieldTrial> trial = CreateFieldTrial(entropy_provider);
  trial->AppendGroup(group_name, kTotalProbability);
  SetFeatureState(feature_list, trial.get(), group_name);
}

}  // namespace

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(kTrialGroupPrefName, std::string());
}

void Create(const base::FieldTrial::EntropyProvider& entropy_provider,
            base::FeatureList* feature_list,
            PrefService* local_state) {
  std::string trial_group = local_state->GetString(kTrialGroupPrefName);
  if (trial_group.empty()) {
    // No group assigned for the device yet. Assign a trial group.
    trial_group = CreateFreshTrial(entropy_provider, feature_list);

    // Persist the assigned group for subsequent runs.
    local_state->SetString(kTrialGroupPrefName, trial_group);
  } else {
    // Group already assigned. Toggle relevant features depending on
    // |trial_group| assigned.
    CreateSubsequentRunTrial(entropy_provider, feature_list, trial_group);
  }
}

}  // namespace ash::hid_detection_revamp_field_trial
