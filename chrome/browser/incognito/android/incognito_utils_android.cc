// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>

#include "chrome/browser/incognito/jni_headers/IncognitoUtils_jni.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

static jboolean JNI_IncognitoUtils_GetIncognitoModeEnabled(JNIEnv* env) {
  PrefService* prefs =
      ProfileManager::GetActiveUserProfile()->GetOriginalProfile()->GetPrefs();
  IncognitoModePrefs::Availability incognito_pref =
      IncognitoModePrefs::GetAvailability(prefs);
  DCHECK(incognito_pref == IncognitoModePrefs::Availability::kEnabled ||
         incognito_pref == IncognitoModePrefs::Availability::kDisabled)
      << "Unsupported incognito mode preference: "
      << static_cast<int>(incognito_pref);
  return incognito_pref != IncognitoModePrefs::Availability::kDisabled;
}

static jboolean JNI_IncognitoUtils_GetIncognitoModeManaged(JNIEnv* env) {
  PrefService* prefs =
      ProfileManager::GetActiveUserProfile()->GetOriginalProfile()->GetPrefs();
  return prefs->IsManagedPreference(prefs::kIncognitoModeAvailability);
}
