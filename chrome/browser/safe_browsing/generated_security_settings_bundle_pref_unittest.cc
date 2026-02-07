// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/generated_security_settings_bundle_pref.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/settings_private/generated_pref_test_base.h"
#include "chrome/browser/net/secure_dns_config.h"
#include "chrome/common/pref_names.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
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
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      safe_browsing::kBundledSecuritySettingsSecureDnsV2);

  // Validate that the generated Security Settings Bundle preference correctly
  // updates the base Security Settings Bundle preference.
  auto pref = std::make_unique<GeneratedSecuritySettingsBundlePref>(profile());
  PrefService* local_state = g_browser_process->local_state();

  // Setup baseline profile preference state.
  prefs()->SetDefaultPrefValue(
      prefs::kSecuritySettingsBundle,
      base::Value(static_cast<int>(SecuritySettingsBundleSetting::STANDARD)));

  // Check all possible settings both correctly update preferences and are
  // correctly returned by the generated preference, and verify Secure DNS
  // automatic-mode prefs are applied when the feature is enabled.
  ValidateGeneratedPrefSetting(prefs(), pref.get(),
                               SecuritySettingsBundleSetting::ENHANCED, 1);
  EXPECT_EQ(local_state->GetString(prefs::kDnsOverHttpsMode),
            SecureDnsConfig::kModeAutomatic);
  EXPECT_EQ(local_state->GetString(prefs::kDnsOverHttpsTemplates), "");
  EXPECT_TRUE(
      local_state->GetBoolean(prefs::kDnsOverHttpsAutomaticModeFallbackToDoh));
  ValidateGeneratedPrefSetting(prefs(), pref.get(),
                               SecuritySettingsBundleSetting::STANDARD, 0);
  EXPECT_EQ(local_state->GetString(prefs::kDnsOverHttpsMode),
            SecureDnsConfig::kModeAutomatic);
  EXPECT_EQ(local_state->GetString(prefs::kDnsOverHttpsTemplates), "");
  EXPECT_FALSE(
      local_state->GetBoolean(prefs::kDnsOverHttpsAutomaticModeFallbackToDoh));

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
