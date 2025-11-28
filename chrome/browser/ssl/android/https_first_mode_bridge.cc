// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_string.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/https_first_mode_settings_tracker.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/ssl/android/jni_headers/HttpsFirstModeBridge_jni.h"

using base::android::JavaParamRef;

static jint JNI_HttpsFirstModeBridge_GetCurrentSetting(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_profile) {
  HttpsFirstModeService* hfm_service =
      HttpsFirstModeServiceFactory::GetForProfile(
          Profile::FromJavaObject(j_profile));
  return static_cast<jint>(hfm_service->GetCurrentSetting());
}

static void JNI_HttpsFirstModeBridge_UpdatePrefs(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_profile,
    jint setting) {
  HttpsFirstModeService* hfm_service =
      HttpsFirstModeServiceFactory::GetForProfile(
          Profile::FromJavaObject(j_profile));
  auto selection = static_cast<HttpsFirstModeSetting>(setting);
  hfm_service->UpdatePrefs(selection);
  return;
}

static jboolean JNI_HttpsFirstModeBridge_IsManaged(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_profile) {
  Profile* profile = Profile::FromJavaObject(j_profile);
  return profile->GetPrefs()->IsManagedPreference(prefs::kHttpsOnlyModeEnabled);
}

DEFINE_JNI(HttpsFirstModeBridge)
