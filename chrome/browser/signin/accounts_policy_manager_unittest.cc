// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/accounts_policy_manager.h"

#include "base/memory/raw_ptr.h"
#include "build/buildflag.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/accounts_policy_manager_factory.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/policy/core/common/features.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
namespace {

const char kTestEmail[] = "me@gmail.com";
const char kTestEmail2[] = "me2@gmail.com";
const char kExampleEmail[] = "me@example.com";

}  // namespace
#endif  //  BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

class AccountsPolicyManagerTest : public testing::Test {
 public:
  AccountsPolicyManagerTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());
    CreateTestingProfile();
  }

  void TearDown() override {
    if (profile_) {
      DestroyProfile();
    }
  }

  ~AccountsPolicyManagerTest() override = default;

  void CreateTestingProfile() {
    DCHECK(!profile_);

    profile_ = profile_manager_.CreateTestingProfile(
        "accounts_policy_manager_test_profile_path",
        IdentityTestEnvironmentProfileAdaptor::
            GetIdentityTestEnvironmentFactories());
    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_);

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
    AccountsPolicyManagerFactory::GetForProfile(GetProfile())
        ->SetHideUIForTesting(true);
#endif
  }

  void DestroyProfile() {
    identity_test_env_adaptor_.reset();
    profile_ = nullptr;
    profile_manager_.DeleteTestingProfile(
        "accounts_policy_manager_test_profile_path");
  }

  PrefService* GetLocalState() { return profile_manager_.local_state()->Get(); }

  TestingProfile* GetProfile() {
    DCHECK(profile_);
    return profile_;
  }

  TestingProfileManager* GetProfileManager() { return &profile_manager_; }

  signin::IdentityTestEnvironment* GetIdentityTestEnv() {
    DCHECK(identity_test_env_adaptor_);
    return identity_test_env_adaptor_->identity_test_env();
  }

  SigninClient* GetSigninSlient(Profile* profile) {
    return ChromeSigninClientFactory::GetForProfile(profile);
  }

  signin::IdentityManager* identity_manager() {
    return GetIdentityTestEnv()->identity_manager();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  raw_ptr<TestingProfile, DanglingUntriaged> profile_ = nullptr;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
};

#if !BUILDFLAG(IS_CHROMEOS)
// All primary accounts are allowed on ChromeOS and Lacros, so this the
// AccountsPolicyManagerTest does not clear the primary account on
// ChromeOS.
//
// TODO(msarda): Exclude |AccountsPolicyManager| from the ChromeOS
// build.
//
// TODO(msarda): These tests are valid for secondary profiles on Lacros. Enable
// them on Lacros.
TEST_F(AccountsPolicyManagerTest, ClearPrimarySyncAccountWhenSigninNotAllowed) {
  GetIdentityTestEnv()->MakePrimaryAccountAvailable(
      "test@foo.com", signin::ConsentLevel::kSync);
  GetProfile()->GetPrefs()->SetBoolean(prefs::kSigninAllowed, false);

  EXPECT_FALSE(GetIdentityTestEnv()->identity_manager()->HasPrimaryAccount(
      signin::ConsentLevel::kSync));
  EXPECT_FALSE(GetIdentityTestEnv()->identity_manager()->HasPrimaryAccount(
      signin::ConsentLevel::kSignin));
}

TEST_F(AccountsPolicyManagerTest,
       ClearPrimarySyncAccountWhenPatternNotAllowed) {
  GetIdentityTestEnv()->MakePrimaryAccountAvailable(
      "test@foo.com", signin::ConsentLevel::kSync);
  GetLocalState()->SetString(prefs::kGoogleServicesUsernamePattern,
                             ".*@bar.com");

  EXPECT_FALSE(GetIdentityTestEnv()->identity_manager()->HasPrimaryAccount(
      signin::ConsentLevel::kSync));
  EXPECT_FALSE(GetIdentityTestEnv()->identity_manager()->HasPrimaryAccount(
      signin::ConsentLevel::kSignin));
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
TEST_F(AccountsPolicyManagerTest, ClearProfileWhenSigninAndSignoutNotAllowed) {
  GetIdentityTestEnv()->MakePrimaryAccountAvailable(
      "test@foo.com", signin::ConsentLevel::kSync);

  // Create a second profile.
  GetProfileManager()->CreateTestingProfile(
      "accounts_policy_manager_test_profile_path_1",
      IdentityTestEnvironmentProfileAdaptor::
          GetIdentityTestEnvironmentFactories());
  ASSERT_EQ(2u, GetProfileManager()->profile_manager()->GetNumberOfProfiles());

  // Disable sign out and sign in. This should result in the initial profile
  // being deleted.
  GetSigninSlient(GetProfile())
      ->set_is_clear_primary_account_allowed_for_testing(
          SigninClient::SignoutDecision::CLEAR_PRIMARY_ACCOUNT_DISALLOWED);
  GetProfile()->GetPrefs()->SetBoolean(prefs::kSigninAllowed, false);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1u, GetProfileManager()->profile_manager()->GetNumberOfProfiles());
}

TEST_F(AccountsPolicyManagerTest,
       ClearProfileWhenSigninAndRevokeSyncNotAllowed) {
  GetIdentityTestEnv()->MakePrimaryAccountAvailable(
      "test@foo.com", signin::ConsentLevel::kSync);

  // Create a second profile.
  GetProfileManager()->CreateTestingProfile(
      "accounts_policy_manager_test_profile_path_1",
      IdentityTestEnvironmentProfileAdaptor::
          GetIdentityTestEnvironmentFactories());
  ASSERT_EQ(2u, GetProfileManager()->profile_manager()->GetNumberOfProfiles());

  // Disable sign out and sign in. This should result in the initial profile
  // being deleted.
  GetSigninSlient(GetProfile())
      ->set_is_clear_primary_account_allowed_for_testing(
          SigninClient::SignoutDecision::REVOKE_SYNC_DISALLOWED);
  GetProfile()->GetPrefs()->SetBoolean(prefs::kSigninAllowed, false);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1u, GetProfileManager()->profile_manager()->GetNumberOfProfiles());
}

TEST_F(AccountsPolicyManagerTest, ClearProfileUnallowedAccounts) {
  base::test::ScopedFeatureList feature_list(
      policy::features::kProfileSeparationDomainExceptionListRetroactive);
  GetIdentityTestEnv()->SetPrimaryAccount(kTestEmail,
                                          signin::ConsentLevel::kSignin);
  GetIdentityTestEnv()->SetRefreshTokenForPrimaryAccount();
  auto primary_account_id =
      identity_manager()->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
  EXPECT_TRUE(
      identity_manager()->HasAccountWithRefreshToken(primary_account_id));

  AccountInfo account_info2 = GetIdentityTestEnv()->MakeAccountAvailable(
      signin::AccountAvailabilityOptionsBuilder().Build(kTestEmail2));
  CoreAccountId account_id2 = account_info2.account_id;
  GetIdentityTestEnv()->SetRefreshTokenForAccount(account_id2);
  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(account_id2));

  AccountInfo account_info3 = GetIdentityTestEnv()->MakeAccountAvailable(
      signin::AccountAvailabilityOptionsBuilder().Build(kExampleEmail));
  CoreAccountId account_id3 = account_info3.account_id;
  GetIdentityTestEnv()->SetRefreshTokenForAccount(account_id3);
  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(account_id3));

  // Set prefs to only allow example.com secondary accounts
  base::Value::List profile_separation_domain_exception_list;
  profile_separation_domain_exception_list.Append("example.com");
  GetProfile()->GetPrefs()->SetList(
      prefs::kProfileSeparationDomainExceptionList,
      std::move(profile_separation_domain_exception_list));

  base::RunLoop run_loop;
  run_loop.RunUntilIdle();

  // Primary account not removed.
  EXPECT_TRUE(
      identity_manager()->HasAccountWithRefreshToken(primary_account_id));
  // @gmail account removed.
  EXPECT_FALSE(identity_manager()->HasAccountWithRefreshToken(account_id2));
  // @example.com account not removed.
  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(account_id3));
}

TEST_F(AccountsPolicyManagerTest, ClearProfileUnallowedAccountsDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      policy::features::kProfileSeparationDomainExceptionListRetroactive);
  GetIdentityTestEnv()->SetPrimaryAccount(kTestEmail,
                                          signin::ConsentLevel::kSignin);
  GetIdentityTestEnv()->SetRefreshTokenForPrimaryAccount();
  auto primary_account_id =
      identity_manager()->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
  EXPECT_TRUE(
      identity_manager()->HasAccountWithRefreshToken(primary_account_id));

  AccountInfo account_info2 = GetIdentityTestEnv()->MakeAccountAvailable(
      signin::AccountAvailabilityOptionsBuilder().Build(kTestEmail2));
  CoreAccountId account_id2 = account_info2.account_id;
  GetIdentityTestEnv()->SetRefreshTokenForAccount(account_id2);
  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(account_id2));

  AccountInfo account_info3 = GetIdentityTestEnv()->MakeAccountAvailable(
      signin::AccountAvailabilityOptionsBuilder().Build(kExampleEmail));
  CoreAccountId account_id3 = account_info3.account_id;
  GetIdentityTestEnv()->SetRefreshTokenForAccount(account_id3);
  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(account_id3));

  // Set prefs to only allow example.com secondary accounts
  base::Value::List profile_separation_domain_exception_list;
  profile_separation_domain_exception_list.Append("example.com");
  GetProfile()->GetPrefs()->SetList(
      prefs::kProfileSeparationDomainExceptionList,
      std::move(profile_separation_domain_exception_list));

  base::RunLoop run_loop;
  run_loop.RunUntilIdle();

  // No accounts were removed.
  EXPECT_TRUE(
      identity_manager()->HasAccountWithRefreshToken(primary_account_id));
  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(account_id2));
  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(account_id3));
}
#endif  // #if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)

#endif  // !BUILDFLAG(IS_CHROMEOS)
