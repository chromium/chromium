// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service.h"
#include <memory>
#include <utility>

#include "base/allocator/partition_allocator/pointers/raw_ptr.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/test/task_environment.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_controller.h"
#include "components/signin/public/base/account_consistency_method.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/test_signin_client.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

using signin::ConsentLevel;

namespace {
constexpr char kEmail[] = "primaryaccount@example.com";
constexpr char kSIDTSCookieName[] = "__Secure-1PSIDTS";

class FakeBoundSessionCookieController : public BoundSessionCookieController {
 public:
  explicit FakeBoundSessionCookieController(const GURL& url,
                                            const std::string& cookie_name,
                                            Delegate* delegate)
      : BoundSessionCookieController(url, cookie_name, delegate) {}

  ~FakeBoundSessionCookieController() override {
    DCHECK(on_destroy_callback_);
    std::move(on_destroy_callback_).Run();
  }

  void set_on_destroy_callback(base::OnceCallback<void()> on_destroy_callback) {
    on_destroy_callback_ = std::move(on_destroy_callback);
  }

  void SimulateOnCookieExpirationDateChanged(
      const base::Time& cookie_expiration_date) {
    cookie_expiration_time_ = cookie_expiration_date;
    delegate_->OnCookieExpirationDateChanged();
  }

 private:
  base::OnceCallback<void()> on_destroy_callback_;
};
}  // namespace

class BoundSessionCookieRefreshServiceTest : public testing::Test {
 public:
  BoundSessionCookieRefreshServiceTest()
      : identity_test_env_(&test_url_loader_factory_,
                           nullptr,
                           signin::AccountConsistencyMethod::kDice) {}

  ~BoundSessionCookieRefreshServiceTest() override = default;

  std::unique_ptr<BoundSessionCookieController> GetBoundSessionCookieController(
      const GURL& url,
      const std::string& cookie_name,
      BoundSessionCookieController::Delegate* delegate) {
    std::unique_ptr<FakeBoundSessionCookieController> controller =
        std::make_unique<FakeBoundSessionCookieController>(url, cookie_name,
                                                           delegate);
    controller->set_on_destroy_callback(base::BindOnce(
        &BoundSessionCookieRefreshServiceTest::OnCookieControllerDestroy,
        base::Unretained(this)));
    cookie_controller_ = controller.get();
    return controller;
  }

  void OnCookieControllerDestroy() { cookie_controller_ = nullptr; }

  BoundSessionCookieRefreshService* CreateCookieRefreshService() {
    if (!cookie_refresh_service_) {
      cookie_refresh_service_ =
          std::make_unique<BoundSessionCookieRefreshService>(
              /*client=*/nullptr, identity_manager());
      cookie_refresh_service_->set_controller_factory_for_testing(
          base::BindRepeating(&BoundSessionCookieRefreshServiceTest::
                                  GetBoundSessionCookieController,
                              base::Unretained(this)));
      cookie_refresh_service_->Initialize();
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

  FakeBoundSessionCookieController* cookie_controller() {
    return cookie_controller_;
  }

 private:
  base::test::TaskEnvironment task_environment_;
  sync_preferences::TestingPrefServiceSyncable prefs_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  signin::IdentityTestEnvironment identity_test_env_;
  std::unique_ptr<BoundSessionCookieRefreshService> cookie_refresh_service_;
  raw_ptr<FakeBoundSessionCookieController> cookie_controller_;
};

TEST_F(BoundSessionCookieRefreshServiceTest, VerifyControllerParams) {
  identity_test_env()->MakePrimaryAccountAvailable(kEmail,
                                                   ConsentLevel::kSignin);
  BoundSessionCookieRefreshService* service = CreateCookieRefreshService();
  EXPECT_TRUE(service->IsBoundSession());
  FakeBoundSessionCookieController* controller = cookie_controller();
  EXPECT_TRUE(controller);
  EXPECT_EQ(controller->url(), GaiaUrls::GetInstance()->secure_google_url());
  EXPECT_EQ(controller->cookie_name(), kSIDTSCookieName);
  EXPECT_EQ(controller->cookie_expiration_time(), base::Time());
}

TEST_F(BoundSessionCookieRefreshServiceTest, IsBoundSession_NoPrimaryAccount) {
  EXPECT_FALSE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSignin));
  BoundSessionCookieRefreshService* service = CreateCookieRefreshService();
  EXPECT_FALSE(service->IsBoundSession());
}

TEST_F(BoundSessionCookieRefreshServiceTest,
       IsBoundSession_SigninPrimaryAccount) {
  identity_test_env()->MakePrimaryAccountAvailable(kEmail,
                                                   ConsentLevel::kSignin);
  EXPECT_TRUE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSignin));
  BoundSessionCookieRefreshService* service = CreateCookieRefreshService();
  EXPECT_TRUE(service->IsBoundSession());
  identity_test_env()->WaitForRefreshTokensLoaded();
  EXPECT_TRUE(service->IsBoundSession());
  EXPECT_TRUE(cookie_controller());
}

TEST_F(BoundSessionCookieRefreshServiceTest,
       IsBoundSession_AccountsNotLoadedYet) {
  identity_test_env()->MakePrimaryAccountAvailable(kEmail,
                                                   ConsentLevel::kSignin);
  EXPECT_TRUE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSignin));
  identity_test_env()->ResetToAccountsNotYetLoadedFromDiskState();
  BoundSessionCookieRefreshService* service = CreateCookieRefreshService();
  EXPECT_TRUE(service->IsBoundSession());
  EXPECT_TRUE(cookie_controller());
}

TEST_F(BoundSessionCookieRefreshServiceTest,
       IsBoundSession_RefreshTokenInPersistentErrorState) {
  identity_test_env()->MakePrimaryAccountAvailable(kEmail,
                                                   ConsentLevel::kSignin);
  EXPECT_TRUE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSignin));
  BoundSessionCookieRefreshService* service = CreateCookieRefreshService();
  EXPECT_TRUE(service->IsBoundSession());
  EXPECT_TRUE(cookie_controller());

  identity_test_env()->UpdatePersistentErrorOfRefreshTokenForAccount(
      identity_manager()->GetPrimaryAccountId(ConsentLevel::kSignin),
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::
              CREDENTIALS_REJECTED_BY_CLIENT));
  EXPECT_FALSE(service->IsBoundSession());
  EXPECT_FALSE(cookie_controller());

  identity_test_env()->ResetToAccountsNotYetLoadedFromDiskState();
  ResetCookieRefreshService();
  service = CreateCookieRefreshService();
  EXPECT_TRUE(service->IsBoundSession());
  EXPECT_TRUE(cookie_controller());

  identity_test_env()->ReloadAccountsFromDisk();
  identity_test_env()->WaitForRefreshTokensLoaded();
  EXPECT_FALSE(service->IsBoundSession());
  EXPECT_FALSE(cookie_controller());
}

TEST_F(BoundSessionCookieRefreshServiceTest,
       IsBoundSession_OnPrimaryAccountChanged) {
  BoundSessionCookieRefreshService* service = CreateCookieRefreshService();
  identity_test_env()->WaitForRefreshTokensLoaded();
  EXPECT_FALSE(service->IsBoundSession());
  EXPECT_FALSE(cookie_controller());

  // `MakeAccountAvailable()` is used to ensure the primary account has
  // already
  // a refresh token when `OnPrimaryAccountChanged()` is fired.
  CoreAccountId account_id =
      identity_test_env()->MakeAccountAvailable(kEmail).account_id;
  EXPECT_FALSE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSignin));
  identity_test_env()->SetPrimaryAccount(kEmail, ConsentLevel::kSignin);
  EXPECT_TRUE(service->IsBoundSession());
  EXPECT_TRUE(cookie_controller());

  identity_test_env()->ClearPrimaryAccount();
  EXPECT_FALSE(service->IsBoundSession());
  EXPECT_FALSE(cookie_controller());
}

TEST_F(BoundSessionCookieRefreshServiceTest, IsBoundSession_EmptyGaiaAccounts) {
  identity_test_env()->MakePrimaryAccountAvailable(kEmail,
                                                   ConsentLevel::kSignin);
  EXPECT_TRUE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSignin));
  BoundSessionCookieRefreshService* service = CreateCookieRefreshService();
  EXPECT_TRUE(service->IsBoundSession());
  EXPECT_TRUE(cookie_controller());

  identity_test_env()->SetCookieAccounts({});
  EXPECT_FALSE(service->IsBoundSession());
  EXPECT_FALSE(cookie_controller());
}
