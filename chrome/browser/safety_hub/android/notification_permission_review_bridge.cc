// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safety_hub/android/notification_permission_review_bridge.h"

#include <jni.h>

#include "base/android/jni_string.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/safety_hub/notification_permission_review_service.h"
#include "components/content_settings/core/common/content_settings_pattern.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/safety_hub/android/jni_headers/NotificationPermissions_jni.h"

NotificationPermissions FromJavaNotificationPermissions(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& jobject) {
  ContentSettingsPattern primary_pattern = ContentSettingsPattern::FromString(
      Java_NotificationPermissions_getPrimaryPattern(env, jobject));

  ContentSettingsPattern secondary_pattern = ContentSettingsPattern::FromString(
      Java_NotificationPermissions_getSecondaryPattern(env, jobject));

  int notification_count =
      Java_NotificationPermissions_getNotificationCount(env, jobject);

  return NotificationPermissions(primary_pattern, secondary_pattern,
                                 notification_count);
}

base::android::ScopedJavaLocalRef<jobject> ToJavaNotificationPermissions(
    JNIEnv* env,
    const NotificationPermissions& obj) {
  return Java_NotificationPermissions_create(
      env, obj.primary_pattern.ToString(), obj.secondary_pattern.ToString(),
      obj.notification_count);
}
