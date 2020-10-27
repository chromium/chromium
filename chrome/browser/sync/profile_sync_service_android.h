// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_PROFILE_SYNC_SERVICE_ANDROID_H_
#define CHROME_BROWSER_SYNC_PROFILE_SYNC_SERVICE_ANDROID_H_

#include <memory>

#include "base/android/jni_weak_ref.h"
#include "base/macros.h"
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
  ProfileSyncServiceAndroid(JNIEnv* env, jobject obj);
  ~ProfileSyncServiceAndroid() override;

  // This method should be called once right after contructing the object.
  // Returns false if we didn't get a ProfileSyncService.
  bool Init();

  // syncer::SyncServiceObserver:
  void OnStateChanged(syncer::SyncService* sync) override;

  // Pure ProfileSyncService calls.
  jboolean IsSyncRequested(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>& obj);
  jboolean CanSyncFeatureStart(JNIEnv* env,
                               const base::android::JavaParamRef<jobject>& obj);
  void RequestStart(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& obj);
  void RequestStop(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& obj);
  void SetSyncAllowedByPlatform(JNIEnv* env,
                                const base::android::JavaParamRef<jobject>& obj,
                                jboolean allowed);
  jboolean IsSyncActive(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& obj);
  jboolean IsSyncDisabledByEnterprisePolicy(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  jboolean IsEngineInitialized(JNIEnv* env,
                               const base::android::JavaParamRef<jobject>& obj);
  jboolean IsTransportStateActive(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  void SetSetupInProgress(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& obj,
                          jboolean in_progress);
  jboolean IsFirstSetupComplete(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  void SetFirstSetupComplete(JNIEnv* env,
                             const base::android::JavaParamRef<jobject>& obj,
                             jint source);
  base::android::ScopedJavaLocalRef<jintArray> GetActiveDataTypes(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  base::android::ScopedJavaLocalRef<jintArray> GetChosenDataTypes(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  base::android::ScopedJavaLocalRef<jintArray> GetPreferredDataTypes(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  void SetChosenDataTypes(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jboolean sync_everything,
      const base::android::JavaParamRef<jintArray>& model_type_selection);
  jboolean IsEncryptEverythingAllowed(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  jboolean IsEncryptEverythingEnabled(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  void EnableEncryptEverything(JNIEnv* env,
                               const base::android::JavaParamRef<jobject>& obj);
  jboolean IsPassphraseRequiredForPreferredDataTypes(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  jboolean IsTrustedVaultKeyRequired(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  jboolean IsTrustedVaultKeyRequiredForPreferredDataTypes(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  jboolean IsUsingSecondaryPassphrase(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  jint GetPassphraseType(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& obj);
  void SetEncryptionPassphrase(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& passphrase);
  jboolean SetDecryptionPassphrase(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& passphrase);
  jboolean HasExplicitPassphraseTime(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>&);
  jlong GetExplicitPassphraseTime(JNIEnv* env,
                                  const base::android::JavaParamRef<jobject>&);
  void GetAllNodes(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& obj,
                   const base::android::JavaParamRef<jobject>& callback);
  jint GetAuthError(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& obj);
  jboolean HasUnrecoverableError(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  jboolean IsUrlKeyedDataCollectionEnabled(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jboolean personalized);
  jboolean RequiresClientUpgrade(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  void SetDecoupledFromAndroidMasterSync(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  jboolean GetDecoupledFromAndroidMasterSync(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  jboolean IsAuthenticatedAccountPrimary(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  // Pure SyncPrefs calls.
  jboolean IsPassphrasePrompted(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  void SetPassphrasePrompted(JNIEnv* env,
                             const base::android::JavaParamRef<jobject>& obj,
                             jboolean prompted);
  void SetSyncSessionsId(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& obj,
                         const base::android::JavaParamRef<jstring>& tag);
  jboolean HasKeepEverythingSynced(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  jint GetNumberOfSyncedDevices(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  // UI string getters.

  base::android::ScopedJavaLocalRef<jstring>
  GetSyncEnterGooglePassphraseBodyWithDateText(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>&);

  base::android::ScopedJavaLocalRef<jstring>
  GetSyncEnterCustomPassphraseBodyWithDateText(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>&);

  base::android::ScopedJavaLocalRef<jstring> GetCurrentSignedInAccountText(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>&);

  base::android::ScopedJavaLocalRef<jstring>
  GetSyncEnterCustomPassphraseBodyText(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>&);

  void RecordKeyRetrievalTrigger(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jint trigger);

  // Functionality only available for testing purposes.

  jlong GetProfileSyncServiceForTest(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  // Returns a timestamp for when a sync was last executed. The return value is
  // the internal value of base::Time.
  jlong GetLastSyncedTimeForTest(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  void OverrideNetworkForTest(const syncer::CreateHttpPostProviderFactory&
                                  create_http_post_provider_factory_cb);

  void TriggerRefresh(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& obj);

 private:
  // A reference to the Chrome profile object.
  Profile* profile_;

  // A reference to the sync service for this profile.
  syncer::ProfileSyncService* sync_service_;

  // Prevents Sync from running until configuration is complete.
  std::unique_ptr<syncer::SyncSetupInProgressHandle> sync_blocker_;

  // Java-side ProfileSyncService object.
  JavaObjectWeakGlobalRef weak_java_profile_sync_service_;

  DISALLOW_COPY_AND_ASSIGN(ProfileSyncServiceAndroid);
};

#endif  // CHROME_BROWSER_SYNC_PROFILE_SYNC_SERVICE_ANDROID_H_
