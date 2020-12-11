// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/signin/signin_manager.h"

#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Mock;

namespace signin {
namespace {
const char kTestEmail[] = "me@gmail.com";
const char kTestEmail2[] = "me2@gmail.com";

class IdentityManagerObserver : public IdentityManager::Observer {
 public:
  MOCK_METHOD1(OnUnconsentedPrimaryAccountChanged,
               void(const CoreAccountInfo& unconsented_primary_account_info));
  MOCK_METHOD1(BeforePrimaryAccountCleared,
               void(const CoreAccountInfo& previous_primary_account_info));
};
}  // namespace

class SigninManagerTest : public testing::Test {
 public:
  SigninManagerTest()
      : identity_test_env_(/*test_url_loader_factory=*/nullptr,
                           /*pref_service=*/nullptr,
                           signin::AccountConsistencyMethod::kDice,
                           /*test_signin_client=*/nullptr) {}

  void SetUp() override {
    testing::Test::SetUp();
    RecreateSigninManager();
    VerifyAndResetCallExpectations();
    identity_manager()->AddObserver(&observer_);
  }

  void TearDown() override { identity_manager()->RemoveObserver(&observer_); }

  void RecreateSigninManager() {
    signin_manger_ = std::make_unique<SigninManager>(identity_manager());
  }

  AccountInfo GetAccountInfo(const std::string& email) {
    AccountInfo account_info;
    account_info.gaia = GetTestGaiaIdForEmail(email);
    account_info.account_id =
        identity_manager()->PickAccountIdForAccount(account_info.gaia, email);
    account_info.email = email;
    return account_info;
  }

  IdentityManager* identity_manager() {
    return identity_test_env_.identity_manager();
  }

  IdentityTestEnvironment* identity_test_env() { return &identity_test_env_; }

  void MakeAccountAvailableWithCookies(const AccountInfo& account_info) {
    EXPECT_EQ(account_info, identity_test_env_.MakeAccountAvailableWithCookies(
                                account_info.email, account_info.gaia));
  }

  IdentityManagerObserver& observer() { return observer_; }

  void VerifyAndResetCallExpectations() {
    Mock::VerifyAndClear(&observer_);
    EXPECT_CALL(observer_, OnUnconsentedPrimaryAccountChanged(_)).Times(0);
    EXPECT_CALL(observer_, BeforePrimaryAccountCleared(_)).Times(0);
  }

  content::BrowserTaskEnvironment task_environment_;
  IdentityTestEnvironment identity_test_env_;
  std::unique_ptr<SigninManager> signin_manger_;
  IdentityManagerObserver observer_;

  DISALLOW_COPY_AND_ASSIGN(SigninManagerTest);
};

TEST_F(
    SigninManagerTest,
    UnconsentedPrimaryAccountUpdatedOnItsAccountRefreshTokenUpdateWithValidTokenWhenNoSyncConsent) {
  // Add an unconsented primary account, incl. proper cookies.
  AccountInfo account_info = GetAccountInfo(kTestEmail);
  EXPECT_CALL(observer(), OnUnconsentedPrimaryAccountChanged(account_info))
      .Times(1);
  MakeAccountAvailableWithCookies(account_info);
  VerifyAndResetCallExpectations();

  EXPECT_EQ(
      identity_manager()->GetPrimaryAccountInfo(ConsentLevel::kNotRequired),
      account_info);
}

TEST_F(
    SigninManagerTest,
    UnconsentedPrimaryAccountUpdatedOnItsAccountRefreshTokenUpdateWithInvalidTokenWhenNoSyncConsent) {
  // Add an unconsented primary account, incl. proper cookies.
  AccountInfo account_info = GetAccountInfo(kTestEmail);
  EXPECT_CALL(observer(), OnUnconsentedPrimaryAccountChanged(account_info))
      .Times(1);
  MakeAccountAvailableWithCookies(account_info);
  VerifyAndResetCallExpectations();

  // Invalid token.
  CoreAccountInfo empty_info;
  EXPECT_CALL(observer(), OnUnconsentedPrimaryAccountChanged(empty_info))
      .Times(1);
  SetInvalidRefreshTokenForAccount(identity_manager(), account_info.account_id);
  EXPECT_EQ(
      identity_manager()->GetPrimaryAccountInfo(ConsentLevel::kNotRequired),
      empty_info);
  VerifyAndResetCallExpectations();

  // Update with a valid token.
  EXPECT_CALL(observer(), OnUnconsentedPrimaryAccountChanged(account_info))
      .Times(1);
  UpdatePersistentErrorOfRefreshTokenForAccount(
      identity_manager(), account_info.account_id,
      GoogleServiceAuthError::AuthErrorNone());
  EXPECT_EQ(
      identity_manager()->GetPrimaryAccountInfo(ConsentLevel::kNotRequired),
      account_info);
  // Unconsented primary account should not be called.
  VerifyAndResetCallExpectations();
}

TEST_F(
    SigninManagerTest,
    UnconsentedPrimaryAccountRemovedOnItsAccountRefreshTokenRemovalWhenNoSyncConsent) {
  // Add an unconsented primary account, incl. proper cookies.
  AccountInfo account_info = GetAccountInfo(kTestEmail);
  EXPECT_CALL(observer(), OnUnconsentedPrimaryAccountChanged(account_info))
      .Times(1);
  MakeAccountAvailableWithCookies(account_info);
  VerifyAndResetCallExpectations();

  // With no refresh token, there is no unconsented primary account any more.
  CoreAccountInfo empty_info;
  EXPECT_CALL(observer(), OnUnconsentedPrimaryAccountChanged(empty_info))
      .Times(1);
  identity_test_env()->RemoveRefreshTokenForAccount(account_info.account_id);
  VerifyAndResetCallExpectations();
  EXPECT_FALSE(
      identity_manager()->HasPrimaryAccount(ConsentLevel::kNotRequired));

  EXPECT_EQ(
      identity_manager()->GetPrimaryAccountInfo(ConsentLevel::kNotRequired),
      empty_info);
  VerifyAndResetCallExpectations();
}

TEST_F(SigninManagerTest, UnconsentedPrimaryAccountNotChangedOnSignout) {
  // Setup cookies and token for the main account.
  AccountInfo account_info = GetAccountInfo(kTestEmail);
  EXPECT_CALL(observer(), OnUnconsentedPrimaryAccountChanged(account_info))
      .Times(1);
  identity_test_env()->MakePrimaryAccountAvailable(account_info.email);
  identity_test_env()->SetCookieAccounts(
      {{account_info.email, account_info.gaia}});

  EXPECT_EQ(account_info, identity_manager()->GetPrimaryAccountInfo(
                              ConsentLevel::kNotRequired));
  EXPECT_EQ(account_info,
            identity_manager()->GetPrimaryAccountInfo(ConsentLevel::kSync));
  EXPECT_TRUE(identity_manager()->HasPrimaryAccountWithRefreshToken());
  VerifyAndResetCallExpectations();
  // Tests that OnUnconsentedPrimaryAccountChanged is never called.
  EXPECT_CALL(observer(), BeforePrimaryAccountCleared(account_info)).Times(1);
  identity_test_env()->RevokeSyncConsent();
  // Primary account is cleared, but unconsented account is not.
  EXPECT_FALSE(identity_manager()->HasPrimaryAccount());
  EXPECT_EQ(account_info, identity_manager()->GetPrimaryAccountInfo(
                              ConsentLevel::kNotRequired));
  // OnUnconsentedPrimaryAccountChanged was not fired.
  VerifyAndResetCallExpectations();
}

TEST_F(SigninManagerTest,
       UnconsentedPrimaryAccountTokenRevokedWithStaleCookies) {
  AccountInfo account_info = GetAccountInfo(kTestEmail);
  // Add an unconsented primary account, incl. proper cookies.
  EXPECT_CALL(observer(), OnUnconsentedPrimaryAccountChanged(account_info))
      .Times(1);
  MakeAccountAvailableWithCookies(account_info);
  VerifyAndResetCallExpectations();

  EXPECT_EQ(
      identity_manager()->GetPrimaryAccountInfo(ConsentLevel::kNotRequired),
      account_info);

  // Make the cookies stale and remove the account.
  EXPECT_CALL(observer(), OnUnconsentedPrimaryAccountChanged(CoreAccountInfo()))
      .Times(1);
  identity_test_env()->SetFreshnessOfAccountsInGaiaCookie(false);
  // Removing the refresh token for the unconsented primary account is
  // sufficient to clear it.
  identity_test_env()->RemoveRefreshTokenForAccount(account_info.account_id);
  AccountsInCookieJarInfo cookie_info =
      identity_manager()->GetAccountsInCookieJar();
  ASSERT_FALSE(cookie_info.accounts_are_fresh);
  // Unconsented account was removed.
  EXPECT_EQ(
      identity_manager()->GetPrimaryAccountInfo(ConsentLevel::kNotRequired),
      CoreAccountInfo());
}

TEST_F(SigninManagerTest,
       UnconsentedPrimaryAccountTokenRevokedWithStaleCookiesMultipleAccounts) {
  // Add two accounts with cookies.
  AccountInfo main_account_info =
      identity_test_env()->MakeAccountAvailable(kTestEmail);
  AccountInfo secondary_account_info =
      identity_test_env()->MakeAccountAvailable(kTestEmail2);

  EXPECT_CALL(observer(), OnUnconsentedPrimaryAccountChanged(main_account_info))
      .Times(1);

  identity_test_env()->SetCookieAccounts(
      {{main_account_info.email, main_account_info.gaia},
       {secondary_account_info.email, secondary_account_info.gaia}});

  VerifyAndResetCallExpectations();
  EXPECT_EQ(
      identity_manager()->GetPrimaryAccountInfo(ConsentLevel::kNotRequired),
      main_account_info);

  // Make the cookies stale and remove the main account.
  EXPECT_CALL(observer(), OnUnconsentedPrimaryAccountChanged(CoreAccountInfo()))
      .Times(1);
  identity_test_env()->SetFreshnessOfAccountsInGaiaCookie(false);
  identity_test_env()->RemoveRefreshTokenForAccount(
      main_account_info.account_id);
  AccountsInCookieJarInfo cookie_info =
      identity_manager()->GetAccountsInCookieJar();
  ASSERT_FALSE(cookie_info.accounts_are_fresh);
  // Unconsented account was removed.
  EXPECT_EQ(
      identity_manager()->GetPrimaryAccountInfo(ConsentLevel::kNotRequired),
      CoreAccountInfo());
}

TEST_F(SigninManagerTest, UnconsentedPrimaryAccountDuringLoad) {
  // Add two accounts with cookies.
  AccountInfo main_account_info =
      identity_test_env()->MakeAccountAvailable(kTestEmail);
  AccountInfo secondary_account_info =
      identity_test_env()->MakeAccountAvailable(kTestEmail2);

  EXPECT_CALL(observer(), OnUnconsentedPrimaryAccountChanged(main_account_info))
      .Times(1);

  identity_test_env()->SetCookieAccounts(
      {{main_account_info.email, main_account_info.gaia},
       {secondary_account_info.email, secondary_account_info.gaia}});

  VerifyAndResetCallExpectations();
  EXPECT_EQ(
      identity_manager()->GetPrimaryAccountInfo(ConsentLevel::kNotRequired),
      main_account_info);

  // Set the token service in "loading" mode.
  identity_test_env()->ResetToAccountsNotYetLoadedFromDiskState();
  RecreateSigninManager();

  // Unconsented primary account is available while tokens are not loaded.
  EXPECT_EQ(
      identity_manager()->GetPrimaryAccountInfo(ConsentLevel::kNotRequired),
      main_account_info);
  VerifyAndResetCallExpectations();

  // Revoking an unrelated token doesn't change the unconsented primary account.
  identity_test_env()->RemoveRefreshTokenForAccount(
      secondary_account_info.account_id);
  EXPECT_EQ(
      identity_manager()->GetPrimaryAccountInfo(ConsentLevel::kNotRequired),
      main_account_info);

  // Revoke the unconsented primary account while tokens are not loaded.
  EXPECT_CALL(observer(), OnUnconsentedPrimaryAccountChanged(CoreAccountInfo()))
      .Times(1);

  identity_test_env()->RemoveRefreshTokenForAccount(
      main_account_info.account_id);
  EXPECT_FALSE(
      identity_manager()->HasPrimaryAccount(ConsentLevel::kNotRequired));
  VerifyAndResetCallExpectations();

  // Finish the token load.
  identity_test_env()->ReloadAccountsFromDisk();
  EXPECT_FALSE(
      identity_manager()->HasPrimaryAccount(ConsentLevel::kNotRequired));
}

TEST_F(SigninManagerTest,
       UnconsentedPrimaryAccountUpdatedOnSyncConsentRevoked) {
  AccountInfo first_account_info =
      identity_test_env()->MakeAccountAvailable(kTestEmail);
  AccountInfo second_account_info =
      identity_test_env()->MakeAccountAvailable(kTestEmail2);

  EXPECT_CALL(observer(),
              OnUnconsentedPrimaryAccountChanged(first_account_info))
      .Times(1);

  identity_test_env()->SetCookieAccounts(
      {{first_account_info.email, first_account_info.gaia},
       {second_account_info.email, second_account_info.gaia}});

  VerifyAndResetCallExpectations();

  // Set the primary account to the second account in cookies.
  // The unconsented primary account should be updated.
  EXPECT_CALL(observer(),
              OnUnconsentedPrimaryAccountChanged(second_account_info))
      .Times(1);
  identity_test_env()->SetPrimaryAccount(second_account_info.email);
  EXPECT_EQ(identity_manager()->GetPrimaryAccountInfo(ConsentLevel::kSync),
            second_account_info);
  VerifyAndResetCallExpectations();

  // Clear primary account but do not delete the account. The unconsented
  // primary account should be updated to be the first account in cookies.
  EXPECT_CALL(observer(), BeforePrimaryAccountCleared(second_account_info))
      .Times(1);
  EXPECT_CALL(observer(),
              OnUnconsentedPrimaryAccountChanged(first_account_info))
      .Times(1);
  identity_test_env()->RevokeSyncConsent();
  // Primary account is cleared, but unconsented account is not.
  EXPECT_FALSE(identity_manager()->HasPrimaryAccount());
  EXPECT_FALSE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSync));
  EXPECT_EQ(first_account_info, identity_manager()->GetPrimaryAccountInfo(
                                    ConsentLevel::kNotRequired));
  // OnUnconsentedPrimaryAccountChanged was fired.
  VerifyAndResetCallExpectations();
}

TEST_F(SigninManagerTest, ClearPrimaryAccountAndSignOut) {
  AccountInfo account_info = GetAccountInfo(kTestEmail);
  EXPECT_CALL(observer(), OnUnconsentedPrimaryAccountChanged(account_info))
      .Times(1);
  identity_test_env()->MakePrimaryAccountAvailable(kTestEmail);
  EXPECT_EQ(identity_manager()->GetPrimaryAccountInfo(ConsentLevel::kSync),
            account_info);
  VerifyAndResetCallExpectations();

  identity_test_env()->SetCookieAccounts(
      {{account_info.email, account_info.gaia}});

  EXPECT_CALL(observer(), BeforePrimaryAccountCleared(account_info)).Times(1);
  EXPECT_CALL(observer(), OnUnconsentedPrimaryAccountChanged(CoreAccountInfo()))
      .Times(1);
  identity_test_env()->ClearPrimaryAccount();
  VerifyAndResetCallExpectations();
}

}  // namespace signin
