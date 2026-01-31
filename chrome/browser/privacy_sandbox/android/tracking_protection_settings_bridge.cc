// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/scoped_java_ref.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/tracking_protection_settings.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/privacy_sandbox/android/jni_headers/TrackingProtectionSettingsBridge_jni.h"

using base::android::JavaRef;

namespace {

PrefService* GetPrefService(const base::android::JavaRef<jobject>& j_profile) {
  return Profile::FromJavaObject(j_profile)->GetPrefs();
}
}  // namespace

static void JNI_TrackingProtectionSettingsBridge_MaybeSetRollbackPrefsModeB(
    JNIEnv* env,
    const JavaRef<jobject>& j_profile) {
  privacy_sandbox::MaybeSetRollbackPrefsModeB(GetPrefService(j_profile));
}

DEFINE_JNI(TrackingProtectionSettingsBridge)
