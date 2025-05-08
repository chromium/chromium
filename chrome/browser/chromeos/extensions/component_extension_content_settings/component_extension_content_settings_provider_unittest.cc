// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/component_extension_content_settings/component_extension_content_settings_provider.h"

#include <utility>
#include <vector>

#include "base/strings/strcat.h"
#include "base/test/gtest_util.h"
#include "chrome/browser/chromeos/extensions/component_extension_content_settings/component_extension_content_settings_allowlist.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace extensions {

namespace {
inline constexpr char kNonExtensionScheme[] = "non-extension";
inline constexpr char kNotAllowlistedId[] = "not-allowlisted-extension";

GURL CreateURL(const std::string& scheme, const std::string& origin) {
  return GURL(base::StrCat({scheme, url::kStandardSchemeSeparator, origin}));
}
GURL CreateExtensionURL(const std::string& extension_id) {
  return CreateURL(kExtensionScheme, extension_id);
}
}  // namespace

class ComponentExtensionContentSettingsProviderTest
    : public ChromeRenderViewHostTestHarness {
 public:
  HostContentSettingsMap* GetHostContentSettingsMap(Profile* profile) {
    return HostContentSettingsMapFactory::GetForProfile(profile);
  }
  HostContentSettingsMap* GetHostContentSettingsMap() {
    return GetHostContentSettingsMap(profile());
  }

  ComponentExtensionContentSettingsAllowlist* GetAllowlist(Profile* profile) {
    return ComponentExtensionContentSettingsAllowlist::Get(profile);
  }
  ComponentExtensionContentSettingsAllowlist* GetAllowlist() {
    return GetAllowlist(profile());
  }
};

TEST_F(ComponentExtensionContentSettingsProviderTest,
       RegisterPermissionForAllowlistedExtension) {
  auto* map = GetHostContentSettingsMap();

  // Set default CONTENT_SETTING_BLOCK for FILE_SYSTEM_READ_GUARD.
  map->SetDefaultContentSetting(ContentSettingsType::FILE_SYSTEM_READ_GUARD,
                                ContentSetting::CONTENT_SETTING_BLOCK);

  const auto pdf_component_extension_url =
      CreateExtensionURL(extension_misc::kPdfExtensionId);
  const auto app_payment_component_extension_url =
      CreateExtensionURL(extension_misc::kInAppPaymentsSupportAppId);

  // Check that pdf_component_extension_url and
  // app_payment_component_extension_url are not affected by
  // allowlisted_schemes. This mechanism take precedence over provider.
  ASSERT_EQ(ContentSetting::CONTENT_SETTING_BLOCK,
            map->GetContentSetting(
                pdf_component_extension_url, pdf_component_extension_url,
                ContentSettingsType::FILE_SYSTEM_READ_GUARD));
  ASSERT_EQ(
      ContentSetting::CONTENT_SETTING_BLOCK,
      map->GetContentSetting(app_payment_component_extension_url,
                             app_payment_component_extension_url,
                             ContentSettingsType::FILE_SYSTEM_READ_GUARD));

  // Grant CONTENT_SETTING_ALLOW to pdf_component_extension_url for
  // FILE_SYSTEM_READ_GUARD
  GetAllowlist()->RegisterAutoGrantedPermission(
      url::Origin::Create(pdf_component_extension_url),
      ContentSettingsType::FILE_SYSTEM_READ_GUARD);

  // Check that pdf_component_extension_url has CONTENT_SETTING_ALLOW for
  // FILE_SYSTEM_READ_GUARD
  EXPECT_EQ(ContentSetting::CONTENT_SETTING_ALLOW,
            map->GetContentSetting(
                pdf_component_extension_url, pdf_component_extension_url,
                ContentSettingsType::FILE_SYSTEM_READ_GUARD));
  // Check that nothing has changed for app_payment_component_extension_url
  EXPECT_EQ(
      ContentSetting::CONTENT_SETTING_BLOCK,
      map->GetContentSetting(app_payment_component_extension_url,
                             app_payment_component_extension_url,
                             ContentSettingsType::FILE_SYSTEM_READ_GUARD));
}

TEST_F(ComponentExtensionContentSettingsProviderTest,
       RegisterPermissionsForAllowlistedExtension) {
  auto* map = GetHostContentSettingsMap();

  // Set default CONTENT_SETTING_BLOCK for FILE_SYSTEM_READ_GUARD and
  // FILE_SYSTEM_WRITE_GUARD
  map->SetDefaultContentSetting(ContentSettingsType::FILE_SYSTEM_READ_GUARD,
                                ContentSetting::CONTENT_SETTING_BLOCK);
  map->SetDefaultContentSetting(ContentSettingsType::FILE_SYSTEM_WRITE_GUARD,
                                ContentSetting::CONTENT_SETTING_BLOCK);

  const auto pdf_component_extension_url =
      CreateExtensionURL(extension_misc::kPdfExtensionId);

  // Check that pdf_component_extension_url are not affected by
  // allowlisted_schemes. This mechanism take precedence over provider.
  ASSERT_EQ(ContentSetting::CONTENT_SETTING_BLOCK,
            map->GetContentSetting(
                pdf_component_extension_url, pdf_component_extension_url,
                ContentSettingsType::FILE_SYSTEM_READ_GUARD));
  ASSERT_EQ(ContentSetting::CONTENT_SETTING_BLOCK,
            map->GetContentSetting(
                pdf_component_extension_url, pdf_component_extension_url,
                ContentSettingsType::FILE_SYSTEM_WRITE_GUARD));

  // Grant CONTENT_SETTING_ALLOW to pdf_component_extension_url for
  // FILE_SYSTEM_READ_GUARD and FILE_SYSTEM_WRITE_GUARD
  GetAllowlist()->RegisterAutoGrantedPermissions(
      url::Origin::Create(pdf_component_extension_url),
      {ContentSettingsType::FILE_SYSTEM_READ_GUARD,
       ContentSettingsType::FILE_SYSTEM_WRITE_GUARD});

  // Check that pdf_component_extension_url has CONTENT_SETTING_ALLOW for
  // FILE_SYSTEM_READ_GUARD and FILE_SYSTEM_WRITE_GUARD
  EXPECT_EQ(ContentSetting::CONTENT_SETTING_ALLOW,
            map->GetContentSetting(
                pdf_component_extension_url, pdf_component_extension_url,
                ContentSettingsType::FILE_SYSTEM_READ_GUARD));
  EXPECT_EQ(ContentSetting::CONTENT_SETTING_ALLOW,
            map->GetContentSetting(
                pdf_component_extension_url, pdf_component_extension_url,
                ContentSettingsType::FILE_SYSTEM_WRITE_GUARD));
}

TEST_F(ComponentExtensionContentSettingsProviderTest, PermissionIsPerProfile) {
  TestingProfileManager profile_manager(TestingBrowserProcess::GetGlobal());
  ASSERT_TRUE(profile_manager.SetUp());

  // Create two profiles.
  Profile* profile_1 = profile_manager.CreateTestingProfile("profile_1");
  Profile* profile_2 = profile_manager.CreateTestingProfile("profile_2");

  // Set default CONTENT_SETTING_BLOCK for FILE_SYSTEM_READ_GUARD for both
  // profiles
  auto* map_1 = GetHostContentSettingsMap(profile_1);
  map_1->SetDefaultContentSetting(ContentSettingsType::FILE_SYSTEM_READ_GUARD,
                                  CONTENT_SETTING_BLOCK);
  auto* map_2 = GetHostContentSettingsMap(profile_2);
  map_2->SetDefaultContentSetting(ContentSettingsType::FILE_SYSTEM_READ_GUARD,
                                  CONTENT_SETTING_BLOCK);

  const auto pdf_component_extension_url =
      CreateExtensionURL(extension_misc::kPdfExtensionId);

  // Grant CONTENT_SETTING_ALLOW for FILE_SYSTEM_READ_GUARD to
  // pdf_component_extension_url for profile_1
  GetAllowlist(profile_1)->RegisterAutoGrantedPermission(
      url::Origin::Create(pdf_component_extension_url),
      ContentSettingsType::FILE_SYSTEM_READ_GUARD);

  // Check that pdf_component_extension_url has CONTENT_SETTING_ALLOW for
  // FILE_SYSTEM_READ_GUARD in profile_1
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            map_1->GetContentSetting(
                pdf_component_extension_url, pdf_component_extension_url,
                ContentSettingsType::FILE_SYSTEM_READ_GUARD));
  // Check that nothing has changed for profile_2
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            map_2->GetContentSetting(
                pdf_component_extension_url, pdf_component_extension_url,
                ContentSettingsType::FILE_SYSTEM_READ_GUARD));
}

TEST_F(ComponentExtensionContentSettingsProviderTest,
       RegisterPermissionsForSpecialProfiles) {
  TestingProfileManager profile_manager(TestingBrowserProcess::GetGlobal());
  ASSERT_TRUE(profile_manager.SetUp());

  // Create special profiles.
  TestingProfile* guest_profile =
      profile_manager.CreateTestingProfile("guest_profile");
  guest_profile->SetGuestSession(true);
  TestingProfile* original_profile =
      profile_manager.CreateTestingProfile("original_profile");
  TestingProfile* off_the_record_profile =
      TestingProfile::Builder().BuildOffTheRecord(
          original_profile, Profile::OTRProfileID::CreateUniqueForTesting());

  // Set default CONTENT_SETTING_BLOCK for FILE_SYSTEM_READ_GUARD for both
  // profiles
  auto* guest_map = GetHostContentSettingsMap(guest_profile);
  guest_map->SetDefaultContentSetting(
      ContentSettingsType::FILE_SYSTEM_READ_GUARD, CONTENT_SETTING_BLOCK);
  auto* off_the_record_map = GetHostContentSettingsMap(off_the_record_profile);
  off_the_record_map->SetDefaultContentSetting(
      ContentSettingsType::FILE_SYSTEM_READ_GUARD, CONTENT_SETTING_BLOCK);

  const auto pdf_component_extension_url =
      CreateExtensionURL(extension_misc::kPdfExtensionId);

  // Grant CONTENT_SETTING_ALLOW for FILE_SYSTEM_READ_GUARD to
  // pdf_component_extension_url for guest_profile and off_the_record_profile
  GetAllowlist(guest_profile)
      ->RegisterAutoGrantedPermission(
          url::Origin::Create(pdf_component_extension_url),
          ContentSettingsType::FILE_SYSTEM_READ_GUARD);
  GetAllowlist(off_the_record_profile)
      ->RegisterAutoGrantedPermission(
          url::Origin::Create(pdf_component_extension_url),
          ContentSettingsType::FILE_SYSTEM_READ_GUARD);

  // Check that pdf_component_extension_url has CONTENT_SETTING_ALLOW for
  // FILE_SYSTEM_READ_GUARD in guest_profile and off_the_record_profile
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            guest_map->GetContentSetting(
                pdf_component_extension_url, pdf_component_extension_url,
                ContentSettingsType::FILE_SYSTEM_READ_GUARD));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            off_the_record_map->GetContentSetting(
                pdf_component_extension_url, pdf_component_extension_url,
                ContentSettingsType::FILE_SYSTEM_READ_GUARD));
}

TEST_F(ComponentExtensionContentSettingsProviderTest,
       RegisterPermissionForNotAllowlistedExtension) {
  const std::vector<GURL> bad_examples = {
      // The scheme and ExtensionId are both incorrect
      CreateURL(kNonExtensionScheme, kNotAllowlistedId),
      // The ExtensionId is incorrect
      CreateExtensionURL(kNotAllowlistedId),
      // The scheme is incorrect
      CreateURL(kNonExtensionScheme, extension_misc::kPdfExtensionId)};

  for (const auto& gurl : bad_examples) {
    EXPECT_DEATH_IF_SUPPORTED(GetAllowlist()->RegisterAutoGrantedPermission(
                                  url::Origin::Create(gurl),
                                  ContentSettingsType::FILE_SYSTEM_READ_GUARD),
                              std::string());
  }
}
}  // namespace extensions
