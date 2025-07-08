// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/component_extension_content_settings/component_extension_content_settings_provider.h"

#include <utility>
#include <vector>

#include "base/test/gtest_util.h"
#include "chrome/browser/chromeos/extensions/component_extension_content_settings/component_extension_content_settings_allowlist.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/unloaded_extension_reason.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace extensions {

namespace {
inline constexpr char kNotAllowlistedId[] = "not-allowlisted-extension";

static const ComponentExtensionContentSettingsAllowlist::
    ExtensionsContentSettingsTypes
        kTestComponentExtensionsContentSettingsTypes = {
            {extension_misc::kPdfExtensionId,
             {ContentSettingsType::FILE_SYSTEM_READ_GUARD,
              ContentSettingsType::FILE_SYSTEM_WRITE_GUARD}},
            {kNotAllowlistedId, {}}};
}  // namespace

class ComponentExtensionContentSettingsProviderTest
    : public ChromeRenderViewHostTestHarness {
 public:
  void SetDefaultContentSetting(
      Profile* profile,
      const ContentSettingsType& content_settings_type,
      const ContentSetting& content_setting) {
    GetHostContentSettingsMap(profile)->SetDefaultContentSetting(
        content_settings_type, content_setting);
  }

  ContentSetting GetContentSettings(
      Profile* profile,
      const GURL& url,
      const ContentSettingsType& content_settings_type) {
    return GetHostContentSettingsMap(profile)->GetContentSetting(
        url, url, content_settings_type);
  }

  void RegisterExtension(Profile* profile, const std::string& extension_id) {
    const auto extension =
        ExtensionBuilder(extension_id).SetID(extension_id).Build();
    auto* extension_registry = ExtensionRegistry::Get(profile);

    extension_registry->AddEnabled(extension);
    extension_registry->TriggerOnLoaded(extension.get());
  }

  void UnregisterExetnsion(Profile* profile, const std::string& extension_id) {
    const auto extension =
        ExtensionBuilder(extension_id).SetID(extension_id).Build();
    auto* extension_registry = ExtensionRegistry::Get(profile);

    extension_registry->RemoveEnabled(extension->id());
    extension_registry->TriggerOnUnloaded(extension.get(),
                                          UnloadedExtensionReason::DISABLE);
  }

 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    ComponentExtensionContentSettingsAllowlist::
        SetComponentExtensionsContentSettingsTypesForTesting(
            kTestComponentExtensionsContentSettingsTypes);
  }

 private:
  HostContentSettingsMap* GetHostContentSettingsMap(Profile* profile) const {
    return HostContentSettingsMapFactory::GetForProfile(profile);
  }
};

TEST_F(ComponentExtensionContentSettingsProviderTest,
       RegisterPermissionsForAllowlistedExtension) {
  // Set default CONTENT_SETTING_BLOCK for FILE_SYSTEM_READ_GUARD and
  // FILE_SYSTEM_WRITE_GUARD.
  SetDefaultContentSetting(profile(),
                           ContentSettingsType::FILE_SYSTEM_READ_GUARD,
                           ContentSetting::CONTENT_SETTING_BLOCK);
  SetDefaultContentSetting(profile(),
                           ContentSettingsType::FILE_SYSTEM_WRITE_GUARD,
                           ContentSetting::CONTENT_SETTING_BLOCK);

  const auto pdf_component_extension_url =
      Extension::GetBaseURLFromExtensionId(extension_misc::kPdfExtensionId);
  const auto in_app_payment_component_extension_url =
      Extension::GetBaseURLFromExtensionId(
          extension_misc::kInAppPaymentsSupportAppId);

  // Check that pdf_component_extension_url and
  // app_payment_component_extension_url are not affected by
  // allowlisted_schemes. This mechanism take precedence over provider.
  ASSERT_EQ(ContentSetting::CONTENT_SETTING_BLOCK,
            GetContentSettings(profile(), pdf_component_extension_url,
                               ContentSettingsType::FILE_SYSTEM_READ_GUARD));
  ASSERT_EQ(ContentSetting::CONTENT_SETTING_BLOCK,
            GetContentSettings(profile(), pdf_component_extension_url,
                               ContentSettingsType::FILE_SYSTEM_WRITE_GUARD));
  ASSERT_EQ(
      ContentSetting::CONTENT_SETTING_BLOCK,
      GetContentSettings(profile(), in_app_payment_component_extension_url,
                         ContentSettingsType::FILE_SYSTEM_READ_GUARD));
  ASSERT_EQ(
      ContentSetting::CONTENT_SETTING_BLOCK,
      GetContentSettings(profile(), in_app_payment_component_extension_url,
                         ContentSettingsType::FILE_SYSTEM_WRITE_GUARD));

  // Register pdf and in app payment support components extensions
  RegisterExtension(profile(), extension_misc::kPdfExtensionId);
  RegisterExtension(profile(), extension_misc::kInAppPaymentsSupportAppId);

  // Check that pdf_component_extension_url has CONTENT_SETTING_ALLOW
  // for FILE_SYSTEM_READ_GUARD and FILE_SYSTEM_WRITE_GUARD.
  ASSERT_EQ(ContentSetting::CONTENT_SETTING_ALLOW,
            GetContentSettings(profile(), pdf_component_extension_url,
                               ContentSettingsType::FILE_SYSTEM_READ_GUARD));
  ASSERT_EQ(ContentSetting::CONTENT_SETTING_ALLOW,
            GetContentSettings(profile(), pdf_component_extension_url,
                               ContentSettingsType::FILE_SYSTEM_WRITE_GUARD));

  // Check that nothing has changed for in_app_payment_component_extension_url.
  ASSERT_EQ(
      ContentSetting::CONTENT_SETTING_BLOCK,
      GetContentSettings(profile(), in_app_payment_component_extension_url,
                         ContentSettingsType::FILE_SYSTEM_READ_GUARD));
  ASSERT_EQ(
      ContentSetting::CONTENT_SETTING_BLOCK,
      GetContentSettings(profile(), in_app_payment_component_extension_url,
                         ContentSettingsType::FILE_SYSTEM_WRITE_GUARD));

  // Unregister pdf and in app payment support components extensions
  UnregisterExetnsion(profile(), extension_misc::kPdfExtensionId);
  UnregisterExetnsion(profile(), extension_misc::kInAppPaymentsSupportAppId);

  // Check that pdf_component_extension_url has CONTENT_SETTING_BLOCK
  // for FILE_SYSTEM_READ_GUARD and FILE_SYSTEM_WRITE_GUARD.
  ASSERT_EQ(ContentSetting::CONTENT_SETTING_BLOCK,
            GetContentSettings(profile(), pdf_component_extension_url,
                               ContentSettingsType::FILE_SYSTEM_READ_GUARD));
  ASSERT_EQ(ContentSetting::CONTENT_SETTING_BLOCK,
            GetContentSettings(profile(), pdf_component_extension_url,
                               ContentSettingsType::FILE_SYSTEM_WRITE_GUARD));

  // Check that nothing has changed for in_app_payment_component_extension_url.
  ASSERT_EQ(
      ContentSetting::CONTENT_SETTING_BLOCK,
      GetContentSettings(profile(), in_app_payment_component_extension_url,
                         ContentSettingsType::FILE_SYSTEM_READ_GUARD));
  ASSERT_EQ(
      ContentSetting::CONTENT_SETTING_BLOCK,
      GetContentSettings(profile(), in_app_payment_component_extension_url,
                         ContentSettingsType::FILE_SYSTEM_WRITE_GUARD));
}

TEST_F(ComponentExtensionContentSettingsProviderTest,
       PermissionsArePerProfile) {
  TestingProfileManager profile_manager(TestingBrowserProcess::GetGlobal());
  ASSERT_TRUE(profile_manager.SetUp());

  // Create two profiles.
  Profile* profile_1 = profile_manager.CreateTestingProfile("profile_1");
  Profile* profile_2 = profile_manager.CreateTestingProfile("profile_2");

  // Set default CONTENT_SETTING_BLOCK for FILE_SYSTEM_READ_GUARD for both
  // profiles.
  SetDefaultContentSetting(profile_1,
                           ContentSettingsType::FILE_SYSTEM_READ_GUARD,
                           ContentSetting::CONTENT_SETTING_BLOCK);
  SetDefaultContentSetting(profile_2,
                           ContentSettingsType::FILE_SYSTEM_READ_GUARD,
                           ContentSetting::CONTENT_SETTING_BLOCK);

  const auto pdf_component_extension_url =
      Extension::GetBaseURLFromExtensionId(extension_misc::kPdfExtensionId);

  // Register pdf component extension for profile_1.
  RegisterExtension(profile_1, extension_misc::kPdfExtensionId);

  // Check that pdf_component_extension_url has CONTENT_SETTING_ALLOW
  // for FILE_SYSTEM_READ_GUARD in profile_1.
  ASSERT_EQ(ContentSetting::CONTENT_SETTING_ALLOW,
            GetContentSettings(profile_1, pdf_component_extension_url,
                               ContentSettingsType::FILE_SYSTEM_READ_GUARD));
  // Check that nothing has changed for profile_2.
  ASSERT_EQ(ContentSetting::CONTENT_SETTING_BLOCK,
            GetContentSettings(profile_2, pdf_component_extension_url,
                               ContentSettingsType::FILE_SYSTEM_READ_GUARD));

  // Unregister pdf component extension for profile_1.
  UnregisterExetnsion(profile_1, extension_misc::kPdfExtensionId);

  // Check that pdf_component_extension_url has CONTENT_SETTING_DEFAULT
  // for FILE_SYSTEM_READ_GUARD in profile_1.
  ASSERT_EQ(ContentSetting::CONTENT_SETTING_BLOCK,
            GetContentSettings(profile_1, pdf_component_extension_url,
                               ContentSettingsType::FILE_SYSTEM_READ_GUARD));
  // Check that nothing has changed for profile_2.
  ASSERT_EQ(ContentSetting::CONTENT_SETTING_BLOCK,
            GetContentSettings(profile_2, pdf_component_extension_url,
                               ContentSettingsType::FILE_SYSTEM_READ_GUARD));
}

TEST_F(ComponentExtensionContentSettingsProviderTest,
       RegisterPermissionsForSpecialProfiles) {
  TestingProfileManager profile_manager(TestingBrowserProcess::GetGlobal());
  ASSERT_TRUE(profile_manager.SetUp());

  // Create special profiles.
  TestingProfile* original_profile =
      profile_manager.CreateTestingProfile("original_profile");
  TestingProfile* guest_profile =
      profile_manager.CreateTestingProfile("guest_profile");
  guest_profile->SetGuestSession(true);
  TestingProfile* off_the_record_profile =
      TestingProfile::Builder().BuildOffTheRecord(
          original_profile, Profile::OTRProfileID::CreateUniqueForTesting());

  // Set default CONTENT_SETTING_BLOCK for FILE_SYSTEM_READ_GUARD for special
  // profiles.
  SetDefaultContentSetting(guest_profile,
                           ContentSettingsType::FILE_SYSTEM_READ_GUARD,
                           CONTENT_SETTING_BLOCK);
  SetDefaultContentSetting(original_profile,
                           ContentSettingsType::FILE_SYSTEM_READ_GUARD,
                           CONTENT_SETTING_BLOCK);

  const auto pdf_component_extension_url =
      Extension::GetBaseURLFromExtensionId(extension_misc::kPdfExtensionId);

  // Register pdf component extension for guest_profile.
  RegisterExtension(guest_profile, extension_misc::kPdfExtensionId);

  // Check that pdf_component_extension_url has CONTENT_SETTING_ALLOW for
  // FILE_SYSTEM_READ_GUARD in guest_profile.
  ASSERT_EQ(ContentSetting::CONTENT_SETTING_ALLOW,
            GetContentSettings(guest_profile, pdf_component_extension_url,
                               ContentSettingsType::FILE_SYSTEM_READ_GUARD));
  // Check that nothing has changed for off_the_record_profile.
  ASSERT_EQ(
      ContentSetting::CONTENT_SETTING_BLOCK,
      GetContentSettings(off_the_record_profile, pdf_component_extension_url,
                         ContentSettingsType::FILE_SYSTEM_READ_GUARD));

  // Register pdf component extension for off_the_record_profile.
  RegisterExtension(off_the_record_profile, extension_misc::kPdfExtensionId);

  // Check that nothing has changed for guest_profile.
  ASSERT_EQ(ContentSetting::CONTENT_SETTING_ALLOW,
            GetContentSettings(guest_profile, pdf_component_extension_url,
                               ContentSettingsType::FILE_SYSTEM_READ_GUARD));
  // Check that pdf_component_extension_url has CONTENT_SETTING_ALLOW for
  // FILE_SYSTEM_READ_GUARD in off_the_record_profile.
  ASSERT_EQ(
      ContentSetting::CONTENT_SETTING_ALLOW,
      GetContentSettings(off_the_record_profile, pdf_component_extension_url,
                         ContentSettingsType::FILE_SYSTEM_READ_GUARD));

  // Unregister pdf component extension for off_the_record_profile.
  UnregisterExetnsion(off_the_record_profile, extension_misc::kPdfExtensionId);

  // Check that pdf_component_extension_url has CONTENT_SETTING_BLOCK for
  // FILE_SYSTEM_READ_GUARD in off_the_record_profile.
  ASSERT_EQ(
      ContentSetting::CONTENT_SETTING_BLOCK,
      GetContentSettings(off_the_record_profile, pdf_component_extension_url,
                         ContentSettingsType::FILE_SYSTEM_READ_GUARD));
  // Check that nothing has changed for guest_profile.
  ASSERT_EQ(ContentSetting::CONTENT_SETTING_ALLOW,
            GetContentSettings(guest_profile, pdf_component_extension_url,
                               ContentSettingsType::FILE_SYSTEM_READ_GUARD));

  // Unregister pdf component extension for guest_profile.
  UnregisterExetnsion(guest_profile, extension_misc::kPdfExtensionId);

  // Check that nothing has changed for off_the_record_profile.
  ASSERT_EQ(
      ContentSetting::CONTENT_SETTING_BLOCK,
      GetContentSettings(off_the_record_profile, pdf_component_extension_url,
                         ContentSettingsType::FILE_SYSTEM_READ_GUARD));
  // Check that pdf_component_extension_url has CONTENT_SETTING_BLOCK for
  // FILE_SYSTEM_READ_GUARD in guest_profile.
  ASSERT_EQ(ContentSetting::CONTENT_SETTING_BLOCK,
            GetContentSettings(guest_profile, pdf_component_extension_url,
                               ContentSettingsType::FILE_SYSTEM_READ_GUARD));
}

TEST_F(ComponentExtensionContentSettingsProviderTest,
       RegisterPermissionForNotAllowlistedExtension) {
  EXPECT_DEATH_IF_SUPPORTED(RegisterExtension(profile(), kNotAllowlistedId),
                            std::string());
}
}  // namespace extensions
