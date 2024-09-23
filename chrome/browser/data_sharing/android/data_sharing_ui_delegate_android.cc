// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/data_sharing/android/data_sharing_ui_delegate_android.h"

#include "base/android/jni_string.h"
#include "chrome/browser/profiles/profile.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/data_sharing/jni_headers/DataSharingUIDelegateAndroid_jni.h"

namespace data_sharing {

DataSharingUIDelegateAndroid::DataSharingUIDelegateAndroid(Profile* profile) {
  JNIEnv* env = base::android::AttachCurrentThread();
  auto j_profile = profile->GetJavaObject();
  java_obj_.Reset(
      env, Java_DataSharingUIDelegateAndroid_create(env, j_profile).obj());
}

DataSharingUIDelegateAndroid::~DataSharingUIDelegateAndroid() = default;

void DataSharingUIDelegateAndroid::HandleShareURLIntercepted(const GURL& url) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_DataSharingUIDelegateAndroid_handleShareURLIntercepted(
      env, java_obj_, url::GURLAndroid::FromNativeGURL(env, url));
}

ScopedJavaLocalRef<jobject> DataSharingUIDelegateAndroid::GetJavaObject() {
  return ScopedJavaLocalRef<jobject>(java_obj_);
}

}  // namespace data_sharing
