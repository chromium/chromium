// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/tachyon_ice_config_fetcher.h"

#include "base/logging.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chrome/browser/nearby_sharing/proto/duration.pb.h"
#include "chrome/browser/nearby_sharing/proto/ice.pb.h"
#include "chrome/browser/nearby_sharing/proto/tachyon.pb.h"
#include "chrome/browser/nearby_sharing/proto/tachyon_common.pb.h"
#include "chrome/browser/nearby_sharing/proto/tachyon_enums.pb.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

namespace tachyon_proto = nearbyshare::tachyon_proto;

const char kApiUrl[] =
    "https://instantmessaging-pa.googleapis.com/v1/peertopeer:geticeserver";
const char kOAuthToken[] = "oauth_token";
const char kTestAccount[] = "test@test.test";
const char kIceConfigFetchedMetric[] = "Sharing.WebRtc.IceConfigFetched";
const char kResultMetric[] =
    "Nearby.Connections.InstantMessaging.TachyonIceConfigFetcher.Result";
const char kFailureReasonMetric[] =
    "Nearby.Connections.InstantMessaging.TachyonIceConfigFetcher.FailureReason";
const char kCacheHitMetric[] =
    "Nearby.Connections.InstantMessaging.TachyonIceConfigFetcher.CacheHit";
const char kTokenFetchSuccessMetric[] =
    "Nearby.Connections.InstantMessaging.TachyonIceConfigFetcher."
    "OAuthTokenFetchResult";
const int kLifetimeDurationSeconds = 86400;

void CheckSuccessResponse(
    const std::vector<::sharing::mojom::IceServerPtr>& ice_servers) {
  ASSERT_EQ(2u, ice_servers.size());

  // First response doesnt have credentials.
  ASSERT_EQ(1u, ice_servers[0]->urls.size());
  ASSERT_FALSE(ice_servers[0]->username);
  ASSERT_FALSE(ice_servers[0]->credential);

  // Second response has credentials.
  ASSERT_EQ(2u, ice_servers[1]->urls.size());
  ASSERT_EQ("username", ice_servers[1]->username);
  ASSERT_EQ("credential", ice_servers[1]->credential);
}

}  // namespace

class TachyonIceConfigFetcherTest : public testing::Test {
 public:
  TachyonIceConfigFetcherTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)),
        ice_config_fetcher_(identity_test_environment_.identity_manager(),
                            test_shared_loader_factory_) {
    identity_test_environment_.MakePrimaryAccountAvailable(
        kTestAccount, signin::ConsentLevel::kSignin);
  }
  ~TachyonIceConfigFetcherTest() override = default;

  std::string GetSuccessResponse() {
    tachyon_proto::GetICEServerResponse response;

    auto* config = response.mutable_ice_config();
    config->mutable_lifetime_duration()->set_seconds(kLifetimeDurationSeconds);
    auto* server1 = config->add_ice_servers();
    server1->add_urls("stun:url1");
    auto* server2 = config->add_ice_servers();
    server2->add_urls("turn:url2?transport=udp");
    server2->add_urls("turn:url3?transport=tcp");
    server2->set_username("username");
    server2->set_credential("credential");

    std::string output;
    response.SerializeToString(&output);
    return output;
  }

  void SetOAuthTokenSuccessful(bool success) {
    identity_test_environment_
        .WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
            success ? kOAuthToken : "", base::Time::Now());
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  TachyonIceConfigFetcher ice_config_fetcher_;
  base::HistogramTester histogram_tester_;
};

TEST_F(TachyonIceConfigFetcherTest, ResponseSuccessful) {
  base::RunLoop run_loop;
  ice_config_fetcher_.GetIceServers(base::BindLambdaForTesting(
      [&](std::vector<::sharing::mojom::IceServerPtr> ice_servers) {
        CheckSuccessResponse(ice_servers);
        run_loop.Quit();
      }));
  SetOAuthTokenSuccessful(true);

  std::string response = GetSuccessResponse();

  ASSERT_TRUE(test_url_loader_factory_.IsPending(kApiUrl, nullptr));

  test_url_loader_factory_.AddResponse(kApiUrl, response, net::HTTP_OK);
  run_loop.Run();

  histogram_tester_.ExpectTotalCount(kIceConfigFetchedMetric, 1);
  histogram_tester_.ExpectBucketCount(kIceConfigFetchedMetric, 2, 1);
  histogram_tester_.ExpectTotalCount(kResultMetric, 1);
  histogram_tester_.ExpectBucketCount(kResultMetric, 1, 1);
  histogram_tester_.ExpectTotalCount(kCacheHitMetric, 1);
  histogram_tester_.ExpectBucketCount(kCacheHitMetric, 0, 1);
  histogram_tester_.ExpectTotalCount(kFailureReasonMetric, 0);
  histogram_tester_.ExpectTotalCount(kTokenFetchSuccessMetric, 1);
  histogram_tester_.ExpectBucketCount(kTokenFetchSuccessMetric, 1, 1);
}

TEST_F(TachyonIceConfigFetcherTest, ResponseError) {
  base::RunLoop run_loop;
  ice_config_fetcher_.GetIceServers(base::BindLambdaForTesting(
      [&](std::vector<::sharing::mojom::IceServerPtr> ice_servers) {
        // Makes sure that we at least return default servers in case of an
        // error.
        EXPECT_FALSE(ice_servers.empty());
        run_loop.Quit();
      }));
  SetOAuthTokenSuccessful(true);

  ASSERT_TRUE(test_url_loader_factory_.IsPending(kApiUrl, nullptr));

  test_url_loader_factory_.AddResponse(kApiUrl, "",
                                       net::HTTP_INTERNAL_SERVER_ERROR);
  run_loop.Run();

  histogram_tester_.ExpectTotalCount(kIceConfigFetchedMetric, 1);
  histogram_tester_.ExpectBucketCount(kIceConfigFetchedMetric, 0, 1);
  histogram_tester_.ExpectTotalCount(kResultMetric, 1);
  histogram_tester_.ExpectBucketCount(kResultMetric, 0, 1);
  histogram_tester_.ExpectTotalCount(kCacheHitMetric, 1);
  histogram_tester_.ExpectBucketCount(kCacheHitMetric, 0, 1);
  histogram_tester_.ExpectTotalCount(kFailureReasonMetric, 1);
  histogram_tester_.ExpectBucketCount(kFailureReasonMetric, 500, 1);
  histogram_tester_.ExpectTotalCount(kTokenFetchSuccessMetric, 1);
  histogram_tester_.ExpectBucketCount(kTokenFetchSuccessMetric, 1, 1);
}

TEST_F(TachyonIceConfigFetcherTest, OverlappingCalls) {
  base::RunLoop run_loop;
  int counter = 2;
  auto callback = [&](std::vector<::sharing::mojom::IceServerPtr> ice_servers) {
    CheckSuccessResponse(ice_servers);
    counter -= 1;
    if (counter == 0) {
      run_loop.Quit();
    }
  };
  // First call.
  ice_config_fetcher_.GetIceServers(base::BindLambdaForTesting(callback));
  SetOAuthTokenSuccessful(true);

  // Second call overlaps before any responses are processed.
  ice_config_fetcher_.GetIceServers(base::BindLambdaForTesting(callback));
  SetOAuthTokenSuccessful(true);

  std::string response = GetSuccessResponse();

  ASSERT_TRUE(test_url_loader_factory_.IsPending(kApiUrl, nullptr));

  test_url_loader_factory_.AddResponse(kApiUrl, response, net::HTTP_OK);
  run_loop.Run();

  histogram_tester_.ExpectTotalCount(kIceConfigFetchedMetric, 2);
  histogram_tester_.ExpectBucketCount(kIceConfigFetchedMetric, 2, 2);
  histogram_tester_.ExpectTotalCount(kResultMetric, 2);
  histogram_tester_.ExpectBucketCount(kResultMetric, 1, 2);
  histogram_tester_.ExpectTotalCount(kCacheHitMetric, 2);
  histogram_tester_.ExpectBucketCount(kCacheHitMetric, 0, 2);
  histogram_tester_.ExpectTotalCount(kFailureReasonMetric, 0);
  histogram_tester_.ExpectTotalCount(kTokenFetchSuccessMetric, 2);
  histogram_tester_.ExpectBucketCount(kTokenFetchSuccessMetric, 1, 2);
}

TEST_F(TachyonIceConfigFetcherTest, IceServersCached) {
  auto callback = [](base::RunLoop* run_loop,
                     std::vector<::sharing::mojom::IceServerPtr> ice_servers) {
    CheckSuccessResponse(ice_servers);
    run_loop->Quit();
  };
  std::string response = GetSuccessResponse();

  // First call.
  auto run_loop = std::make_unique<base::RunLoop>();
  ice_config_fetcher_.GetIceServers(base::BindOnce(callback, run_loop.get()));
  SetOAuthTokenSuccessful(true);
  ASSERT_TRUE(test_url_loader_factory_.IsPending(kApiUrl));
  test_url_loader_factory_.SimulateResponseForPendingRequest(kApiUrl, response);

  // Complete first call before beginning second call
  run_loop->Run();

  // Second call returns cached result
  run_loop.reset(new base::RunLoop());
  ice_config_fetcher_.GetIceServers(base::BindOnce(callback, run_loop.get()));
  ASSERT_FALSE(test_url_loader_factory_.IsPending(kApiUrl));
  run_loop->Run();

  // Wait until the cache has expired.
  task_environment_.FastForwardBy(base::Seconds(kLifetimeDurationSeconds + 1));

  // Expired cache results in fetching the servers again.
  run_loop.reset(new base::RunLoop());
  ice_config_fetcher_.GetIceServers(base::BindOnce(callback, run_loop.get()));
  SetOAuthTokenSuccessful(true);
  ASSERT_TRUE(test_url_loader_factory_.IsPending(kApiUrl));
  test_url_loader_factory_.SimulateResponseForPendingRequest(kApiUrl, response);
  run_loop->Run();

  histogram_tester_.ExpectTotalCount(kIceConfigFetchedMetric, 2);
  histogram_tester_.ExpectBucketCount(kIceConfigFetchedMetric, 2, 2);
  histogram_tester_.ExpectTotalCount(kResultMetric, 2);
  histogram_tester_.ExpectBucketCount(kResultMetric, 1, 2);
  histogram_tester_.ExpectTotalCount(kCacheHitMetric, 3);
  histogram_tester_.ExpectBucketCount(kCacheHitMetric, 0, 2);
  histogram_tester_.ExpectTotalCount(kFailureReasonMetric, 0);
  histogram_tester_.ExpectTotalCount(kTokenFetchSuccessMetric, 2);
  histogram_tester_.ExpectBucketCount(kTokenFetchSuccessMetric, 1, 2);
}

TEST_F(TachyonIceConfigFetcherTest, OAuthTokenFailed) {
  base::RunLoop run_loop;
  ice_config_fetcher_.GetIceServers(base::BindLambdaForTesting(
      [&](std::vector<::sharing::mojom::IceServerPtr> ice_servers) {
        // Makes sure that we at least return default servers in case of an
        // error.
        EXPECT_FALSE(ice_servers.empty());
        run_loop.Quit();
      }));
  SetOAuthTokenSuccessful(false);
  ASSERT_EQ(0, test_url_loader_factory_.NumPending());
  run_loop.Run();

  histogram_tester_.ExpectTotalCount(kIceConfigFetchedMetric, 0);
  histogram_tester_.ExpectTotalCount(kResultMetric, 0);
  histogram_tester_.ExpectTotalCount(kCacheHitMetric, 1);
  histogram_tester_.ExpectBucketCount(kCacheHitMetric, 0, 1);
  histogram_tester_.ExpectTotalCount(kFailureReasonMetric, 0);
  histogram_tester_.ExpectTotalCount(kTokenFetchSuccessMetric, 1);
  histogram_tester_.ExpectBucketCount(kTokenFetchSuccessMetric, 0, 1);
}

TEST_F(TachyonIceConfigFetcherTest, OverlappingTokenFetch) {
  base::RunLoop run_loop;
  int counter = 2;
  auto callback = [&](std::vector<::sharing::mojom::IceServerPtr> ice_servers) {
    CheckSuccessResponse(ice_servers);
    counter -= 1;
    if (counter == 0) {
      run_loop.Quit();
    }
  };
  // First call.
  ice_config_fetcher_.GetIceServers(base::BindLambdaForTesting(callback));

  // Second call overlaps before the first has an OAuth token.
  ice_config_fetcher_.GetIceServers(base::BindLambdaForTesting(callback));

  // Return an OAuth token for both requests.
  SetOAuthTokenSuccessful(true);

  std::string response = GetSuccessResponse();

  ASSERT_TRUE(test_url_loader_factory_.IsPending(kApiUrl, nullptr));

  test_url_loader_factory_.AddResponse(kApiUrl, response, net::HTTP_OK);
  run_loop.Run();

  histogram_tester_.ExpectTotalCount(kIceConfigFetchedMetric, 2);
  histogram_tester_.ExpectBucketCount(kIceConfigFetchedMetric, 2, 2);
  histogram_tester_.ExpectTotalCount(kResultMetric, 2);
  histogram_tester_.ExpectBucketCount(kResultMetric, 1, 2);
  histogram_tester_.ExpectTotalCount(kCacheHitMetric, 2);
  histogram_tester_.ExpectBucketCount(kCacheHitMetric, 0, 2);
  histogram_tester_.ExpectTotalCount(kFailureReasonMetric, 0);
  histogram_tester_.ExpectTotalCount(kTokenFetchSuccessMetric, 2);
  histogram_tester_.ExpectBucketCount(kTokenFetchSuccessMetric, 1, 2);
}
