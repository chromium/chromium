// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/readaloud/android/features_jni_headers/ReadAloudFeatures_jni.h"
#include "chrome/browser/readaloud/android/synthetic_trial.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::JavaParamRef;

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

}  // namespace readaloud
