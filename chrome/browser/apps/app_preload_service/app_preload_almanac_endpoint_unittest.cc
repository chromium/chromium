// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_preload_service/app_preload_almanac_endpoint.h"

#include <optional>
#include <tuple>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/apps/almanac_api_client/device_info_manager.h"
#include "chrome/browser/apps/almanac_api_client/proto/client_context.pb.h"
#include "chrome/browser/apps/app_preload_service/app_preload_service.h"
#include "chrome/browser/apps/app_preload_service/preload_app_definition.h"
#include "chrome/browser/apps/app_preload_service/proto/app_preload.pb.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "content/public/test/browser_task_environment.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace apps {

class AppPreloadAlmanacEndpointTest : public testing::Test {
 public:
  AppPreloadAlmanacEndpointTest() {
    feature_list_.InitAndDisableFeature(kAppPreloadServiceEnableTestApps);
  }

  void SetUp() override {
    TestingProfile::Builder profile_builder;
    profile_builder.SetSharedURLLoaderFactory(
        url_loader_factory_.GetSafeWeakWrapper());
    profile_ = profile_builder.Build();
  }

  Profile* profile() { return profile_.get(); }

 protected:
  network::TestURLLoaderFactory url_loader_factory_;
  base::HistogramTester histograms_;
  base::test::ScopedFeatureList feature_list_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  ash::system::ScopedFakeStatisticsProvider fake_statistics_provider_;
  std::unique_ptr<TestingProfile> profile_;
};

TEST_F(AppPreloadAlmanacEndpointTest, GetAppsForFirstLoginRequest) {
  std::string body;

  base::RunLoop run_loop;
  url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        body = network::GetUploadData(request);
        run_loop.Quit();
      }));

  app_preload_almanac_endpoint::GetAppsForFirstLogin(profile(),
                                                     base::DoNothing());
  run_loop.Run();

  proto::AppPreloadListRequest request;
  ASSERT_TRUE(request.ParseFromString(body));

  EXPECT_TRUE(request.has_device_context());
  EXPECT_EQ(request.user_context().user_type(),
            apps::proto::ClientUserContext::USERTYPE_UNMANAGED);
}

TEST_F(AppPreloadAlmanacEndpointTest, GetAppsForFirstLoginSuccessfulResponse) {
  PackageId chrome_app =
      *PackageId::FromString("chromeapp:mgndgikekgjfcpckkfioiadnlibdjbkf");
  PackageId lacros_app = *PackageId::FromString("system:lacros-chrome");
  PackageId web_app1 = *PackageId::FromString("web:http://example.com/app1");
  PackageId android_app1 = *PackageId::FromString("android:com.example.app1");
  PackageId web_app2 = *PackageId::FromString("web:http://example.com/app2");
  PackageId web_app3 = *PackageId::FromString("web:http://example.com/app3");
  PackageId web_app4 = *PackageId::FromString("web:http://example.com/app4");

  auto type_chrome =
      proto::AppPreloadListResponse_LauncherType_LAUNCHER_TYPE_CHROME;
  auto type_app = proto::AppPreloadListResponse_LauncherType_LAUNCHER_TYPE_APP;
  auto type_folder =
      proto::AppPreloadListResponse_LauncherType_LAUNCHER_TYPE_FOLDER;
  proto::AppPreloadListResponse response;
  auto* app = response.add_apps_to_install();
  app->set_name("Peanut Types");

  // Add chrome with no package.
  auto* launcher_config_item = response.add_launcher_config();
  launcher_config_item->set_type(type_chrome);
  launcher_config_item->set_order(1);

  launcher_config_item = response.add_launcher_config();
  launcher_config_item->set_type(type_app);
  launcher_config_item->set_order(2);
  launcher_config_item->add_package_id(web_app1.ToString());
  launcher_config_item->add_package_id(android_app1.ToString());

  launcher_config_item = response.add_launcher_config();
  launcher_config_item->set_type(type_app);
  launcher_config_item->set_order(3);
  launcher_config_item->set_feature_flag("unknown");
  launcher_config_item->add_package_id(web_app2.ToString());

  launcher_config_item = response.add_launcher_config();
  launcher_config_item->set_type(type_app);
  launcher_config_item->set_order(4);
  launcher_config_item->set_feature_flag("AppPreloadServiceEnableTestApps");
  launcher_config_item->add_package_id(web_app3.ToString());

  // Add folder with web_app4.
  launcher_config_item = response.add_launcher_config();
  launcher_config_item->set_type(type_folder);
  launcher_config_item->set_order(5);
  launcher_config_item->set_folder_name("other-folder");
  auto* folder_item = launcher_config_item->add_child_config();
  folder_item->set_type(type_app);
  folder_item->set_order(1);
  folder_item->add_package_id(web_app4.ToString());

  auto* shelf_config_item = response.add_shelf_config();
  shelf_config_item->set_order(1);
  shelf_config_item->add_package_id(web_app1.ToString());
  shelf_config_item->add_package_id(android_app1.ToString());

  shelf_config_item = response.add_shelf_config();
  shelf_config_item->set_order(2);
  shelf_config_item->set_feature_flag("unknown");
  shelf_config_item->add_package_id(web_app2.ToString());

  shelf_config_item = response.add_shelf_config();
  shelf_config_item->set_order(3);
  shelf_config_item->set_feature_flag("AppPreloadServiceEnableTestApps");
  shelf_config_item->add_package_id(web_app3.ToString());

  url_loader_factory_.AddResponse(
      app_preload_almanac_endpoint::GetServerUrl().spec(),
      response.SerializeAsString());

  base::test::TestFuture<std::optional<std::vector<PreloadAppDefinition>>,
                         LauncherOrdering, ShelfPinOrdering>
      test_callback;
  app_preload_almanac_endpoint::GetAppsForFirstLogin(
      profile(), test_callback.GetCallback());

  auto apps = std::get<0>(test_callback.Get());
  EXPECT_TRUE(apps.has_value());
  EXPECT_EQ(apps->size(), 1u);
  EXPECT_EQ(apps.value()[0].GetName(), "Peanut Types");

  auto launcher_ordering = std::get<1>(test_callback.Get());
  EXPECT_EQ(launcher_ordering.size(), 2u);
  auto root_folder = launcher_ordering[""];
  EXPECT_EQ(root_folder.size(), 5u);
  EXPECT_EQ(root_folder[chrome_app].type, type_chrome);
  EXPECT_EQ(root_folder[chrome_app].order, 1u);
  EXPECT_EQ(root_folder[lacros_app].type, type_chrome);
  EXPECT_EQ(root_folder[lacros_app].order, 1u);
  EXPECT_EQ(root_folder[web_app1].type, type_app);
  EXPECT_EQ(root_folder[web_app1].order, 2u);
  EXPECT_EQ(root_folder[android_app1].type, type_app);
  EXPECT_EQ(root_folder[android_app1].order, 2u);
  EXPECT_EQ(root_folder["other-folder"].type, type_folder);
  EXPECT_EQ(root_folder["other-folder"].order, 5u);
  auto other_folder = launcher_ordering["other-folder"];
  EXPECT_EQ(other_folder.size(), 1u);
  EXPECT_EQ(other_folder[web_app4].type, type_app);
  EXPECT_EQ(other_folder[web_app4].order, 1u);

  auto shelf_pin_ordering = std::get<2>(test_callback.Get());
  EXPECT_EQ(shelf_pin_ordering.size(), 2u);
  EXPECT_EQ(shelf_pin_ordering[web_app1], 1u);
  EXPECT_EQ(shelf_pin_ordering[android_app1], 1u);
}

TEST_F(AppPreloadAlmanacEndpointTest, GetAppsForFirstLoginServerError) {
  url_loader_factory_.AddResponse(
      app_preload_almanac_endpoint::GetServerUrl().spec(), /*content=*/"",
      net::HTTP_INTERNAL_SERVER_ERROR);

  base::test::TestFuture<std::optional<std::vector<PreloadAppDefinition>>,
                         LauncherOrdering, ShelfPinOrdering>
      result;
  app_preload_almanac_endpoint::GetAppsForFirstLogin(profile(),
                                                     result.GetCallback());
  EXPECT_FALSE(std::get<0>(result.Get()).has_value());
}

TEST_F(AppPreloadAlmanacEndpointTest, GetAppsForFirstLoginNetworkError) {
  url_loader_factory_.AddResponse(
      app_preload_almanac_endpoint::GetServerUrl(),
      network::mojom::URLResponseHead::New(), /*content=*/"",
      network::URLLoaderCompletionStatus(net::ERR_TIMED_OUT));

  base::test::TestFuture<std::optional<std::vector<PreloadAppDefinition>>,
                         LauncherOrdering, ShelfPinOrdering>
      result;
  app_preload_almanac_endpoint::GetAppsForFirstLogin(profile(),
                                                     result.GetCallback());
  EXPECT_FALSE(std::get<0>(result.Get()).has_value());
}

}  // namespace apps
