// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_preload_service/app_preload_server_connector.h"

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/apps/app_preload_service/device_info_manager.h"
#include "chrome/browser/apps/app_preload_service/preload_app_definition.h"
#include "chrome/browser/apps/app_preload_service/proto/app_provisioning.pb.h"
#include "components/version_info/channel.h"
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

 private:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(AppPreloadServerConnectorTest, GetAppsForFirstLoginRequest) {
  DeviceInfo device_info;
  device_info.board = "brya";
  device_info.model = "taniks";
  device_info.user_type = "unmanaged";
  device_info.version_info.ash_chrome = "10.10.10";
  device_info.version_info.platform = "12345.0.0";
  device_info.version_info.channel = version_info::Channel::STABLE;
  device_info.locale = "en-US";

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

  proto::AppProvisioningListAppsRequest request;
  ASSERT_TRUE(request.ParseFromString(body));

  EXPECT_EQ(request.board(), "brya");
  EXPECT_EQ(request.language(), "en-US");
  EXPECT_EQ(request.model(), "taniks");
  EXPECT_EQ(request.user_type(),
            apps::proto::AppProvisioningListAppsRequest::USERTYPE_UNMANAGED);
  EXPECT_EQ(request.chrome_os_version().ash_chrome(), "10.10.10");
  EXPECT_EQ(request.chrome_os_version().platform(), "12345.0.0");
  EXPECT_EQ(request.chrome_os_version().channel(),
            apps::proto::AppProvisioningListAppsRequest::CHANNEL_STABLE);
}

TEST_F(AppPreloadServerConnectorTest, GetAppsForFirstLoginSuccessfulResponse) {
  proto::AppProvisioningListAppsResponse response;
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
}

}  // namespace apps
