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
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/prefs/android/pref_service_android.h"
#include "components/prefs/pref_service.h"
#include "components/sync/android/sync_service_android_bridge.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/password_manager/android/jni_headers/PasswordManagerUtilBridge_jni.h"

static jboolean JNI_PasswordManagerUtilBridge_IsPasswordManagerAvailable(
    JNIEnv* env,
    jboolean is_internal_backend_present) {
  return password_manager_android_util::IsPasswordManagerAvailable(
      is_internal_backend_present);
}

namespace password_manager_android_util {

bool PasswordManagerUtilBridge::IsInternalBackendPresent() {
  return Java_PasswordManagerUtilBridge_isInternalBackendPresent(
      base::android::AttachCurrentThread());
}

bool PasswordManagerUtilBridge::IsGooglePlayServicesUpdatable() {
  return Java_PasswordManagerUtilBridge_isGooglePlayServicesUpdatable(
      base::android::AttachCurrentThread());
}

}  // namespace password_manager_android_util

DEFINE_JNI(PasswordManagerUtilBridge)
