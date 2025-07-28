// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_manager_util_bridge.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/files/file_path.h"
#include "chrome/browser/password_manager/android/password_manager_android_util.h"
#include "chrome/browser/profiles/profile.h"
#include "components/password_manager/core/browser/export/login_db_deprecation_password_exporter.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/split_stores_and_local_upm.h"
#include "components/prefs/android/pref_service_android.h"
#include "components/prefs/pref_service.h"
#include "components/sync/android/sync_service_android_bridge.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/password_manager/android/jni_headers/PasswordManagerUtilBridge_jni.h"

using password_manager::IsGmsCoreUpdateRequired;

jboolean JNI_PasswordManagerUtilBridge_IsPasswordManagerAvailable(
    JNIEnv* env,
    PrefService* pref_service,
    jboolean is_internal_backend_present) {
  return password_manager_android_util::IsPasswordManagerAvailable(
      pref_service, is_internal_backend_present);
}

jboolean JNI_PasswordManagerUtilBridge_IsGmsCoreUpdateRequired(
    JNIEnv* env,
    syncer::SyncService* sync_service) {
  return IsGmsCoreUpdateRequired(sync_service);
}

base::android::ScopedJavaLocalRef<jstring>
JNI_PasswordManagerUtilBridge_GetAutoExportCsvFilePath(JNIEnv* env,
                                                       Profile* profile) {
  return base::android::ConvertUTF8ToJavaString(
      env, profile->GetPath()
               .Append(FILE_PATH_LITERAL(
                   password_manager::kExportedPasswordsFileName))
               .value());
}

namespace password_manager_android_util {

bool PasswordManagerUtilBridge::IsInternalBackendPresent() {
  return Java_PasswordManagerUtilBridge_isInternalBackendPresent(
      base::android::AttachCurrentThread());
}

bool PasswordManagerUtilBridge::IsPlayStoreAppPresent() {
  return Java_PasswordManagerUtilBridge_isPlayStoreAppPresent(
      base::android::AttachCurrentThread());
}

bool PasswordManagerUtilBridge::IsGooglePlayServicesUpdatable() {
  return Java_PasswordManagerUtilBridge_isGooglePlayServicesUpdatable(
      base::android::AttachCurrentThread());
}

}  // namespace password_manager_android_util
