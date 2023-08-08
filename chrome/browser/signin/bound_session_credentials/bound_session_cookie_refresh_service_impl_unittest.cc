// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service_impl.h"

#include <memory>
#include <utility>

#include "base/allocator/partition_allocator/pointers/raw_ptr.h"
#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_controller.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_registration_params.pb.h"
#include "chrome/common/renderer_configuration.mojom.h"
#include "components/signin/public/base/test_signin_client.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/unexportable_keys/fake_unexportable_key_service.h"
#include "google_apis/gaia/gaia_urls.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
constexpr char k1PSIDTSCookieName[] = "__Secure-1PSIDTS";
constexpr char k3PSIDTSCookieName[] = "__Secure-3PSIDTS";
constexpr char kRegistrationParamsPref[] =
    "bound_session_credentials_registration_params";
const char kSessionTerminationHeader[] = "Sec-Session-Google-Termination";
constexpr char kWrappedKey[] = "wrapped_key";

class FakeBoundSessionCookieController : public BoundSessionCookieController {
 public:
  explicit FakeBoundSessionCookieController(
      bound_session_credentials::RegistrationParams registration_params,
      const base::flat_set<std::string>& cookie_names,
      Delegate* delegate)
      : BoundSessionCookieController(registration_params,
                                     cookie_names,
                                     delegate) {
    std::string wrapped_key_str = registration_params.wrapped_key();
    wrapped_key_.assign(wrapped_key_str.begin(), wrapped_key_str.end());
  }

  ~FakeBoundSessionCookieController() override {
    DCHECK(on_destroy_callback_);
    std::move(on_destroy_callback_).Run();
  }

  base::flat_set<std::string> cookie_names() {
    base::flat_set<std::string> cookie_names;
    for (const auto& [cookie_name, _] : bound_cookies_info_) {
      cookie_names.insert(cookie_name);
    }
    return cookie_names;
  }

  const std::vector<uint8_t>& wrapped_key() { return wrapped_key_; }

  void OnRequestBlockedOnCookie(
      base::OnceClosure resume_blocked_request) override {
    resume_blocked_requests_.push_back(std::move(resume_blocked_request));
  }

  void set_on_destroy_callback(base::OnceCallback<void()> on_destroy_callback) {
    on_destroy_callback_ = std::move(on_destroy_callback);
  }

  void SimulateOnCookieExpirationDateChanged(
      const std::string& cookie_name,
      const base::Time& cookie_expiration_date) {
    base::Time old_min_cookie_expiration_time = min_cookie_expiration_time();
    bound_cookies_info_[cookie_name] = cookie_expiration_date;
    if (min_cookie_expiration_time() != old_min_cookie_expiration_time) {
      delegate_->OnBoundSessionParamsChanged();
    }
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
  std::vector<uint8_t> wrapped_key_;
};

bound_session_credentials::RegistrationParams CreateTestRegistrationParams() {
  bound_session_credentials::RegistrationParams params;
  params.set_site(GaiaUrls::GetInstance()->secure_google_url().spec());
  params.set_session_id("test_session_id");
  params.set_wrapped_key(kWrappedKey);
  return params;
}

}  // namespace

class BoundSessionCookieRefreshServiceImplTest : public testing::Test {
 public:
  const GURL kTestGaiaURL = GURL("https://google.com");
  BoundSessionCookieRefreshServiceImplTest() : signin_client_(&prefs_) {
    BoundSessionCookieRefreshServiceImpl::RegisterProfilePrefs(
        prefs_.registry());
  }

  ~BoundSessionCookieRefreshServiceImplTest() override = default;

  std::unique_ptr<BoundSessionCookieController> GetBoundSessionCookieController(
      bound_session_credentials::RegistrationParams registration_params,
      const base::flat_set<std::string>& cookie_names,
      BoundSessionCookieController::Delegate* delegate) {
    std::unique_ptr<FakeBoundSessionCookieController> controller =
        std::make_unique<FakeBoundSessionCookieController>(
            registration_params, cookie_names, delegate);
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
              fake_unexportable_key_service_, &prefs_, &signin_client_);
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

  FakeBoundSessionCookieController* cookie_controller() {
    return cookie_controller_;
  }

  sync_preferences::TestingPrefServiceSyncable* prefs() { return &prefs_; }

  // Emulates an existing session that resumes after `cookie_refresh_service_`
  // is created
  void SetupPreConditionForBoundSession() {
    CHECK(!cookie_refresh_service_)
        << "If the cookie refresh service is already created, consider using "
           "`RegisterNewBoundSession` to start a new bound session.";
    bound_session_credentials::RegistrationParams params =
        CreateTestRegistrationParams();
    std::string registration_params;
    base::Base64Encode(params.SerializeAsString(), &registration_params);
    prefs()->SetString(kRegistrationParamsPref, registration_params);
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  void VerifyBoundSession() {
    CHECK(cookie_refresh_service_);
    EXPECT_TRUE(cookie_refresh_service_->IsBoundSession());
    EXPECT_TRUE(cookie_refresh_service_->GetBoundSessionParams());
    EXPECT_TRUE(cookie_controller());
  }

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
  TestSigninClient signin_client_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<BoundSessionCookieRefreshServiceImpl> cookie_refresh_service_;
  unexportable_keys::FakeUnexportableKeyService fake_unexportable_key_service_;
  raw_ptr<FakeBoundSessionCookieController> cookie_controller_ = nullptr;
};

TEST_F(BoundSessionCookieRefreshServiceImplTest, VerifyControllerParams) {
  SetupPreConditionForBoundSession();
  BoundSessionCookieRefreshServiceImpl* service = GetCookieRefreshServiceImpl();
  EXPECT_TRUE(service->IsBoundSession());
  FakeBoundSessionCookieController* controller = cookie_controller();
  EXPECT_TRUE(controller);
  EXPECT_EQ(controller->url(), kTestGaiaURL);
  EXPECT_EQ(
      controller->cookie_names(),
      base::flat_set<std::string>({k1PSIDTSCookieName, k3PSIDTSCookieName}));
  EXPECT_EQ(controller->min_cookie_expiration_time(), base::Time());
  EXPECT_EQ(controller->wrapped_key(),
            std::vector<uint8_t>(std::begin(kWrappedKey),
                                 // Omit `\0`.
                                 std::end(kWrappedKey) - 1));
}

TEST_F(BoundSessionCookieRefreshServiceImplTest,
       VerifyBoundSessionParamsUnboundSession) {
  GetCookieRefreshServiceImpl();
  VerifyNoBoundSession();
}

TEST_F(BoundSessionCookieRefreshServiceImplTest,
       VerifyBoundSessionParamsBoundSession) {
  SetupPreConditionForBoundSession();
  BoundSessionCookieRefreshServiceImpl* service = GetCookieRefreshServiceImpl();
  EXPECT_TRUE(service->IsBoundSession());

  chrome::mojom::BoundSessionParamsPtr bound_session_params =
      service->GetBoundSessionParams();
  EXPECT_EQ(bound_session_params->domain, kTestGaiaURL.host());
  EXPECT_EQ(bound_session_params->path, kTestGaiaURL.path_piece());
}

TEST_F(BoundSessionCookieRefreshServiceImplTest,
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

TEST_F(BoundSessionCookieRefreshServiceImplTest,
       RefreshBoundSessionCookieUnboundSession) {
  BoundSessionCookieRefreshServiceImpl* service = GetCookieRefreshServiceImpl();
  EXPECT_FALSE(service->IsBoundSession());

  // Unbound session, the callback should be called immediately.
  base::test::TestFuture<void> future;
  service->OnRequestBlockedOnCookie(future.GetCallback());
  EXPECT_TRUE(future.IsReady());
}

TEST_F(BoundSessionCookieRefreshServiceImplTest,
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
  service->RegisterNewBoundSession(CreateTestRegistrationParams());
  testing::Mock::VerifyAndClearExpectations(&renderer_updater);
}

TEST_F(BoundSessionCookieRefreshServiceImplTest,
       UpdateAllRenderersOnBoundSessionParamsChanged) {
  base::MockRepeatingCallback<void()> renderer_updater;
  EXPECT_CALL(renderer_updater, Run()).Times(0);
  SetupPreConditionForBoundSession();
  BoundSessionCookieRefreshServiceImpl* service = GetCookieRefreshServiceImpl();
  EXPECT_TRUE(service->IsBoundSession());
  SetRendererUpdater(renderer_updater.Get());
  testing::Mock::VerifyAndClearExpectations(&renderer_updater);

  EXPECT_CALL(renderer_updater, Run()).Times(0);
  cookie_controller()->SimulateOnCookieExpirationDateChanged(k1PSIDTSCookieName,
                                                             base::Time::Now());
  testing::Mock::VerifyAndClearExpectations(&renderer_updater);

  EXPECT_CALL(renderer_updater, Run()).WillOnce([&] {
    EXPECT_TRUE(service->IsBoundSession());
    EXPECT_FALSE(service->GetBoundSessionParams().is_null());
  });
  cookie_controller()->SimulateOnCookieExpirationDateChanged(k3PSIDTSCookieName,
                                                             base::Time::Now());
  testing::Mock::VerifyAndClearExpectations(&renderer_updater);
}

TEST_F(BoundSessionCookieRefreshServiceImplTest,
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

TEST_F(BoundSessionCookieRefreshServiceImplTest, TerminateSession) {
  SetupPreConditionForBoundSession();
  BoundSessionCookieRefreshServiceImpl* service = GetCookieRefreshServiceImpl();
  EXPECT_TRUE(service->IsBoundSession());
  EXPECT_TRUE(service->GetBoundSessionParams());

  cookie_controller()->SimulateTerminateSession();
  VerifyNoBoundSession();

  // Verify prefs were cleared.
  // Ensure on next startup, there won't be a bound session.
  ResetCookieRefreshService();
  service = GetCookieRefreshServiceImpl();

  SCOPED_TRACE("No bound session on Startup.");
  VerifyNoBoundSession();
}

// TODO(b/293433229): Verify session terminated only if `session_id` matches the
// current session's id.
TEST_F(BoundSessionCookieRefreshServiceImplTest,
       TerminateSessionOnSessionTerminationHeader) {
  SetupPreConditionForBoundSession();
  scoped_refptr<net::HttpResponseHeaders> headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("");
  headers->AddHeader(kSessionTerminationHeader, "session-id");

  BoundSessionCookieRefreshServiceImpl* service = GetCookieRefreshServiceImpl();
  service->MaybeTerminateSession(headers.get());
  VerifyNoBoundSession();
}

TEST_F(BoundSessionCookieRefreshServiceImplTest,
       DontTerminateSessionWithoutSessionTerminationHeader) {
  SetupPreConditionForBoundSession();
  scoped_refptr<net::HttpResponseHeaders> headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("");

  BoundSessionCookieRefreshServiceImpl* service = GetCookieRefreshServiceImpl();
  service->MaybeTerminateSession(headers.get());
  VerifyBoundSession();
}

TEST_F(BoundSessionCookieRefreshServiceImplTest,
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

TEST_F(BoundSessionCookieRefreshServiceImplTest, RegisterNewBoundSession) {
  BoundSessionCookieRefreshServiceImpl* service = GetCookieRefreshServiceImpl();
  EXPECT_FALSE(service->IsBoundSession());
  EXPECT_FALSE(cookie_controller());

  service->RegisterNewBoundSession(CreateTestRegistrationParams());
  EXPECT_TRUE(service->IsBoundSession());
  EXPECT_TRUE(cookie_controller());
  // TODO(http://b/286222327): check registration params once they are
  // properly passed to controller.
}

TEST_F(BoundSessionCookieRefreshServiceImplTest, OverrideExistingBoundSession) {
  BoundSessionCookieRefreshServiceImpl* service = GetCookieRefreshServiceImpl();
  service->RegisterNewBoundSession(CreateTestRegistrationParams());

  auto new_params = CreateTestRegistrationParams();
  new_params.set_session_id("test_session_id_2");
  service->RegisterNewBoundSession(new_params);

  EXPECT_TRUE(service->IsBoundSession());
  EXPECT_TRUE(cookie_controller());
  // TODO(http://b/286222327): check registration params once they are
  // properly passed to controller.
}
