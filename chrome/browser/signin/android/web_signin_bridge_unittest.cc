// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "web_signin_bridge.h"

#include <memory>
#include <set>

#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "components/signin/public/base/test_signin_client.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/set_accounts_in_cookie_result.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

class StubAccountReconcilorDelegate : public signin::AccountReconcilorDelegate {
 public:
  StubAccountReconcilorDelegate() = default;
  ~StubAccountReconcilorDelegate() override = default;

  bool IsReconcileEnabled() const override { return true; }
  bool ShouldAbortReconcileIfPrimaryHasError() const override { return true; }
};

class StubAccountReconcilor : public AccountReconcilor {
 public:
  StubAccountReconcilor(signin::IdentityManager* identity_manager,
                        SigninClient* client)
      : AccountReconcilor(identity_manager,
                          client,
                          std::make_unique<StubAccountReconcilorDelegate>()) {}
  ~StubAccountReconcilor() override {
    EXPECT_FALSE(perform_logout_all_accounts_called_);
    EXPECT_FALSE(perform_set_cookies_called_);
  }

  void PerformLogoutAllAccountsAction() override {
    perform_logout_all_accounts_called_ = true;
  }

  void PerformSetCookiesAction(
      const signin::MultiloginParameters& parameters) override {
    perform_set_cookies_called_ = true;
  }

  void SimulateLogoutAllAccountsFinished() {
    EXPECT_TRUE(perform_logout_all_accounts_called_);
    perform_logout_all_accounts_called_ = false;
    OnLogOutFromCookieCompleted(GoogleServiceAuthError::AuthErrorNone());
  }

 private:
  bool perform_set_cookies_called_ = false;
  bool perform_logout_all_accounts_called_ = false;
};

class WebSigninBridgeTest : public ::testing::Test {
 public:
  WebSigninBridgeTest()
      : signin_client_(&prefs_),
        identity_test_env_(nullptr, &prefs_, &signin_client_) {
    account_reconcilor_ = std::make_unique<StubAccountReconcilor>(
        identity_test_env_.identity_manager(), &signin_client_);
  }

  WebSigninBridgeTest(const WebSigninBridgeTest&) = delete;
  WebSigninBridgeTest& operator=(const WebSigninBridgeTest&) = delete;

  ~WebSigninBridgeTest() override { account_reconcilor_->Shutdown(); }

  std::unique_ptr<WebSigninBridge> CreateWebSigninBridge(
      CoreAccountInfo account,
      WebSigninBridge::OnSigninCompletedCallback callback) {
    return std::make_unique<WebSigninBridge>(
        identity_test_env_.identity_manager(), account_reconcilor_.get(),
        account, std::move(callback));
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  sync_preferences::TestingPrefServiceSyncable prefs_;
  TestSigninClient signin_client_;
  signin::IdentityTestEnvironment identity_test_env_;
  std::unique_ptr<StubAccountReconcilor> account_reconcilor_;
};

TEST_F(WebSigninBridgeTest,
       CookiesWithSigninAccountShouldTriggerOnSigninSucceeded) {
  AccountInfo account =
      identity_test_env_.MakeAccountAvailable("test@gmail.com");
  base::MockCallback<WebSigninBridge::OnSigninCompletedCallback> callback;
  std::unique_ptr<WebSigninBridge> web_signin_bridge =
      CreateWebSigninBridge(account, callback.Get());
  EXPECT_CALL(callback, Run(GoogleServiceAuthError()));

  identity_test_env_.SetPrimaryAccount(account.email,
                                       signin::ConsentLevel::kSync);
  signin::CookieParamsForTest cookie_params{account.email, account.gaia};
  identity_test_env_.SetCookieAccounts({cookie_params});
}

TEST_F(
    WebSigninBridgeTest,
    CookiesWithSigninAccountShouldTriggerOnSigninSucceededAfterSigninFailed) {
  AccountInfo account =
      identity_test_env_.MakeAccountAvailable("test@gmail.com");
  base::MockCallback<WebSigninBridge::OnSigninCompletedCallback> callback;
  std::unique_ptr<WebSigninBridge> web_signin_bridge =
      CreateWebSigninBridge(account, callback.Get());

  EXPECT_CALL(callback,
              Run(GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
                  GoogleServiceAuthError::InvalidGaiaCredentialsReason::
                      CREDENTIALS_REJECTED_BY_SERVER)));
  identity_test_env_.SetPrimaryAccount(account.email,
                                       signin::ConsentLevel::kSync);
  identity_test_env_.SetInvalidRefreshTokenForAccount(account.account_id);
  identity_test_env_.UpdatePersistentErrorOfRefreshTokenForAccount(
      account.account_id,
      GoogleServiceAuthError(
          GoogleServiceAuthError::State::INVALID_GAIA_CREDENTIALS));
  account_reconcilor_->EnableReconcile();
  EXPECT_EQ(signin_metrics::AccountReconcilorState::kError,
            account_reconcilor_->GetState());

  EXPECT_CALL(callback, Run(GoogleServiceAuthError()));
  identity_test_env_.SetRefreshTokenForAccount(account.account_id);
  signin::CookieParamsForTest cookie_params{account.email, account.gaia};
  identity_test_env_.SetCookieAccounts({cookie_params});
  account_reconcilor_->SimulateLogoutAllAccountsFinished();
}

TEST_F(WebSigninBridgeTest,
       CookiesWithoutSigninAccountDontTriggerOnSigninSucceeded) {
  AccountInfo signin_account =
      identity_test_env_.MakeAccountAvailable("test1@gmail.com");
  AccountInfo non_signin_account =
      identity_test_env_.MakeAccountAvailable("test2@gmail.com");
  base::MockCallback<WebSigninBridge::OnSigninCompletedCallback> callback;
  std::unique_ptr<WebSigninBridge> web_signin_bridge =
      CreateWebSigninBridge(signin_account, callback.Get());
  EXPECT_CALL(callback, Run(_)).Times(0);

  identity_test_env_.SetPrimaryAccount(non_signin_account.email,
                                       signin::ConsentLevel::kSync);
  signin::CookieParamsForTest cookie_params{non_signin_account.email,
                                            non_signin_account.gaia};
  identity_test_env_.SetCookieAccounts({cookie_params});
}

TEST_F(WebSigninBridgeTest, ReconcilorErrorShouldTriggerOnSigninFailed) {
  AccountInfo account =
      identity_test_env_.MakeAccountAvailable("test@gmail.com");
  base::MockCallback<WebSigninBridge::OnSigninCompletedCallback> callback;
  std::unique_ptr<WebSigninBridge> web_signin_bridge =
      CreateWebSigninBridge(account, callback.Get());
  EXPECT_CALL(callback,
              Run(GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
                  GoogleServiceAuthError::InvalidGaiaCredentialsReason::
                      CREDENTIALS_REJECTED_BY_SERVER)));

  identity_test_env_.SetPrimaryAccount(account.email,
                                       signin::ConsentLevel::kSync);
  identity_test_env_.SetInvalidRefreshTokenForAccount(account.account_id);
  identity_test_env_.UpdatePersistentErrorOfRefreshTokenForAccount(
      account.account_id,
      GoogleServiceAuthError(
          GoogleServiceAuthError::State::INVALID_GAIA_CREDENTIALS));
  CoreAccountId account_id1 =
      identity_test_env_.identity_manager()->GetPrimaryAccountId(
          signin::ConsentLevel::kSync);
  EXPECT_TRUE(
      identity_test_env_.identity_manager()
          ->HasAccountWithRefreshTokenInPersistentErrorState(account_id1));
  account_reconcilor_->EnableReconcile();
  EXPECT_EQ(signin_metrics::AccountReconcilorState::kError,
            account_reconcilor_->GetState());
  CoreAccountId account_id2 =
      identity_test_env_.identity_manager()->GetPrimaryAccountId(
          signin::ConsentLevel::kSync);
  EXPECT_TRUE(
      identity_test_env_.identity_manager()
          ->HasAccountWithRefreshTokenInPersistentErrorState(account_id2));
}
