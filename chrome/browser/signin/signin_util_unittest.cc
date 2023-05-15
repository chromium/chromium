// Copyright 2017 The Chromium Authors
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
#include "content/public/test/browser_task_environment.h"

using signin_util::ProfileSeparationPolicyState;
using signin_util::ProfileSeparationPolicyStateSet;

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
TEST_F(SigninUtilTest, GetProfileSeparationPolicyState) {
  std::unique_ptr<TestingProfile> profile = TestingProfile::Builder().Build();

  // No policy set on the active profile.
  EXPECT_TRUE(
      signin_util::GetProfileSeparationPolicyState(profile.get()).Empty());
  EXPECT_TRUE(
      signin_util::GetProfileSeparationPolicyState(profile.get(), "none")
          .Empty());
  EXPECT_EQ(signin_util::GetProfileSeparationPolicyState(profile.get(),
                                                         "primary_account"),
            ProfileSeparationPolicyStateSet(
                {ProfileSeparationPolicyState::kEnforcedByInterceptedAccount}));
  EXPECT_EQ(signin_util::GetProfileSeparationPolicyState(
                profile.get(), "primary_account_keep_existing_data"),
            ProfileSeparationPolicyStateSet(
                {ProfileSeparationPolicyState::kEnforcedByInterceptedAccount,
                 ProfileSeparationPolicyState::kKeepsBrowsingData}));
  EXPECT_EQ(signin_util::GetProfileSeparationPolicyState(
                profile.get(), "primary_account_strict_keep_existing_data"),
            ProfileSeparationPolicyStateSet(
                {ProfileSeparationPolicyState::kEnforcedByInterceptedAccount,
                 ProfileSeparationPolicyState::kStrict,
                 ProfileSeparationPolicyState::kKeepsBrowsingData}));

  // Active profile has "none" as a user level policy.
  profile->GetPrefs()->SetString(prefs::kManagedAccountsSigninRestriction,
                                 "none");
  profile->GetPrefs()->SetBoolean(
      prefs::kManagedAccountsSigninRestrictionScopeMachine, false);
  EXPECT_TRUE(
      signin_util::GetProfileSeparationPolicyState(profile.get()).Empty());
  EXPECT_TRUE(
      signin_util::GetProfileSeparationPolicyState(profile.get(), "none")
          .Empty());
  EXPECT_EQ(signin_util::GetProfileSeparationPolicyState(profile.get(),
                                                         "primary_account"),
            ProfileSeparationPolicyStateSet(
                {ProfileSeparationPolicyState::kEnforcedByInterceptedAccount}));
  EXPECT_EQ(signin_util::GetProfileSeparationPolicyState(
                profile.get(), "primary_account_strict"),
            ProfileSeparationPolicyStateSet(
                {ProfileSeparationPolicyState::kStrict,
                 ProfileSeparationPolicyState::kEnforcedByInterceptedAccount}));
  EXPECT_EQ(signin_util::GetProfileSeparationPolicyState(
                profile.get(), "primary_account_keep_existing_data"),
            ProfileSeparationPolicyStateSet(
                {ProfileSeparationPolicyState::kEnforcedByInterceptedAccount,
                 ProfileSeparationPolicyState::kKeepsBrowsingData}));
  EXPECT_EQ(signin_util::GetProfileSeparationPolicyState(
                profile.get(), "primary_account_strict_keep_existing_data"),
            ProfileSeparationPolicyStateSet(
                {ProfileSeparationPolicyState::kEnforcedByInterceptedAccount,
                 ProfileSeparationPolicyState::kKeepsBrowsingData,
                 ProfileSeparationPolicyState::kStrict}));

  // Active profile has "none" as a machine level policy.
  profile->GetPrefs()->SetString(prefs::kManagedAccountsSigninRestriction,
                                 "none");
  profile->GetPrefs()->SetBoolean(
      prefs::kManagedAccountsSigninRestrictionScopeMachine, true);
  EXPECT_TRUE(
      signin_util::GetProfileSeparationPolicyState(profile.get()).Empty());
  EXPECT_TRUE(
      signin_util::GetProfileSeparationPolicyState(profile.get(), "none")
          .Empty());
  EXPECT_EQ(signin_util::GetProfileSeparationPolicyState(profile.get(),
                                                         "primary_account"),
            ProfileSeparationPolicyStateSet(
                {ProfileSeparationPolicyState::kEnforcedByInterceptedAccount}));
  EXPECT_EQ(signin_util::GetProfileSeparationPolicyState(
                profile.get(), "primary_account_strict"),
            ProfileSeparationPolicyStateSet(
                {ProfileSeparationPolicyState::kStrict,
                 ProfileSeparationPolicyState::kEnforcedByInterceptedAccount}));
  EXPECT_EQ(signin_util::GetProfileSeparationPolicyState(
                profile.get(), "primary_account_keep_existing_data"),
            ProfileSeparationPolicyStateSet(
                {ProfileSeparationPolicyState::kEnforcedByInterceptedAccount,
                 ProfileSeparationPolicyState::kKeepsBrowsingData}));
  EXPECT_EQ(signin_util::GetProfileSeparationPolicyState(
                profile.get(), "primary_account_strict_keep_existing_data"),
            ProfileSeparationPolicyStateSet(
                {ProfileSeparationPolicyState::kEnforcedByInterceptedAccount,
                 ProfileSeparationPolicyState::kKeepsBrowsingData,
                 ProfileSeparationPolicyState::kStrict}));

  // Active profile has "primary_account" as a user level policy.
  profile->GetPrefs()->SetString(prefs::kManagedAccountsSigninRestriction,
                                 "primary_account");
  profile->GetPrefs()->SetBoolean(
      prefs::kManagedAccountsSigninRestrictionScopeMachine, false);
  EXPECT_EQ(signin_util::GetProfileSeparationPolicyState(profile.get()),
            ProfileSeparationPolicyStateSet(
                {ProfileSeparationPolicyState::kEnforcedByExistingProfile}));
  EXPECT_EQ(signin_util::GetProfileSeparationPolicyState(profile.get(), "none"),
            ProfileSeparationPolicyStateSet(
                {ProfileSeparationPolicyState::kEnforcedByExistingProfile}));
  EXPECT_EQ(signin_util::GetProfileSeparationPolicyState(profile.get(),
                                                         "primary_account"),
            ProfileSeparationPolicyStateSet(
                {ProfileSeparationPolicyState::kEnforcedByExistingProfile,
                 ProfileSeparationPolicyState::kEnforcedByInterceptedAccount}));
  EXPECT_EQ(signin_util::GetProfileSeparationPolicyState(
                profile.get(), "primary_account_strict"),
            ProfileSeparationPolicyStateSet(
                {ProfileSeparationPolicyState::kEnforcedByExistingProfile,
                 ProfileSeparationPolicyState::kStrict,
                 ProfileSeparationPolicyState::kEnforcedByInterceptedAccount}));
  EXPECT_EQ(signin_util::GetProfileSeparationPolicyState(
                profile.get(), "primary_account_keep_existing_data"),
            ProfileSeparationPolicyStateSet(
                {ProfileSeparationPolicyState::kEnforcedByExistingProfile,
                 ProfileSeparationPolicyState::kEnforcedByInterceptedAccount}));
  EXPECT_EQ(signin_util::GetProfileSeparationPolicyState(
                profile.get(), "primary_account_strict_keep_existing_data"),
            ProfileSeparationPolicyStateSet(
                {ProfileSeparationPolicyState::kEnforcedByExistingProfile,
                 ProfileSeparationPolicyState::kEnforcedByInterceptedAccount,
                 ProfileSeparationPolicyState::kStrict}));

  // Active profile has "primary_account_strict" as a user level
  // policy.
  profile->GetPrefs()->SetString(prefs::kManagedAccountsSigninRestriction,
                                 "primary_account_strict");
  profile->GetPrefs()->SetBoolean(
      prefs::kManagedAccountsSigninRestrictionScopeMachine, false);
  EXPECT_EQ(signin_util::GetProfileSeparationPolicyState(profile.get()),
            ProfileSeparationPolicyStateSet(
                {ProfileSeparationPolicyState::kEnforcedByExistingProfile,
                 ProfileSeparationPolicyState::kStrict}));
  EXPECT_EQ(signin_util::GetProfileSeparationPolicyState(profile.get(), "none"),
            ProfileSeparationPolicyStateSet(
                {ProfileSeparationPolicyState::kEnforcedByExistingProfile,
                 ProfileSeparationPolicyState::kStrict}));
  EXPECT_EQ(signin_util::GetProfileSeparationPolicyState(profile.get(),
                                                         "primary_account"),
            ProfileSeparationPolicyStateSet(
                {ProfileSeparationPolicyState::kEnforcedByExistingProfile,
                 ProfileSeparationPolicyState::kEnforcedByInterceptedAccount,
                 ProfileSeparationPolicyState::kStrict}));
  EXPECT_EQ(signin_util::GetProfileSeparationPolicyState(
                profile.get(), "primary_account_strict"),
            ProfileSeparationPolicyStateSet(
                {ProfileSeparationPolicyState::kEnforcedByExistingProfile,
                 ProfileSeparationPolicyState::kEnforcedByInterceptedAccount,
                 ProfileSeparationPolicyState::kStrict}));
  EXPECT_EQ(signin_util::GetProfileSeparationPolicyState(
                profile.get(), "primary_account_keep_existing_data"),
            ProfileSeparationPolicyStateSet(
                {ProfileSeparationPolicyState::kEnforcedByExistingProfile,
                 ProfileSeparationPolicyState::kEnforcedByInterceptedAccount,
                 ProfileSeparationPolicyState::kStrict}));
  EXPECT_EQ(signin_util::GetProfileSeparationPolicyState(
                profile.get(), "primary_account_strict_keep_existing_data"),
            ProfileSeparationPolicyStateSet(
                {ProfileSeparationPolicyState::kEnforcedByExistingProfile,
                 ProfileSeparationPolicyState::kEnforcedByInterceptedAccount,
                 ProfileSeparationPolicyState::kStrict}));

  // Active profile has "primary_account" as a machine level policy.
  profile->GetPrefs()->SetString(prefs::kManagedAccountsSigninRestriction,
                                 "primary_account");
  profile->GetPrefs()->SetBoolean(
      prefs::kManagedAccountsSigninRestrictionScopeMachine, true);
  EXPECT_EQ(signin_util::GetProfileSeparationPolicyState(profile.get()),
            ProfileSeparationPolicyStateSet(
                {ProfileSeparationPolicyState::kEnforcedByExistingProfile,
                 ProfileSeparationPolicyState::kEnforcedOnMachineLevel}));
  EXPECT_EQ(signin_util::GetProfileSeparationPolicyState(profile.get(), "none"),
            ProfileSeparationPolicyStateSet(
                {ProfileSeparationPolicyState::kEnforcedByExistingProfile,
                 ProfileSeparationPolicyState::kEnforcedOnMachineLevel}));
  EXPECT_EQ(signin_util::GetProfileSeparationPolicyState(profile.get(),
                                                         "primary_account"),
            ProfileSeparationPolicyStateSet(
                {ProfileSeparationPolicyState::kEnforcedByExistingProfile,
                 ProfileSeparationPolicyState::kEnforcedOnMachineLevel,
                 ProfileSeparationPolicyState::kEnforcedByInterceptedAccount}));
  EXPECT_EQ(signin_util::GetProfileSeparationPolicyState(
                profile.get(), "primary_account_strict"),
            ProfileSeparationPolicyStateSet(
                {ProfileSeparationPolicyState::kEnforcedByExistingProfile,
                 ProfileSeparationPolicyState::kEnforcedOnMachineLevel,
                 ProfileSeparationPolicyState::kStrict,
                 ProfileSeparationPolicyState::kEnforcedByInterceptedAccount}));
  EXPECT_EQ(signin_util::GetProfileSeparationPolicyState(
                profile.get(), "primary_account_keep_existing_data"),
            ProfileSeparationPolicyStateSet(
                {ProfileSeparationPolicyState::kEnforcedByExistingProfile,
                 ProfileSeparationPolicyState::kEnforcedOnMachineLevel,
                 ProfileSeparationPolicyState::kEnforcedByInterceptedAccount}));
  EXPECT_EQ(signin_util::GetProfileSeparationPolicyState(
                profile.get(), "primary_account_strict_keep_existing_data"),
            ProfileSeparationPolicyStateSet(
                {ProfileSeparationPolicyState::kEnforcedByExistingProfile,
                 ProfileSeparationPolicyState::kEnforcedOnMachineLevel,
                 ProfileSeparationPolicyState::kEnforcedByInterceptedAccount,
                 ProfileSeparationPolicyState::kStrict}));

  // Active profile has "primary_account_keep_existing_data" as a
  // machine level policy.
  profile->GetPrefs()->SetString(prefs::kManagedAccountsSigninRestriction,
                                 "primary_account_keep_existing_data");
  profile->GetPrefs()->SetBoolean(
      prefs::kManagedAccountsSigninRestrictionScopeMachine, true);
  EXPECT_EQ(signin_util::GetProfileSeparationPolicyState(profile.get()),
            ProfileSeparationPolicyStateSet(
                {ProfileSeparationPolicyState::kEnforcedByExistingProfile,
                 ProfileSeparationPolicyState::kEnforcedOnMachineLevel,
                 ProfileSeparationPolicyState::kKeepsBrowsingData}));
  EXPECT_EQ(signin_util::GetProfileSeparationPolicyState(profile.get(), "none"),
            ProfileSeparationPolicyStateSet(
                {ProfileSeparationPolicyState::kEnforcedByExistingProfile,
                 ProfileSeparationPolicyState::kEnforcedOnMachineLevel,
                 ProfileSeparationPolicyState::kKeepsBrowsingData}));
  EXPECT_EQ(signin_util::GetProfileSeparationPolicyState(profile.get(),
                                                         "primary_account"),
            ProfileSeparationPolicyStateSet(
                {ProfileSeparationPolicyState::kEnforcedByExistingProfile,
                 ProfileSeparationPolicyState::kEnforcedOnMachineLevel,
                 ProfileSeparationPolicyState::kEnforcedByInterceptedAccount}));
  EXPECT_EQ(signin_util::GetProfileSeparationPolicyState(
                profile.get(), "primary_account_strict"),
            ProfileSeparationPolicyStateSet(
                {ProfileSeparationPolicyState::kEnforcedByExistingProfile,
                 ProfileSeparationPolicyState::kEnforcedOnMachineLevel,
                 ProfileSeparationPolicyState::kStrict,
                 ProfileSeparationPolicyState::kEnforcedByInterceptedAccount}));
  EXPECT_EQ(signin_util::GetProfileSeparationPolicyState(
                profile.get(), "primary_account_keep_existing_data"),
            ProfileSeparationPolicyStateSet(
                {ProfileSeparationPolicyState::kEnforcedByExistingProfile,
                 ProfileSeparationPolicyState::kEnforcedOnMachineLevel,
                 ProfileSeparationPolicyState::kEnforcedByInterceptedAccount,
                 ProfileSeparationPolicyState::kKeepsBrowsingData}));
  EXPECT_EQ(signin_util::GetProfileSeparationPolicyState(
                profile.get(), "primary_account_strict_keep_existing_data"),
            ProfileSeparationPolicyStateSet(
                {ProfileSeparationPolicyState::kEnforcedByExistingProfile,
                 ProfileSeparationPolicyState::kEnforcedOnMachineLevel,
                 ProfileSeparationPolicyState::kEnforcedByInterceptedAccount,
                 ProfileSeparationPolicyState::kStrict,
                 ProfileSeparationPolicyState::kKeepsBrowsingData}));

  // Active profile has "primary_account_strict" as a machine level
  // policy.
  profile->GetPrefs()->SetString(prefs::kManagedAccountsSigninRestriction,
                                 "primary_account_strict");
  profile->GetPrefs()->SetBoolean(
      prefs::kManagedAccountsSigninRestrictionScopeMachine, true);
  EXPECT_EQ(signin_util::GetProfileSeparationPolicyState(profile.get()),
            ProfileSeparationPolicyStateSet(
                {ProfileSeparationPolicyState::kEnforcedByExistingProfile,
                 ProfileSeparationPolicyState::kEnforcedOnMachineLevel,
                 ProfileSeparationPolicyState::kStrict}));
  EXPECT_EQ(signin_util::GetProfileSeparationPolicyState(profile.get(), "none"),
            ProfileSeparationPolicyStateSet(
                {ProfileSeparationPolicyState::kEnforcedByExistingProfile,
                 ProfileSeparationPolicyState::kEnforcedOnMachineLevel,
                 ProfileSeparationPolicyState::kStrict}));
  EXPECT_EQ(signin_util::GetProfileSeparationPolicyState(profile.get(),
                                                         "primary_account"),
            ProfileSeparationPolicyStateSet(
                {ProfileSeparationPolicyState::kEnforcedByExistingProfile,
                 ProfileSeparationPolicyState::kEnforcedOnMachineLevel,
                 ProfileSeparationPolicyState::kStrict,
                 ProfileSeparationPolicyState::kEnforcedByInterceptedAccount}));
  EXPECT_EQ(signin_util::GetProfileSeparationPolicyState(
                profile.get(), "primary_account_strict"),
            ProfileSeparationPolicyStateSet(
                {ProfileSeparationPolicyState::kEnforcedByExistingProfile,
                 ProfileSeparationPolicyState::kEnforcedOnMachineLevel,
                 ProfileSeparationPolicyState::kStrict,
                 ProfileSeparationPolicyState::kEnforcedByInterceptedAccount}));
  EXPECT_EQ(signin_util::GetProfileSeparationPolicyState(
                profile.get(), "primary_account_keep_existing_data"),
            ProfileSeparationPolicyStateSet(
                {ProfileSeparationPolicyState::kEnforcedByExistingProfile,
                 ProfileSeparationPolicyState::kEnforcedOnMachineLevel,
                 ProfileSeparationPolicyState::kStrict,
                 ProfileSeparationPolicyState::kEnforcedByInterceptedAccount}));
  EXPECT_EQ(signin_util::GetProfileSeparationPolicyState(
                profile.get(), "primary_account_strict_keep_existing_data"),
            ProfileSeparationPolicyStateSet(
                {ProfileSeparationPolicyState::kEnforcedByExistingProfile,
                 ProfileSeparationPolicyState::kEnforcedOnMachineLevel,
                 ProfileSeparationPolicyState::kEnforcedByInterceptedAccount,
                 ProfileSeparationPolicyState::kStrict}));

  // Active profile has "primary_account_strict_keep_existing_data"
  // as a machine level policy.
  profile->GetPrefs()->SetString(prefs::kManagedAccountsSigninRestriction,
                                 "primary_account_strict_keep_existing_data");
  profile->GetPrefs()->SetBoolean(
      prefs::kManagedAccountsSigninRestrictionScopeMachine, true);
  EXPECT_EQ(signin_util::GetProfileSeparationPolicyState(profile.get()),
            ProfileSeparationPolicyStateSet(
                {ProfileSeparationPolicyState::kEnforcedByExistingProfile,
                 ProfileSeparationPolicyState::kEnforcedOnMachineLevel,
                 ProfileSeparationPolicyState::kStrict,
                 ProfileSeparationPolicyState::kKeepsBrowsingData}));
  EXPECT_EQ(signin_util::GetProfileSeparationPolicyState(profile.get(), "none"),
            ProfileSeparationPolicyStateSet(
                {ProfileSeparationPolicyState::kEnforcedByExistingProfile,
                 ProfileSeparationPolicyState::kEnforcedOnMachineLevel,
                 ProfileSeparationPolicyState::kStrict,
                 ProfileSeparationPolicyState::kKeepsBrowsingData}));
  EXPECT_EQ(signin_util::GetProfileSeparationPolicyState(profile.get(),
                                                         "primary_account"),
            ProfileSeparationPolicyStateSet(
                {ProfileSeparationPolicyState::kEnforcedByExistingProfile,
                 ProfileSeparationPolicyState::kEnforcedOnMachineLevel,
                 ProfileSeparationPolicyState::kStrict,
                 ProfileSeparationPolicyState::kEnforcedByInterceptedAccount}));
  EXPECT_EQ(signin_util::GetProfileSeparationPolicyState(
                profile.get(), "primary_account_strict"),
            ProfileSeparationPolicyStateSet(
                {ProfileSeparationPolicyState::kEnforcedByExistingProfile,
                 ProfileSeparationPolicyState::kEnforcedOnMachineLevel,
                 ProfileSeparationPolicyState::kStrict,
                 ProfileSeparationPolicyState::kEnforcedByInterceptedAccount}));
  EXPECT_EQ(signin_util::GetProfileSeparationPolicyState(
                profile.get(), "primary_account_keep_existing_data"),
            ProfileSeparationPolicyStateSet(
                {ProfileSeparationPolicyState::kEnforcedByExistingProfile,
                 ProfileSeparationPolicyState::kEnforcedOnMachineLevel,
                 ProfileSeparationPolicyState::kStrict,
                 ProfileSeparationPolicyState::kEnforcedByInterceptedAccount,
                 ProfileSeparationPolicyState::kKeepsBrowsingData}));
  EXPECT_EQ(signin_util::GetProfileSeparationPolicyState(
                profile.get(), "primary_account_strict_keep_existing_data"),
            ProfileSeparationPolicyStateSet(
                {ProfileSeparationPolicyState::kEnforcedByExistingProfile,
                 ProfileSeparationPolicyState::kEnforcedOnMachineLevel,
                 ProfileSeparationPolicyState::kEnforcedByInterceptedAccount,
                 ProfileSeparationPolicyState::kStrict,
                 ProfileSeparationPolicyState::kKeepsBrowsingData}));
}

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

  // Active profile has "none" as a user level policy.
  profile->GetPrefs()->SetString(prefs::kManagedAccountsSigninRestriction,
                                 "none");
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

  // Active profile has "none" as a machine level policy.
  profile->GetPrefs()->SetString(prefs::kManagedAccountsSigninRestriction,
                                 "none");
  profile->GetPrefs()->SetBoolean(
      prefs::kManagedAccountsSigninRestrictionScopeMachine, true);
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
                                 "primary_account_strict");
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
