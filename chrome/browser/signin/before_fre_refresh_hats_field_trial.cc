// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/before_fre_refresh_hats_field_trial.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_param_associator.h"
#include "base/version_info/channel.h"
#include "chrome/common/channel_info.h"
#include "components/signin/public/base/signin_switches.h"

namespace signin {
namespace {

constexpr std::string_view kEnabledGroup = "EnabledClientSide";
constexpr std::string_view kDisabledGroup = "DisabledClientSide";
constexpr std::string_view kDefaultGroup = "Default";

constexpr std::string_view kTrialName = "BeforeFirstRunDesktopRefreshSurvey";

constexpr base::FieldTrial::Probability kTotalProbability = 100;

void SetFeatureState(base::FeatureList& feature_list,
                     base::FieldTrial& trial,
                     std::string_view group_name) {
  if (group_name == kDefaultGroup) {
    return;
  }

  base::FeatureList::OverrideState feature_state =
      base::FeatureList::OVERRIDE_DISABLE_FEATURE;
  if (group_name == kEnabledGroup) {
    feature_state = base::FeatureList::OVERRIDE_ENABLE_FEATURE;
    const std::map<std::string, std::string> params = {
        {"en_site_id", "XhHJ3uboj0ugnJ3q1cK0S6RQC7u7"},
        {"probability", "1.0"},
        {"hats_histogram_name",
         "Feedback.HappinessTrackingSurvey."
         "BeforeFirstRunDesktopRefreshSurvey"}};
    base::FieldTrialParamAssociator::GetInstance()->AssociateFieldTrialParams(
        trial.trial_name(), std::string(group_name), params);
  }
  feature_list.RegisterFieldTrialOverride(
      switches::kBeforeFirstRunDesktopRefreshSurvey.name, feature_state,
      &trial);
}

}  // namespace

void CreateBeforeFreRefreshHatsFieldTrial(
    base::FeatureList& feature_list,
    const base::FieldTrial::EntropyProvider& entropy_provider) {
  int enabled_percent = 0;
  int disabled_percent = 0;
  int default_percent = 0;
  switch (chrome::GetChannel()) {
    case version_info::Channel::UNKNOWN:
    case version_info::Channel::CANARY:
    case version_info::Channel::DEV:
      enabled_percent = 50;
      disabled_percent = 50;
      default_percent = 0;
      break;
    case version_info::Channel::BETA:
    case version_info::Channel::STABLE:
      enabled_percent = 0;
      disabled_percent = 0;
      default_percent = 100;
      break;
  }
  CHECK_EQ(kTotalProbability,
           enabled_percent + disabled_percent + default_percent);

  scoped_refptr<base::FieldTrial> trial =
      base::FieldTrialList::FactoryGetFieldTrial(
          kTrialName, kTotalProbability, kDefaultGroup, entropy_provider);
  CHECK(trial);
  trial->AppendGroup(std::string(kEnabledGroup), enabled_percent);
  trial->AppendGroup(std::string(kDisabledGroup), disabled_percent);
  trial->AppendGroup(std::string(kDefaultGroup), default_percent);

  SetFeatureState(feature_list, *trial, trial->GetGroupNameWithoutActivation());
}

}  // namespace signin
