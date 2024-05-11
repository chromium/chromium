// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safety_hub/android/unused_site_permissions_bridge.h"

#include <jni.h>

#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/check.h"
#include "base/time/time.h"
#include "chrome/browser/safety_hub/android/jni_headers/PermissionsData_jni.h"
#include "chrome/browser/safety_hub/android/jni_headers/UnusedSitePermissionsBridge_jni.h"
#include "chrome/browser/ui/safety_hub/unused_site_permissions_service.h"
#include "chrome/browser/ui/safety_hub/unused_site_permissions_service_factory.h"
#include "components/content_settings/core/common/content_settings_constraints.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"

namespace jni_zero {

template <>
PermissionsData FromJniType<PermissionsData>(JNIEnv* env,
                                             const JavaRef<jobject>& jobject) {
  return FromJavaPermissionsData(env, jobject);
}

template <>
ScopedJavaLocalRef<jobject> ToJniType(JNIEnv* env, const PermissionsData& obj) {
  return ToJavaPermissionsData(env, obj);
}

}  // namespace jni_zero

PermissionsData FromJavaPermissionsData(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& jobject) {
  PermissionsData permissions_data;

  permissions_data.origin = ContentSettingsPattern::FromString(
      Java_PermissionsData_getOrigin(env, jobject));

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
  return Java_PermissionsData_create(
      env, obj.origin.ToString(), permissions,
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

static std::vector<PermissionsData>
JNI_UnusedSitePermissionsBridge_GetRevokedPermissions(JNIEnv* env,
                                                      Profile* profile) {
  return GetRevokedPermissions(profile);
}
