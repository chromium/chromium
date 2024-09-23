// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safety_hub/android/notification_permission_review_bridge.h"

#include <jni.h>

#include <vector>

#include "base/android/jni_string.h"
#include "base/check.h"
#include "chrome/browser/ui/safety_hub/notification_permission_review_service.h"
#include "chrome/browser/ui/safety_hub/notification_permission_review_service_factory.h"
#include "components/content_settings/core/common/content_settings_pattern.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/safety_hub/android/jni_headers/NotificationPermissionReviewBridge_jni.h"
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

std::vector<NotificationPermissions> GetNotificationPermissions(
    Profile* profile) {
  NotificationPermissionsReviewService* service =
      NotificationPermissionsReviewServiceFactory::GetForProfile(profile);
  CHECK(service);
  std::unique_ptr<SafetyHubService::Result> result =
      service->GetNotificationPermissions();

  return (static_cast<NotificationPermissionsReviewService::
                          NotificationPermissionsResult*>(result.get()))
      ->GetSortedNotificationPermissions();
}

void IgnoreOriginForNotificationPermissionReview(Profile* profile,
                                                 const std::string& origin) {
  NotificationPermissionsReviewService* service =
      NotificationPermissionsReviewServiceFactory::GetForProfile(profile);
  CHECK(service);

  const ContentSettingsPattern primary_pattern =
      ContentSettingsPattern::FromString(origin);
  service->AddPatternToNotificationPermissionReviewBlocklist(
      primary_pattern, ContentSettingsPattern::Wildcard());
}

void UndoIgnoreOriginForNotificationPermissionReview(
    Profile* profile,
    const std::string& origin) {
  NotificationPermissionsReviewService* service =
      NotificationPermissionsReviewServiceFactory::GetForProfile(profile);
  CHECK(service);

  const ContentSettingsPattern& primary_pattern =
      ContentSettingsPattern::FromString(origin);
  service->RemovePatternFromNotificationPermissionReviewBlocklist(
      primary_pattern, ContentSettingsPattern::Wildcard());
}

void AllowNotificationPermissionForOrigin(Profile* profile,
                                          const std::string& origin) {
  NotificationPermissionsReviewService* service =
      NotificationPermissionsReviewServiceFactory::GetForProfile(profile);
  CHECK(service);

  service->SetNotificationPermissionsForOrigin(origin, CONTENT_SETTING_ALLOW);
}

void ResetNotificationPermissionForOrigin(Profile* profile,
                                          const std::string& origin) {
  NotificationPermissionsReviewService* service =
      NotificationPermissionsReviewServiceFactory::GetForProfile(profile);
  CHECK(service);

  service->SetNotificationPermissionsForOrigin(origin, CONTENT_SETTING_DEFAULT);
}

static std::vector<NotificationPermissions>
JNI_NotificationPermissionReviewBridge_GetNotificationPermissions(
    JNIEnv* env,
    Profile* profile) {
  return GetNotificationPermissions(profile);
}

static void
JNI_NotificationPermissionReviewBridge_IgnoreOriginForNotificationPermissionReview(
    JNIEnv* env,
    Profile* profile,
    std::string& origin) {
  IgnoreOriginForNotificationPermissionReview(profile, origin);
}

static void
JNI_NotificationPermissionReviewBridge_UndoIgnoreOriginForNotificationPermissionReview(
    JNIEnv* env,
    Profile* profile,
    std::string& origin) {
  UndoIgnoreOriginForNotificationPermissionReview(profile, origin);
}

static void
JNI_NotificationPermissionReviewBridge_AllowNotificationPermissionForOrigin(
    JNIEnv* env,
    Profile* profile,
    std::string& origin) {
  AllowNotificationPermissionForOrigin(profile, origin);
}

static void
JNI_NotificationPermissionReviewBridge_ResetNotificationPermissionForOrigin(
    JNIEnv* env,
    Profile* profile,
    std::string& origin) {
  ResetNotificationPermissionForOrigin(profile, origin);
}
