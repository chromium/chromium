// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/scoped_java_ref.h"
#include "chrome/browser/privacy_sandbox/tracking_protection_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/tracking_protection_settings.h"
#include "components/sync/service/sync_service.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/privacy_sandbox/android/jni_headers/TrackingProtectionSettingsBridge_jni.h"

using base::android::JavaParamRef;

namespace {
privacy_sandbox::TrackingProtectionSettings* GetTrackingProtectionSettings(
    const base::android::JavaRef<jobject>& j_profile) {
  return TrackingProtectionSettingsFactory::GetForProfile(
      Profile::FromJavaObject(j_profile));
}

PrefService* GetPrefService(const base::android::JavaRef<jobject>& j_profile) {
  return Profile::FromJavaObject(j_profile)->GetPrefs();
}

syncer::SyncService* GetSyncService(
    const base::android::JavaRef<jobject>& j_profile) {
  return SyncServiceFactory::GetForProfile(Profile::FromJavaObject(j_profile));
}
}  // namespace

jboolean
JNI_TrackingProtectionSettingsBridge_IsIpProtectionDisabledForEnterprise(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_profile) {
  return GetTrackingProtectionSettings(j_profile)
      ->IsIpProtectionDisabledForEnterprise();
}

void JNI_TrackingProtectionSettingsBridge_MaybeSetRollbackPrefsModeB(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_profile) {
  privacy_sandbox::MaybeSetRollbackPrefsModeB(GetSyncService(j_profile),
                                              GetPrefService(j_profile));
}
