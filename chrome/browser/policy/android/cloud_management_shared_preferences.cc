// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/android/cloud_management_shared_preferences.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/policy/android/util_jni/CloudManagementSharedPreferences_jni.h"

namespace policy {
namespace android {

void SaveDmTokenInSharedPreferences(const std::string& dm_token) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_CloudManagementSharedPreferences_saveDmToken(
      env, base::android::ConvertUTF8ToJavaString(env, dm_token));
}

void DeleteDmTokenFromSharedPreferences() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_CloudManagementSharedPreferences_deleteDmToken(env);
}

std::string ReadDmTokenFromSharedPreferences() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return base::android::ConvertJavaStringToUTF8(
      env, Java_CloudManagementSharedPreferences_readDmToken(env));
}

}  // namespace android
}  // namespace policy
