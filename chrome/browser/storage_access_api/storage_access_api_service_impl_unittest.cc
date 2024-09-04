// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/storage_access_api/storage_access_api_service_impl.h"

#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/test/simple_test_clock.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/storage_access_api/storage_access_api_service_factory.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_constraints.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/features_generated.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {
constexpr char kHostA[] = "a.test";
constexpr char kHostB[] = "b.test";
}  // namespace

class StorageAccessAPIServiceImplTest : public testing::Test {
 public:
  StorageAccessAPIServiceImplTest() = default;

  void SetUp() override {
    // TODO(crbug.com/362466866): Instead of disabling the
    // `kSafetyHubAbusiveNotificationRevocation` feature, find a stable
    // fix such that the tests still pass when the feature is enabled.
    features_.InitWithFeatures(
        {}, {safe_browsing::kSafetyHubAbusiveNotificationRevocation});
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    profile_ = profile_manager_->CreateTestingProfile("TestProfile");
  }

  void TearDown() override {
    profile_ = nullptr;
    profile_manager_->DeleteAllTestingProfiles();
    profile_manager_.reset();
  }

  content::BrowserTaskEnvironment& env() { return env_; }

 protected:
  Profile* profile() { return profile_; }

 private:
  content::BrowserTaskEnvironment env_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<TestingProfileManager> profile_manager_;
  raw_ptr<Profile> profile_;
  base::test::ScopedFeatureList features_;
};

TEST_F(StorageAccessAPIServiceImplTest, RenewPermissionGrant) {
  url::Origin origin_a(
      url::Origin::Create(GURL(base::StrCat({"https://", kHostA}))));
  url::Origin origin_b(
      url::Origin::Create(GURL(base::StrCat({"https://", kHostB}))));

  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile());

  settings_map->SetClockForTesting(env().GetMockClock());

  content_settings::ContentSettingConstraints constraints(
      env().GetMockClock()->Now());
  constraints.set_lifetime(base::Days(30));

  settings_map->SetContentSettingDefaultScope(
      origin_a.GetURL(), origin_b.GetURL(), ContentSettingsType::STORAGE_ACCESS,
      CONTENT_SETTING_ALLOW, constraints);

  StorageAccessAPIServiceImpl* service =
      StorageAccessAPIServiceFactory::GetForBrowserContext(profile());
  ASSERT_NE(nullptr, service);

  env().FastForwardBy(base::Days(20));

  // 20 days into a 30 day lifetime, so the setting hasn't expired yet.
  EXPECT_EQ(CONTENT_SETTING_ALLOW, settings_map->GetContentSetting(
                                       origin_a.GetURL(), origin_b.GetURL(),
                                       ContentSettingsType::STORAGE_ACCESS));

  EXPECT_TRUE(service->RenewPermissionGrant(origin_a, origin_b));

  env().FastForwardBy(base::Days(20));
  // The 30d lifetime was renewed 20 days ago, so it hasn't expired yet.
  EXPECT_EQ(CONTENT_SETTING_ALLOW, settings_map->GetContentSetting(
                                       origin_a.GetURL(), origin_b.GetURL(),
                                       ContentSettingsType::STORAGE_ACCESS));

  env().FastForwardBy(base::Days(11));
  // The 30d lifetime was renewed 31 days ago, so it has expired now.
  EXPECT_EQ(CONTENT_SETTING_ASK, settings_map->GetContentSetting(
                                     origin_a.GetURL(), origin_b.GetURL(),
                                     ContentSettingsType::STORAGE_ACCESS));

  EXPECT_FALSE(service->RenewPermissionGrant(origin_a, origin_b));
  // The setting was already expired, so renewing it has no effect.
  EXPECT_EQ(CONTENT_SETTING_ASK, settings_map->GetContentSetting(
                                     origin_a.GetURL(), origin_b.GetURL(),
                                     ContentSettingsType::STORAGE_ACCESS));
}

TEST_F(StorageAccessAPIServiceImplTest, PermissionDenial_NotRenewed) {
  url::Origin origin_a(
      url::Origin::Create(GURL(base::StrCat({"https://", kHostA}))));
  url::Origin origin_b(
      url::Origin::Create(GURL(base::StrCat({"https://", kHostB}))));

  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile());

  settings_map->SetClockForTesting(env().GetMockClock());

  content_settings::ContentSettingConstraints constraints(
      env().GetMockClock()->Now());
  constraints.set_lifetime(base::Days(30));

  settings_map->SetContentSettingDefaultScope(
      origin_a.GetURL(), origin_b.GetURL(), ContentSettingsType::STORAGE_ACCESS,
      CONTENT_SETTING_BLOCK, constraints);

  StorageAccessAPIServiceImpl* service =
      StorageAccessAPIServiceFactory::GetForBrowserContext(profile());
  ASSERT_NE(nullptr, service);

  env().FastForwardBy(base::Days(20));

  // 20 days into a 30 day lifetime, so the setting hasn't expired yet.
  EXPECT_EQ(CONTENT_SETTING_BLOCK, settings_map->GetContentSetting(
                                       origin_a.GetURL(), origin_b.GetURL(),
                                       ContentSettingsType::STORAGE_ACCESS));

  EXPECT_FALSE(service->RenewPermissionGrant(origin_a, origin_b));

  env().FastForwardBy(base::Days(20));
  // Denials are not renewed by user interaction, so the setting has expired by
  // now.
  EXPECT_EQ(CONTENT_SETTING_ASK, settings_map->GetContentSetting(
                                     origin_a.GetURL(), origin_b.GetURL(),
                                     ContentSettingsType::STORAGE_ACCESS));
}

TEST_F(StorageAccessAPIServiceImplTest, RenewPermissionGrant_DailyCache) {
  StorageAccessAPIServiceImpl* service =
      StorageAccessAPIServiceFactory::GetForBrowserContext(profile());
  ASSERT_NE(nullptr, service);

  url::Origin origin_a(
      url::Origin::Create(GURL(base::StrCat({"https://", kHostA}))));
  url::Origin origin_b(
      url::Origin::Create(GURL(base::StrCat({"https://", kHostB}))));

  content_settings::ContentSettingConstraints constraints;
  constraints.set_lifetime(base::Days(30));

  HostContentSettingsMapFactory::GetForProfile(profile())
      ->SetContentSettingDefaultScope(origin_a.GetURL(), origin_b.GetURL(),
                                      ContentSettingsType::STORAGE_ACCESS,
                                      CONTENT_SETTING_ALLOW, constraints);

  EXPECT_TRUE(service->RenewPermissionGrant(origin_a, origin_b));
  EXPECT_FALSE(service->RenewPermissionGrant(origin_a, origin_b));

  // The first cache reset should happen 24 hours after test start.
  env().FastForwardBy(base::Days(1));

  EXPECT_TRUE(service->RenewPermissionGrant(origin_a, origin_b));
  EXPECT_FALSE(service->RenewPermissionGrant(origin_a, origin_b));

  // The next cache reset should happen in another 24 hours.
  env().FastForwardBy(base::Hours(23));

  EXPECT_FALSE(service->RenewPermissionGrant(origin_a, origin_b));

  env().FastForwardBy(base::Hours(1));

  EXPECT_TRUE(service->RenewPermissionGrant(origin_a, origin_b));
  EXPECT_FALSE(service->RenewPermissionGrant(origin_a, origin_b));
}
