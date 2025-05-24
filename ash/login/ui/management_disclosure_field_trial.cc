// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/management_disclosure_field_trial.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_number_conversions.h"
#include "chromeos/ash/components/channel/channel_info.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/variations/entropy_provider.h"
#include "components/version_info/version_info.h"

namespace ash::management_disclosure_field_trial {
namespace {

// The field trial name.
const char kTrialName[] = "ManagementDisclosure";

// Group names for the trial.
const char kEnabledGroup[] = "Enabled";
const char kDisabledGroup[] = "Disabled";
const char kDefaultGroup[] = "Default";

// Probabilities for all field trial groups add up to kTotalProbability.
const base::FieldTrial::Probability kTotalProbability = 100;

// Creates the field trial.
scoped_refptr<base::FieldTrial> CreateFieldTrial(
    const variations::EntropyProviders& entropy_provider) {
  return base::FieldTrialList::FactoryGetFieldTrial(
      kTrialName, kTotalProbability, kDefaultGroup,
      entropy_provider.default_entropy());
}

// Sets the feature state based on the trial group. Defaults to disabled.
void SetFeatureState(base::FeatureList* feature_list,
                     base::FieldTrial* trial,
                     const std::string& group_name) {
  if (group_name == kDefaultGroup) {
    return;
  }

  base::FeatureList::OverrideState feature_state =
      group_name == kEnabledGroup ? base::FeatureList::OVERRIDE_ENABLE_FEATURE
                                  : base::FeatureList::OVERRIDE_DISABLE_FEATURE;
  feature_list->RegisterFieldTrialOverride(
      features::kImprovedManagementDisclosure.name, feature_state, trial);
}

// Creates a trial for the first run (when there is no variations seed) and
// enables the feature based on the randomly selected trial group. Returns the
// group name.
std::string CreateFirstRunTrial(
    base::FeatureList* feature_list,
    const variations::EntropyProviders& entropy_provider) {
  int enabled_percent;
  int disabled_percent;
  int default_percent;
  switch (GetChannel()) {
    case version_info::Channel::UNKNOWN:
    case version_info::Channel::CANARY:
    case version_info::Channel::DEV:
    case version_info::Channel::BETA:
      enabled_percent = 50;
      disabled_percent = 50;
      default_percent = 0;
      break;
    case version_info::Channel::STABLE:
      // Disable on stable.
      enabled_percent = 0;
      disabled_percent = 0;
      default_percent = 100;
      break;
  }
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
    base::FeatureList* feature_list,
    const std::string& group_name,
    const variations::EntropyProviders& entropy_provider) {
  scoped_refptr<base::FieldTrial> trial = CreateFieldTrial(entropy_provider);
  trial->AppendGroup(group_name, kTotalProbability);
  SetFeatureState(feature_list, trial.get(), group_name);
}

}  // namespace

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kImprovedManagementDisclosure,
                               std::string());
}

void Create(base::FeatureList* feature_list,
            PrefService* local_state,
            const variations::EntropyProviders& entropy_provider) {
  std::string trial_group =
      local_state->GetString(prefs::kImprovedManagementDisclosure);
  if (trial_group.empty()) {
    // No group assigned, this is the first run.
    trial_group = CreateFirstRunTrial(feature_list, entropy_provider);
    // Persist the assigned group for subsequent runs.
    local_state->SetString(prefs::kImprovedManagementDisclosure, trial_group);
  } else {
    // Group already assigned.
    CreateSubsequentRunTrial(feature_list, trial_group, entropy_provider);
  }

  // Activate experiment after trial group is set.
  base::FeatureList::IsEnabled(features::kImprovedManagementDisclosure);
}

}  // namespace ash::management_disclosure_field_trial
