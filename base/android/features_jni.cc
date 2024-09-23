// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_string.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "base/base_jni/Features_jni.h"

namespace base {
namespace android {

jboolean JNI_Features_IsEnabled(JNIEnv* env, jlong native_feature_pointer) {
  return base::FeatureList::IsEnabled(
      *reinterpret_cast<base::Feature*>(native_feature_pointer));
}

jboolean JNI_Features_GetFieldTrialParamByFeatureAsBoolean(
    JNIEnv* env,
    jlong native_feature_pointer,
    std::string& param_name,
    const jboolean jdefault_value) {
  const base::Feature& feature =
      *reinterpret_cast<base::Feature*>(native_feature_pointer);
  return base::GetFieldTrialParamByFeatureAsBool(feature, param_name,
                                                 jdefault_value);
}

std::string JNI_Features_GetFieldTrialParamByFeatureAsString(
    JNIEnv* env,
    jlong native_feature_pointer,
    std::string& param_name) {
  const base::Feature& feature =
      *reinterpret_cast<base::Feature*>(native_feature_pointer);
  return base::GetFieldTrialParamValueByFeature(feature, param_name);
}

}  // namespace android
}  // namespace base
