// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/incognito/android/incognito_utils_android.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "components/policy/core/browser/url_list/url_list_policy_pref_names.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/incognito/jni_headers/IncognitoUtils_jni.h"

namespace chrome::android {

bool ShouldOpenIncognitoAsWindow() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_IncognitoUtils_shouldOpenIncognitoAsWindow(env);
}

}  // namespace chrome::android

static bool JNI_IncognitoUtils_GetIncognitoModeEnabled(JNIEnv* env,
                                                       Profile* profile) {
  policy::IncognitoModeAvailability incognito_pref =
      IncognitoModePrefs::GetAvailability(profile);
  DCHECK(incognito_pref == policy::IncognitoModeAvailability::kEnabled ||
         incognito_pref == policy::IncognitoModeAvailability::kDisabled ||
         incognito_pref == policy::IncognitoModeAvailability::kForced)
      << "Unsupported incognito mode preference: "
      << static_cast<int>(incognito_pref);
  return incognito_pref != policy::IncognitoModeAvailability::kDisabled;
}

static bool JNI_IncognitoUtils_GetIncognitoModeForced(JNIEnv* env,
                                                      Profile* profile) {
  policy::IncognitoModeAvailability incognito_pref =
      IncognitoModePrefs::GetAvailability(profile);
  DCHECK(incognito_pref == policy::IncognitoModeAvailability::kEnabled ||
         incognito_pref == policy::IncognitoModeAvailability::kDisabled ||
         incognito_pref == policy::IncognitoModeAvailability::kForced)
      << "Unsupported incognito mode preference: "
      << static_cast<int>(incognito_pref);
  return incognito_pref == policy::IncognitoModeAvailability::kForced;
}

static bool JNI_IncognitoUtils_GetIncognitoModeManaged(JNIEnv* env,
                                                       Profile* profile) {
  PrefService* prefs = profile->GetPrefs();
  return prefs->IsManagedPreference(
             policy::policy_prefs::kIncognitoModeAvailability) ||
         prefs->IsManagedPreference(
             policy::policy_prefs::kIncognitoModeUrlAllowlist) ||
         prefs->IsManagedPreference(
             policy::policy_prefs::kIncognitoModeUrlBlocklist);
}

DEFINE_JNI(IncognitoUtils)
