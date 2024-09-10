// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/field_trial_params.h"

#include <optional>
#include <set>
#include <string_view>
#include <utility>
#include <vector>

#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/features.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_param_associator.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/metrics_hashes.h"
#include "base/strings/escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/time/time_delta_from_string.h"

namespace base {

namespace internal {

bool IsFeatureParamWithCacheEnabled() {
  return FeatureList::IsEnabled(features::kFeatureParamWithCache);
}

}  // namespace internal

void LogInvalidValue(const Feature& feature,
                     const char* type,
                     const std::string& param_name,
                     const std::string& value_as_string,
                     const std::string& default_value_as_string) {
  UmaHistogramSparse("Variations.FieldTriamParamsLogInvalidValue",
                     static_cast<int>(base::HashFieldTrialName(
                         FeatureList::GetFieldTrial(feature)->trial_name())));
  // To anyone noticing these crash dumps in the wild, these parameters come
  // from server-side experiment configuration. If you're seeing an increase it
  // is likely due to a bad experiment rollout rather than changes in the client
  // code.
  SCOPED_CRASH_KEY_STRING32("FieldTrialParams", "feature_name", feature.name);
  SCOPED_CRASH_KEY_STRING32("FieldTrialParams", "param_name", param_name);
  SCOPED_CRASH_KEY_STRING32("FieldTrialParams", "value", value_as_string);
  SCOPED_CRASH_KEY_STRING32("FieldTrialParams", "default",
                            default_value_as_string);
  LOG(ERROR) << "Failed to parse field trial param " << param_name
             << " with string value " << value_as_string << " under feature "
             << feature.name << " into " << type
             << ". Falling back to default value of "
             << default_value_as_string;
  base::debug::DumpWithoutCrashing();
}

std::string UnescapeValue(const std::string& value) {
  return UnescapeURLComponent(
      value, UnescapeRule::PATH_SEPARATORS |
                 UnescapeRule::URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS);
}

bool AssociateFieldTrialParams(const std::string& trial_name,
                               const std::string& group_name,
                               const FieldTrialParams& params) {
  return FieldTrialParamAssociator::GetInstance()->AssociateFieldTrialParams(
      trial_name, group_name, params);
}

bool AssociateFieldTrialParamsFromString(
    const std::string& params_string,
    FieldTrialParamsDecodeStringFunc decode_data_func) {
  // Format: Trial1.Group1:k1/v1/k2/v2,Trial2.Group2:k1/v1/k2/v2
  std::set<std::pair<std::string, std::string>> trial_groups;
  for (std::string_view experiment_group :
       SplitStringPiece(params_string, ",", TRIM_WHITESPACE, SPLIT_WANT_ALL)) {
    std::vector<std::string_view> experiment = SplitStringPiece(
        experiment_group, ":", TRIM_WHITESPACE, SPLIT_WANT_ALL);
    if (experiment.size() != 2) {
      DLOG(ERROR) << "Experiment and params should be separated by ':'";
      return false;
    }

    std::vector<std::string> group_parts =
        SplitString(experiment[0], ".", TRIM_WHITESPACE, SPLIT_WANT_ALL);
    if (group_parts.size() != 2) {
      DLOG(ERROR) << "Trial and group name should be separated by '.'";
      return false;
    }

    std::vector<std::string> key_values =
        SplitString(experiment[1], "/", TRIM_WHITESPACE, SPLIT_WANT_ALL);
    if (key_values.size() % 2 != 0) {
      DLOG(ERROR) << "Param name and param value should be separated by '/'";
      return false;
    }
    std::string trial = decode_data_func(group_parts[0]);
    std::string group = decode_data_func(group_parts[1]);
    auto trial_group = std::make_pair(trial, group);
    if (trial_groups.find(trial_group) != trial_groups.end()) {
      DLOG(ERROR) << StringPrintf(
          "A (trial, group) pair listed more than once. (%s, %s)",
          trial.c_str(), group.c_str());
      return false;
    }
    trial_groups.insert(trial_group);
    std::map<std::string, std::string> params;
    for (size_t i = 0; i < key_values.size(); i += 2) {
      std::string key = decode_data_func(key_values[i]);
      std::string value = decode_data_func(key_values[i + 1]);
      params[key] = value;
    }
    bool result = AssociateFieldTrialParams(trial, group, params);
    if (!result) {
      DLOG(ERROR) << "Failed to associate field trial params for group \""
                  << group << "\" in trial \"" << trial << "\"";
      return false;
    }
  }
  return true;
}

bool GetFieldTrialParams(const std::string& trial_name,
                         FieldTrialParams* params) {
  FieldTrial* trial = FieldTrialList::Find(trial_name);
  return FieldTrialParamAssociator::GetInstance()->GetFieldTrialParams(trial,
                                                                       params);
}

bool GetFieldTrialParamsByFeature(const Feature& feature,
                                  FieldTrialParams* params) {
  if (!FeatureList::IsEnabled(feature)) {
    return false;
  }

  FieldTrial* trial = FeatureList::GetFieldTrial(feature);
  return FieldTrialParamAssociator::GetInstance()->GetFieldTrialParams(trial,
                                                                       params);
}

std::string GetFieldTrialParamValue(const std::string& trial_name,
                                    const std::string& param_name) {
  FieldTrialParams params;
  if (GetFieldTrialParams(trial_name, &params)) {
    auto it = params.find(param_name);
    if (it != params.end()) {
      return it->second;
    }
  }
  return std::string();
}

std::string GetFieldTrialParamValueByFeature(const Feature& feature,
                                             const std::string& param_name) {
  FieldTrialParams params;
  if (GetFieldTrialParamsByFeature(feature, &params)) {
    auto it = params.find(param_name);
    if (it != params.end()) {
      return it->second;
    }
  }
  return std::string();
}

std::string GetFieldTrialParamByFeatureAsString(
    const Feature& feature,
    const std::string& param_name,
    const std::string& default_value) {
  FieldTrialParams params;
  if (!GetFieldTrialParamsByFeature(feature, &params)) {
    return default_value;
  }
  auto it = params.find(param_name);
  if (it == params.end()) {
    return default_value;
  }
  return it->second;
}

int GetFieldTrialParamByFeatureAsInt(const Feature& feature,
                                     const std::string& param_name,
                                     int default_value) {
  std::string value_as_string =
      GetFieldTrialParamValueByFeature(feature, param_name);
  int value_as_int = 0;
  if (!StringToInt(value_as_string, &value_as_int)) {
    if (!value_as_string.empty()) {
      LogInvalidValue(feature, "an int", param_name, value_as_string,
                      base::NumberToString(default_value));
    }
    value_as_int = default_value;
  }
  return value_as_int;
}

double GetFieldTrialParamByFeatureAsDouble(const Feature& feature,
                                           const std::string& param_name,
                                           double default_value) {
  std::string value_as_string =
      GetFieldTrialParamValueByFeature(feature, param_name);
  double value_as_double = 0;
  if (!StringToDouble(value_as_string, &value_as_double)) {
    if (!value_as_string.empty()) {
      LogInvalidValue(feature, "a double", param_name, value_as_string,
                      base::NumberToString(default_value));
    }
    value_as_double = default_value;
  }
  return value_as_double;
}

bool GetFieldTrialParamByFeatureAsBool(const Feature& feature,
                                       const std::string& param_name,
                                       bool default_value) {
  std::string value_as_string =
      GetFieldTrialParamValueByFeature(feature, param_name);
  if (value_as_string == "true") {
    return true;
  }
  if (value_as_string == "false") {
    return false;
  }

  if (!value_as_string.empty()) {
    LogInvalidValue(feature, "a bool", param_name, value_as_string,
                    default_value ? "true" : "false");
  }
  return default_value;
}

base::TimeDelta GetFieldTrialParamByFeatureAsTimeDelta(
    const Feature& feature,
    const std::string& param_name,
    base::TimeDelta default_value) {
  std::string value_as_string =
      GetFieldTrialParamValueByFeature(feature, param_name);

  if (value_as_string.empty()) {
    return default_value;
  }

  std::optional<base::TimeDelta> ret = TimeDeltaFromString(value_as_string);
  if (!ret.has_value()) {
    LogInvalidValue(feature, "a base::TimeDelta", param_name, value_as_string,
                    base::NumberToString(default_value.InSecondsF()) + " s");
    return default_value;
  }

  return ret.value();
}

template <>
bool FeatureParam<bool>::GetWithoutCache() const {
  return GetFieldTrialParamByFeatureAsBool(*feature, name, default_value);
}

template <>
int FeatureParam<int>::GetWithoutCache() const {
  return GetFieldTrialParamByFeatureAsInt(*feature, name, default_value);
}

template <>
size_t FeatureParam<size_t>::GetWithoutCache() const {
  return checked_cast<size_t>(GetFieldTrialParamByFeatureAsInt(
      *feature, name, checked_cast<int>(default_value)));
}

template <>
double FeatureParam<double>::GetWithoutCache() const {
  return GetFieldTrialParamByFeatureAsDouble(*feature, name, default_value);
}

template <>
std::string FeatureParam<std::string>::GetWithoutCache() const {
  return GetFieldTrialParamByFeatureAsString(*feature, name, default_value);
}

template <>
base::TimeDelta FeatureParam<base::TimeDelta>::GetWithoutCache() const {
  return GetFieldTrialParamByFeatureAsTimeDelta(*feature, name, default_value);
}

void LogInvalidEnumValue(const Feature& feature,
                         const std::string& param_name,
                         const std::string& value_as_string,
                         int default_value_as_int) {
  LogInvalidValue(feature, "an enum", param_name, value_as_string,
                  base::NumberToString(default_value_as_int));
}

}  // namespace base
