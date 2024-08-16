// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_discovery_service/launcher_app_almanac_endpoint.h"

#include <optional>

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

namespace apps {
namespace {

class LauncherAppAlmanacEndpointTest : public testing::Test {
 public:
  LauncherAppAlmanacEndpointTest()
      : test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &url_loader_factory_)) {}

 protected:
  network::TestURLLoaderFactory url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;

  DeviceInfo device_info_;

 private:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(LauncherAppAlmanacEndpointTest, GetAppsRequest) {
  std::string method;
  std::optional<std::string> method_override_header;
  std::optional<std::string> content_type;

  url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        content_type =
            request.headers.GetHeader(net::HttpRequestHeaders::kContentType);
        method_override_header =
            request.headers.GetHeader("X-HTTP-Method-Override");
        method = request.method;
      }));

  launcher_app_almanac_endpoint::GetApps(
      device_info_, *test_shared_loader_factory_, base::DoNothing());

  EXPECT_EQ(method, "POST");
  EXPECT_EQ(method_override_header, "GET");
  EXPECT_EQ(content_type, "application/x-protobuf");
}

TEST_F(LauncherAppAlmanacEndpointTest, GetAppsSuccess) {
  proto::LauncherAppResponse response;
  response.add_app_groups();

  url_loader_factory_.AddResponse(
      launcher_app_almanac_endpoint::GetServerUrl().spec(),
      response.SerializeAsString());

  base::test::TestFuture<std::optional<proto::LauncherAppResponse>>
      observed_response;
  launcher_app_almanac_endpoint::GetApps(device_info_,
                                       *test_shared_loader_factory_,
                                       observed_response.GetCallback());
  ASSERT_TRUE(observed_response.Get().has_value());
  EXPECT_EQ(observed_response.Get()->app_groups_size(), 1);
}

TEST_F(LauncherAppAlmanacEndpointTest, GetAppsEmptyResponse) {
  url_loader_factory_.AddResponse(
      launcher_app_almanac_endpoint::GetServerUrl().spec(), "");
  base::test::TestFuture<std::optional<proto::LauncherAppResponse>> response;
  launcher_app_almanac_endpoint::GetApps(
      device_info_, *test_shared_loader_factory_, response.GetCallback());
  ASSERT_TRUE(response.Get().has_value());
  EXPECT_EQ(response.Get()->app_groups_size(), 0);
}

TEST_F(LauncherAppAlmanacEndpointTest, GetAppsError) {
  url_loader_factory_.AddResponse(
      launcher_app_almanac_endpoint::GetServerUrl().spec(),
      /*content=*/"", net::HTTP_INTERNAL_SERVER_ERROR);

  base::test::TestFuture<std::optional<proto::LauncherAppResponse>> response;
  launcher_app_almanac_endpoint::GetApps(
      device_info_, *test_shared_loader_factory_, response.GetCallback());
  EXPECT_FALSE(response.Get().has_value());
}

TEST_F(LauncherAppAlmanacEndpointTest, GetAppsNetworkError) {
  url_loader_factory_.AddResponse(
      launcher_app_almanac_endpoint::GetServerUrl(),
      network::mojom::URLResponseHead::New(),
      /*content=*/"",
      network::URLLoaderCompletionStatus(net::ERR_INSUFFICIENT_RESOURCES));

  base::test::TestFuture<std::optional<proto::LauncherAppResponse>> response;
  launcher_app_almanac_endpoint::GetApps(
      device_info_, *test_shared_loader_factory_, response.GetCallback());
  EXPECT_FALSE(response.Get().has_value());
}

}  // namespace
}  // namespace apps
