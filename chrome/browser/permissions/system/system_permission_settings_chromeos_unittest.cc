// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/system/system_permission_settings.h"

#include <type_traits>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/geolocation_access_level.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/privacy_hub/privacy_hub_util.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/content_settings/core/common/features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

class ChromeOsSystemSettingsTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<ContentSettingsType> {
 public:
  ChromeOsSystemSettingsTest() {
    scoped_feature_list_.InitWithFeatures(
        {ash::features::kCrosPrivacyHub,
         content_settings::features::kCrosSystemLevelPermissionBlockedWarnings},
        {});
  }

  ~ChromeOsSystemSettingsTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(ChromeOsSystemSettingsTest, RetrieveSystemSettings) {
  const ContentSettingsType permission_type = GetParam();
  {
    // Permission disabled
    ash::privacy_hub_util::ScopedUserPermissionPrefForTest permission_override(
        permission_type, ash::GeolocationAccessLevel::kDisallowed);
    EXPECT_FALSE(system_permission_settings::IsAllowed(permission_type));
    EXPECT_TRUE(system_permission_settings::IsDenied(permission_type));
  }
  {
    // Permission not disabled
    ash::privacy_hub_util::ScopedUserPermissionPrefForTest permission_override(
        permission_type, ash::GeolocationAccessLevel::kAllowed);
    EXPECT_TRUE(system_permission_settings::IsAllowed(permission_type));
    EXPECT_FALSE(system_permission_settings::IsDenied(permission_type));
  }
  if (permission_type == ContentSettingsType::GEOLOCATION) {
    // Geolocation: Permission disabled for browser and OS both
    ash::privacy_hub_util::ScopedUserPermissionPrefForTest permission_override(
        permission_type, ash::GeolocationAccessLevel::kOnlyAllowedForSystem);
    EXPECT_FALSE(system_permission_settings::IsAllowed(permission_type));
    EXPECT_TRUE(system_permission_settings::IsDenied(permission_type));
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ChromeOsSystemSettingsTest,
    testing::Values(ContentSettingsType::MEDIASTREAM_CAMERA,
                    ContentSettingsType::MEDIASTREAM_MIC,
                    ContentSettingsType::GEOLOCATION));
