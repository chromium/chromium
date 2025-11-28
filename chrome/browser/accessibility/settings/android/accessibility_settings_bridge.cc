// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/settings/android/accessibility_settings_bridge.h"

#include "chrome/android/chrome_jni_headers/AccessibilitySettingsBridge_jni.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

using base::android::JavaParamRef;

// static
static jboolean JNI_AccessibilitySettingsBridge_IsCaretBrowsingEnabled(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_profile) {
  Profile* profile = Profile::FromJavaObject(j_profile);
  return profile->GetPrefs()->GetBoolean(prefs::kCaretBrowsingEnabled);
}

// static
static void JNI_AccessibilitySettingsBridge_SetCaretBrowsingEnabled(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_profile,
    jboolean enabled) {
  Profile* profile = Profile::FromJavaObject(j_profile);
  profile->GetPrefs()->SetBoolean(prefs::kCaretBrowsingEnabled, enabled);
}

// static
static void
JNI_AccessibilitySettingsBridge_SetShowCaretBrowsingDialogPreference(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_profile,
    jboolean enabled) {
  Profile* profile = Profile::FromJavaObject(j_profile);
  profile->GetPrefs()->SetBoolean(prefs::kShowCaretBrowsingDialog, enabled);
}

// static
static jboolean
JNI_AccessibilitySettingsBridge_IsShowCaretBrowsingDialogPreference(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_profile) {
  Profile* profile = Profile::FromJavaObject(j_profile);
  return profile->GetPrefs()->GetBoolean(prefs::kShowCaretBrowsingDialog);
}

DEFINE_JNI(AccessibilitySettingsBridge)
