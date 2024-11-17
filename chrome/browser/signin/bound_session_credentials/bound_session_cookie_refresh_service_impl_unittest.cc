// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service_impl.h"

#include <memory>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_controller.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_params.pb.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_params_storage.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_params_util.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_refresh_cookie_fetcher.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_registration_fetcher.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_registration_fetcher_param.h"
#include "chrome/browser/signin/bound_session_credentials/fake_bound_session_refresh_cookie_fetcher.h"
#include "chrome/browser/signin/bound_session_credentials/rotation_debug_info.pb.h"
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
using testing::AllOf;
using testing::ElementsAre;
using testing::ElementsAreArray;
using testing::Eq;
using testing::Field;
using testing::IsEmpty;
using testing::IsFalse;
using testing::IsNull;
using testing::IsTrue;
using testing::Not;
using testing::NotNull;
using testing::Property;
using testing::ResultOf;
using testing::UnorderedPointwise;

constexpr char k1PSIDTSCookieName[] = "__Secure-1PSIDTS";
constexpr char k3PSIDTSCookieName[] = "__Secure-3PSIDTS";
const char kSessionTerminationHeader[] = "Sec-Session-Google-Termination";
constexpr char kWrappedKey[] = "wrapped_key";
constexpr char kTestSessionId[] = "test_session_id";
constexpr char kDefaultRegistrationPath[] = "/RegisterSession";
constexpr ResumeBlockedRequestsTrigger kRefreshCompletedTrigger =
    ResumeBlockedRequestsTrigger::kObservedFreshCookies;

// Matches a cookie name against a `bound_session_credentials::Credential` for
// use inside testing::Pointwise().
// `arg` type is std::tuple<std::string, bound_session_credentials::Credential>
MATCHER(IsCookieCredential, "") {
  const auto& [cookie_name, credential] = arg;
  if (!credential.has_cookie_credential()) {
    return false;
  }

  return cookie_name == credential.cookie_credential().name();
}

// Matches bound session throttler params against bound session params for use
// inside testing::Pointwise().
// `arg` type is std::tuple<BoundSessionThrottlerParamsPtr,
// bound_session_credentials::BoundSessionParams>
MATCHER(IsThrottlerParams, "") {
  const auto& [throttler_params, bound_session_params] = arg;

  GURL scope_url =
      bound_session_credentials::GetBoundSessionScope(bound_session_params);
  return throttler_params->domain == scope_url.host_piece() &&
         throttler_params->path == scope_url.path_piece();
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

  ~FakeBoundSessionCookieController() override {
    for (auto& callback : resume_blocked_requests_) {
      std::move(callback).Run(
          ResumeBlockedRequestsTrigger::kShutdownOrSessionTermination);
    }
  }

  const std::vector<uint8_t>& wrapped_key() const { return wrapped_key_; }

  void HandleRequestBlockedOnCookie(
      chrome::mojom::BoundSessionRequestThrottledHandler::
          HandleRequestBlockedOnCookieCallback resume_blocked_request)
      override {
    if (ShouldPauseThrottlingRequests()) {
      std::move(resume_blocked_request)
          .Run(ResumeBlockedRequestsTrigger::kThrottlingRequestsPaused);
      return;
    }
    resume_blocked_requests_.push_back(std::move(resume_blocked_request));
  }

  bound_session_credentials::RotationDebugInfo TakeDebugInfo() override {
    return {};
  }

  bool ShouldPauseThrottlingRequests() const override {
    return throttling_requests_paused_;
  }

  void SetThrottlingRequestsPaused(bool paused) {
    throttling_requests_paused_ = paused;
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
    delegate_->OnPersistentErrorEncountered(
        this, BoundSessionRefreshCookieFetcher::Result::kServerPersistentError);
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
  bool throttling_requests_paused_ = false;
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
    CHECK(callback);
    callback_ = std::move(callback);
  }

  bool HasStarted() { return !!callback_; }

 private:
  BoundSessionRegistrationFetcherParam params_;
  RegistrationCompleteCallback callback_;
  base::WeakPtrFactory<FakeBoundSessionRegistrationFetcher> weak_ptr_factory_{
      this};
};

// Matchers below have to appear after `FakeBoundSessionCookieController` and
// `FakeBoundSessionRegistrationFetcher` as they depend on the class definition.

// Matches a bound session cookie controller against bound session params.
// `arg` type is FakeBoundSessionCookieController*.
// `bound_session_params` type is
// `bound_session_credentials::BoundSessionParams`.
MATCHER_P(IsBoundSessionCookieController, bound_session_params, "") {
  return testing::ExplainMatchResult(
      AllOf(
          NotNull(),
          Property("session_id()",
                   &FakeBoundSessionCookieController::session_id,
                   bound_session_params.session_id()),
          Property("scope_url()", &FakeBoundSessionCookieController::scope_url,
                   bound_session_credentials::GetBoundSessionScope(
                       bound_session_params)),
          Property("site()", &FakeBoundSessionCookieController::site,
                   bound_session_params.site()),
          Property("wrapped_key()",
                   &FakeBoundSessionCookieController::wrapped_key,
                   ElementsAreArray(base::as_bytes(
                       base::make_span(bound_session_params.wrapped_key())))),
          Property("bound_cookie_names()",
                   &FakeBoundSessionCookieController::bound_cookie_names,
                   UnorderedPointwise(IsCookieCredential(),
                                      bound_session_params.credentials()))),
      arg, result_listener);
}

// Matches a map<BoundSessionKey, BoundSessionCookieController> element against
// bound session params for use inside testing::Pointwise().
// `arg` type is std::tuple<std::pair<BoundSessionKey,
// base::WeakPtr<FakeBoundSessionCookieController>>,
// bound_session_credentials::BoundSessionParams>
MATCHER(IsBoundSessionKeyAndControllerPair, "") {
  const auto& [map_entry, bound_session_params] = arg;
  const auto& [key, controller] = map_entry;

  return testing::ExplainMatchResult(
             Eq(GetBoundSessionKey(bound_session_params)), key,
             result_listener) &&
         testing::ExplainMatchResult(
             IsBoundSessionCookieController(bound_session_params),
             controller.get(), result_listener);
}

// Matches a bound session registration fetcher against the registration path
// and whether the registration fetch has started.
// `arg` type is base::WeakPtr<FakeBoundSessionRegistrationFetcher>.
// `registration_path` type is std::string.
// `has_started` type is bool.
MATCHER_P2(IsBoundSessionRegistrationFetcher,
           registration_path,
           has_started,
           "") {
  auto get_registration_path = [](const auto& fetcher) {
    return fetcher->params().registration_endpoint().path_piece();
  };
  auto fetcher_has_started = [](const auto& fetcher) {
    return fetcher->HasStarted();
  };
  return testing::ExplainMatchResult(
      AllOf(ResultOf(get_registration_path, Eq(registration_path)),
            ResultOf(fetcher_has_started, has_started)),
      arg, result_listener);
}

class FakeBoundSessionDebugReportFetcher
    : public BoundSessionRefreshCookieFetcher {
 public:
  void Start(
      RefreshCookieCompleteCallback callback,
      std::optional<std::string> sec_session_challenge_response) override {
    std::move(callback).Run(Result::kSuccess);
  }
  bool IsChallengeReceived() const override { return false; }
  std::optional<std::string> TakeSecSessionChallengeResponseIfAny() override {
    return std::nullopt;
  }
};

class MockObserver : public BoundSessionCookieRefreshService::Observer {
 public:
  MOCK_METHOD(void,
              OnBoundSessionTerminated,
              (const GURL& site,
               const base::flat_set<std::string>& bound_cookie_names),
              (override));
};

bound_session_credentials::Credential CreateCookieCredential(
    const std::string& cookie_name,
    const GURL& domain) {
  bound_session_credentials::Credential credential;
  bound_session_credentials::CookieCredential* cookie_credential =
      credential.mutable_cookie_credential();
  cookie_credential->set_name(cookie_name);
  cookie_credential->set_domain(base::StrCat({".", domain.host_piece()}));
  cookie_credential->set_path("/");
  return credential;
}

bound_session_credentials::BoundSessionParams CreateBoundSessionParams(
    const GURL& site,
    const std::string& session_id,
    const std::vector<std::string>& cookie_names) {
  bound_session_credentials::BoundSessionParams params;
  params.set_site(site.spec());
  params.set_session_id(session_id);
  params.set_wrapped_key(kWrappedKey);
  params.set_refresh_url(site.Resolve("/rotate").spec());
  *params.mutable_creation_time() =
      bound_session_credentials::TimeToTimestamp(base::Time::Now());
  for (const auto& cookie_name : cookie_names) {
    *params.add_credentials() = CreateCookieCredential(cookie_name, site);
  }
  return params;
}

bound_session_credentials::BoundSessionParams CreateBoundSessionParams(
    const BoundSessionKey& key,
    const std::vector<std::string>& cookie_names) {
  return CreateBoundSessionParams(key.site, key.session_id, cookie_names);
}

}  // namespace

class BoundSessionCookieRefreshServiceImplTestBase : public testing::Test {
 public:
  const GURL kTestGoogleURL = GURL("https://google.com");
  const GURL kTestYoutubeURL = GURL("https://youtube.com");
  const GURL kTestOtherURL = GURL("https://example.org");

  BoundSessionCookieRefreshServiceImplTestBase() {
    BoundSessionParamsStorage::RegisterProfilePrefs(prefs_.registry());
    test_storage_ =
        BoundSessionParamsStorage::CreatePrefsStorageForTesting(prefs_);
    EXPECT_CALL(*this, CreateBoundSessionDebugReportFetcher)
        .WillRepeatedly(testing::InvokeWithoutArgs([] {
          return std::make_unique<FakeBoundSessionDebugReportFetcher>();
        }));
  }

  ~BoundSessionCookieRefreshServiceImplTestBase() override = default;

  virtual std::unique_ptr<BoundSessionCookieController>
  CreateBoundSessionCookieController(
      const bound_session_credentials::BoundSessionParams& bound_session_params,
      BoundSessionCookieController::Delegate* delegate) = 0;

  virtual std::unique_ptr<BoundSessionRegistrationFetcher>
  CreateBoundSessionRegistrationFetcher(
      BoundSessionRegistrationFetcherParam fetcher_params) = 0;

  MOCK_METHOD(std::unique_ptr<BoundSessionRefreshCookieFetcher>,
              CreateBoundSessionDebugReportFetcher,
              (std::string_view session_id,
               const GURL& refresh_url,
               bool is_off_the_record_profile,
               bound_session_credentials::RotationDebugInfo debug_info),
              ());

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

  void SimulateTerminateSession(
      BoundSessionCookieController* controller,
      SessionTerminationTrigger trigger,
      std::optional<BoundSessionRefreshCookieFetcher::Result> refresh_error =
          std::nullopt) {
    CHECK(cookie_refresh_service_);
    cookie_refresh_service_->TerminateSession(controller, trigger,
                                              refresh_error);
  }

  void VerifySessionTerminationTriggerRecorded(
      SessionTerminationTrigger trigger) {
    histogram_tester_.ExpectUniqueSample(
        "Signin.BoundSessionCredentials.SessionTerminationTrigger", trigger, 1);
  }

  void ResetCookieRefreshService() { cookie_refresh_service_.reset(); }

  BoundSessionParamsStorage* storage() { return test_storage_.get(); }

  MockObserver* mock_observer() { return &mock_observer_; }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  BoundSessionRegistrationFetcherParam CreateTestRegistrationFetcherParams(
      std::string_view registration_path) {
    return BoundSessionRegistrationFetcherParam::CreateInstanceForTesting(
        kTestGoogleURL.Resolve(registration_path),
        {crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256},
        "test_challenge");
  }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

  BoundSessionCookieRefreshService* cookie_refresh_service() {
    return cookie_refresh_service_.get();
  }

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
        base::BindRepeating(&BoundSessionCookieRefreshServiceImplTestBase::
                                CreateBoundSessionCookieController,
                            base::Unretained(this)));
    cookie_refresh_service->set_registration_fetcher_factory_for_testing(
        base::BindRepeating(&BoundSessionCookieRefreshServiceImplTestBase::
                                CreateBoundSessionRegistrationFetcher,
                            base::Unretained(this)));
    cookie_refresh_service->set_debug_report_fetcher_factory_for_testing(
        base::BindRepeating(&BoundSessionCookieRefreshServiceImplTestBase::
                                CreateBoundSessionDebugReportFetcher,
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
};

// Test suite for testing a single session.
class BoundSessionCookieRefreshServiceImplTest
    : public BoundSessionCookieRefreshServiceImplTestBase {
 public:
  // Emulates an existing session that resumes after `cookie_refresh_service_`
  // is created.
  void SetupPreConditionForBoundSession() {
    CHECK(!cookie_refresh_service())
        << "If the cookie refresh service is already created, consider using "
           "`RegisterNewBoundSession()` to start a new bound session.";
    ASSERT_TRUE(storage()->SaveParams(CreateTestBoundSessionParams()));
  }

  std::unique_ptr<BoundSessionCookieController>
  CreateBoundSessionCookieController(
      const bound_session_credentials::BoundSessionParams& bound_session_params,
      BoundSessionCookieController::Delegate* delegate) override {
    auto controller = std::make_unique<FakeBoundSessionCookieController>(
        bound_session_params, delegate);
    cookie_controller_ = controller->GetWeakPtr();
    return controller;
  }

  std::unique_ptr<BoundSessionRegistrationFetcher>
  CreateBoundSessionRegistrationFetcher(
      BoundSessionRegistrationFetcherParam fetcher_params) override {
    auto fetcher = std::make_unique<FakeBoundSessionRegistrationFetcher>(
        std::move(fetcher_params));
    registration_fetcher_ = fetcher->GetWeakPtr();
    return fetcher;
  }

  bound_session_credentials::BoundSessionParams CreateTestBoundSessionParams() {
    return CreateBoundSessionParams(kTestGoogleURL, kTestSessionId,
                                    {k1PSIDTSCookieName, k3PSIDTSCookieName});
  }

  void VerifyBoundSession(
      const bound_session_credentials::BoundSessionParams& expected_params) {
    CHECK(cookie_refresh_service());
    EXPECT_THAT(cookie_refresh_service()->GetBoundSessionThrottlerParams(),
                UnorderedPointwise(IsThrottlerParams(), {expected_params}));
    EXPECT_THAT(storage()->ReadAllParamsAndCleanStorageIfNecessary(),
                ElementsAre(base::test::EqualsProto(expected_params)));
    EXPECT_THAT(cookie_controller().get(),
                IsBoundSessionCookieController(expected_params));
  }

  void VerifyNoBoundSession() {
    CHECK(cookie_refresh_service());
    EXPECT_THAT(cookie_refresh_service()->GetBoundSessionThrottlerParams(),
                IsEmpty());
    EXPECT_FALSE(cookie_controller());
    EXPECT_THAT(storage()->ReadAllParamsAndCleanStorageIfNecessary(),
                IsEmpty());
  }

  void SimulateTerminateSession(
      SessionTerminationTrigger trigger,
      std::optional<BoundSessionRefreshCookieFetcher::Result> refresh_error =
          std::nullopt) {
    BoundSessionCookieRefreshServiceImplTestBase::SimulateTerminateSession(
        cookie_controller_.get(), trigger, refresh_error);
  }

  base::WeakPtr<FakeBoundSessionCookieController> cookie_controller() {
    return cookie_controller_;
  }

  base::WeakPtr<FakeBoundSessionRegistrationFetcher> registration_fetcher() {
    return registration_fetcher_;
  }

 private:
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

  std::vector<chrome::mojom::BoundSessionThrottlerParamsPtr>
      bound_session_throttler_params =
          service->GetBoundSessionThrottlerParams();
  ASSERT_EQ(bound_session_throttler_params.size(), 1U);
  EXPECT_EQ(bound_session_throttler_params[0]->domain, kTestGoogleURL.host());
  EXPECT_EQ(bound_session_throttler_params[0]->path,
            kTestGoogleURL.path_piece());
}

TEST_F(BoundSessionCookieRefreshServiceImplTest,
       VerifyBoundSessionWithSubdomainScope) {
  bound_session_credentials::BoundSessionParams params =
      CreateBoundSessionParams(GURL("https://google.com"), kTestSessionId, {});
  *params.add_credentials() =
      CreateCookieCredential("cookieA", GURL("https://accounts.google.com"));
  *params.add_credentials() =
      CreateCookieCredential("cookieB", GURL("https://accounts.google.com"));

  ASSERT_TRUE(storage()->SaveParams(params));
  GetCookieRefreshServiceImpl();
  VerifyBoundSession(params);
}

TEST_F(BoundSessionCookieRefreshServiceImplTest,
       RefreshBoundSessionCookieBoundSession) {
  SetupPreConditionForBoundSession();
  BoundSessionCookieRefreshServiceImpl* service = GetCookieRefreshServiceImpl();
  EXPECT_TRUE(cookie_controller());
  base::test::TestFuture<ResumeBlockedRequestsTrigger> future;
  service->HandleRequestBlockedOnCookie(kTestGoogleURL, future.GetCallback());

  EXPECT_FALSE(future.IsReady());
  cookie_controller()->SimulateRefreshBoundSessionCompleted();
  EXPECT_TRUE(future.IsReady());
  EXPECT_EQ(future.Get(), kRefreshCompletedTrigger);
}

TEST_F(BoundSessionCookieRefreshServiceImplTest,
       RequestBlockedOnCookieThrottlingPaused) {
  SetupPreConditionForBoundSession();
  BoundSessionCookieRefreshServiceImpl* service = GetCookieRefreshServiceImpl();
  EXPECT_TRUE(cookie_controller());
  cookie_controller()->SetThrottlingRequestsPaused(true);
  base::test::TestFuture<ResumeBlockedRequestsTrigger> future;
  service->HandleRequestBlockedOnCookie(kTestGoogleURL, future.GetCallback());

  ASSERT_TRUE(future.IsReady());
  EXPECT_EQ(future.Get(),
            ResumeBlockedRequestsTrigger::kThrottlingRequestsPaused);
}

TEST_F(BoundSessionCookieRefreshServiceImplTest,
       RefreshBoundSessionCookieUnboundSession) {
  BoundSessionCookieRefreshServiceImpl* service = GetCookieRefreshServiceImpl();
  EXPECT_FALSE(cookie_controller());

  // Unbound session, the callback should be called immediately.
  base::test::TestFuture<ResumeBlockedRequestsTrigger> future;
  service->HandleRequestBlockedOnCookie(kTestGoogleURL, future.GetCallback());
  EXPECT_TRUE(future.IsReady());
  EXPECT_EQ(future.Get(),
            ResumeBlockedRequestsTrigger::kShutdownOrSessionTermination);
}

TEST_F(BoundSessionCookieRefreshServiceImplTest,
       UpdateAllRenderersOnBoundSessionStarted) {
  BoundSessionCookieRefreshServiceImpl* service = GetCookieRefreshServiceImpl();
  EXPECT_FALSE(cookie_controller());
  EXPECT_THAT(service->GetBoundSessionThrottlerParams(), IsEmpty());
  base::MockRepeatingCallback<void()> renderer_updater;
  EXPECT_CALL(renderer_updater, Run()).Times(0);
  SetRendererUpdater(renderer_updater.Get());
  testing::Mock::VerifyAndClearExpectations(&renderer_updater);

  // Create bound session.
  EXPECT_CALL(renderer_updater, Run()).WillOnce([&] {
    EXPECT_TRUE(cookie_controller());
    EXPECT_THAT(service->GetBoundSessionThrottlerParams(), Not(IsEmpty()));
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
    EXPECT_THAT(service->GetBoundSessionThrottlerParams(), Not(IsEmpty()));
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
                                   {k1PSIDTSCookieName, k3PSIDTSCookieName})))
      .Times(1);
  SimulateTerminateSession(
      SessionTerminationTrigger::kSessionTerminationHeader);
  testing::Mock::VerifyAndClearExpectations(&renderer_updater);
}

TEST_F(BoundSessionCookieRefreshServiceImplTest,
       SendDebugReportOnBoundSessionTerminated) {
  SetupPreConditionForBoundSession();
  GetCookieRefreshServiceImpl();
  EXPECT_TRUE(cookie_controller());

  EXPECT_CALL(
      *mock_observer(),
      OnBoundSessionTerminated(kTestGoogleURL,
                               base::flat_set<std::string>(
                                   {k1PSIDTSCookieName, k3PSIDTSCookieName})))
      .Times(1);
  bound_session_credentials::RotationDebugInfo sent_rotation_debug_info;
  EXPECT_CALL(*this, CreateBoundSessionDebugReportFetcher(
                         kTestSessionId, kTestGoogleURL.Resolve("/rotate"),
                         /*is_off_the_record_profile=*/false, testing::_))
      .WillOnce(testing::DoAll(
          testing::SaveArg<3>(&sent_rotation_debug_info),
          testing::InvokeWithoutArgs([] {
            return std::make_unique<FakeBoundSessionDebugReportFetcher>();
          })));
  SimulateTerminateSession(
      SessionTerminationTrigger::kCookieRotationPersistentError,
      BoundSessionRefreshCookieFetcher::Result::kSignChallengeFailed);
  EXPECT_EQ(sent_rotation_debug_info.termination_reason(),
            bound_session_credentials::RotationDebugInfo::
                ROTATION_SIGN_CHALLENGE_FAILED);
}

TEST_F(BoundSessionCookieRefreshServiceImplTest, TerminateSession) {
  SetupPreConditionForBoundSession();
  BoundSessionCookieRefreshServiceImpl* service = GetCookieRefreshServiceImpl();
  EXPECT_THAT(service->GetBoundSessionThrottlerParams(), Not(IsEmpty()));

  EXPECT_CALL(
      *mock_observer(),
      OnBoundSessionTerminated(kTestGoogleURL,
                               base::flat_set<std::string>(
                                   {k1PSIDTSCookieName, k3PSIDTSCookieName})))
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
  EXPECT_THAT(service->GetBoundSessionThrottlerParams(), Not(IsEmpty()));

  ASSERT_TRUE(cookie_controller());
  EXPECT_CALL(
      *mock_observer(),
      OnBoundSessionTerminated(kTestGoogleURL,
                               base::flat_set<std::string>(
                                   {k1PSIDTSCookieName, k3PSIDTSCookieName})))
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
                                   {k1PSIDTSCookieName, k3PSIDTSCookieName})))
      .Times(1);
  service->MaybeTerminateSession(GURL("https://google.com/SignOut"),
                                 headers.get());
  VerifyNoBoundSession();
  VerifySessionTerminationTriggerRecorded(
      SessionTerminationTrigger::kSessionTerminationHeader);
}

TEST_F(BoundSessionCookieRefreshServiceImplTest,
       TerminateSessionTerminationHeaderOnSubdomain) {
  SetupPreConditionForBoundSession();
  scoped_refptr<net::HttpResponseHeaders> headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("");
  headers->AddHeader(kSessionTerminationHeader, kTestSessionId);
  BoundSessionCookieRefreshServiceImpl* service = GetCookieRefreshServiceImpl();
  EXPECT_CALL(
      *mock_observer(),
      OnBoundSessionTerminated(kTestGoogleURL,
                               base::flat_set<std::string>(
                                   {k1PSIDTSCookieName, k3PSIDTSCookieName})))
      .Times(1);
  service->MaybeTerminateSession(
      GURL("https://accounts.google.com/accounts/SignOut"), headers.get());
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
  service->MaybeTerminateSession(kTestGoogleURL, headers.get());
  VerifyBoundSession(CreateTestBoundSessionParams());
  histogram_tester().ExpectTotalCount(
      "Signin.BoundSessionCredentials.SessionTerminationTrigger", 0);
}

TEST_F(BoundSessionCookieRefreshServiceImplTest,
       DontTerminateSessionSiteMismatch) {
  SetupPreConditionForBoundSession();
  scoped_refptr<net::HttpResponseHeaders> headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("");
  headers->AddHeader(kSessionTerminationHeader, kTestSessionId);

  BoundSessionCookieRefreshServiceImpl* service = GetCookieRefreshServiceImpl();
  // `kTestOtherURL` and the bound session URL are from different sites.
  service->MaybeTerminateSession(kTestOtherURL, headers.get());
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
  service->MaybeTerminateSession(kTestGoogleURL, headers.get());
  VerifyBoundSession(CreateTestBoundSessionParams());
  histogram_tester().ExpectTotalCount(
      "Signin.BoundSessionCredentials.SessionTerminationTrigger", 0);
}

TEST_F(BoundSessionCookieRefreshServiceImplTest,
       AddBoundSessionRequestThrottledHandlerReceivers) {
  SetupPreConditionForBoundSession();
  BoundSessionCookieRefreshServiceImpl* service = GetCookieRefreshServiceImpl();
  ASSERT_TRUE(cookie_controller());
  mojo::Remote<chrome::mojom::BoundSessionRequestThrottledHandler> handler_1;
  mojo::Remote<chrome::mojom::BoundSessionRequestThrottledHandler> handler_2;
  service->AddBoundSessionRequestThrottledHandlerReceiver(
      handler_1.BindNewPipeAndPassReceiver());
  service->AddBoundSessionRequestThrottledHandlerReceiver(
      handler_2.BindNewPipeAndPassReceiver());

  base::test::TestFuture<ResumeBlockedRequestsTrigger> future_1;
  base::test::TestFuture<ResumeBlockedRequestsTrigger> future_2;
  handler_1->HandleRequestBlockedOnCookie(kTestGoogleURL,
                                          future_1.GetCallback());
  handler_2->HandleRequestBlockedOnCookie(kTestGoogleURL,
                                          future_2.GetCallback());
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

// This test is specific to `kMultipleBoundSessionsEnabled` being disabled.
// BoundSessionCookieRefreshServiceImplMultiSessionTest.RegisterBoundSessionSameSessionKey
// tests a similar scenario with `kMultipleBoundSessionsEnabled` enabled.
TEST_F(BoundSessionCookieRefreshServiceImplTest, OverrideExistingBoundSession) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kMultipleBoundSessionsEnabled);
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
  *new_params.add_credentials() =
      CreateCookieCredential("new_cookie", kTestGoogleURL);

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
                                   {k1PSIDTSCookieName, k3PSIDTSCookieName})))
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
  EXPECT_THAT(registration_fetcher(), IsBoundSessionRegistrationFetcher(
                                          kDefaultRegistrationPath, true));
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
  const std::string kNonDefaultRegistrationPath = "/NonDefaultPath";
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      switches::kEnableBoundSessionCredentials,
      {{"exclusive-registration-path", ""}});
  BoundSessionCookieRefreshServiceImpl* service = GetCookieRefreshServiceImpl();
  service->CreateRegistrationRequest(
      CreateTestRegistrationFetcherParams(kNonDefaultRegistrationPath));
  ASSERT_TRUE(registration_fetcher());
  EXPECT_THAT(registration_fetcher(), IsBoundSessionRegistrationFetcher(
                                          kNonDefaultRegistrationPath, true));
}

TEST_F(BoundSessionCookieRefreshServiceImplTest,
       CreateRegistrationRequestOverriddenExclusivePathMatchingPath) {
  const std::string kCustomPath = "/CustomPath";
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      switches::kEnableBoundSessionCredentials,
      {{"exclusive-registration-path", kCustomPath}});
  BoundSessionCookieRefreshServiceImpl* service = GetCookieRefreshServiceImpl();
  service->CreateRegistrationRequest(
      CreateTestRegistrationFetcherParams(kCustomPath));
  ASSERT_TRUE(registration_fetcher());
  EXPECT_THAT(registration_fetcher(),
              IsBoundSessionRegistrationFetcher(kCustomPath, true));
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

// This test is specific to `kMultipleBoundSessionsEnabled` being disabled.
// BoundSessionCookieRefreshServiceImplMultiSessionTest.CreateRegistrationRequest
// tests a similar scenario with `kMultipleBoundSessionsEnabled` enabled.
TEST_F(BoundSessionCookieRefreshServiceImplTest,
       CreateRegistrationRequestMultipleRequests) {
  // Turn path restrictions off to test with two different paths.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeaturesAndParameters(
      {{switches::kEnableBoundSessionCredentials,
        {{"exclusive-registration-path", ""}}}},
      {kMultipleBoundSessionsEnabled});

  BoundSessionCookieRefreshServiceImpl* service = GetCookieRefreshServiceImpl();
  const std::string kFirstPath = "/First";
  const std::string kSecondPath = "/Second";

  service->CreateRegistrationRequest(
      CreateTestRegistrationFetcherParams(kFirstPath));
  // The second registration request should be ignored.
  service->CreateRegistrationRequest(
      CreateTestRegistrationFetcherParams(kSecondPath));
  ASSERT_TRUE(registration_fetcher());
  EXPECT_THAT(registration_fetcher(),
              IsBoundSessionRegistrationFetcher(kFirstPath, true));

  // Verify that a request can complete normally.
  bound_session_credentials::BoundSessionParams params =
      CreateTestBoundSessionParams();
  registration_fetcher()->SimulateRegistrationFetchCompleted(params);
  EXPECT_FALSE(registration_fetcher());
  VerifyBoundSession(params);
}

// Test suite for tests involving multiple sessions.
class BoundSessionCookieRefreshServiceImplMultiSessionTest
    : public BoundSessionCookieRefreshServiceImplTestBase {
 public:
  const BoundSessionKey kGoogleSessionKeyOne{.site = kTestGoogleURL,
                                             .session_id = "session_one"};
  const BoundSessionKey kGoogleSessionKeyTwo{.site = kTestGoogleURL,
                                             .session_id = "session_two"};
  const BoundSessionKey kYoutubeSessionKeyOne{.site = kTestYoutubeURL,
                                              .session_id = "session_one"};

  std::unique_ptr<BoundSessionCookieController>
  CreateBoundSessionCookieController(
      const bound_session_credentials::BoundSessionParams& bound_session_params,
      BoundSessionCookieController::Delegate* delegate) override {
    PruneDestroyedControllers();
    auto controller = std::make_unique<FakeBoundSessionCookieController>(
        bound_session_params, delegate);
    auto [it, inserted] = cookie_controllers_.emplace(
        controller->GetBoundSessionKey(), controller->GetWeakPtr());
    CHECK(inserted) << "Unexpected session override "
                    << bound_session_params.site() << " "
                    << bound_session_params.session_id();
    return controller;
  }

  std::unique_ptr<BoundSessionRegistrationFetcher>
  CreateBoundSessionRegistrationFetcher(
      BoundSessionRegistrationFetcherParam fetcher_params) override {
    auto fetcher = std::make_unique<FakeBoundSessionRegistrationFetcher>(
        std::move(fetcher_params));
    registration_fetchers_.push_back(fetcher->GetWeakPtr());
    return fetcher;
  }

  base::WeakPtr<FakeBoundSessionCookieController> GetCookieController(
      const BoundSessionKey& key) {
    CHECK(cookie_refresh_service());
    auto it = cookie_controllers_.find(key);
    if (it == cookie_controllers_.end()) {
      return nullptr;
    }
    return it->second;
  }

  // Erase controllers whose weak pointers were invalidated.
  void PruneDestroyedControllers() {
    base::EraseIf(cookie_controllers_, [](const auto& key_controller_pair) {
      return !key_controller_pair.second;
    });
  }

  void VerifyBoundSessions(
      const std::vector<bound_session_credentials::BoundSessionParams>&
          all_expected_params) {
    CHECK(cookie_refresh_service());

    // Verify throttler params.
    EXPECT_THAT(cookie_refresh_service()->GetBoundSessionThrottlerParams(),
                UnorderedPointwise(IsThrottlerParams(), all_expected_params));

    // Verify storage.
    EXPECT_THAT(
        storage()->ReadAllParamsAndCleanStorageIfNecessary(),
        UnorderedPointwise(base::test::EqualsProto(), all_expected_params));

    // Verify controllers.
    EXPECT_THAT(cookie_controllers_,
                UnorderedPointwise(IsBoundSessionKeyAndControllerPair(),
                                   all_expected_params));
  }

  std::vector<base::WeakPtr<FakeBoundSessionRegistrationFetcher>>&
  registration_fetchers() {
    return registration_fetchers_;
  }

 private:
  base::flat_map<BoundSessionKey,
                 base::WeakPtr<FakeBoundSessionCookieController>>
      cookie_controllers_;
  std::vector<base::WeakPtr<FakeBoundSessionRegistrationFetcher>>
      registration_fetchers_;

  base::test::ScopedFeatureList scoped_feature_list_{
      kMultipleBoundSessionsEnabled};
};

TEST_F(BoundSessionCookieRefreshServiceImplMultiSessionTest, Initialize) {
  std::vector<bound_session_credentials::BoundSessionParams> all_params = {
      CreateBoundSessionParams(kGoogleSessionKeyOne, {"cookieA", "cookieB"}),
      CreateBoundSessionParams(kGoogleSessionKeyTwo, {"cookieC"}),
      CreateBoundSessionParams(kYoutubeSessionKeyOne, {"cookieA"})};
  for (const auto& params : all_params) {
    ASSERT_TRUE(storage()->SaveParams(params));
  }
  GetCookieRefreshServiceImpl();
  VerifyBoundSessions(all_params);
}

TEST_F(BoundSessionCookieRefreshServiceImplMultiSessionTest,
       CreateRegistrationRequest) {
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
  service->CreateRegistrationRequest(
      CreateTestRegistrationFetcherParams(kSecondPath));
  EXPECT_THAT(
      registration_fetchers(),
      ElementsAre(IsBoundSessionRegistrationFetcher(kFirstPath, true),
                  IsBoundSessionRegistrationFetcher(kSecondPath, false)));
  // The test cannot continue if `registration_fetchers()` doesn't contain two
  // elements.
  ASSERT_EQ(registration_fetchers().size(), 2U);

  // Verify that the second registration request starts after the first one
  // completes.
  bound_session_credentials::BoundSessionParams first_params =
      CreateBoundSessionParams(kGoogleSessionKeyOne, {"cookieA", "cookieB"});
  registration_fetchers()[0]->SimulateRegistrationFetchCompleted(first_params);
  EXPECT_THAT(registration_fetchers(),
              ElementsAre(IsNull(), IsBoundSessionRegistrationFetcher(
                                        kSecondPath, true)));
  VerifyBoundSessions({first_params});

  // The second request can complete normally.
  bound_session_credentials::BoundSessionParams second_params =
      CreateBoundSessionParams(kGoogleSessionKeyTwo, {"cookieC"});
  registration_fetchers()[1]->SimulateRegistrationFetchCompleted(second_params);
  EXPECT_THAT(registration_fetchers(), ElementsAre(IsNull(), IsNull()));
  VerifyBoundSessions({first_params, second_params});
}

TEST_F(BoundSessionCookieRefreshServiceImplMultiSessionTest,
       HandleRequestBlockedOnCookieCrossDomains) {
  std::vector<bound_session_credentials::BoundSessionParams> all_params = {
      CreateBoundSessionParams(kGoogleSessionKeyOne, {"cookieA", "cookieB"}),
      CreateBoundSessionParams(kGoogleSessionKeyTwo, {"cookieC"}),
      CreateBoundSessionParams(kYoutubeSessionKeyOne, {"cookieA"})};
  for (const auto& params : all_params) {
    ASSERT_TRUE(storage()->SaveParams(params));
  }
  BoundSessionCookieRefreshServiceImpl* service = GetCookieRefreshServiceImpl();

  base::test::TestFuture<ResumeBlockedRequestsTrigger> future;
  service->HandleRequestBlockedOnCookie(kTestGoogleURL, future.GetCallback());
  EXPECT_FALSE(future.IsReady());

  GetCookieController(kGoogleSessionKeyOne)
      ->SimulateRefreshBoundSessionCompleted();
  EXPECT_FALSE(future.IsReady());

  GetCookieController(kGoogleSessionKeyTwo)
      ->SimulateRefreshBoundSessionCompleted();
  EXPECT_TRUE(future.IsReady());
  EXPECT_EQ(future.Get(), kRefreshCompletedTrigger);
}

TEST_F(BoundSessionCookieRefreshServiceImplMultiSessionTest,
       HandleRequestBlockedOnCookieOneError) {
  std::vector<bound_session_credentials::BoundSessionParams> all_params = {
      CreateBoundSessionParams(kGoogleSessionKeyOne, {"cookieA", "cookieB"}),
      CreateBoundSessionParams(kGoogleSessionKeyTwo, {"cookieC"})};
  for (const auto& params : all_params) {
    ASSERT_TRUE(storage()->SaveParams(params));
  }
  BoundSessionCookieRefreshServiceImpl* service = GetCookieRefreshServiceImpl();

  base::test::TestFuture<ResumeBlockedRequestsTrigger> future;
  service->HandleRequestBlockedOnCookie(kTestGoogleURL, future.GetCallback());
  EXPECT_FALSE(future.IsReady());

  GetCookieController(kGoogleSessionKeyOne)
      ->SimulateRefreshBoundSessionCompleted();
  EXPECT_FALSE(future.IsReady());

  EXPECT_CALL(*mock_observer(),
              OnBoundSessionTerminated(
                  kTestGoogleURL, base::flat_set<std::string>({"cookieC"})))
      .WillOnce([this] { PruneDestroyedControllers(); });
  GetCookieController(kGoogleSessionKeyTwo)
      ->SimulateOnPersistentErrorEncountered();
  ASSERT_TRUE(future.IsReady());
  EXPECT_EQ(future.Get(),
            ResumeBlockedRequestsTrigger::kShutdownOrSessionTermination);
}

TEST_F(BoundSessionCookieRefreshServiceImplMultiSessionTest,
       HandleRequestBlockedOnCookieOneThrottlingPaused) {
  std::vector<bound_session_credentials::BoundSessionParams> all_params = {
      CreateBoundSessionParams(kGoogleSessionKeyOne, {"cookieA", "cookieB"}),
      CreateBoundSessionParams(kGoogleSessionKeyTwo, {"cookieC"})};
  for (const auto& params : all_params) {
    ASSERT_TRUE(storage()->SaveParams(params));
  }

  BoundSessionCookieRefreshServiceImpl* service = GetCookieRefreshServiceImpl();
  GetCookieController(kGoogleSessionKeyOne)->SetThrottlingRequestsPaused(true);

  base::test::TestFuture<ResumeBlockedRequestsTrigger> future;
  service->HandleRequestBlockedOnCookie(kTestGoogleURL, future.GetCallback());
  EXPECT_FALSE(future.IsReady());

  GetCookieController(kGoogleSessionKeyTwo)
      ->SimulateRefreshBoundSessionCompleted();
  ASSERT_TRUE(future.IsReady());
  EXPECT_EQ(future.Get(),
            ResumeBlockedRequestsTrigger::kThrottlingRequestsPaused);
}

TEST_F(BoundSessionCookieRefreshServiceImplMultiSessionTest,
       HandleRequestBlockedOnCookieAllThrottlingPaused) {
  std::vector<bound_session_credentials::BoundSessionParams> all_params = {
      CreateBoundSessionParams(kGoogleSessionKeyOne, {"cookieA", "cookieB"}),
      CreateBoundSessionParams(kGoogleSessionKeyTwo, {"cookieC"})};
  for (const auto& params : all_params) {
    ASSERT_TRUE(storage()->SaveParams(params));
  }

  BoundSessionCookieRefreshServiceImpl* service = GetCookieRefreshServiceImpl();
  GetCookieController(kGoogleSessionKeyOne)->SetThrottlingRequestsPaused(true);
  GetCookieController(kGoogleSessionKeyTwo)->SetThrottlingRequestsPaused(true);

  base::test::TestFuture<ResumeBlockedRequestsTrigger> future;
  service->HandleRequestBlockedOnCookie(kTestGoogleURL, future.GetCallback());
  ASSERT_TRUE(future.IsReady());
  EXPECT_EQ(future.Get(),
            ResumeBlockedRequestsTrigger::kThrottlingRequestsPaused);
}

TEST_F(BoundSessionCookieRefreshServiceImplMultiSessionTest,
       HandleRequestBlockedOnCookieOneExpired) {
  std::vector<bound_session_credentials::BoundSessionParams> all_params = {
      CreateBoundSessionParams(kGoogleSessionKeyOne, {"cookieA", "cookieB"}),
      CreateBoundSessionParams(kGoogleSessionKeyTwo, {"cookieC"})};
  for (const auto& params : all_params) {
    ASSERT_TRUE(storage()->SaveParams(params));
  }
  BoundSessionCookieRefreshServiceImpl* service = GetCookieRefreshServiceImpl();
  // Mark the second session as fresh.
  GetCookieController(kGoogleSessionKeyTwo)
      ->SimulateOnCookieExpirationDateChanged(
          "cookieC", base::Time::Now() + base::Minutes(10));

  base::test::TestFuture<ResumeBlockedRequestsTrigger> future;
  service->HandleRequestBlockedOnCookie(kTestGoogleURL, future.GetCallback());
  EXPECT_FALSE(future.IsReady());

  GetCookieController(kGoogleSessionKeyOne)
      ->SimulateRefreshBoundSessionCompleted();
  ASSERT_TRUE(future.IsReady());
  EXPECT_EQ(future.Get(), kRefreshCompletedTrigger);
}

TEST_F(BoundSessionCookieRefreshServiceImplMultiSessionTest,
       HandleRequestBlockedOnCookieZeroExpired) {
  std::vector<bound_session_credentials::BoundSessionParams> all_params = {
      CreateBoundSessionParams(kGoogleSessionKeyOne, {"cookieA", "cookieB"}),
      CreateBoundSessionParams(kGoogleSessionKeyTwo, {"cookieC"})};
  for (const auto& params : all_params) {
    ASSERT_TRUE(storage()->SaveParams(params));
  }
  BoundSessionCookieRefreshServiceImpl* service = GetCookieRefreshServiceImpl();
  // Mark both sessions as fresh.
  GetCookieController(kGoogleSessionKeyOne)
      ->SimulateOnCookieExpirationDateChanged(
          "cookieA", base::Time::Now() + base::Minutes(10));
  GetCookieController(kGoogleSessionKeyOne)
      ->SimulateOnCookieExpirationDateChanged(
          "cookieB", base::Time::Now() + base::Minutes(10));
  GetCookieController(kGoogleSessionKeyTwo)
      ->SimulateOnCookieExpirationDateChanged(
          "cookieC", base::Time::Now() + base::Minutes(10));

  base::test::TestFuture<ResumeBlockedRequestsTrigger> future;
  service->HandleRequestBlockedOnCookie(kTestGoogleURL, future.GetCallback());
  ASSERT_TRUE(future.IsReady());
  EXPECT_EQ(future.Get(), ResumeBlockedRequestsTrigger::kCookieAlreadyFresh);
}

TEST_F(BoundSessionCookieRefreshServiceImplMultiSessionTest,
       HandleRequestBlockedOnCookieNotCovered) {
  // No youtube.com session.
  std::vector<bound_session_credentials::BoundSessionParams> all_params = {
      CreateBoundSessionParams(kGoogleSessionKeyOne, {"cookieA", "cookieB"}),
      CreateBoundSessionParams(kGoogleSessionKeyTwo, {"cookieC"})};
  for (const auto& params : all_params) {
    ASSERT_TRUE(storage()->SaveParams(params));
  }
  BoundSessionCookieRefreshServiceImpl* service = GetCookieRefreshServiceImpl();

  base::test::TestFuture<ResumeBlockedRequestsTrigger> future;
  // This request is not covered by any session.
  service->HandleRequestBlockedOnCookie(kTestYoutubeURL, future.GetCallback());
  EXPECT_TRUE(future.IsReady());
  EXPECT_EQ(future.Get(),
            ResumeBlockedRequestsTrigger::kShutdownOrSessionTermination);
}

TEST_F(BoundSessionCookieRefreshServiceImplMultiSessionTest,
       HandleRequestBlockedOnCookieNoSessions) {
  BoundSessionCookieRefreshServiceImpl* service = GetCookieRefreshServiceImpl();

  base::test::TestFuture<ResumeBlockedRequestsTrigger> future;
  service->HandleRequestBlockedOnCookie(kTestGoogleURL, future.GetCallback());
  ASSERT_TRUE(future.IsReady());
  EXPECT_EQ(future.Get(),
            ResumeBlockedRequestsTrigger::kShutdownOrSessionTermination);
}

TEST_F(BoundSessionCookieRefreshServiceImplMultiSessionTest,
       HandleRequestBlockedOnCookieServiceShutdown) {
  std::vector<bound_session_credentials::BoundSessionParams> all_params = {
      CreateBoundSessionParams(kGoogleSessionKeyOne, {"cookieA", "cookieB"}),
      CreateBoundSessionParams(kGoogleSessionKeyTwo, {"cookieC"}),
      CreateBoundSessionParams(kYoutubeSessionKeyOne, {"cookieA"})};
  for (const auto& params : all_params) {
    ASSERT_TRUE(storage()->SaveParams(params));
  }
  BoundSessionCookieRefreshServiceImpl* service = GetCookieRefreshServiceImpl();

  base::test::TestFuture<ResumeBlockedRequestsTrigger> future;
  service->HandleRequestBlockedOnCookie(kTestGoogleURL, future.GetCallback());
  EXPECT_FALSE(future.IsReady());

  ResetCookieRefreshService();
  EXPECT_TRUE(future.IsReady());
  EXPECT_EQ(future.Get(),
            ResumeBlockedRequestsTrigger::kShutdownOrSessionTermination);
}

TEST_F(BoundSessionCookieRefreshServiceImplMultiSessionTest,
       RegisterNewBoundSession) {
  BoundSessionCookieRefreshServiceImpl* service = GetCookieRefreshServiceImpl();
  VerifyBoundSessions({});

  auto params =
      CreateBoundSessionParams(kGoogleSessionKeyOne, {"cookieA", "cookieB"});
  service->RegisterNewBoundSession(params);
  VerifyBoundSessions({params});
}

TEST_F(BoundSessionCookieRefreshServiceImplMultiSessionTest,
       RegisterSecondBoundSessionSameDomainDifferentSessionIds) {
  BoundSessionCookieRefreshServiceImpl* service = GetCookieRefreshServiceImpl();
  auto first_params =
      CreateBoundSessionParams(kGoogleSessionKeyOne, {"cookieA", "cookieB"});
  service->RegisterNewBoundSession(first_params);

  auto second_params =
      CreateBoundSessionParams(kGoogleSessionKeyTwo, {"cookieC"});
  service->RegisterNewBoundSession(second_params);
  VerifyBoundSessions({first_params, second_params});
}

TEST_F(BoundSessionCookieRefreshServiceImplMultiSessionTest,
       RegisterSecondBoundSessionSameSessionIdDifferentDomains) {
  BoundSessionCookieRefreshServiceImpl* service = GetCookieRefreshServiceImpl();
  auto first_params =
      CreateBoundSessionParams(kGoogleSessionKeyOne, {"cookieA", "cookieB"});
  service->RegisterNewBoundSession(first_params);

  auto second_params =
      CreateBoundSessionParams(kYoutubeSessionKeyOne, {"cookieC"});
  service->RegisterNewBoundSession(second_params);
  VerifyBoundSessions({first_params, second_params});
}

TEST_F(BoundSessionCookieRefreshServiceImplMultiSessionTest,
       RegisterBoundSessionSameSessionKey) {
  BoundSessionCookieRefreshServiceImpl* service = GetCookieRefreshServiceImpl();
  auto other_params =
      CreateBoundSessionParams(kGoogleSessionKeyTwo, {"cookieX"});
  service->RegisterNewBoundSession(other_params);
  auto params_to_be_overridden =
      CreateBoundSessionParams(kGoogleSessionKeyOne, {"cookieA", "cookieB"});
  service->RegisterNewBoundSession(params_to_be_overridden);

  auto new_params =
      CreateBoundSessionParams(kGoogleSessionKeyOne, {"cookieA", "cookieD"});
  service->RegisterNewBoundSession(new_params);
  VerifyBoundSessions({new_params, other_params});
  VerifySessionTerminationTriggerRecorded(
      SessionTerminationTrigger::kSessionOverride);
}

TEST_F(BoundSessionCookieRefreshServiceImplMultiSessionTest,
       TerminateSessionOnPersistentErrorEncountered) {
  std::vector<bound_session_credentials::BoundSessionParams> all_params = {
      CreateBoundSessionParams(kGoogleSessionKeyOne, {"cookieA", "cookieB"}),
      CreateBoundSessionParams(kGoogleSessionKeyTwo, {"cookieC"}),
      CreateBoundSessionParams(kYoutubeSessionKeyOne, {"cookieA"})};
  for (const auto& params : all_params) {
    ASSERT_TRUE(storage()->SaveParams(params));
  }
  GetCookieRefreshServiceImpl();

  EXPECT_CALL(
      *mock_observer(),
      OnBoundSessionTerminated(
          kTestGoogleURL, base::flat_set<std::string>({"cookieA", "cookieB"})))
      .WillOnce([this] { PruneDestroyedControllers(); });
  GetCookieController(kGoogleSessionKeyOne)
      ->SimulateOnPersistentErrorEncountered();
  // all_params[0] should have been terminated.
  VerifyBoundSessions({all_params[1], all_params[2]});
  VerifySessionTerminationTriggerRecorded(
      SessionTerminationTrigger::kCookieRotationPersistentError);
}

TEST_F(BoundSessionCookieRefreshServiceImplMultiSessionTest,
       TerminateSessionOnSessionTerminationHeader) {
  std::vector<bound_session_credentials::BoundSessionParams> all_params = {
      CreateBoundSessionParams(kGoogleSessionKeyOne, {"cookieA", "cookieB"}),
      CreateBoundSessionParams(kGoogleSessionKeyTwo, {"cookieC"}),
      CreateBoundSessionParams(kYoutubeSessionKeyOne, {"cookieA"})};
  for (const auto& params : all_params) {
    ASSERT_TRUE(storage()->SaveParams(params));
  }
  scoped_refptr<net::HttpResponseHeaders> headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("");
  headers->AddHeader(kSessionTerminationHeader,
                     kGoogleSessionKeyOne.session_id);

  EXPECT_CALL(
      *mock_observer(),
      OnBoundSessionTerminated(
          kTestGoogleURL, base::flat_set<std::string>({"cookieA", "cookieB"})))
      .WillOnce([this] { PruneDestroyedControllers(); });
  BoundSessionCookieRefreshServiceImpl* service = GetCookieRefreshServiceImpl();
  service->MaybeTerminateSession(GURL("https://google.com/SignOut"),
                                 headers.get());
  // all_params[0] should have been terminated.
  VerifyBoundSessions({all_params[1], all_params[2]});
  VerifySessionTerminationTriggerRecorded(
      SessionTerminationTrigger::kSessionTerminationHeader);
}

TEST_F(BoundSessionCookieRefreshServiceImplMultiSessionTest,
       TerminateSessionOnClearBrowsingData) {
  std::vector<bound_session_credentials::BoundSessionParams> all_params = {
      CreateBoundSessionParams(kGoogleSessionKeyOne, {"cookieA", "cookieB"}),
      CreateBoundSessionParams(kGoogleSessionKeyTwo, {"cookieC"}),
      CreateBoundSessionParams(kYoutubeSessionKeyOne, {"cookieA"})};
  for (const auto& params : all_params) {
    ASSERT_TRUE(storage()->SaveParams(params));
  }
  GetCookieRefreshServiceImpl();

  EXPECT_CALL(
      *mock_observer(),
      OnBoundSessionTerminated(
          kTestGoogleURL, base::flat_set<std::string>({"cookieA", "cookieB"})))
      .WillOnce([this] { PruneDestroyedControllers(); });
  EXPECT_CALL(*mock_observer(),
              OnBoundSessionTerminated(
                  kTestGoogleURL, base::flat_set<std::string>({"cookieC"})))
      .WillOnce([this] { PruneDestroyedControllers(); });
  ClearOriginData(content::StoragePartition::REMOVE_DATA_MASK_COOKIES,
                  url::Origin::Create(kTestGoogleURL));
  // all_params[0] and all_params[1] should have been terminated.
  VerifyBoundSessions({all_params[2]});
  histogram_tester().ExpectUniqueSample(
      "Signin.BoundSessionCredentials.SessionTerminationTrigger",
      SessionTerminationTrigger::kCookiesCleared, 2);
}
