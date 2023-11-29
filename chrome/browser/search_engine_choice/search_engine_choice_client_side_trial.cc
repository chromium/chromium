// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engine_choice/search_engine_choice_client_side_trial.h"

#include "base/check_deref.h"
#include "base/check_is_test.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/common/channel_info.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/search_engines_switches.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/version_info/channel.h"

namespace SearchEngineChoiceClientSideTrial {
namespace {

absl::optional<version_info::Channel> g_channel_for_testing;

// Should match the trial name from Finch.
const char kTrialName[] = "WaffleStudy";

// Group names for the trial.
const char kEnabledGroup[] = "ClientSideEnabledForTaggedProfiles";
const char kDisabledGroup[] = "ClientSideDisabled";
const char kDefaultGroup[] = "Default";

// Probabilities for all field trial groups add up to kTotalGroupWeight.
constexpr base::FieldTrial::Probability kTotalGroupWeight = 1000;

constexpr int kNonStableEnabledWeight = 500;
constexpr int kNonStableDisabledWeight = 500;
constexpr int kNonStableDefaultWeight = 0;
static_assert(kTotalGroupWeight == kNonStableEnabledWeight +
                                       kNonStableDisabledWeight +
                                       kNonStableDefaultWeight);

constexpr int kStableEnabledWeight = 5;
constexpr int kStableDisabledWeight = 5;
constexpr int kStableDefaultWeight = 990;
static_assert(kTotalGroupWeight == kStableEnabledWeight +
                                       kStableDisabledWeight +
                                       kStableDefaultWeight);

std::string PickTrialGroupWithoutActivation(base::FieldTrial& trial,
                                            version_info::Channel channel) {
  int enabled_weight;
  int disabled_weight;
  int default_weight;
  switch (channel) {
    case version_info::Channel::UNKNOWN:
    case version_info::Channel::CANARY:
    case version_info::Channel::DEV:
    case version_info::Channel::BETA:
      enabled_weight = kNonStableEnabledWeight;
      disabled_weight = kNonStableDisabledWeight;
      default_weight = kNonStableDefaultWeight;
      break;
    case version_info::Channel::STABLE:
      enabled_weight = kStableEnabledWeight;
      disabled_weight = kStableDisabledWeight;
      default_weight = kStableDefaultWeight;
      break;
  }
  DCHECK_EQ(kTotalGroupWeight,
            enabled_weight + disabled_weight + default_weight);

  trial.AppendGroup(kEnabledGroup, enabled_weight);
  trial.AppendGroup(kDisabledGroup, disabled_weight);
  trial.AppendGroup(kDefaultGroup, default_weight);

  return trial.GetGroupNameWithoutActivation();
}

void SetUp(const base::FieldTrial::EntropyProvider& entropy_provider,
           base::FeatureList& feature_list,
           PrefService& local_state,
           version_info::Channel channel) {
  // Set up the trial and determine the group for the current client.
  scoped_refptr<base::FieldTrial> trial =
      base::FieldTrialList::FactoryGetFieldTrial(
          kTrialName, kTotalGroupWeight, kDefaultGroup, entropy_provider);

  std::string group_name;
  if (local_state.HasPrefPath(prefs::kSearchEnginesStudyGroup)) {
    DVLOG(1) << "Continuing field trial setup with already set group "
             << group_name;
    group_name = local_state.GetString(prefs::kSearchEnginesStudyGroup);
  } else {
    group_name = PickTrialGroupWithoutActivation(*trial, channel);
    DVLOG(1) << "Setting field trial with selected group " << group_name;
    local_state.SetString(prefs::kSearchEnginesStudyGroup, group_name);
  }

  // Set up the state of the features based on the obtained group.
  base::FeatureList::OverrideState feature_state =
      group_name == kEnabledGroup ? base::FeatureList::OVERRIDE_ENABLE_FEATURE
                                  : base::FeatureList::OVERRIDE_DISABLE_FEATURE;

  if (feature_state == base::FeatureList::OVERRIDE_ENABLE_FEATURE) {
    base::AssociateFieldTrialParams(
        kTrialName, group_name,
        {{switches::kSearchEngineChoiceTriggerForTaggedProfilesOnly.name,
          "true"}});
  }

  feature_list.RegisterFieldTrialOverride(
      switches::kSearchEngineChoiceTrigger.name, feature_state, trial.get());
  feature_list.RegisterFieldTrialOverride(switches::kSearchEngineChoice.name,
                                          feature_state, trial.get());
  feature_list.RegisterFieldTrialOverride(switches::kSearchEngineChoiceFre.name,
                                          feature_state, trial.get());

  // Activate only after the overrides are completed.
  trial->Activate();

  // Can't call `RegisterSyntheticFieldTrial` here, it requires
  // `g_browser_process` to be available, we are too early for this.
}

}  // namespace

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kSearchEnginesStudyGroup, "");
}

void SetUpIfNeeded(const base::FieldTrial::EntropyProvider& entropy_provider,
                   base::FeatureList* feature_list,
                   PrefService* local_state) {
#if !BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_MAC)
  // Platform not in scope for this client-side trial.
  return;
#else
  // Make sure that Finch, fieldtrial_testing_config and command line flags take
  // precedence over features defined here. In particular, not detecting
  // fieldtrial_testing_config triggers a DCHECK.
  if (base::FieldTrialList::Find(kTrialName) ||
      feature_list->HasAssociatedFieldTrialByFeatureName(
          switches::kSearchEngineChoiceTrigger.name)) {
    DVLOG(1) << "Not setting up client-side trial for WaffleStudy, trial "
                "already registered";
    return;
  }

  // Skip setup if an associated feature is overriden, typically via the
  // commandline or setup during tests.
  if (feature_list->IsFeatureOverridden(
          switches::kSearchEngineChoiceTrigger.name) ||
      feature_list->IsFeatureOverridden(switches::kSearchEngineChoice.name) ||
      feature_list->IsFeatureOverridden(
          switches::kSearchEngineChoiceFre.name)) {
    LOG(WARNING) << "Not setting up client-side trial for WaffleStudy, feature "
                    "already overridden.";
    return;
  }

  // Proceed with actually setting up the field trial.
  SetUp(entropy_provider, CHECK_DEREF(feature_list), CHECK_DEREF(local_state),
        g_channel_for_testing.value_or(chrome::GetChannel()));
#endif
}

void SetUpForTesting(const base::FieldTrial::EntropyProvider& entropy_provider,
                     base::FeatureList& feature_list,
                     PrefService& local_state,
                     version_info::Channel channel) {
  CHECK_IS_TEST();
  SetUp(entropy_provider, feature_list, local_state, channel);
}

void RegisterSyntheticTrials() {
  CHECK(g_browser_process);
  auto enrolled_study_group = g_browser_process->local_state()->GetString(
      prefs::kSearchEnginesStudyGroup);
  if (enrolled_study_group.empty()) {
    // The user was not enrolled or exited the study at some point.
    return;
  }

  if (enrolled_study_group == kDefaultGroup) {
    // No need to register for the default group.
    return;
  }

  DVLOG(1) << "Registering synthetic field trial for group "
           << enrolled_study_group;
  ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
      kSyntheticTrialName, enrolled_study_group,
      variations::SyntheticTrialAnnotationMode::kCurrentLog);
}

ScopedChannelOverride CreateScopedChannelOverrideForTesting(
    version_info::Channel channel) {
  CHECK_IS_TEST();
  return ScopedChannelOverride(&g_channel_for_testing, channel);
}

}  // namespace SearchEngineChoiceClientSideTrial
