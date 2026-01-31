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

using base::android::JavaRef;

static int32_t JNI_HttpsFirstModeBridge_GetCurrentSetting(
    JNIEnv* env,
    const JavaRef<jobject>& j_profile) {
  HttpsFirstModeService* hfm_service =
      HttpsFirstModeServiceFactory::GetForProfile(
          Profile::FromJavaObject(j_profile));
  return static_cast<int32_t>(hfm_service->GetCurrentSetting());
}

static void JNI_HttpsFirstModeBridge_UpdatePrefs(
    JNIEnv* env,
    const JavaRef<jobject>& j_profile,
    int32_t setting) {
  HttpsFirstModeService* hfm_service =
      HttpsFirstModeServiceFactory::GetForProfile(
          Profile::FromJavaObject(j_profile));
  auto selection = static_cast<HttpsFirstModeSetting>(setting);
  hfm_service->UpdatePrefs(selection);
  return;
}

static bool JNI_HttpsFirstModeBridge_IsManaged(
    JNIEnv* env,
    const JavaRef<jobject>& j_profile) {
  Profile* profile = Profile::FromJavaObject(j_profile);
  return profile->GetPrefs()->IsManagedPreference(prefs::kHttpsOnlyModeEnabled);
}

DEFINE_JNI(HttpsFirstModeBridge)
