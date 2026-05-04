// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safety_hub/android/unused_site_permissions_bridge.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/containers/flat_map.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/safety_hub/safety_hub_test_util.h"
#include "chrome/browser/ui/safety_hub/unused_site_permissions_manager.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/browser/permission_settings_registry.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_constraints.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/constants.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::android::AttachCurrentThread;

namespace {

constexpr char kUnusedTestSite[] = "https://example.com";
const base::Time kExpiration = base::Time::Now();
const base::TimeDelta kLifetime = base::Days(30);

}  // namespace

class UnusedSitePermissionsBridgeTest : public testing::Test {
 public:
  UnusedSitePermissionsBridgeTest() : env_(AttachCurrentThread()) {}

  void SetUp() override {
    safety_hub_test_util::CreateRevokedPermissionsService(profile());

    hcsm_ = HostContentSettingsMapFactory::GetForProfile(profile());
  }

  base::flat_map<ContentSettingsType, base::Value> GetUnusedPermissionMap() {
    base::Value geolocation_value =
        content_settings::PermissionSettingsRegistry::GetInstance()
            ->Get(ContentSettingsType::GEOLOCATION_WITH_OPTIONS)
            ->delegate()
            .ToValue(
                GeolocationSetting{.approximate = PermissionOption::kAllowed,
                                   .precise = PermissionOption::kDenied});
    base::flat_map<ContentSettingsType, base::Value> unused_permission_map;
    unused_permission_map.insert(
        std::make_pair(ContentSettingsType::GEOLOCATION_WITH_OPTIONS,
                       std::move(geolocation_value)));
    unused_permission_map.insert(
        std::make_pair(ContentSettingsType::MEDIASTREAM_CAMERA,
                       base::Value(CONTENT_SETTING_ALLOW)));
    unused_permission_map.insert(
        std::make_pair(ContentSettingsType::MEDIASTREAM_MIC,
                       base::Value(CONTENT_SETTING_ALLOW)));
    return unused_permission_map;
  }

  void AddRevokedPermissions() {
    base::ListValue permission_list;
    for (auto&& [content_setting_type, setting_value] :
         GetUnusedPermissionMap()) {
      std::string content_setting_key =
          UnusedSitePermissionsManager::ConvertContentSettingsTypeToKey(
              content_setting_type);
      if (setting_value == base::Value(CONTENT_SETTING_ALLOW)) {
        permission_list.Append(content_setting_key);
      } else {
        base::DictValue item;
        item.Set(permissions::kRevokedPermissionType, content_setting_key);
        item.Set(permissions::kRevokedPermissionSettingValue,
                 std::move(setting_value));
        permission_list.Append(std::move(item));
      }
    }
    auto dict = base::DictValue().Set(permissions::kRevokedKey,
                                      std::move(permission_list));

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
  expected.permissions = GetUnusedPermissionMap();
  expected.constraints =
      content_settings::ContentSettingConstraints(kExpiration - kLifetime);
  expected.constraints.set_lifetime(kLifetime);
  expected.revocation_type = PermissionsRevocationType::kUnusedPermissions;

  const auto jobject = ToJavaPermissionsData(env(), expected);
  PermissionsData converted = FromJavaPermissionsData(env(), jobject);

  EXPECT_EQ(expected.primary_pattern, converted.primary_pattern);
  EXPECT_EQ(expected.permissions, converted.permissions);
  EXPECT_EQ(kExpiration, converted.constraints.expiration());
  EXPECT_EQ(kLifetime, converted.constraints.lifetime());
  EXPECT_EQ(PermissionsRevocationType::kUnusedPermissions,
            converted.revocation_type);
}

TEST_F(UnusedSitePermissionsBridgeTest, TestDefaultValuesRoundTrip) {
  PermissionsData expected;
  // The pattern has to be a valid single origin pattern.
  expected.primary_pattern =
      ContentSettingsPattern::FromString(kUnusedTestSite);

  const auto jobject = ToJavaPermissionsData(env(), expected);
  PermissionsData converted = FromJavaPermissionsData(env(), jobject);

  EXPECT_EQ(expected.primary_pattern, converted.primary_pattern);
  EXPECT_EQ(expected.permissions, converted.permissions);
  EXPECT_EQ(expected.constraints.expiration(),
            converted.constraints.expiration());
  EXPECT_EQ(expected.constraints.lifetime(), converted.constraints.lifetime());
  EXPECT_EQ(expected.revocation_type, converted.revocation_type);
}

TEST_F(UnusedSitePermissionsBridgeTest, TestGetRevokedPermissions) {
  // Populate revoked permissions.
  AddRevokedPermissions();
  std::vector<PermissionsData> revoked_permissions_list =
      GetRevokedPermissions(profile());
  EXPECT_EQ(revoked_permissions_list.size(), 1UL);

  PermissionsData& permissions_data = revoked_permissions_list[0];
  EXPECT_EQ(permissions_data.permissions, GetUnusedPermissionMap());
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
