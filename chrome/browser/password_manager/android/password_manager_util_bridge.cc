// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>
#include "chrome/browser/password_manager/android/jni_headers/PasswordManagerUtilBridge_jni.h"
#include "chrome/browser/password_manager/android/password_manager_android_util.h"
#include "components/prefs/android/pref_service_android.h"

using password_manager_android_util::CanUseUPMBackend;
using password_manager_android_util::UsesSplitStoresAndUPMForLocal;

jboolean JNI_PasswordManagerUtilBridge_UsesSplitStoresAndUPMForLocal(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_pref_service) {
  PrefService* pref_service =
      PrefServiceAndroid::FromPrefServiceAndroid(j_pref_service);
  return UsesSplitStoresAndUPMForLocal(pref_service);
}

// Called via JNI when it's necessary to check that the user is either syncing
// and enrolled in UPM or not syncing and ready to use local UPM.
jboolean JNI_PasswordManagerUtilBridge_CanUseUPMBackend(
    JNIEnv* env,
    jboolean is_pwd_sync_enabled,
    const base::android::JavaParamRef<jobject>& j_pref_service) {
  PrefService* pref_service =
      PrefServiceAndroid::FromPrefServiceAndroid(j_pref_service);
  return CanUseUPMBackend(is_pwd_sync_enabled, pref_service);
}
