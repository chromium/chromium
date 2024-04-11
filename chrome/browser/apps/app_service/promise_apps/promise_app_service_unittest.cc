// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/promise_apps/promise_app_service.h"

#include <optional>

#include "ash/constants/ash_features.h"
#include "base/functional/callback_helpers.h"
#include "base/scoped_observation.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_factory.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app_almanac_connector.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app_metrics.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app_registry_cache.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app_update.h"
#include "chrome/browser/apps/app_service/promise_apps/proto/promise_app.pb.h"
#include "chrome/browser/ash/app_list/arc/arc_app_test.h"
#include "chrome/browser/ash/apps/apk_web_app_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "components/services/app_service/public/cpp/package_id.h"
#include "components/webapps/browser/install_result_code.h"
#include "content/public/test/browser_task_environment.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/skia_util.h"

namespace apps {

const PackageId kTestPackageId(PackageType::kArc, "test.package.name");

class PromiseAppServiceTest : public testing::Test,
                              public PromiseAppRegistryCache::Observer {
 public:
  void SetUp() override {
    feature_list_.InitAndEnableFeature(ash::features::kPromiseIcons);
    url_loader_factory_ = std::make_unique<network::TestURLLoaderFactory>();
    testing::Test::SetUp();
    TestingProfile::Builder profile_builder;
    profile_builder.SetSharedURLLoaderFactory(
        url_loader_factory_->GetSafeWeakWrapper());
    profile_ = profile_builder.Build();
    arc_test_.SetUp(profile_.get());
    test_shared_loader_factory_ = url_loader_factory_->GetSafeWeakWrapper();
    service_ = proxy()->PromiseAppService();
    service_->SetSkipApiKeyCheckForTesting(true);
    service_->SetSkipAlmanacForTesting(false);
  }

  void TearDown() override {
    arc_test_.StopArcInstance();
    arc_test_.TearDown();
  }

  network::TestURLLoaderFactory* url_loader_factory() {
    return url_loader_factory_.get();
  }

  PromiseAppRegistryCache* cache() {
    return service_->PromiseAppRegistryCache();
  }

  AppServiceProxy* proxy() {
    return AppServiceProxyFactory::GetForProfile(profile_.get());
  }

  PromiseAppIconCache* icon_cache() { return service_->PromiseAppIconCache(); }

  PromiseAppService* service() { return service_; }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

  ash::ApkWebAppService* apk_web_app_service() {
    return ash::ApkWebAppService::Get(profile_.get());
  }

  PromiseAppIconPtr CreatePromiseAppIcon(int width) {
    PromiseAppIconPtr icon = std::make_unique<PromiseAppIcon>();
    icon->icon = gfx::test::CreateBitmap(width, width);
    icon->width_in_pixels = width;
    return icon;
  }

  // Create a data string that represents an image of the requested dimensions.
  // This is used to produce mock content for the url_loader_factory.
  std::string CreateImageString(int width) {
    SkBitmap bitmap = gfx::test::CreateBitmap(width, width);
    std::vector<unsigned char> compressed;
    gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, true, &compressed);
    std::string image_string(compressed.begin(), compressed.end());
    return image_string;
  }

  // Set the number of updates we expect the Promise App Registry Cache to
  // receive in the test.
  void ExpectNumUpdates(int num_updates) {
    expected_num_updates_ = num_updates;
    current_num_updates_ = 0;
    if (!obs_.IsObserving()) {
      obs_.Observe(cache());
    }
  }

  void WaitForPromiseAppUpdates() {
    if (expected_num_updates_ == current_num_updates_) {
      return;
    }
    wait_run_loop_ = std::make_unique<base::RunLoop>();
    wait_run_loop_->Run();
  }

  void FinishApkWebAppInstall(const std::string& package_name,
                              const std::string& app_id) {
    apk_web_app_service()->OnDidFinishInstall(
        package_name, app_id, true, std::nullopt,
        webapps::InstallResultCode::kWriteDataFailed);
  }

  // apps::PromiseAppRegistryCache::Observer:
  void OnPromiseAppUpdate(const PromiseAppUpdate& update) override {
    current_num_updates_++;
    if (wait_run_loop_ && wait_run_loop_->running() &&
        expected_num_updates_ == current_num_updates_) {
      wait_run_loop_->Quit();
    }
  }

  void OnPromiseAppRegistryCacheWillBeDestroyed(
      apps::PromiseAppRegistryCache* cache) override {
    obs_.Reset();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<base::RunLoop> wait_run_loop_;
  std::unique_ptr<Profile> profile_;
  ArcAppTest arc_test_;
  raw_ptr<apps::PromiseAppService> service_;
  std::unique_ptr<network::TestURLLoaderFactory> url_loader_factory_;
  base::ScopedObservation<PromiseAppRegistryCache,
                          PromiseAppRegistryCache::Observer>
      obs_{this};
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  ash::system::ScopedFakeStatisticsProvider fake_statistics_provider_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  base::HistogramTester histogram_tester_;
  base::test::ScopedFeatureList feature_list_;

  // Tracks how many times we should expect OnPromiseAppUpdate to be called
  // before proceeding with a unit test.
  int expected_num_updates_;
  int current_num_updates_;
};

TEST_F(PromiseAppServiceTest, AlmanacResponseUpdatesPromiseAppName) {
  proto::PromiseAppResponse response;
  response.set_package_id(kTestPackageId.ToString());
  response.set_name("Name");
  response.add_icons();
  response.mutable_icons(0)->set_url("www.image");

  url_loader_factory()->AddResponse(
      PromiseAppAlmanacConnector::GetServerUrl().spec(),
      response.SerializeAsString());

  // We expect 2 Promise App Registry Cache updates in this test:
  // The first update is when we initially register the promise app in the
  // cache. The second update is when we take the app name provided by the
  // Almanac API response and save it to the promise app object.
  ExpectNumUpdates(/*num_updates=*/2);

  // Add promise app to the cache, which will trigger an Almanac API call.
  service()->OnPromiseApp(std::make_unique<PromiseApp>(kTestPackageId));

  // Wait for all the updates to trigger.
  WaitForPromiseAppUpdates();

  const PromiseApp* promise_app_result = cache()->GetPromiseApp(kTestPackageId);
  EXPECT_TRUE(promise_app_result->name.has_value());
  EXPECT_EQ(promise_app_result->name.value(), "Name");
}

// Tests that icons can be successfully downloaded from the URLs provided by an
// Almanac response.
TEST_F(PromiseAppServiceTest, AlmanacIconsGetDownloadedThenAddedToIconCache) {
  std::string url = "http://image.test/test.png";
  std::string url_other = "http://image.test/second-test.png";

  proto::PromiseAppResponse response;
  response.set_package_id(kTestPackageId.ToString());
  response.set_name("Name");
  response.add_icons();
  response.mutable_icons(0)->set_url(url);
  response.add_icons();
  response.mutable_icons(1)->set_url(url_other);

  // Set up response for Almanac endpoint.
  url_loader_factory()->AddResponse(
      PromiseAppAlmanacConnector::GetServerUrl().spec(),
      response.SerializeAsString());

  // Set up mock icon response.
  url_loader_factory()->AddResponse(url, CreateImageString(512));
  url_loader_factory()->AddResponse(url_other, CreateImageString(1024));

  // Confirm there aren't any icons for the package yet.
  EXPECT_FALSE(icon_cache()->DoesPackageIdHaveIcons(kTestPackageId));

  // We expect 3 Promise App Registry Cache updates in this test:
  // The first update is when we initially register the promise app in the
  // cache. The second update is when we take the app name provided by the
  // Almanac API response and save it to the promise app object. The third is
  // after we finish downloading all the icons for the promise app and mark the
  // promise app as ready to be shown to the user.
  ExpectNumUpdates(/*num_updates=*/3);
  service()->OnPromiseApp(std::make_unique<PromiseApp>(kTestPackageId));

  // Wait for all the updates to trigger.
  WaitForPromiseAppUpdates();

  // Verify that there are 2 icons now saved in cache.
  EXPECT_TRUE(icon_cache()->DoesPackageIdHaveIcons(kTestPackageId));
  std::vector<PromiseAppIcon*> icons =
      icon_cache()->GetIconsForTesting(kTestPackageId);
  EXPECT_EQ(icons.size(), 2u);
  EXPECT_EQ(icons[0]->width_in_pixels, 512);
  EXPECT_TRUE(
      gfx::BitmapsAreEqual(icons[0]->icon, gfx::test::CreateBitmap(512, 512)));
  EXPECT_EQ(icons[1]->width_in_pixels, 1024);
  EXPECT_TRUE(gfx::BitmapsAreEqual(icons[1]->icon,
                                   gfx::test::CreateBitmap(1024, 1024)));
  histogram_tester().ExpectBucketCount(kPromiseAppIconTypeHistogram,
                                       PromiseAppIconType::kPlaceholderIcon, 0);
  histogram_tester().ExpectBucketCount(kPromiseAppIconTypeHistogram,
                                       PromiseAppIconType::kRealIcon, 1);

  // Verify that the promise app is allowed to be visible now.
  const PromiseApp* promise_app_result = cache()->GetPromiseApp(kTestPackageId);
  EXPECT_TRUE(promise_app_result->should_show);
}

// Tests that we can deal with broken URLs/ failed icon downloads and avoid
// updating the icon cache.
TEST_F(PromiseAppServiceTest, FailedIconDownloadsDoNotUpdateIconCache) {
  proto::PromiseAppResponse response;
  response.set_package_id(kTestPackageId.ToString());
  response.set_name("Name");
  response.add_icons();
  response.mutable_icons(0)->set_url("broken-url");

  // Set up response for Almanac endpoint.
  url_loader_factory()->AddResponse(
      PromiseAppAlmanacConnector::GetServerUrl().spec(),
      response.SerializeAsString());

  // Set up mock icon response.
  url_loader_factory()->AddResponse("broken-url", "invalid icon");

  // Confirm there aren't any icons for the package yet.
  EXPECT_FALSE(icon_cache()->DoesPackageIdHaveIcons(kTestPackageId));

  // We expect 3 Promise App Registry Cache updates in this test:
  // The first update is when we initially register the promise app in the
  // cache. The second update is when we take the app name provided by the
  // Almanac API response and save it to the promise app object. The third is
  // after we attempt to download the icons for the promise app (but fail) and
  // mark the promise app as ready to be shown to the user.
  ExpectNumUpdates(/*num_updates=*/3);
  service()->OnPromiseApp(std::make_unique<PromiseApp>(kTestPackageId));

  // Wait for all the updates to trigger.
  WaitForPromiseAppUpdates();

  // Icon cache should still be empty.
  EXPECT_FALSE(icon_cache()->DoesPackageIdHaveIcons(kTestPackageId));
  histogram_tester().ExpectBucketCount(kPromiseAppIconTypeHistogram,
                                       PromiseAppIconType::kPlaceholderIcon, 1);
  histogram_tester().ExpectBucketCount(kPromiseAppIconTypeHistogram,
                                       PromiseAppIconType::kRealIcon, 0);
}

// Tests that for an empty Almanac response, we still show the promise app (but
// it will eventually result in using a placeholder icon).
TEST_F(PromiseAppServiceTest, ShowPromiseAppDespiteErrorAlmanacResponse) {
  url_loader_factory()->AddResponse(
      PromiseAppAlmanacConnector::GetServerUrl().spec(),
      /*content=*/"", net::HTTP_INTERNAL_SERVER_ERROR);

  // We expect a Promise App Registry Cache update when we initially register
  // the promise app in the cache, then a subsequent update that sets the
  // promise app should_show=true after receiving the Almanac response.
  ExpectNumUpdates(2);

  // Add promise app to the cache, which will trigger an Almanac API call.
  service()->OnPromiseApp(std::make_unique<PromiseApp>(kTestPackageId));

  // Wait for all the updates to trigger.
  WaitForPromiseAppUpdates();

  // Confirm that we have no icons in the icon cache but that the promise app is
  // visible anyway.
  std::vector<PromiseAppIcon*> icons_saved =
      icon_cache()->GetIconsForTesting(kTestPackageId);
  EXPECT_EQ(icons_saved.size(), 0u);
  const PromiseApp* promise_app_result = cache()->GetPromiseApp(kTestPackageId);
  EXPECT_TRUE(promise_app_result->should_show);
}

TEST_F(PromiseAppServiceTest, CompleteAppInstallationRemovesPromiseApp) {
  AppType app_type = AppType::kArc;
  std::string identifier = "test.com.example";
  PackageId package_id(PackageType::kArc, identifier);

  // Register test promise app.
  PromiseAppPtr promise_app = std::make_unique<PromiseApp>(package_id);
  promise_app->should_show = true;
  promise_app->status = PromiseStatus::kInstalling;
  service()->OnPromiseApp(std::move(promise_app));

  // Confirm that the promise app gets registered.
  const PromiseApp* promise_app_registered = cache()->GetPromiseApp(package_id);
  EXPECT_TRUE(promise_app_registered);
  EXPECT_TRUE(promise_app_registered->should_show.has_value());
  EXPECT_TRUE(promise_app_registered->should_show.value());
  EXPECT_EQ(promise_app_registered->status, PromiseStatus::kInstalling);

  // Add test icons.
  icon_cache()->SaveIcon(package_id, CreatePromiseAppIcon(100));
  icon_cache()->SaveIcon(package_id, CreatePromiseAppIcon(200));
  EXPECT_TRUE(icon_cache()->DoesPackageIdHaveIcons(package_id));

  // Register (i.e. "install") an app with a matching package ID. This should
  // trigger removal of the promise app.
  std::string app_id = "asdfghjkl";
  apps::AppPtr app = std::make_unique<apps::App>(app_type, app_id);
  app->publisher_id = identifier;
  app->readiness = apps::Readiness::kReady;

  std::vector<apps::AppPtr> apps;
  apps.push_back(std::move(app));
  proxy()->OnApps(std::move(apps), app_type,
                  /*should_notify_initialized=*/false);

  // Confirm that the promise app is now absent from the Promise App Registry
  // and Promise App Icon Cache.
  EXPECT_FALSE(cache()->HasPromiseApp(package_id));
  EXPECT_FALSE(icon_cache()->DoesPackageIdHaveIcons(package_id));
}

TEST_F(PromiseAppServiceTest,
       SuppressPromiseAppsForAppsRegisteredInAppRegistryCache) {
  AppType app_type = AppType::kArc;
  std::string identifier = "test.com.example";
  PackageId package_id(PackageType::kArc, identifier);

  // Register sample test app in AppRegistryCache.
  apps::AppPtr app = std::make_unique<apps::App>(app_type, "asdfghjkl");
  app->publisher_id = identifier;
  app->readiness = apps::Readiness::kReady;
  std::vector<apps::AppPtr> apps;
  apps.push_back(std::move(app));
  proxy()->OnApps(std::move(apps), app_type,
                  /*should_notify_initialized=*/false);

  // Attempt to register test promise app with a matching package ID.
  PromiseAppPtr promise_app = std::make_unique<PromiseApp>(package_id);
  promise_app->status = PromiseStatus::kPending;
  service()->OnPromiseApp(std::move(promise_app));

  // Confirm that the promise app does NOT get created.
  EXPECT_FALSE(cache()->HasPromiseApp(package_id));
}

TEST_F(PromiseAppServiceTest, AllowPromiseAppsForReinstallingApps) {
  AppType app_type = AppType::kArc;
  std::string identifier = "test.com.example";
  PackageId package_id(PackageType::kArc, identifier);

  // Register an app in AppRegistryCache, marked as uninstalled.
  apps::AppPtr app = std::make_unique<apps::App>(app_type, "asdfghjkl");
  app->publisher_id = identifier;
  app->readiness = apps::Readiness::kUninstalledByUser;
  std::vector<apps::AppPtr> apps;
  apps.push_back(std::move(app));
  proxy()->OnApps(std::move(apps), app_type,
                  /*should_notify_initialized=*/false);

  // Register test promise app with a matching package ID.
  PromiseAppPtr promise_app = std::make_unique<PromiseApp>(package_id);
  promise_app->status = PromiseStatus::kPending;
  service()->OnPromiseApp(std::move(promise_app));

  // Confirm that the promise app does get created.
  EXPECT_TRUE(cache()->HasPromiseApp(package_id));
}

TEST_F(PromiseAppServiceTest, WebOnlyTwaInstallationReplacesArcPromiseApp) {
  std::string package_name = "com.example.this";
  PackageId package_id(PackageType::kArc, package_name);
  std::string app_id = "asdfghjkl";

  // Add a promise app to the cache.
  std::unique_ptr<apps::PromiseApp> promise_app =
      std::make_unique<apps::PromiseApp>(package_id);
  promise_app->should_show = true;
  promise_app->status = apps::PromiseStatus::kInstalling;
  service()->OnPromiseApp(std::move(promise_app));
  EXPECT_TRUE(cache()->HasPromiseApp(package_id));

  apk_web_app_service()->AddInstallingWebApkPackageName(app_id, package_name);

  // Register the installed web app to indicate that the promise app
  // successfully installed.
  apps::AppPtr app = std::make_unique<apps::App>(apps::AppType::kWeb, app_id);
  app->publisher_id = "https://something.com";
  app->readiness = apps::Readiness::kReady;
  std::vector<apps::AppPtr> apps;
  apps.push_back(std::move(app));
  proxy()->OnApps(std::move(apps), apps::AppType::kWeb,
                  /*should_notify_initialized=*/false);

  // Confirm that the web app installation matched with the ARC promise app and
  // removed the promise app from the PromiseAppRegistryCache.
  EXPECT_FALSE(cache()->HasPromiseApp(package_id));
}

TEST_F(PromiseAppServiceTest, FailedWebAppInstallationRemovesTwaPromiseApp) {
  std::string package_name = "test.com.example";
  PackageId package_id(PackageType::kArc, package_name);
  std::string app_id = "asdfghjkl";

  // Register test promise app.
  PromiseAppPtr promise_app = std::make_unique<PromiseApp>(package_id);
  promise_app->should_show = true;
  promise_app->status = PromiseStatus::kInstalling;
  promise_app->progress = 0.9;
  service()->OnPromiseApp(std::move(promise_app));
  EXPECT_TRUE(cache()->HasPromiseApp(package_id));

  // Simulate the TWA failed web app installation.
  apk_web_app_service()->AddInstallingWebApkPackageName(app_id, package_name);
  FinishApkWebAppInstall(package_name, app_id);

  // Confirm that the promise app was removed from PromiseAppRegistryCache.
  EXPECT_FALSE(cache()->HasPromiseApp(package_id));
}

TEST_F(PromiseAppServiceTest, PromiseAppTypeRecorded) {
  // Case 1: ARC promise app.
  std::string arc_package_name = "com.arc.example";
  PackageId arc_package_id(PackageType::kArc, arc_package_name);
  std::string arc_app_id = "qwerty";

  // Add an ARC package ID promise app to the cache.
  std::unique_ptr<apps::PromiseApp> arc_promise_app =
      std::make_unique<apps::PromiseApp>(arc_package_id);
  service()->OnPromiseApp(std::move(arc_promise_app));

  // Register the installed ARC app.
  apps::AppPtr arc_app =
      std::make_unique<apps::App>(apps::AppType::kArc, arc_app_id);
  arc_app->publisher_id = arc_package_name;
  arc_app->readiness = apps::Readiness::kReady;
  std::vector<apps::AppPtr> arc_apps;
  arc_apps.push_back(std::move(arc_app));
  proxy()->OnApps(std::move(arc_apps), apps::AppType::kArc,
                  /*should_notify_initialized=*/false);

  histogram_tester().ExpectBucketCount(kPromiseAppTypeHistogram,
                                       PromiseAppType::kArc, 1);
  histogram_tester().ExpectBucketCount(kPromiseAppTypeHistogram,
                                       PromiseAppType::kTwa, 0);

  // Case 2: TWA promise app: should have an ARC package ID but results in a web
  // app.
  std::string twa_package_name = "com.twa.example";
  PackageId twa_package_id(PackageType::kArc, twa_package_name);
  std::string twa_app_id = "asdfghjkl";

  // Add an ARC package ID promise app to the cache.
  std::unique_ptr<apps::PromiseApp> twa_promise_app =
      std::make_unique<apps::PromiseApp>(twa_package_id);
  service()->OnPromiseApp(std::move(twa_promise_app));

  // Register the installed web app.
  apk_web_app_service()->AddInstallingWebApkPackageName(twa_app_id,
                                                        twa_package_name);
  apps::AppPtr twa_app =
      std::make_unique<apps::App>(apps::AppType::kWeb, twa_app_id);
  twa_app->publisher_id = "https://something.com";
  twa_app->readiness = apps::Readiness::kReady;
  std::vector<apps::AppPtr> twa_apps;
  twa_apps.push_back(std::move(twa_app));
  proxy()->OnApps(std::move(twa_apps), apps::AppType::kWeb,
                  /*should_notify_initialized=*/false);

  histogram_tester().ExpectBucketCount(kPromiseAppTypeHistogram,
                                       PromiseAppType::kArc, 1);
  histogram_tester().ExpectBucketCount(kPromiseAppTypeHistogram,
                                       PromiseAppType::kTwa, 1);
}

}  // namespace apps
