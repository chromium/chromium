// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_discovery_service/launcher_app_almanac_connector.h"

#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/apps/almanac_api_client/device_info_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace apps {
namespace {

class LauncherAppAlmanacConnectorTest : public testing::Test {
 public:
  LauncherAppAlmanacConnectorTest()
      : test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &url_loader_factory_)) {}

 protected:
  network::TestURLLoaderFactory url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;

  LauncherAppAlmanacConnector server_connector_;
  DeviceInfo device_info_;

 private:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(LauncherAppAlmanacConnectorTest, GetAppsRequest) {
  std::string method;
  std::string method_override_header;
  std::string content_type;

  url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        request.headers.GetHeader(net::HttpRequestHeaders::kContentType,
                                  &content_type);
        request.headers.GetHeader("X-HTTP-Method-Override",
                                  &method_override_header);
        method = request.method;
      }));

  server_connector_.GetApps(device_info_, test_shared_loader_factory_,
                            base::DoNothing());

  EXPECT_EQ(method, "POST");
  EXPECT_EQ(method_override_header, "GET");
  EXPECT_EQ(content_type, "application/x-protobuf");
}

TEST_F(LauncherAppAlmanacConnectorTest, GetAppsSuccess) {
  proto::LauncherAppResponse response;
  response.add_app_groups();

  url_loader_factory_.AddResponse(
      LauncherAppAlmanacConnector::GetServerUrl().spec(),
      response.SerializeAsString());

  base::test::TestFuture<absl::optional<proto::LauncherAppResponse>>
      observed_response;
  server_connector_.GetApps(device_info_, test_shared_loader_factory_,
                            observed_response.GetCallback());
  ASSERT_TRUE(observed_response.Get().has_value());
  EXPECT_EQ(observed_response.Get()->app_groups_size(), 1);
}

TEST_F(LauncherAppAlmanacConnectorTest, GetAppsEmptyResponse) {
  url_loader_factory_.AddResponse(
      LauncherAppAlmanacConnector::GetServerUrl().spec(), "");
  base::test::TestFuture<absl::optional<proto::LauncherAppResponse>> response;
  server_connector_.GetApps(device_info_, test_shared_loader_factory_,
                            response.GetCallback());
  ASSERT_TRUE(response.Get().has_value());
  EXPECT_EQ(response.Get()->app_groups_size(), 0);
}

TEST_F(LauncherAppAlmanacConnectorTest, GetAppsError) {
  url_loader_factory_.AddResponse(
      LauncherAppAlmanacConnector::GetServerUrl().spec(),
      /*content=*/"", net::HTTP_INTERNAL_SERVER_ERROR);

  base::test::TestFuture<absl::optional<proto::LauncherAppResponse>> response;
  server_connector_.GetApps(device_info_, test_shared_loader_factory_,
                            response.GetCallback());
  EXPECT_FALSE(response.Get().has_value());
}

TEST_F(LauncherAppAlmanacConnectorTest, GetAppsNetworkError) {
  url_loader_factory_.AddResponse(
      LauncherAppAlmanacConnector::GetServerUrl(),
      network::mojom::URLResponseHead::New(),
      /*content=*/"",
      network::URLLoaderCompletionStatus(net::ERR_INSUFFICIENT_RESOURCES));

  base::test::TestFuture<absl::optional<proto::LauncherAppResponse>> response;
  server_connector_.GetApps(device_info_, test_shared_loader_factory_,
                            response.GetCallback());
  EXPECT_FALSE(response.Get().has_value());
}

}  // namespace
}  // namespace apps
