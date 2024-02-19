// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service_impl.h"

#include <memory>
#include <utility>

#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_controller.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_params.pb.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_params_storage.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_params_util.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_registration_fetcher.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_registration_fetcher_param.h"
#include "chrome/browser/signin/bound_session_credentials/fake_bound_session_refresh_cookie_fetcher.h"
#include "chrome/common/renderer_configuration.mojom.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/unexportable_keys/fake_unexportable_key_service.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/test/test_storage_partition.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {
using SessionTerminationTrigger =
    BoundSessionCookieRefreshServiceImpl::SessionTerminationTrigger;
using chrome::mojom::ResumeBlockedRequestsTrigger;

constexpr char k1PSIDTSCookieName[] = "__Secure-1PSIDTS";
constexpr char k3PSIDTSCookieName[] = "__Secure-3PSIDTS";
const char kSessionTerminationHeader[] = "Sec-Session-Google-Termination";
constexpr char kWrappedKey[] = "wrapped_key";
constexpr char kTestSessionId[] = "test_session_id";
constexpr char kDefaultRegistrationPath[] = "/RegisterSession";
constexpr ResumeBlockedRequestsTrigger kRefreshCompletedTrigger =
    ResumeBlockedRequestsTrigger::kObservedFreshCookies;

// Matches a cookie name against a `bound_session_credentials::Credential`.
// `arg` type is std::tuple<std::string, bound_session_credentials::Credential>
MATCHER(IsCookieCredential, "") {
  const auto& [cookie_name, credential] = arg;
  if (!credential.has_cookie_credential()) {
    return false;
  }

  return cookie_name == credential.cookie_credential().name();
}

// Checks equality of the two protos in an std::tuple. Useful for matching two
// two protos using ::testing::Pointwise or ::testing::UnorderedPointwise.
MATCHER(TupleEqualsProto, "") {
  return testing::ExplainMatchResult(base::test::EqualsProto(std::get<1>(arg)),
                                     std::get<0>(arg), result_listener);
}

class FakeBoundSessionCookieController : public BoundSessionCookieController {
 public:
  explicit FakeBoundSessionCookieController(
      const bound_session_credentials::BoundSessionParams& bound_session_params,
      Delegate* delegate)
      : BoundSessionCookieController(bound_session_params, delegate) {
    std::string wrapped_key_str = bound_session_params.wrapped_key();
    wrapped_key_.assign(wrapped_key_str.begin(), wrapped_key_str.end());
  }

  const std::vector<uint8_t>& wrapped_key() { return wrapped_key_; }

  void HandleRequestBlockedOnCookie(
      chrome::mojom::BoundSessionRequestThrottledHandler::
          HandleRequestBlockedOnCookieCallback resume_blocked_request)
      override {
    resume_blocked_requests_.push_back(std::move(resume_blocked_request));
  }

  void SimulateOnCookieExpirationDateChanged(
      const std::string& cookie_name,
      const base::Time& cookie_expiration_date) {
    base::Time old_min_cookie_expiration_time = min_cookie_expiration_time();
    bound_cookies_info_[cookie_name] = cookie_expiration_date;
    if (min_cookie_expiration_time() != old_min_cookie_expiration_time) {
      delegate_->OnBoundSessionThrottlerParamsChanged();
    }
  }

  void SimulateOnPersistentErrorEncountered() {
    delegate_->OnPersistentErrorEncountered();
  }

  void SimulateRefreshBoundSessionCompleted() {
    EXPECT_FALSE(resume_blocked_requests_.empty());
    std::vector<chrome::mojom::BoundSessionRequestThrottledHandler::
                    HandleRequestBlockedOnCookieCallback>
        callbacks;
    std::swap(resume_blocked_requests_, callbacks);
    for (auto& callback : callbacks) {
      std::move(callback).Run(kRefreshCompletedTrigger);
    }
  }

  base::WeakPtr<FakeBoundSessionCookieController> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  std::vector<chrome::mojom::BoundSessionRequestThrottledHandler::
                  HandleRequestBlockedOnCookieCallback>
      resume_blocked_requests_;
  std::vector<uint8_t> wrapped_key_;
  base::WeakPtrFactory<FakeBoundSessionCookieController> weak_ptr_factory_{
      this};
};

class FakeBoundSessionRegistrationFetcher
    : public BoundSessionRegistrationFetcher {
 public:
  explicit FakeBoundSessionRegistrationFetcher(
      BoundSessionRegistrationFetcherParam params)
      : params_(std::move(params)) {}

  void SimulateRegistrationFetchCompleted(
      bound_session_credentials::BoundSessionParams session_params) {
    CHECK(callback_);
    std::move(callback_).Run(std::move(session_params));
  }

  base::WeakPtr<FakeBoundSessionRegistrationFetcher> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  const BoundSessionRegistrationFetcherParam& params() { return params_; }

  // BoundSessionRegistrationFetcher:
  void Start(RegistrationCompleteCallback callback) override {
    callback_ = std::move(callback);
  }

 private:
  BoundSessionRegistrationFetcherParam params_;
  RegistrationCompleteCallback callback_;
  base::WeakPtrFactory<FakeBoundSessionRegistrationFetcher> weak_ptr_factory_{
      this};
};

class MockObserver : public BoundSessionCookieRefreshService::Observer {
 public:
  MOCK_METHOD(void,
              OnBoundSessionTerminated,
              (const GURL& site,
               const base::flat_set<std::string>& bound_cookie_names),
              (override));
};
}  // namespace

class BoundSessionCookieRefreshServiceImplTest : public testing::Test {
 public:
  const GURL kTestGoogleURL = GURL("https://google.com");

  BoundSessionCookieRefreshServiceImplTest() {
    BoundSessionParamsStorage::RegisterProfilePrefs(prefs_.registry());
    test_storage_ =
        BoundSessionParamsStorage::CreatePrefsStorageForTesting(prefs_);
  }

  ~BoundSessionCookieRefreshServiceImplTest() override = default;

  std::unique_ptr<BoundSessionCookieController>
  CreateBoundSessionCookieController(
      const bound_session_credentials::BoundSessionParams& bound_session_params,
      BoundSessionCookieController::Delegate* delegate) {
    auto controller = std::make_unique<FakeBoundSessionCookieController>(
        bound_session_params, delegate);
    cookie_controller_ = controller->GetWeakPtr();
    return controller;
  }

  std::unique_ptr<BoundSessionRegistrationFetcher>
  CreateBoundSessionRegistrationFetcher(
      BoundSessionRegistrationFetcherParam fetcher_params) {
    auto fetcher = std::make_unique<FakeBoundSessionRegistrationFetcher>(
        std::move(fetcher_params));
    registration_fetcher_ = fetcher->GetWeakPtr();
    return fetcher;
  }

  BoundSessionCookieRefreshServiceImpl* GetCookieRefreshServiceImpl() {
    if (!cookie_refresh_service_) {
      cookie_refresh_service_ = CreateBoundSessionCookieRefreshServiceImpl();
    }
    return cookie_refresh_service_.get();
  }

  void SetRendererUpdater(
      BoundSessionCookieRefreshService::
          RendererBoundSessionThrottlerParamsUpdaterDelegate renderer_updater) {
    CHECK(cookie_refresh_service_);
    cookie_refresh_service_
        ->SetRendererBoundSessionThrottlerParamsUpdaterDelegate(
            renderer_updater);
  }

  void ResetRendererUpdater() {
    CHECK(cookie_refresh_service_);
    cookie_refresh_service_
        ->SetRendererBoundSessionThrottlerParamsUpdaterDelegate(
            base::RepeatingClosure());
  }

  void ClearOriginData(uint32_t remove_mask,
                       const url::Origin& origin,
                       base::Time begin = base::Time::Now(),
                       base::Time end = base::Time::Now()) {
    CHECK(cookie_refresh_service_);
    cookie_refresh_service_->OnStorageKeyDataCleared(
        remove_mask,
        base::BindLambdaForTesting(
            [&origin](const blink::StorageKey& storage_key) {
              return storage_key.MatchesOriginForTrustedStorageDeletion(origin);
            }),
        begin, end);
  }

  void SimulateTerminateSession(SessionTerminationTrigger trigger) {
    CHECK(cookie_refresh_service_);
    cookie_refresh_service_->TerminateSession(trigger);
  }

  void VerifySessionTerminationTriggerRecorded(
      SessionTerminationTrigger trigger) {
    histogram_tester_.ExpectUniqueSample(
        "Signin.BoundSessionCredentials.SessionTerminationTrigger", trigger, 1);
  }

  void ResetCookieRefreshService() { cookie_refresh_service_.reset(); }

  base::WeakPtr<FakeBoundSessionCookieController> cookie_controller() {
    return cookie_controller_;
  }

  base::WeakPtr<FakeBoundSessionRegistrationFetcher> registration_fetcher() {
    return registration_fetcher_;
  }

  BoundSessionParamsStorage* storage() { return test_storage_.get(); }

  MockObserver* mock_observer() { return &mock_observer_; }

  // Emulates an existing session that resumes after `cookie_refresh_service_`
  // is created.
  void SetupPreConditionForBoundSession() {
    CHECK(!cookie_refresh_service_)
        << "If the cookie refresh service is already created, consider using "
           "`RegisterNewBoundSession()` to start a new bound session.";
    ASSERT_TRUE(storage()->SaveParams(CreateTestBoundSessionParams()));
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  void VerifyBoundSession(
      const bound_session_credentials::BoundSessionParams& expected_params) {
    CHECK(cookie_refresh_service_);
    EXPECT_TRUE(cookie_refresh_service_->GetBoundSessionThrottlerParams());
    EXPECT_THAT(storage()->ReadAllParams(),
                testing::Pointwise(TupleEqualsProto(), {expected_params}));
    ASSERT_TRUE(cookie_controller());

    EXPECT_EQ(cookie_controller()->session_id(), expected_params.session_id());
    EXPECT_EQ(cookie_controller()->url(), GURL(expected_params.site()));
    EXPECT_THAT(cookie_controller()->wrapped_key(),
                testing::ElementsAreArray(base::as_bytes(
                    base::make_span(expected_params.wrapped_key()))));
    EXPECT_THAT(cookie_controller()->bound_cookie_names(),
                testing::UnorderedPointwise(IsCookieCredential(),
                                            expected_params.credentials()));
  }

  void VerifyNoBoundSession() {
    CHECK(cookie_refresh_service_);
    EXPECT_FALSE(cookie_refresh_service_->GetBoundSessionThrottlerParams());
    EXPECT_FALSE(cookie_controller());
    EXPECT_THAT(storage()->ReadAllParams(), testing::IsEmpty());
  }

  bound_session_credentials::Credential CreateCookieCredential(
      const std::string& cookie_name) {
    bound_session_credentials::Credential credential;
    bound_session_credentials::CookieCredential* cookie_credential =
        credential.mutable_cookie_credential();
    cookie_credential->set_name(cookie_name);
    cookie_credential->set_domain(".google.com");
    cookie_credential->set_path("/");
    return credential;
  }

  bound_session_credentials::BoundSessionParams CreateTestBoundSessionParams() {
    static const std::vector<std::string> cookie_names = {"__Secure-1PSIDTS",
                                                          "__Secure-3PSIDTS"};

    bound_session_credentials::BoundSessionParams params;
    params.set_site(kTestGoogleURL.spec());
    params.set_session_id(kTestSessionId);
    params.set_wrapped_key(kWrappedKey);
    *params.mutable_creation_time() =
        bound_session_credentials::TimeToTimestamp(base::Time::Now());
    for (const std::string& cookie_name : cookie_names) {
      *params.add_credentials() = CreateCookieCredential(cookie_name);
    }
    return params;
  }

  BoundSessionRegistrationFetcherParam CreateTestRegistrationFetcherParams(
      std::string_view registration_path) {
    return BoundSessionRegistrationFetcherParam::CreateInstanceForTesting(
        kTestGoogleURL.Resolve(registration_path),
        {crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256},
        "test_challenge");
  }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

 private:
  std::unique_ptr<BoundSessionCookieRefreshServiceImpl>
  CreateBoundSessionCookieRefreshServiceImpl() {
    auto cookie_refresh_service =
        std::make_unique<BoundSessionCookieRefreshServiceImpl>(
            fake_unexportable_key_service_,
            BoundSessionParamsStorage::CreatePrefsStorageForTesting(prefs_),
            &storage_partition_, content::GetNetworkConnectionTracker(),
            /*is_off_the_record_profile=*/false);
    cookie_refresh_service->set_controller_factory_for_testing(
        base::BindRepeating(&BoundSessionCookieRefreshServiceImplTest::
                                CreateBoundSessionCookieController,
                            base::Unretained(this)));
    cookie_refresh_service->set_registration_fetcher_factory_for_testing(
        base::BindRepeating(&BoundSessionCookieRefreshServiceImplTest::
                                CreateBoundSessionRegistrationFetcher,
                            base::Unretained(this)));
    cookie_refresh_service->AddObserver(&mock_observer_);
    cookie_refresh_service->Initialize();
    return cookie_refresh_service;
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::HistogramTester histogram_tester_;
  sync_preferences::TestingPrefServiceSyncable prefs_;
  std::unique_ptr<BoundSessionParamsStorage> test_storage_;
  content::TestStoragePartition storage_partition_;
  ::testing::StrictMock<MockObserver> mock_observer_;
  std::unique_ptr<BoundSessionCookieRefreshServiceImpl> cookie_refresh_service_;
  unexportable_keys::FakeUnexportableKeyService fake_unexportable_key_service_;
  base::WeakPtr<FakeBoundSessionCookieController> cookie_controller_ = nullptr;
  base::WeakPtr<FakeBoundSessionRegistrationFetcher> registration_fetcher_ =
      nullptr;
};

TEST_F(BoundSessionCookieRefreshServiceImplTest, VerifyControllerParams) {
  SetupPreConditionForBoundSession();
  GetCookieRefreshServiceImpl();
  VerifyBoundSession(CreateTestBoundSessionParams());
}

TEST_F(BoundSessionCookieRefreshServiceImplTest,
       VerifyBoundSessionThrottlerParamsUnboundSession) {
  GetCookieRefreshServiceImpl();
  VerifyNoBoundSession();
}

TEST_F(BoundSessionCookieRefreshServiceImplTest,
       VerifyBoundSessionThrottlerParamsBoundSession) {
  SetupPreConditionForBoundSession();
  BoundSessionCookieRefreshServiceImpl* service = GetCookieRefreshServiceImpl();
  ASSERT_TRUE(cookie_controller());

  chrome::mojom::BoundSessionThrottlerParamsPtr bound_session_throttler_params =
      service->GetBoundSessionThrottlerParams();
  EXPECT_EQ(bound_session_throttler_params->domain, kTestGoogleURL.host());
  EXPECT_EQ(bound_session_throttler_params->path, kTestGoogleURL.path_piece());
}

TEST_F(BoundSessionCookieRefreshServiceImplTest,
       RefreshBoundSessionCookieBoundSession) {
  SetupPreConditionForBoundSession();
  BoundSessionCookieRefreshServiceImpl* service = GetCookieRefreshServiceImpl();
  EXPECT_TRUE(cookie_controller());
  base::test::TestFuture<ResumeBlockedRequestsTrigger> future;
  service->HandleRequestBlockedOnCookie(future.GetCallback());

  EXPECT_FALSE(future.IsReady());
  cookie_controller()->SimulateRefreshBoundSessionCompleted();
  EXPECT_TRUE(future.IsReady());
  EXPECT_EQ(future.Get(), kRefreshCompletedTrigger);
}

TEST_F(BoundSessionCookieRefreshServiceImplTest,
       RefreshBoundSessionCookieUnboundSession) {
  BoundSessionCookieRefreshServiceImpl* service = GetCookieRefreshServiceImpl();
  EXPECT_FALSE(cookie_controller());

  // Unbound session, the callback should be called immediately.
  base::test::TestFuture<ResumeBlockedRequestsTrigger> future;
  service->HandleRequestBlockedOnCookie(future.GetCallback());
  EXPECT_TRUE(future.IsReady());
  EXPECT_EQ(future.Get(),
            ResumeBlockedRequestsTrigger::kShutdownOrSessionTermination);
}

TEST_F(BoundSessionCookieRefreshServiceImplTest,
       UpdateAllRenderersOnBoundSessionStarted) {
  BoundSessionCookieRefreshServiceImpl* service = GetCookieRefreshServiceImpl();
  EXPECT_FALSE(cookie_controller());
  EXPECT_FALSE(service->GetBoundSessionThrottlerParams());
  base::MockRepeatingCallback<void()> renderer_updater;
  EXPECT_CALL(renderer_updater, Run()).Times(0);
  SetRendererUpdater(renderer_updater.Get());
  testing::Mock::VerifyAndClearExpectations(&renderer_updater);

  // Create bound session.
  EXPECT_CALL(renderer_updater, Run()).WillOnce([&] {
    EXPECT_TRUE(cookie_controller());
    EXPECT_FALSE(service->GetBoundSessionThrottlerParams().is_null());
  });
  service->RegisterNewBoundSession(CreateTestBoundSessionParams());
  testing::Mock::VerifyAndClearExpectations(&renderer_updater);
}

TEST_F(BoundSessionCookieRefreshServiceImplTest,
       UpdateAllRenderersOnBoundSessionThrottlerParamsChanged) {
  base::MockRepeatingCallback<void()> renderer_updater;
  EXPECT_CALL(renderer_updater, Run()).Times(0);
  SetupPreConditionForBoundSession();
  BoundSessionCookieRefreshServiceImpl* service = GetCookieRefreshServiceImpl();
  EXPECT_TRUE(cookie_controller());
  SetRendererUpdater(renderer_updater.Get());
  testing::Mock::VerifyAndClearExpectations(&renderer_updater);

  EXPECT_CALL(renderer_updater, Run()).Times(0);
  cookie_controller()->SimulateOnCookieExpirationDateChanged(k1PSIDTSCookieName,
                                                             base::Time::Now());
  testing::Mock::VerifyAndClearExpectations(&renderer_updater);

  EXPECT_CALL(renderer_updater, Run()).WillOnce([&] {
    EXPECT_TRUE(cookie_controller());
    EXPECT_FALSE(service->GetBoundSessionThrottlerParams().is_null());
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
  GetCookieRefreshServiceImpl();
  EXPECT_TRUE(cookie_controller());
  SetRendererUpdater(renderer_updater.Get());
  testing::Mock::VerifyAndClearExpectations(&renderer_updater);

  EXPECT_CALL(renderer_updater, Run()).WillOnce([&] {
    VerifyNoBoundSession();
  });
  EXPECT_CALL(
      *mock_observer(),
      OnBoundSessionTerminated(kTestGoogleURL,
                               base::flat_set<std::string>(
                                   {"__Secure-1PSIDTS", "__Secure-3PSIDTS"})))
      .Times(1);
  SimulateTerminateSession(
      SessionTerminationTrigger::kSessionTerminationHeader);
  testing::Mock::VerifyAndClearExpectations(&renderer_updater);
}

TEST_F(BoundSessionCookieRefreshServiceImplTest, TerminateSession) {
  SetupPreConditionForBoundSession();
  BoundSessionCookieRefreshServiceImpl* service = GetCookieRefreshServiceImpl();
  EXPECT_TRUE(service->GetBoundSessionThrottlerParams());

  EXPECT_CALL(
      *mock_observer(),
      OnBoundSessionTerminated(kTestGoogleURL,
                               base::flat_set<std::string>(
                                   {"__Secure-1PSIDTS", "__Secure-3PSIDTS"})))
      .Times(1);
  SimulateTerminateSession(
      SessionTerminationTrigger::kSessionTerminationHeader);
  VerifyNoBoundSession();
  VerifySessionTerminationTriggerRecorded(
      SessionTerminationTrigger::kSessionTerminationHeader);

  // Verify prefs were cleared.
  // Ensure on next startup, there won't be a bound session.
  ResetCookieRefreshService();
  service = GetCookieRefreshServiceImpl();

  SCOPED_TRACE("No bound session on Startup.");
  VerifyNoBoundSession();
}

TEST_F(BoundSessionCookieRefreshServiceImplTest,
       TerminateSessionOnPersistentErrorEncountered) {
  SetupPreConditionForBoundSession();
  BoundSessionCookieRefreshServiceImpl* service = GetCookieRefreshServiceImpl();
  EXPECT_TRUE(service->GetBoundSessionThrottlerParams());

  ASSERT_TRUE(cookie_controller());
  EXPECT_CALL(
      *mock_observer(),
      OnBoundSessionTerminated(kTestGoogleURL,
                               base::flat_set<std::string>(
                                   {"__Secure-1PSIDTS", "__Secure-3PSIDTS"})))
      .Times(1);
  cookie_controller()->SimulateOnPersistentErrorEncountered();

  VerifyNoBoundSession();
  VerifySessionTerminationTriggerRecorded(
      SessionTerminationTrigger::kCookieRotationPersistentError);

  // Verify prefs were cleared.
  // Ensure on next startup, there won't be a bound session.
  ResetCookieRefreshService();
  service = GetCookieRefreshServiceImpl();

  SCOPED_TRACE("No bound session on Startup.");
  VerifyNoBoundSession();
}

TEST_F(BoundSessionCookieRefreshServiceImplTest,
       TerminateSessionOnSessionTerminationHeader) {
  SetupPreConditionForBoundSession();
  scoped_refptr<net::HttpResponseHeaders> headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("");
  headers->AddHeader(kSessionTerminationHeader, kTestSessionId);
  BoundSessionCookieRefreshServiceImpl* service = GetCookieRefreshServiceImpl();
  EXPECT_CALL(
      *mock_observer(),
      OnBoundSessionTerminated(kTestGoogleURL,
                               base::flat_set<std::string>(
                                   {"__Secure-1PSIDTS", "__Secure-3PSIDTS"})))
      .Times(1);
  service->MaybeTerminateSession(headers.get());
  VerifyNoBoundSession();
  VerifySessionTerminationTriggerRecorded(
      SessionTerminationTrigger::kSessionTerminationHeader);
}

TEST_F(BoundSessionCookieRefreshServiceImplTest,
       DontTerminateSessionSessionIdsMismatch) {
  SetupPreConditionForBoundSession();
  scoped_refptr<net::HttpResponseHeaders> headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("");
  headers->AddHeader(kSessionTerminationHeader, "different_session_id");

  BoundSessionCookieRefreshServiceImpl* service = GetCookieRefreshServiceImpl();
  service->MaybeTerminateSession(headers.get());
  VerifyBoundSession(CreateTestBoundSessionParams());
  histogram_tester().ExpectTotalCount(
      "Signin.BoundSessionCredentials.SessionTerminationTrigger", 0);
}

TEST_F(BoundSessionCookieRefreshServiceImplTest,
       DontTerminateSessionWithoutSessionTerminationHeader) {
  SetupPreConditionForBoundSession();
  scoped_refptr<net::HttpResponseHeaders> headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("");

  BoundSessionCookieRefreshServiceImpl* service = GetCookieRefreshServiceImpl();
  service->MaybeTerminateSession(headers.get());
  VerifyBoundSession(CreateTestBoundSessionParams());
  histogram_tester().ExpectTotalCount(
      "Signin.BoundSessionCredentials.SessionTerminationTrigger", 0);
}

TEST_F(BoundSessionCookieRefreshServiceImplTest,
       AddBoundSessionRequestThrottledHandlerReceivers) {
  SetupPreConditionForBoundSession();
  BoundSessionCookieRefreshServiceImpl* service = GetCookieRefreshServiceImpl();
  ASSERT_TRUE(cookie_controller());
  mojo::Remote<chrome::mojom::BoundSessionRequestThrottledHandler> listener_1;
  mojo::Remote<chrome::mojom::BoundSessionRequestThrottledHandler> listener_2;
  service->AddBoundSessionRequestThrottledHandlerReceiver(
      listener_1.BindNewPipeAndPassReceiver());
  service->AddBoundSessionRequestThrottledHandlerReceiver(
      listener_2.BindNewPipeAndPassReceiver());

  base::test::TestFuture<ResumeBlockedRequestsTrigger> future_1;
  base::test::TestFuture<ResumeBlockedRequestsTrigger> future_2;
  listener_1->HandleRequestBlockedOnCookie(future_1.GetCallback());
  listener_2->HandleRequestBlockedOnCookie(future_2.GetCallback());
  RunUntilIdle();

  EXPECT_FALSE(future_1.IsReady());
  EXPECT_FALSE(future_2.IsReady());

  cookie_controller()->SimulateRefreshBoundSessionCompleted();
  EXPECT_TRUE(future_1.Wait());
  EXPECT_TRUE(future_2.Wait());
  EXPECT_EQ(future_1.Get(), kRefreshCompletedTrigger);
  EXPECT_EQ(future_2.Get(), kRefreshCompletedTrigger);
}

TEST_F(BoundSessionCookieRefreshServiceImplTest, RegisterNewBoundSession) {
  BoundSessionCookieRefreshServiceImpl* service = GetCookieRefreshServiceImpl();
  VerifyNoBoundSession();

  auto params = CreateTestBoundSessionParams();
  service->RegisterNewBoundSession(params);
  VerifyBoundSession(params);
}

TEST_F(BoundSessionCookieRefreshServiceImplTest, OverrideExistingBoundSession) {
  BoundSessionCookieRefreshServiceImpl* service = GetCookieRefreshServiceImpl();
  service->RegisterNewBoundSession(CreateTestBoundSessionParams());

  auto new_params = CreateTestBoundSessionParams();
  new_params.set_session_id("test_session_id_2");

  service->RegisterNewBoundSession(new_params);

  VerifyBoundSession(new_params);
  VerifySessionTerminationTriggerRecorded(
      SessionTerminationTrigger::kSessionOverride);
}

TEST_F(BoundSessionCookieRefreshServiceImplTest,
       OverrideExistingBoundSessionSameSessionId) {
  BoundSessionCookieRefreshServiceImpl* service = GetCookieRefreshServiceImpl();
  service->RegisterNewBoundSession(CreateTestBoundSessionParams());

  auto new_params = CreateTestBoundSessionParams();
  new_params.clear_credentials();
  *new_params.add_credentials() = CreateCookieCredential("new_cookie");

  service->RegisterNewBoundSession(new_params);

  VerifyBoundSession(new_params);
  VerifySessionTerminationTriggerRecorded(
      SessionTerminationTrigger::kSessionOverride);
}

TEST_F(BoundSessionCookieRefreshServiceImplTest,
       OverrideExistingBoundSessionWithInvalidParams) {
  BoundSessionCookieRefreshServiceImpl* service = GetCookieRefreshServiceImpl();
  auto original_params = CreateTestBoundSessionParams();
  service->RegisterNewBoundSession(original_params);

  auto invalid_params = CreateTestBoundSessionParams();
  invalid_params.clear_session_id();
  service->RegisterNewBoundSession(invalid_params);

  // Original session should not be modified.
  VerifyBoundSession(original_params);
  histogram_tester().ExpectTotalCount(
      "Signin.BoundSessionCredentials.SessionTerminationTrigger", 0);
}

TEST_F(BoundSessionCookieRefreshServiceImplTest, ClearMatchingData) {
  BoundSessionCookieRefreshServiceImpl* service = GetCookieRefreshServiceImpl();
  service->RegisterNewBoundSession(CreateTestBoundSessionParams());

  EXPECT_CALL(
      *mock_observer(),
      OnBoundSessionTerminated(kTestGoogleURL,
                               base::flat_set<std::string>(
                                   {"__Secure-1PSIDTS", "__Secure-3PSIDTS"})))
      .Times(1);
  ClearOriginData(content::StoragePartition::REMOVE_DATA_MASK_COOKIES,
                  url::Origin::Create(kTestGoogleURL));
  VerifyNoBoundSession();
  VerifySessionTerminationTriggerRecorded(
      SessionTerminationTrigger::kCookiesCleared);
}

TEST_F(BoundSessionCookieRefreshServiceImplTest,
       ClearMatchingDataTypeMismatch) {
  BoundSessionCookieRefreshServiceImpl* service = GetCookieRefreshServiceImpl();
  auto params = CreateTestBoundSessionParams();
  service->RegisterNewBoundSession(params);

  ClearOriginData(content::StoragePartition::REMOVE_DATA_MASK_CACHE_STORAGE,
                  url::Origin::Create(kTestGoogleURL));
  VerifyBoundSession(params);
  histogram_tester().ExpectTotalCount(
      "Signin.BoundSessionCredentials.SessionTerminationTrigger", 0);
}

TEST_F(BoundSessionCookieRefreshServiceImplTest,
       ClearMatchingDataOriginMismatch) {
  BoundSessionCookieRefreshServiceImpl* service = GetCookieRefreshServiceImpl();
  auto params = CreateTestBoundSessionParams();
  service->RegisterNewBoundSession(params);

  ClearOriginData(content::StoragePartition::REMOVE_DATA_MASK_COOKIES,
                  url::Origin::Create(GURL("https://example.org")));
  VerifyBoundSession(params);
  histogram_tester().ExpectTotalCount(
      "Signin.BoundSessionCredentials.SessionTerminationTrigger", 0);
}

TEST_F(BoundSessionCookieRefreshServiceImplTest,
       ClearMatchingDataOriginMismatchSuborigin) {
  BoundSessionCookieRefreshServiceImpl* service = GetCookieRefreshServiceImpl();
  auto params = CreateTestBoundSessionParams();
  service->RegisterNewBoundSession(params);

  ClearOriginData(content::StoragePartition::REMOVE_DATA_MASK_COOKIES,
                  url::Origin::Create(GURL("https://accounts.google.com")));
  VerifyBoundSession(params);
  histogram_tester().ExpectTotalCount(
      "Signin.BoundSessionCredentials.SessionTerminationTrigger", 0);
}

TEST_F(BoundSessionCookieRefreshServiceImplTest,
       ClearMatchingDataCreationTimeMismatch) {
  BoundSessionCookieRefreshServiceImpl* service = GetCookieRefreshServiceImpl();
  auto params = CreateTestBoundSessionParams();
  service->RegisterNewBoundSession(params);

  ClearOriginData(content::StoragePartition::REMOVE_DATA_MASK_COOKIES,
                  url::Origin::Create(kTestGoogleURL),
                  base::Time::Now() - base::Seconds(5),
                  base::Time::Now() - base::Seconds(3));
  VerifyBoundSession(params);
  histogram_tester().ExpectTotalCount(
      "Signin.BoundSessionCredentials.SessionTerminationTrigger", 0);
}

TEST_F(BoundSessionCookieRefreshServiceImplTest, CreateRegistrationRequest) {
  BoundSessionCookieRefreshServiceImpl* service = GetCookieRefreshServiceImpl();
  service->CreateRegistrationRequest(
      CreateTestRegistrationFetcherParams(kDefaultRegistrationPath));
  ASSERT_TRUE(registration_fetcher());
  bound_session_credentials::BoundSessionParams params =
      CreateTestBoundSessionParams();
  registration_fetcher()->SimulateRegistrationFetchCompleted(params);
  EXPECT_FALSE(registration_fetcher());
  VerifyBoundSession(params);
}

TEST_F(BoundSessionCookieRefreshServiceImplTest,
       CreateRegistrationRequestNonDefaultPath) {
  BoundSessionCookieRefreshServiceImpl* service = GetCookieRefreshServiceImpl();
  service->CreateRegistrationRequest(
      CreateTestRegistrationFetcherParams("/NonDefaultPath"));
  EXPECT_FALSE(registration_fetcher());
}

TEST_F(BoundSessionCookieRefreshServiceImplTest,
       CreateRegistrationRequestExclusivePathOff) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      switches::kEnableBoundSessionCredentials,
      {{"exclusive-registration-path", ""}});
  BoundSessionCookieRefreshServiceImpl* service = GetCookieRefreshServiceImpl();
  service->CreateRegistrationRequest(
      CreateTestRegistrationFetcherParams("/NonDefaultPath"));
  EXPECT_TRUE(registration_fetcher());
}

TEST_F(BoundSessionCookieRefreshServiceImplTest,
       CreateRegistrationRequestOverriddenExclusivePathMatchingPath) {
  base::test::ScopedFeatureList scoped_feature_list;
  const std::string kCustomPath = "/CustomPath";
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      switches::kEnableBoundSessionCredentials,
      {{"exclusive-registration-path", kCustomPath}});
  BoundSessionCookieRefreshServiceImpl* service = GetCookieRefreshServiceImpl();
  service->CreateRegistrationRequest(
      CreateTestRegistrationFetcherParams(kCustomPath));
  EXPECT_TRUE(registration_fetcher());
}

TEST_F(BoundSessionCookieRefreshServiceImplTest,
       CreateRegistrationRequestOverriddenExclusivePathNonMatchingPath) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      switches::kEnableBoundSessionCredentials,
      {{"exclusive-registration-path", "/CustomPath"}});
  BoundSessionCookieRefreshServiceImpl* service = GetCookieRefreshServiceImpl();
  service->CreateRegistrationRequest(
      CreateTestRegistrationFetcherParams("/AnotherPath"));
  EXPECT_FALSE(registration_fetcher());
}

TEST_F(BoundSessionCookieRefreshServiceImplTest,
       CreateRegistrationRequestMultipleRequests) {
  // Turn path restrictions off to test with two different paths.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      switches::kEnableBoundSessionCredentials,
      {{"exclusive-registration-path", ""}});

  BoundSessionCookieRefreshServiceImpl* service = GetCookieRefreshServiceImpl();
  const std::string kFirstPath = "/First";
  const std::string kSecondPath = "/Second";

  service->CreateRegistrationRequest(
      CreateTestRegistrationFetcherParams(kFirstPath));
  // The second registration request should be ignored.
  service->CreateRegistrationRequest(
      CreateTestRegistrationFetcherParams(kSecondPath));
  ASSERT_TRUE(registration_fetcher());
  EXPECT_EQ(
      registration_fetcher()->params().RegistrationEndpoint().path_piece(),
      kFirstPath);

  // Verify that a request can complete normally.
  bound_session_credentials::BoundSessionParams params =
      CreateTestBoundSessionParams();
  registration_fetcher()->SimulateRegistrationFetchCompleted(params);
  EXPECT_FALSE(registration_fetcher());
  VerifyBoundSession(params);
}
