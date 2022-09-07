// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_jni_headers/Features_jni.h"
#include "base/android/jni_string.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace base {
namespace android {

jboolean JNI_Features_IsEnabled(JNIEnv* env, jlong native_feature_pointer) {
  return base::FeatureList::IsEnabled(
      *reinterpret_cast<base::Feature*>(native_feature_pointer));
}

jboolean JNI_Features_GetFieldTrialParamByFeatureAsBoolean(
    JNIEnv* env,
    jlong native_feature_pointer,
    const JavaParamRef<jstring>& jparam_name,
    const jboolean jdefault_value) {
  const base::Feature& feature =
      *reinterpret_cast<base::Feature*>(native_feature_pointer);
  const std::string& param_name = ConvertJavaStringToUTF8(env, jparam_name);
  return base::GetFieldTrialParamByFeatureAsBool(feature, param_name,
                                                 jdefault_value);
}

}  // namespace android
}  // namespace base
