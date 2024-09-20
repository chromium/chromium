// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/strings/strcat.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/readaloud/android/synthetic_trial.h"
#include "components/metrics/metrics_service.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/readaloud/android/features_jni_headers/ReadAloudFeatures_jni.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace readaloud {

jlong JNI_ReadAloudFeatures_InitSyntheticTrial(
    JNIEnv* env,
    const JavaParamRef<jstring>& j_feature_name,
    const JavaParamRef<jstring>& j_synthetic_trial_name_suffix) {
  return reinterpret_cast<jlong>(
      SyntheticTrial::Create(
          ConvertJavaStringToUTF8(j_feature_name),
          ConvertJavaStringToUTF8(j_synthetic_trial_name_suffix))
          .release());
}

void JNI_ReadAloudFeatures_ActivateSyntheticTrial(JNIEnv* env,
                                                  jlong j_synthetic_trial_ptr) {
  SyntheticTrial* synthetic_trial =
      reinterpret_cast<SyntheticTrial*>(j_synthetic_trial_ptr);
  if (synthetic_trial) {
    synthetic_trial->Activate();
  }
}

void JNI_ReadAloudFeatures_DestroySyntheticTrial(JNIEnv* env,
                                                 jlong j_synthetic_trial_ptr) {
  SyntheticTrial* synthetic_trial =
      reinterpret_cast<SyntheticTrial*>(j_synthetic_trial_ptr);
  if (synthetic_trial) {
    delete synthetic_trial;
  }
}

void JNI_ReadAloudFeatures_ClearStaleSyntheticTrialPrefs(JNIEnv* env) {
  SyntheticTrial::ClearStalePrefs();
}

ScopedJavaLocalRef<jstring> JNI_ReadAloudFeatures_GetMetricsId(JNIEnv* env) {
  if (g_browser_process && g_browser_process->metrics_service()) {
    return ConvertUTF8ToJavaString(
        env, g_browser_process->metrics_service()->GetClientId());
  }
  return ConvertUTF8ToJavaString(env, "");
}

ScopedJavaLocalRef<jstring> JNI_ReadAloudFeatures_GetServerExperimentFlag(
    JNIEnv* env) {
  base::FieldTrial* trial =
      base::FeatureList::GetInstance()->GetAssociatedFieldTrialByFeatureName(
          chrome::android::kReadAloudServerExperiments.name);
  if (!trial) {
    return ConvertUTF8ToJavaString(env, "");
  }
  return ConvertUTF8ToJavaString(
      env, base::StrCat({trial->trial_name(), "_", trial->group_name()}));
}

}  // namespace readaloud
