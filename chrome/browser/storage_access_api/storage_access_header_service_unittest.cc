// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/storage_access_api/storage_access_header_service.h"

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/content_settings/core/common/content_settings.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/schemeful_site.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace storage_access_api::trial {

constexpr char kDomainEnabledForTrial[] = "a.test";

// This class uses the bool parameter to test behaviors when the
// `STORAGE_ACCESS_HEADER_ORIGIN_TRIAL` is enabled for subdomains (true), and
// when it is not (false).
class StorageAccessHeaderServiceTest : public testing::TestWithParam<bool> {
 public:
  StorageAccessHeaderServiceTest() = default;

  void SetUp() override { profile_ = std::make_unique<TestingProfile>(); }

  bool match_subdomains() const { return GetParam(); }

  TestingProfile* GetProfile() { return profile_.get(); }

 private:
  content::BrowserTaskEnvironment env_;
  std::unique_ptr<TestingProfile> profile_;
};

INSTANTIATE_TEST_SUITE_P(, StorageAccessHeaderServiceTest, testing::Bool());

TEST_P(StorageAccessHeaderServiceTest,
       UpdateStorageAccessHeadersTrial_BlockToAllow) {
  url::Origin origin = url::Origin::Create(
      GURL(base::StrCat({"https://", kDomainEnabledForTrial})));
  url::Origin partition_origin = url::Origin::Create(GURL("https://b.test"));

  content::OriginTrialStatusChangeDetails details(
      origin, net::SchemefulSite(partition_origin).Serialize(),
      match_subdomains(), /*enabled=*/true, std::nullopt);

  StorageAccessHeaderService service(GetProfile());
  service.UpdateSettingsForTesting(details);

  auto* settings_map =
      HostContentSettingsMapFactory::GetForProfile(GetProfile());
  EXPECT_EQ(
      settings_map->GetContentSetting(
          origin.GetURL(), partition_origin.GetURL(),
          ContentSettingsType::STORAGE_ACCESS_HEADER_ORIGIN_TRIAL, nullptr),
      CONTENT_SETTING_ALLOW);

  url::Origin subdomain_origin = url::Origin::Create(
      GURL(base::StrCat({"https://subdomain.", kDomainEnabledForTrial})));
  EXPECT_EQ(
      settings_map->GetContentSetting(
          subdomain_origin.GetURL(), partition_origin.GetURL(),
          ContentSettingsType::STORAGE_ACCESS_HEADER_ORIGIN_TRIAL, nullptr),
      match_subdomains() ? CONTENT_SETTING_ALLOW : CONTENT_SETTING_BLOCK);
}

TEST_P(StorageAccessHeaderServiceTest,
       UpdateStorageAccessHeadersTrial_AllowToBlock) {
  url::Origin origin = url::Origin::Create(
      GURL(base::StrCat({"https://", kDomainEnabledForTrial})));
  url::Origin partition_origin = url::Origin::Create(GURL("https://b.test"));
  auto* settings_map =
      HostContentSettingsMapFactory::GetForProfile(GetProfile());

  if (match_subdomains()) {
    settings_map->SetContentSettingCustomScope(
        ContentSettingsPattern::FromURL(origin.GetURL()),
        ContentSettingsPattern::FromURLToSchemefulSitePattern(
            partition_origin.GetURL()),
        ContentSettingsType::STORAGE_ACCESS_HEADER_ORIGIN_TRIAL,
        CONTENT_SETTING_ALLOW);
  } else {
    settings_map->SetContentSettingDefaultScope(
        origin.GetURL(), partition_origin.GetURL(),
        ContentSettingsType::STORAGE_ACCESS_HEADER_ORIGIN_TRIAL,
        CONTENT_SETTING_ALLOW);
  }
  ASSERT_EQ(
      settings_map->GetContentSetting(
          origin.GetURL(), partition_origin.GetURL(),
          ContentSettingsType::STORAGE_ACCESS_HEADER_ORIGIN_TRIAL, nullptr),
      CONTENT_SETTING_ALLOW);

  StorageAccessHeaderService service(GetProfile());
  content::OriginTrialStatusChangeDetails details(
      origin, net::SchemefulSite(partition_origin).Serialize(),
      match_subdomains(), /*enabled=*/false, std::nullopt);
  service.UpdateSettingsForTesting(details);

  EXPECT_EQ(
      settings_map->GetContentSetting(
          origin.GetURL(), partition_origin.GetURL(),
          ContentSettingsType::STORAGE_ACCESS_HEADER_ORIGIN_TRIAL, nullptr),
      CONTENT_SETTING_BLOCK);
}

}  // namespace storage_access_api::trial
