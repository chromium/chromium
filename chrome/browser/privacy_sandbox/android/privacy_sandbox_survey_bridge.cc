// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/no_destructor.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_survey_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/privacy_sandbox/privacy_sandbox_survey_service.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/privacy_sandbox/android/jni_headers/PrivacySandboxSurveyBridge_jni.h"

using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace {

privacy_sandbox::PrivacySandboxSurveyService* GetSurveyService(
    const base::android::JavaRef<jobject>& j_profile) {
  return PrivacySandboxSurveyFactory::GetForProfile(
      Profile::FromJavaObject(j_profile));
}

}  // namespace

static jni_zero::ScopedJavaLocalRef<jobject>
JNI_PrivacySandboxSurveyBridge_GetPrivacySandboxSentimentSurveyPsb(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_profile) {
  ScopedJavaLocalRef<jobject> product_specific_bits =
      Java_PrivacySandboxSurveyBridge_createSentimentSurveyPsb(env);
  if (!GetSurveyService(j_profile)) {
    return product_specific_bits;
  }
  for (const auto& entry :
       GetSurveyService(j_profile)->GetSentimentSurveyPsb()) {
    Java_PrivacySandboxSurveyBridge_insertSurveyBitIntoMap(
        env, product_specific_bits, ConvertUTF8ToJavaString(env, entry.first),
        entry.second);
  }
  return product_specific_bits;
}
