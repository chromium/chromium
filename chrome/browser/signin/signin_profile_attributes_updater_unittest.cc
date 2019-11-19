// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_profile_attributes_updater.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/signin/core/browser/signin_error_controller.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const char kEmail[] = "example@email.com";

#if !defined(OS_CHROMEOS)
void CheckProfilePrefsReset(PrefService* pref_service,
                            bool expected_using_default_name) {
  EXPECT_TRUE(pref_service->GetBoolean(prefs::kProfileUsingDefaultAvatar));
  EXPECT_FALSE(pref_service->GetBoolean(prefs::kProfileUsingGAIAAvatar));
  EXPECT_EQ(expected_using_default_name,
            pref_service->GetBoolean(prefs::kProfileUsingDefaultName));
}

void CheckProfilePrefsSet(PrefService* pref_service,
                          bool expected_is_using_default_name) {
  EXPECT_FALSE(pref_service->GetBoolean(prefs::kProfileUsingDefaultAvatar));
  EXPECT_TRUE(pref_service->GetBoolean(prefs::kProfileUsingGAIAAvatar));
  EXPECT_EQ(expected_is_using_default_name,
            pref_service->GetBoolean(prefs::kProfileUsingDefaultName));
}

// Set the prefs to nondefault values.
void SetProfilePrefs(PrefService* pref_service) {
  pref_service->SetBoolean(prefs::kProfileUsingDefaultAvatar, false);
  pref_service->SetBoolean(prefs::kProfileUsingGAIAAvatar, true);
  pref_service->SetBoolean(prefs::kProfileUsingDefaultName, false);

  CheckProfilePrefsSet(pref_service, false);
}
#endif  // !defined(OS_CHROMEOS)
}  // namespace

class SigninProfileAttributesUpdaterTest : public testing::Test {
 public:
  SigninProfileAttributesUpdaterTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()),
        signin_error_controller_(
            SigninErrorController::AccountMode::PRIMARY_ACCOUNT,
            identity_test_env_.identity_manager()) {}

  // Recreates |signin_profile_attributes_updater_|. Useful for tests that want
  // to set up the updater with specific preconditions.
  void RecreateSigninProfileAttributesUpdater() {
    signin_profile_attributes_updater_ =
        std::make_unique<SigninProfileAttributesUpdater>(
            identity_test_env_.identity_manager(), &signin_error_controller_,
            profile_manager_.profile_attributes_storage(), profile_->GetPath(),
            profile_->GetPrefs());
  }

  void SetUp() override {
    testing::Test::SetUp();

    ASSERT_TRUE(profile_manager_.SetUp());
    std::string name = "profile_name";
    profile_ = profile_manager_.CreateTestingProfile(
        name, /*prefs=*/nullptr, base::UTF8ToUTF16(name), 0, std::string(),
        TestingProfile::TestingFactories());

    RecreateSigninProfileAttributesUpdater();
  }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  TestingProfile* profile_;
  signin::IdentityTestEnvironment identity_test_env_;
  SigninErrorController signin_error_controller_;
  std::unique_ptr<SigninProfileAttributesUpdater>
      signin_profile_attributes_updater_;
};

#if !defined(OS_CHROMEOS)
// Tests that the browser state info is updated on signin and signout.
// ChromeOS does not support signout.
TEST_F(SigninProfileAttributesUpdaterTest, SigninSignout) {
  ProfileAttributesEntry* entry;
  ASSERT_TRUE(profile_manager_.profile_attributes_storage()
                  ->GetProfileAttributesWithPath(profile_->GetPath(), &entry));
  ASSERT_EQ(entry->GetSigninState(), SigninState::kNotSignedIn);
  EXPECT_FALSE(entry->IsSigninRequired());

  // Signin.
  identity_test_env_.MakePrimaryAccountAvailable(kEmail);
  EXPECT_TRUE(entry->IsAuthenticated());
  EXPECT_EQ(signin::GetTestGaiaIdForEmail(kEmail), entry->GetGAIAId());
  EXPECT_EQ(kEmail, base::UTF16ToUTF8(entry->GetUserName()));

  // Signout.
  identity_test_env_.ClearPrimaryAccount();
  EXPECT_EQ(entry->GetSigninState(), SigninState::kNotSignedIn);
  EXPECT_FALSE(entry->IsSigninRequired());
}
#endif  // !defined(OS_CHROMEOS)

// Tests that the browser state info is updated on auth error change.
TEST_F(SigninProfileAttributesUpdaterTest, AuthError) {
  ProfileAttributesEntry* entry;
  ASSERT_TRUE(profile_manager_.profile_attributes_storage()
                  ->GetProfileAttributesWithPath(profile_->GetPath(), &entry));

  CoreAccountId account_id =
      identity_test_env_.MakePrimaryAccountAvailable(kEmail).account_id;

#if defined(OS_CHROMEOS)
  // ChromeOS only observes signin state at initial creation of the updater, so
  // recreate the updater after having set the primary account.
  RecreateSigninProfileAttributesUpdater();
#endif

  EXPECT_TRUE(entry->IsAuthenticated());
  EXPECT_FALSE(entry->IsAuthError());

  // Set auth error.
  identity_test_env_.UpdatePersistentErrorOfRefreshTokenForAccount(
      account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
  EXPECT_TRUE(entry->IsAuthError());

  // Remove auth error.
  identity_test_env_.UpdatePersistentErrorOfRefreshTokenForAccount(
      account_id, GoogleServiceAuthError::AuthErrorNone());
  EXPECT_FALSE(entry->IsAuthError());
}

#if !defined(OS_CHROMEOS)
class SigninProfileAttributesUpdaterTestWithParam
    : public SigninProfileAttributesUpdaterTest,
      public ::testing::WithParamInterface<bool> {
 public:
  SigninProfileAttributesUpdaterTestWithParam()
      : SigninProfileAttributesUpdaterTest() {
    concatenate_enabled_ = GetParam();
    if (concatenate_enabled_) {
      scoped_feature_list_.InitAndEnableFeature(features::kProfileMenuRevamp);
    } else {
      scoped_feature_list_.InitAndDisableFeature(features::kProfileMenuRevamp);
    }
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  bool concatenate_enabled_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SigninProfileAttributesUpdaterTestWithParam);
};

INSTANTIATE_TEST_SUITE_P(SigninProfileAttributesUpdaterTest,
                         SigninProfileAttributesUpdaterTestWithParam,
                         testing::Bool());

TEST_P(SigninProfileAttributesUpdaterTestWithParam,
       SigninSignoutResetsProfilePrefs) {
  PrefService* pref_service = profile_->GetPrefs();
  ProfileAttributesEntry* entry;
  ASSERT_TRUE(profile_manager_.profile_attributes_storage()
                  ->GetProfileAttributesWithPath(profile_->GetPath(), &entry));

  // Set profile prefs.
  CheckProfilePrefsReset(pref_service, true);
#if !defined(OS_ANDROID)
  SetProfilePrefs(pref_service);

  // Set UPA should reset profile prefs.
  AccountInfo account_info = identity_test_env_.MakeAccountAvailableWithCookies(
      "email1@example.com", "gaia_id_1");
  EXPECT_FALSE(entry->IsAuthenticated());
  // If concatenate is disabled, we reset kProfileIsUsingDefault to true on
  // sign in/ sync. Otherwise, we don't reset kProfileIsUsingDefault.
  CheckProfilePrefsReset(pref_service, !concatenate_enabled_);
  SetProfilePrefs(pref_service);
  // Signout should reset profile prefs.
  identity_test_env_.SetCookieAccounts({});
  CheckProfilePrefsReset(pref_service, false);
#endif  // !defined(OS_ANDROID)

  SetProfilePrefs(pref_service);
  // Set primary account should reset profile prefs.
  AccountInfo primary_account =
      identity_test_env_.MakePrimaryAccountAvailable("primary@example.com");
  CheckProfilePrefsReset(pref_service, !concatenate_enabled_);
  SetProfilePrefs(pref_service);
  // Disabling sync should reset profile prefs.
  identity_test_env_.ClearPrimaryAccount();
  CheckProfilePrefsReset(pref_service, false);
}

#if !defined(OS_ANDROID)
TEST_F(SigninProfileAttributesUpdaterTest,
       EnablingSyncWithUPAAccountShouldNotResetProfilePrefs) {
  PrefService* pref_service = profile_->GetPrefs();
  ProfileAttributesEntry* entry;
  ASSERT_TRUE(profile_manager_.profile_attributes_storage()
                  ->GetProfileAttributesWithPath(profile_->GetPath(), &entry));
  // Set UPA.
  AccountInfo account_info = identity_test_env_.MakeAccountAvailableWithCookies(
      "email1@example.com", "gaia_id_1");
  EXPECT_FALSE(entry->IsAuthenticated());
  SetProfilePrefs(pref_service);
  // Set primary account to be the same as the UPA.
  // Given it is the same account, profile prefs should keep the same state.
  identity_test_env_.SetPrimaryAccount(account_info.email);
  EXPECT_TRUE(entry->IsAuthenticated());
  CheckProfilePrefsSet(pref_service, false);
  identity_test_env_.ClearPrimaryAccount();
  CheckProfilePrefsReset(pref_service, false);
}

TEST_P(SigninProfileAttributesUpdaterTestWithParam,
       EnablingSyncWithDifferentAccountThanUPAResetsProfilePrefs) {
  PrefService* pref_service = profile_->GetPrefs();
  ProfileAttributesEntry* entry;
  ASSERT_TRUE(profile_manager_.profile_attributes_storage()
                  ->GetProfileAttributesWithPath(profile_->GetPath(), &entry));
  AccountInfo account_info = identity_test_env_.MakeAccountAvailableWithCookies(
      "email1@example.com", "gaia_id_1");
  EXPECT_FALSE(entry->IsAuthenticated());
  SetProfilePrefs(pref_service);
  // Set primary account to a different account than the UPA.
  AccountInfo primary_account =
      identity_test_env_.MakePrimaryAccountAvailable("primary@example.com");
  EXPECT_TRUE(entry->IsAuthenticated());
  CheckProfilePrefsReset(pref_service, !concatenate_enabled_);
}
#endif  // !defined(OS_ANDROID)

class SigninProfileAttributesUpdaterWithForceSigninTest
    : public SigninProfileAttributesUpdaterTest {
  void SetUp() override {
    signin_util::SetForceSigninForTesting(true);
    SigninProfileAttributesUpdaterTest::SetUp();
  }

  void TearDown() override {
    SigninProfileAttributesUpdaterTest::TearDown();
    signin_util::ResetForceSigninForTesting();
  }
};

TEST_F(SigninProfileAttributesUpdaterWithForceSigninTest, IsSigninRequired) {
  ProfileAttributesEntry* entry;
  ASSERT_TRUE(profile_manager_.profile_attributes_storage()
                  ->GetProfileAttributesWithPath(profile_->GetPath(), &entry));
  EXPECT_FALSE(entry->IsAuthenticated());
  EXPECT_TRUE(entry->IsSigninRequired());

  AccountInfo account_info =
      identity_test_env_.MakePrimaryAccountAvailable(kEmail);

  EXPECT_TRUE(entry->IsAuthenticated());
  EXPECT_EQ(signin::GetTestGaiaIdForEmail(kEmail), entry->GetGAIAId());
  EXPECT_EQ(kEmail, base::UTF16ToUTF8(entry->GetUserName()));

  identity_test_env_.ClearPrimaryAccount();
  EXPECT_EQ(entry->GetSigninState(), SigninState::kNotSignedIn);
  EXPECT_TRUE(entry->IsSigninRequired());
}
#endif  // !defined(OS_CHROMEOS)
