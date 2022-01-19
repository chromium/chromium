// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/signin/fre_mobile_identity_consistency_field_trial.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/browser/signin/services/android/jni_headers/FREMobileIdentityConsistencyFieldTrial_jni.h"

namespace fre_mobile_identity_consistency_field_trial {

std::string GetFREFieldTrialGroup() {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> group =
      Java_FREMobileIdentityConsistencyFieldTrial_getFirstRunTrialGroup(env);
  return base::android::ConvertJavaStringToUTF8(env, group);
}

bool IsFREFieldTrialEnabled() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_FREMobileIdentityConsistencyFieldTrial_isEnabled(env);
}

std::string GetFREVariationsFieldTrialGroup() {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> group =
      Java_FREMobileIdentityConsistencyFieldTrial_getFirstRunVariationsTrialGroup(
          env);
  return base::android::ConvertJavaStringToUTF8(env, group);
}

}  // namespace fre_mobile_identity_consistency_field_trial
