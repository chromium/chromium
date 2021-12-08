// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feature_guide/notifications/internal/android/feature_notification_guide_bridge.h"

#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/browser/feature_guide/notifications/feature_notification_guide_service.h"
#include "chrome/browser/feature_guide/notifications/feature_type.h"
#include "chrome/browser/feature_guide/notifications/internal/jni_headers/FeatureNotificationGuideBridge_jni.h"

namespace feature_guide {
namespace {
const char kFeatureNotificationGuideBridgeKey[] =
    "feature_notification_guide_bridge";
}  // namespace

// static
FeatureNotificationGuideBridge*
FeatureNotificationGuideBridge::GetFeatureNotificationGuideBridge(
    FeatureNotificationGuideService* feature_notification_guide_service) {
  if (!feature_notification_guide_service->GetUserData(
          kFeatureNotificationGuideBridgeKey)) {
    feature_notification_guide_service->SetUserData(
        kFeatureNotificationGuideBridgeKey,
        std::make_unique<FeatureNotificationGuideBridge>(
            feature_notification_guide_service));
  }

  return static_cast<FeatureNotificationGuideBridge*>(
      feature_notification_guide_service->GetUserData(
          kFeatureNotificationGuideBridgeKey));
}

ScopedJavaLocalRef<jobject> FeatureNotificationGuideBridge::GetJavaObj() {
  return ScopedJavaLocalRef<jobject>(java_obj_);
}

FeatureNotificationGuideBridge::FeatureNotificationGuideBridge(
    FeatureNotificationGuideService* feature_notification_guide_service)
    : feature_notification_guide_service_(feature_notification_guide_service) {
  DCHECK(feature_notification_guide_service);
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

}  // namespace feature_guide
