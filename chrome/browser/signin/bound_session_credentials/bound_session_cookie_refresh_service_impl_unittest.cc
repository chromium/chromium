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
#include "chrome/common/renderer_configuration.mojom.h"
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

constexpr char k1PSIDTSCookieName[] = "__Secure-1PSIDTS";
constexpr char k3PSIDTSCookieName[] = "__Secure-3PSIDTS";
const char kSessionTerminationHeader[] = "Sec-Session-Google-Termination";
constexpr char kWrappedKey[] = "wrapped_key";
constexpr char kTestSessionId[] = "test_session_id";

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

  ~FakeBoundSessionCookieController() override {
    DCHECK(on_destroy_callback_);
    std::move(on_destroy_callback_).Run();
  }

  const std::vector<uint8_t>& wrapped_key() { return wrapped_key_; }

  void HandleRequestBlockedOnCookie(
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
      delegate_->OnBoundSessionThrottlerParamsChanged();
    }
  }

  void SimulateOnPersistentErrorEncountered() {
    delegate_->OnPersistentErrorEncountered();
  }

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
    std::unique_ptr<FakeBoundSessionCookieController> controller =
        std::make_unique<FakeBoundSessionCookieController>(bound_session_params,
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

  FakeBoundSessionCookieController* cookie_controller() {
    return cookie_controller_;
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

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

 private:
  std::unique_ptr<BoundSessionCookieRefreshServiceImpl>
  CreateBoundSessionCookieRefreshServiceImpl() {
    auto cookie_refresh_service =
        std::make_unique<BoundSessionCookieRefreshServiceImpl>(
            fake_unexportable_key_service_,
            BoundSessionParamsStorage::CreatePrefsStorageForTesting(prefs_),
            &storage_partition_, content::GetNetworkConnectionTracker());
    cookie_refresh_service->set_controller_factory_for_testing(
        base::BindRepeating(&BoundSessionCookieRefreshServiceImplTest::
                                CreateBoundSessionCookieController,
                            base::Unretained(this)));
    cookie_refresh_service->AddObserver(&mock_observer_);
    cookie_refresh_service->Initialize();
    return cookie_refresh_service;
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::HistogramTester histogram_tester_;
  sync_preferences::TestingPrefServiceSyncable prefs_;
  std::unique_ptr<BoundSessionParamsStorage> test_storage_;
  content::TestStoragePartition storage_partition_;
  ::testing::StrictMock<MockObserver> mock_observer_;
  std::unique_ptr<BoundSessionCookieRefreshServiceImpl> cookie_refresh_service_;
  unexportable_keys::FakeUnexportableKeyService fake_unexportable_key_service_;
  raw_ptr<FakeBoundSessionCookieController> cookie_controller_ = nullptr;
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
  base::test::TestFuture<void> future;
  service->HandleRequestBlockedOnCookie(future.GetCallback());

  EXPECT_FALSE(future.IsReady());
  cookie_controller()->SimulateRefreshBoundSessionCompleted();
  EXPECT_TRUE(future.IsReady());
}

TEST_F(BoundSessionCookieRefreshServiceImplTest,
       RefreshBoundSessionCookieUnboundSession) {
  BoundSessionCookieRefreshServiceImpl* service = GetCookieRefreshServiceImpl();
  EXPECT_FALSE(cookie_controller());

  // Unbound session, the callback should be called immediately.
  base::test::TestFuture<void> future;
  service->HandleRequestBlockedOnCookie(future.GetCallback());
  EXPECT_TRUE(future.IsReady());
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

  base::test::TestFuture<void> future_1;
  base::test::TestFuture<void> future_2;
  listener_1->HandleRequestBlockedOnCookie(future_1.GetCallback());
  listener_2->HandleRequestBlockedOnCookie(future_2.GetCallback());
  RunUntilIdle();

  EXPECT_FALSE(future_1.IsReady());
  EXPECT_FALSE(future_2.IsReady());

  cookie_controller()->SimulateRefreshBoundSessionCompleted();
  EXPECT_TRUE(future_1.Wait());
  EXPECT_TRUE(future_2.Wait());
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
