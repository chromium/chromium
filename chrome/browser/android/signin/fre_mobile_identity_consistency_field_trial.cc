// Copyright 2021 The Chromium Authors
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

variations::VariationID GetFREFieldTrialVariationId(int low_entropy_source,
                                                    int low_entropy_size) {
  JNIEnv* env = base::android::AttachCurrentThread();
  jint variation_id =
      Java_FREMobileIdentityConsistencyFieldTrial_getFirstRunTrialVariationId(
          env, low_entropy_source, low_entropy_size);
  return static_cast<variations::VariationID>(variation_id);
}

}  // namespace fre_mobile_identity_consistency_field_trial
