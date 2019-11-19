// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_util.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/permissions/permission_uma_util.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

class PermissionUtilTest : public testing::Test {
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(PermissionUtilTest, ScopedRevocationReporter) {
  TestingProfile profile;
  ASSERT_TRUE(profile.CreateHistoryService(
      /* delete_file= */ true,
      /* no_db= */ false));

  // TODO(tsergeant): Add more comprehensive tests of PermissionUmaUtil.
  base::HistogramTester histograms;
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(&profile);
  GURL host("https://example.com");
  ContentSettingsPattern host_pattern =
      ContentSettingsPattern::FromURLNoWildcard(host);
  ContentSettingsPattern host_containing_wildcards_pattern =
      ContentSettingsPattern::FromString("https://[*.]example.com/");
  ContentSettingsType type = ContentSettingsType::GEOLOCATION;
  PermissionSourceUI source_ui = PermissionSourceUI::SITE_SETTINGS;

  // Allow->Block triggers a revocation.
  map->SetContentSettingDefaultScope(host, host, type, std::string(),
                                     CONTENT_SETTING_ALLOW);
  {
    PermissionUtil::ScopedRevocationReporter scoped_revocation_reporter(
        &profile, host, host, type, source_ui);
    map->SetContentSettingDefaultScope(host, host, type, std::string(),
                                       CONTENT_SETTING_BLOCK);
  }
  histograms.ExpectBucketCount("Permissions.Action.Geolocation",
                               static_cast<int>(PermissionAction::REVOKED), 1);

  // Block->Allow does not trigger a revocation.
  {
    PermissionUtil::ScopedRevocationReporter scoped_revocation_reporter(
        &profile, host, host, type, source_ui);
    map->SetContentSettingDefaultScope(host, host, type, std::string(),
                                       CONTENT_SETTING_ALLOW);
  }
  histograms.ExpectBucketCount("Permissions.Action.Geolocation",
                               static_cast<int>(PermissionAction::REVOKED), 1);

  // Allow->Default triggers a revocation when default is 'ask'.
  map->SetDefaultContentSetting(type, CONTENT_SETTING_ASK);
  {
    PermissionUtil::ScopedRevocationReporter scoped_revocation_reporter(
        &profile, host, host, type, source_ui);
    map->SetContentSettingDefaultScope(host, host, type, std::string(),
                                       CONTENT_SETTING_DEFAULT);
  }
  histograms.ExpectBucketCount("Permissions.Action.Geolocation",
                               static_cast<int>(PermissionAction::REVOKED), 2);

  // Allow->Default does not trigger a revocation when default is 'allow'.
  map->SetDefaultContentSetting(type, CONTENT_SETTING_ALLOW);
  {
    PermissionUtil::ScopedRevocationReporter scoped_revocation_reporter(
        &profile, host, host, type, source_ui);
    map->SetContentSettingDefaultScope(host, host, type, std::string(),
                                       CONTENT_SETTING_DEFAULT);
  }
  histograms.ExpectBucketCount("Permissions.Action.Geolocation",
                               static_cast<int>(PermissionAction::REVOKED), 2);

  // Allow->Block with url pattern string triggers a revocation.
  map->SetContentSettingDefaultScope(host, host, type, std::string(),
                                     CONTENT_SETTING_ALLOW);
  {
    PermissionUtil::ScopedRevocationReporter scoped_revocation_reporter(
        &profile, host_pattern, host_pattern, type, source_ui);
    map->SetContentSettingCustomScope(host_pattern, host_pattern, type,
                                      std::string(), CONTENT_SETTING_BLOCK);
  }
  histograms.ExpectBucketCount("Permissions.Action.Geolocation",
                               static_cast<int>(PermissionAction::REVOKED), 3);

  // Allow->Block with non url pattern string does not trigger a revocation.
  map->SetContentSettingDefaultScope(host, host, type, std::string(),
                                     CONTENT_SETTING_ALLOW);
  {
    PermissionUtil::ScopedRevocationReporter scoped_revocation_reporter(
        &profile, host_containing_wildcards_pattern, host_pattern, type,
        source_ui);
    map->SetContentSettingCustomScope(host_containing_wildcards_pattern,
                                      host_pattern, type, std::string(),
                                      CONTENT_SETTING_BLOCK);
  }
  histograms.ExpectBucketCount("Permissions.Action.Geolocation",
                               static_cast<int>(PermissionAction::REVOKED), 3);
}
