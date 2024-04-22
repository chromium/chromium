// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/network_traversal_ice_config_fetcher.h"

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chrome/common/chrome_paths.h"
#include "google_apis/google_api_keys.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class NetworkTraversalIceConfigFetcherTest : public testing::Test {
 public:
  NetworkTraversalIceConfigFetcherTest()
      : test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)),
        ice_config_fetcher_(test_shared_loader_factory_) {}
  ~NetworkTraversalIceConfigFetcherTest() override = default;

  std::string GetApiUrl() const {
    return base::StrCat(
        {"https://networktraversal.googleapis.com/v1alpha/iceconfig?key=",
         google_apis::GetSharingAPIKey()});
  }

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

  void GetSuccessResponse(std::string* response) const {
    base::FilePath path;
    ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &path));
    path = path.AppendASCII("sharing");
    ASSERT_TRUE(base::PathExists(path));
    ASSERT_TRUE(base::ReadFileToString(
        path.AppendASCII("network_traversal_response.json"), response));
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  NetworkTraversalIceConfigFetcher ice_config_fetcher_;
  base::HistogramTester histogram_tester_;
};

TEST_F(NetworkTraversalIceConfigFetcherTest, ResponseSuccessful) {
  base::RunLoop run_loop;
  ice_config_fetcher_.GetIceServers(base::BindLambdaForTesting(
      [&](std::vector<::sharing::mojom::IceServerPtr> ice_servers) {
        CheckSuccessResponse(ice_servers);
        run_loop.Quit();
      }));

  const std::string expected_api_url = GetApiUrl();
  std::string response;
  GetSuccessResponse(&response);

  ASSERT_TRUE(test_url_loader_factory_.IsPending(expected_api_url, nullptr));

  test_url_loader_factory_.AddResponse(expected_api_url, response,
                                       net::HTTP_OK);
  run_loop.Run();

  const std::string metric_name = "Sharing.WebRtc.IceConfigFetched";
  histogram_tester_.ExpectTotalCount(metric_name, 1);
  histogram_tester_.ExpectBucketCount(metric_name, 2, 1);
}

TEST_F(NetworkTraversalIceConfigFetcherTest, ResponseError) {
  base::RunLoop run_loop;
  ice_config_fetcher_.GetIceServers(base::BindLambdaForTesting(
      [&](std::vector<::sharing::mojom::IceServerPtr> ice_servers) {
        // Makes sure that we at least return default servers in case of an
        // error.
        EXPECT_FALSE(ice_servers.empty());
        run_loop.Quit();
      }));

  const std::string expected_api_url = GetApiUrl();
  ASSERT_TRUE(test_url_loader_factory_.IsPending(expected_api_url, nullptr));

  test_url_loader_factory_.AddResponse(expected_api_url, "",
                                       net::HTTP_INTERNAL_SERVER_ERROR);
  run_loop.Run();

  const std::string metric_name = "Sharing.WebRtc.IceConfigFetched";
  histogram_tester_.ExpectTotalCount(metric_name, 1);
  histogram_tester_.ExpectBucketCount(metric_name, 0, 1);
}

TEST_F(NetworkTraversalIceConfigFetcherTest, OverlappingCalls) {
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
  // Second call overlaps before any responses are processed. This previously
  // prevented the first call from ever returning.
  ice_config_fetcher_.GetIceServers(base::BindLambdaForTesting(callback));

  const std::string expected_api_url = GetApiUrl();
  std::string response;
  GetSuccessResponse(&response);

  ASSERT_TRUE(test_url_loader_factory_.IsPending(expected_api_url, nullptr));

  test_url_loader_factory_.AddResponse(expected_api_url, response,
                                       net::HTTP_OK);
  run_loop.Run();

  const std::string metric_name = "Sharing.WebRtc.IceConfigFetched";
  histogram_tester_.ExpectTotalCount(metric_name, 2);
  histogram_tester_.ExpectBucketCount(metric_name, 2, 2);
}
