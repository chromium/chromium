// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safety_hub/android/unused_site_permissions_bridge.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/safety_hub/unused_site_permissions_service.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_constraints.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/constants.h"
#include "content/public/test/browser_task_environment.h"
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

  void SetUp() override {
    hcsm_ = HostContentSettingsMapFactory::GetForProfile(profile());
  }

  void AddRevokedPermissions() {
    base::Value::List revoked_permissions_list;
    for (ContentSettingsType type : kUnusedPermissionList) {
      revoked_permissions_list.Append(
          UnusedSitePermissionsService::ConvertContentSettingsTypeToKey(type));
    }
    auto dict = base::Value::Dict().Set(permissions::kRevokedKey,
                                        revoked_permissions_list.Clone());

    hcsm_->SetWebsiteSettingDefaultScope(
        GURL(kUnusedTestSite), GURL(kUnusedTestSite),
        ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS,
        base::Value(dict.Clone()));
  }

  TestingProfile* profile() { return &testing_profile_; }
  raw_ptr<JNIEnv> env() { return env_; }

 private:
  raw_ptr<JNIEnv> env_;
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile testing_profile_;
  scoped_refptr<HostContentSettingsMap> hcsm_;
};

TEST_F(UnusedSitePermissionsBridgeTest, TestJavaRoundTrip) {
  PermissionsData expected;
  expected.primary_pattern =
      ContentSettingsPattern::FromString(kUnusedTestSite);
  expected.permission_types = kUnusedPermissionList;
  expected.constraints =
      content_settings::ContentSettingConstraints(kExpiration - kLifetime);
  expected.constraints.set_lifetime(kLifetime);

  const auto jobject = ToJavaPermissionsData(env(), expected);
  PermissionsData converted = FromJavaPermissionsData(env(), jobject);

  EXPECT_EQ(expected.primary_pattern, converted.primary_pattern);
  EXPECT_EQ(expected.permission_types, converted.permission_types);
  EXPECT_EQ(kExpiration, converted.constraints.expiration());
  EXPECT_EQ(kLifetime, converted.constraints.lifetime());
}

TEST_F(UnusedSitePermissionsBridgeTest, TestDefaultValuesRoundTrip) {
  PermissionsData expected;
  // The pattern has to be a valid single origin pattern.
  expected.primary_pattern =
      ContentSettingsPattern::FromString(kUnusedTestSite);

  const auto jobject = ToJavaPermissionsData(env(), expected);
  PermissionsData converted = FromJavaPermissionsData(env(), jobject);

  EXPECT_EQ(expected.primary_pattern, converted.primary_pattern);
  EXPECT_EQ(expected.permission_types, converted.permission_types);
  EXPECT_EQ(expected.constraints.expiration(),
            converted.constraints.expiration());
  EXPECT_EQ(expected.constraints.lifetime(), converted.constraints.lifetime());
}

TEST_F(UnusedSitePermissionsBridgeTest, TestGetRevokedPermissions) {
  // Populate revoked permissions.
  AddRevokedPermissions();
  std::vector<PermissionsData> revoked_permissions_list =
      GetRevokedPermissions(profile());
  EXPECT_EQ(revoked_permissions_list.size(), 1UL);

  PermissionsData& permissions_data = revoked_permissions_list[0];
  EXPECT_EQ(permissions_data.permission_types, kUnusedPermissionList);
}

TEST_F(UnusedSitePermissionsBridgeTest, TestRegrantAndUndoRegrantPermissions) {
  // Populate revoked permissions.
  AddRevokedPermissions();
  std::vector<PermissionsData> revoked_permissions_list =
      GetRevokedPermissions(profile());
  EXPECT_EQ(revoked_permissions_list.size(), 1UL);
  PermissionsData revoked_permissions_data = revoked_permissions_list[0];

  // Regrant the revoked permissions.
  std::string primary_pattern(kUnusedTestSite);
  RegrantPermissions(profile(), primary_pattern);
  revoked_permissions_list = GetRevokedPermissions(profile());
  EXPECT_EQ(revoked_permissions_list.size(), 0UL);

  // Undo the previous regranting and revoke the permissions again.
  UndoRegrantPermissions(profile(), revoked_permissions_data);
  revoked_permissions_list = GetRevokedPermissions(profile());
  EXPECT_EQ(revoked_permissions_list.size(), 1UL);
}

TEST_F(UnusedSitePermissionsBridgeTest, TestAckAndUndoAckPermissionsList) {
  // Populate revoked permissions.
  AddRevokedPermissions();
  std::vector<PermissionsData> revoked_permissions_list =
      GetRevokedPermissions(profile());
  EXPECT_EQ(revoked_permissions_list.size(), 1UL);

  // Acknowledge the revoked permissions list
  ClearRevokedPermissionsReviewList(profile());
  std::vector<PermissionsData> empty_permissions_list =
      GetRevokedPermissions(profile());
  EXPECT_EQ(empty_permissions_list.size(), 0UL);

  // Undo the previous acknowledgement and restore the revoked permissions list.
  RestoreRevokedPermissionsReviewList(profile(), revoked_permissions_list);
  revoked_permissions_list = GetRevokedPermissions(profile());
  EXPECT_EQ(revoked_permissions_list.size(), 1UL);
}
