// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "chrome/android/chrome_jni_headers/UnifiedConsentServiceBridge_jni.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/unified_consent/unified_consent_service_factory.h"
#include "components/prefs/pref_service.h"
#include "components/sync/driver/sync_service.h"
#include "components/unified_consent/pref_names.h"
#include "components/unified_consent/unified_consent_service.h"

using base::android::JavaParamRef;

static jboolean
JNI_UnifiedConsentServiceBridge_IsUrlKeyedAnonymizedDataCollectionEnabled(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& profileAndroid) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(profileAndroid);
  return profile->GetPrefs()->GetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled);
}

static jboolean
JNI_UnifiedConsentServiceBridge_IsUrlKeyedAnonymizedDataCollectionManaged(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& profileAndroid) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(profileAndroid);
  return profile->GetPrefs()->IsManagedPreference(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled);
}

static void
JNI_UnifiedConsentServiceBridge_SetUrlKeyedAnonymizedDataCollectionEnabled(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& profileAndroid,
    const jboolean enabled) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(profileAndroid);
  auto* unifiedConsentService =
      UnifiedConsentServiceFactory::GetForProfile(profile);
  unifiedConsentService->SetUrlKeyedAnonymizedDataCollectionEnabled(enabled);
}

static void JNI_UnifiedConsentServiceBridge_RecordSyncSetupDataTypesHistogram(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& profileAndroid) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(profileAndroid);
  auto* syncService = ProfileSyncServiceFactory::GetForProfile(profile);
  unified_consent::metrics::RecordSyncSetupDataTypesHistrogam(
      syncService->GetUserSettings(), profile->GetPrefs());
}
