// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_fetcher.h"

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/signin/public/base/test_signin_client.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_access_result.h"
#include "services/network/test/test_cookie_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

class FakeCookieManager : public network::TestCookieManager {
 public:
  void SetCanonicalCookie(const net::CanonicalCookie& cookie,
                          const GURL& source_url,
                          const net::CookieOptions& cookie_options,
                          SetCanonicalCookieCallback callback) override {
    cookie_ = cookie;
    if (callback) {
      std::move(callback).Run(net::CookieAccessResult());
    }
  }

  net::CanonicalCookie& cookie() { return cookie_; }

 private:
  net::CanonicalCookie cookie_;
};

class BoundSessionCookieFetcherTest : public testing::Test {
 public:
  BoundSessionCookieFetcherTest()
      : task_environment_(
            base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME) {
    std::unique_ptr<FakeCookieManager> fake_cookie_manager =
        std::make_unique<FakeCookieManager>();
    cookie_manager_ = fake_cookie_manager.get();
    signin_client_.set_cookie_manager(std::move(fake_cookie_manager));
  }

  ~BoundSessionCookieFetcherTest() override = default;

  void InitializeFetcher(base::OnceClosure on_done) {
    fetcher_ = std::make_unique<BoundSessionCookieFetcher>(
        &signin_client_,
        base::BindOnce(&BoundSessionCookieFetcherTest::OnCookieSet,
                       base::Unretained(this), std::move(on_done)));
  }

  void OnCookieSet(base::OnceClosure on_done,
                   absl::optional<const base::Time> result) {
    expected_expiry_date_ = result.value_or(base::Time());
    std::move(on_done).Run();
  }

  void VerifyCookie() {
    EXPECT_TRUE(cookie_manager_);
    constexpr char kSIDTSCookieName[] = "__Secure-1PSIDTS";
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
  std::unique_ptr<BoundSessionCookieFetcher> fetcher_;
  sync_preferences::TestingPrefServiceSyncable prefs_;
  TestSigninClient signin_client_{&prefs_};
  FakeCookieManager* cookie_manager_ = nullptr;
};

TEST_F(BoundSessionCookieFetcherTest, SetSIDTSCookie) {
  base::RunLoop run_loop;
  InitializeFetcher(run_loop.QuitClosure());
  task_environment_.FastForwardBy(base::Milliseconds(100));
  run_loop.Run();
  VerifyCookie();
}
