// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_string.h"
#include "base/files/file_path.h"
// NOTE: This target is transitively depended on by //chrome/browser and thus
// can't depend on it.
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"  // nogncheck
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/content/browser/safe_browsing_service_interface.h"
#include "components/safe_browsing/content/common/file_type_policies.h"
#include "components/safe_browsing/core/common/hashprefix_realtime/hash_realtime_utils.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/browser/web_contents.h"

// Must come after headers that provide symbols used by @JniType.
#include "chrome/browser/safe_browsing/android/jni_headers/SafeBrowsingBridge_jni.h"

namespace safe_browsing {

static int32_t JNI_SafeBrowsingBridge_UmaValueForFile(const std::string& path) {
  return safe_browsing::FileTypePolicies::GetInstance()->UmaValueForFile(
      base::FilePath(path));
}

static bool JNI_SafeBrowsingBridge_GetSafeBrowsingExtendedReportingEnabled(
    Profile* profile) {
  return safe_browsing::IsExtendedReportingEnabled(*profile->GetPrefs());
}

static void JNI_SafeBrowsingBridge_SetSafeBrowsingExtendedReportingEnabled(
    Profile* profile,
    bool enabled) {
  safe_browsing::SetExtendedReportingPrefAndMetric(
      profile->GetPrefs(), enabled,
      safe_browsing::SBER_OPTIN_SITE_ANDROID_SETTINGS);
}

static bool JNI_SafeBrowsingBridge_GetSafeBrowsingExtendedReportingManaged(
    Profile* profile) {
  return profile->GetPrefs()->IsManagedPreference(
      prefs::kSafeBrowsingScoutReportingEnabled);
}

static int32_t JNI_SafeBrowsingBridge_GetSafeBrowsingState(Profile* profile) {
  return static_cast<int32_t>(
      safe_browsing::GetSafeBrowsingState(*profile->GetPrefs()));
}

static void JNI_SafeBrowsingBridge_SetSafeBrowsingState(Profile* profile,
                                                        int32_t state) {
  return safe_browsing::SetSafeBrowsingState(
      profile->GetPrefs(), static_cast<SafeBrowsingState>(state),
      /*is_esb_enabled_by_account_integration=*/false);
}

static void JNI_SafeBrowsingBridge_EnableSafeBrowsingSettingSetLocallyPref(
    Profile* profile) {
  return safe_browsing::EnableSafeBrowsingSettingSetLocallyPref(
      profile->GetPrefs());
}

static bool JNI_SafeBrowsingBridge_IsSafeBrowsingManaged(Profile* profile) {
  return safe_browsing::IsSafeBrowsingPolicyManaged(*profile->GetPrefs());
}

static bool JNI_SafeBrowsingBridge_IsHashRealTimeLookupEligibleInSession() {
  return safe_browsing::hash_realtime_utils::
      IsHashRealTimeLookupEligibleInSession();
}

}  // namespace safe_browsing

DEFINE_JNI(SafeBrowsingBridge)
