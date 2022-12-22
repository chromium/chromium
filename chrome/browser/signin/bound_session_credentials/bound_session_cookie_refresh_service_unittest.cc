// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service.h"

#include "base/test/task_environment.h"
#include "components/signin/public/base/account_consistency_method.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

using signin::ConsentLevel;

constexpr char kEmail[] = "primaryaccount@example.com";

class BoundSessionCookieRefreshServiceTest : public testing::Test {
 public:
  BoundSessionCookieRefreshServiceTest()
      : identity_test_env_(&test_url_loader_factory_,
                           nullptr,
                           signin::AccountConsistencyMethod::kDice) {}

  ~BoundSessionCookieRefreshServiceTest() override = default;

  BoundSessionCookieRefreshService* GetCookieRefreshService() {
    if (!cookie_refresh_service_) {
      cookie_refresh_service_ =
          std::make_unique<BoundSessionCookieRefreshService>(
              identity_manager());
    }
    return cookie_refresh_service_.get();
  }

  void ResetCookieRefreshService() { cookie_refresh_service_.reset(); }

  signin::IdentityManager* identity_manager() {
    return identity_test_env_.identity_manager();
  }

  signin::IdentityTestEnvironment* identity_test_env() {
    return &identity_test_env_;
  }

 private:
  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  signin::IdentityTestEnvironment identity_test_env_;
  std::unique_ptr<BoundSessionCookieRefreshService> cookie_refresh_service_;
};

TEST_F(BoundSessionCookieRefreshServiceTest, IsBoundSession_NoPrimaryAccount) {
  EXPECT_FALSE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSignin));
  BoundSessionCookieRefreshService* service = GetCookieRefreshService();
  EXPECT_FALSE(service->IsBoundSession());
}

TEST_F(BoundSessionCookieRefreshServiceTest,
       IsBoundSession_SigninPrimaryAccount) {
  identity_test_env()->MakePrimaryAccountAvailable(kEmail,
                                                   ConsentLevel::kSignin);
  EXPECT_TRUE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSignin));
  BoundSessionCookieRefreshService* service = GetCookieRefreshService();
  EXPECT_TRUE(service->IsBoundSession());
  identity_test_env()->WaitForRefreshTokensLoaded();
  EXPECT_TRUE(service->IsBoundSession());
}

TEST_F(BoundSessionCookieRefreshServiceTest,
       IsBoundSession_AccountsNotLoadedYet) {
  identity_test_env()->MakePrimaryAccountAvailable(kEmail,
                                                   ConsentLevel::kSignin);
  EXPECT_TRUE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSignin));
  identity_test_env()->ResetToAccountsNotYetLoadedFromDiskState();
  BoundSessionCookieRefreshService* service = GetCookieRefreshService();
  EXPECT_TRUE(service->IsBoundSession());
}

TEST_F(BoundSessionCookieRefreshServiceTest,
       IsBoundSession_RefreshTokenInPersistentErrorState) {
  identity_test_env()->MakePrimaryAccountAvailable(kEmail,
                                                   ConsentLevel::kSignin);
  EXPECT_TRUE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSignin));
  BoundSessionCookieRefreshService* service = GetCookieRefreshService();
  EXPECT_TRUE(service->IsBoundSession());

  identity_test_env()->UpdatePersistentErrorOfRefreshTokenForAccount(
      identity_manager()->GetPrimaryAccountId(ConsentLevel::kSignin),
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::
              CREDENTIALS_REJECTED_BY_CLIENT));
  EXPECT_FALSE(service->IsBoundSession());

  identity_test_env()->ResetToAccountsNotYetLoadedFromDiskState();
  ResetCookieRefreshService();
  service = GetCookieRefreshService();
  EXPECT_TRUE(service->IsBoundSession());

  identity_test_env()->ReloadAccountsFromDisk();
  identity_test_env()->WaitForRefreshTokensLoaded();
  EXPECT_FALSE(service->IsBoundSession());
}

TEST_F(BoundSessionCookieRefreshServiceTest,
       IsBoundSession_OnPrimaryAccountChanged) {
  BoundSessionCookieRefreshService* service = GetCookieRefreshService();
  identity_test_env()->WaitForRefreshTokensLoaded();
  EXPECT_FALSE(service->IsBoundSession());

  // `MakeAccountAvailable()` is used to ensure the primary account has already
  // a refresh token when `OnPrimaryAccountChanged()` is fired.
  CoreAccountId account_id =
      identity_test_env()->MakeAccountAvailable(kEmail).account_id;
  EXPECT_FALSE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSignin));
  identity_test_env()->SetPrimaryAccount(kEmail, ConsentLevel::kSignin);
  EXPECT_TRUE(service->IsBoundSession());

  identity_test_env()->ClearPrimaryAccount();
  EXPECT_FALSE(service->IsBoundSession());
}

TEST_F(BoundSessionCookieRefreshServiceTest, IsBoundSession_EmptyGaiaAccounts) {
  identity_test_env()->MakePrimaryAccountAvailable(kEmail,
                                                   ConsentLevel::kSignin);
  EXPECT_TRUE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSignin));
  BoundSessionCookieRefreshService* service = GetCookieRefreshService();
  EXPECT_TRUE(service->IsBoundSession());

  identity_test_env()->SetCookieAccounts({});
  EXPECT_FALSE(service->IsBoundSession());
}
