// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safety_hub/android/unused_site_permissions_bridge.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/time/time.h"
#include "chrome/browser/ui/safety_hub/unused_site_permissions_service.h"
#include "components/content_settings/core/common/content_settings_constraints.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::android::AttachCurrentThread;

namespace {

constexpr char kUnusedTestSite[] = "https://example.com";
std::set<ContentSettingsType> kUnusedPermissionList = {
    ContentSettingsType::GEOLOCATION, ContentSettingsType::MEDIASTREAM_CAMERA,
    ContentSettingsType::MEDIASTREAM_MIC};
const base::Time kExpiration = base::Time::Now();
const base::TimeDelta kLifetime = base::Days(30);

}  // namespace

class UnusedSitePermissionsBridgeTest : public testing::Test {
 public:
  UnusedSitePermissionsBridgeTest() : env_(AttachCurrentThread()) {}

  raw_ptr<JNIEnv> env() { return env_; }

 private:
  raw_ptr<JNIEnv> env_;
};

TEST_F(UnusedSitePermissionsBridgeTest, TestJavaRoundTrip) {
  PermissionsData expected;
  expected.origin = ContentSettingsPattern::FromString(kUnusedTestSite);
  expected.permission_types = kUnusedPermissionList;
  expected.constraints =
      content_settings::ContentSettingConstraints(kExpiration - kLifetime);
  expected.constraints.set_lifetime(kLifetime);

  const auto jobject = ToJavaPermissionsData(env(), expected);
  PermissionsData converted = FromJavaPermissionsData(env(), jobject);

  EXPECT_EQ(expected.origin, converted.origin);
  EXPECT_EQ(expected.permission_types, converted.permission_types);
  EXPECT_EQ(kExpiration, converted.constraints.expiration());
  EXPECT_EQ(kLifetime, converted.constraints.lifetime());
}

TEST_F(UnusedSitePermissionsBridgeTest, TestDefaultValuesRoundTrip) {
  PermissionsData expected;

  const auto jobject = ToJavaPermissionsData(env(), expected);
  PermissionsData converted = FromJavaPermissionsData(env(), jobject);

  EXPECT_EQ(expected.origin, converted.origin);
  EXPECT_EQ(expected.permission_types, converted.permission_types);
  EXPECT_EQ(expected.constraints.expiration(),
            converted.constraints.expiration());
  EXPECT_EQ(expected.constraints.lifetime(), converted.constraints.lifetime());
}
