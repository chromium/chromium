// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/almanac_api_client/almanac_api_util.h"

#include <string>
#include <string_view>

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "net/base/net_errors.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace apps {
namespace {

struct TestProto {
  bool ParseFromString(std::string_view string) { return string == "valid"; }

  bool operator==(const TestProto& other) const { return true; }
};

class AlmanacApiUtilTest : public testing::Test {
 public:
  AlmanacApiUtilTest() = default;
  ~AlmanacApiUtilTest() override = default;

  base::expected<TestProto, QueryError> QueryEndpoint() {
    base::test::TestFuture<base::expected<TestProto, QueryError>> future;
    QueryAlmanacApi<TestProto>(
        test_url_loader_factory_, TRAFFIC_ANNOTATION_FOR_TESTS, "request body",
        "endpoint",
        /*max_response_size=*/1024 * 1024,
        /*error_histogram_name=*/std::nullopt, future.GetCallback());
    return future.Get();
  }

 protected:
  network::TestURLLoaderFactory test_url_loader_factory_;
  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(AlmanacApiUtilTest, GetEndpointUrl) {
  EXPECT_EQ(GetAlmanacEndpointUrl("").spec(),
            "https://chromeosalmanac-pa.googleapis.com/");
  EXPECT_EQ(GetAlmanacEndpointUrl("endpoint").spec(),
            "https://chromeosalmanac-pa.googleapis.com/endpoint");
  EXPECT_EQ(GetAlmanacEndpointUrl("v1/app-preload").spec(),
            "https://chromeosalmanac-pa.googleapis.com/v1/app-preload");
}

TEST_F(AlmanacApiUtilTest, QueryAlmanacApi_ConnectionFailed) {
  test_url_loader_factory_.AddResponse(
      GetAlmanacEndpointUrl("endpoint"), network::mojom::URLResponseHead::New(),
      /*content=*/"",
      network::URLLoaderCompletionStatus(net::ERR_CONNECTION_FAILED));
  EXPECT_EQ(QueryEndpoint(), base::unexpected(QueryError{
                                 QueryError::kConnectionError,
                                 "net error: net::ERR_CONNECTION_FAILED"}));
}

TEST_F(AlmanacApiUtilTest, QueryAlmanacApi_ServerError) {
  test_url_loader_factory_.AddResponse(GetAlmanacEndpointUrl("endpoint").spec(),
                                       /*content=*/"",
                                       net::HTTP_INTERNAL_SERVER_ERROR);
  EXPECT_EQ(QueryEndpoint(),
            base::unexpected(QueryError{QueryError::kConnectionError,
                                        "HTTP error code: 500"}));
}

TEST_F(AlmanacApiUtilTest, QueryAlmanacApi_ServerReject) {
  test_url_loader_factory_.AddResponse(GetAlmanacEndpointUrl("endpoint").spec(),
                                       /*content=*/"", net::HTTP_NOT_FOUND);
  EXPECT_EQ(QueryEndpoint(),
            base::unexpected(
                QueryError{QueryError::kBadRequest, "HTTP error code: 404"}));
}

TEST_F(AlmanacApiUtilTest, QueryAlmanacApi_ServerInvalid) {
  test_url_loader_factory_.AddResponse(GetAlmanacEndpointUrl("endpoint").spec(),
                                       /*content=*/"invalid", net::HTTP_OK);
  EXPECT_EQ(QueryEndpoint(), base::unexpected(QueryError{
                                 QueryError::kBadResponse, "Parsing failed"}));
}

TEST_F(AlmanacApiUtilTest, QueryAlmanacApi_ServerValid) {
  test_url_loader_factory_.AddResponse(GetAlmanacEndpointUrl("endpoint").spec(),
                                       /*content=*/"valid", net::HTTP_OK);
  EXPECT_EQ(QueryEndpoint(), base::ok(TestProto()));
}

}  // namespace
}  // namespace apps
