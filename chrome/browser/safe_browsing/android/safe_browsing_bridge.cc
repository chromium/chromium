// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_string.h"
#include "base/files/file_path.h"
#include "chrome/android/chrome_jni_headers/SafeBrowsingBridge_jni.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/safe_browsing/file_type_policies.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"

using base::android::JavaParamRef;

namespace {

PrefService* GetPrefService() {
  return ProfileManager::GetActiveUserProfile()
      ->GetOriginalProfile()
      ->GetPrefs();
}

}  // namespace

namespace safe_browsing {

static jint JNI_SafeBrowsingBridge_UmaValueForFile(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& path) {
  base::FilePath file_path(ConvertJavaStringToUTF8(env, path));
  return safe_browsing::FileTypePolicies::GetInstance()->UmaValueForFile(
      file_path);
}

static jboolean JNI_SafeBrowsingBridge_GetSafeBrowsingExtendedReportingEnabled(
    JNIEnv* env) {
  return safe_browsing::IsExtendedReportingEnabled(*GetPrefService());
}

static void JNI_SafeBrowsingBridge_SetSafeBrowsingExtendedReportingEnabled(
    JNIEnv* env,
    jboolean enabled) {
  safe_browsing::SetExtendedReportingPrefAndMetric(
      GetPrefService(), enabled,
      safe_browsing::SBER_OPTIN_SITE_ANDROID_SETTINGS);
}

static jboolean JNI_SafeBrowsingBridge_GetSafeBrowsingExtendedReportingManaged(
    JNIEnv* env) {
  PrefService* pref_service = GetPrefService();
  return pref_service->IsManagedPreference(
      prefs::kSafeBrowsingScoutReportingEnabled);
}

}  // namespace safe_browsing
