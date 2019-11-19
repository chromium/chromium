// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial_param_associator.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/test/mock_entropy_provider.h"

namespace base {
namespace test {

namespace {

constexpr char kTrialGroup[] = "scoped_feature_list_trial_group";

std::vector<StringPiece> GetFeatureVector(
    const std::vector<Feature>& features) {
  std::vector<StringPiece> output;
  for (const Feature& feature : features) {
    output.push_back(feature.name);
  }

  return output;
}

std::vector<StringPiece> GetFeatureVectorFromFeaturesAndParams(
    const std::vector<ScopedFeatureList::FeatureAndParams>&
        features_and_params) {
  std::vector<StringPiece> output;
  for (const auto& entry : features_and_params) {
    output.push_back(entry.feature.name);
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

// Features in |feature_vector| came from |merged_features| in
// OverrideFeatures() and contains linkage with field trial is case when they
// have parameters (with '<' simbol). In |feature_name| name is already cleared
// with GetFeatureName() and also could be without parameters.
bool ContainsFeature(const std::vector<StringPiece>& feature_vector,
                     StringPiece feature_name) {
  auto iter = std::find_if(feature_vector.begin(), feature_vector.end(),
                           [&feature_name](const StringPiece& a) {
                             return GetFeatureName(a) == feature_name;
                           });
  return iter != feature_vector.end();
}

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

    if (ContainsFeature(merged_features->enabled_feature_list, feature_name) ||
        ContainsFeature(merged_features->disabled_feature_list, feature_name)) {
      continue;
    }

    if (override_state == FeatureList::OverrideState::OVERRIDE_ENABLE_FEATURE) {
      merged_features->enabled_feature_list.push_back(feature);
    } else {
      DCHECK_EQ(override_state,
                FeatureList::OverrideState::OVERRIDE_DISABLE_FEATURE);
      merged_features->disabled_feature_list.push_back(feature);
    }
  }
}

// Hex encode params so that special characters do not break formatting.
std::string HexEncodeString(const std::string& input) {
  return HexEncode(input.data(), input.size());
}

// Inverse of HexEncodeString().
std::string HexDecodeString(const std::string& input) {
  if (input.empty())
    return std::string();
  std::string bytes;
  bool result = HexStringToString(input, &bytes);
  DCHECK(result);
  return bytes;
}

}  // namespace

ScopedFeatureList::FeatureAndParams::FeatureAndParams(
    const Feature& feature,
    const FieldTrialParams& params)
    : feature(feature), params(params) {}

ScopedFeatureList::FeatureAndParams::~FeatureAndParams() = default;

ScopedFeatureList::FeatureAndParams::FeatureAndParams(
    const FeatureAndParams& other) = default;

ScopedFeatureList::ScopedFeatureList() = default;

ScopedFeatureList::~ScopedFeatureList() {
  Reset();
}

void ScopedFeatureList::Reset() {
  // If one of the Init() functions was never called, don't reset anything.
  if (!init_called_)
    return;

  init_called_ = false;

  FeatureList::ClearInstanceForTesting();

  if (field_trial_list_) {
    field_trial_list_.reset();

    // Restore params to how they were before.
    FieldTrialParamAssociator::GetInstance()->ClearAllParamsForTesting();
    AssociateFieldTrialParamsFromString(original_params_, &HexDecodeString);

    FieldTrialList::RestoreInstanceForTesting(original_field_trial_list_);
    original_field_trial_list_ = nullptr;
  }
  if (original_feature_list_)
    FeatureList::RestoreInstanceForTesting(std::move(original_feature_list_));
}

void ScopedFeatureList::Init() {
  InitWithFeaturesImpl({}, {}, {});
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
  InitWithFeaturesImpl(enabled_features, {}, disabled_features);
}

void ScopedFeatureList::InitAndEnableFeature(const Feature& feature) {
  InitWithFeaturesImpl({feature}, {}, {});
}

void ScopedFeatureList::InitAndDisableFeature(const Feature& feature) {
  InitWithFeaturesImpl({}, {}, {feature});
}

void ScopedFeatureList::InitWithFeatureState(const Feature& feature,
                                             bool enabled) {
  if (enabled) {
    InitAndEnableFeature(feature);
  } else {
    InitAndDisableFeature(feature);
  }
}

void ScopedFeatureList::InitWithFeaturesImpl(
    const std::vector<Feature>& enabled_features,
    const std::vector<FeatureAndParams>& enabled_features_and_params,
    const std::vector<Feature>& disabled_features) {
  DCHECK(!init_called_);
  DCHECK(enabled_features.empty() || enabled_features_and_params.empty());

  Features merged_features;
  if (!enabled_features_and_params.empty()) {
    merged_features.enabled_feature_list =
        GetFeatureVectorFromFeaturesAndParams(enabled_features_and_params);
  } else {
    merged_features.enabled_feature_list = GetFeatureVector(enabled_features);
  }
  merged_features.disabled_feature_list = GetFeatureVector(disabled_features);

  std::string current_enabled_features;
  std::string current_disabled_features;
  FeatureList* feature_list = FeatureList::GetInstance();
  if (feature_list) {
    feature_list->GetFeatureOverrides(&current_enabled_features,
                                      &current_disabled_features);
  }

  // Save off the existing field trials and params.
  std::string existing_trial_state;
  FieldTrialList::AllStatesToString(&existing_trial_state, true);
  original_params_ = FieldTrialList::AllParamsToString(true, &HexEncodeString);

  // Back up the current field trial list, to be restored in Reset().
  original_field_trial_list_ = FieldTrialList::BackupInstanceForTesting();

  // Create a field trial list, to which we'll add trials corresponding to the
  // features that have params, before restoring the field trial state from the
  // previous instance, further down in this function.
  field_trial_list_ =
      std::make_unique<FieldTrialList>(std::make_unique<MockEntropyProvider>());

  // Associate override params. This needs to be done before trial state gets
  // restored, as that will activate trials, locking down param association.
  auto* field_trial_param_associator = FieldTrialParamAssociator::GetInstance();
  std::vector<std::string> features_with_trial;
  auto feature_it = merged_features.enabled_feature_list.begin();
  for (const auto& enabled_feature : enabled_features_and_params) {
    const std::string feature_name = enabled_feature.feature.name;
    const std::string trial_name =
        "scoped_feature_list_trial_for_" + feature_name;

    scoped_refptr<FieldTrial> field_trial_override =
        FieldTrialList::CreateFieldTrial(trial_name, kTrialGroup);
    DCHECK(field_trial_override);

    field_trial_param_associator->ClearParamsForTesting(trial_name,
                                                        kTrialGroup);
    bool success = field_trial_param_associator->AssociateFieldTrialParams(
        trial_name, kTrialGroup, enabled_feature.params);
    DCHECK(success);

    features_with_trial.push_back(feature_name + "<" + trial_name);
    *feature_it = features_with_trial.back();
    ++feature_it;
  }
  // Restore other field trials. Note: We don't need to do anything for params
  // here because the param associator already has the right state, which has
  // been backed up via |original_params_| to be restored later.
  FieldTrialList::CreateTrialsFromString(existing_trial_state, {});

  OverrideFeatures(current_enabled_features,
                   FeatureList::OverrideState::OVERRIDE_ENABLE_FEATURE,
                   &merged_features);
  OverrideFeatures(current_disabled_features,
                   FeatureList::OverrideState::OVERRIDE_DISABLE_FEATURE,
                   &merged_features);

  std::string enabled = JoinString(merged_features.enabled_feature_list, ",");
  std::string disabled = JoinString(merged_features.disabled_feature_list, ",");
  InitFromCommandLine(enabled, disabled);
}

void ScopedFeatureList::InitAndEnableFeatureWithParameters(
    const Feature& feature,
    const FieldTrialParams& feature_parameters) {
  InitWithFeaturesAndParameters({{feature, feature_parameters}}, {});
}

void ScopedFeatureList::InitWithFeaturesAndParameters(
    const std::vector<FeatureAndParams>& enabled_features,
    const std::vector<Feature>& disabled_features) {
  InitWithFeaturesImpl({}, enabled_features, disabled_features);
}

}  // namespace test
}  // namespace base
