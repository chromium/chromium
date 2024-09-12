// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_install/app_install_almanac_endpoint.h"

#include <optional>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/to_string.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/apps/almanac_api_client/device_info_manager.h"
#include "chrome/browser/apps/almanac_api_client/proto/client_context.pb.h"
#include "chrome/browser/apps/app_service/app_install/app_install.pb.h"
#include "chrome/browser/apps/app_service/app_install/app_install_types.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "content/public/test/browser_task_environment.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace apps {

namespace {

using ResponseFuture =
    base::test::TestFuture<base::expected<AppInstallData, QueryError>>;

const PackageId kTestPackageId(PackageType::kWeb, "https://example.com/");

}  // namespace

class AppInstallAlmanacEndpointTest : public testing::Test {
 public:
  AppInstallAlmanacEndpointTest() = default;

  void SetUp() override {
    TestingProfile::Builder profile_builder;
    profile_builder.SetSharedURLLoaderFactory(
        test_url_loader_factory_.GetSafeWeakWrapper());
    profile_ = profile_builder.Build();
  }

  Profile* profile() { return profile_.get(); }

 protected:
  network::TestURLLoaderFactory test_url_loader_factory_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  ash::system::ScopedFakeStatisticsProvider fake_statistics_provider_;
  std::unique_ptr<TestingProfile> profile_;
};

TEST_F(AppInstallAlmanacEndpointTest, GetAppInstallInfoRequest) {
  std::string body;

  base::RunLoop run_loop;
  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        body = network::GetUploadData(request);
        run_loop.Quit();
      }));

  app_install_almanac_endpoint::GetAppInstallInfo(
      profile(), PackageId(PackageType::kWeb, "https://example.com/"),
      base::DoNothing());
  run_loop.Run();

  proto::AppInstallRequest request;
  ASSERT_TRUE(request.ParseFromString(body));

  EXPECT_TRUE(request.has_device_context());
  EXPECT_EQ(request.user_context().user_type(),
            apps::proto::ClientUserContext::USERTYPE_UNMANAGED);
  EXPECT_EQ(request.package_id(), "web:https://example.com/");
}

TEST_F(AppInstallAlmanacEndpointTest, GetAppInstallInfoSuccessfulResponse) {
  proto::AppInstallResponse response;
  proto::AppInstallResponse_AppInstance& instance =
      *response.mutable_app_instance();
  instance.set_package_id("web:https://example.com/");
  instance.set_name("Example");
  instance.set_description("Description.");
  {
    proto::AppInstallResponse_Icon& icon = *instance.mutable_icon();
    icon.set_url("https://example.com/icon.png");
    icon.set_width_in_pixels(144);
    icon.set_mime_type("image/png");
    icon.set_is_masking_allowed(true);
  }
  {
    proto::AppInstallResponse_Screenshot& screenshot =
        *instance.add_screenshots();
    screenshot.set_url("https://example.com/screenshot1.png");
    screenshot.set_mime_type("image/png");
    screenshot.set_width_in_pixels(400);
    screenshot.set_height_in_pixels(400);
  }
  {
    proto::AppInstallResponse_Screenshot& screenshot =
        *instance.add_screenshots();
    screenshot.set_url("https://example.com/screenshot2.png");
    screenshot.set_mime_type("image/png");
    screenshot.set_width_in_pixels(800);
    screenshot.set_height_in_pixels(800);
  }
  instance.set_install_url("https://example.com/install");
  proto::AppInstallResponse_WebExtras& web_app_extras =
      *instance.mutable_web_extras();
  web_app_extras.set_document_url("https://example.com/start.html");
  web_app_extras.set_original_manifest_url("https://example.com/manifest.json");
  web_app_extras.set_scs_url(
      "https://almanac.chromium.org/example_manifest.json");
  web_app_extras.set_open_as_window(true);

  test_url_loader_factory_.AddResponse(
      app_install_almanac_endpoint::GetEndpointUrlForTesting().spec(),
      response.SerializeAsString());

  ResponseFuture response_future;
  app_install_almanac_endpoint::GetAppInstallInfo(
      profile(), kTestPackageId, response_future.GetCallback());
  EXPECT_TRUE(response_future.Get().has_value());

  AppInstallData expected_data(
      PackageId(PackageType::kWeb, "https://example.com/"));
  expected_data.name = "Example";
  expected_data.description = "Description.";
  expected_data.icon = AppInstallIcon{
      .url = GURL("https://example.com/icon.png"),
      .width_in_pixels = 144,
      .mime_type = "image/png",
      .is_masking_allowed = true,
  };
  expected_data.screenshots = {
      AppInstallScreenshot{
          .url = GURL("https://example.com/screenshot1.png"),
          .mime_type = "image/png",
          .width_in_pixels = 400,
          .height_in_pixels = 400,
      },
      AppInstallScreenshot{
          .url = GURL("https://example.com/screenshot2.png"),
          .mime_type = "image/png",
          .width_in_pixels = 800,
          .height_in_pixels = 800,
      }};
  expected_data.install_url = GURL("https://example.com/install");
  auto& web_app_data = expected_data.app_type_data.emplace<WebAppInstallData>();
  web_app_data.original_manifest_url =
      GURL("https://example.com/manifest.json");
  web_app_data.proxied_manifest_url =
      GURL("https://almanac.chromium.org/example_manifest.json");
  web_app_data.document_url = GURL("https://example.com/start.html");
  web_app_data.open_as_window = true;
  EXPECT_EQ(base::ToString(response_future.Get().value()),
            base::ToString(expected_data));
}

TEST_F(AppInstallAlmanacEndpointTest, GetAppInstallInfoMinimalResponse) {
  proto::AppInstallResponse response;
  proto::AppInstallResponse_AppInstance& instance =
      *response.mutable_app_instance();
  instance.set_package_id("android:com.foo.app");
  instance.set_name("Example");

  test_url_loader_factory_.AddResponse(
      app_install_almanac_endpoint::GetEndpointUrlForTesting().spec(),
      response.SerializeAsString());

  ResponseFuture response_future;
  app_install_almanac_endpoint::GetAppInstallInfo(
      profile(), kTestPackageId, response_future.GetCallback());

  AppInstallData expected_data(PackageId(PackageType::kArc, "com.foo.app"));
  expected_data.name = "Example";

  EXPECT_EQ(base::ToString(response_future.Get().value()),
            base::ToString(expected_data));
}

TEST_F(AppInstallAlmanacEndpointTest, GetAppInstallInfoIncompleteResponse) {
  proto::AppInstallResponse response;
  proto::AppInstallResponse_AppInstance& instance =
      *response.mutable_app_instance();
  instance.set_package_id("web:https://example.com/");

  test_url_loader_factory_.AddResponse(
      app_install_almanac_endpoint::GetEndpointUrlForTesting().spec(),
      response.SerializeAsString());

  ResponseFuture response_future;
  app_install_almanac_endpoint::GetAppInstallInfo(
      profile(), kTestPackageId, response_future.GetCallback());
  EXPECT_EQ(response_future.Get().error().type, QueryError::kBadResponse);
}

TEST_F(AppInstallAlmanacEndpointTest, GetAppInstallInfoMalformedResponse) {
  test_url_loader_factory_.AddResponse(
      app_install_almanac_endpoint::GetEndpointUrlForTesting().spec(),
      "Not a valid proto");

  ResponseFuture response_future;
  app_install_almanac_endpoint::GetAppInstallInfo(
      profile(), kTestPackageId, response_future.GetCallback());
  EXPECT_EQ(response_future.Get().error().type, QueryError::kBadResponse);
}

TEST_F(AppInstallAlmanacEndpointTest, GetAppInstallInfoWrongExtras) {
  proto::AppInstallResponse response;
  proto::AppInstallResponse_AppInstance& instance =
      *response.mutable_app_instance();
  instance.set_package_id("web:https://example.com/");
  instance.set_name("Example");
  instance.set_description("Description.");
  instance.mutable_android_extras();

  test_url_loader_factory_.AddResponse(
      app_install_almanac_endpoint::GetEndpointUrlForTesting().spec(),
      response.SerializeAsString());

  ResponseFuture response_future;
  app_install_almanac_endpoint::GetAppInstallInfo(
      profile(), PackageId(PackageType::kWeb, "https://example.com/"),
      response_future.GetCallback());
  EXPECT_EQ(response_future.Get().error().type, QueryError::kBadResponse);
}

TEST_F(AppInstallAlmanacEndpointTest, GetAppInstallInfoServerError) {
  test_url_loader_factory_.AddResponse(
      app_install_almanac_endpoint::GetEndpointUrlForTesting().spec(),
      /*content=*/"", net::HTTP_INTERNAL_SERVER_ERROR);

  ResponseFuture response_future;
  app_install_almanac_endpoint::GetAppInstallInfo(
      profile(), kTestPackageId, response_future.GetCallback());
  EXPECT_EQ(response_future.Get().error().type, QueryError::kConnectionError);
}

TEST_F(AppInstallAlmanacEndpointTest, GetAppInstallInfoNetworkError) {
  test_url_loader_factory_.AddResponse(
      app_install_almanac_endpoint::GetEndpointUrlForTesting(),
      network::mojom::URLResponseHead::New(), /*content=*/"",
      network::URLLoaderCompletionStatus(net::ERR_TIMED_OUT));

  ResponseFuture response_future;
  app_install_almanac_endpoint::GetAppInstallInfo(
      profile(), kTestPackageId, response_future.GetCallback());
  EXPECT_EQ(response_future.Get().error().type, QueryError::kConnectionError);
}

TEST_F(AppInstallAlmanacEndpointTest, GetAppInstallInfoNotFound) {
  test_url_loader_factory_.AddResponse(
      app_install_almanac_endpoint::GetEndpointUrlForTesting().spec(),
      /*content=*/"",

      net::HTTP_NOT_FOUND);

  ResponseFuture response_future;
  app_install_almanac_endpoint::GetAppInstallInfo(
      profile(), kTestPackageId, response_future.GetCallback());
  EXPECT_EQ(response_future.Get().error().type, QueryError::kBadRequest);
}

}  // namespace apps
