// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_manager_util_bridge.h"

#include <jni.h>
#include "base/android/build_info.h"
#include "base/android/jni_android.h"
#include "chrome/browser/password_manager/android/jni_headers/PasswordManagerUtilBridge_jni.h"
#include "chrome/browser/password_manager/android/password_manager_android_util.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_store/split_stores_and_local_upm.h"
#include "components/prefs/android/pref_service_android.h"

using password_manager::IsGmsCoreUpdateRequired;
using password_manager::UsesSplitStoresAndUPMForLocal;
using password_manager_android_util::ShouldUseUpmWiring;

jboolean JNI_PasswordManagerUtilBridge_UsesSplitStoresAndUPMForLocal(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_pref_service) {
  PrefService* pref_service =
      PrefServiceAndroid::FromPrefServiceAndroid(j_pref_service);
  return UsesSplitStoresAndUPMForLocal(pref_service);
}

// Called via JNI when it's necessary to check that the user is either syncing
// and enrolled in UPM or not syncing and ready to use local UPM.
jboolean JNI_PasswordManagerUtilBridge_ShouldUseUpmWiring(
    JNIEnv* env,
    jboolean is_pwd_sync_enabled,
    const base::android::JavaParamRef<jobject>& j_pref_service) {
  PrefService* pref_service =
      PrefServiceAndroid::FromPrefServiceAndroid(j_pref_service);
  return ShouldUseUpmWiring(is_pwd_sync_enabled, pref_service);
}

jboolean JNI_PasswordManagerUtilBridge_IsGmsCoreUpdateRequired(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_pref_service,
    jboolean is_pwd_sync_enabled) {
  PrefService* pref_service =
      PrefServiceAndroid::FromPrefServiceAndroid(j_pref_service);
  return IsGmsCoreUpdateRequired(
      pref_service, is_pwd_sync_enabled,
      base::android::BuildInfo::GetInstance()->gms_version_code());
}

jboolean
JNI_PasswordManagerUtilBridge_IsUnifiedPasswordManagerSyncOnlyInGMSCoreEnabled(
    JNIEnv* env) {
  return password_manager::features::
      IsUnifiedPasswordManagerSyncOnlyInGMSCoreEnabled();
}

namespace password_manager_android_util {
bool IsInternalBackendPresent() {
  return Java_PasswordManagerUtilBridge_isInternalBackendPresent(
      base::android::AttachCurrentThread());
}
}  // namespace password_manager_android_util
