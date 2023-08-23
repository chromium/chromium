// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/generated_notification_pref.h"

#include "base/ranges/algorithm.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/extensions/api/settings_private/generated_pref_test_base.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/content_settings/core/test/content_settings_mock_provider.h"
#include "components/content_settings/core/test/content_settings_test_utils.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace settings_api = extensions::api::settings_private;
namespace settings_private = extensions::settings_private;

namespace content_settings {

namespace {

// Sets the value of |generated_pref| to |pref_value| and then ensures
// that the notification content settings and preferences match
// |expected_content_setting| and |expected_quieter_ui|. The value of
// the new PrefObject returned by the |generated_pref| is then checked
// against |pref_value|.
void ValidateGeneratedPrefSetting(
    HostContentSettingsMap* map,
    sync_preferences::TestingPrefServiceSyncable* prefs,
    GeneratedNotificationPref* generated_pref,
    NotificationSetting pref_value,
    ContentSetting expected_content_setting,
    bool expected_quieter_ui) {
  EXPECT_EQ(
      generated_pref->SetPref(
          std::make_unique<base::Value>(static_cast<int>(pref_value)).get()),
      settings_private::SetPrefResult::SUCCESS);
  EXPECT_EQ(map->GetDefaultContentSetting(ContentSettingsType::NOTIFICATIONS),
            expected_content_setting);

  EXPECT_EQ(prefs->GetUserPref(prefs::kEnableQuietNotificationPermissionUi)
                ->GetBool(),
            expected_quieter_ui);
  EXPECT_EQ(static_cast<NotificationSetting>(
                generated_pref->GetPrefObject().value->GetInt()),
            pref_value);
}

const NotificationSetting kNoRecommendedValue =
    static_cast<NotificationSetting>(-1);

// Represents a set of settings, preferences and the associated expected
// fields for the returned preference object.
struct NotificationSettingManagedTestCase {
  ContentSetting default_content_setting;
  content_settings::SettingSource default_content_setting_source;
  settings_private::PrefSetting quieter_ui;
  settings_private::PrefSource quieter_ui_source;
  settings_api::ControlledBy expected_controlled_by;
  settings_api::Enforcement expected_enforcement;
  NotificationSetting expected_recommended_value;
  std::vector<NotificationSetting> expected_user_selectable_values;
};

static const std::vector<NotificationSettingManagedTestCase> managed_test_cases{
    {CONTENT_SETTING_ASK,
     SETTING_SOURCE_USER,
     settings_private::PrefSetting::kNotSet,
     settings_private::PrefSource::kNone,
     settings_api::ControlledBy::CONTROLLED_BY_NONE,
     settings_api::Enforcement::ENFORCEMENT_NONE,
     kNoRecommendedValue,
     {}},
    {CONTENT_SETTING_ASK,
     SETTING_SOURCE_EXTENSION,
     settings_private::PrefSetting::kNotSet,
     settings_private::PrefSource::kNone,
     settings_api::ControlledBy::CONTROLLED_BY_EXTENSION,
     settings_api::Enforcement::ENFORCEMENT_ENFORCED,
     kNoRecommendedValue,
     {NotificationSetting::ASK, NotificationSetting::QUIETER_MESSAGING}},
    {CONTENT_SETTING_ASK,
     SETTING_SOURCE_USER,
     settings_private::PrefSetting::kEnforcedOn,
     settings_private::PrefSource::kDevicePolicy,
     settings_api::ControlledBy::CONTROLLED_BY_DEVICE_POLICY,
     settings_api::Enforcement::ENFORCEMENT_ENFORCED,
     kNoRecommendedValue,
     {NotificationSetting::QUIETER_MESSAGING, NotificationSetting::BLOCK}},
    {CONTENT_SETTING_ASK,
     SETTING_SOURCE_USER,
     settings_private::PrefSetting::kEnforcedOff,
     settings_private::PrefSource::kExtension,
     settings_api::ControlledBy::CONTROLLED_BY_EXTENSION,
     settings_api::Enforcement::ENFORCEMENT_ENFORCED,
     kNoRecommendedValue,
     {NotificationSetting::ASK, NotificationSetting::BLOCK}},
    {CONTENT_SETTING_ASK,
     SETTING_SOURCE_POLICY,
     settings_private::PrefSetting::kEnforcedOn,
     settings_private::PrefSource::kDevicePolicy,
     settings_api::ControlledBy::CONTROLLED_BY_DEVICE_POLICY,
     settings_api::Enforcement::ENFORCEMENT_ENFORCED,
     kNoRecommendedValue,
     {}},
    {CONTENT_SETTING_ASK,
     SETTING_SOURCE_SUPERVISED,
     settings_private::PrefSetting::kRecommendedOn,
     settings_private::PrefSource::kRecommended,
     settings_api::ControlledBy::CONTROLLED_BY_CHILD_RESTRICTION,
     settings_api::Enforcement::ENFORCEMENT_ENFORCED,
     NotificationSetting::QUIETER_MESSAGING,
     {NotificationSetting::ASK, NotificationSetting::QUIETER_MESSAGING}},
    {CONTENT_SETTING_ASK,
     SETTING_SOURCE_EXTENSION,
     settings_private::PrefSetting::kRecommendedOff,
     settings_private::PrefSource::kRecommended,
     settings_api::ControlledBy::CONTROLLED_BY_EXTENSION,
     settings_api::Enforcement::ENFORCEMENT_ENFORCED,
     NotificationSetting::ASK,
     {NotificationSetting::ASK, NotificationSetting::QUIETER_MESSAGING}},
    {CONTENT_SETTING_BLOCK,
     SETTING_SOURCE_EXTENSION,
     settings_private::PrefSetting::kRecommendedOn,
     settings_private::PrefSource::kRecommended,
     settings_api::ControlledBy::CONTROLLED_BY_EXTENSION,
     settings_api::Enforcement::ENFORCEMENT_ENFORCED,
     kNoRecommendedValue,
     {}},
    {CONTENT_SETTING_BLOCK,
     SETTING_SOURCE_EXTENSION,
     settings_private::PrefSetting::kRecommendedOff,
     settings_private::PrefSource::kRecommended,
     settings_api::ControlledBy::CONTROLLED_BY_EXTENSION,
     settings_api::Enforcement::ENFORCEMENT_ENFORCED,
     kNoRecommendedValue,
     {}},
    {CONTENT_SETTING_BLOCK,
     SETTING_SOURCE_EXTENSION,
     settings_private::PrefSetting::kEnforcedOn,
     settings_private::PrefSource::kRecommended,
     settings_api::ControlledBy::CONTROLLED_BY_EXTENSION,
     settings_api::Enforcement::ENFORCEMENT_ENFORCED,
     kNoRecommendedValue,
     {}},
    {CONTENT_SETTING_BLOCK,
     SETTING_SOURCE_EXTENSION,
     settings_private::PrefSetting::kEnforcedOff,
     settings_private::PrefSource::kRecommended,
     settings_api::ControlledBy::CONTROLLED_BY_EXTENSION,
     settings_api::Enforcement::ENFORCEMENT_ENFORCED,
     kNoRecommendedValue,
     {}},
    {CONTENT_SETTING_BLOCK,
     SETTING_SOURCE_EXTENSION,
     settings_private::PrefSetting::kNotSet,
     settings_private::PrefSource::kRecommended,
     settings_api::ControlledBy::CONTROLLED_BY_EXTENSION,
     settings_api::Enforcement::ENFORCEMENT_ENFORCED,
     kNoRecommendedValue,
     {}},
};

void SetupManagedTestConditions(
    HostContentSettingsMap* map,
    sync_preferences::TestingPrefServiceSyncable* prefs,
    const NotificationSettingManagedTestCase& test_case) {
  auto provider = std::make_unique<content_settings::MockProvider>();
  provider->SetWebsiteSetting(ContentSettingsPattern::Wildcard(),
                              ContentSettingsPattern::Wildcard(),
                              ContentSettingsType::NOTIFICATIONS,
                              base::Value(test_case.default_content_setting));
  HostContentSettingsMap::ProviderType provider_type;
  switch (test_case.default_content_setting_source) {
    case content_settings::SETTING_SOURCE_POLICY:
      provider_type = HostContentSettingsMap::POLICY_PROVIDER;
      break;
    case content_settings::SETTING_SOURCE_EXTENSION:
      provider_type = HostContentSettingsMap::CUSTOM_EXTENSION_PROVIDER;
      break;
    case content_settings::SETTING_SOURCE_SUPERVISED:
      provider_type = HostContentSettingsMap::SUPERVISED_PROVIDER;
      break;
    case content_settings::SETTING_SOURCE_USER:
    case content_settings::SETTING_SOURCE_NONE:
    case content_settings::SETTING_SOURCE_ALLOWLIST:
    case content_settings::SETTING_SOURCE_INSTALLED_WEBAPP:
      provider_type = HostContentSettingsMap::DEFAULT_PROVIDER;
  }
  content_settings::TestUtils::OverrideProvider(map, std::move(provider),
                                                provider_type);

  settings_private::SetPrefFromSource(
      prefs, prefs::kEnableQuietNotificationPermissionUi, test_case.quieter_ui,
      test_case.quieter_ui_source);
}

void ValidateManagedPreference(
    settings_api::PrefObject& pref,
    const NotificationSettingManagedTestCase& test_case) {
  if (test_case.expected_controlled_by !=
      settings_api::ControlledBy::CONTROLLED_BY_NONE) {
    EXPECT_EQ(pref.controlled_by, test_case.expected_controlled_by);
  }

  if (test_case.expected_enforcement !=
      settings_api::Enforcement::ENFORCEMENT_NONE) {
    EXPECT_EQ(pref.enforcement, test_case.expected_enforcement);
  }

  if (test_case.expected_recommended_value != kNoRecommendedValue) {
    EXPECT_EQ(
        static_cast<NotificationSetting>(pref.recommended_value->GetInt()),
        test_case.expected_recommended_value);
  }

  // Ensure user selectable values are as expected. Ordering is enforced here
  // despite not being required by the SettingsPrivate API.
  // First convert std::vector<std::unique_ptr<base::value(T)>> to
  // std::vector<T> for easier comparison.
  std::vector<NotificationSetting> pref_user_selectable_values;
  if (pref.user_selectable_values) {
    for (const auto& value : *pref.user_selectable_values) {
      pref_user_selectable_values.push_back(
          static_cast<NotificationSetting>(value.GetInt()));
    }
  }

  EXPECT_TRUE(base::ranges::equal(pref_user_selectable_values,
                                  test_case.expected_user_selectable_values));
}

}  // namespace

typedef settings_private::GeneratedPrefTestBase GeneratedNotificationPrefTest;

TEST_F(GeneratedNotificationPrefTest, UpdatePreference) {
  auto pref = std::make_unique<GeneratedNotificationPref>(profile());
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile());

  // Setup a baseline content setting and preference state.
  map->SetDefaultContentSetting(ContentSettingsType::NOTIFICATIONS,
                                ContentSetting::CONTENT_SETTING_ASK);
  prefs()->SetUserPref(prefs::kEnableQuietNotificationPermissionUi,
                       std::make_unique<base::Value>(false));

  // Check that each of the three possible preference values sets the correct
  // state and is correctly reflected in a newly returned PrefObject.
  ValidateGeneratedPrefSetting(map, prefs(), pref.get(),
                               NotificationSetting::BLOCK,
                               ContentSetting::CONTENT_SETTING_BLOCK, false);
  ValidateGeneratedPrefSetting(map, prefs(), pref.get(),
                               NotificationSetting::ASK,
                               ContentSetting::CONTENT_SETTING_ASK, false);
  ValidateGeneratedPrefSetting(map, prefs(), pref.get(),
                               NotificationSetting::QUIETER_MESSAGING,
                               ContentSetting::CONTENT_SETTING_ASK, true);
  // Setting notification content setting to
  // |ContentSetting::CONTENT_SETTING_BLOCK| should not change the quieter UI
  // pref.
  ValidateGeneratedPrefSetting(map, prefs(), pref.get(),
                               NotificationSetting::BLOCK,
                               ContentSetting::CONTENT_SETTING_BLOCK, true);
}

TEST_F(GeneratedNotificationPrefTest, UpdatePreferenceInvalidAction) {
  auto pref = std::make_unique<GeneratedNotificationPref>(profile());
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile());

  // Setup a baseline content setting and preference state.
  map->SetDefaultContentSetting(ContentSettingsType::NOTIFICATIONS,
                                ContentSetting::CONTENT_SETTING_BLOCK);
  prefs()->SetUserPref(prefs::kEnableQuietNotificationPermissionUi,
                       std::make_unique<base::Value>(false));

  // Confirm that a type mismatch is reported as such.
  EXPECT_EQ(pref->SetPref(std::make_unique<base::Value>(true).get()),
            settings_private::SetPrefResult::PREF_TYPE_MISMATCH);
  // Check a numerical value outside of the acceptable range.
  EXPECT_EQ(pref->SetPref(std::make_unique<base::Value>(
                              static_cast<int>(NotificationSetting::BLOCK) + 1)
                              .get()),
            settings_private::SetPrefResult::PREF_TYPE_MISMATCH);

  // Make quieter UI preference not user modifiable.
  settings_private::SetPrefFromSource(
      prefs(), prefs::kEnableQuietNotificationPermissionUi,
      settings_private::PrefSetting::kEnforcedOff,
      settings_private::PrefSource::kDevicePolicy);

  // Confirm that quieter UI preference isn't modified when it's enforced.
  EXPECT_EQ(pref->SetPref(
                std::make_unique<base::Value>(
                    static_cast<int>(NotificationSetting::QUIETER_MESSAGING))
                    .get()),
            settings_private::SetPrefResult::PREF_NOT_MODIFIABLE);

  // Confirm the neither value was modified.
  EXPECT_EQ(map->GetDefaultContentSetting(ContentSettingsType::NOTIFICATIONS),
            ContentSetting::CONTENT_SETTING_BLOCK);
  EXPECT_FALSE(prefs()
                   ->FindPreference(prefs::kEnableQuietNotificationPermissionUi)
                   ->GetValue()
                   ->GetBool());

  // Make notification content setting not user modifiable.
  auto provider = std::make_unique<content_settings::MockProvider>();
  provider->SetWebsiteSetting(ContentSettingsPattern::Wildcard(),
                              ContentSettingsPattern::Wildcard(),
                              ContentSettingsType::NOTIFICATIONS,
                              base::Value(ContentSetting::CONTENT_SETTING_ASK));

  content_settings::TestUtils::OverrideProvider(
      map, std::move(provider), HostContentSettingsMap::POLICY_PROVIDER);

  // Confirm that notification content setting isn't modified when it's
  // enforced.
  EXPECT_EQ(pref->SetPref(
                std::make_unique<base::Value>(
                    static_cast<int>(NotificationSetting::QUIETER_MESSAGING))
                    .get()),
            settings_private::SetPrefResult::PREF_NOT_MODIFIABLE);

  // Confirm the neither value was modified.
  EXPECT_EQ(map->GetDefaultContentSetting(ContentSettingsType::NOTIFICATIONS),
            ContentSetting::CONTENT_SETTING_ASK);
  EXPECT_FALSE(prefs()
                   ->FindPreference(prefs::kEnableQuietNotificationPermissionUi)
                   ->GetValue()
                   ->GetBool());

  // Confirm that notification content setting isn't modified when it's
  // enforced.
  EXPECT_EQ(pref->SetPref(std::make_unique<base::Value>(
                              static_cast<int>(NotificationSetting::BLOCK))
                              .get()),
            settings_private::SetPrefResult::PREF_NOT_MODIFIABLE);

  // Confirm the neither value was modified.
  EXPECT_EQ(map->GetDefaultContentSetting(ContentSettingsType::NOTIFICATIONS),
            ContentSetting::CONTENT_SETTING_ASK);
  EXPECT_FALSE(prefs()
                   ->FindPreference(prefs::kEnableQuietNotificationPermissionUi)
                   ->GetValue()
                   ->GetBool());
}

TEST_F(GeneratedNotificationPrefTest, NotifyPrefUpdates) {
  // Update source Notification preferences and ensure an observer is fired.
  auto pref = std::make_unique<GeneratedNotificationPref>(profile());

  settings_private::TestGeneratedPrefObserver test_observer;
  pref->AddObserver(&test_observer);

  prefs()->SetUserPref(prefs::kEnableQuietNotificationPermissionUi,
                       std::make_unique<base::Value>(true));
  EXPECT_EQ(test_observer.GetUpdatedPrefName(), kGeneratedNotificationPref);
  test_observer.Reset();

  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile());
  map->SetDefaultContentSetting(ContentSettingsType::NOTIFICATIONS,
                                ContentSetting::CONTENT_SETTING_BLOCK);

  EXPECT_EQ(test_observer.GetUpdatedPrefName(), kGeneratedNotificationPref);
}

TEST_F(GeneratedNotificationPrefTest, ManagedState) {
  for (const auto& test_case : managed_test_cases) {
    TestingProfile profile;
    HostContentSettingsMap* map =
        HostContentSettingsMapFactory::GetForProfile(&profile);

    testing::Message scope_message;
    scope_message << "Content Setting:" << test_case.default_content_setting
                  << " Quieter UI:" << static_cast<int>(test_case.quieter_ui);
    SCOPED_TRACE(scope_message);
    SetupManagedTestConditions(map, profile.GetTestingPrefService(), test_case);
    auto pref =
        std::make_unique<content_settings::GeneratedNotificationPref>(&profile);
    auto pref_object = pref->GetPrefObject();
    ValidateManagedPreference(pref_object, test_case);
  }
}

}  // namespace content_settings
