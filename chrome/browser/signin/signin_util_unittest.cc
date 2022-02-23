// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_util.h"

#include <memory>

#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"

class SigninUtilTest : public BrowserWithTestWindowTest {
 public:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    signin_util::ResetForceSigninForTesting();
  }

  void TearDown() override {
    signin_util::ResetForceSigninForTesting();
    BrowserWithTestWindowTest::TearDown();
  }
};

TEST_F(SigninUtilTest, GetForceSigninPolicy) {
  EXPECT_FALSE(signin_util::IsForceSigninEnabled());

  g_browser_process->local_state()->SetBoolean(prefs::kForceBrowserSignin,
                                               true);
  signin_util::ResetForceSigninForTesting();
  EXPECT_TRUE(signin_util::IsForceSigninEnabled());
  g_browser_process->local_state()->SetBoolean(prefs::kForceBrowserSignin,
                                               false);
  signin_util::ResetForceSigninForTesting();
  EXPECT_FALSE(signin_util::IsForceSigninEnabled());
}

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
TEST_F(SigninUtilTest, ProfileSeparationEnforcedByPolicy) {
  std::unique_ptr<TestingProfile> profile = TestingProfile::Builder().Build();

  // No policy set on the active profile.
  EXPECT_FALSE(signin_util::ProfileSeparationEnforcedByPolicy(profile.get(),
                                                              std::string()));
  EXPECT_FALSE(
      signin_util::ProfileSeparationEnforcedByPolicy(profile.get(), "none"));
  EXPECT_TRUE(signin_util::ProfileSeparationEnforcedByPolicy(
      profile.get(), "primary_account"));
  EXPECT_TRUE(signin_util::ProfileSeparationEnforcedByPolicy(
      profile.get(), "primary_account_strict"));

  // Active profile has "primary_account" as a user level policy.
  profile->GetPrefs()->SetString(prefs::kManagedAccountsSigninRestriction,
                                 "primary_account");
  profile->GetPrefs()->SetBoolean(
      prefs::kManagedAccountsSigninRestrictionScopeMachine, false);
  EXPECT_FALSE(signin_util::ProfileSeparationEnforcedByPolicy(profile.get(),
                                                              std::string()));
  EXPECT_FALSE(
      signin_util::ProfileSeparationEnforcedByPolicy(profile.get(), "none"));
  EXPECT_TRUE(signin_util::ProfileSeparationEnforcedByPolicy(
      profile.get(), "primary_account"));
  EXPECT_TRUE(signin_util::ProfileSeparationEnforcedByPolicy(
      profile.get(), "primary_account_strict"));

  // Active profile has "primary_account_strict" as a user level policy.
  profile->GetPrefs()->SetString(prefs::kManagedAccountsSigninRestriction,
                                 "primary_account_strict");
  profile->GetPrefs()->SetBoolean(
      prefs::kManagedAccountsSigninRestrictionScopeMachine, false);
  EXPECT_TRUE(signin_util::ProfileSeparationEnforcedByPolicy(profile.get(),
                                                             std::string()));
  EXPECT_TRUE(
      signin_util::ProfileSeparationEnforcedByPolicy(profile.get(), "none"));
  EXPECT_TRUE(signin_util::ProfileSeparationEnforcedByPolicy(
      profile.get(), "primary_account"));
  EXPECT_TRUE(signin_util::ProfileSeparationEnforcedByPolicy(
      profile.get(), "primary_account_strict"));

  // Active profile has "primary_account" as a machine level policy.
  profile->GetPrefs()->SetString(prefs::kManagedAccountsSigninRestriction,
                                 "primary_account");
  profile->GetPrefs()->SetBoolean(
      prefs::kManagedAccountsSigninRestrictionScopeMachine, true);
  EXPECT_TRUE(signin_util::ProfileSeparationEnforcedByPolicy(profile.get(),
                                                             std::string()));
  EXPECT_TRUE(
      signin_util::ProfileSeparationEnforcedByPolicy(profile.get(), "none"));
  EXPECT_TRUE(signin_util::ProfileSeparationEnforcedByPolicy(
      profile.get(), "primary_account"));
  EXPECT_TRUE(signin_util::ProfileSeparationEnforcedByPolicy(
      profile.get(), "primary_account_strict"));

  // Active profile has "primary_account_strict" as a machine level policy.
  profile->GetPrefs()->SetString(prefs::kManagedAccountsSigninRestriction,
                                 "primary_account");
  profile->GetPrefs()->SetBoolean(
      prefs::kManagedAccountsSigninRestrictionScopeMachine, true);
  EXPECT_TRUE(signin_util::ProfileSeparationEnforcedByPolicy(profile.get(),
                                                             std::string()));
  EXPECT_TRUE(
      signin_util::ProfileSeparationEnforcedByPolicy(profile.get(), "none"));
  EXPECT_TRUE(signin_util::ProfileSeparationEnforcedByPolicy(
      profile.get(), "primary_account"));
  EXPECT_TRUE(signin_util::ProfileSeparationEnforcedByPolicy(
      profile.get(), "primary_account_strict"));
}
#endif
