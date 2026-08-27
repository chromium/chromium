// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/first_run_desktop_refresh_field_trial.h"

#include <map>
#include <string>
#include <string_view>

#include "base/check.h"
#include "base/check_op.h"
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

constexpr std::string_view kTrialName = "FirstRunDesktopRefresh";

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
  }
  feature_list.RegisterFieldTrialOverride(
      switches::kFirstRunDesktopRefresh.name, feature_state, &trial);
  feature_list.RegisterFieldTrialOverride(
      switches::kFirstRunDesktopChoiceScreenRefresh.name, feature_state,
      &trial);
}

}  // namespace

void CreateFirstRunDesktopRefreshFieldTrial(
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
    // TODO(crbug.com/475441477): Rollout this experiment to Beta and Stable.
    case version_info::Channel::BETA:
    case version_info::Channel::STABLE:
      enabled_percent = 0;
      disabled_percent = 0;
      default_percent = 100;
      break;
  }
  constexpr base::FieldTrial::Probability total_probablility = 100;
  CHECK_EQ(total_probablility,
           enabled_percent + disabled_percent + default_percent);

  scoped_refptr<base::FieldTrial> trial =
      base::FieldTrialList::FactoryGetFieldTrial(
          kTrialName, total_probablility, kDefaultGroup, entropy_provider);
  CHECK(trial);
  trial->AppendGroup(kEnabledGroup, enabled_percent);
  trial->AppendGroup(kDisabledGroup, disabled_percent);
  trial->AppendGroup(kDefaultGroup, default_percent);

  SetFeatureState(feature_list, *trial, trial->GetGroupNameWithoutActivation());
}

}  // namespace signin
