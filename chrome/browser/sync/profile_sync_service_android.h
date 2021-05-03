// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_PROFILE_SYNC_SERVICE_ANDROID_H_
#define CHROME_BROWSER_SYNC_PROFILE_SYNC_SERVICE_ANDROID_H_

#include <memory>

#include "base/android/jni_weak_ref.h"
#include "components/sync/driver/sync_service_observer.h"
#include "components/sync/engine/net/http_post_provider_factory.h"

class Profile;

namespace syncer {
class ProfileSyncService;
class SyncSetupInProgressHandle;
}

// Android wrapper of the ProfileSyncService which provides access from the Java
// layer. Note that on Android, there's only a single profile, and therefore
// a single instance of this wrapper. The name of the Java class is
// ProfileSyncService.
// This class should only be accessed from the UI thread.
class ProfileSyncServiceAndroid : public syncer::SyncServiceObserver {
 public:
  ProfileSyncServiceAndroid(JNIEnv* env, jobject java_profile_sync_service);
  ~ProfileSyncServiceAndroid() override;

  ProfileSyncServiceAndroid(const ProfileSyncServiceAndroid&) = delete;
  ProfileSyncServiceAndroid& operator=(const ProfileSyncServiceAndroid&) =
      delete;

  // This method should be called once right after contructing the object.
  // Returns false if we didn't get a ProfileSyncService.
  bool Init();

  // syncer::SyncServiceObserver:
  void OnStateChanged(syncer::SyncService* sync) override;

  // Pure ProfileSyncService calls.
  jboolean IsSyncRequested(JNIEnv* env);
  void SetSyncRequested(JNIEnv* env,
                        jboolean requested);
  jboolean CanSyncFeatureStart(JNIEnv* env);
  jboolean IsSyncAllowedByPlatform(JNIEnv* env);
  void SetSyncAllowedByPlatform(JNIEnv* env,
                                jboolean allowed);
  jboolean IsSyncFeatureActive(JNIEnv* env);
  jboolean IsSyncDisabledByEnterprisePolicy(JNIEnv* env);
  jboolean IsEngineInitialized(JNIEnv* env);
  jboolean IsTransportStateActive(JNIEnv* env);
  void SetSetupInProgress(JNIEnv* env,
                          jboolean in_progress);
  jboolean IsFirstSetupComplete(JNIEnv* env);
  void SetFirstSetupComplete(JNIEnv* env,
                             jint source);
  base::android::ScopedJavaLocalRef<jintArray> GetActiveDataTypes(JNIEnv* env);
  base::android::ScopedJavaLocalRef<jintArray> GetChosenDataTypes(JNIEnv* env);
  base::android::ScopedJavaLocalRef<jintArray> GetPreferredDataTypes(
      JNIEnv* env);
  void SetChosenDataTypes(
      JNIEnv* env,
      jboolean sync_everything,
      const base::android::JavaParamRef<jintArray>& model_type_selection);
  jboolean IsCustomPassphraseAllowed(JNIEnv* env);
  jboolean IsEncryptEverythingEnabled(JNIEnv* env);
  jboolean IsPassphraseRequiredForPreferredDataTypes(JNIEnv* env);
  jboolean IsTrustedVaultKeyRequired(JNIEnv* env);
  jboolean IsTrustedVaultKeyRequiredForPreferredDataTypes(JNIEnv* env);
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
  jboolean IsUrlKeyedDataCollectionEnabled(
      JNIEnv* env,
      jboolean personalized);
  jboolean RequiresClientUpgrade(JNIEnv* env);
  void SetDecoupledFromAndroidMasterSync(JNIEnv* env);
  jboolean GetDecoupledFromAndroidMasterSync(JNIEnv* env);
  base::android::ScopedJavaLocalRef<jobject> GetAuthenticatedAccountInfo(
      JNIEnv* env);
  jboolean IsAuthenticatedAccountPrimary(JNIEnv* env);

  // Pure SyncPrefs calls.
  jboolean IsPassphrasePromptMutedForCurrentProductVersion(JNIEnv* env);
  void MarkPassphrasePromptMutedForCurrentProductVersion(JNIEnv* env);
  jboolean HasKeepEverythingSynced(JNIEnv* env);

  void RecordKeyRetrievalTrigger(
      JNIEnv* env,
      jint trigger);

  // Functionality only available for testing purposes.

  jlong GetProfileSyncServiceForTest(JNIEnv* env);

  // Returns a timestamp for when a sync was last executed. The return value is
  // the internal value of base::Time.
  jlong GetLastSyncedTimeForTest(JNIEnv* env);

  void OverrideNetworkForTest(const syncer::CreateHttpPostProviderFactory&
                                  create_http_post_provider_factory_cb);

  void TriggerRefresh(JNIEnv* env);

 private:
  // A reference to the Chrome profile object.
  Profile* profile_;

  // A reference to the sync service for this profile.
  syncer::ProfileSyncService* sync_service_;

  // Prevents Sync from running until configuration is complete.
  std::unique_ptr<syncer::SyncSetupInProgressHandle> sync_blocker_;

  // Java-side ProfileSyncService object.
  JavaObjectWeakGlobalRef weak_java_profile_sync_service_;
};

#endif  // CHROME_BROWSER_SYNC_PROFILE_SYNC_SERVICE_ANDROID_H_
