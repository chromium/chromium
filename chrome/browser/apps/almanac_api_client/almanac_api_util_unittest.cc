// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/almanac_api_client/almanac_api_util.h"

#include <string>
#include <string_view>

#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/apps/almanac_api_client/proto/test_request.pb.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/net_errors.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
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

  void SetUp() override {
    TestingProfile::Builder profile_builder;
    profile_builder.SetSharedURLLoaderFactory(
        test_url_loader_factory_.GetSafeWeakWrapper());
    profile_ = profile_builder.Build();
  }

  base::expected<TestProto, QueryError> QueryEndpoint() {
    base::test::TestFuture<base::expected<TestProto, QueryError>> future;
    QueryAlmanacApi<TestProto>(
        test_url_loader_factory_.GetSafeWeakWrapper(),
        TRAFFIC_ANNOTATION_FOR_TESTS, "request body", "endpoint",
        /*max_response_size=*/1024 * 1024,
        /*error_histogram_name=*/std::nullopt, future.GetCallback());
    return future.Get();
  }

  base::expected<TestProto, QueryError> QueryEndpointWithContext(
      proto::TestRequest request) {
    base::test::TestFuture<base::expected<TestProto, QueryError>> future;
    QueryAlmanacApiWithContext<proto::TestRequest, TestProto>(
        profile_.get(), "endpoint", request, TRAFFIC_ANNOTATION_FOR_TESTS,
        /*max_response_size=*/1024 * 1024,
        /*error_histogram_name=*/std::nullopt, future.GetCallback());
    return future.Get();
  }

 protected:
  network::TestURLLoaderFactory test_url_loader_factory_;
  content::BrowserTaskEnvironment task_environment_;
  ash::system::ScopedFakeStatisticsProvider fake_statistics_provider_;
  std::unique_ptr<TestingProfile> profile_;
};

TEST_F(AlmanacApiUtilTest, GetEndpointUrl) {
  EXPECT_EQ(GetAlmanacEndpointUrl("").spec(),
            "https://chromeosalmanac-pa.googleapis.com/");
  EXPECT_EQ(GetAlmanacEndpointUrl("endpoint").spec(),
            "https://chromeosalmanac-pa.googleapis.com/endpoint");
  EXPECT_EQ(GetAlmanacEndpointUrl("v1/app-preload").spec(),
            "https://chromeosalmanac-pa.googleapis.com/v1/app-preload");
}

TEST_F(AlmanacApiUtilTest, QueryAlmanacApi_RequestSettings) {
  std::string method;
  std::optional<std::string> method_override_header;
  std::optional<std::string> content_type;
  std::string body;
  GURL url;

  base::RunLoop run_loop;
  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        url = request.url;
        content_type =
            request.headers.GetHeader(net::HttpRequestHeaders::kContentType);
        method_override_header =
            request.headers.GetHeader("X-HTTP-Method-Override");
        method = request.method;
        body = network::GetUploadData(request);
        run_loop.Quit();
      }));

  QueryAlmanacApi<TestProto>(
      test_url_loader_factory_.GetSafeWeakWrapper(),
      TRAFFIC_ANNOTATION_FOR_TESTS, "serialized proto", "endpoint",
      /*max_response_size=*/1024 * 1024,
      /*error_histogram_name=*/std::nullopt, base::DoNothing());
  run_loop.Run();
  EXPECT_EQ(url, GetAlmanacEndpointUrl("endpoint"));
  EXPECT_EQ(method, "POST");
  EXPECT_EQ(method_override_header, "GET");
  EXPECT_EQ(content_type, "application/x-protobuf");
  EXPECT_EQ(body, "serialized proto");
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

TEST_F(AlmanacApiUtilTest, QueryAlmanacApiWithContext_AddsContext) {
  proto::TestRequest request;
  request.set_package_id("web:foo");

  proto::TestRequest sent_request;
  base::RunLoop run_loop;

  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        ASSERT_TRUE(
            sent_request.ParseFromString(network::GetUploadData(request)));
      }));

  test_url_loader_factory_.AddResponse(GetAlmanacEndpointUrl("endpoint").spec(),
                                       /*content=*/"valid", net::HTTP_OK);
  EXPECT_EQ(QueryEndpointWithContext(request), base::ok(TestProto()));

  EXPECT_EQ(sent_request.package_id(), "web:foo");
  EXPECT_TRUE(sent_request.has_device_context());
  EXPECT_TRUE(sent_request.has_user_context());
}

}  // namespace
}  // namespace apps
