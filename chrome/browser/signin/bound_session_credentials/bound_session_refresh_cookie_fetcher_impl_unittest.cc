// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/bound_session_refresh_cookie_fetcher_impl.h"

#include <memory>
#include <string>

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_test_cookie_manager.h"
#include "components/signin/public/base/test_signin_client.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "net/base/net_errors.h"
#include "net/cookies/canonical_cookie.h"
#include "net/http/http_status_code.h"
#include "services/network/public/mojom/cookie_access_observer.mojom.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
using RefreshTestFuture =
    base::test::TestFuture<BoundSessionRefreshCookieFetcher::Result>;
}  // namespace

class BoundSessionRefreshCookieFetcherImplTest : public ::testing::Test {
 public:
  BoundSessionRefreshCookieFetcherImplTest() {
    test_url_loader_factory_ = signin_client_.GetTestURLLoaderFactory();
    fetcher_ = std::make_unique<BoundSessionRefreshCookieFetcherImpl>(
        &signin_client_, kGaiaUrl,
        base::flat_set<std::string>{k1PSIDTSCookieName, k3PSIDTSCookieName});
    UpdateCookieList();
  }

 protected:
  const GURL kGaiaUrl = GURL("https://google.com");
  const std::string k1PSIDTSCookieName = "__Secure-1PSIDTS";
  const std::string k3PSIDTSCookieName = "__Secure-3PSIDTS";

  void UpdateCookieList(
      const base::flat_set<std::string>& excluded_cookies = {}) {
    const std::string kCookiesNames[4] = {
        "__Secure-cookie1", k1PSIDTSCookieName, "__Secure-cookie2",
        k3PSIDTSCookieName};
    cookies_.clear();
    for (const auto& cookie_name : kCookiesNames) {
      if (excluded_cookies.contains(cookie_name)) {
        continue;
      }
      cookies_.emplace_back(
          BoundSessionTestCookieManager::CreateCookie(kGaiaUrl, cookie_name));
    }
  }

  std::vector<network::mojom::CookieOrLineWithAccessResultPtr>
  CreateReportedCookies(const net::CookieList& cookie_list) {
    std::vector<network::mojom::CookieOrLineWithAccessResultPtr>
        reported_cookies;
    for (auto cookie : cookie_list) {
      network::mojom::CookieOrLinePtr cookie_or_line =
          network::mojom::CookieOrLine::NewCookie(cookie);
      reported_cookies.push_back(
          network::mojom::CookieOrLineWithAccessResult::New(
              std::move(cookie_or_line), net::CookieAccessResult()));
    }
    return reported_cookies;
  }

  void SimulateOnCookiesAccessed(
      network::mojom::CookieAccessDetails::Type access_type) {
    std::vector<network::mojom::CookieAccessDetailsPtr> cookie_access_details;
    cookie_access_details.emplace_back(network::mojom::CookieAccessDetails::New(
        access_type, kGaiaUrl, net::SiteForCookies(),
        CreateReportedCookies(cookies_), absl::nullopt));
    fetcher_->OnCookiesAccessed(std::move(cookie_access_details));
  }

  bool reported_cookies_notified() {
    return fetcher_->reported_cookies_notified_;
  }

  bool expected_cookies_set() { return fetcher_->expected_cookies_set_; }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  sync_preferences::TestingPrefServiceSyncable prefs_;
  TestSigninClient signin_client_{&prefs_};
  raw_ptr<network::TestURLLoaderFactory> test_url_loader_factory_ = nullptr;
  std::unique_ptr<BoundSessionRefreshCookieFetcherImpl> fetcher_;
  net::CookieList cookies_;
};

TEST_F(BoundSessionRefreshCookieFetcherImplTest, SuccessExpectedCookieSet) {
  EXPECT_FALSE(signin_client_.AreNetworkCallsDelayed());
  RefreshTestFuture future;
  fetcher_->Start(future.GetCallback());

  EXPECT_EQ(test_url_loader_factory_->total_requests(), 1u);
  network::TestURLLoaderFactory::PendingRequest* pending_request =
      test_url_loader_factory_->GetPendingRequest(0);
  EXPECT_EQ(pending_request->request.url,
            "https://accounts.google.com/RotateBoundCookies");
  EXPECT_EQ(pending_request->request.method, "GET");
  EXPECT_EQ(pending_request->request.credentials_mode,
            network::mojom::CredentialsMode::kInclude);

  SimulateOnCookiesAccessed(network::mojom::CookieAccessDetails::Type::kChange);
  EXPECT_FALSE(future.IsReady());
  EXPECT_TRUE(reported_cookies_notified());
  EXPECT_TRUE(expected_cookies_set());

  test_url_loader_factory_->SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), "");

  EXPECT_TRUE(future.IsReady());
  EXPECT_EQ(future.Get(), BoundSessionRefreshCookieFetcher::Result::kSuccess);
}

TEST_F(BoundSessionRefreshCookieFetcherImplTest,
       SuccessCookiesReportedDelayed) {
  EXPECT_FALSE(signin_client_.AreNetworkCallsDelayed());
  RefreshTestFuture future;
  fetcher_->Start(future.GetCallback());

  EXPECT_EQ(test_url_loader_factory_->total_requests(), 1u);
  network::TestURLLoaderFactory::PendingRequest* pending_request =
      test_url_loader_factory_->GetPendingRequest(0);

  test_url_loader_factory_->SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), "");
  EXPECT_FALSE(future.IsReady());
  EXPECT_FALSE(reported_cookies_notified());

  SimulateOnCookiesAccessed(network::mojom::CookieAccessDetails::Type::kChange);
  EXPECT_TRUE(future.IsReady());
  EXPECT_TRUE(reported_cookies_notified());
  EXPECT_TRUE(expected_cookies_set());

  EXPECT_EQ(future.Get(), BoundSessionRefreshCookieFetcher::Result::kSuccess);
}

TEST_F(BoundSessionRefreshCookieFetcherImplTest,
       ResultNotReportedOnCookieRead) {
  EXPECT_FALSE(signin_client_.AreNetworkCallsDelayed());
  RefreshTestFuture future;
  fetcher_->Start(future.GetCallback());

  EXPECT_EQ(test_url_loader_factory_->total_requests(), 1u);
  network::TestURLLoaderFactory::PendingRequest* pending_request =
      test_url_loader_factory_->GetPendingRequest(0);

  test_url_loader_factory_->SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), "");
  EXPECT_FALSE(future.IsReady());
  EXPECT_FALSE(reported_cookies_notified());

  SimulateOnCookiesAccessed(network::mojom::CookieAccessDetails::Type::kRead);
  EXPECT_FALSE(future.IsReady());
  EXPECT_FALSE(reported_cookies_notified());

  SimulateOnCookiesAccessed(network::mojom::CookieAccessDetails::Type::kChange);
  EXPECT_EQ(future.Get(), BoundSessionRefreshCookieFetcher::Result::kSuccess);
}

TEST_F(BoundSessionRefreshCookieFetcherImplTest, CookiesNotReported) {
  EXPECT_FALSE(signin_client_.AreNetworkCallsDelayed());
  RefreshTestFuture future;
  fetcher_->Start(future.GetCallback());

  EXPECT_EQ(test_url_loader_factory_->total_requests(), 1u);
  network::TestURLLoaderFactory::PendingRequest* pending_request =
      test_url_loader_factory_->GetPendingRequest(0);

  test_url_loader_factory_->SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), "");
  EXPECT_FALSE(future.IsReady());
  EXPECT_FALSE(reported_cookies_notified());

  task_environment_.FastForwardBy(base::Milliseconds(100));
  EXPECT_TRUE(future.IsReady());
  EXPECT_FALSE(reported_cookies_notified());
  EXPECT_EQ(
      future.Get(),
      BoundSessionRefreshCookieFetcher::Result::kServerUnexepectedResponse);
}

TEST_F(BoundSessionRefreshCookieFetcherImplTest,
       CookiesReportedExpectedCookieNotSet) {
  EXPECT_FALSE(signin_client_.AreNetworkCallsDelayed());
  RefreshTestFuture future;
  fetcher_->Start(future.GetCallback());

  EXPECT_EQ(test_url_loader_factory_->total_requests(), 1u);
  network::TestURLLoaderFactory::PendingRequest* pending_request =
      test_url_loader_factory_->GetPendingRequest(0);

  UpdateCookieList(
      /*excluded_cookies=*/{k1PSIDTSCookieName, k3PSIDTSCookieName});
  SimulateOnCookiesAccessed(network::mojom::CookieAccessDetails::Type::kChange);
  EXPECT_FALSE(future.IsReady());
  EXPECT_TRUE(reported_cookies_notified());
  EXPECT_FALSE(expected_cookies_set());

  test_url_loader_factory_->SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), "");

  EXPECT_TRUE(future.IsReady());
  EXPECT_EQ(
      future.Get(),
      BoundSessionRefreshCookieFetcher::Result::kServerUnexepectedResponse);
}

TEST_F(BoundSessionRefreshCookieFetcherImplTest,
       CookiesReportedNotAllExpectedCookiesSet) {
  EXPECT_FALSE(signin_client_.AreNetworkCallsDelayed());
  RefreshTestFuture future;
  fetcher_->Start(future.GetCallback());

  EXPECT_EQ(test_url_loader_factory_->total_requests(), 1u);
  network::TestURLLoaderFactory::PendingRequest* pending_request =
      test_url_loader_factory_->GetPendingRequest(0);

  // Remove one of the expected cookies
  UpdateCookieList(/*excluded_cookies=*/{k3PSIDTSCookieName});
  SimulateOnCookiesAccessed(network::mojom::CookieAccessDetails::Type::kChange);
  EXPECT_FALSE(future.IsReady());
  EXPECT_TRUE(reported_cookies_notified());
  EXPECT_FALSE(expected_cookies_set());

  test_url_loader_factory_->SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), "");

  EXPECT_TRUE(future.IsReady());
  EXPECT_EQ(
      future.Get(),
      BoundSessionRefreshCookieFetcher::Result::kServerUnexepectedResponse);
}

TEST_F(BoundSessionRefreshCookieFetcherImplTest, FailureNetError) {
  EXPECT_FALSE(signin_client_.AreNetworkCallsDelayed());
  RefreshTestFuture future;
  fetcher_->Start(future.GetCallback());

  EXPECT_EQ(test_url_loader_factory_->total_requests(), 1u);
  network::TestURLLoaderFactory::PendingRequest* pending_request =
      test_url_loader_factory_->GetPendingRequest(0);

  network::URLLoaderCompletionStatus status(net::ERR_UNEXPECTED);
  test_url_loader_factory_->SimulateResponseForPendingRequest(
      pending_request->request.url, status,
      network::mojom::URLResponseHead::New(), std::string());

  EXPECT_TRUE(future.IsReady());
  EXPECT_FALSE(reported_cookies_notified());
  BoundSessionRefreshCookieFetcher::Result result = future.Get<0>();
  EXPECT_EQ(result, BoundSessionRefreshCookieFetcher::Result::kConnectionError);
}

TEST_F(BoundSessionRefreshCookieFetcherImplTest, FailureHttpError) {
  EXPECT_FALSE(signin_client_.AreNetworkCallsDelayed());
  RefreshTestFuture future;
  fetcher_->Start(future.GetCallback());

  EXPECT_EQ(test_url_loader_factory_->total_requests(), 1u);
  network::TestURLLoaderFactory::PendingRequest* pending_request =
      test_url_loader_factory_->GetPendingRequest(0);

  test_url_loader_factory_->SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), "", net::HTTP_UNAUTHORIZED);

  EXPECT_TRUE(future.IsReady());
  EXPECT_FALSE(reported_cookies_notified());
  BoundSessionRefreshCookieFetcher::Result result = future.Get<0>();
  EXPECT_EQ(result,
            BoundSessionRefreshCookieFetcher::Result::kServerPersistentError);
}

TEST_F(BoundSessionRefreshCookieFetcherImplTest,
       GetResultFromNetErrorAndHttpStatusCode) {
  // Connection error.
  EXPECT_EQ(fetcher_->GetResultFromNetErrorAndHttpStatusCode(
                net::ERR_CONNECTION_TIMED_OUT, absl::nullopt),
            BoundSessionRefreshCookieFetcher::Result::kConnectionError);
  // net::OK.
  EXPECT_EQ(
      fetcher_->GetResultFromNetErrorAndHttpStatusCode(net::OK, net::HTTP_OK),
      BoundSessionRefreshCookieFetcher::Result::kSuccess);
  // net::ERR_HTTP_RESPONSE_CODE_FAILURE
  EXPECT_EQ(fetcher_->GetResultFromNetErrorAndHttpStatusCode(
                net::ERR_HTTP_RESPONSE_CODE_FAILURE, net::HTTP_BAD_REQUEST),
            BoundSessionRefreshCookieFetcher::Result::kServerPersistentError);
  // Persistent error.
  EXPECT_EQ(fetcher_->GetResultFromNetErrorAndHttpStatusCode(
                net::OK, net::HTTP_BAD_REQUEST),
            BoundSessionRefreshCookieFetcher::Result::kServerPersistentError);
  EXPECT_EQ(fetcher_->GetResultFromNetErrorAndHttpStatusCode(
                net::OK, net::HTTP_NOT_FOUND),
            BoundSessionRefreshCookieFetcher::Result::kServerPersistentError);
  // Transient error.
  EXPECT_EQ(fetcher_->GetResultFromNetErrorAndHttpStatusCode(
                net::OK, net::HTTP_INTERNAL_SERVER_ERROR),
            BoundSessionRefreshCookieFetcher::Result::kServerTransientError);
  EXPECT_EQ(fetcher_->GetResultFromNetErrorAndHttpStatusCode(
                net::OK, net::HTTP_GATEWAY_TIMEOUT),
            BoundSessionRefreshCookieFetcher::Result::kServerTransientError);
}

TEST_F(BoundSessionRefreshCookieFetcherImplTest, NetworkDelayed) {
  signin_client_.SetNetworkCallsDelayed(true);
  RefreshTestFuture future;
  fetcher_->Start(future.GetCallback());
  EXPECT_EQ(test_url_loader_factory_->total_requests(), 0u);

  signin_client_.SetNetworkCallsDelayed(false);
  EXPECT_EQ(test_url_loader_factory_->total_requests(), 1u);
  network::TestURLLoaderFactory::PendingRequest* pending_request =
      test_url_loader_factory_->GetPendingRequest(0);
  EXPECT_EQ(pending_request->request.url,
            "https://accounts.google.com/RotateBoundCookies");
  test_url_loader_factory_->SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), "");

  EXPECT_TRUE(future.Wait());
}

TEST_F(BoundSessionRefreshCookieFetcherImplTest, OnCookiesAccessedRead) {
  EXPECT_FALSE(reported_cookies_notified());
  SimulateOnCookiesAccessed(network::mojom::CookieAccessDetails::Type::kRead);
  EXPECT_FALSE(reported_cookies_notified());
  EXPECT_FALSE(expected_cookies_set());
}

TEST_F(BoundSessionRefreshCookieFetcherImplTest, OnCookiesAccessedChange) {
  EXPECT_FALSE(reported_cookies_notified());
  SimulateOnCookiesAccessed(network::mojom::CookieAccessDetails::Type::kChange);
  EXPECT_TRUE(reported_cookies_notified());
  EXPECT_TRUE(expected_cookies_set());
}
