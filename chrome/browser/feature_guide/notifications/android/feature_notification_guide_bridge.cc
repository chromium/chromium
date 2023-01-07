// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feature_guide/notifications/android/feature_notification_guide_bridge.h"

#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/android/chrome_jni_headers/FeatureNotificationGuideBridge_jni.h"
#include "chrome/browser/feature_guide/notifications/feature_notification_guide_service.h"
#include "chrome/browser/feature_guide/notifications/feature_type.h"

namespace feature_guide {

ScopedJavaLocalRef<jobject> FeatureNotificationGuideBridge::GetJavaObj() {
  return ScopedJavaLocalRef<jobject>(java_obj_);
}

FeatureNotificationGuideBridge::FeatureNotificationGuideBridge() {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_obj_.Reset(env, Java_FeatureNotificationGuideBridge_create(
                           env, reinterpret_cast<int64_t>(this))
                           .obj());
}

FeatureNotificationGuideBridge::~FeatureNotificationGuideBridge() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_FeatureNotificationGuideBridge_clearNativePtr(env, java_obj_);
}

std::u16string FeatureNotificationGuideBridge::GetNotificationTitle(
    FeatureType feature) {
  JNIEnv* env = base::android::AttachCurrentThread();
  std::u16string title;
  base::android::ConvertJavaStringToUTF16(
      env,
      Java_FeatureNotificationGuideBridge_getNotificationTitle(
          env, java_obj_, static_cast<int>(feature))
          .obj(),
      &title);
  return title;
}

std::u16string FeatureNotificationGuideBridge::GetNotificationMessage(
    FeatureType feature) {
  JNIEnv* env = base::android::AttachCurrentThread();
  std::u16string message;
  base::android::ConvertJavaStringToUTF16(
      env,
      Java_FeatureNotificationGuideBridge_getNotificationMessage(
          env, java_obj_, static_cast<int>(feature))
          .obj(),
      &message);
  return message;
}

void FeatureNotificationGuideBridge::OnNotificationClick(FeatureType feature) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_FeatureNotificationGuideBridge_onNotificationClick(
      env, java_obj_, static_cast<int>(feature));
}

void FeatureNotificationGuideBridge::CloseNotification(
    const std::string& notification_guid) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_FeatureNotificationGuideBridge_closeNotification(
      env, java_obj_,
      base::android::ConvertUTF8ToJavaString(env, notification_guid));
}

bool FeatureNotificationGuideBridge::ShouldSkipFeature(FeatureType feature) {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_FeatureNotificationGuideBridge_shouldSkipFeature(
      env, java_obj_, static_cast<int>(feature));
}

std::string FeatureNotificationGuideBridge::GetNotificationParamGuidForFeature(
    FeatureType feature) {
  JNIEnv* env = base::android::AttachCurrentThread();
  auto j_guid =
      Java_FeatureNotificationGuideBridge_getNotificationParamGuidForFeature(
          env, java_obj_, static_cast<int>(feature));
  return base::android::ConvertJavaStringToUTF8(env, j_guid);
}

}  // namespace feature_guide
