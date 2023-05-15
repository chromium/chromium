// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/bound_session_refresh_cookie_fetcher_impl.h"

#include <memory>
#include <string>

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "components/signin/public/base/test_signin_client.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class BoundSessionRefreshCookieFetcherImplTest : public ::testing::Test {
 public:
  BoundSessionRefreshCookieFetcherImplTest() {
    test_url_loader_factory_ = signin_client_.GetTestURLLoaderFactory();
    fetcher_ =
        std::make_unique<BoundSessionRefreshCookieFetcherImpl>(&signin_client_);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  sync_preferences::TestingPrefServiceSyncable prefs_;
  TestSigninClient signin_client_{&prefs_};
  raw_ptr<network::TestURLLoaderFactory> test_url_loader_factory_ = nullptr;
  std::unique_ptr<BoundSessionRefreshCookieFetcherImpl> fetcher_;
};

TEST_F(BoundSessionRefreshCookieFetcherImplTest, Success) {
  EXPECT_FALSE(signin_client_.AreNetworkCallsDelayed());
  base::test::TestFuture<BoundSessionRefreshCookieFetcher::Result> future;
  fetcher_->Start(future.GetCallback());

  EXPECT_EQ(test_url_loader_factory_->total_requests(), 1u);
  network::TestURLLoaderFactory::PendingRequest* pending_request =
      test_url_loader_factory_->GetPendingRequest(0);
  EXPECT_EQ(pending_request->request.url,
            "https://accounts.google.com/RotateBoundCookies");
  EXPECT_EQ(pending_request->request.method, "GET");
  EXPECT_EQ(pending_request->request.credentials_mode,
            network::mojom::CredentialsMode::kInclude);

  test_url_loader_factory_->SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), "");
  EXPECT_TRUE(future.Wait());
  BoundSessionRefreshCookieFetcher::Result result = future.Get();
  EXPECT_EQ(result.net_error, net::OK);
  EXPECT_EQ(result.response_code, net::HTTP_OK);
}

TEST_F(BoundSessionRefreshCookieFetcherImplTest, FailureNetError) {
  EXPECT_FALSE(signin_client_.AreNetworkCallsDelayed());
  base::test::TestFuture<BoundSessionRefreshCookieFetcher::Result> future;
  fetcher_->Start(future.GetCallback());

  EXPECT_EQ(test_url_loader_factory_->total_requests(), 1u);
  network::TestURLLoaderFactory::PendingRequest* pending_request =
      test_url_loader_factory_->GetPendingRequest(0);

  network::URLLoaderCompletionStatus status(net::ERR_UNEXPECTED);
  test_url_loader_factory_->SimulateResponseForPendingRequest(
      pending_request->request.url, status,
      network::mojom::URLResponseHead::New(), std::string());

  EXPECT_TRUE(future.Wait());
  BoundSessionRefreshCookieFetcher::Result result = future.Get();
  EXPECT_EQ(result.net_error, status.error_code);
  EXPECT_FALSE(result.response_code.has_value());
}

TEST_F(BoundSessionRefreshCookieFetcherImplTest, FailureHttpError) {
  EXPECT_FALSE(signin_client_.AreNetworkCallsDelayed());
  base::test::TestFuture<BoundSessionRefreshCookieFetcher::Result> future;
  fetcher_->Start(future.GetCallback());

  EXPECT_EQ(test_url_loader_factory_->total_requests(), 1u);
  network::TestURLLoaderFactory::PendingRequest* pending_request =
      test_url_loader_factory_->GetPendingRequest(0);

  test_url_loader_factory_->SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), "", net::HTTP_UNAUTHORIZED);

  EXPECT_TRUE(future.Wait());
  BoundSessionRefreshCookieFetcher::Result result = future.Get();
  EXPECT_EQ(result.net_error, net::ERR_HTTP_RESPONSE_CODE_FAILURE);
  EXPECT_EQ(result.response_code, net::HTTP_UNAUTHORIZED);
}

TEST_F(BoundSessionRefreshCookieFetcherImplTest, NetworkDelayed) {
  signin_client_.SetNetworkCallsDelayed(true);
  base::test::TestFuture<BoundSessionRefreshCookieFetcher::Result> future;
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

}  // namespace
