// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/consolidated_consent_field_trial.h"

#include "ash/constants/ash_features.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/common/channel_info.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/variations/variations_associated_data.h"

namespace ash::consolidated_consent_field_trial {

namespace {

// Probabilities for all field trial groups add up to kTotalProbability.
const base::FieldTrial::Probability kTotalProbability = 100;

// Creates the field trial.
scoped_refptr<base::FieldTrial> CreateFieldTrial(
    const base::FieldTrial::EntropyProvider& entropy_provider) {
  return base::FieldTrialList::FactoryGetFieldTrial(
      kTrialName, kTotalProbability, kDisabledGroup, entropy_provider);
}

// Sets the feature state based on the trial group.
void SetFeatureState(base::FeatureList* feature_list,
                     base::FieldTrial* trial,
                     const std::string& group_name) {
  const base::FeatureList::OverrideState feature_state =
      group_name == kEnabledGroup ? base::FeatureList::OVERRIDE_ENABLE_FEATURE
                                  : base::FeatureList::OVERRIDE_DISABLE_FEATURE;

  // Both features need to be in the same state.
  feature_list->RegisterFieldTrialOverride(
      features::kOobeConsolidatedConsent.name, feature_state, trial);
  feature_list->RegisterFieldTrialOverride(features::kPerUserMetrics.name,
                                           feature_state, trial);
}

// Creates a trial if there is no group name saved and enables the features
// based on the randomly selected trial group. Returns the group name.
std::string CreateFreshTrial(
    const base::FieldTrial::EntropyProvider& entropy_provider,
    base::FeatureList* feature_list) {
  int enabled_percent;
  int disabled_percent;
  switch (chrome::GetChannel()) {
    case version_info::Channel::UNKNOWN:
    case version_info::Channel::CANARY:
    case version_info::Channel::DEV:
    case version_info::Channel::BETA:
      enabled_percent = 100;
      disabled_percent = 0;
      break;
    case version_info::Channel::STABLE:
      enabled_percent = 100;
      disabled_percent = 0;
      break;
  }
  DCHECK_EQ(kTotalProbability, enabled_percent + disabled_percent);

  // Set up the trial and groups.
  scoped_refptr<base::FieldTrial> trial = CreateFieldTrial(entropy_provider);
  trial->AppendGroup(kEnabledGroup, enabled_percent);
  trial->AppendGroup(kDisabledGroup, disabled_percent);

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

// Field trial override should not be used since feature is now enabled by
// default.
bool ShouldEnableTrial(version_info::Channel channel) {
  return false;
}

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(kTrialGroupPrefName, std::string());
}

void Create(const base::FieldTrial::EntropyProvider& entropy_provider,
            base::FeatureList* feature_list,
            PrefService* local_state) {
  // Storing the pref before the experiment is enabled would cause a skew when
  // this experiment is rolled out as existing clients would be in the
  // |kDisabled| group.
  if (!ShouldEnableTrial(chrome::GetChannel()))
    return;

  // Load the trial group from local state. Groups should be consistent once
  // assigned for the device since the feature involves OOBE and modifies
  // metrics opt-in/out model.
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

}  // namespace ash::consolidated_consent_field_trial
