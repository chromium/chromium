// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/promise_apps/promise_app_service.h"

#include "base/scoped_observation.h"
#include "chrome/browser/apps/app_service/package_id.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app_almanac_connector.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app_registry_cache.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app_update.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app_wrapper.h"
#include "chrome/browser/apps/app_service/promise_apps/proto/promise_app.pb.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "components/image_fetcher/core/fake_image_decoder.h"
#include "components/image_fetcher/core/image_fetcher.h"
#include "components/image_fetcher/core/image_fetcher_impl.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

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
    service_ = std::make_unique<PromiseAppService>(profile_.get());
    std::unique_ptr<image_fetcher::ImageFetcher> image_fetcher =
        std::make_unique<image_fetcher::ImageFetcherImpl>(
            std::make_unique<image_fetcher::FakeImageDecoder>(),
            profile_->GetURLLoaderFactory());
    service_->SetImageFetcherForTesting(std::move(image_fetcher));
  }

  network::TestURLLoaderFactory* url_loader_factory() {
    return url_loader_factory_.get();
  }

  PromiseAppRegistryCache* cache() {
    return service_->PromiseAppRegistryCache();
  }

  PromiseAppIconCache* icon_cache() { return service_->PromiseAppIconCache(); }

  PromiseAppService* service() { return service_.get(); }

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

  // Tracks how many times we should expect OnPromiseAppUpdate to be called
  // before proceeding with a unit test.
  int expected_num_updates_;
  int current_num_updates_;
};

TEST_F(PromiseAppServiceTest, OnPromiseApp_AlmanacResponseUpdatesPromiseApp) {
  proto::PromiseAppResponse response;
  response.set_package_id(kTestPackageId.ToString());
  response.set_name("Name");
  response.add_icons();
  response.mutable_icons(0)->set_url("www.image");
  response.mutable_icons(0)->set_width_in_pixels(512);
  response.mutable_icons(0)->set_mime_type("image/png");
  response.mutable_icons(0)->set_is_masking_allowed(true);

  // Wait for the registry cache update that follows the Almanac API response.
  ExpectNumUpdates(/*num_updates=*/2);
  url_loader_factory()->AddResponse(
      PromiseAppAlmanacConnector::GetServerUrl().spec(),
      response.SerializeAsString());

  // Add promise app to cache and trigger Almanac API call.
  service()->OnPromiseApp(std::make_unique<PromiseApp>(kTestPackageId));

  WaitForPromiseAppUpdates();

  const PromiseApp* promise_app_result =
      cache()->GetPromiseAppForTesting(kTestPackageId);
  EXPECT_TRUE(promise_app_result->name.has_value());
  EXPECT_EQ(promise_app_result->name.value(), "Name");
}

// Tests that icons can be successfully downloaded from the URLs provided by an
// Almanac response.
TEST_F(PromiseAppServiceTest, OnPromiseApp_IconsDownloaded) {
  std::string url = "http://image.test/test.png";
  std::string url_other = "http://image.test/second-test.png";

  proto::PromiseAppResponse response;
  response.set_package_id(kTestPackageId.ToString());
  response.set_name("Name");
  response.add_icons();
  response.mutable_icons(0)->set_url(url);
  response.mutable_icons(0)->set_width_in_pixels(512);
  response.mutable_icons(0)->set_mime_type("image/png");
  response.mutable_icons(0)->set_is_masking_allowed(true);
  response.add_icons();
  response.mutable_icons(1)->set_url(url_other);
  response.mutable_icons(1)->set_width_in_pixels(1024);
  response.mutable_icons(1)->set_mime_type("image/png");
  response.mutable_icons(1)->set_is_masking_allowed(false);

  // Set up response for Almanac endpoint.
  url_loader_factory()->AddResponse(
      PromiseAppAlmanacConnector::GetServerUrl().spec(),
      response.SerializeAsString());

  // Set up responses for image fetcher.
  url_loader_factory()->AddResponse(url, "data");
  url_loader_factory()->AddResponse(url_other, "data");

  // Confirm there aren't any icons for the package yet.
  EXPECT_FALSE(icon_cache()->DoesPackageIdHaveIcons(kTestPackageId));

  // Add promise app to cache and trigger Almanac API and image fetcher calls.
  ExpectNumUpdates(/*num_updates=*/3);
  service()->OnPromiseApp(std::make_unique<PromiseApp>(kTestPackageId));

  // Wait for the separate registry cache updates that follow the Almanac API
  // response and image fetcher completion.
  WaitForPromiseAppUpdates();

  // Verify that there are 2 icons now saved in cache.
  EXPECT_TRUE(icon_cache()->DoesPackageIdHaveIcons(kTestPackageId));
  std::vector<PromiseAppIcon*> icons =
      icon_cache()->GetIconsForTesting(kTestPackageId);
  EXPECT_EQ(icons.size(), 2u);
  EXPECT_TRUE(icons[0]->is_masking_allowed);
  EXPECT_EQ(icons[0]->width_in_pixels, 512);
  EXPECT_FALSE(icons[1]->is_masking_allowed);
  EXPECT_EQ(icons[1]->width_in_pixels, 1024);

  // Verify that the promise app is allowed to be visible now.
  const PromiseApp* promise_app_result =
      cache()->GetPromiseAppForTesting(kTestPackageId);
  EXPECT_TRUE(promise_app_result->should_show);
}

// Tests that we can deal with broken URLs/ failed icon downloads and avoid
// updating the icon cache.
TEST_F(PromiseAppServiceTest, OnPromiseApp_FailedIconDownload) {
  proto::PromiseAppResponse response;
  response.set_package_id(kTestPackageId.ToString());
  response.set_name("Name");
  response.add_icons();
  response.mutable_icons(0)->set_url("broken-url");
  response.mutable_icons(0)->set_width_in_pixels(512);
  response.mutable_icons(0)->set_mime_type("image/png");
  response.mutable_icons(0)->set_is_masking_allowed(true);

  // Set up response for Almanac endpoint.
  url_loader_factory()->AddResponse(
      PromiseAppAlmanacConnector::GetServerUrl().spec(),
      response.SerializeAsString());

  // Confirm there aren't any icons for the package yet.
  EXPECT_FALSE(icon_cache()->DoesPackageIdHaveIcons(kTestPackageId));

  // Add promise app to cache and trigger Almanac API call.
  ExpectNumUpdates(/*num_updates=*/2);
  service()->OnPromiseApp(std::make_unique<PromiseApp>(kTestPackageId));

  WaitForPromiseAppUpdates();

  // Icon cache should still be empty.
  EXPECT_FALSE(icon_cache()->DoesPackageIdHaveIcons(kTestPackageId));
}
}  // namespace apps
