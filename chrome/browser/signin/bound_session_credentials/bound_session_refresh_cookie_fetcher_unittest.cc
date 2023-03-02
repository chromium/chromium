// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/bound_session_refresh_cookie_fetcher.h"

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_test_cookie_manager.h"
#include "components/signin/public/base/test_signin_client.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/cookies/canonical_cookie.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
constexpr char kSIDTSCookieName[] = "__Secure-1PSIDTS";

class BoundSessionRefreshCookieFetcherTest : public testing::Test {
 public:
  BoundSessionRefreshCookieFetcherTest()
      : task_environment_(
            base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME) {
    std::unique_ptr<BoundSessionTestCookieManager> fake_cookie_manager =
        std::make_unique<BoundSessionTestCookieManager>();
    cookie_manager_ = fake_cookie_manager.get();
    signin_client_.set_cookie_manager(std::move(fake_cookie_manager));
  }

  ~BoundSessionRefreshCookieFetcherTest() override = default;

  void InitializeFetcher(base::OnceClosure on_done) {
    fetcher_ = std::make_unique<BoundSessionRefreshCookieFetcher>(
        &signin_client_, GaiaUrls::GetInstance()->secure_google_url(),
        kSIDTSCookieName);

    fetcher_->Start(
        base::BindOnce(&BoundSessionRefreshCookieFetcherTest::OnCookieSet,
                       base::Unretained(this), std::move(on_done)));
  }

  void OnCookieSet(base::OnceClosure on_done,
                   absl::optional<base::Time> result) {
    expected_expiry_date_ = result.value_or(base::Time());
    std::move(on_done).Run();
  }

  void VerifyCookie() {
    EXPECT_TRUE(cookie_manager_);
    constexpr char kFakeCookieValue[] = "FakeCookieValue";

    net::CanonicalCookie& cookie = cookie_manager_->cookie();
    EXPECT_TRUE(cookie.IsCanonical());
    EXPECT_EQ(cookie.ExpiryDate(), expected_expiry_date_);
    EXPECT_EQ(cookie.Domain(), ".google.com");
    EXPECT_EQ(cookie.Name(), kSIDTSCookieName);
    EXPECT_EQ(cookie.Value(), kFakeCookieValue);
    EXPECT_TRUE(cookie.IsExpired(base::Time::Now() + base::Minutes(10)));
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  base::Time expected_expiry_date_;
  std::unique_ptr<BoundSessionRefreshCookieFetcher> fetcher_;
  sync_preferences::TestingPrefServiceSyncable prefs_;
  TestSigninClient signin_client_{&prefs_};
  BoundSessionTestCookieManager* cookie_manager_ = nullptr;
};

TEST_F(BoundSessionRefreshCookieFetcherTest, SetSIDTSCookie) {
  base::RunLoop run_loop;
  InitializeFetcher(run_loop.QuitClosure());
  task_environment_.FastForwardBy(base::Milliseconds(100));
  run_loop.Run();
  VerifyCookie();
}
}  // namespace
