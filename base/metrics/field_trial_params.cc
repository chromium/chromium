// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/field_trial_params.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_param_associator.h"
#include "base/strings/string_number_conversions.h"

namespace base {

bool AssociateFieldTrialParams(
    const std::string& trial_name,
    const std::string& group_name,
    const std::map<std::string, std::string>& params) {
  return base::FieldTrialParamAssociator::GetInstance()
      ->AssociateFieldTrialParams(trial_name, group_name, params);
}

bool GetFieldTrialParams(const std::string& trial_name,
                         std::map<std::string, std::string>* params) {
  return base::FieldTrialParamAssociator::GetInstance()->GetFieldTrialParams(
      trial_name, params);
}

bool GetFieldTrialParamsByFeature(const base::Feature& feature,
                                  std::map<std::string, std::string>* params) {
  if (!base::FeatureList::IsEnabled(feature))
    return false;

  base::FieldTrial* trial = base::FeatureList::GetFieldTrial(feature);
  if (!trial)
    return false;

  return GetFieldTrialParams(trial->trial_name(), params);
}

std::string GetFieldTrialParamValue(const std::string& trial_name,
                                    const std::string& param_name) {
  std::map<std::string, std::string> params;
  if (GetFieldTrialParams(trial_name, &params)) {
    auto it = params.find(param_name);
    if (it != params.end())
      return it->second;
  }
  return std::string();
}

std::string GetFieldTrialParamValueByFeature(const base::Feature& feature,
                                             const std::string& param_name) {
  if (!base::FeatureList::IsEnabled(feature))
    return std::string();

  base::FieldTrial* trial = base::FeatureList::GetFieldTrial(feature);
  if (!trial)
    return std::string();

  return GetFieldTrialParamValue(trial->trial_name(), param_name);
}

int GetFieldTrialParamByFeatureAsInt(const base::Feature& feature,
                                     const std::string& param_name,
                                     int default_value) {
  std::string value_as_string =
      GetFieldTrialParamValueByFeature(feature, param_name);
  int value_as_int = 0;
  if (!base::StringToInt(value_as_string, &value_as_int)) {
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

double GetFieldTrialParamByFeatureAsDouble(const base::Feature& feature,
                                           const std::string& param_name,
                                           double default_value) {
  std::string value_as_string =
      GetFieldTrialParamValueByFeature(feature, param_name);
  double value_as_double = 0;
  if (!base::StringToDouble(value_as_string, &value_as_double)) {
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

bool GetFieldTrialParamByFeatureAsBool(const base::Feature& feature,
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

void LogInvalidEnumValue(const base::Feature& feature,
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
