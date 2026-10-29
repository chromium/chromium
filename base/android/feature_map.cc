// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/feature_map.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/android/jni_string.h"
#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/strcat.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "base/features_jni/FeatureMap_jni.h"

namespace base::android {

namespace {

std::pair<std::string_view, const Feature*> MakeNameToFeaturePair(
    const Feature* feature) {
  return std::make_pair(feature->name, feature);
}

template <typename T>
using ParamMapping =
    flat_map<std::string, raw_ptr<const FeatureParam<T>, CtnExperimental>>;

std::string MakeParamKey(std::string_view feature_name,
                         std::string_view param_name) {
  return base::StrCat({feature_name, ":", param_name});
}

template <typename T>
ParamMapping<T> MakeParamMapping(
    base::span<const FeatureParam<T>* const> params_exposed_to_java) {
  return MakeFlatMap<std::string,
                     raw_ptr<const FeatureParam<T>, CtnExperimental>>(
      params_exposed_to_java, {}, [](const FeatureParam<T>* param) {
        return std::make_pair(MakeParamKey(param->feature->name, param->name),
                              param);
      });
}

template <typename T>
const FeatureParam<T>* FindParam(const ParamMapping<T>& mapping,
                                 const std::string& feature_name,
                                 const std::string& param_name) {
  std::string key = MakeParamKey(feature_name, param_name);
  auto it = mapping.find(key);
  if (it != mapping.end()) {
    return it->second;
  }

  LOG(FATAL) << "Queried param cannot be found in FeatureMap: " << key;
}

}  // namespace

FeatureMap::FeatureMap(
    base::span<const Feature* const> features_exposed_to_java,
    base::span<const FeatureParam<bool>* const> boolean_params_exposed_to_java,
    base::span<const FeatureParam<int>* const> int_params_exposed_to_java,
    base::span<const FeatureParam<double>* const> double_params_exposed_to_java,
    base::span<const FeatureParam<std::string>* const>
        string_params_exposed_to_java) {
  mapping_ =
      MakeFlatMap<std::string_view, raw_ptr<const Feature, CtnExperimental>>(
          features_exposed_to_java, {}, &MakeNameToFeaturePair);
  boolean_params_mapping_ = MakeParamMapping(boolean_params_exposed_to_java);
  int_params_mapping_ = MakeParamMapping(int_params_exposed_to_java);
  double_params_mapping_ = MakeParamMapping(double_params_exposed_to_java);
  string_params_mapping_ = MakeParamMapping(string_params_exposed_to_java);
}

FeatureMap::~FeatureMap() = default;

const Feature* FeatureMap::FindFeatureExposedToJava(
    const std::string& feature_name) {
  auto it = mapping_.find(feature_name);
  if (it != mapping_.end()) {
    return it->second;
  }

  LOG(FATAL) << "Queried feature cannot be found in FeatureMap: "
             << feature_name;
}

const FeatureParam<bool>* FeatureMap::FindBooleanParamExposedToJava(
    const std::string& feature_name,
    const std::string& param_name) {
  return FindParam(boolean_params_mapping_, feature_name, param_name);
}

const FeatureParam<int>* FeatureMap::FindIntParamExposedToJava(
    const std::string& feature_name,
    const std::string& param_name) {
  return FindParam(int_params_mapping_, feature_name, param_name);
}

const FeatureParam<double>* FeatureMap::FindDoubleParamExposedToJava(
    const std::string& feature_name,
    const std::string& param_name) {
  return FindParam(double_params_mapping_, feature_name, param_name);
}

const FeatureParam<std::string>* FeatureMap::FindStringParamExposedToJava(
    const std::string& feature_name,
    const std::string& param_name) {
  return FindParam(string_params_mapping_, feature_name, param_name);
}

static bool JNI_FeatureMap_IsEnabled(int64_t jfeature_map,
                                     const std::string& feature_name) {
  FeatureMap* feature_map = reinterpret_cast<FeatureMap*>(jfeature_map);
  const base::Feature* feature =
      feature_map->FindFeatureExposedToJava(feature_name);
  return base::FeatureList::IsEnabled(*feature);
}

static std::string JNI_FeatureMap_GetFieldTrialParamByFeature(
    int64_t jfeature_map,
    const std::string& feature_name,
    const std::string& param_name) {
  FeatureMap* feature_map = reinterpret_cast<FeatureMap*>(jfeature_map);
  const base::Feature* feature =
      feature_map->FindFeatureExposedToJava(feature_name);
  return base::GetFieldTrialParamValueByFeature(*feature, param_name);
}

static int32_t JNI_FeatureMap_GetFieldTrialParamByFeatureAsInt(
    int64_t jfeature_map,
    const std::string& feature_name,
    const std::string& param_name,
    const int32_t jdefault_value) {
  FeatureMap* feature_map = reinterpret_cast<FeatureMap*>(jfeature_map);
  const base::Feature* feature =
      feature_map->FindFeatureExposedToJava(feature_name);
  return base::GetFieldTrialParamByFeatureAsInt(*feature, param_name,
                                                jdefault_value);
}

static double JNI_FeatureMap_GetFieldTrialParamByFeatureAsDouble(
    int64_t jfeature_map,
    const std::string& feature_name,
    const std::string& param_name,
    const double jdefault_value) {
  FeatureMap* feature_map = reinterpret_cast<FeatureMap*>(jfeature_map);
  const base::Feature* feature =
      feature_map->FindFeatureExposedToJava(feature_name);
  return base::GetFieldTrialParamByFeatureAsDouble(*feature, param_name,
                                                   jdefault_value);
}

static bool JNI_FeatureMap_GetFieldTrialParamByFeatureAsBoolean(
    int64_t jfeature_map,
    const std::string& feature_name,
    const std::string& param_name,
    const bool jdefault_value) {
  FeatureMap* feature_map = reinterpret_cast<FeatureMap*>(jfeature_map);
  const base::Feature* feature =
      feature_map->FindFeatureExposedToJava(feature_name);
  return base::GetFieldTrialParamByFeatureAsBool(*feature, param_name,
                                                 jdefault_value);
}

static std::string JNI_FeatureMap_GetFieldTrialParamByFeatureAsString(
    int64_t jfeature_map,
    const std::string& feature_name,
    const std::string& param_name,
    const std::string& jdefault_value) {
  FeatureMap* feature_map = reinterpret_cast<FeatureMap*>(jfeature_map);
  const base::Feature* feature =
      feature_map->FindFeatureExposedToJava(feature_name);
  return base::GetFieldTrialParamByFeatureAsString(*feature, param_name,
                                                   jdefault_value);
}

static std::vector<std::string>
JNI_FeatureMap_GetFlattedFieldTrialParamsForFeature(
    int64_t jfeature_map,
    const std::string& feature_name) {
  FeatureMap* feature_map = reinterpret_cast<FeatureMap*>(jfeature_map);
  base::FieldTrialParams params;
  std::vector<std::string> keys_and_values;
  const base::Feature* feature =
      feature_map->FindFeatureExposedToJava(feature_name);
  if (feature && base::GetFieldTrialParamsByFeature(*feature, &params)) {
    for (const auto& param_pair : params) {
      keys_and_values.push_back(param_pair.first);
      keys_and_values.push_back(param_pair.second);
    }
  }
  return keys_and_values;
}

static bool JNI_FeatureMap_GetParamValueAsBoolean(
    int64_t jfeature_map,
    const std::string& feature_name,
    const std::string& param_name) {
  FeatureMap* feature_map = reinterpret_cast<FeatureMap*>(jfeature_map);
  const base::FeatureParam<bool>* param =
      feature_map->FindBooleanParamExposedToJava(feature_name, param_name);
  return param->Get();
}

static int32_t JNI_FeatureMap_GetParamValueAsInt(
    int64_t jfeature_map,
    const std::string& feature_name,
    const std::string& param_name) {
  FeatureMap* feature_map = reinterpret_cast<FeatureMap*>(jfeature_map);
  const base::FeatureParam<int>* param =
      feature_map->FindIntParamExposedToJava(feature_name, param_name);
  return param->Get();
}

static double JNI_FeatureMap_GetParamValueAsDouble(
    int64_t jfeature_map,
    const std::string& feature_name,
    const std::string& param_name) {
  FeatureMap* feature_map = reinterpret_cast<FeatureMap*>(jfeature_map);
  const base::FeatureParam<double>* param =
      feature_map->FindDoubleParamExposedToJava(feature_name, param_name);
  return param->Get();
}

static std::string JNI_FeatureMap_GetParamValueAsString(
    int64_t jfeature_map,
    const std::string& feature_name,
    const std::string& param_name) {
  FeatureMap* feature_map = reinterpret_cast<FeatureMap*>(jfeature_map);
  const base::FeatureParam<std::string>* param =
      feature_map->FindStringParamExposedToJava(feature_name, param_name);
  return param->Get();
}

}  // namespace base::android

DEFINE_JNI(FeatureMap)
