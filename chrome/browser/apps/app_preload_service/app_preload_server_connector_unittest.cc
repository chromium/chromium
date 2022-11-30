// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_preload_service/app_preload_server_connector.h"

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "chrome/browser/apps/app_preload_service/device_info_manager.h"
#include "chrome/browser/apps/app_preload_service/preload_app_definition.h"
#include "chrome/browser/apps/app_preload_service/proto/app_provisioning.pb.h"
#include "chrome/browser/apps/app_preload_service/proto/common.pb.h"
#include "content/public/test/browser_task_environment.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
static constexpr char kServerUrl[] =
    "http://localhost:9876/v1/app_provisioning/apps?alt=proto";
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
      device_info, test_shared_loader_factory_,
      base::OnceCallback<void(std::vector<PreloadAppDefinition>)>());

  EXPECT_EQ(method, "POST");
  EXPECT_EQ(method_override_header, "GET");
  EXPECT_EQ(content_type, "application/json");
  EXPECT_EQ(body,
            "{\"board\":\"brya\",\"chrome_os_version\":{\"ash_chrome\":\"10.10."
            "10\",\"platform\":\"12345.0.0\"},\"language\":\"en-US\",\"model\":"
            "\"taniks\"}");
}

TEST_F(AppPreloadServerConnectorTest, GetAppsForFirstLoginSuccessfulResponse) {
  proto::AppProvisioningResponse response;
  auto* app = response.add_apps_to_install();
  auto* app_group = app->mutable_app_group();
  app_group->set_name("Peanut Types");

  url_loader_factory_.AddResponse(kServerUrl, response.SerializeAsString());

  base::RunLoop run_loop;
  server_connector_.GetAppsForFirstLogin(
      DeviceInfo(), test_shared_loader_factory_,
      base::BindLambdaForTesting([&](std::vector<PreloadAppDefinition> apps) {
        EXPECT_EQ(apps.size(), 1u);
        EXPECT_EQ(apps[0].GetName(), "Peanut Types");
        run_loop.Quit();
      }));
  run_loop.Run();
}

}  // namespace apps
