// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_profile_attributes_updater.h"

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
#if !BUILDFLAG(IS_CHROMEOS_ASH)
const char kEmail[] = "example@email.com";

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
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
}  // namespace

class SigninProfileAttributesUpdaterTest : public testing::Test {
 public:
  SigninProfileAttributesUpdaterTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  // Recreates |signin_profile_attributes_updater_|. Useful for tests that want
  // to set up the updater with specific preconditions.
  void RecreateSigninProfileAttributesUpdater() {
    signin_profile_attributes_updater_ =
        std::make_unique<SigninProfileAttributesUpdater>(
            identity_test_env_.identity_manager(),
            profile_manager_.profile_attributes_storage(), profile_->GetPath(),
            profile_->GetPrefs());
  }

  void SetUp() override {
    testing::Test::SetUp();

    ASSERT_TRUE(profile_manager_.SetUp());
    std::string name = "profile_name";
    profile_ = profile_manager_.CreateTestingProfile(
        name, /*prefs=*/nullptr, base::UTF8ToUTF16(name), 0,
        TestingProfile::TestingFactories());

    RecreateSigninProfileAttributesUpdater();
  }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  raw_ptr<TestingProfile> profile_;
  signin::IdentityTestEnvironment identity_test_env_;
  std::unique_ptr<SigninProfileAttributesUpdater>
      signin_profile_attributes_updater_;
};

#if !BUILDFLAG(IS_CHROMEOS_ASH)
// Tests that the browser state info is updated on signin and signout.
// ChromeOS does not support signout.
TEST_F(SigninProfileAttributesUpdaterTest, SigninSignout) {
  ProfileAttributesEntry* entry =
      profile_manager_.profile_attributes_storage()
          ->GetProfileAttributesWithPath(profile_->GetPath());
  ASSERT_NE(entry, nullptr);
  ASSERT_EQ(entry->GetSigninState(), SigninState::kNotSignedIn);
  EXPECT_FALSE(entry->IsSigninRequired());

  // Signin.
  identity_test_env_.MakePrimaryAccountAvailable(kEmail,
                                                 signin::ConsentLevel::kSync);
  EXPECT_TRUE(entry->IsAuthenticated());
  EXPECT_EQ(signin::GetTestGaiaIdForEmail(kEmail), entry->GetGAIAId());
  EXPECT_EQ(kEmail, base::UTF16ToUTF8(entry->GetUserName()));

  // Signout.
  identity_test_env_.ClearPrimaryAccount();
  EXPECT_EQ(entry->GetSigninState(), SigninState::kNotSignedIn);
  EXPECT_FALSE(entry->IsSigninRequired());
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

#if !BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(SigninProfileAttributesUpdaterTest, SigninSignoutResetsProfilePrefs) {
  PrefService* pref_service = profile_->GetPrefs();
  ProfileAttributesEntry* entry =
      profile_manager_.profile_attributes_storage()
          ->GetProfileAttributesWithPath(profile_->GetPath());
  ASSERT_NE(entry, nullptr);

  // Set profile prefs.
  CheckProfilePrefsReset(pref_service, true);
#if !BUILDFLAG(IS_ANDROID)
  SetProfilePrefs(pref_service);

  // Set UPA should reset profile prefs.
  AccountInfo account_info = identity_test_env_.MakePrimaryAccountAvailable(
      "email1@example.com", signin::ConsentLevel::kSignin);
  EXPECT_FALSE(entry->IsAuthenticated());
  CheckProfilePrefsReset(pref_service, false);
  SetProfilePrefs(pref_service);
  // Signout should reset profile prefs.
  identity_test_env_.ClearPrimaryAccount();
  CheckProfilePrefsReset(pref_service, false);
#endif  // !BUILDFLAG(IS_ANDROID)

  SetProfilePrefs(pref_service);
  // Set primary account should reset profile prefs.
  AccountInfo primary_account = identity_test_env_.MakePrimaryAccountAvailable(
      "primary@example.com", signin::ConsentLevel::kSync);
  CheckProfilePrefsReset(pref_service, false);
  SetProfilePrefs(pref_service);
  // Disabling sync should reset profile prefs.
  identity_test_env_.ClearPrimaryAccount();
  CheckProfilePrefsReset(pref_service, false);
}

#if !BUILDFLAG(IS_ANDROID)
TEST_F(SigninProfileAttributesUpdaterTest,
       EnablingSyncWithUPAAccountShouldNotResetProfilePrefs) {
  PrefService* pref_service = profile_->GetPrefs();
  ProfileAttributesEntry* entry =
      profile_manager_.profile_attributes_storage()
          ->GetProfileAttributesWithPath(profile_->GetPath());
  ASSERT_NE(entry, nullptr);
  // Set UPA.
  AccountInfo account_info = identity_test_env_.MakePrimaryAccountAvailable(
      "email1@example.com", signin::ConsentLevel::kSignin);
  EXPECT_FALSE(entry->IsAuthenticated());
  SetProfilePrefs(pref_service);
  // Set primary account to be the same as the UPA.
  // Given it is the same account, profile prefs should keep the same state.
  identity_test_env_.SetPrimaryAccount(account_info.email,
                                       signin::ConsentLevel::kSync);
  EXPECT_TRUE(entry->IsAuthenticated());
  CheckProfilePrefsSet(pref_service, false);
  identity_test_env_.ClearPrimaryAccount();
  CheckProfilePrefsReset(pref_service, false);
}

TEST_F(SigninProfileAttributesUpdaterTest,
       EnablingSyncWithDifferentAccountThanUPAResetsProfilePrefs) {
  PrefService* pref_service = profile_->GetPrefs();
  ProfileAttributesEntry* entry =
      profile_manager_.profile_attributes_storage()
          ->GetProfileAttributesWithPath(profile_->GetPath());
  ASSERT_NE(entry, nullptr);
  AccountInfo account_info = identity_test_env_.MakePrimaryAccountAvailable(
      "email1@example.com", signin::ConsentLevel::kSignin);
  EXPECT_FALSE(entry->IsAuthenticated());
  SetProfilePrefs(pref_service);
  // Set primary account to a different account than the UPA.
  AccountInfo primary_account = identity_test_env_.MakePrimaryAccountAvailable(
      "primary@example.com", signin::ConsentLevel::kSync);
  EXPECT_TRUE(entry->IsAuthenticated());
  CheckProfilePrefsReset(pref_service, false);
}
#endif  // !BUILDFLAG(IS_ANDROID)

class SigninProfileAttributesUpdaterWithForceSigninTest
    : public SigninProfileAttributesUpdaterTest {
 public:
  SigninProfileAttributesUpdaterWithForceSigninTest()
      : forced_signin_setter_(true) {}

 private:
  signin_util::ScopedForceSigninSetterForTesting forced_signin_setter_;
};

TEST_F(SigninProfileAttributesUpdaterWithForceSigninTest, IsSigninRequired) {
  ProfileAttributesEntry* entry =
      profile_manager_.profile_attributes_storage()
          ->GetProfileAttributesWithPath(profile_->GetPath());
  ASSERT_NE(entry, nullptr);
  EXPECT_FALSE(entry->IsAuthenticated());
  EXPECT_TRUE(entry->IsSigninRequired());

  AccountInfo account_info = identity_test_env_.MakePrimaryAccountAvailable(
      kEmail, signin::ConsentLevel::kSync);

  EXPECT_TRUE(entry->IsAuthenticated());
  EXPECT_EQ(signin::GetTestGaiaIdForEmail(kEmail), entry->GetGAIAId());
  EXPECT_EQ(kEmail, base::UTF16ToUTF8(entry->GetUserName()));

  identity_test_env_.ClearPrimaryAccount();
  EXPECT_EQ(entry->GetSigninState(), SigninState::kNotSignedIn);
  EXPECT_TRUE(entry->IsSigninRequired());
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
