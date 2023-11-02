// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_ANDROID_SYNC_SERVICE_ANDROID_BRIDGE_H_
#define CHROME_BROWSER_SYNC_ANDROID_SYNC_SERVICE_ANDROID_BRIDGE_H_

#include <memory>

#include "base/android/jni_weak_ref.h"
#include "base/memory/raw_ptr.h"
#include "components/sync/driver/sync_service_observer.h"
#include "components/sync/engine/net/http_post_provider_factory.h"

namespace syncer {
class SyncService;
class SyncSetupInProgressHandle;
}  // namespace syncer

// Forwards calls from SyncServiceImpl.java to the native SyncService and
// back. Instead of directly implementing JNI free functions, this class is used
// so it can manage the lifetime of objects like SyncSetupInProgressHandle.
// Note that on Android, there's only a single profile, a single native
// SyncService, and therefore a single instance of this class.
// Must only be accessed from the UI thread.
class SyncServiceAndroidBridge : public syncer::SyncServiceObserver {
 public:
  // |native_sync_service| and |java_sync_service| must not be null.
  SyncServiceAndroidBridge(JNIEnv* env,
                           syncer::SyncService* native_sync_service,
                           jobject java_sync_service);
  ~SyncServiceAndroidBridge() override;

  SyncServiceAndroidBridge(const SyncServiceAndroidBridge&) = delete;
  SyncServiceAndroidBridge& operator=(const SyncServiceAndroidBridge&) = delete;

  // syncer::SyncServiceObserver:
  void OnStateChanged(syncer::SyncService* sync) override;

  // Please keep all methods below in the same order as the @NativeMethods in
  // SyncServiceImpl.java.
  jboolean IsSyncRequested(JNIEnv* env);
  void SetSyncRequested(JNIEnv* env, jboolean requested);
  jboolean CanSyncFeatureStart(JNIEnv* env);
  jboolean IsSyncFeatureEnabled(JNIEnv* env);
  jboolean IsSyncFeatureActive(JNIEnv* env);
  jboolean IsSyncDisabledByEnterprisePolicy(JNIEnv* env);
  jboolean IsEngineInitialized(JNIEnv* env);
  jboolean IsTransportStateActive(JNIEnv* env);
  void SetSetupInProgress(JNIEnv* env, jboolean in_progress);
  jboolean IsFirstSetupComplete(JNIEnv* env);
  void SetFirstSetupComplete(JNIEnv* env, jint source);
  base::android::ScopedJavaLocalRef<jintArray> GetActiveDataTypes(JNIEnv* env);
  base::android::ScopedJavaLocalRef<jintArray> GetSelectedTypes(JNIEnv* env);
  void SetSelectedTypes(JNIEnv* env,
                        jboolean sync_everything,
                        const base::android::JavaParamRef<jintArray>&
                            user_selectable_type_selection);
  jboolean IsCustomPassphraseAllowed(JNIEnv* env);
  jboolean IsEncryptEverythingEnabled(JNIEnv* env);
  jboolean IsPassphraseRequiredForPreferredDataTypes(JNIEnv* env);
  jboolean IsTrustedVaultKeyRequired(JNIEnv* env);
  jboolean IsTrustedVaultKeyRequiredForPreferredDataTypes(JNIEnv* env);
  jboolean IsTrustedVaultRecoverabilityDegraded(JNIEnv* env);
  jboolean IsUsingExplicitPassphrase(JNIEnv* env);
  jint GetPassphraseType(JNIEnv* env);
  void SetEncryptionPassphrase(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& passphrase);
  jboolean SetDecryptionPassphrase(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& passphrase);
  // Returns 0 if there's no passphrase time.
  jlong GetExplicitPassphraseTime(JNIEnv* env);
  void GetAllNodes(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& callback);
  jint GetAuthError(JNIEnv* env);
  jboolean HasUnrecoverableError(JNIEnv* env);
  jboolean RequiresClientUpgrade(JNIEnv* env);
  base::android::ScopedJavaLocalRef<jobject> GetAccountInfo(JNIEnv* env);
  jboolean HasSyncConsent(JNIEnv* env);
  jboolean IsPassphrasePromptMutedForCurrentProductVersion(JNIEnv* env);
  void MarkPassphrasePromptMutedForCurrentProductVersion(JNIEnv* env);
  jboolean HasKeepEverythingSynced(JNIEnv* env);
  jboolean ShouldOfferTrustedVaultOptIn(JNIEnv* env);
  void TriggerRefresh(JNIEnv* env);
  // Returns a timestamp for when a sync was last executed. The return value is
  // the internal value of base::Time.
  jlong GetLastSyncedTimeForDebugging(JNIEnv* env);

 private:
  // A reference to the sync service for this profile.
  const raw_ptr<syncer::SyncService> native_sync_service_;

  // Java-side SyncServiceImpl object.
  const JavaObjectWeakGlobalRef java_sync_service_;

  // Prevents Sync from running until configuration is complete.
  std::unique_ptr<syncer::SyncSetupInProgressHandle> sync_blocker_;
};

#endif  // CHROME_BROWSER_SYNC_ANDROID_SYNC_SERVICE_ANDROID_BRIDGE_H_
