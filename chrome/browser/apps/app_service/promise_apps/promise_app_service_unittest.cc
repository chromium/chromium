// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/promise_apps/promise_app_service.h"

#include "base/functional/callback_helpers.h"
#include "base/scoped_observation.h"
#include "base/test/test_future.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_factory.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/package_id.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app_almanac_connector.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app_registry_cache.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app_update.h"
#include "chrome/browser/apps/app_service/promise_apps/proto/promise_app.pb.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "content/public/test/browser_task_environment.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/skia_util.h"

namespace apps {

const PackageId kTestPackageId(AppType::kArc, "test.package.name");

class PromiseAppServiceTest : public testing::Test,
                              public PromiseAppRegistryCache::Observer {
 public:
  void SetUp() override {
    url_loader_factory_ = std::make_unique<network::TestURLLoaderFactory>();
    testing::Test::SetUp();
    TestingProfile::Builder profile_builder;
    profile_builder.SetSharedURLLoaderFactory(
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            url_loader_factory_.get()));
    profile_ = profile_builder.Build();
    test_shared_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            url_loader_factory_.get());
    app_cache_ = &AppServiceProxyFactory::GetForProfile(profile_.get())
                      ->AppRegistryCache();
    service_ = std::make_unique<PromiseAppService>(profile_.get(), *app_cache_);
    service_->SetSkipApiKeyCheckForTesting(true);
  }

  network::TestURLLoaderFactory* url_loader_factory() {
    return url_loader_factory_.get();
  }

  PromiseAppRegistryCache* cache() {
    return service_->PromiseAppRegistryCache();
  }

  AppRegistryCache* app_cache() { return app_cache_; }

  PromiseAppIconCache* icon_cache() { return service_->PromiseAppIconCache(); }

  PromiseAppService* service() { return service_.get(); }

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
  std::unique_ptr<PromiseAppService> service_;
  std::unique_ptr<Profile> profile_;
  std::unique_ptr<network::TestURLLoaderFactory> url_loader_factory_;
  base::ScopedObservation<PromiseAppRegistryCache,
                          PromiseAppRegistryCache::Observer>
      obs_{this};
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  ash::system::ScopedFakeStatisticsProvider fake_statistics_provider_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  raw_ptr<AppRegistryCache> app_cache_;

  // Tracks how many times we should expect OnPromiseAppUpdate to be called
  // before proceeding with a unit test.
  int expected_num_updates_;
  int current_num_updates_;
};

// Tests that icons can be successfully downloaded from the URLs provided by an
// Almanac response.
TEST_F(PromiseAppServiceTest, OnPromiseApp_AlmanacIconsDownloaded) {
  std::string url = "http://image.test/test.png";
  std::string url_other = "http://image.test/second-test.png";

  proto::PromiseAppResponse response;
  response.set_package_id(kTestPackageId.ToString());
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

  // We expect 2 Promise App Registry Cache updates in this test:
  // The first update is when we initially register the promise app in the
  // cache. The second update is after we finish downloading all the icons for
  // the promise app and mark the promise app as ready to be shown to the user.
  ExpectNumUpdates(/*num_updates=*/2);
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

  // Verify that the promise app is allowed to be visible now.
  const PromiseApp* promise_app_result = cache()->GetPromiseApp(kTestPackageId);
  EXPECT_TRUE(promise_app_result->should_show);
}

// Tests that we can deal with broken URLs/ failed icon downloads and avoid
// updating the icon cache.
TEST_F(PromiseAppServiceTest, OnPromiseApp_FailedIconDownload) {
  proto::PromiseAppResponse response;
  response.set_package_id(kTestPackageId.ToString());
  response.add_icons();
  response.mutable_icons(0)->set_url("broken-url");

  // Set up response for Almanac endpoint.
  url_loader_factory()->AddResponse(
      PromiseAppAlmanacConnector::GetServerUrl().spec(),
      response.SerializeAsString());

  // Confirm there aren't any icons for the package yet.
  EXPECT_FALSE(icon_cache()->DoesPackageIdHaveIcons(kTestPackageId));

  // We expect a Promise App Registry Cache update when we register the promise
  // app in the cache.
  ExpectNumUpdates(/*num_updates=*/1);
  service()->OnPromiseApp(std::make_unique<PromiseApp>(kTestPackageId));

  // Wait for all the updates to trigger.
  WaitForPromiseAppUpdates();

  // Icon cache should still be empty.
  EXPECT_FALSE(icon_cache()->DoesPackageIdHaveIcons(kTestPackageId));
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
  PackageId package_id(app_type, identifier);

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
  app_cache()->OnApps(std::move(apps), app_type,
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
  PackageId package_id(app_type, identifier);

  // Register sample test app in AppRegistryCache.
  apps::AppPtr app = std::make_unique<apps::App>(app_type, "asdfghjkl");
  app->publisher_id = identifier;
  app->readiness = apps::Readiness::kReady;
  std::vector<apps::AppPtr> apps;
  apps.push_back(std::move(app));
  app_cache()->OnApps(std::move(apps), app_type,
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
  PackageId package_id(app_type, identifier);

  // Register an app in AppRegistryCache, marked as uninstalled.
  apps::AppPtr app = std::make_unique<apps::App>(app_type, "asdfghjkl");
  app->publisher_id = identifier;
  app->readiness = apps::Readiness::kUninstalledByUser;
  std::vector<apps::AppPtr> apps;
  apps.push_back(std::move(app));
  app_cache()->OnApps(std::move(apps), app_type,
                      /*should_notify_initialized=*/false);

  // Register test promise app with a matching package ID.
  PromiseAppPtr promise_app = std::make_unique<PromiseApp>(package_id);
  promise_app->status = PromiseStatus::kPending;
  service()->OnPromiseApp(std::move(promise_app));

  // Confirm that the promise app does get created.
  EXPECT_TRUE(cache()->HasPromiseApp(package_id));
}

}  // namespace apps
