// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/promise_apps/promise_app_service.h"

#include "chrome/browser/apps/app_service/package_id.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app_almanac_connector.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app_registry_cache.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app_wrapper.h"
#include "chrome/browser/apps/app_service/promise_apps/proto/promise_app.pb.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace apps {

const PackageId kTestPackageId(AppType::kArc, "test.package.name");

class PromiseAppServiceTest : public testing::Test {
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
  }

  network::TestURLLoaderFactory* url_loader_factory() {
    return url_loader_factory_.get();
  }

  PromiseAppRegistryCache* cache() {
    return service_->PromiseAppRegistryCache();
  }

  PromiseAppService* service() { return service_.get(); }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<PromiseAppService> service_;
  std::unique_ptr<Profile> profile_;
  std::unique_ptr<network::TestURLLoaderFactory> url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  ash::system::ScopedFakeStatisticsProvider fake_statistics_provider_;
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

  url_loader_factory()->AddResponse(
      PromiseAppAlmanacConnector::GetServerUrl().spec(),
      response.SerializeAsString());

  // Add promise app to cache and trigger Almanac API call.
  service()->OnPromiseApp(std::make_unique<PromiseApp>(kTestPackageId));

  // TODO(b/261907233): Replace with wait for PromiseAppRegistryCache observer
  // update.
  RunUntilIdle();

  const PromiseApp* promise_app_result =
      cache()->GetPromiseAppForTesting(kTestPackageId);
  EXPECT_TRUE(promise_app_result->name.has_value());
  EXPECT_EQ(promise_app_result->name.value(), "Name");
  EXPECT_TRUE(promise_app_result->should_show);

  // TODO(b/261910028): Add testcases for promise_app_result icons;
}

}  // namespace apps
