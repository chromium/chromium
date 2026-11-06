// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/client_id_backup_correctness_fix_field_trial.h"

#include <string_view>

#include "base/check.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/field_trial.h"
#include "base/version_info/channel.h"
#include "chrome/common/channel_info.h"
#include "components/metrics/metrics_features.h"

namespace metrics {
namespace {

constexpr std::string_view kEnabledGroup = "EnabledClientSide";
constexpr std::string_view kDisabledGroup = "DisabledClientSide";
constexpr std::string_view kDefaultGroup = "Default";

void CreateFieldTrialImpl(
    base::FeatureList& feature_list,
    const base::FieldTrial::EntropyProvider& entropy_provider,
    version_info::Channel channel) {
  if (base::FieldTrialList::TrialExists(
          kClientIdBackupCorrectnessFixTrialName) ||
      feature_list.IsFeatureOverridden(
          features::kClientIdBackupCorrectnessFix.name)) {
    return;
  }

  base::FieldTrial::Probability enabled_percent = 0;
  base::FieldTrial::Probability disabled_percent = 0;
  base::FieldTrial::Probability default_percent = 0;
  switch (channel) {
    case version_info::Channel::CANARY:
    case version_info::Channel::DEV:
      enabled_percent = 50;
      disabled_percent = 50;
      default_percent = 0;
      break;
    case version_info::Channel::UNKNOWN:
    case version_info::Channel::BETA:
    case version_info::Channel::STABLE:
      enabled_percent = 0;
      disabled_percent = 0;
      default_percent = 100;
      break;
  }

  constexpr base::FieldTrial::Probability kTotalProbability = 100;
  CHECK_EQ(kTotalProbability,
           enabled_percent + disabled_percent + default_percent);

  scoped_refptr<base::FieldTrial> trial =
      base::FieldTrialList::FactoryGetFieldTrial(
          kClientIdBackupCorrectnessFixTrialName, kTotalProbability,
          kDefaultGroup, entropy_provider);
  CHECK(trial);
  trial->AppendGroup(kEnabledGroup, enabled_percent);
  trial->AppendGroup(kDisabledGroup, disabled_percent);
  trial->AppendGroup(kDefaultGroup, default_percent);

  const std::string_view group_name = trial->GetGroupNameWithoutActivation();
  if (group_name == kEnabledGroup) {
    feature_list.RegisterFieldTrialOverride(
        features::kClientIdBackupCorrectnessFix.name,
        base::FeatureList::OVERRIDE_ENABLE_FEATURE, trial.get());
  } else if (group_name == kDisabledGroup) {
    feature_list.RegisterFieldTrialOverride(
        features::kClientIdBackupCorrectnessFix.name,
        base::FeatureList::OVERRIDE_DISABLE_FEATURE, trial.get());
  }
}

}  // namespace

void CreateClientIdBackupCorrectnessFixFieldTrial(
    base::FeatureList& feature_list,
    const base::FieldTrial::EntropyProvider& entropy_provider) {
  CreateFieldTrialImpl(feature_list, entropy_provider, chrome::GetChannel());
}

void CreateClientIdBackupCorrectnessFixFieldTrialForTesting(
    base::FeatureList& feature_list,
    const base::FieldTrial::EntropyProvider& entropy_provider,
    version_info::Channel channel) {
  CreateFieldTrialImpl(feature_list, entropy_provider, channel);
}

}  // namespace metrics
