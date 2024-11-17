// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/feature_map.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <string_view>

#include "base/android/jni_string.h"
#include "base/metrics/field_trial_params.h"
#include "base/notreached.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "base/base_jni/FeatureMap_jni.h"

namespace base::android {

std::pair<std::string_view, const Feature*> MakeNameToFeaturePair(
    const Feature* feature) {
  return std::make_pair(feature->name, feature);
}

FeatureMap::FeatureMap(std::vector<const Feature*> features_exposed_to_java) {
  mapping_ =
      MakeFlatMap<std::string_view, raw_ptr<const Feature, CtnExperimental>>(
          features_exposed_to_java, {}, &MakeNameToFeaturePair);
}

FeatureMap::~FeatureMap() = default;

const Feature* FeatureMap::FindFeatureExposedToJava(
    const std::string& feature_name) {
  auto it = mapping_.find(feature_name);
  if (it != mapping_.end()) {
    return it->second;
  }

  NOTREACHED() << "Queried feature cannot be found in FeatureMap: "
               << feature_name;
}

static jboolean JNI_FeatureMap_IsEnabled(JNIEnv* env,
                                         jlong jfeature_map,
                                         std::string& feature_name) {
  FeatureMap* feature_map = reinterpret_cast<FeatureMap*>(jfeature_map);
  const base::Feature* feature =
      feature_map->FindFeatureExposedToJava(feature_name);
  return base::FeatureList::IsEnabled(*feature);
}

static std::string JNI_FeatureMap_GetFieldTrialParamByFeature(
    JNIEnv* env,
    jlong jfeature_map,
    std::string& feature_name,
    std::string& param_name) {
  FeatureMap* feature_map = reinterpret_cast<FeatureMap*>(jfeature_map);
  const base::Feature* feature =
      feature_map->FindFeatureExposedToJava(feature_name);
  return base::GetFieldTrialParamValueByFeature(*feature, param_name);
}

static jint JNI_FeatureMap_GetFieldTrialParamByFeatureAsInt(
    JNIEnv* env,
    jlong jfeature_map,
    std::string& feature_name,
    std::string& param_name,
    const jint jdefault_value) {
  FeatureMap* feature_map = reinterpret_cast<FeatureMap*>(jfeature_map);
  const base::Feature* feature =
      feature_map->FindFeatureExposedToJava(feature_name);
  return base::GetFieldTrialParamByFeatureAsInt(*feature, param_name,
                                                jdefault_value);
}

static jdouble JNI_FeatureMap_GetFieldTrialParamByFeatureAsDouble(
    JNIEnv* env,
    jlong jfeature_map,
    std::string& feature_name,
    std::string& param_name,
    const jdouble jdefault_value) {
  FeatureMap* feature_map = reinterpret_cast<FeatureMap*>(jfeature_map);
  const base::Feature* feature =
      feature_map->FindFeatureExposedToJava(feature_name);
  return base::GetFieldTrialParamByFeatureAsDouble(*feature, param_name,
                                                   jdefault_value);
}

static jboolean JNI_FeatureMap_GetFieldTrialParamByFeatureAsBoolean(
    JNIEnv* env,
    jlong jfeature_map,
    std::string& feature_name,
    std::string& param_name,
    const jboolean jdefault_value) {
  FeatureMap* feature_map = reinterpret_cast<FeatureMap*>(jfeature_map);
  const base::Feature* feature =
      feature_map->FindFeatureExposedToJava(feature_name);
  return base::GetFieldTrialParamByFeatureAsBool(*feature, param_name,
                                                 jdefault_value);
}

std::vector<std::string> JNI_FeatureMap_GetFlattedFieldTrialParamsForFeature(
    JNIEnv* env,
    jlong jfeature_map,
    std::string& feature_name) {
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

}  // namespace base::android
