// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/android/cloud_management_android_connection.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/policy/android/jni_headers/CloudManagementAndroidConnection_jni.h"

namespace policy {
namespace android {

std::string GetClientId() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return base::android::ConvertJavaStringToUTF8(
      env, Java_CloudManagementAndroidConnection_getClientId(
               env, Java_CloudManagementAndroidConnection_getInstance(env)));
}

}  // namespace android
}  // namespace policy
