// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_observer.h"

#include <cstddef>
#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_test_cookie_manager.h"
#include "content/public/test/test_storage_partition.h"
#include "net/cookies/canonical_cookie.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {
constexpr char kSIDTSCookieName[] = "__Secure-1PSIDTS";

class BoundSessionCookieObserverTest : public testing::Test {
 public:
  const GURL kGaiaUrl = GURL("https://google.com");
  BoundSessionCookieObserverTest() { ResetCookieManager(); }

  ~BoundSessionCookieObserverTest() override = default;

  void CreateObserver() {
    if (!bound_session_cookie_observer_) {
      bound_session_cookie_observer_ =
          std::make_unique<BoundSessionCookieObserver>(
              &storage_partition_, kGaiaUrl, kSIDTSCookieName,
              base::BindRepeating(
                  &BoundSessionCookieObserverTest::UpdateExpirationDate,
                  base::Unretained(this)));
    }
  }

  void ResetCookieManager() {
    auto cookie_manager = std::make_unique<BoundSessionTestCookieManager>();
    storage_partition_.set_cookie_manager_for_browser_process(
        cookie_manager.get());
    // Reset storage partition's cookie manager before resetting
    // `cookie_manager_` to avoid having a dangling raw pointer.
    cookie_manager_ = std::move(cookie_manager);
  }

  void Reset() {
    bound_session_cookie_observer_.reset();
    on_cookie_change_callback_.Reset();
    update_expiration_date_call_count_ = 0;
    cookie_expiration_date_ = base::Time();
  }

  void SetNextCookieChangeCallback(
      base::OnceCallback<void(const std::string&, base::Time)> callback) {
    // Old expectations should have been verified.
    EXPECT_FALSE(on_cookie_change_callback_);
    on_cookie_change_callback_ = std::move(callback);
  }

  void UpdateExpirationDate(const std::string& cookie_name,
                            base::Time expiration_date) {
    update_expiration_date_call_count_++;
    cookie_expiration_date_ = expiration_date;
    if (on_cookie_change_callback_) {
      std::move(on_cookie_change_callback_).Run(cookie_name, expiration_date);
    }
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<BoundSessionTestCookieManager> cookie_manager_;
  content::TestStoragePartition storage_partition_;
  std::unique_ptr<BoundSessionCookieObserver> bound_session_cookie_observer_;
  size_t update_expiration_date_call_count_ = 0;
  base::Time cookie_expiration_date_;
  base::OnceCallback<void(const std::string&, base::Time)>
      on_cookie_change_callback_;
};

TEST_F(BoundSessionCookieObserverTest, CookieAvailableOnStartup) {
  // Set Cookie.
  net::CanonicalCookie cookie =
      BoundSessionTestCookieManager::CreateCookie(kGaiaUrl, kSIDTSCookieName);
  cookie_manager_->SetCanonicalCookie(cookie, kGaiaUrl, net::CookieOptions(),
                                      base::DoNothing());

  CreateObserver();
  // `BoundSessionTestCookieManager` calls the callback immediately so no need
  // to wait.
  EXPECT_EQ(update_expiration_date_call_count_, 1u);
  EXPECT_EQ(cookie_expiration_date_, cookie.ExpiryDate());
}

TEST_F(BoundSessionCookieObserverTest, CookieMissingOnStartup) {
  CreateObserver();

  EXPECT_EQ(update_expiration_date_call_count_, 1u);
  EXPECT_EQ(cookie_expiration_date_, base::Time());
}

TEST_F(BoundSessionCookieObserverTest, CookieInserted) {
  CreateObserver();
  // Cookie not set.
  update_expiration_date_call_count_ = 0;

  // Insert event.
  net::CanonicalCookie cookie =
      BoundSessionTestCookieManager::CreateCookie(kGaiaUrl, kSIDTSCookieName);
  base::test::TestFuture<const std::string&, base::Time> future;
  SetNextCookieChangeCallback(future.GetCallback());
  cookie_manager_->DispatchCookieChange(net::CookieChangeInfo(
      cookie, net::CookieAccessResult(), net::CookieChangeCause::INSERTED));

  EXPECT_EQ(future.Get<0>(), cookie.Name());
  EXPECT_EQ(future.Get<1>(), cookie.ExpiryDate());
  EXPECT_EQ(update_expiration_date_call_count_, 1u);
  EXPECT_EQ(cookie_expiration_date_, cookie.ExpiryDate());
}

TEST_F(BoundSessionCookieObserverTest,
       CookieOverWriteDoesNotTriggerANotification) {
  CreateObserver();
  update_expiration_date_call_count_ = 0;

  net::CanonicalCookie cookie =
      BoundSessionTestCookieManager::CreateCookie(kGaiaUrl, kSIDTSCookieName);
  // No notification should be fired for `net::CookieChangeCause::OVERWRITE`.
  // Replacing an existing cookie is actually a two-phase
  // delete + set operation, so we get an extra notification.
  // Note: This is tested in the for loop below when `OnCookieChange` is
  // triggered that the `update_expiration_date_call_count_` hasn't increased
  // due to this call.
  cookie_manager_->DispatchCookieChange(net::CookieChangeInfo(
      cookie, net::CookieAccessResult(), net::CookieChangeCause::OVERWRITE));

  task_environment_.RunUntilIdle();
  EXPECT_EQ(update_expiration_date_call_count_, 0u);
  EXPECT_EQ(cookie_expiration_date_, base::Time());
}

TEST_F(BoundSessionCookieObserverTest, CookieDeleted) {
  CreateObserver();

  net::CanonicalCookie cookie =
      BoundSessionTestCookieManager::CreateCookie(kGaiaUrl, kSIDTSCookieName);

  std::vector<net::CookieChangeCause> cookie_deleted{
      net::CookieChangeCause::UNKNOWN_DELETION,
      net::CookieChangeCause::EXPLICIT, net::CookieChangeCause::EVICTED,
      net::CookieChangeCause::EXPIRED_OVERWRITE};
  size_t expected_update_expiration_date_call_count =
      update_expiration_date_call_count_;
  for (auto cookie_change_cause : cookie_deleted) {
    SCOPED_TRACE(net::CookieChangeCauseToString(cookie_change_cause));
    base::test::TestFuture<const std::string&, base::Time> future;
    SetNextCookieChangeCallback(future.GetCallback());
    cookie_manager_->DispatchCookieChange(net::CookieChangeInfo(
        cookie, net::CookieAccessResult(), cookie_change_cause));
    expected_update_expiration_date_call_count++;
    EXPECT_EQ(future.Get<0>(), cookie.Name());
    EXPECT_EQ(future.Get<1>(), base::Time());
    EXPECT_EQ(update_expiration_date_call_count_,
              expected_update_expiration_date_call_count);
    EXPECT_TRUE(cookie_expiration_date_.is_null());
  }
}

TEST_F(BoundSessionCookieObserverTest, CookieExpired) {
  CreateObserver();

  net::CanonicalCookie cookie = BoundSessionTestCookieManager::CreateCookie(
      kGaiaUrl, kSIDTSCookieName, base::Time::Now() - base::Minutes(1));

  base::test::TestFuture<const std::string&, base::Time> future;
  SetNextCookieChangeCallback(future.GetCallback());
  cookie_manager_->DispatchCookieChange(net::CookieChangeInfo(
      cookie, net::CookieAccessResult(), net::CookieChangeCause::EXPIRED));
  EXPECT_EQ(future.Get<0>(), cookie.Name());
  EXPECT_EQ(future.Get<1>(), cookie.ExpiryDate());
  EXPECT_EQ(update_expiration_date_call_count_, 2u);
  EXPECT_EQ(cookie_expiration_date_, cookie.ExpiryDate());
}

TEST_F(BoundSessionCookieObserverTest, OnCookieChangeListenerConnectionError) {
  // Set cookie.
  net::CanonicalCookie cookie =
      BoundSessionTestCookieManager::CreateCookie(kGaiaUrl, kSIDTSCookieName);
  cookie_manager_->SetCanonicalCookie(cookie, kGaiaUrl, net::CookieOptions(),
                                      base::DoNothing());
  CreateObserver();
  EXPECT_EQ(update_expiration_date_call_count_, 1u);
  EXPECT_EQ(cookie_expiration_date_, cookie.ExpiryDate());

  base::test::TestFuture<const std::string&, base::Time> future_removed;
  SetNextCookieChangeCallback(future_removed.GetCallback());

  // Reset the cookie manager to simulate
  // `OnCookieChangeListenerConnectionError`.
  // The new `cookie_manager_` doesn't have the cookie, it is expected to
  // trigger a notification that the cookie has been removed.
  ResetCookieManager();

  // Expect a notification that the cookie was removed.
  EXPECT_EQ(future_removed.Get<1>(), base::Time());
  EXPECT_EQ(update_expiration_date_call_count_, 2u);
  EXPECT_TRUE(cookie_expiration_date_.is_null());

  // Trigger a cookie change to verify the cookie listener has been hooked up
  // to the new `cookie_manager_`.
  // Insert event.
  base::test::TestFuture<const std::string&, base::Time> future_inserted;
  SetNextCookieChangeCallback(future_inserted.GetCallback());
  cookie_manager_->DispatchCookieChange(net::CookieChangeInfo(
      cookie, net::CookieAccessResult(), net::CookieChangeCause::INSERTED));

  EXPECT_EQ(future_inserted.Get<1>(), cookie.ExpiryDate());
  EXPECT_EQ(update_expiration_date_call_count_, 3u);
  EXPECT_EQ(cookie_expiration_date_, cookie.ExpiryDate());
}
}  // namespace
