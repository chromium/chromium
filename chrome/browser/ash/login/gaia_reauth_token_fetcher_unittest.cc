// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/gaia_reauth_token_fetcher.h"

#include <string>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chromeos/ash/components/login/auth/recovery/service_constants.h"
#include "google_apis/google_api_keys.h"
#include "net/base/net_errors.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ash {
namespace {

constexpr char kApiKeyParameter[] = "key";
constexpr char kFakeReauthToken[] = "fake-reauth-request-token";
constexpr char kSuccessHistogram[] = "Login.ReauthToken.FetchDuration.Success";
constexpr char kFailureHistogram[] = "Login.ReauthToken.FetchDuration.Failure";

GURL GetExpectedFetchUrl() {
  return net::AppendQueryParameter(GetRecoveryServiceReauthTokenURL(),
                                   kApiKeyParameter, google_apis::GetAPIKey());
}

}  // namespace

class GaiaReauthTokenFetcherTest : public ::testing::Test {
 public:
  GaiaReauthTokenFetcherTest() = default;
  ~GaiaReauthTokenFetcherTest() override = default;

  scoped_refptr<network::SharedURLLoaderFactory> GetSharedURLLoaderFactory() {
    return url_loader_factory_.GetSafeWeakWrapper();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory url_loader_factory_;
};

TEST_F(GaiaReauthTokenFetcherTest, FetchSuccess) {
  base::HistogramTester histogram_tester;
  url_loader_factory_.AddResponse(
      GetExpectedFetchUrl().spec(),
      R"({"encodedReauthRequestToken": "fake-reauth-request-token"})",
      net::HTTP_OK);

  base::test::TestFuture<const std::string&> future;
  GaiaReauthTokenFetcher fetcher(GetSharedURLLoaderFactory(),
                                 future.GetCallback());
  fetcher.Fetch();

  EXPECT_EQ(future.Get(), kFakeReauthToken);
  histogram_tester.ExpectTotalCount(kSuccessHistogram, 1);
  histogram_tester.ExpectTotalCount(kFailureHistogram, 0);
}

TEST_F(GaiaReauthTokenFetcherTest,
       FetchHttpOkWithMissingBodyOnTimeoutFailsGracefully) {
  base::HistogramTester histogram_tester;
  url_loader_factory_.AddResponse(
      GetExpectedFetchUrl(), network::CreateURLResponseHead(net::HTTP_OK),
      std::string(), network::URLLoaderCompletionStatus(net::ERR_TIMED_OUT),
      network::TestURLLoaderFactory::Redirects(),
      network::TestURLLoaderFactory::kSendHeadersOnNetworkError);

  base::test::TestFuture<const std::string&> future;
  GaiaReauthTokenFetcher fetcher(GetSharedURLLoaderFactory(),
                                 future.GetCallback());
  fetcher.Fetch();

  EXPECT_TRUE(future.Get().empty());
  histogram_tester.ExpectTotalCount(kSuccessHistogram, 0);
  histogram_tester.ExpectTotalCount(kFailureHistogram, 1);
}

TEST_F(GaiaReauthTokenFetcherTest, FetchHttpErrorFails) {
  base::HistogramTester histogram_tester;
  url_loader_factory_.AddResponse(GetExpectedFetchUrl().spec(), std::string(),
                                  net::HTTP_INTERNAL_SERVER_ERROR);

  base::test::TestFuture<const std::string&> future;
  GaiaReauthTokenFetcher fetcher(GetSharedURLLoaderFactory(),
                                 future.GetCallback());
  fetcher.Fetch();

  EXPECT_TRUE(future.Get().empty());
  histogram_tester.ExpectTotalCount(kSuccessHistogram, 0);
  histogram_tester.ExpectTotalCount(kFailureHistogram, 1);
}

TEST_F(GaiaReauthTokenFetcherTest, FetchInvalidJsonFails) {
  base::HistogramTester histogram_tester;
  url_loader_factory_.AddResponse(GetExpectedFetchUrl().spec(), "invalid-json",
                                  net::HTTP_OK);

  base::test::TestFuture<const std::string&> future;
  GaiaReauthTokenFetcher fetcher(GetSharedURLLoaderFactory(),
                                 future.GetCallback());
  fetcher.Fetch();

  EXPECT_TRUE(future.Get().empty());
  histogram_tester.ExpectTotalCount(kSuccessHistogram, 0);
  histogram_tester.ExpectTotalCount(kFailureHistogram, 1);
}

}  // namespace ash
