// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_update_requester_android.h"

#include "base/android/jni_array.h"
#include "components/permissions/android/android_permission_util.h"
#include "content/public/browser/web_contents.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/PermissionUpdateRequester_jni.h"

PermissionUpdateRequester::PermissionUpdateRequester(
    content::WebContents* web_contents,
    const std::vector<std::string>& required_android_permissions,
    const std::vector<std::string>& optional_android_permissions,
    base::OnceCallback<void(bool)> callback) {
  callback_ = std::move(callback);
  JNIEnv* env = base::android::AttachCurrentThread();
  java_object_.Reset(Java_PermissionUpdateRequester_create(
      env, reinterpret_cast<intptr_t>(this), web_contents->GetJavaWebContents(),
      base::android::ToJavaArrayOfStrings(env, required_android_permissions),
      base::android::ToJavaArrayOfStrings(env, optional_android_permissions)));
}

void PermissionUpdateRequester::RequestPermissions() {
  Java_PermissionUpdateRequester_requestPermissions(
      base::android::AttachCurrentThread(), java_object_);
}

PermissionUpdateRequester::~PermissionUpdateRequester() {
  Java_PermissionUpdateRequester_onNativeDestroyed(
      base::android::AttachCurrentThread(), java_object_);
  java_object_.Reset();
}

void PermissionUpdateRequester::OnPermissionResult(
    JNIEnv* env,
    jboolean all_permissions_granted) {
  std::move(callback_).Run(all_permissions_granted);
}
