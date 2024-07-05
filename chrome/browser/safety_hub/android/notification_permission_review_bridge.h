// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFETY_HUB_ANDROID_NOTIFICATION_PERMISSION_REVIEW_BRIDGE_H_
#define CHROME_BROWSER_SAFETY_HUB_ANDROID_NOTIFICATION_PERMISSION_REVIEW_BRIDGE_H_

#include <jni.h>

#include <vector>

#include "base/android/scoped_java_ref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/safety_hub/notification_permission_review_service.h"

class Profile;

// JNI helper methods to enable unit testing.
NotificationPermissions FromJavaNotificationPermissions(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& jobject);

base::android::ScopedJavaLocalRef<jobject> ToJavaNotificationPermissions(
    JNIEnv* env,
    const NotificationPermissions& obj);

std::vector<NotificationPermissions> GetNotificationPermissions(
    Profile* profile);

void IgnoreOriginForNotificationPermissionReview(Profile* profile,
                                                 const std::string& origin);

void UndoIgnoreOriginForNotificationPermissionReview(Profile* profile,
                                                     const std::string& origin);

void AllowNotificationPermissionForOrigin(Profile* profile,
                                          const std::string& origin);

void ResetNotificationPermissionForOrigin(Profile* profile,
                                          const std::string& origin);

namespace jni_zero {

template <>
inline NotificationPermissions FromJniType<NotificationPermissions>(
    JNIEnv* env,
    const JavaRef<jobject>& jobject) {
  return FromJavaNotificationPermissions(env, jobject);
}

template <>
inline ScopedJavaLocalRef<jobject> ToJniType(
    JNIEnv* env,
    const NotificationPermissions& obj) {
  return ToJavaNotificationPermissions(env, obj);
}

}  // namespace jni_zero

#endif  // CHROME_BROWSER_SAFETY_HUB_ANDROID_NOTIFICATION_PERMISSION_REVIEW_BRIDGE_H_
