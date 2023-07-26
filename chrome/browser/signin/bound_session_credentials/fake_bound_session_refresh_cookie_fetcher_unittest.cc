// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/fake_bound_session_refresh_cookie_fetcher.h"

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
constexpr char k1PSIDTSCookieName[] = "__Secure-1PSIDTS";
constexpr char k3PSIDTSCookieName[] = "__Secure-3PSIDTS";

class FakeBoundSessionRefreshCookieFetcherTest : public testing::Test {
 public:
  FakeBoundSessionRefreshCookieFetcherTest()
      : task_environment_(
            base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME) {
    std::unique_ptr<BoundSessionTestCookieManager> fake_cookie_manager =
        std::make_unique<BoundSessionTestCookieManager>();
    cookie_manager_ = fake_cookie_manager.get();
    signin_client_.set_cookie_manager(std::move(fake_cookie_manager));
  }

  ~FakeBoundSessionRefreshCookieFetcherTest() override = default;

  void InitializeFetcher(base::OnceClosure on_done) {
    fetcher_ = std::make_unique<FakeBoundSessionRefreshCookieFetcher>(
        &signin_client_, GaiaUrls::GetInstance()->secure_google_url(),
        base::flat_set<std::string>({k1PSIDTSCookieName, k3PSIDTSCookieName}),
        /*unlock_automatically_in=*/base::Milliseconds(100));

    fetcher_->Start(
        base::BindOnce(&FakeBoundSessionRefreshCookieFetcherTest::OnCookieSet,
                       base::Unretained(this), std::move(on_done)));
  }

  void OnCookieSet(base::OnceClosure on_done,
                   BoundSessionRefreshCookieFetcher::Result result) {
    std::move(on_done).Run();
  }

  void VerifyCookies() {
    EXPECT_TRUE(cookie_manager_);
    constexpr char kFakeCookieValue[] = "FakeCookieValue";

    const base::flat_set<net::CanonicalCookie>& cookies =
        cookie_manager_->cookies();
    EXPECT_EQ(cookies.size(), 2u);
    for (const auto& cookie : cookies) {
      EXPECT_TRUE(cookie.IsCanonical());
      EXPECT_EQ(cookie.Domain(), ".google.com");
      EXPECT_TRUE(cookie.Name() == k1PSIDTSCookieName ||
                  cookie.Name() == k3PSIDTSCookieName);
      EXPECT_EQ(cookie.Value(), kFakeCookieValue);
      EXPECT_GT(cookie.ExpiryDate(), base::Time::Now());
      EXPECT_TRUE(cookie.IsExpired(base::Time::Now() + base::Minutes(10)));
    }
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<FakeBoundSessionRefreshCookieFetcher> fetcher_;
  sync_preferences::TestingPrefServiceSyncable prefs_;
  TestSigninClient signin_client_{&prefs_};
  raw_ptr<BoundSessionTestCookieManager> cookie_manager_ = nullptr;
};

TEST_F(FakeBoundSessionRefreshCookieFetcherTest, SetSIDTSCookies) {
  base::RunLoop run_loop;
  InitializeFetcher(run_loop.QuitClosure());
  task_environment_.FastForwardBy(base::Milliseconds(100));
  run_loop.Run();
  VerifyCookies();
}
}  // namespace
