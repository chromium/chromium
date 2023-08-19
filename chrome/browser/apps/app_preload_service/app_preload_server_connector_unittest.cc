// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_preload_service/app_preload_server_connector.h"

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "chrome/browser/apps/almanac_api_client/device_info_manager.h"
#include "chrome/browser/apps/almanac_api_client/proto/client_context.pb.h"
#include "chrome/browser/apps/app_preload_service/preload_app_definition.h"
#include "chrome/browser/apps/app_preload_service/proto/app_preload.pb.h"
#include "content/public/test/browser_task_environment.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {

constexpr char kServerRoundTripHistogram[] =
    "AppPreloadService.ServerRoundTripTimeForFirstLogin";

}  // namespace

namespace apps {

class AppPreloadServerConnectorTest : public testing::Test {
 public:
  AppPreloadServerConnectorTest()
      : test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &url_loader_factory_)) {}

 protected:
  network::TestURLLoaderFactory url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  AppPreloadServerConnector server_connector_;
  base::HistogramTester histograms_;

 private:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(AppPreloadServerConnectorTest, GetAppsForFirstLoginRequest) {
  // We only set enough fields to verify that context protos are attached to the
  // request.
  DeviceInfo device_info;
  device_info.board = "brya";
  device_info.user_type = "unmanaged";

  std::string method;
  std::string method_override_header;
  std::string content_type;
  std::string body;

  url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        request.headers.GetHeader(net::HttpRequestHeaders::kContentType,
                                  &content_type);
        request.headers.GetHeader("X-HTTP-Method-Override",
                                  &method_override_header);
        method = request.method;
        body = network::GetUploadData(request);
      }));

  server_connector_.GetAppsForFirstLogin(
      device_info, test_shared_loader_factory_, base::DoNothing());

  EXPECT_EQ(method, "POST");
  EXPECT_EQ(method_override_header, "GET");
  EXPECT_EQ(content_type, "application/x-protobuf");

  proto::AppPreloadListRequest request;
  ASSERT_TRUE(request.ParseFromString(body));

  EXPECT_EQ(request.device_context().board(), "brya");
  EXPECT_EQ(request.user_context().user_type(),
            apps::proto::ClientUserContext::USERTYPE_UNMANAGED);
}

TEST_F(AppPreloadServerConnectorTest, GetAppsForFirstLoginSuccessfulResponse) {
  proto::AppPreloadListResponse response;
  auto* app = response.add_apps_to_install();
  app->set_name("Peanut Types");

  url_loader_factory_.AddResponse(
      AppPreloadServerConnector::GetServerUrl().spec(),
      response.SerializeAsString());

  base::test::TestFuture<absl::optional<std::vector<PreloadAppDefinition>>>
      test_callback;
  server_connector_.GetAppsForFirstLogin(
      DeviceInfo(), test_shared_loader_factory_, test_callback.GetCallback());
  auto apps = test_callback.Get();
  EXPECT_TRUE(apps.has_value());
  EXPECT_EQ(apps->size(), 1u);
  EXPECT_EQ(apps.value()[0].GetName(), "Peanut Types");

  histograms_.ExpectTotalCount(kServerRoundTripHistogram, 1);
}

TEST_F(AppPreloadServerConnectorTest, GetAppsForFirstLoginServerError) {
  url_loader_factory_.AddResponse(
      AppPreloadServerConnector::GetServerUrl().spec(), /*content=*/"",
      net::HTTP_INTERNAL_SERVER_ERROR);

  base::test::TestFuture<absl::optional<std::vector<PreloadAppDefinition>>>
      result;
  server_connector_.GetAppsForFirstLogin(
      DeviceInfo(), test_shared_loader_factory_, result.GetCallback());
  EXPECT_FALSE(result.Get().has_value());

  histograms_.ExpectTotalCount(kServerRoundTripHistogram, 0);
}

TEST_F(AppPreloadServerConnectorTest, GetAppsForFirstLoginNetworkError) {
  url_loader_factory_.AddResponse(
      AppPreloadServerConnector::GetServerUrl(),
      network::mojom::URLResponseHead::New(), /*content=*/"",
      network::URLLoaderCompletionStatus(net::ERR_TIMED_OUT));

  base::test::TestFuture<absl::optional<std::vector<PreloadAppDefinition>>>
      result;
  server_connector_.GetAppsForFirstLogin(
      DeviceInfo(), test_shared_loader_factory_, result.GetCallback());
  EXPECT_FALSE(result.Get().has_value());

  histograms_.ExpectTotalCount(kServerRoundTripHistogram, 0);
}

}  // namespace apps
