// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/generated_cookie_prefs.h"

#include <memory>
#include <tuple>

#include "base/ranges/algorithm.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/extensions/api/settings_private/generated_pref.h"
#include "chrome/browser/extensions/api/settings_private/generated_pref_test_base.h"
#include "chrome/common/extensions/api/settings_private.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/content_settings/core/test/content_settings_mock_provider.h"
#include "components/content_settings/core/test/content_settings_test_utils.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content_settings {
namespace {

namespace settings_api = ::extensions::api::settings_private;
namespace settings_private = ::extensions::settings_private;

typedef extensions::settings_private::GeneratedPrefTestBase
    GeneratedCookiePrefsTest;

TEST_F(GeneratedCookiePrefsTest, DefaultContentSettingPrefValidType) {
  auto pref = std::make_unique<
      content_settings::GeneratedCookieDefaultContentSettingPref>(profile());
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile());

  // Ensure that the preference represents the content setting value.
  map->SetDefaultContentSetting(ContentSettingsType::COOKIES,
                                CONTENT_SETTING_ALLOW);
  std::optional<extensions::api::settings_private::PrefObject> pref_object =
      pref->GetPrefObject();
  EXPECT_EQ(pref_object->value->GetString(), "allow");

  // Ensure setting the preference correctly updates content settings and the
  // preference state.
  EXPECT_EQ(pref->SetPref(std::make_unique<base::Value>("session_only").get()),
            extensions::settings_private::SetPrefResult::SUCCESS);
  EXPECT_EQ(map->GetDefaultContentSetting(ContentSettingsType::COOKIES),
            CONTENT_SETTING_SESSION_ONLY);
  pref_object = pref->GetPrefObject();
  EXPECT_EQ(pref_object->value->GetString(), "session_only");

  EXPECT_EQ(pref->SetPref(std::make_unique<base::Value>("allow").get()),
            extensions::settings_private::SetPrefResult::SUCCESS);
  EXPECT_EQ(map->GetDefaultContentSetting(ContentSettingsType::COOKIES),
            CONTENT_SETTING_ALLOW);
  pref_object = pref->GetPrefObject();
  EXPECT_EQ(pref_object->value->GetString(), "allow");

  EXPECT_EQ(pref->SetPref(std::make_unique<base::Value>("block").get()),
            extensions::settings_private::SetPrefResult::SUCCESS);
  EXPECT_EQ(map->GetDefaultContentSetting(ContentSettingsType::COOKIES),
            CONTENT_SETTING_BLOCK);
  pref_object = pref->GetPrefObject();
  EXPECT_EQ(pref_object->value->GetString(), "block");
}

TEST_F(GeneratedCookiePrefsTest, DefaultContentSettingPrefTypeMismatch) {
  auto pref = std::make_unique<
      content_settings::GeneratedCookieDefaultContentSettingPref>(profile());

  // Confirm that a type mismatch is reported as such.
  EXPECT_EQ(pref->SetPref(std::make_unique<base::Value>(false).get()),
            extensions::settings_private::SetPrefResult::PREF_TYPE_MISMATCH);
  EXPECT_EQ(pref->SetPref(std::make_unique<base::Value>("default").get()),
            extensions::settings_private::SetPrefResult::PREF_TYPE_MISMATCH);
  EXPECT_EQ(pref->SetPref(std::make_unique<base::Value>("ask").get()),
            extensions::settings_private::SetPrefResult::PREF_TYPE_MISMATCH);
  EXPECT_EQ(
      pref->SetPref(
          std::make_unique<base::Value>("detect_important_content").get()),
      extensions::settings_private::SetPrefResult::PREF_TYPE_MISMATCH);
  EXPECT_EQ(pref->SetPref(std::make_unique<base::Value>(100).get()),
            extensions::settings_private::SetPrefResult::PREF_TYPE_MISMATCH);
}

TEST_F(GeneratedCookiePrefsTest, DefaultContentSettingPrefEnforced) {
  auto pref = std::make_unique<
      content_settings::GeneratedCookieDefaultContentSettingPref>(profile());
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile());

  // Ensure management state is correctly reported for all possible content
  // setting management sources.
  auto provider = std::make_unique<content_settings::MockProvider>();
  provider->SetWebsiteSetting(
      ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
      ContentSettingsType::COOKIES, base::Value(CONTENT_SETTING_ALLOW),
      /*constraints=*/{},
      content_settings::PartitionKey::GetDefaultForTesting());
  content_settings::TestUtils::OverrideProvider(
      map, std::move(provider), ProviderType::kCustomExtensionProvider);
  std::optional<extensions::api::settings_private::PrefObject> pref_object =
      pref->GetPrefObject();
  EXPECT_EQ(pref_object->controlled_by, settings_api::ControlledBy::kExtension);
  EXPECT_EQ(pref_object->enforcement, settings_api::Enforcement::kEnforced);

  provider = std::make_unique<content_settings::MockProvider>();
  provider->SetWebsiteSetting(
      ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
      ContentSettingsType::COOKIES, base::Value(CONTENT_SETTING_ALLOW),
      /*constraints=*/{},
      content_settings::PartitionKey::GetDefaultForTesting());
  content_settings::TestUtils::OverrideProvider(
      map, std::move(provider), ProviderType::kSupervisedProvider);
  pref_object = pref->GetPrefObject();
  EXPECT_EQ(pref_object->controlled_by,
            settings_api::ControlledBy::kChildRestriction);
  EXPECT_EQ(pref_object->enforcement, settings_api::Enforcement::kEnforced);

  provider = std::make_unique<content_settings::MockProvider>();
  provider->SetWebsiteSetting(
      ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
      ContentSettingsType::COOKIES, base::Value(CONTENT_SETTING_ALLOW),
      /*constraints=*/{},
      content_settings::PartitionKey::GetDefaultForTesting());
  content_settings::TestUtils::OverrideProvider(map, std::move(provider),
                                                ProviderType::kPolicyProvider);
  pref_object = pref->GetPrefObject();
  EXPECT_EQ(pref_object->controlled_by,
            settings_api::ControlledBy::kDevicePolicy);
  EXPECT_EQ(pref_object->enforcement, settings_api::Enforcement::kEnforced);

  // Ensure the preference cannot be changed when it is enforced.
  EXPECT_EQ(pref->SetPref(std::make_unique<base::Value>("block").get()),
            extensions::settings_private::SetPrefResult::PREF_NOT_MODIFIABLE);
  EXPECT_EQ(map->GetDefaultContentSetting(ContentSettingsType::COOKIES),
            CONTENT_SETTING_ALLOW);
}

typedef std::tuple<ThirdPartyCookieBlockingSetting, CookieControlsMode>
    CookieSettingPair;

class GeneratedThirdPartyCookieBlockingSettingSetPrefTest
    : public GeneratedCookiePrefsTest,
      public testing::WithParamInterface<CookieSettingPair> {
 public:
  GeneratedThirdPartyCookieBlockingSettingSetPrefTest() = default;
  ~GeneratedThirdPartyCookieBlockingSettingSetPrefTest() override = default;
};

TEST_P(GeneratedThirdPartyCookieBlockingSettingSetPrefTest,
       SetPrefSucceedsWithValidPrefValue) {
  auto pref = std::make_unique<
      content_settings::GeneratedThirdPartyCookieBlockingSettingPref>(
      profile());

  auto [third_party_blocking_setting, cookie_controls_mode] = GetParam();
  EXPECT_EQ(pref->SetPref(std::make_unique<base::Value>(
                              static_cast<int>(third_party_blocking_setting))
                              .get()),
            extensions::settings_private::SetPrefResult::SUCCESS);
  EXPECT_EQ(static_cast<CookieControlsMode>(
                prefs()->GetUserPref(prefs::kCookieControlsMode)->GetInt()),
            cookie_controls_mode);
}

INSTANTIATE_TEST_SUITE_P(
    Validation,
    GeneratedThirdPartyCookieBlockingSettingSetPrefTest,
    testing::Values(
        CookieSettingPair(ThirdPartyCookieBlockingSetting::INCOGNITO_ONLY,
                          CookieControlsMode::kIncognitoOnly),
        CookieSettingPair(ThirdPartyCookieBlockingSetting::BLOCK_THIRD_PARTY,
                          CookieControlsMode::kBlockThirdParty)));

using GeneratedThirdPartyCookieBlockingSettingPrefTest =
    GeneratedCookiePrefsTest;

TEST_F(GeneratedThirdPartyCookieBlockingSettingPrefTest,
       SetPrefFailsWithTypeMismatchForInvalidInteger) {
  auto pref = std::make_unique<
      content_settings::GeneratedThirdPartyCookieBlockingSettingPref>(
      profile());
  EXPECT_EQ(pref->SetPref(std::make_unique<base::Value>(123).get()),
            extensions::settings_private::SetPrefResult::PREF_TYPE_MISMATCH);
}

TEST_F(GeneratedThirdPartyCookieBlockingSettingPrefTest,
       SetPrefFailsWithTypeMismatchForNonIntegerType) {
  auto pref = std::make_unique<
      content_settings::GeneratedThirdPartyCookieBlockingSettingPref>(
      profile());
  EXPECT_EQ(
      pref->SetPref(std::make_unique<base::Value>("default string").get()),
      extensions::settings_private::SetPrefResult::PREF_TYPE_MISMATCH);
}

class GeneratedThirdPartyCookieBlockingSettingGetPrefTest
    : public GeneratedCookiePrefsTest,
      public testing::WithParamInterface<CookieSettingPair> {
 public:
  GeneratedThirdPartyCookieBlockingSettingGetPrefTest() = default;
  ~GeneratedThirdPartyCookieBlockingSettingGetPrefTest() override = default;
};

TEST_P(GeneratedThirdPartyCookieBlockingSettingGetPrefTest,
       GetPrefSucceedsForAllValuesOfCookieControlsMode) {
  auto pref = std::make_unique<
      content_settings::GeneratedThirdPartyCookieBlockingSettingPref>(
      profile());

  auto [third_party_blocking_setting, cookie_controls_mode] = GetParam();
  prefs()->SetUserPref(prefs::kCookieControlsMode,
                       base::Value(static_cast<int>(cookie_controls_mode)));
  EXPECT_EQ(static_cast<ThirdPartyCookieBlockingSetting>(
                pref->GetPrefObject().value->GetInt()),
            third_party_blocking_setting);
}

INSTANTIATE_TEST_SUITE_P(
    Validation,
    GeneratedThirdPartyCookieBlockingSettingGetPrefTest,
    testing::Values(
        CookieSettingPair(ThirdPartyCookieBlockingSetting::INCOGNITO_ONLY,
                          CookieControlsMode::kIncognitoOnly),
        CookieSettingPair(ThirdPartyCookieBlockingSetting::INCOGNITO_ONLY,
                          CookieControlsMode::kOff),
        CookieSettingPair(ThirdPartyCookieBlockingSetting::INCOGNITO_ONLY,
                          CookieControlsMode::kLimited),
        CookieSettingPair(ThirdPartyCookieBlockingSetting::BLOCK_THIRD_PARTY,
                          CookieControlsMode::kBlockThirdParty)));

}  // namespace
}  // namespace content_settings
