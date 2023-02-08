// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_controller_impl.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/test/task_environment.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_refresh_cookie_fetcher.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_test_cookie_manager.h"
#include "components/signin/public/base/test_signin_client.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "google_apis/gaia/gaia_urls.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
constexpr char kSIDTSCookieName[] = "__Secure-1PSIDTS";

class FakeBoundSessionRefreshCookieFetcher
    : public BoundSessionRefreshCookieFetcher {
 public:
  FakeBoundSessionRefreshCookieFetcher(SigninClient* client,
                                       const GURL& url,
                                       const std::string& cookie_name)
      : BoundSessionRefreshCookieFetcher(client, url, cookie_name) {}

  void Start(RefreshCookieCompleteCallback callback) override {
    if (cookie_expiration_.has_value()) {
      callback_ = std::move(callback);
      OnRefreshCookieCompleted(CreateFakeCookie(cookie_expiration_.value()));
    } else {
      std::move(callback).Run(cookie_expiration_);
    }
  }

  void set_cookie_expiration(absl::optional<base::Time> cookie_expiration) {
    cookie_expiration_ = cookie_expiration;
  }

 private:
  absl::optional<base::Time> cookie_expiration_;
};
}  // namespace

class BoundSessionCookieControllerImplTest
    : public testing::Test,
      public BoundSessionCookieController::Delegate {
 public:
  BoundSessionCookieControllerImplTest() : signin_client_(&prefs_) {
    signin_client_.set_cookie_manager(
        std::make_unique<BoundSessionTestCookieManager>());
  }

  void SetNextRefreshCookieFetcherResult(
      absl::optional<base::Time> next_refresh_cookie_fetcher_result) {
    next_refresh_cookie_fetcher_result_ = next_refresh_cookie_fetcher_result;
  }

  std::unique_ptr<BoundSessionRefreshCookieFetcher>
  CreateBoundSessionRefreshCookieFetcher(SigninClient* client,
                                         const GURL& url,
                                         const std::string& cookie_name) {
    // params must match the one passed to `bound_session_cookie_controller_`.
    auto fetcher = std::make_unique<FakeBoundSessionRefreshCookieFetcher>(
        client, url, cookie_name);
    fetcher->set_cookie_expiration(next_refresh_cookie_fetcher_result_);
    return fetcher;
  }

  BoundSessionCookieControllerImpl* bound_session_cookie_controller() {
    if (!bound_session_cookie_controller_) {
      bound_session_cookie_controller_ =
          CreateBoundSessionCookieControllerImpl();
    }
    return bound_session_cookie_controller_.get();
  }

  ~BoundSessionCookieControllerImplTest() override = default;

  void OnCookieExpirationDateChanged() override {
    on_cookie_expiration_date_changed_call_count_++;
  }

  void SetExpirationTimeAndNotify(const base::Time& expiration_time) {
    bound_session_cookie_controller_->SetCookieExpirationTimeAndNotify(
        expiration_time);
  }

  size_t on_cookie_expiration_date_changed_call_count() {
    return on_cookie_expiration_date_changed_call_count_;
  }

  std::unique_ptr<BoundSessionCookieControllerImpl>
  CreateBoundSessionCookieControllerImpl() {
    auto bound_session_cookie_controller =
        absl::WrapUnique(new BoundSessionCookieControllerImpl(
            &signin_client_, GaiaUrls::GetInstance()->secure_google_url(),
            kSIDTSCookieName, this));

    bound_session_cookie_controller
        ->set_refresh_cookie_fetcher_factory_for_testing(
            base::BindRepeating(&BoundSessionCookieControllerImplTest::
                                    CreateBoundSessionRefreshCookieFetcher,
                                base::Unretained(this)));
    bound_session_cookie_controller->Initialize();
    return bound_session_cookie_controller;
  }

 private:
  base::test::TaskEnvironment task_environment_;
  sync_preferences::TestingPrefServiceSyncable prefs_;
  TestSigninClient signin_client_;
  std::unique_ptr<BoundSessionCookieControllerImpl>
      bound_session_cookie_controller_;
  size_t on_cookie_expiration_date_changed_call_count_ = 0;
  absl::optional<base::Time> next_refresh_cookie_fetcher_result_;
};

TEST_F(BoundSessionCookieControllerImplTest, CookieFetchOnStartup) {
  base::Time cookie_expiration = base::Time::Now() + base::Minutes(10);
  SetNextRefreshCookieFetcherResult(cookie_expiration);
  BoundSessionCookieControllerImpl* controller =
      bound_session_cookie_controller();
  EXPECT_EQ(on_cookie_expiration_date_changed_call_count(), 1u);
  EXPECT_EQ(controller->cookie_expiration_time(), cookie_expiration);
}

TEST_F(BoundSessionCookieControllerImplTest, OnRefreshCookieFailedDoesNothing) {
  SetNextRefreshCookieFetcherResult(absl::nullopt);
  bound_session_cookie_controller();
  EXPECT_EQ(on_cookie_expiration_date_changed_call_count(), 0u);
}

TEST_F(BoundSessionCookieControllerImplTest,
       NotifiesOnlyIfCookieExpiryDateChanged) {
  base::Time current_cookie_expiration = base::Time::Now() + base::Minutes(10);
  SetNextRefreshCookieFetcherResult(current_cookie_expiration);
  BoundSessionCookieControllerImpl* controller =
      bound_session_cookie_controller();
  EXPECT_EQ(on_cookie_expiration_date_changed_call_count(), 1u);
  EXPECT_EQ(controller->cookie_expiration_time(), current_cookie_expiration);

  // Update with the same date
  SetExpirationTimeAndNotify(current_cookie_expiration);
  // The count remained the same
  EXPECT_EQ(on_cookie_expiration_date_changed_call_count(), 1u);

  // Update with null time should trigger a notification.
  SetExpirationTimeAndNotify(base::Time());
  // The count remained the same
  EXPECT_EQ(on_cookie_expiration_date_changed_call_count(), 2u);
}
