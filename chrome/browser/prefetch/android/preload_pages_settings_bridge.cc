// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>

#include "chrome/browser/prefetch/android/jni_headers/PreloadPagesSettingsBridge_jni.h"
#include "chrome/browser/prefetch/pref_names.h"
#include "chrome/browser/prefetch/prefetch_prefs.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_service.h"

namespace {

PrefService* GetPrefService() {
  return ProfileManager::GetActiveUserProfile()
      ->GetOriginalProfile()
      ->GetPrefs();
}

}  // namespace

namespace prefetch {

static jint JNI_PreloadPagesSettingsBridge_GetState(JNIEnv* env) {
  return static_cast<int>(prefetch::GetPreloadPagesState(*GetPrefService()));
}

static jboolean JNI_PreloadPagesSettingsBridge_IsNetworkPredictionManaged(
    JNIEnv* env) {
  return GetPrefService()->IsManagedPreference(
      prefetch::prefs::kNetworkPredictionOptions);
}

static void JNI_PreloadPagesSettingsBridge_SetState(JNIEnv* env, jint state) {
  prefetch::SetPreloadPagesState(
      GetPrefService(), static_cast<prefetch::PreloadPagesState>(state));
}

}  // namespace prefetch
