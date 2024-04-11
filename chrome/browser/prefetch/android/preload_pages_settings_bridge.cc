// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>

#include "chrome/browser/prefetch/android/jni_headers/PreloadPagesSettingsBridge_jni.h"
#include "chrome/browser/prefetch/pref_names.h"
#include "chrome/browser/preloading/preloading_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_service.h"

namespace {

PrefService* GetPrefService(const base::android::JavaRef<jobject>& j_profile) {
  return ProfileAndroid::FromProfileAndroid(j_profile)->GetPrefs();
}

}  // namespace

namespace prefetch {

static jint JNI_PreloadPagesSettingsBridge_GetState(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_profile) {
  return static_cast<int>(
      prefetch::GetPreloadPagesState(*GetPrefService(j_profile)));
}

static jboolean JNI_PreloadPagesSettingsBridge_IsNetworkPredictionManaged(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_profile) {
  return GetPrefService(j_profile)->IsManagedPreference(
      prefetch::prefs::kNetworkPredictionOptions);
}

static void JNI_PreloadPagesSettingsBridge_SetState(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_profile,
    jint state) {
  prefetch::SetPreloadPagesState(
      GetPrefService(j_profile),
      static_cast<prefetch::PreloadPagesState>(state));
}

}  // namespace prefetch
