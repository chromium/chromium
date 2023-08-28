// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_controller_impl.h"

#include <memory>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_controller.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_observer.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_refresh_cookie_fetcher.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_test_cookie_manager.h"
#include "chrome/browser/signin/bound_session_credentials/fake_bound_session_refresh_cookie_fetcher.h"
#include "chrome/browser/signin/bound_session_credentials/session_binding_helper.h"
#include "components/unexportable_keys/service_error.h"
#include "components/unexportable_keys/unexportable_key_id.h"
#include "components/unexportable_keys/unexportable_key_loader.h"
#include "components/unexportable_keys/unexportable_key_service_impl.h"
#include "components/unexportable_keys/unexportable_key_task_manager.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/test/test_storage_partition.h"
#include "crypto/scoped_mock_unexportable_key_provider.h"
#include "crypto/signature_verifier.h"
#include "net/cookies/canonical_cookie.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using unexportable_keys::ServiceErrorOr;
using unexportable_keys::UnexportableKeyId;

namespace {
constexpr char k1PSIDTSCookieName[] = "__Secure-1PSIDTS";
constexpr char k3PSIDTSCookieName[] = "__Secure-3PSIDTS";

const base::TimeDelta kCookieExpirationThreshold = base::Seconds(15);
const base::TimeDelta kCookieRefreshInterval = base::Minutes(2);
const base::TimeDelta kResumeBlockedRequestTimeout = base::Seconds(20);

base::Time GetTimeInTenMinutes() {
  return base::Time::Now() + base::Minutes(10);
}
}  // namespace

class BoundSessionCookieControllerImplTest
    : public testing::Test,
      public BoundSessionCookieController::Delegate {
 public:
  BoundSessionCookieControllerImplTest()
      : unexportable_key_service_(unexportable_key_task_manager_),
        key_id_(GenerateNewKey()) {

    std::vector<uint8_t> wrapped_key = GetWrappedKey(key_id_);
    bound_session_credentials::BoundSessionParams bound_session_params;
    bound_session_params.set_site("https://google.com");
    bound_session_params.set_session_id("test_session_id");
    bound_session_params.set_wrapped_key(
        std::string(wrapped_key.begin(), wrapped_key.end()));

    storage_partition_.set_cookie_manager_for_browser_process(&cookie_manager_);

    SetUpNetworkConnection(true,
                           network::mojom::ConnectionType::CONNECTION_WIFI);

    bound_session_cookie_controller_ =
        std::make_unique<BoundSessionCookieControllerImpl>(
            unexportable_key_service_, &storage_partition_,
            content::GetNetworkConnectionTracker(), bound_session_params,
            base::flat_set<std::string>(
                {k1PSIDTSCookieName, k3PSIDTSCookieName}),
            this);

    bound_session_cookie_controller_
        ->set_refresh_cookie_fetcher_factory_for_testing(
            base::BindRepeating(&BoundSessionCookieControllerImplTest::
                                    CreateBoundSessionRefreshCookieFetcher,
                                base::Unretained(this)));
    bound_session_cookie_controller_->Initialize();
  }

  ~BoundSessionCookieControllerImplTest() override = default;

  UnexportableKeyId GenerateNewKey() {
    base::test::TestFuture<ServiceErrorOr<UnexportableKeyId>> generate_future;
    unexportable_key_service_.GenerateSigningKeySlowlyAsync(
        base::span<const crypto::SignatureVerifier::SignatureAlgorithm>(
            {crypto::SignatureVerifier::ECDSA_SHA256}),
        unexportable_keys::BackgroundTaskPriority::kUserBlocking,
        generate_future.GetCallback());
    ServiceErrorOr<unexportable_keys::UnexportableKeyId> key_id =
        generate_future.Get();
    CHECK(key_id.has_value());
    return *key_id;
  }

  std::vector<uint8_t> GetWrappedKey(const UnexportableKeyId& key_id) {
    ServiceErrorOr<std::vector<uint8_t>> wrapped_key =
        unexportable_key_service_.GetWrappedKey(key_id);
    CHECK(wrapped_key.has_value());
    return *wrapped_key;
  }

  std::unique_ptr<BoundSessionRefreshCookieFetcher>
  CreateBoundSessionRefreshCookieFetcher(
      network::mojom::CookieManager* cookie_manager,
      const GURL& url,
      base::flat_set<std::string> cookie_names) {
    // `SimulateCompleteRefreshRequest()` must be called for the
    return std::make_unique<FakeBoundSessionRefreshCookieFetcher>(
        cookie_manager, url, std::move(cookie_names));
  }

  void MaybeRefreshCookie() {
    bound_session_cookie_controller_->MaybeRefreshCookie();
  }

  bool AreAllCookiesFresh() {
    return bound_session_cookie_controller_->AreAllCookiesFresh();
  }

  bool CompletePendingRefreshRequestIfAny() {
    if (!cookie_fetcher()) {
      return false;
    }
    SimulateCompleteRefreshRequest(
        BoundSessionRefreshCookieFetcher::Result::kSuccess,
        GetTimeInTenMinutes());
    task_environment_.RunUntilIdle();
    return true;
  }

  void SimulateCompleteRefreshRequest(
      BoundSessionRefreshCookieFetcher::Result result,
      absl::optional<base::Time> cookie_expiration) {
    ASSERT_TRUE(cookie_fetcher());
    FakeBoundSessionRefreshCookieFetcher* fetcher =
        static_cast<FakeBoundSessionRefreshCookieFetcher*>(cookie_fetcher());
    fetcher->SimulateCompleteRefreshRequest(result, cookie_expiration);
  }

  void SimulateCookieChange(const std::string& cookie_name,
                            absl::optional<base::Time> cookie_expiration) {
    net::CanonicalCookie cookie = BoundSessionTestCookieManager::CreateCookie(
        bound_session_cookie_controller()->url(), cookie_name,
        cookie_expiration);
    cookie_observer(cookie_name)
        ->OnCookieChange(
            net::CookieChangeInfo(cookie, net::CookieAccessResult(),
                                  net::CookieChangeCause::INSERTED));
  }

  base::test::TaskEnvironment* task_environment() { return &task_environment_; }

  void OnBoundSessionThrottlerParamsChanged() override {
    on_bound_session_throttler_params_changed_call_count_++;
  }

  void TerminateSession() override { on_terminate_session_called_ = true; }

  void SetExpirationTimeAndNotify(const std::string& cookie_name,
                                  const base::Time& expiration_time) {
    bound_session_cookie_controller_->SetCookieExpirationTimeAndNotify(
        cookie_name, expiration_time);
  }

  BoundSessionCookieControllerImpl* bound_session_cookie_controller() {
    return bound_session_cookie_controller_.get();
  }

  BoundSessionRefreshCookieFetcher* cookie_fetcher() {
    return bound_session_cookie_controller_->refresh_cookie_fetcher_.get();
  }

  std::vector<std::unique_ptr<BoundSessionCookieObserver>>*
  bound_cookies_observers() {
    return &bound_session_cookie_controller()->bound_cookies_observers_;
  }

  BoundSessionCookieObserver* cookie_observer(const std::string& cookie_name) {
    for (auto& observer : *bound_cookies_observers()) {
      if (observer->cookie_name_ == cookie_name) {
        return observer.get();
      }
    }
    NOTREACHED_NORETURN() << "No observer found for " << cookie_name;
  }

  base::Time cookie_expiration_time(const std::string& cookie_name) {
    auto& bound_cookies_info =
        bound_session_cookie_controller()->bound_cookies_info_;
    auto it = bound_cookies_info.find(cookie_name);
    CHECK(it != bound_cookies_info.end())
        << "No cookie found for " << cookie_name;
    return it->second;
  }

  base::OneShotTimer* preemptive_cookie_refresh_timer() {
    return &bound_session_cookie_controller()->preemptive_cookie_refresh_timer_;
  }

  base::OneShotTimer* resume_blocked_requests_timer() {
    return &bound_session_cookie_controller()->resume_blocked_requests_timer_;
  }

  unexportable_keys::UnexportableKeyLoader* key_loader() {
    return bound_session_cookie_controller()
        ->session_binding_helper_->key_loader_.get();
  }

  const UnexportableKeyId& key_id() { return key_id_; }

  size_t on_bound_session_throttler_params_changed_call_count() {
    return on_bound_session_throttler_params_changed_call_count_;
  }

  bool on_cookie_refresh_persistent_failure_called() {
    return on_terminate_session_called_;
  }

  void ResetOnBoundSessionThrottlerParamsChangedCallCount() {
    on_bound_session_throttler_params_changed_call_count_ = 0;
  }

  void ResetBoundSessionCookieController() {
    bound_session_cookie_controller_.reset();
  }

  void SetUpNetworkConnection(bool respond_synchronously,
                              network::mojom::ConnectionType connection_type) {
    network::TestNetworkConnectionTracker* tracker =
        network::TestNetworkConnectionTracker::GetInstance();
    tracker->SetRespondSynchronously(respond_synchronously);
    tracker->SetConnectionType(connection_type);
  }

  void SetConnectionType(network::mojom::ConnectionType connection_type) {
    network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
        connection_type);
  }

  bool IsConnectionTypeAvailableAndOffline() {
    CHECK(bound_session_cookie_controller_);
    return bound_session_cookie_controller_
        ->IsConnectionTypeAvailableAndOffline();
  }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  crypto::ScopedMockUnexportableKeyProvider scoped_key_provider_;
  unexportable_keys::UnexportableKeyTaskManager unexportable_key_task_manager_;
  unexportable_keys::UnexportableKeyServiceImpl unexportable_key_service_;
  BoundSessionTestCookieManager cookie_manager_;
  content::TestStoragePartition storage_partition_;
  UnexportableKeyId key_id_;
  std::unique_ptr<BoundSessionCookieControllerImpl>
      bound_session_cookie_controller_;
  size_t on_bound_session_throttler_params_changed_call_count_ = 0;
  bool on_terminate_session_called_ = false;
};

TEST_F(BoundSessionCookieControllerImplTest, KeyLoadedOnStartup) {
  EXPECT_NE(key_loader()->GetStateForTesting(),
            unexportable_keys::UnexportableKeyLoader::State::kNotStarted);
  base::test::TestFuture<ServiceErrorOr<UnexportableKeyId>> future;
  key_loader()->InvokeCallbackAfterKeyLoaded(future.GetCallback());
  EXPECT_EQ(*future.Get(), key_id());
}

TEST_F(BoundSessionCookieControllerImplTest, TwoCookieObserversCreated) {
  EXPECT_EQ(bound_cookies_observers()->size(), 2u);
  CHECK(cookie_observer(k1PSIDTSCookieName));
  CHECK(cookie_observer(k3PSIDTSCookieName));
}

TEST_F(BoundSessionCookieControllerImplTest, CookieRefreshOnStartup) {
  EXPECT_TRUE(CompletePendingRefreshRequestIfAny());
  EXPECT_EQ(on_bound_session_throttler_params_changed_call_count(), 1u);
  EXPECT_EQ(cookie_expiration_time(k1PSIDTSCookieName),
            GetTimeInTenMinutes() - kCookieExpirationThreshold);
  EXPECT_EQ(cookie_expiration_time(k3PSIDTSCookieName),
            GetTimeInTenMinutes() - kCookieExpirationThreshold);
  EXPECT_TRUE(AreAllCookiesFresh());
}

TEST_F(BoundSessionCookieControllerImplTest,
       MaybeRefreshCookieMultipleRequests) {
  CompletePendingRefreshRequestIfAny();
  ResetOnBoundSessionThrottlerParamsChangedCallCount();

  EXPECT_FALSE(cookie_fetcher());
  MaybeRefreshCookie();
  BoundSessionRefreshCookieFetcher* fetcher = cookie_fetcher();
  EXPECT_TRUE(fetcher);

  MaybeRefreshCookie();
  EXPECT_EQ(cookie_fetcher(), fetcher);
  EXPECT_TRUE(CompletePendingRefreshRequestIfAny());
  EXPECT_FALSE(cookie_fetcher());
}

TEST_F(BoundSessionCookieControllerImplTest,
       NotifiesOnlyIfMinimumCookieExpirationDateChanged) {
  CompletePendingRefreshRequestIfAny();
  ResetOnBoundSessionThrottlerParamsChangedCallCount();

  // Update with the same date.
  SetExpirationTimeAndNotify(
      k1PSIDTSCookieName,
      cookie_expiration_time(k1PSIDTSCookieName) + kCookieExpirationThreshold);
  EXPECT_EQ(on_bound_session_throttler_params_changed_call_count(), 0u);

  // Update with null time should change the minimum expiration date and
  // trigger a notification.
  SetExpirationTimeAndNotify(k1PSIDTSCookieName, base::Time());
  EXPECT_EQ(on_bound_session_throttler_params_changed_call_count(), 1u);
  EXPECT_EQ(cookie_expiration_time(k1PSIDTSCookieName), base::Time());
  EXPECT_EQ(bound_session_cookie_controller()->min_cookie_expiration_time(),
            base::Time());
}

TEST_F(BoundSessionCookieControllerImplTest, CookieChange) {
  CompletePendingRefreshRequestIfAny();
  ResetOnBoundSessionThrottlerParamsChangedCallCount();
  task_environment()->FastForwardBy(base::Minutes(2));

  BoundSessionCookieController* controller = bound_session_cookie_controller();
  base::Time expiration_time_1PSIDTS =
      cookie_expiration_time(k1PSIDTSCookieName);
  base::Time expiration_time_3PSIDTS =
      cookie_expiration_time(k3PSIDTSCookieName);
  base::Time minimum_expiration_time = controller->min_cookie_expiration_time();
  EXPECT_EQ(expiration_time_1PSIDTS, minimum_expiration_time);
  EXPECT_EQ(expiration_time_1PSIDTS, expiration_time_3PSIDTS);

  // Simulate cookie change of 1st cookie.
  SimulateCookieChange(k1PSIDTSCookieName, GetTimeInTenMinutes());
  expiration_time_1PSIDTS = cookie_expiration_time(k1PSIDTSCookieName);
  EXPECT_EQ(expiration_time_1PSIDTS,
            GetTimeInTenMinutes() - kCookieExpirationThreshold);
  // The other cookie expiration time remains unchanged.
  EXPECT_EQ(cookie_expiration_time(k3PSIDTSCookieName),
            expiration_time_3PSIDTS);
  // The new `expiration_time_1PSIDTS` is larger than the other cookie
  // expiration time so the minimum remains unchanged.
  EXPECT_EQ(controller->min_cookie_expiration_time(), minimum_expiration_time);
  EXPECT_EQ(on_bound_session_throttler_params_changed_call_count(), 0u);

  task_environment()->FastForwardBy(base::Minutes(2));
  // Simulate cookie change of 2nd cookie.
  SimulateCookieChange(k3PSIDTSCookieName, GetTimeInTenMinutes());
  EXPECT_EQ(cookie_expiration_time(k3PSIDTSCookieName),
            GetTimeInTenMinutes() - kCookieExpirationThreshold);
  // Expiration time of: `k3PSIDTSCookieName` > `k1PSIDTSCookieName`.
  // The minimum changes to the expiration date of `k1PSIDTSCookieName`.
  EXPECT_EQ(controller->min_cookie_expiration_time(), expiration_time_1PSIDTS);
  EXPECT_EQ(on_bound_session_throttler_params_changed_call_count(), 1u);
}

TEST_F(BoundSessionCookieControllerImplTest,
       RequestBlockedOnCookieWhenCookieFresh) {
  ASSERT_FALSE(IsConnectionTypeAvailableAndOffline());
  // Set fresh cookie.
  CompletePendingRefreshRequestIfAny();
  BoundSessionCookieController* controller = bound_session_cookie_controller();
  EXPECT_TRUE(AreAllCookiesFresh());

  // No fetch should be triggered since the cookie is fresh.
  // The callback should return immediately.
  base::test::TestFuture<void> future;
  controller->OnRequestBlockedOnCookie(future.GetCallback());
  EXPECT_TRUE(future.IsReady());
  EXPECT_FALSE(cookie_fetcher());
}

TEST_F(BoundSessionCookieControllerImplTest,
       RequestBlockedOnCookieWhenCookieStaleTriggersARefresh) {
  ASSERT_FALSE(IsConnectionTypeAvailableAndOffline());
  CompletePendingRefreshRequestIfAny();

  BoundSessionCookieController* controller = bound_session_cookie_controller();
  task_environment()->FastForwardBy(base::Minutes(12));
  // Cookie stale.
  EXPECT_FALSE(AreAllCookiesFresh());
  // Preemptive cookie rotation also fails with persistent error.
  SimulateCompleteRefreshRequest(
      BoundSessionRefreshCookieFetcher::Result::kConnectionError,
      absl::nullopt);
  EXPECT_FALSE(cookie_fetcher());

  // Request blocked on the cookie.
  base::test::TestFuture<void> future;
  controller->OnRequestBlockedOnCookie(future.GetCallback());
  EXPECT_FALSE(future.IsReady());

  // Simulate refresh complete.
  SimulateCompleteRefreshRequest(
      BoundSessionRefreshCookieFetcher::Result::kSuccess,
      GetTimeInTenMinutes());
  task_environment()->RunUntilIdle();
  EXPECT_TRUE(future.IsReady());
  EXPECT_TRUE(AreAllCookiesFresh());
}

TEST_F(BoundSessionCookieControllerImplTest,
       RequestBlockedWhenNotAllCookiesFresh) {
  ASSERT_FALSE(IsConnectionTypeAvailableAndOffline());
  CompletePendingRefreshRequestIfAny();
  BoundSessionCookieController* controller = bound_session_cookie_controller();

  // All cookies stale.
  task_environment()->FastForwardBy(base::Minutes(12));
  EXPECT_FALSE(AreAllCookiesFresh());
  // Request blocked on the cookies.
  base::test::TestFuture<void> future;
  controller->OnRequestBlockedOnCookie(future.GetCallback());
  EXPECT_FALSE(future.IsReady());

  // One cookie is fresh.
  SetExpirationTimeAndNotify(k1PSIDTSCookieName, GetTimeInTenMinutes());
  EXPECT_FALSE(future.IsReady());
  EXPECT_FALSE(AreAllCookiesFresh());

  // All cookies fresh.
  SetExpirationTimeAndNotify(k3PSIDTSCookieName, GetTimeInTenMinutes());
  EXPECT_TRUE(future.IsReady());
  EXPECT_TRUE(AreAllCookiesFresh());
  CompletePendingRefreshRequestIfAny();
}

TEST_F(BoundSessionCookieControllerImplTest,
       RequestBlockedOnCookieRefreshFailedWithPersistentError) {
  ASSERT_FALSE(IsConnectionTypeAvailableAndOffline());
  CompletePendingRefreshRequestIfAny();
  EXPECT_FALSE(on_cookie_refresh_persistent_failure_called());

  BoundSessionCookieController* controller = bound_session_cookie_controller();
  task_environment()->FastForwardBy(base::Minutes(12));
  base::Time min_cookie_expiration = controller->min_cookie_expiration_time();

  // Cookie stale.
  EXPECT_FALSE(AreAllCookiesFresh());
  // Preemptive cookie rotation also fails with persistent error.
  SimulateCompleteRefreshRequest(
      BoundSessionRefreshCookieFetcher::Result::kConnectionError,
      absl::nullopt);
  EXPECT_FALSE(cookie_fetcher());

  base::test::TestFuture<void> future;
  controller->OnRequestBlockedOnCookie(future.GetCallback());
  EXPECT_FALSE(future.IsReady());

  // Simulate refresh completes with persistent failure.
  SimulateCompleteRefreshRequest(
      BoundSessionRefreshCookieFetcher::Result::kServerPersistentError,
      absl::nullopt);
  task_environment()->RunUntilIdle();
  EXPECT_TRUE(on_cookie_refresh_persistent_failure_called());
  EXPECT_TRUE(future.IsReady());
  EXPECT_EQ(controller->min_cookie_expiration_time(), min_cookie_expiration);
}

TEST_F(BoundSessionCookieControllerImplTest, RefreshFailedTransient) {
  ASSERT_FALSE(IsConnectionTypeAvailableAndOffline());
  CompletePendingRefreshRequestIfAny();
  task_environment()->FastForwardBy(base::Minutes(12));
  EXPECT_FALSE(AreAllCookiesFresh());
  std::array<BoundSessionRefreshCookieFetcher::Result, 2> result_types = {
      BoundSessionRefreshCookieFetcher::Result::kConnectionError,
      BoundSessionRefreshCookieFetcher::Result::kServerTransientError};

  for (auto& result : result_types) {
    SCOPED_TRACE(result);
    base::test::TestFuture<void> future;
    bound_session_cookie_controller()->OnRequestBlockedOnCookie(
        future.GetCallback());
    EXPECT_FALSE(future.IsReady());
    SimulateCompleteRefreshRequest(result, absl::nullopt);
    EXPECT_TRUE(future.IsReady());
  }

  // Subsequent requests are not impacted.
  base::test::TestFuture<void> future;
  bound_session_cookie_controller()->OnRequestBlockedOnCookie(
      future.GetCallback());
  EXPECT_FALSE(future.IsReady());
  EXPECT_TRUE(cookie_fetcher());
  SimulateCompleteRefreshRequest(
      BoundSessionRefreshCookieFetcher::Result::kSuccess,
      GetTimeInTenMinutes());
  EXPECT_TRUE(future.IsReady());
  EXPECT_FALSE(on_cookie_refresh_persistent_failure_called());
}

TEST_F(BoundSessionCookieControllerImplTest,
       RequestBlockedOnCookieMultipleRequests) {
  ASSERT_FALSE(IsConnectionTypeAvailableAndOffline());
  CompletePendingRefreshRequestIfAny();
  ResetOnBoundSessionThrottlerParamsChangedCallCount();
  // Cookie stale.
  task_environment()->FastForwardBy(base::Minutes(12));

  BoundSessionCookieController* controller = bound_session_cookie_controller();
  std::array<base::test::TestFuture<void>, 5> futures;
  for (auto& future : futures) {
    controller->OnRequestBlockedOnCookie(future.GetCallback());
    EXPECT_FALSE(future.IsReady());
  }

  SimulateCompleteRefreshRequest(
      BoundSessionRefreshCookieFetcher::Result::kSuccess,
      GetTimeInTenMinutes());
  task_environment()->RunUntilIdle();
  for (auto& future : futures) {
    EXPECT_TRUE(future.IsReady());
  }
  EXPECT_EQ(on_bound_session_throttler_params_changed_call_count(), 1u);
  EXPECT_TRUE(AreAllCookiesFresh());
}

TEST_F(BoundSessionCookieControllerImplTest,
       CookieChangesToFreshWhileRequestBlockedOnCookieIsPending) {
  ASSERT_FALSE(IsConnectionTypeAvailableAndOffline());
  CompletePendingRefreshRequestIfAny();
  // Stale cookie.
  task_environment()->FastForwardBy(base::Minutes(12));

  base::test::TestFuture<void> future;
  bound_session_cookie_controller()->OnRequestBlockedOnCookie(
      future.GetCallback());
  // Refresh request pending.
  EXPECT_TRUE(cookie_fetcher());
  EXPECT_FALSE(future.IsReady());

  // Cookie fresh.
  SimulateCookieChange(k1PSIDTSCookieName, GetTimeInTenMinutes());
  EXPECT_FALSE(future.IsReady());
  SimulateCookieChange(k3PSIDTSCookieName, GetTimeInTenMinutes());
  EXPECT_TRUE(future.IsReady());

  // Complete the pending fetch.
  EXPECT_TRUE(cookie_fetcher());
  SimulateCompleteRefreshRequest(
      BoundSessionRefreshCookieFetcher::Result::kSuccess,
      GetTimeInTenMinutes());
}

TEST_F(BoundSessionCookieControllerImplTest,
       ControllerDestroyedRequestBlockedOnCookieIsPending) {
  ASSERT_FALSE(IsConnectionTypeAvailableAndOffline());
  BoundSessionCookieController* controller = bound_session_cookie_controller();
  std::array<base::test::TestFuture<void>, 5> futures;
  for (auto& future : futures) {
    controller->OnRequestBlockedOnCookie(future.GetCallback());
    EXPECT_FALSE(future.IsReady());
  }

  ResetBoundSessionCookieController();
  for (auto& future : futures) {
    EXPECT_TRUE(future.IsReady());
  }
}

TEST_F(BoundSessionCookieControllerImplTest, ResumeBlockedRequestsOnTimeout) {
  ASSERT_FALSE(IsConnectionTypeAvailableAndOffline());
  base::test::TestFuture<void> future;
  bound_session_cookie_controller()->OnRequestBlockedOnCookie(
      future.GetCallback());
  EXPECT_FALSE(future.IsReady());

  task_environment()->FastForwardBy(kResumeBlockedRequestTimeout);
  // The required cookies are stale, but the request is resumed due to timeout.
  EXPECT_FALSE(AreAllCookiesFresh());
  // Request resumed.
  EXPECT_TRUE(future.IsReady());
  // Fetcher reset.
  EXPECT_FALSE(cookie_fetcher());
  EXPECT_FALSE(resume_blocked_requests_timer()->IsRunning());
  EXPECT_FALSE(preemptive_cookie_refresh_timer()->IsRunning());
}

TEST_F(BoundSessionCookieControllerImplTest,
       BlockedRequestsCalculateTimeoutFromFirstRequest) {
  ASSERT_FALSE(IsConnectionTypeAvailableAndOffline());
  constexpr int kBlockedRequestCount = 2;
  constexpr base::TimeDelta kDeltaBetweenRequests = base::Seconds(1);
  BoundSessionCookieController* controller = bound_session_cookie_controller();
  std::array<base::test::TestFuture<void>, kBlockedRequestCount> futures;

  for (auto& future : futures) {
    controller->OnRequestBlockedOnCookie(future.GetCallback());
    EXPECT_FALSE(future.IsReady());
    task_environment()->FastForwardBy(kDeltaBetweenRequests);
  }

  // We should release as soon as `kResumeBlockedRequestTimeout` has passed
  // since the first blocked request.
  task_environment()->FastForwardBy(kResumeBlockedRequestTimeout -
                                    kDeltaBetweenRequests *
                                        kBlockedRequestCount);
  // The required cookies are stale, but requests are resumed due to timeout.
  EXPECT_FALSE(AreAllCookiesFresh());
  EXPECT_FALSE(cookie_fetcher());
  for (auto& future : futures) {
    EXPECT_TRUE(future.IsReady());
  }
  EXPECT_FALSE(resume_blocked_requests_timer()->IsRunning());
}

TEST_F(BoundSessionCookieControllerImplTest, ResumeBlockedRequestsTimerReset) {
  ASSERT_FALSE(IsConnectionTypeAvailableAndOffline());
  base::test::TestFuture<void> future;
  bound_session_cookie_controller()->OnRequestBlockedOnCookie(
      future.GetCallback());
  EXPECT_FALSE(future.IsReady());
  EXPECT_TRUE(resume_blocked_requests_timer()->IsRunning());
  SimulateCompleteRefreshRequest(
      BoundSessionRefreshCookieFetcher::Result::kSuccess,
      GetTimeInTenMinutes());
  EXPECT_TRUE(future.IsReady());
  EXPECT_FALSE(resume_blocked_requests_timer()->IsRunning());
}

TEST_F(BoundSessionCookieControllerImplTest,
       ResumeNewBlockedRequestsWhenOffline) {
  SetUpNetworkConnection(true, network::mojom::ConnectionType::CONNECTION_NONE);
  ASSERT_TRUE(IsConnectionTypeAvailableAndOffline());
  base::test::TestFuture<void> future;
  bound_session_cookie_controller()->OnRequestBlockedOnCookie(
      future.GetCallback());
  EXPECT_FALSE(AreAllCookiesFresh());
  EXPECT_TRUE(cookie_fetcher());
  EXPECT_TRUE(future.IsReady());
  EXPECT_FALSE(resume_blocked_requests_timer()->IsRunning());
}

TEST_F(BoundSessionCookieControllerImplTest,
       BlockRequestsWhenConnectionTypeUnavailable) {
  SetUpNetworkConnection(false,
                         network::mojom::ConnectionType::CONNECTION_WIFI);
  ASSERT_FALSE(IsConnectionTypeAvailableAndOffline());
  base::test::TestFuture<void> future;
  bound_session_cookie_controller()->OnRequestBlockedOnCookie(
      future.GetCallback());
  EXPECT_FALSE(future.IsReady());
  EXPECT_TRUE(resume_blocked_requests_timer()->IsRunning());
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(future.IsReady());
  CompletePendingRefreshRequestIfAny();
  EXPECT_TRUE(future.IsReady());
}

TEST_F(BoundSessionCookieControllerImplTest,
       ResumePendingBlockedRequestsOnNetworkConnectionChangedToOffline) {
  ASSERT_FALSE(IsConnectionTypeAvailableAndOffline());
  base::test::TestFuture<void> future;
  bound_session_cookie_controller()->OnRequestBlockedOnCookie(
      future.GetCallback());
  EXPECT_FALSE(future.IsReady());
  EXPECT_TRUE(resume_blocked_requests_timer()->IsRunning());

  SetConnectionType(network::mojom::ConnectionType::CONNECTION_NONE);
  ASSERT_TRUE(IsConnectionTypeAvailableAndOffline());
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(AreAllCookiesFresh());
  EXPECT_TRUE(cookie_fetcher());
  EXPECT_TRUE(future.IsReady());
  EXPECT_FALSE(resume_blocked_requests_timer()->IsRunning());
}

TEST_F(BoundSessionCookieControllerImplTest,
       NotNullCookieExpirationTimeIsReducedByThreshold) {
  EXPECT_TRUE(CompletePendingRefreshRequestIfAny());
  EXPECT_EQ(cookie_expiration_time(k1PSIDTSCookieName),
            GetTimeInTenMinutes() - kCookieExpirationThreshold);
  EXPECT_EQ(cookie_expiration_time(k3PSIDTSCookieName),
            GetTimeInTenMinutes() - kCookieExpirationThreshold);
}

TEST_F(BoundSessionCookieControllerImplTest,
       NullCookieExpirationTimeIsNotReducedByThreshold) {
  EXPECT_TRUE(CompletePendingRefreshRequestIfAny());
  SetExpirationTimeAndNotify(k1PSIDTSCookieName, base::Time());
  EXPECT_EQ(cookie_expiration_time(k1PSIDTSCookieName), base::Time());
}

TEST_F(BoundSessionCookieControllerImplTest,
       ScheduleCookieRotationOnSetCookieExpiration) {
  ResetOnBoundSessionThrottlerParamsChangedCallCount();
  EXPECT_TRUE(CompletePendingRefreshRequestIfAny());
  EXPECT_EQ(on_bound_session_throttler_params_changed_call_count(), 1u);
  EXPECT_TRUE(preemptive_cookie_refresh_timer()->IsRunning());
  base::TimeDelta expected_refresh_delay =
      bound_session_cookie_controller()->min_cookie_expiration_time() -
      base::Time::Now() - kCookieRefreshInterval;
  EXPECT_EQ(preemptive_cookie_refresh_timer()->GetCurrentDelay(),
            expected_refresh_delay);
  task_environment()->FastForwardBy(expected_refresh_delay);
  EXPECT_TRUE(cookie_fetcher());
  CompletePendingRefreshRequestIfAny();
}

TEST_F(BoundSessionCookieControllerImplTest,
       RescheduleCookieRotationOnlyIfMinimumExpirationDateChanged) {
  CompletePendingRefreshRequestIfAny();
  EXPECT_TRUE(preemptive_cookie_refresh_timer()->IsRunning());
  task_environment()->FastForwardBy(base::Minutes(12));

  // We want to test that a cookie refresh is scheduled only when the minimum
  // expiration time of the two cookies changes.
  // We first set up a situation where both cookies are stale and there is no
  // ongoing refresh. `kServerTransientError` is used to complete the refresh
  // request without updating the cookies.
  SimulateCompleteRefreshRequest(
      BoundSessionRefreshCookieFetcher::Result::kServerTransientError,
      absl::nullopt);
  EXPECT_FALSE(cookie_fetcher());
  EXPECT_FALSE(preemptive_cookie_refresh_timer()->IsRunning());
  base::Time old_min_cookie_expiration =
      bound_session_cookie_controller()->min_cookie_expiration_time();

  SetExpirationTimeAndNotify(k1PSIDTSCookieName, GetTimeInTenMinutes());
  // The new expiration time of `k1PSIDTSCookieName` is larger than the other
  // cookie expiration time so the minimum remains unchanged.
  EXPECT_EQ(bound_session_cookie_controller()->min_cookie_expiration_time(),
            old_min_cookie_expiration);
  // Cookie rotation is not scheduled.
  EXPECT_FALSE(preemptive_cookie_refresh_timer()->IsRunning());

  SetExpirationTimeAndNotify(k3PSIDTSCookieName, GetTimeInTenMinutes());
  // The expiration time of the other cookie is updated, and the minimum
  // expiration time changes.
  EXPECT_NE(bound_session_cookie_controller()->min_cookie_expiration_time(),
            old_min_cookie_expiration);
  // Cookie rotation scheduled.
  EXPECT_TRUE(preemptive_cookie_refresh_timer()->IsRunning());
}

TEST_F(BoundSessionCookieControllerImplTest,
       RefreshCookieImmediatelyOnSetCookieExpirationBelowRefreshInterval) {
  EXPECT_TRUE(CompletePendingRefreshRequestIfAny());
  ResetOnBoundSessionThrottlerParamsChangedCallCount();
  SetExpirationTimeAndNotify(k1PSIDTSCookieName,
                             base::Time::Now() + kCookieRefreshInterval / 2);
  EXPECT_EQ(on_bound_session_throttler_params_changed_call_count(), 1u);
  EXPECT_FALSE(preemptive_cookie_refresh_timer()->IsRunning());
  EXPECT_TRUE(cookie_fetcher());
  CompletePendingRefreshRequestIfAny();
}

TEST_F(BoundSessionCookieControllerImplTest,
       StopCookieRotationOnCookieRefresh) {
  EXPECT_TRUE(CompletePendingRefreshRequestIfAny());
  EXPECT_TRUE(preemptive_cookie_refresh_timer()->IsRunning());
  MaybeRefreshCookie();
  EXPECT_FALSE(preemptive_cookie_refresh_timer()->IsRunning());
  CompletePendingRefreshRequestIfAny();
}
