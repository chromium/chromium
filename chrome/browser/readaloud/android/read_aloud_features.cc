// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/strings/strcat.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "components/metrics/metrics_service.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/readaloud/android/features_jni_headers/ReadAloudFeatures_jni.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace readaloud {

static ScopedJavaLocalRef<jstring> JNI_ReadAloudFeatures_GetMetricsId(
    JNIEnv* env) {
  if (g_browser_process && g_browser_process->metrics_service()) {
    return ConvertUTF8ToJavaString(
        env, g_browser_process->metrics_service()->GetClientId());
  }
  return ConvertUTF8ToJavaString(env, "");
}

static ScopedJavaLocalRef<jstring>
JNI_ReadAloudFeatures_GetServerExperimentFlag(JNIEnv* env) {
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

DEFINE_JNI(ReadAloudFeatures)
