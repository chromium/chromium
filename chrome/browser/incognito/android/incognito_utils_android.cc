// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>

#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/incognito/jni_headers/IncognitoUtils_jni.h"

static jboolean JNI_IncognitoUtils_GetIncognitoModeEnabled(JNIEnv* env,
                                                           Profile* profile) {
  PrefService* prefs = profile->GetPrefs();
  policy::IncognitoModeAvailability incognito_pref =
      IncognitoModePrefs::GetAvailability(prefs);
  DCHECK(incognito_pref == policy::IncognitoModeAvailability::kEnabled ||
         incognito_pref == policy::IncognitoModeAvailability::kDisabled)
      << "Unsupported incognito mode preference: "
      << static_cast<int>(incognito_pref);
  return incognito_pref != policy::IncognitoModeAvailability::kDisabled;
}

static jboolean JNI_IncognitoUtils_GetIncognitoModeManaged(JNIEnv* env,
                                                           Profile* profile) {
  PrefService* prefs = profile->GetPrefs();
  return prefs->IsManagedPreference(
      policy::policy_prefs::kIncognitoModeAvailability);
}
