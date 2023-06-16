// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service_impl.h"

#include <memory>
#include <utility>

#include "base/allocator/partition_allocator/pointers/raw_ptr.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_controller.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_registration_params.pb.h"
#include "chrome/common/renderer_configuration.mojom.h"
#include "components/signin/public/base/account_consistency_method.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/test_signin_client.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using signin::ConsentLevel;

namespace {
constexpr char kEmail[] = "primaryaccount@example.com";
constexpr char kSIDTSCookieName[] = "__Secure-1PSIDTS";
constexpr char kRegistrationParamsPref[] =
    "bound_session_credentials_registration_params";

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

  void OnRequestBlockedOnCookie(
      base::OnceClosure resume_blocked_request) override {
    resume_blocked_requests_.push_back(std::move(resume_blocked_request));
  }

  void set_on_destroy_callback(base::OnceCallback<void()> on_destroy_callback) {
    on_destroy_callback_ = std::move(on_destroy_callback);
  }

  void SimulateOnCookieExpirationDateChanged(
      const base::Time& cookie_expiration_date) {
    cookie_expiration_time_ = cookie_expiration_date;
    delegate_->OnCookieExpirationDateChanged();
  }

  void SimulateTerminateSession() { delegate_->TerminateSession(); }

  void SimulateRefreshBoundSessionCompleted() {
    EXPECT_FALSE(resume_blocked_requests_.empty());
    std::vector<base::OnceClosure> callbacks;
    std::swap(resume_blocked_requests_, callbacks);
    for (auto& callback : callbacks) {
      std::move(callback).Run();
    }
  }

 private:
  base::OnceCallback<void()> on_destroy_callback_;
  std::vector<base::OnceClosure> resume_blocked_requests_;
};

bound_session_credentials::RegistrationParams CreateTestRegistrationParams() {
  bound_session_credentials::RegistrationParams params;
  params.set_site("google.com");
  params.set_session_id("test_session_id");
  return params;
}

}  // namespace

class BoundSessionCookieRefreshServiceImplTest
    : public testing::TestWithParam<bool> {
 public:
  const GURL kTestGaiaURL = GURL("https://google.com");

  BoundSessionCookieRefreshServiceImplTest()
      : identity_test_env_(&test_url_loader_factory_,
                           &prefs_,
                           signin::AccountConsistencyMethod::kDice) {
    scoped_feature_list_.InitWithFeatureState(kBoundSessionExplicitRegistration,
                                              IsExplicitRegistrationEnabled());
    BoundSessionCookieRefreshServiceImpl::RegisterProfilePrefs(
        prefs_.registry());
  }

  ~BoundSessionCookieRefreshServiceImplTest() override = default;

  std::unique_ptr<BoundSessionCookieController> GetBoundSessionCookieController(
      const GURL& url,
      const std::string& cookie_name,
      BoundSessionCookieController::Delegate* delegate) {
    std::unique_ptr<FakeBoundSessionCookieController> controller =
        std::make_unique<FakeBoundSessionCookieController>(url, cookie_name,
                                                           delegate);
    controller->set_on_destroy_callback(base::BindOnce(
        &BoundSessionCookieRefreshServiceImplTest::OnCookieControllerDestroy,
        base::Unretained(this)));
    cookie_controller_ = controller.get();
    return controller;
  }

  void OnCookieControllerDestroy() { cookie_controller_ = nullptr; }

  BoundSessionCookieRefreshServiceImpl* GetCookieRefreshServiceImpl() {
    if (!cookie_refresh_service_) {
      cookie_refresh_service_ =
          std::make_unique<BoundSessionCookieRefreshServiceImpl>(
              identity_test_env_.signin_client(), identity_manager());
      cookie_refresh_service_->set_controller_factory_for_testing(
          base::BindRepeating(&BoundSessionCookieRefreshServiceImplTest::
                                  GetBoundSessionCookieController,
                              base::Unretained(this)));
      cookie_refresh_service_->Initialize();
    }
    return cookie_refresh_service_.get();
  }

  void SetRendererUpdater(
      BoundSessionCookieRefreshService::
          RendererBoundSessionParamsUpdaterDelegate renderer_updater) {
    CHECK(cookie_refresh_service_);
    cookie_refresh_service_->SetRendererBoundSessionParamsUpdaterDelegate(
        renderer_updater);
  }

  void ResetRendererUpdater() {
    CHECK(cookie_refresh_service_);
    cookie_refresh_service_->SetRendererBoundSessionParamsUpdaterDelegate(
        base::RepeatingClosure());
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

  sync_preferences::TestingPrefServiceSyncable* prefs() { return &prefs_; }

  void SetupPreConditionForBoundSession() {
    if (IsExplicitRegistrationEnabled()) {
      bound_session_credentials::RegistrationParams params =
          CreateTestRegistrationParams();
      if (cookie_refresh_service_) {
        cookie_refresh_service_->RegisterNewBoundSession(params);
      } else {
        // Emulates an existing session that starts after
        // `cookie_refresh_service_` is created.
        prefs()->SetString(kRegistrationParamsPref, params.SerializeAsString());
      }
    } else {
      identity_test_env_.MakePrimaryAccountAvailable(kEmail,
                                                     ConsentLevel::kSignin);
    }
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  bool IsExplicitRegistrationEnabled() { return GetParam(); }

  void VerifyNoBoundSession() {
    CHECK(cookie_refresh_service_);
    EXPECT_FALSE(cookie_refresh_service_->IsBoundSession());
    EXPECT_FALSE(cookie_refresh_service_->GetBoundSessionParams());
    EXPECT_FALSE(cookie_controller());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_;
  sync_preferences::TestingPrefServiceSyncable prefs_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  signin::IdentityTestEnvironment identity_test_env_;
  std::unique_ptr<BoundSessionCookieRefreshServiceImpl> cookie_refresh_service_;
  raw_ptr<FakeBoundSessionCookieController> cookie_controller_ = nullptr;
};

TEST_P(BoundSessionCookieRefreshServiceImplTest, VerifyControllerParams) {
  SetupPreConditionForBoundSession();
  BoundSessionCookieRefreshServiceImpl* service = GetCookieRefreshServiceImpl();
  EXPECT_TRUE(service->IsBoundSession());
  FakeBoundSessionCookieController* controller = cookie_controller();
  EXPECT_TRUE(controller);
  EXPECT_EQ(controller->url(), kTestGaiaURL);
  EXPECT_EQ(controller->cookie_name(), kSIDTSCookieName);
  EXPECT_EQ(controller->cookie_expiration_time(), base::Time());
}

TEST_P(BoundSessionCookieRefreshServiceImplTest,
       VerifyBoundSessionParamsUnboundSession) {
  GetCookieRefreshServiceImpl();
  VerifyNoBoundSession();
}

TEST_P(BoundSessionCookieRefreshServiceImplTest,
       VerifyBoundSessionParamsBoundSession) {
  SetupPreConditionForBoundSession();
  BoundSessionCookieRefreshServiceImpl* service = GetCookieRefreshServiceImpl();
  EXPECT_TRUE(service->IsBoundSession());

  chrome::mojom::BoundSessionParamsPtr bound_session_params =
      service->GetBoundSessionParams();
  EXPECT_EQ(bound_session_params->domain, kTestGaiaURL.host());
  EXPECT_EQ(bound_session_params->path, kTestGaiaURL.path_piece());
}

TEST_P(BoundSessionCookieRefreshServiceImplTest,
       RefreshBoundSessionCookieBoundSession) {
  SetupPreConditionForBoundSession();
  BoundSessionCookieRefreshServiceImpl* service = GetCookieRefreshServiceImpl();
  EXPECT_TRUE(service->IsBoundSession());
  base::test::TestFuture<void> future;
  service->OnRequestBlockedOnCookie(future.GetCallback());
  EXPECT_TRUE(cookie_controller());

  EXPECT_FALSE(future.IsReady());
  cookie_controller()->SimulateRefreshBoundSessionCompleted();
  EXPECT_TRUE(future.IsReady());
}

TEST_P(BoundSessionCookieRefreshServiceImplTest,
       RefreshBoundSessionCookieUnboundSession) {
  BoundSessionCookieRefreshServiceImpl* service = GetCookieRefreshServiceImpl();
  EXPECT_FALSE(service->IsBoundSession());

  // Unbound session, the callback should be called immediately.
  base::test::TestFuture<void> future;
  service->OnRequestBlockedOnCookie(future.GetCallback());
  EXPECT_TRUE(future.IsReady());
}

TEST_P(BoundSessionCookieRefreshServiceImplTest,
       UpdateAllRenderersOnBoundSessionStarted) {
  BoundSessionCookieRefreshServiceImpl* service = GetCookieRefreshServiceImpl();
  EXPECT_FALSE(service->IsBoundSession());
  base::MockRepeatingCallback<void()> renderer_updater;
  EXPECT_CALL(renderer_updater, Run()).Times(0);
  SetRendererUpdater(renderer_updater.Get());
  testing::Mock::VerifyAndClearExpectations(&renderer_updater);

  // Create bound session.
  EXPECT_CALL(renderer_updater, Run()).WillOnce([&] {
    EXPECT_TRUE(service->IsBoundSession());
    EXPECT_FALSE(service->GetBoundSessionParams().is_null());
  });
  SetupPreConditionForBoundSession();
  testing::Mock::VerifyAndClearExpectations(&renderer_updater);
}

TEST_P(BoundSessionCookieRefreshServiceImplTest,
       UpdateAllRenderersOnCookieExpirationDateChanged) {
  base::MockRepeatingCallback<void()> renderer_updater;
  EXPECT_CALL(renderer_updater, Run()).Times(0);
  SetupPreConditionForBoundSession();
  BoundSessionCookieRefreshServiceImpl* service = GetCookieRefreshServiceImpl();
  EXPECT_TRUE(service->IsBoundSession());
  SetRendererUpdater(renderer_updater.Get());
  testing::Mock::VerifyAndClearExpectations(&renderer_updater);

  EXPECT_CALL(renderer_updater, Run()).WillOnce([&] {
    EXPECT_TRUE(service->IsBoundSession());
    EXPECT_FALSE(service->GetBoundSessionParams().is_null());
  });
  cookie_controller()->SimulateOnCookieExpirationDateChanged(base::Time::Now());
  testing::Mock::VerifyAndClearExpectations(&renderer_updater);
}

TEST_P(BoundSessionCookieRefreshServiceImplTest,
       UpdateAllRenderersOnBoundSessionTerminated) {
  base::MockRepeatingCallback<void()> renderer_updater;
  EXPECT_CALL(renderer_updater, Run()).Times(0);
  SetupPreConditionForBoundSession();
  BoundSessionCookieRefreshServiceImpl* service = GetCookieRefreshServiceImpl();
  EXPECT_TRUE(service->IsBoundSession());
  SetRendererUpdater(renderer_updater.Get());
  testing::Mock::VerifyAndClearExpectations(&renderer_updater);

  EXPECT_CALL(renderer_updater, Run()).WillOnce([&] {
    VerifyNoBoundSession();
  });
  cookie_controller()->SimulateTerminateSession();
  testing::Mock::VerifyAndClearExpectations(&renderer_updater);
}

TEST_P(BoundSessionCookieRefreshServiceImplTest, TerminateSession) {
  SetupPreConditionForBoundSession();
  BoundSessionCookieRefreshServiceImpl* service = GetCookieRefreshServiceImpl();
  EXPECT_TRUE(service->IsBoundSession());
  EXPECT_TRUE(service->GetBoundSessionParams());

  cookie_controller()->SimulateTerminateSession();
  VerifyNoBoundSession();

  if (IsExplicitRegistrationEnabled()) {
    // Verify prefs were cleared.
    // Ensure on next startup, there won't be a bound session.
    ResetCookieRefreshService();
    service = GetCookieRefreshServiceImpl();

    SCOPED_TRACE("No bound session on Startup.");
    VerifyNoBoundSession();
  }
}

TEST_P(BoundSessionCookieRefreshServiceImplTest,
       AddBoundSessionRequestThrottledListenerReceivers) {
  SetupPreConditionForBoundSession();
  BoundSessionCookieRefreshServiceImpl* service = GetCookieRefreshServiceImpl();
  EXPECT_TRUE(service->IsBoundSession());
  mojo::Remote<chrome::mojom::BoundSessionRequestThrottledListener> listener_1;
  mojo::Remote<chrome::mojom::BoundSessionRequestThrottledListener> listener_2;
  service->AddBoundSessionRequestThrottledListenerReceiver(
      listener_1.BindNewPipeAndPassReceiver());
  service->AddBoundSessionRequestThrottledListenerReceiver(
      listener_2.BindNewPipeAndPassReceiver());

  base::test::TestFuture<void> future_1;
  base::test::TestFuture<void> future_2;
  listener_1->OnRequestBlockedOnCookie(future_1.GetCallback());
  listener_2->OnRequestBlockedOnCookie(future_2.GetCallback());
  RunUntilIdle();

  EXPECT_FALSE(future_1.IsReady());
  EXPECT_FALSE(future_2.IsReady());

  cookie_controller()->SimulateRefreshBoundSessionCompleted();
  EXPECT_TRUE(future_1.Wait());
  EXPECT_TRUE(future_2.Wait());
}

TEST_P(BoundSessionCookieRefreshServiceImplTest,
       IsBoundSessionNoPrimaryAccount) {
  if (IsExplicitRegistrationEnabled()) {
    GTEST_SKIP();
  }
  EXPECT_FALSE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSignin));
  BoundSessionCookieRefreshServiceImpl* service = GetCookieRefreshServiceImpl();
  EXPECT_FALSE(service->IsBoundSession());
}

TEST_P(BoundSessionCookieRefreshServiceImplTest,
       IsBoundSessionSigninPrimaryAccount) {
  if (IsExplicitRegistrationEnabled()) {
    GTEST_SKIP();
  }
  SetupPreConditionForBoundSession();
  EXPECT_TRUE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSignin));
  BoundSessionCookieRefreshServiceImpl* service = GetCookieRefreshServiceImpl();
  EXPECT_TRUE(service->IsBoundSession());
  identity_test_env()->WaitForRefreshTokensLoaded();
  EXPECT_TRUE(service->IsBoundSession());
  EXPECT_TRUE(cookie_controller());
}

TEST_P(BoundSessionCookieRefreshServiceImplTest,
       IsBoundSessionAccountsNotLoadedYet) {
  if (IsExplicitRegistrationEnabled()) {
    GTEST_SKIP();
  }
  SetupPreConditionForBoundSession();
  EXPECT_TRUE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSignin));
  identity_test_env()->ResetToAccountsNotYetLoadedFromDiskState();
  BoundSessionCookieRefreshServiceImpl* service = GetCookieRefreshServiceImpl();
  EXPECT_TRUE(service->IsBoundSession());
  EXPECT_TRUE(cookie_controller());
}

TEST_P(BoundSessionCookieRefreshServiceImplTest,
       IsBoundSessionRefreshTokenInPersistentErrorState) {
  if (IsExplicitRegistrationEnabled()) {
    GTEST_SKIP();
  }
  SetupPreConditionForBoundSession();
  EXPECT_TRUE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSignin));
  BoundSessionCookieRefreshServiceImpl* service = GetCookieRefreshServiceImpl();
  EXPECT_TRUE(service->IsBoundSession());
  EXPECT_TRUE(cookie_controller());

  identity_test_env()->UpdatePersistentErrorOfRefreshTokenForAccount(
      identity_manager()->GetPrimaryAccountId(ConsentLevel::kSignin),
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::
              CREDENTIALS_REJECTED_BY_CLIENT));
  VerifyNoBoundSession();

  identity_test_env()->ResetToAccountsNotYetLoadedFromDiskState();
  ResetCookieRefreshService();
  service = GetCookieRefreshServiceImpl();
  EXPECT_TRUE(service->IsBoundSession());
  EXPECT_TRUE(cookie_controller());

  identity_test_env()->ReloadAccountsFromDisk();
  identity_test_env()->WaitForRefreshTokensLoaded();
  VerifyNoBoundSession();
}

TEST_P(BoundSessionCookieRefreshServiceImplTest,
       IsBoundSessionOnPrimaryAccountChanged) {
  if (IsExplicitRegistrationEnabled()) {
    GTEST_SKIP();
  }
  BoundSessionCookieRefreshServiceImpl* service = GetCookieRefreshServiceImpl();
  identity_test_env()->WaitForRefreshTokensLoaded();
  VerifyNoBoundSession();

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
  VerifyNoBoundSession();
}

TEST_P(BoundSessionCookieRefreshServiceImplTest,
       IsBoundSessionEmptyGaiaAccounts) {
  if (IsExplicitRegistrationEnabled()) {
    GTEST_SKIP();
  }
  SetupPreConditionForBoundSession();
  EXPECT_TRUE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSignin));
  BoundSessionCookieRefreshServiceImpl* service = GetCookieRefreshServiceImpl();
  EXPECT_TRUE(service->IsBoundSession());
  EXPECT_TRUE(cookie_controller());

  identity_test_env()->SetCookieAccounts({});
  VerifyNoBoundSession();
}

TEST_P(BoundSessionCookieRefreshServiceImplTest, RegisterNewBoundSession) {
  if (!IsExplicitRegistrationEnabled()) {
    GTEST_SKIP();
  }
  BoundSessionCookieRefreshServiceImpl* service = GetCookieRefreshServiceImpl();
  EXPECT_FALSE(service->IsBoundSession());
  EXPECT_FALSE(cookie_controller());

  service->RegisterNewBoundSession(CreateTestRegistrationParams());
  EXPECT_TRUE(service->IsBoundSession());
  EXPECT_TRUE(cookie_controller());
  // TODO(http://b/286222327): check registration params once they are properly
  // passed to controller.
}

TEST_P(BoundSessionCookieRefreshServiceImplTest, OverrideExistingBoundSession) {
  if (!IsExplicitRegistrationEnabled()) {
    GTEST_SKIP();
  }
  BoundSessionCookieRefreshServiceImpl* service = GetCookieRefreshServiceImpl();
  service->RegisterNewBoundSession(CreateTestRegistrationParams());

  auto new_params = CreateTestRegistrationParams();
  new_params.set_session_id("test_session_id_2");
  service->RegisterNewBoundSession(new_params);

  EXPECT_TRUE(service->IsBoundSession());
  EXPECT_TRUE(cookie_controller());
  // TODO(http://b/286222327): check registration params once they are properly
  // passed to controller.
}

INSTANTIATE_TEST_SUITE_P(,
                         BoundSessionCookieRefreshServiceImplTest,
                         testing::Bool());
