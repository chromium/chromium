// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_PROFILE_SYNC_SERVICE_ANDROID_H_
#define CHROME_BROWSER_SYNC_PROFILE_SYNC_SERVICE_ANDROID_H_

#include <map>
#include <memory>

#include "base/android/jni_weak_ref.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "components/invalidation/public/invalidation_util.h"
#include "components/sync/base/sync_prefs.h"
#include "components/sync/driver/sync_service_observer.h"
#include "google/cacheinvalidation/include/types.h"
#include "google_apis/gaia/google_service_auth_error.h"

class Profile;

namespace browser_sync {
class ProfileSyncService;
}

namespace syncer {
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
  void RequestStart(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& obj);
  void RequestStop(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& obj);
  void SetSyncAllowedByPlatform(JNIEnv* env,
                                const base::android::JavaParamRef<jobject>& obj,
                                jboolean allowed);
  jboolean IsSyncActive(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& obj);
  jboolean IsEngineInitialized(JNIEnv* env,
                               const base::android::JavaParamRef<jobject>& obj);
  void SetSetupInProgress(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& obj,
                          jboolean in_progress);
  jboolean IsFirstSetupComplete(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  void SetFirstSetupComplete(JNIEnv* env,
                             const base::android::JavaParamRef<jobject>& obj);
  base::android::ScopedJavaLocalRef<jintArray> GetActiveDataTypes(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  base::android::ScopedJavaLocalRef<jintArray> GetPreferredDataTypes(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  void SetPreferredDataTypes(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jboolean sync_everything,
      const base::android::JavaParamRef<jintArray>& model_type_selection);
  jboolean IsCryptographerReady(JNIEnv* env,
                                const base::android::JavaParamRef<jobject>&);
  jboolean IsEncryptEverythingAllowed(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  jboolean IsEncryptEverythingEnabled(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  void EnableEncryptEverything(JNIEnv* env,
                               const base::android::JavaParamRef<jobject>& obj);
  jboolean IsPassphraseRequiredForDecryption(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  jboolean IsUsingSecondaryPassphrase(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  base::android::ScopedJavaLocalRef<jbyteArray> GetCustomPassphraseKey(
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
  void FlushDirectory(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& obj);
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

  // Gets SyncProtocolError.ClientAction.
  jint GetProtocolErrorClientAction(
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

  // Functionality only available for testing purposes.

  // Returns a timestamp for when a sync was last executed. The return value is
  // the internal value of base::Time.
  jlong GetLastSyncedTimeForTest(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  // Overrides ProfileSyncService's NetworkResources object. This is used to
  // set up the Sync FakeServer for testing.
  void OverrideNetworkResourcesForTest(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jlong network_resources);

  static ProfileSyncServiceAndroid* GetProfileSyncServiceAndroid();

 private:
  // Returns whether sync is allowed by Android.
  bool IsSyncAllowedByAndroid() const;

  // A reference to the Chrome profile object.
  Profile* profile_;

  // A reference to the sync service for this profile.
  browser_sync::ProfileSyncService* sync_service_;

  // Prevents Sync from running until configuration is complete.
  std::unique_ptr<syncer::SyncSetupInProgressHandle> sync_blocker_;

  // The class that handles getting, setting, and persisting sync
  // preferences.
  std::unique_ptr<syncer::SyncPrefs> sync_prefs_;

  // Java-side ProfileSyncService object.
  JavaObjectWeakGlobalRef weak_java_profile_sync_service_;

  DISALLOW_COPY_AND_ASSIGN(ProfileSyncServiceAndroid);
};

#endif  // CHROME_BROWSER_SYNC_PROFILE_SYNC_SERVICE_ANDROID_H_
