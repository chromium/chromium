// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial_param_associator.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"

namespace base {
namespace test {

namespace {

std::vector<StringPiece> GetFeatureVector(
    const std::vector<Feature>& features) {
  std::vector<StringPiece> output;
  for (const Feature& feature : features) {
    output.push_back(feature.name);
  }

  return output;
}

// Extracts a feature name from a feature state string. For example, given
// the input "*MyLovelyFeature<SomeFieldTrial", returns "MyLovelyFeature".
StringPiece GetFeatureName(StringPiece feature) {
  StringPiece feature_name = feature;

  // Remove default info.
  if (feature_name.starts_with("*"))
    feature_name = feature_name.substr(1);

  // Remove field_trial info.
  std::size_t index = feature_name.find("<");
  if (index != std::string::npos)
    feature_name = feature_name.substr(0, index);

  return feature_name;
}

struct Features {
  std::vector<StringPiece> enabled_feature_list;
  std::vector<StringPiece> disabled_feature_list;
};

// Merges previously-specified feature overrides with those passed into one of
// the Init() methods. |features| should be a list of features previously
// overridden to be in the |override_state|. |merged_features| should contain
// the enabled and disabled features passed into the Init() method, plus any
// overrides merged as a result of previous calls to this function.
void OverrideFeatures(const std::string& features,
                      FeatureList::OverrideState override_state,
                      Features* merged_features) {
  std::vector<StringPiece> features_list =
      SplitStringPiece(features, ",", TRIM_WHITESPACE, SPLIT_WANT_NONEMPTY);

  for (StringPiece feature : features_list) {
    StringPiece feature_name = GetFeatureName(feature);

    if (ContainsValue(merged_features->enabled_feature_list, feature_name) ||
        ContainsValue(merged_features->disabled_feature_list, feature_name))
      continue;

    if (override_state == FeatureList::OverrideState::OVERRIDE_ENABLE_FEATURE) {
      merged_features->enabled_feature_list.push_back(feature);
    } else {
      DCHECK_EQ(override_state,
                FeatureList::OverrideState::OVERRIDE_DISABLE_FEATURE);
      merged_features->disabled_feature_list.push_back(feature);
    }
  }
}

}  // namespace

ScopedFeatureList::ScopedFeatureList() = default;

ScopedFeatureList::~ScopedFeatureList() {
  // If one of the Init() functions was never called, don't reset anything.
  if (!init_called_)
    return;

  if (field_trial_override_) {
    base::FieldTrialParamAssociator::GetInstance()->ClearParamsForTesting(
        field_trial_override_->trial_name(),
        field_trial_override_->group_name());
  }

  FeatureList::ClearInstanceForTesting();
  if (original_feature_list_)
    FeatureList::RestoreInstanceForTesting(std::move(original_feature_list_));
}

void ScopedFeatureList::Init() {
  std::unique_ptr<FeatureList> feature_list(new FeatureList);
  feature_list->InitializeFromCommandLine(std::string(), std::string());
  InitWithFeatureList(std::move(feature_list));
}

void ScopedFeatureList::InitWithFeatureList(
    std::unique_ptr<FeatureList> feature_list) {
  DCHECK(!original_feature_list_);
  original_feature_list_ = FeatureList::ClearInstanceForTesting();
  FeatureList::SetInstance(std::move(feature_list));
  init_called_ = true;
}

void ScopedFeatureList::InitFromCommandLine(
    const std::string& enable_features,
    const std::string& disable_features) {
  std::unique_ptr<FeatureList> feature_list(new FeatureList);
  feature_list->InitializeFromCommandLine(enable_features, disable_features);
  InitWithFeatureList(std::move(feature_list));
}

void ScopedFeatureList::InitWithFeatures(
    const std::vector<Feature>& enabled_features,
    const std::vector<Feature>& disabled_features) {
  InitWithFeaturesAndFieldTrials(enabled_features, {}, disabled_features);
}

void ScopedFeatureList::InitAndEnableFeature(const Feature& feature) {
  InitWithFeaturesAndFieldTrials({feature}, {}, {});
}

void ScopedFeatureList::InitAndEnableFeatureWithFieldTrialOverride(
    const Feature& feature,
    FieldTrial* trial) {
  InitWithFeaturesAndFieldTrials({feature}, {trial}, {});
}

void ScopedFeatureList::InitAndDisableFeature(const Feature& feature) {
  InitWithFeaturesAndFieldTrials({}, {}, {feature});
}

void ScopedFeatureList::InitWithFeatureState(const Feature& feature,
                                             bool enabled) {
  if (enabled) {
    InitAndEnableFeature(feature);
  } else {
    InitAndDisableFeature(feature);
  }
}

void ScopedFeatureList::InitWithFeaturesAndFieldTrials(
    const std::vector<Feature>& enabled_features,
    const std::vector<FieldTrial*>& trials_for_enabled_features,
    const std::vector<Feature>& disabled_features) {
  DCHECK_LE(trials_for_enabled_features.size(), enabled_features.size());

  Features merged_features;
  merged_features.enabled_feature_list = GetFeatureVector(enabled_features);
  merged_features.disabled_feature_list = GetFeatureVector(disabled_features);

  FeatureList* feature_list = FeatureList::GetInstance();

  // |current_enabled_features| and |current_disabled_features| must declare out
  // of if scope to avoid them out of scope before JoinString calls because
  // |merged_features| may contains StringPiece which holding pointer points to
  // |current_enabled_features| and |current_disabled_features|.
  std::string current_enabled_features;
  std::string current_disabled_features;
  if (feature_list) {
    FeatureList::GetInstance()->GetFeatureOverrides(&current_enabled_features,
                                                    &current_disabled_features);
    OverrideFeatures(current_enabled_features,
                     FeatureList::OverrideState::OVERRIDE_ENABLE_FEATURE,
                     &merged_features);
    OverrideFeatures(current_disabled_features,
                     FeatureList::OverrideState::OVERRIDE_DISABLE_FEATURE,
                     &merged_features);
  }

  // Add the field trial overrides. This assumes that |enabled_features| are at
  // the begining of |merged_features.enabled_feature_list|, in the same order.
  auto trial_it = trials_for_enabled_features.begin();
  auto feature_it = merged_features.enabled_feature_list.begin();
  std::vector<std::unique_ptr<std::string>> features_with_trial;
  features_with_trial.reserve(trials_for_enabled_features.size());
  while (trial_it != trials_for_enabled_features.end()) {
    features_with_trial.push_back(std::make_unique<std::string>(
        feature_it->as_string() + "<" + (*trial_it)->trial_name()));
    // |features_with_trial| owns the string, and feature_it points to it.
    *feature_it = *(features_with_trial.back());
    ++trial_it;
    ++feature_it;
  }

  std::string enabled = JoinString(merged_features.enabled_feature_list, ",");
  std::string disabled = JoinString(merged_features.disabled_feature_list, ",");
  InitFromCommandLine(enabled, disabled);
}

void ScopedFeatureList::InitAndEnableFeatureWithParameters(
    const Feature& feature,
    const std::map<std::string, std::string>& feature_parameters) {
  if (!FieldTrialList::IsGlobalSetForTesting()) {
    field_trial_list_ = std::make_unique<base::FieldTrialList>(nullptr);
  }

  // TODO(crbug.com/794021) Remove this unique field trial name hack when there
  // is a cleaner solution.
  // Ensure that each call to this method uses a distinct field trial name.
  // Otherwise, nested calls might fail due to the shared FieldTrialList
  // already having the field trial registered.
  static int num_calls = 0;
  ++num_calls;
  std::string kTrialName =
      "scoped_feature_list_trial_name" + base::NumberToString(num_calls);
  std::string kTrialGroup = "scoped_feature_list_trial_group";

  field_trial_override_ =
      base::FieldTrialList::CreateFieldTrial(kTrialName, kTrialGroup);
  DCHECK(field_trial_override_);
  FieldTrialParamAssociator::GetInstance()->AssociateFieldTrialParams(
      kTrialName, kTrialGroup, feature_parameters);
  InitAndEnableFeatureWithFieldTrialOverride(feature,
                                             field_trial_override_.get());
}

}  // namespace test
}  // namespace base
