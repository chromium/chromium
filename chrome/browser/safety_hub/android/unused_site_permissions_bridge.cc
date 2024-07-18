// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safety_hub/android/unused_site_permissions_bridge.h"

#include <jni.h>

#include <vector>

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/check.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/safety_hub/unused_site_permissions_service.h"
#include "chrome/browser/ui/safety_hub/unused_site_permissions_service_factory.h"
#include "components/content_settings/core/common/content_settings_constraints.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/page_info/page_info_ui.h"
#include "url/gurl.h"
#include "url/origin.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/safety_hub/android/jni_headers/PermissionsData_jni.h"
#include "chrome/browser/safety_hub/android/jni_headers/UnusedSitePermissionsBridge_jni.h"

PermissionsData FromJavaPermissionsData(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& jobject) {
  PermissionsData permissions_data;

  permissions_data.primary_pattern = ContentSettingsPattern::FromString(
      Java_PermissionsData_getOrigin(env, jobject));
  CHECK(permissions_data.primary_pattern.IsValid());

  for (const int32_t permission_type :
       Java_PermissionsData_getPermissions(env, jobject)) {
    permissions_data.permission_types.insert(
        static_cast<ContentSettingsType>(permission_type));
  }

  const base::Time expiration = base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(Java_PermissionsData_getExpiration(env, jobject)));
  const base::TimeDelta lifetime =
      base::Microseconds(Java_PermissionsData_getLifetime(env, jobject));
  permissions_data.constraints =
      content_settings::ContentSettingConstraints(expiration - lifetime);
  permissions_data.constraints.set_lifetime(lifetime);

  return permissions_data;
}

base::android::ScopedJavaLocalRef<jobject> ToJavaPermissionsData(
    JNIEnv* env,
    const PermissionsData& obj) {
  std::vector<int32_t> permissions;
  for (ContentSettingsType type : obj.permission_types) {
    permissions.push_back(static_cast<int32_t>(type));
  }

  // Converting a primary pattern to an origin is normally an anti-pattern
  // but here it is ok since the primary pattern belongs to a single
  // origin. Therefore, it has a fully defined URL+scheme+port which makes
  // converting primary pattern to origin successful.
  url::Origin origin =
      UnusedSitePermissionsService::ConvertPrimaryPatternToOrigin(
          obj.primary_pattern);
  return Java_PermissionsData_create(
      env, origin.Serialize(), permissions,
      obj.constraints.expiration().ToDeltaSinceWindowsEpoch().InMicroseconds(),
      obj.constraints.lifetime().InMicroseconds());
}

std::vector<PermissionsData> GetRevokedPermissions(Profile* profile) {
  UnusedSitePermissionsService* service =
      UnusedSitePermissionsServiceFactory::GetForProfile(profile);
  CHECK(service);
  const auto service_result =
      service->GetRevokedPermissions()->GetRevokedPermissions();
  return std::vector<PermissionsData>(service_result.begin(),
                                      service_result.end());
}

void RegrantPermissions(Profile* profile, std::string& origin_str) {
  UnusedSitePermissionsService* service =
      UnusedSitePermissionsServiceFactory::GetForProfile(profile);
  CHECK(service);

  url::Origin origin = url::Origin::Create(GURL(origin_str));
  service->RegrantPermissionsForOrigin(origin);
}

void UndoRegrantPermissions(Profile* profile,
                            PermissionsData& permissions_data) {
  UnusedSitePermissionsService* service =
      UnusedSitePermissionsServiceFactory::GetForProfile(profile);
  CHECK(service);

  service->UndoRegrantPermissionsForOrigin(permissions_data);
}

void ClearRevokedPermissionsReviewList(Profile* profile) {
  UnusedSitePermissionsService* service =
      UnusedSitePermissionsServiceFactory::GetForProfile(profile);
  CHECK(service);

  service->ClearRevokedPermissionsList();
}

void RestoreRevokedPermissionsReviewList(
    Profile* profile,
    std::vector<PermissionsData>& permissions_data_list) {
  UnusedSitePermissionsService* service =
      UnusedSitePermissionsServiceFactory::GetForProfile(profile);
  CHECK(service);

  for (const auto& permissions_data : permissions_data_list) {
    service->StorePermissionInRevokedPermissionSetting(permissions_data);
  }
}

std::vector<std::u16string> ContentSettingsTypeToString(
    std::vector<int32_t>& content_settings_type_list) {
  std::vector<std::u16string> content_settings_string_list;
  for (int32_t content_settings_type : content_settings_type_list) {
    content_settings_string_list.push_back(PageInfoUI::PermissionTypeToUIString(
        static_cast<ContentSettingsType>(content_settings_type)));
  }
  return content_settings_string_list;
}

static std::vector<PermissionsData>
JNI_UnusedSitePermissionsBridge_GetRevokedPermissions(JNIEnv* env,
                                                      Profile* profile) {
  return GetRevokedPermissions(profile);
}

static void JNI_UnusedSitePermissionsBridge_RegrantPermissions(
    JNIEnv* env,
    Profile* profile,
    std::string& origin_str) {
  RegrantPermissions(profile, origin_str);
}

static void JNI_UnusedSitePermissionsBridge_UndoRegrantPermissions(
    JNIEnv* env,
    Profile* profile,
    PermissionsData& permissions_data) {
  UndoRegrantPermissions(profile, permissions_data);
}

static void JNI_UnusedSitePermissionsBridge_ClearRevokedPermissionsReviewList(
    JNIEnv* env,
    Profile* profile) {
  ClearRevokedPermissionsReviewList(profile);
}

static void JNI_UnusedSitePermissionsBridge_RestoreRevokedPermissionsReviewList(
    JNIEnv* env,
    Profile* profile,
    std::vector<PermissionsData>& permissions_data_list) {
  RestoreRevokedPermissionsReviewList(profile, permissions_data_list);
}

static std::vector<std::u16string>
JNI_UnusedSitePermissionsBridge_ContentSettingsTypeToString(
    JNIEnv* env,
    std::vector<std::int32_t>& content_settings_type_list) {
  return ContentSettingsTypeToString(content_settings_type_list);
}
