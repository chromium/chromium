// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/field_trial_params.h"

#include <set>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_param_associator.h"
#include "base/strings/escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/time/time_delta_from_string.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {

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
  for (StringPiece experiment_group :
       SplitStringPiece(params_string, ",", TRIM_WHITESPACE, SPLIT_WANT_ALL)) {
    std::vector<StringPiece> experiment = SplitStringPiece(
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
  if (!FeatureList::IsEnabled(feature))
    return false;

  FieldTrial* trial = FeatureList::GetFieldTrial(feature);
  return FieldTrialParamAssociator::GetInstance()->GetFieldTrialParams(trial,
                                                                       params);
}

std::string GetFieldTrialParamValue(const std::string& trial_name,
                                    const std::string& param_name) {
  FieldTrialParams params;
  if (GetFieldTrialParams(trial_name, &params)) {
    auto it = params.find(param_name);
    if (it != params.end())
      return it->second;
  }
  return std::string();
}

std::string GetFieldTrialParamValueByFeature(const Feature& feature,
                                             const std::string& param_name) {
  FieldTrialParams params;
  if (GetFieldTrialParamsByFeature(feature, &params)) {
    auto it = params.find(param_name);
    if (it != params.end())
      return it->second;
  }
  return std::string();
}

int GetFieldTrialParamByFeatureAsInt(const Feature& feature,
                                     const std::string& param_name,
                                     int default_value) {
  std::string value_as_string =
      GetFieldTrialParamValueByFeature(feature, param_name);
  int value_as_int = 0;
  if (!StringToInt(value_as_string, &value_as_int)) {
    if (!value_as_string.empty()) {
      DLOG(WARNING) << "Failed to parse field trial param " << param_name
                    << " with string value " << value_as_string
                    << " under feature " << feature.name
                    << " into an int. Falling back to default value of "
                    << default_value;
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
      DLOG(WARNING) << "Failed to parse field trial param " << param_name
                    << " with string value " << value_as_string
                    << " under feature " << feature.name
                    << " into a double. Falling back to default value of "
                    << default_value;
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
  if (value_as_string == "true")
    return true;
  if (value_as_string == "false")
    return false;

  if (!value_as_string.empty()) {
    DLOG(WARNING) << "Failed to parse field trial param " << param_name
                  << " with string value " << value_as_string
                  << " under feature " << feature.name
                  << " into a bool. Falling back to default value of "
                  << default_value;
  }
  return default_value;
}

base::TimeDelta GetFieldTrialParamByFeatureAsTimeDelta(
    const Feature& feature,
    const std::string& param_name,
    base::TimeDelta default_value) {
  std::string value_as_string =
      GetFieldTrialParamValueByFeature(feature, param_name);

  if (value_as_string.empty())
    return default_value;

  absl::optional<base::TimeDelta> ret = TimeDeltaFromString(value_as_string);
  if (!ret.has_value()) {
    DLOG(WARNING)
        << "Failed to parse field trial param " << param_name
        << " with string value " << value_as_string << " under feature "
        << feature.name
        << " into a base::TimeDelta. Falling back to default value of "
        << default_value;
    return default_value;
  }

  return ret.value();
}

std::string FeatureParam<std::string>::Get() const {
  const std::string value = GetFieldTrialParamValueByFeature(*feature, name);
  return value.empty() ? default_value : value;
}

double FeatureParam<double>::Get() const {
  return GetFieldTrialParamByFeatureAsDouble(*feature, name, default_value);
}

int FeatureParam<int>::Get() const {
  return GetFieldTrialParamByFeatureAsInt(*feature, name, default_value);
}

bool FeatureParam<bool>::Get() const {
  return GetFieldTrialParamByFeatureAsBool(*feature, name, default_value);
}

base::TimeDelta FeatureParam<base::TimeDelta>::Get() const {
  return GetFieldTrialParamByFeatureAsTimeDelta(*feature, name, default_value);
}

void LogInvalidEnumValue(const Feature& feature,
                         const std::string& param_name,
                         const std::string& value_as_string,
                         int default_value_as_int) {
  DLOG(WARNING) << "Failed to parse field trial param " << param_name
                << " with string value " << value_as_string << " under feature "
                << feature.name
                << " into an enum. Falling back to default value of "
                << default_value_as_int;
}

}  // namespace base
