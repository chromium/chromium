// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_install/app_install_almanac_connector.h"

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/to_string.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/apps/almanac_api_client/device_info_manager.h"
#include "chrome/browser/apps/almanac_api_client/proto/client_context.pb.h"
#include "chrome/browser/apps/app_service/app_install/app_install.pb.h"
#include "chrome/browser/apps/app_service/app_install/app_install_types.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "content/public/test/browser_task_environment.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace apps {

namespace {

const PackageId kTestPackageId(AppType::kWeb, "https://example.com/");

}  // namespace

class AppInstallAlmanacConnectorTest : public testing::Test {
 public:
  AppInstallAlmanacConnectorTest() = default;

 protected:
  network::TestURLLoaderFactory test_url_loader_factory_;
  AppInstallAlmanacConnector connector_;

 private:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(AppInstallAlmanacConnectorTest, GetAppInstallInfoRequest) {
  DeviceInfo device_info;
  device_info.board = "brya";
  device_info.user_type = "unmanaged";

  std::string method;
  std::string method_override_header;
  std::string content_type;
  std::string body;

  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        request.headers.GetHeader(net::HttpRequestHeaders::kContentType,
                                  &content_type);
        request.headers.GetHeader("X-HTTP-Method-Override",
                                  &method_override_header);
        method = request.method;
        body = network::GetUploadData(request);
      }));

  connector_.GetAppInstallInfo(PackageId(AppType::kWeb, "https://example.com/"),
                               device_info, test_url_loader_factory_,
                               base::DoNothing());

  EXPECT_EQ(method, "POST");
  EXPECT_EQ(method_override_header, "GET");
  EXPECT_EQ(content_type, "application/x-protobuf");

  proto::AppInstallRequest request;
  ASSERT_TRUE(request.ParseFromString(body));

  EXPECT_EQ(request.device_context().board(), "brya");
  EXPECT_EQ(request.user_context().user_type(),
            apps::proto::ClientUserContext::USERTYPE_UNMANAGED);
  EXPECT_EQ(request.package_id(), "web:https://example.com/");
}

TEST_F(AppInstallAlmanacConnectorTest, GetAppInstallInfoSuccessfulResponse) {
  proto::AppInstallResponse response;
  proto::AppInstallResponse_AppInstance& instance =
      *response.mutable_app_instance();
  instance.set_package_id("web:https://example.com/");
  instance.set_name("Example");
  instance.set_description("Description.");
  proto::AppInstallResponse_Icon& icon = *instance.add_icons();
  icon.set_url("https://example.com/icon.png");
  icon.set_width_in_pixels(144);
  icon.set_mime_type("image/png");
  icon.set_is_masking_allowed(true);
  proto::AppInstallResponse_WebExtras& web_app_extras =
      *instance.mutable_web_extras();
  web_app_extras.set_document_url("https://example.com/start.html");
  web_app_extras.set_original_manifest_url("https://example.com/manifest.json");
  web_app_extras.set_scs_url(
      "https://almanac.chromium.org/example_manifest.json");

  test_url_loader_factory_.AddResponse(
      AppInstallAlmanacConnector::GetEndpointUrlForTesting().spec(),
      response.SerializeAsString());

  base::test::TestFuture<absl::optional<AppInstallData>> response_future;
  connector_.GetAppInstallInfo(kTestPackageId, DeviceInfo(),
                               test_url_loader_factory_,
                               response_future.GetCallback());
  EXPECT_TRUE(response_future.Get().has_value());

  AppInstallData expected_data(
      PackageId(AppType::kWeb, "https://example.com/"));
  expected_data.name = "Example";
  expected_data.description = "Description.";
  expected_data.icons = {{
      .url = GURL("https://example.com/icon.png"),
      .width_in_pixels = 144,
      .mime_type = "image/png",
      .is_masking_allowed = true,
  }};
  auto& web_app_data = expected_data.app_type_data.emplace<WebAppInstallData>();
  web_app_data.manifest_id = GURL("https://example.com/");
  web_app_data.original_manifest_url =
      GURL("https://example.com/manifest.json");
  web_app_data.proxied_manifest_url =
      GURL("https://almanac.chromium.org/example_manifest.json");
  web_app_data.document_url = GURL("https://example.com/start.html");
  EXPECT_EQ(base::ToString(*response_future.Get()),
            base::ToString(expected_data));
}

TEST_F(AppInstallAlmanacConnectorTest, GetAppInstallInfoIncompleteResponse) {
  proto::AppInstallResponse response;
  proto::AppInstallResponse_AppInstance& instance =
      *response.mutable_app_instance();
  instance.set_package_id("web:https://example.com/");

  test_url_loader_factory_.AddResponse(
      AppInstallAlmanacConnector::GetEndpointUrlForTesting().spec(),
      response.SerializeAsString());

  base::test::TestFuture<absl::optional<AppInstallData>> response_future;
  connector_.GetAppInstallInfo(kTestPackageId, DeviceInfo(),
                               test_url_loader_factory_,
                               response_future.GetCallback());
  EXPECT_FALSE(response_future.Get().has_value());
}

TEST_F(AppInstallAlmanacConnectorTest, GetAppInstallInfoMalformedResponse) {
  test_url_loader_factory_.AddResponse(
      AppInstallAlmanacConnector::GetEndpointUrlForTesting().spec(),
      "Not a valid proto");

  base::test::TestFuture<absl::optional<AppInstallData>> response_future;
  connector_.GetAppInstallInfo(kTestPackageId, DeviceInfo(),
                               test_url_loader_factory_,
                               response_future.GetCallback());
  EXPECT_FALSE(response_future.Get().has_value());
}

TEST_F(AppInstallAlmanacConnectorTest, GetAppInstallInfoServerError) {
  test_url_loader_factory_.AddResponse(
      AppInstallAlmanacConnector::GetEndpointUrlForTesting().spec(),
      /*content=*/"", net::HTTP_INTERNAL_SERVER_ERROR);

  base::test::TestFuture<absl::optional<AppInstallData>> response_future;
  connector_.GetAppInstallInfo(kTestPackageId, DeviceInfo(),
                               test_url_loader_factory_,
                               response_future.GetCallback());
  EXPECT_FALSE(response_future.Get().has_value());
}

TEST_F(AppInstallAlmanacConnectorTest, GetAppInstallInfoNetworkError) {
  test_url_loader_factory_.AddResponse(
      AppInstallAlmanacConnector::GetEndpointUrlForTesting(),
      network::mojom::URLResponseHead::New(), /*content=*/"",
      network::URLLoaderCompletionStatus(net::ERR_TIMED_OUT));

  base::test::TestFuture<absl::optional<AppInstallData>> response_future;
  connector_.GetAppInstallInfo(kTestPackageId, DeviceInfo(),
                               test_url_loader_factory_,
                               response_future.GetCallback());
  EXPECT_FALSE(response_future.Get().has_value());
}

}  // namespace apps
