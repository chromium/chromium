// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_controller_impl.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_controller.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_observer.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_refresh_cookie_fetcher.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_test_cookie_manager.h"
#include "chrome/browser/signin/bound_session_credentials/fake_bound_session_refresh_cookie_fetcher.h"
#include "components/signin/public/base/test_signin_client.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/cookies/canonical_cookie.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {
constexpr char kSIDTSCookieName[] = "__Secure-1PSIDTS";

base::Time GetTimeInTenMinutes() {
  return base::Time::Now() + base::Minutes(10);
}
}  // namespace

class BoundSessionCookieControllerImplTest
    : public testing::Test,
      public BoundSessionCookieController::Delegate {
 public:
  BoundSessionCookieControllerImplTest() : signin_client_(&prefs_) {
    signin_client_.set_cookie_manager(
        std::make_unique<BoundSessionTestCookieManager>());
    bound_session_cookie_controller_ =
        std::make_unique<BoundSessionCookieControllerImpl>(
            &signin_client_, GaiaUrls::GetInstance()->secure_google_url(),
            kSIDTSCookieName, this);

    bound_session_cookie_controller_
        ->set_refresh_cookie_fetcher_factory_for_testing(
            base::BindRepeating(&BoundSessionCookieControllerImplTest::
                                    CreateBoundSessionRefreshCookieFetcher,
                                base::Unretained(this)));
    bound_session_cookie_controller_->Initialize();
  }

  ~BoundSessionCookieControllerImplTest() override = default;

  std::unique_ptr<BoundSessionRefreshCookieFetcher>
  CreateBoundSessionRefreshCookieFetcher(SigninClient* client,
                                         const GURL& url,
                                         const std::string& cookie_name) {
    // `SimulateCompleteRefreshRequest()` must be called for the
    // refresh request to complete.
    auto fetcher = std::make_unique<FakeBoundSessionRefreshCookieFetcher>(
        client, url, cookie_name);
    cookie_fetcher_ = fetcher.get();
    return fetcher;
  }

  void MaybeRefreshCookie() {
    bound_session_cookie_controller_->MaybeRefreshCookie();
  }

  bool CompletePendingRefreshRequestIfAny() {
    if (!cookie_fetcher_) {
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
    EXPECT_TRUE(cookie_fetcher_);
    cookie_fetcher_->SimulateCompleteRefreshRequest(result, cookie_expiration);
    // It is not safe to access the `cookie_fetcher_` after the request has been
    // completed. The controller will destroy the fetcher upon completion.
    cookie_fetcher_ = nullptr;
  }

  void SimulateCookieChange(absl::optional<base::Time> cookie_expiration) {
    net::CanonicalCookie cookie = BoundSessionTestCookieManager::CreateCookie(
        bound_session_cookie_controller()->url(),
        bound_session_cookie_controller()->cookie_name(), cookie_expiration);
    cookie_observer()->OnCookieChange(net::CookieChangeInfo(
        cookie, net::CookieAccessResult(), net::CookieChangeCause::INSERTED));
  }

  base::test::TaskEnvironment* task_environment() { return &task_environment_; }

  void OnCookieExpirationDateChanged() override {
    on_cookie_expiration_date_changed_call_count_++;
  }

  void TerminateSession() override { on_terminate_session_called_ = true; }

  void SetExpirationTimeAndNotify(const base::Time& expiration_time) {
    bound_session_cookie_controller_->SetCookieExpirationTimeAndNotify(
        expiration_time);
  }

  BoundSessionCookieControllerImpl* bound_session_cookie_controller() {
    return bound_session_cookie_controller_.get();
  }

  FakeBoundSessionRefreshCookieFetcher* cookie_fetcher() {
    return cookie_fetcher_;
  }

  BoundSessionCookieObserver* cookie_observer() {
    return bound_session_cookie_controller()->cookie_observer_.get();
  }

  size_t on_cookie_expiration_date_changed_call_count() {
    return on_cookie_expiration_date_changed_call_count_;
  }

  bool on_cookie_refresh_persistent_failure_called() {
    return on_terminate_session_called_;
  }

  void ResetOnCookieExpirationDateChangedCallCount() {
    on_cookie_expiration_date_changed_call_count_ = 0;
  }

  void ResetBoundSessionCookieController() {
    bound_session_cookie_controller_.reset();
  }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  sync_preferences::TestingPrefServiceSyncable prefs_;
  TestSigninClient signin_client_;
  std::unique_ptr<BoundSessionCookieControllerImpl>
      bound_session_cookie_controller_;
  raw_ptr<FakeBoundSessionRefreshCookieFetcher, DanglingUntriaged>
      cookie_fetcher_ = nullptr;
  size_t on_cookie_expiration_date_changed_call_count_ = 0;
  bool on_terminate_session_called_ = false;
};

TEST_F(BoundSessionCookieControllerImplTest, CookieRefreshOnStartup) {
  EXPECT_TRUE(CompletePendingRefreshRequestIfAny());
  EXPECT_EQ(on_cookie_expiration_date_changed_call_count(), 1u);
  EXPECT_EQ(bound_session_cookie_controller()->cookie_expiration_time(),
            GetTimeInTenMinutes());
}

TEST_F(BoundSessionCookieControllerImplTest,
       OnRefreshCookieFailedDoesNotUpdateCookieExpirationTime) {
  CompletePendingRefreshRequestIfAny();
  ResetOnCookieExpirationDateChangedCallCount();
  base::Time cookie_expiration =
      bound_session_cookie_controller()->cookie_expiration_time();

  MaybeRefreshCookie();
  SimulateCompleteRefreshRequest(
      BoundSessionRefreshCookieFetcher::Result::kServerTransientError,
      absl::nullopt);
  EXPECT_EQ(on_cookie_expiration_date_changed_call_count(), 0u);
  EXPECT_EQ(bound_session_cookie_controller()->cookie_expiration_time(),
            cookie_expiration);
}

TEST_F(BoundSessionCookieControllerImplTest,
       MaybeRefreshCookieMultipleRequests) {
  CompletePendingRefreshRequestIfAny();
  ResetOnCookieExpirationDateChangedCallCount();

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
       NotifiesOnlyIfCookieExpiryDateChanged) {
  CompletePendingRefreshRequestIfAny();
  ResetOnCookieExpirationDateChangedCallCount();

  // Update with the same date
  base::Time cookie_expiration =
      bound_session_cookie_controller()->cookie_expiration_time();
  SetExpirationTimeAndNotify(cookie_expiration);
  EXPECT_EQ(on_cookie_expiration_date_changed_call_count(), 0u);

  // Update with null time should trigger a notification.
  SetExpirationTimeAndNotify(base::Time());
  EXPECT_EQ(on_cookie_expiration_date_changed_call_count(), 1u);
  EXPECT_EQ(bound_session_cookie_controller()->cookie_expiration_time(),
            base::Time());
}

TEST_F(BoundSessionCookieControllerImplTest, CookieChange) {
  CompletePendingRefreshRequestIfAny();
  ResetOnCookieExpirationDateChangedCallCount();

  // Simulate cookie change.
  BoundSessionCookieController* controller = bound_session_cookie_controller();
  SimulateCookieChange(base::Time::Now());
  EXPECT_EQ(on_cookie_expiration_date_changed_call_count(), 1u);
  EXPECT_EQ(controller->cookie_expiration_time(), base::Time::Now());
}

TEST_F(BoundSessionCookieControllerImplTest,
       RequestBlockedOnCookieWhenCookieFresh) {
  // Set fresh cookie.
  CompletePendingRefreshRequestIfAny();
  BoundSessionCookieController* controller = bound_session_cookie_controller();
  EXPECT_EQ(controller->cookie_expiration_time(), GetTimeInTenMinutes());

  // No fetch should be triggered since the cookie is fresh.
  // The callback should return immediately.
  base::test::TestFuture<void> future;
  controller->OnRequestBlockedOnCookie(future.GetCallback());
  EXPECT_TRUE(future.IsReady());
  EXPECT_FALSE(cookie_fetcher());
}

TEST_F(BoundSessionCookieControllerImplTest,
       RequestBlockedOnCookieWhenCookieStale) {
  CompletePendingRefreshRequestIfAny();

  BoundSessionCookieController* controller = bound_session_cookie_controller();
  task_environment()->FastForwardBy(base::Minutes(12));
  // Cookie stale.
  EXPECT_LT(controller->cookie_expiration_time(), base::Time::Now());
  EXPECT_FALSE(cookie_fetcher());

  // Request blocked on the cookie
  base::test::TestFuture<void> future;
  controller->OnRequestBlockedOnCookie(future.GetCallback());
  EXPECT_FALSE(future.IsReady());

  // Simulate refresh complete.
  SimulateCompleteRefreshRequest(
      BoundSessionRefreshCookieFetcher::Result::kSuccess,
      GetTimeInTenMinutes());
  task_environment()->RunUntilIdle();
  EXPECT_TRUE(future.IsReady());
  EXPECT_EQ(controller->cookie_expiration_time(), GetTimeInTenMinutes());
}

TEST_F(BoundSessionCookieControllerImplTest,
       RequestBlockedOnCookieRefreshFailedWithPersistentError) {
  CompletePendingRefreshRequestIfAny();
  EXPECT_FALSE(on_cookie_refresh_persistent_failure_called());

  BoundSessionCookieController* controller = bound_session_cookie_controller();
  task_environment()->FastForwardBy(base::Minutes(12));
  base::Time cookie_expiration = controller->cookie_expiration_time();

  // Cookie stale.
  EXPECT_LT(cookie_expiration, base::Time::Now());
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
  EXPECT_EQ(controller->cookie_expiration_time(), cookie_expiration);
}

TEST_F(BoundSessionCookieControllerImplTest, RefreshFailedTransient) {
  CompletePendingRefreshRequestIfAny();
  task_environment()->FastForwardBy(base::Minutes(12));

  BoundSessionCookieController* controller = bound_session_cookie_controller();
  EXPECT_LT(controller->cookie_expiration_time(), base::Time::Now());

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
  CompletePendingRefreshRequestIfAny();
  ResetOnCookieExpirationDateChangedCallCount();
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
  EXPECT_EQ(on_cookie_expiration_date_changed_call_count(), 1u);
  EXPECT_EQ(controller->cookie_expiration_time(), GetTimeInTenMinutes());
}

TEST_F(BoundSessionCookieControllerImplTest,
       CookieChangesToFreshWhileRequestBlockedOnCookieIsPending) {
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
  SimulateCookieChange(GetTimeInTenMinutes());
  EXPECT_TRUE(future.IsReady());

  // Complete the pending fetch.
  EXPECT_TRUE(cookie_fetcher());
  SimulateCompleteRefreshRequest(
      BoundSessionRefreshCookieFetcher::Result::kSuccess,
      GetTimeInTenMinutes());
}

TEST_F(BoundSessionCookieControllerImplTest,
       ControllerDestroyedRequestBlockedOnCookieIsPending) {
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
