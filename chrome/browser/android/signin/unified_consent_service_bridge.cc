// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/unified_consent/unified_consent_service_factory.h"
#include "components/prefs/pref_service.h"
#include "components/sync/service/sync_service.h"
#include "components/unified_consent/pref_names.h"
#include "components/unified_consent/unified_consent_metrics.h"
#include "components/unified_consent/unified_consent_service.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/signin/services/android/jni_headers/UnifiedConsentServiceBridge_jni.h"

using base::android::JavaParamRef;

static jboolean
JNI_UnifiedConsentServiceBridge_IsUrlKeyedAnonymizedDataCollectionEnabled(
    JNIEnv* env,
    Profile* profile) {
  return profile->GetPrefs()->GetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled);
}

static jboolean
JNI_UnifiedConsentServiceBridge_IsUrlKeyedAnonymizedDataCollectionManaged(
    JNIEnv* env,
    Profile* profile) {
  return profile->GetPrefs()->IsManagedPreference(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled);
}

static void
JNI_UnifiedConsentServiceBridge_SetUrlKeyedAnonymizedDataCollectionEnabled(
    JNIEnv* env,
    Profile* profile,
    const jboolean enabled) {
  auto* unifiedConsentService =
      UnifiedConsentServiceFactory::GetForProfile(profile);
  DCHECK(unifiedConsentService);
  unifiedConsentService->SetUrlKeyedAnonymizedDataCollectionEnabled(enabled);
}

static void JNI_UnifiedConsentServiceBridge_RecordSyncSetupDataTypesHistogram(
    JNIEnv* env,
    Profile* profile) {
  auto* syncService = SyncServiceFactory::GetForProfile(profile);
  unified_consent::metrics::RecordSyncSetupDataTypesHistrogam(
      syncService->GetUserSettings());
}
