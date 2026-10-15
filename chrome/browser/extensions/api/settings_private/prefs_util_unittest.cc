// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/settings_private/prefs_util.h"

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/settings_private/prefs_util_enums.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/optimization_guide/core/feature_registry/feature_registration.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_pref_names.h"
#include "chrome/browser/ash/child_accounts/parent_access_code/parent_access_service.h"
#include "chrome/browser/ash/login/users/profile_user_manager_controller.h"
#include "chrome/browser/ash/login/users/scoped_account_id_annotator.h"
#include "components/account_id/account_id.h"
#include "components/session_manager/test/user_session_test_environment.h"
#include "components/user_manager/user_manager.h"
#include "google_apis/gaia/gaia_id.h"
#endif

namespace extensions {

class PrefsUtilTest : public testing::Test {
 public:
  PrefsUtilTest() : profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("test_profile");
    prefs_util_ = std::make_unique<PrefsUtil>(profile_);
  }

  void TearDown() override {
    prefs_util_.reset();
    profile_ = nullptr;
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  raw_ptr<TestingProfile> profile_;
  std::unique_ptr<PrefsUtil> prefs_util_;
};

TEST_F(PrefsUtilTest, GetAllowlistedPref) {
  profile_->GetPrefs()->SetBoolean(::prefs::kShowHomeButton, true);

  std::optional<api::settings_private::PrefObject> pref =
      prefs_util_->GetPref(::prefs::kShowHomeButton);
  ASSERT_TRUE(pref.has_value());
  EXPECT_EQ(pref->key, ::prefs::kShowHomeButton);
  EXPECT_EQ(pref->type, api::settings_private::PrefType::kBoolean);
  ASSERT_TRUE(pref->value);
  EXPECT_TRUE(pref->value->is_bool());
  EXPECT_TRUE(pref->value->GetBool());
}

TEST_F(PrefsUtilTest, SetAllowlistedPref) {
  base::Value value(true);
  EXPECT_EQ(prefs_util_->SetPref(::prefs::kShowHomeButton, &value),
            settings_private::SetPrefResult::SUCCESS);
  EXPECT_TRUE(profile_->GetPrefs()->GetBoolean(::prefs::kShowHomeButton));

  base::Value value2(false);
  EXPECT_EQ(prefs_util_->SetPref(::prefs::kShowHomeButton, &value2),
            settings_private::SetPrefResult::SUCCESS);
  EXPECT_FALSE(profile_->GetPrefs()->GetBoolean(::prefs::kShowHomeButton));
}

TEST_F(PrefsUtilTest, GetNonAllowlistedPref) {
  std::optional<api::settings_private::PrefObject> pref =
      prefs_util_->GetPref("non_allowlisted_pref");
  EXPECT_FALSE(pref.has_value());
}

TEST_F(PrefsUtilTest, SetNonAllowlistedPref) {
  base::Value value(true);
  EXPECT_EQ(prefs_util_->SetPref("non_allowlisted_pref", &value),
            settings_private::SetPrefResult::PREF_NOT_FOUND);
}

TEST_F(PrefsUtilTest, SetPrefTypeMismatch) {
  base::Value value("true");
  EXPECT_EQ(prefs_util_->SetPref(::prefs::kRestoreOnStartup, &value),
            settings_private::SetPrefResult::PREF_TYPE_MISMATCH);
}

// Regression test for http://crash/browse?q=reportid=%273b8cc7a091742d56%27.
TEST_F(PrefsUtilTest, SetPref_FixesUpValidUrl) {
  base::Value value("example.com");
  EXPECT_EQ(prefs_util_->SetPref(::prefs::kHomePage, &value),
            settings_private::SetPrefResult::SUCCESS);
  EXPECT_EQ(profile_->GetPrefs()->GetString(::prefs::kHomePage),
            "http://example.com/");
}

// Regression test for http://crash/browse?q=reportid=%273b8cc7a091742d56%27.
TEST_F(PrefsUtilTest, SetPref_FixesUpInvalidUrl) {
  base::Value value("http://%");  // Invalid URL
  EXPECT_EQ(prefs_util_->SetPref(::prefs::kHomePage, &value),
            settings_private::SetPrefResult::SUCCESS);
  EXPECT_EQ(profile_->GetPrefs()->GetString(::prefs::kHomePage), "");
}

TEST_F(PrefsUtilTest, GlicPrefsAllowlisted) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kGlic);
  glic::GlicEnabling::ScopedBypassEnablementChecksForTesting scoped_glic_bypass;

  std::optional<api::settings_private::PrefObject> pref =
      prefs_util_->GetPref(glic::prefs::kGlicHotkeyGlobalScopeEnabled);
  ASSERT_TRUE(pref.has_value());
  EXPECT_EQ(pref->key, glic::prefs::kGlicHotkeyGlobalScopeEnabled);
  EXPECT_EQ(pref->type, api::settings_private::PrefType::kBoolean);
}

TEST_F(PrefsUtilTest, FindAndFillWithGeminiSettingsReadOnly) {
  base::Value value;
  EXPECT_EQ(
      prefs_util_->SetPref(
          optimization_guide::prefs::kFindAndFillWithGeminiSettings, &value),
      settings_private::SetPrefResult::PREF_NOT_MODIFIABLE);
}

#if BUILDFLAG(IS_CHROMEOS)
TEST_F(PrefsUtilTest, ChildUserTimezoneRequiresParentApproval) {
  ash::test::UserSessionTestEnvironment user_session_test_environment(
      TestingBrowserProcess::GetGlobal()->local_state());
  ash::ProfileUserManagerController profile_user_manager_controller(
      profile_manager_.profile_manager(), user_manager::UserManager::Get());
  ash::parent_access::ParentAccessService parent_access_service(
      TestingBrowserProcess::GetGlobal()->local_state());

  const AccountId account_id =
      AccountId::FromUserEmailGaiaId("child@example.com", GaiaId("12345"));
  ASSERT_TRUE(user_session_test_environment.AddChildUser(account_id));
  user_session_test_environment.LogIn(account_id);

  ash::ScopedAccountIdAnnotator annotator(profile_manager_.profile_manager(),
                                          account_id);
  TestingProfile* child_profile = profile_manager_.CreateTestingProfile(
      std::string(account_id.GetUserEmail()));
  {
    PrefsUtil child_prefs_util(child_profile);

    base::Value timezone_1("America/Los_Angeles");
    EXPECT_EQ(child_prefs_util.SetPref(ash::prefs::kUserTimezone, &timezone_1),
              settings_private::SetPrefResult::PREF_NOT_MODIFIABLE);

    parent_access_service.SetTimezoneChangeApproved(account_id, true);
    EXPECT_EQ(child_prefs_util.SetPref(ash::prefs::kUserTimezone, &timezone_1),
              settings_private::SetPrefResult::SUCCESS);
    EXPECT_EQ(child_profile->GetPrefs()->GetString(ash::prefs::kUserTimezone),
              "America/Los_Angeles");

    parent_access_service.SetTimezoneChangeApproved(account_id, false);
    base::Value timezone_2("Asia/Tokyo");
    EXPECT_EQ(child_prefs_util.SetPref(ash::prefs::kUserTimezone, &timezone_2),
              settings_private::SetPrefResult::PREF_NOT_MODIFIABLE);
    EXPECT_EQ(child_profile->GetPrefs()->GetString(ash::prefs::kUserTimezone),
              "America/Los_Angeles");
  }

  profile_manager_.DeleteTestingProfile(std::string(account_id.GetUserEmail()));
}
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace extensions
