// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/generated_safe_browsing_pref.h"

#include "base/ranges/algorithm.h"
#include "chrome/browser/extensions/api/settings_private/generated_pref_test_base.h"
#include "chrome/test/base/testing_profile.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace settings_api = extensions::api::settings_private;
namespace settings_private = extensions::settings_private;

namespace safe_browsing {

namespace {

// Sets the value of |generated_pref| to |pref_value| and then checks Safe
// Browsing preference values in |prefs| match the provided expected values.
// After preference has been set, the value of |generated_pref| is checked to
// ensure it has correctly updated to |pref_value|.
void ValidateGeneratedPrefSetting(
    sync_preferences::TestingPrefServiceSyncable* prefs,
    GeneratedSafeBrowsingPref* generated_pref,
    SafeBrowsingSetting pref_value,
    bool expected_safe_browsing_enabled,
    bool expected_safe_browsing_enhanced) {
  EXPECT_EQ(
      generated_pref->SetPref(
          std::make_unique<base::Value>(static_cast<int>(pref_value)).get()),
      settings_private::SetPrefResult::SUCCESS);
  EXPECT_EQ(prefs->GetUserPref(prefs::kSafeBrowsingEnabled)->GetBool(),
            expected_safe_browsing_enabled);
  EXPECT_EQ(prefs->GetUserPref(prefs::kSafeBrowsingEnhanced)->GetBool(),
            expected_safe_browsing_enhanced);
  EXPECT_EQ(static_cast<SafeBrowsingSetting>(
                generated_pref->GetPrefObject().value->GetInt()),
            pref_value);
}

// Define additional value for SafeBrowsingSetting to support testing a not
// set recommended value or not enforced value.
const SafeBrowsingSetting kNoRecommendedValue =
    static_cast<SafeBrowsingSetting>(-1);
const SafeBrowsingSetting kNoEnforcedValue =
    static_cast<SafeBrowsingSetting>(-2);

struct SafeBrowsingManagementTestCase {
  settings_private::PrefSetting safe_browsing_enabled;
  settings_private::PrefSetting safe_browsing_enhanced;
  settings_private::PrefSetting safe_browsing_reporting;
  settings_private::PrefSource enabled_enhanced_preference_source;
  settings_private::PrefSource reporting_preference_source;
  settings_api::ControlledBy expected_controlled_by;
  settings_api::Enforcement expected_enforcement;
  SafeBrowsingSetting expected_enforced_value;
  SafeBrowsingSetting expected_recommended_value;
  std::vector<SafeBrowsingSetting> expected_user_selectable_values;
};

const std::vector<SafeBrowsingManagementTestCase> kManagedTestCases = {
    {settings_private::PrefSetting::kNotSet,
     settings_private::PrefSetting::kNotSet,
     settings_private::PrefSetting::kNotSet,
     settings_private::PrefSource::kNone,
     settings_private::PrefSource::kNone,
     settings_api::ControlledBy::kNone,
     settings_api::Enforcement::kNone,
     kNoEnforcedValue,
     kNoRecommendedValue,
     {}},
    {settings_private::PrefSetting::kEnforcedOn,
     settings_private::PrefSetting::kEnforcedOn,
     settings_private::PrefSetting::kNotSet,
     settings_private::PrefSource::kExtension,
     settings_private::PrefSource::kNone,
     settings_api::ControlledBy::kExtension,
     settings_api::Enforcement::kEnforced,
     SafeBrowsingSetting::ENHANCED,
     kNoRecommendedValue,
     {}},
    {settings_private::PrefSetting::kEnforcedOff,
     settings_private::PrefSetting::kEnforcedOff,
     settings_private::PrefSetting::kNotSet,
     settings_private::PrefSource::kDevicePolicy,
     settings_private::PrefSource::kNone,
     settings_api::ControlledBy::kDevicePolicy,
     settings_api::Enforcement::kEnforced,
     SafeBrowsingSetting::DISABLED,
     kNoRecommendedValue,
     {}},
    {settings_private::PrefSetting::kEnforcedOn,
     settings_private::PrefSetting::kEnforcedOff,
     settings_private::PrefSetting::kNotSet,
     settings_private::PrefSource::kExtension,
     settings_private::PrefSource::kNone,
     settings_api::ControlledBy::kExtension,
     settings_api::Enforcement::kEnforced,
     SafeBrowsingSetting::STANDARD,
     kNoRecommendedValue,
     {}},
    {settings_private::PrefSetting::kRecommendedOn,
     settings_private::PrefSetting::kRecommendedOn,
     settings_private::PrefSetting::kNotSet,
     settings_private::PrefSource::kRecommended,
     settings_private::PrefSource::kNone,
     settings_api::ControlledBy::kNone,
     settings_api::Enforcement::kRecommended,
     kNoEnforcedValue,
     SafeBrowsingSetting::ENHANCED,
     {}},
    {settings_private::PrefSetting::kRecommendedOn,
     settings_private::PrefSetting::kRecommendedOff,
     settings_private::PrefSetting::kNotSet,
     settings_private::PrefSource::kRecommended,
     settings_private::PrefSource::kNone,
     settings_api::ControlledBy::kNone,
     settings_api::Enforcement::kRecommended,
     kNoEnforcedValue,
     SafeBrowsingSetting::STANDARD,
     {}},
    {settings_private::PrefSetting::kRecommendedOff,
     settings_private::PrefSetting::kRecommendedOff,
     settings_private::PrefSetting::kNotSet,
     settings_private::PrefSource::kRecommended,
     settings_private::PrefSource::kNone,
     settings_api::ControlledBy::kNone,
     settings_api::Enforcement::kRecommended,
     kNoEnforcedValue,
     SafeBrowsingSetting::DISABLED,
     {}},
    {settings_private::PrefSetting::kNotSet,
     settings_private::PrefSetting::kNotSet,
     settings_private::PrefSetting::kEnforcedOff,
     settings_private::PrefSource::kNone,
     settings_private::PrefSource::kDevicePolicy,
     settings_api::ControlledBy::kDevicePolicy,
     settings_api::Enforcement::kEnforced,
     kNoEnforcedValue,
     kNoRecommendedValue,
     {SafeBrowsingSetting::STANDARD, SafeBrowsingSetting::DISABLED}},
    {settings_private::PrefSetting::kRecommendedOff,
     settings_private::PrefSetting::kRecommendedOff,
     settings_private::PrefSetting::kEnforcedOff,
     settings_private::PrefSource::kRecommended,
     settings_private::PrefSource::kDevicePolicy,
     settings_api::ControlledBy::kDevicePolicy,
     settings_api::Enforcement::kEnforced,
     kNoEnforcedValue,
     SafeBrowsingSetting::DISABLED,
     {SafeBrowsingSetting::STANDARD, SafeBrowsingSetting::DISABLED}},
    {settings_private::PrefSetting::kRecommendedOn,
     settings_private::PrefSetting::kRecommendedOff,
     settings_private::PrefSetting::kEnforcedOff,
     settings_private::PrefSource::kRecommended,
     settings_private::PrefSource::kDevicePolicy,
     settings_api::ControlledBy::kDevicePolicy,
     settings_api::Enforcement::kEnforced,
     kNoEnforcedValue,
     SafeBrowsingSetting::STANDARD,
     {SafeBrowsingSetting::STANDARD, SafeBrowsingSetting::DISABLED}},
};

void SetupManagedTestConditions(
    sync_preferences::TestingPrefServiceSyncable* prefs,
    const SafeBrowsingManagementTestCase& test_case) {
  extensions::settings_private::SetPrefFromSource(
      prefs, prefs::kSafeBrowsingEnabled, test_case.safe_browsing_enabled,
      test_case.enabled_enhanced_preference_source);
  extensions::settings_private::SetPrefFromSource(
      prefs, prefs::kSafeBrowsingEnhanced, test_case.safe_browsing_enhanced,
      test_case.enabled_enhanced_preference_source);
  extensions::settings_private::SetPrefFromSource(
      prefs, prefs::kSafeBrowsingScoutReportingEnabled,
      test_case.safe_browsing_reporting, test_case.reporting_preference_source);
}

void ValidateManagedPreference(
    settings_api::PrefObject& pref,
    const SafeBrowsingManagementTestCase& test_case) {
  EXPECT_EQ(pref.controlled_by, test_case.expected_controlled_by);

  EXPECT_EQ(pref.enforcement, test_case.expected_enforcement);

  if (test_case.expected_enforced_value != kNoEnforcedValue) {
    EXPECT_EQ(static_cast<SafeBrowsingSetting>(pref.value->GetInt()),
              test_case.expected_enforced_value);
  }

  if (test_case.expected_recommended_value == kNoRecommendedValue) {
    EXPECT_FALSE(pref.recommended_value);
  } else {
    EXPECT_EQ(
        static_cast<SafeBrowsingSetting>(pref.recommended_value->GetInt()),
        test_case.expected_recommended_value);
  }

  // Ensure the user selectable values for the preference are correct.
  std::vector<SafeBrowsingSetting> pref_user_selectable_values;
  if (pref.user_selectable_values) {
    for (const auto& value : *pref.user_selectable_values) {
      pref_user_selectable_values.push_back(
          static_cast<SafeBrowsingSetting>(value.GetInt()));
    }
  }

  EXPECT_TRUE(base::ranges::equal(pref_user_selectable_values,
                                  test_case.expected_user_selectable_values));
}

}  // namespace

typedef settings_private::GeneratedPrefTestBase GeneratedSafeBrowsingPrefTest;

TEST_F(GeneratedSafeBrowsingPrefTest, UpdatePreference) {
  // Validate that the generated Safe Browsing preference correctly updates
  // the base Safe Browsing preferences.
  auto pref = std::make_unique<GeneratedSafeBrowsingPref>(profile());

  // Setup baseline profile preference state.
  prefs()->SetDefaultPrefValue(prefs::kSafeBrowsingEnabled, base::Value(false));
  prefs()->SetDefaultPrefValue(prefs::kSafeBrowsingEnhanced,
                               base::Value(false));

  // Check all possible settings both correctly update preferences and are
  // correctly returned by the generated preference.
  ValidateGeneratedPrefSetting(prefs(), pref.get(),
                               SafeBrowsingSetting::ENHANCED,
                               /* enabled */ true, /* enhanced */ true);
  ValidateGeneratedPrefSetting(prefs(), pref.get(),
                               SafeBrowsingSetting::STANDARD,
                               /* enabled */ true, /* enhanced */ false);
  ValidateGeneratedPrefSetting(prefs(), pref.get(),
                               SafeBrowsingSetting::DISABLED,
                               /* enabled */ false, /* enhanced */ false);

  // Confirm that a type mismatch is reported as such.
  EXPECT_EQ(pref->SetPref(std::make_unique<base::Value>(true).get()),
            settings_private::SetPrefResult::PREF_TYPE_MISMATCH);
  // Check a numerical value outside of the acceptable range.
  EXPECT_EQ(
      pref->SetPref(std::make_unique<base::Value>(
                        static_cast<int>(SafeBrowsingSetting::DISABLED) + 1)
                        .get()),
      settings_private::SetPrefResult::PREF_TYPE_MISMATCH);

  // Confirm when SBER is forcefully disabled, setting the preference to
  // enhanced is reported as invalid.
  prefs()->SetManagedPref(prefs::kSafeBrowsingScoutReportingEnabled,
                          std::make_unique<base::Value>(false));
  EXPECT_EQ(pref->SetPref(std::make_unique<base::Value>(
                              static_cast<int>(SafeBrowsingSetting::ENHANCED))
                              .get()),
            settings_private::SetPrefResult::PREF_NOT_MODIFIABLE);
}

TEST_F(GeneratedSafeBrowsingPrefTest, NotifyPrefUpdates) {
  // Update source Safe Browsing preferences and ensure an observer is fired.
  auto pref = std::make_unique<GeneratedSafeBrowsingPref>(profile());

  settings_private::TestGeneratedPrefObserver test_observer;
  pref->AddObserver(&test_observer);

  prefs()->SetUserPref(prefs::kSafeBrowsingEnabled,
                       std::make_unique<base::Value>(true));
  EXPECT_EQ(test_observer.GetUpdatedPrefName(), kGeneratedSafeBrowsingPref);
  test_observer.Reset();

  prefs()->SetUserPref(prefs::kSafeBrowsingEnhanced,
                       std::make_unique<base::Value>(true));
  EXPECT_EQ(test_observer.GetUpdatedPrefName(), kGeneratedSafeBrowsingPref);
  test_observer.Reset();

  prefs()->SetUserPref(prefs::kSafeBrowsingScoutReportingEnabled,
                       std::make_unique<base::Value>(true));
  EXPECT_EQ(test_observer.GetUpdatedPrefName(), kGeneratedSafeBrowsingPref);
  test_observer.Reset();
}

TEST_F(GeneratedSafeBrowsingPrefTest, ManagementState) {
  for (const auto& test_case : kManagedTestCases) {
    TestingProfile profile;

    testing::Message scope_message;
    scope_message << "Enabled:"
                  << static_cast<int>(test_case.safe_browsing_enabled)
                  << " Enhanced:"
                  << static_cast<int>(test_case.safe_browsing_enhanced)
                  << " Reporting:"
                  << static_cast<int>(test_case.safe_browsing_reporting);
    SCOPED_TRACE(scope_message);
    SetupManagedTestConditions(profile.GetTestingPrefService(), test_case);
    auto pref = std::make_unique<GeneratedSafeBrowsingPref>(&profile);
    auto pref_object = pref->GetPrefObject();
    ValidateManagedPreference(pref_object, test_case);
  }
}

}  // namespace safe_browsing
