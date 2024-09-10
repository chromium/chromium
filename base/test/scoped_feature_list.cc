// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"

#include <atomic>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/features.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial_param_associator.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/mock_entropy_provider.h"
#include "base/test/task_environment.h"

namespace base::test {

// A struct describes ParsedEnableFeatures()' result.
struct ScopedFeatureList::FeatureWithStudyGroup {
  FeatureWithStudyGroup(const std::string& feature_name,
                        const std::string& study_name,
                        const std::string& group_name,
                        const std::string& params)
      : feature_name(feature_name),
        study_name(study_name),
        group_name(group_name),
        params(params) {
    DCHECK(IsValidFeatureName(feature_name));
    DCHECK(IsValidFeatureOrFieldTrialName(study_name));
    DCHECK(IsValidFeatureOrFieldTrialName(group_name));
  }

  explicit FeatureWithStudyGroup(const std::string& feature_name)
      : feature_name(feature_name) {
    DCHECK(IsValidFeatureName(feature_name));
  }

  ~FeatureWithStudyGroup() = default;
  FeatureWithStudyGroup(const FeatureWithStudyGroup& other) = default;

  bool operator==(const FeatureWithStudyGroup& other) const {
    return feature_name == other.feature_name &&
           StudyNameOrDefault() == other.StudyNameOrDefault() &&
           GroupNameOrDefault() == other.GroupNameOrDefault();
  }

  std::string FeatureName() const {
    return StartsWith(feature_name, "*") ? feature_name.substr(1)
                                         : feature_name;
  }

  // If |study_name| is empty, returns a default study name for |feature_name|.
  // Otherwise, just return |study_name|.
  std::string StudyNameOrDefault() const {
    return study_name.empty() ? "Study" + FeatureName() : study_name;
  }

  // If |group_name| is empty, returns a default group name for |feature_name|.
  // Otherwise, just return |group_name|.
  std::string GroupNameOrDefault() const {
    return group_name.empty() ? "Group" + FeatureName() : group_name;
  }

  bool has_params() const { return !params.empty(); }

  std::string ParamsForFeatureList() const {
    if (params.empty()) {
      return "";
    }
    return ":" + params;
  }

  static bool IsValidFeatureOrFieldTrialName(std::string_view name) {
    return IsStringASCII(name) &&
           name.find_first_of(",<*") == std::string::npos;
  }

  static bool IsValidFeatureName(std::string_view feature_name) {
    return IsValidFeatureOrFieldTrialName(
        StartsWith(feature_name, "*") ? feature_name.substr(1) : feature_name);
  }

  // When ParseEnableFeatures() gets
  // "FeatureName<StudyName.GroupName:Param1/Value1/Param2/Value2",
  // a new FeatureWithStudyGroup with:
  // - feature_name = "FeatureName"
  // - study_name = "StudyName"
  // - group_name = "GroupName"
  // - params = "Param1/Value1/Param2/Value2"
  // will be created and be returned.
  const std::string feature_name;
  const std::string study_name;
  const std::string group_name;
  const std::string params;
};

struct ScopedFeatureList::Features {
  std::vector<FeatureWithStudyGroup> enabled_feature_list;
  std::vector<FeatureWithStudyGroup> disabled_feature_list;
};

namespace {

constexpr char kTrialGroup[] = "scoped_feature_list_trial_group";

// Checks and parses the |enable_features| flag and appends each parsed
// feature, an instance of FeatureWithStudyGroup, to |parsed_enable_features|.
// Returns true if |enable_features| is parsable, otherwise false.
// The difference between this function and ParseEnabledFeatures() defined in
// feature_list.cc is:
// if "Feature1<Study1.Group1:Param1/Value1/Param2/Value2," +
//    "Feature2<Study2.Group2" is given,
// feature_list.cc's one returns strings:
//   parsed_enable_features = "Feature1<Study1,Feature2<Study2"
//   force_field_trials = "Study1/Group1"
//   force_fieldtrial_params = "Study1<Group1:Param1/Value1/Param2/Value2"
//  this function returns a vector:
//   [0] FeatureWithStudyGroup("Feature1", "Study1", "Group1",
//         "Param1/Value1/Param2/Value2")
//   [1] FeatureWithStudyGroup("Feature2", "Study2", "Group2", "")
bool ParseEnableFeatures(const std::string& enable_features,
                         std::vector<ScopedFeatureList::FeatureWithStudyGroup>&
                             parsed_enable_features) {
  for (const auto& enable_feature :
       FeatureList::SplitFeatureListString(enable_features)) {
    std::string feature_name;
    std::string study;
    std::string group;
    std::string feature_params;
    if (!FeatureList::ParseEnableFeatureString(
            enable_feature, &feature_name, &study, &group, &feature_params)) {
      return false;
    }

    parsed_enable_features.emplace_back(feature_name, study, group,
                                        feature_params);
  }
  return true;
}

// Escapes separators used by enable-features command line.
// E.g. Feature '<' Study '.' Group ':' param1 '/' value1 ','
// ('*' is not a separator. No need to escape it.)
std::string EscapeValue(const std::string& value) {
  std::string escaped_str;
  for (const auto ch : value) {
    if (ch == ',' || ch == '/' || ch == ':' || ch == '<' || ch == '.') {
      escaped_str.append(base::StringPrintf("%%%02X", ch));
    } else {
      escaped_str.append(1, ch);
    }
  }
  return escaped_str;
}

// Extracts a feature name from a feature state string. For example, given
// the input "*MyLovelyFeature<SomeFieldTrial", returns "MyLovelyFeature".
std::string_view GetFeatureName(std::string_view feature) {
  std::string_view feature_name = feature;

  // Remove default info.
  if (StartsWith(feature_name, "*")) {
    feature_name = feature_name.substr(1);
  }

  // Remove field_trial info.
  std::size_t index = feature_name.find("<");
  if (index != std::string::npos) {
    feature_name = feature_name.substr(0, index);
  }

  return feature_name;
}

// Features in |feature_vector| came from |merged_features| in
// OverrideFeatures() and contains linkage with field trial is case when they
// have parameters (with '<' simbol). In |feature_name| name is already cleared
// with GetFeatureName() and also could be without parameters.
bool ContainsFeature(
    const std::vector<ScopedFeatureList::FeatureWithStudyGroup>& feature_vector,
    std::string_view feature_name) {
  return Contains(feature_vector, feature_name,
                  [](const ScopedFeatureList::FeatureWithStudyGroup& a) {
                    return a.feature_name;
                  });
}

// Merges previously-specified feature overrides with those passed into one of
// the Init() methods. |features| should be a list of features previously
// overridden to be in the |override_state|. |merged_features| should contain
// the enabled and disabled features passed into the Init() method, plus any
// overrides merged as a result of previous calls to this function.
void OverrideFeatures(
    const std::vector<ScopedFeatureList::FeatureWithStudyGroup>& features_list,
    FeatureList::OverrideState override_state,
    ScopedFeatureList::Features* merged_features) {
  for (const auto& feature : features_list) {
    std::string_view feature_name = GetFeatureName(feature.feature_name);

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

// Merges previously-specified feature overrides with those passed into one of
// the Init() methods. |feature_list| should be a string whose format is the
// same as --enable-features or --disable-features command line flag, and
// specifies features overridden to be in the |override_state|.
// |merged_features| should contain the enabled and disabled features passed in
// to the Init() method, plus any overrides merged as a result of previous
// calls to this function.
void OverrideFeatures(const std::string& features_list,
                      FeatureList::OverrideState override_state,
                      ScopedFeatureList::Features* merged_features) {
  std::vector<ScopedFeatureList::FeatureWithStudyGroup> parsed_features;
  bool parse_enable_features_result =
      ParseEnableFeatures(features_list, parsed_features);
  DCHECK(parse_enable_features_result);
  OverrideFeatures(parsed_features, override_state, merged_features);
}

// Hex encode params so that special characters do not break formatting.
std::string HexEncodeString(const std::string& input) {
  return HexEncode(input.data(), input.size());
}

// Inverse of HexEncodeString().
std::string HexDecodeString(const std::string& input) {
  if (input.empty()) {
    return std::string();
  }
  std::string bytes;
  bool result = HexStringToString(input, &bytes);
  DCHECK(result);
  return bytes;
}

// Returns a command line string suitable to pass to
// FeatureList::InitFromCommandLine(). For example,
// {{"Feature1", "Study1", "Group1", "Param1/Value1/"}, {"Feature2"}} returns:
// - |enabled_feature|=true -> "Feature1<Study1.Group1:Param1/Value1/,Feature2"
// - |enabled_feature|=false -> "Feature1<Study1.Group1,Feature2"
std::string CreateCommandLineArgumentFromFeatureList(
    const std::vector<ScopedFeatureList::FeatureWithStudyGroup>& feature_list,
    bool enable_features) {
  std::vector<std::string> features;
  for (const auto& feature : feature_list) {
    std::string feature_with_study_group = feature.feature_name;
    if (feature.has_params() || !feature.study_name.empty()) {
      feature_with_study_group += "<";
      feature_with_study_group += feature.StudyNameOrDefault();
      if (feature.has_params() || !feature.group_name.empty()) {
        feature_with_study_group += ".";
        feature_with_study_group += feature.GroupNameOrDefault();
      }
      if (feature.has_params() && enable_features) {
        feature_with_study_group += feature.ParamsForFeatureList();
      }
    }
    features.push_back(feature_with_study_group);
  }
  return JoinString(features, ",");
}

}  // namespace

FeatureRefAndParams::FeatureRefAndParams(const Feature& feature,
                                         const FieldTrialParams& params)
    : feature(feature), params(params) {}

FeatureRefAndParams::FeatureRefAndParams(const FeatureRefAndParams& other) =
    default;

FeatureRefAndParams::~FeatureRefAndParams() = default;

ScopedFeatureList::ScopedFeatureList() = default;

ScopedFeatureList::ScopedFeatureList(const Feature& enable_feature) {
  InitAndEnableFeature(enable_feature);
}

ScopedFeatureList::~ScopedFeatureList() {
  Reset();
}

void ScopedFeatureList::Reset() {
  // If one of the Init() functions was never called, don't reset anything.
  if (!init_called_) {
    return;
  }

  init_called_ = false;

  // ThreadPool tasks racily probing FeatureList while it's initialized/reset
  // are problematic and while callers should ideally set up ScopedFeatureList
  // before TaskEnvironment, that's not always possible. Fencing execution here
  // avoids an entire class of bugs by making sure no ThreadPool task queries
  // FeatureList while it's being modified. This local action is preferred to
  // requiring all such callers to manually flush all tasks before each
  // ScopedFeatureList Init/Reset: crbug.com/1275502#c45
  //
  // All FeatureList modifications in this file should have this as well.
  TaskEnvironment::ParallelExecutionFence fence(
      "ScopedFeatureList must be Reset from the test main thread");

  FeatureList::ClearInstanceForTesting();

  if (field_trial_list_) {
    field_trial_list_.reset();
  }

  // Restore params to how they were before.
  FieldTrialParamAssociator::GetInstance()->ClearAllParamsForTesting();
  if (!original_params_.empty()) {
    // Before restoring params, we need to make all field trials in-active,
    // because FieldTrialParamAssociator checks whether the given field trial
    // is active or not, and associates no parameters if the trial is active.
    // So temporarily restore field trial list to be nullptr.
    FieldTrialList::RestoreInstanceForTesting(nullptr);
    AssociateFieldTrialParamsFromString(original_params_, &HexDecodeString);
  }

  if (original_field_trial_list_) {
    FieldTrialList::RestoreInstanceForTesting(original_field_trial_list_);
    original_field_trial_list_ = nullptr;
  }

  if (original_feature_list_) {
    FeatureList::RestoreInstanceForTesting(std::move(original_feature_list_));
  }
}

void ScopedFeatureList::Init() {
  InitWithFeaturesImpl({}, {}, {}, /*keep_existing_states=*/true);
}

void ScopedFeatureList::InitWithEmptyFeatureAndFieldTrialLists() {
  InitWithFeaturesImpl({}, {}, {}, /*keep_existing_states=*/false);
}

void ScopedFeatureList::InitWithNullFeatureAndFieldTrialLists() {
  DCHECK(!init_called_);

  // Back up the current field trial parameters to be restored in Reset().
  original_params_ = FieldTrialList::AllParamsToString(&HexEncodeString);

  // Back up the current field trial list, to be restored in Reset().
  original_field_trial_list_ = FieldTrialList::BackupInstanceForTesting();

  auto* field_trial_param_associator = FieldTrialParamAssociator::GetInstance();
  field_trial_param_associator->ClearAllParamsForTesting();
  field_trial_list_ = nullptr;

  DCHECK(!original_feature_list_);

  // Execution fence required while modifying FeatureList, as in Reset.
  TaskEnvironment::ParallelExecutionFence fence(
      "ScopedFeatureList must be Init from the test main thread");

  // Back up the current feature list, to be restored in Reset().
  original_feature_list_ = FeatureList::ClearInstanceForTesting();
  init_called_ = true;
}

void ScopedFeatureList::InitWithFeatureList(
    std::unique_ptr<FeatureList> feature_list) {
  DCHECK(!original_feature_list_);

  // Execution fence required while modifying FeatureList, as in Reset.
  TaskEnvironment::ParallelExecutionFence fence(
      "ScopedFeatureList must be Init from the test main thread");

  original_feature_list_ = FeatureList::ClearInstanceForTesting();
  FeatureList::SetInstance(std::move(feature_list));
  init_called_ = true;
}

void ScopedFeatureList::InitFromCommandLine(
    const std::string& enable_features,
    const std::string& disable_features) {
  Features merged_features;
  bool parse_enable_features_result =
      ParseEnableFeatures(enable_features,
                          merged_features.enabled_feature_list) &&
      ParseEnableFeatures(disable_features,
                          merged_features.disabled_feature_list);
  DCHECK(parse_enable_features_result);
  return InitWithMergedFeatures(std::move(merged_features),
                                /*create_associated_field_trials=*/false,
                                /*keep_existing_states=*/true);
}

void ScopedFeatureList::InitWithFeatures(
    const std::vector<FeatureRef>& enabled_features,
    const std::vector<FeatureRef>& disabled_features) {
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

void ScopedFeatureList::InitWithFeatureStates(
    const flat_map<FeatureRef, bool>& feature_states) {
  std::vector<FeatureRef> enabled_features, disabled_features;
  for (const auto& [feature, enabled] : feature_states) {
    if (enabled) {
      enabled_features.push_back(feature);
    } else {
      disabled_features.push_back(feature);
    }
  }
  InitWithFeaturesImpl(enabled_features, {}, disabled_features);
}

void ScopedFeatureList::InitWithFeaturesImpl(
    const std::vector<FeatureRef>& enabled_features,
    const std::vector<FeatureRefAndParams>& enabled_features_and_params,
    const std::vector<FeatureRef>& disabled_features,
    bool keep_existing_states) {
  DCHECK(!init_called_);
  DCHECK(enabled_features.empty() || enabled_features_and_params.empty());

  Features merged_features;
  bool create_associated_field_trials = false;
  if (!enabled_features_and_params.empty()) {
    for (const auto& feature : enabled_features_and_params) {
      std::string trial_name = "scoped_feature_list_trial_for_";
      trial_name += feature.feature->name;

      // If features.params has 2 params whose values are value1 and value2,
      // |params| will be "param1/value1/param2/value2/".
      std::string params;
      for (const auto& param : feature.params) {
        // Add separator from previous param information if it exists.
        if (!params.empty()) {
          params.append(1, '/');
        }
        params.append(EscapeValue(param.first));
        params.append(1, '/');
        params.append(EscapeValue(param.second));
      }

      merged_features.enabled_feature_list.emplace_back(
          feature.feature->name, trial_name, kTrialGroup, params);
    }
    create_associated_field_trials = true;
  } else {
    for (const auto& feature : enabled_features) {
      merged_features.enabled_feature_list.emplace_back(feature->name);
    }
  }
  // If there is any parameter override, we need to disable parameter cache so
  // that the FeatureParam doesn't pick up a cached value.
  bool need_to_disable_parameter_cache = create_associated_field_trials;
  for (const auto& feature : disabled_features) {
    merged_features.disabled_feature_list.emplace_back(feature->name);
    if (feature->name == features::kFeatureParamWithCache.name) {
      // Reset the flag as the cache is already ordered to be disabled.
      need_to_disable_parameter_cache = false;
    }
  }
  if (need_to_disable_parameter_cache) {
    merged_features.disabled_feature_list.emplace_back(
        features::kFeatureParamWithCache.name);
  }

  InitWithMergedFeatures(std::move(merged_features),
                         create_associated_field_trials, keep_existing_states);
}

void ScopedFeatureList::InitAndEnableFeatureWithParameters(
    const Feature& feature,
    const FieldTrialParams& feature_parameters) {
  InitWithFeaturesAndParameters({{feature, feature_parameters}}, {});
}

void ScopedFeatureList::InitWithFeaturesAndParameters(
    const std::vector<FeatureRefAndParams>& enabled_features,
    const std::vector<FeatureRef>& disabled_features) {
  InitWithFeaturesImpl({}, enabled_features, disabled_features);
}

void ScopedFeatureList::InitWithMergedFeatures(
    Features&& merged_features,
    bool create_associated_field_trials,
    bool keep_existing_states) {
  DCHECK(!init_called_);

  std::string current_enabled_features;
  std::string current_disabled_features;
  const FeatureList* feature_list = FeatureList::GetInstance();
  if (feature_list && keep_existing_states) {
    feature_list->GetFeatureOverrides(&current_enabled_features,
                                      &current_disabled_features);
  }

  std::vector<FieldTrial::State> all_states =
      FieldTrialList::GetAllFieldTrialStates(PassKey());
  original_params_ = FieldTrialList::AllParamsToString(&HexEncodeString);

  std::vector<ScopedFeatureList::FeatureWithStudyGroup>
      parsed_current_enabled_features;
  // Check relationship between current enabled features and field trials.
  bool parse_enable_features_result = ParseEnableFeatures(
      current_enabled_features, parsed_current_enabled_features);
  DCHECK(parse_enable_features_result);

  // Back up the current field trial list, to be restored in Reset().
  original_field_trial_list_ = FieldTrialList::BackupInstanceForTesting();

  // Create a field trial list, to which we'll add trials corresponding to the
  // features that have params, before restoring the field trial state from the
  // previous instance, further down in this function.
  field_trial_list_ = std::make_unique<FieldTrialList>();

  auto* field_trial_param_associator = FieldTrialParamAssociator::GetInstance();
  for (const auto& feature : merged_features.enabled_feature_list) {
    // If we don't need to create any field trials for the |feature| (i.e.
    // unless |create_associated_field_trials|=true or |feature| has any
    // params), we can skip the code: EraseIf()...ClearParamsForTesting().
    if (!(create_associated_field_trials || feature.has_params())) {
      continue;
    }

    // |all_states| contains the existing field trials, and is used to
    // restore the field trials into a newly created field trial list with
    // FieldTrialList::CreateTrialsFromFieldTrialStates().
    // However |all_states| may have a field trial that's being explicitly
    // set through |merged_features.enabled_feature_list|. In this case,
    // FieldTrialParamAssociator::AssociateFieldTrialParams() will fail.
    // So remove such field trials from |all_states| here.
    std::erase_if(all_states, [feature](const auto& state) {
      return state.trial_name == feature.StudyNameOrDefault();
    });

    // If |create_associated_field_trials| is true, we want to match the
    // behavior of VariationsFieldTrialCreator to always associate a field
    // trial, even when there no params. Since
    // FeatureList::InitFromCommandLine() doesn't associate a field trial
    // when there are no params, we do it here.
    if (!feature.has_params()) {
      scoped_refptr<FieldTrial> field_trial_without_params =
          FieldTrialList::CreateFieldTrial(feature.StudyNameOrDefault(),
                                           feature.GroupNameOrDefault());
      DCHECK(field_trial_without_params);
    }

    // Re-assigning field trial parameters is not allowed. Clear
    // all field trial parameters.
    field_trial_param_associator->ClearParamsForTesting(
        feature.StudyNameOrDefault(), feature.GroupNameOrDefault());
  }

  if (keep_existing_states) {
    // Restore other field trials. Note: We don't need to do anything for params
    // here because the param associator already has the right state for these
    // restored trials, which has been backed up via |original_params_| to be
    // restored later.
    FieldTrialList::CreateTrialsFromFieldTrialStates(PassKey(), all_states);
  } else {
    // No need to keep existing field trials. Instead, clear all parameters.
    field_trial_param_associator->ClearAllParamsForTesting();
  }

  // Create enable-features and disable-features arguments.
  OverrideFeatures(parsed_current_enabled_features,
                   FeatureList::OverrideState::OVERRIDE_ENABLE_FEATURE,
                   &merged_features);
  OverrideFeatures(current_disabled_features,
                   FeatureList::OverrideState::OVERRIDE_DISABLE_FEATURE,
                   &merged_features);

  std::string enabled = CreateCommandLineArgumentFromFeatureList(
      merged_features.enabled_feature_list, /*enable_features=*/true);
  std::string disabled = CreateCommandLineArgumentFromFeatureList(
      merged_features.disabled_feature_list, /*enable_features=*/false);

  std::unique_ptr<FeatureList> new_feature_list(new FeatureList);
  new_feature_list->InitFromCommandLine(enabled, disabled);
  InitWithFeatureList(std::move(new_feature_list));
}

}  // namespace base::test
