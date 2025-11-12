// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/generated_security_settings_bundle_pref.h"

#include <algorithm>

#include "chrome/browser/extensions/api/settings_private/generated_pref_test_base.h"
#include "chrome/test/base/testing_profile.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace settings_private = extensions::settings_private;

namespace safe_browsing {

namespace {

// Sets the value of |generated_pref| to |pref_value| and then checks Security
// Bundled Settings preference value in |prefs| match the provided expected
// values. After preference has been set, the value of |generated_pref| is
// checked to ensure it has correctly updated to |pref_value|.
void ValidateGeneratedPrefSetting(
    sync_preferences::TestingPrefServiceSyncable* prefs,
    GeneratedSecuritySettingsBundlePref* generated_pref,
    SecuritySettingsBundleSetting pref_value,
    int security_settings_bundle_level) {
  EXPECT_EQ(
      generated_pref->SetPref(
          std::make_unique<base::Value>(static_cast<int>(pref_value)).get()),
      settings_private::SetPrefResult::SUCCESS);
  EXPECT_EQ(prefs->GetUserPref(prefs::kSecuritySettingsBundle)->GetInt(),
            security_settings_bundle_level);
  EXPECT_EQ(static_cast<SecuritySettingsBundleSetting>(
                generated_pref->GetPrefObject().value->GetInt()),
            pref_value);
}

}  // namespace

typedef settings_private::GeneratedPrefTestBase
    GeneratedSecuritySettingsBundlePrefTest;

TEST_F(GeneratedSecuritySettingsBundlePrefTest, UpdatePreference) {
  // Validate that the generated Security Settings Bundle preference correctly
  // updates the base Security Settings Bundle preference.
  auto pref = std::make_unique<GeneratedSecuritySettingsBundlePref>(profile());

  // Setup baseline profile preference state.
  prefs()->SetDefaultPrefValue(
      prefs::kSecuritySettingsBundle,
      base::Value(static_cast<int>(SecuritySettingsBundleSetting::STANDARD)));

  // Check all possible settings both correctly update preferences and are
  // correctly returned by the generated preference.
  ValidateGeneratedPrefSetting(prefs(), pref.get(),
                               SecuritySettingsBundleSetting::ENHANCED, 1);
  ValidateGeneratedPrefSetting(prefs(), pref.get(),
                               SecuritySettingsBundleSetting::STANDARD, 0);

  // Confirm that a type mismatch is reported as such.
  EXPECT_EQ(pref->SetPref(std::make_unique<base::Value>(true).get()),
            settings_private::SetPrefResult::PREF_TYPE_MISMATCH);
  // Check a numerical value outside of the acceptable range.
  EXPECT_EQ(
      pref->SetPref(
          std::make_unique<base::Value>(
              static_cast<int>(SecuritySettingsBundleSetting::ENHANCED) + 1)
              .get()),
      settings_private::SetPrefResult::PREF_TYPE_MISMATCH);
}

TEST_F(GeneratedSecuritySettingsBundlePrefTest, NotifyPrefUpdates) {
  // Update source Security Settings Bundle preference and ensure an observer is
  // fired.
  auto pref = std::make_unique<GeneratedSecuritySettingsBundlePref>(profile());

  settings_private::TestGeneratedPrefObserver test_observer;
  pref->AddObserver(&test_observer);

  prefs()->SetUserPref(prefs::kSecuritySettingsBundle,
                       std::make_unique<base::Value>(static_cast<int>(
                           SecuritySettingsBundleSetting::ENHANCED)));
  EXPECT_EQ(test_observer.GetUpdatedPrefName(),
            kGeneratedSecuritySettingsBundlePref);
  test_observer.Reset();
}

}  // namespace safe_browsing
