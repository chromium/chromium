// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>

#include <string>
#include <vector>

#include "base/android/jni_array.h"
#include "base/functional/bind.h"
#include "chrome/browser/android/webapk/webapk_sync_service.h"
#include "chrome/browser/android/webapk/webapk_sync_service_factory.h"
#include "chrome/browser/profiles/profile.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_test_util_jni/PwaRestoreBottomSheetTestUtils_jni.h"

using base::android::JavaParamRef;
using base::android::JavaRef;

namespace webapps {

void OnWebApkDatabaseInitialized(JNIEnv* env, bool initialized) {
  Java_PwaRestoreBottomSheetTestUtils_onWebApkDatabaseInitialized(env,
                                                                  initialized);
}

void JNI_PwaRestoreBottomSheetTestUtils_WaitForWebApkDatabaseInitialization(
    JNIEnv* env,
    Profile* profile) {
  DCHECK(profile);
  if (!profile) {
    OnWebApkDatabaseInitialized(env, /* initialized= */ false);
    return;
  }

  webapk::WebApkSyncService* service =
      webapk::WebApkSyncServiceFactory::GetForProfile(profile);
  service->RegisterDoneInitializingCallback(
      base::BindOnce(&OnWebApkDatabaseInitialized, env));
}

void JNI_PwaRestoreBottomSheetTestUtils_SetAppListForRestoring(
    JNIEnv* env,
    const JavaParamRef<jobjectArray>& apps,
    const JavaParamRef<jintArray>& last_used_in_days,
    Profile* profile) {
  DCHECK(profile);
  if (!profile) {
    return;
  }

  std::vector<std::vector<std::string>> app_vector;
  base::android::Java2dStringArrayTo2dStringVector(env, apps, &app_vector);

  std::vector<int> last_used_in_days_vector;
  base::android::JavaIntArrayToIntVector(env, last_used_in_days,
                                         &last_used_in_days_vector);

  webapk::WebApkSyncService* service =
      webapk::WebApkSyncServiceFactory::GetForProfile(profile);
  service->MergeSyncDataForTesting(app_vector, last_used_in_days_vector);
}

}  // namespace webapps
