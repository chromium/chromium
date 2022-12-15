// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_preload_service/web_app_preload_installer.h"

#include "base/test/test_future.h"
#include "chrome/browser/apps/app_preload_service/preload_app_definition.h"
#include "chrome/browser/apps/app_preload_service/proto/app_provisioning.pb.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/test/base/testing_profile.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace apps {

class WebAppPreloadInstallerTest : public testing::Test {
 public:
  void SetUp() override {
    testing::Test::SetUp();
    web_app::test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  Profile* profile() { return &profile_; }

  AppRegistryCache& app_registry_cache() {
    auto* proxy = AppServiceProxyFactory::GetForProfile(profile());
    return proxy->AppRegistryCache();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

// TODO(b/261632289): temporarily disabled while refactoring is in progress.
TEST_F(WebAppPreloadInstallerTest, DISABLED_InstallOemApp) {
  WebAppPreloadInstaller installer(profile());

  proto::AppProvisioningListAppsResponse_App app;
  app.set_name("Test app");
  app.set_platform(proto::AppProvisioningListAppsResponse::PLATFORM_WEB);
  app.set_install_reason(
      proto::AppProvisioningListAppsResponse::INSTALL_REASON_OEM);

  auto* web_extras = app.mutable_web_extras();
  web_extras->set_manifest_id("https://www.example.com/home");
  web_extras->set_manifest_url("https://www.example.com/home");

  base::test::TestFuture<bool> result;
  installer.InstallApp(PreloadAppDefinition(app), result.GetCallback());
  ASSERT_TRUE(result.Get());

  auto app_id = web_app::GenerateAppId(absl::nullopt,
                                       GURL("https://www.example.com/home"));
  bool found =
      app_registry_cache().ForOneApp(app_id, [](const AppUpdate& update) {
        EXPECT_EQ(update.Name(), "Test app");
        EXPECT_EQ(update.InstallReason(), InstallReason::kOem);
      });
  ASSERT_TRUE(found);
}

TEST_F(WebAppPreloadInstallerTest, InstallFailure) {
  WebAppPreloadInstaller installer(profile());

  proto::AppProvisioningListAppsResponse_App app;
  app.set_name("Test app");
  app.set_platform(proto::AppProvisioningListAppsResponse::PLATFORM_WEB);
  app.set_install_reason(
      proto::AppProvisioningListAppsResponse::INSTALL_REASON_OEM);

  // Installation should fail due to missing web_extras field.
  base::test::TestFuture<bool> result;
  installer.InstallApp(PreloadAppDefinition(app), result.GetCallback());
  ASSERT_FALSE(result.Get());
}

// TODO(b/261632289): temporarily disabled while refactoring is in progress.
TEST_F(WebAppPreloadInstallerTest, DISABLED_InstallWithManifestId) {
  WebAppPreloadInstaller installer(profile());

  proto::AppProvisioningListAppsResponse_App app;
  app.set_name("Test app");
  app.set_platform(proto::AppProvisioningListAppsResponse::PLATFORM_WEB);
  app.set_install_reason(
      proto::AppProvisioningListAppsResponse::INSTALL_REASON_OEM);

  auto* web_extras = app.mutable_web_extras();
  web_extras->set_manifest_id("https://www.example.com/app");
  web_extras->set_manifest_url("https://www.example.com/manifest.json");

  base::test::TestFuture<bool> result;
  installer.InstallApp(PreloadAppDefinition(app), result.GetCallback());
  ASSERT_TRUE(result.Get());

  // The generated app ID should take the manifest ID into account.
  auto app_id =
      web_app::GenerateAppId("app", GURL("https://www.example.com/home"));
  ASSERT_TRUE(
      app_registry_cache().ForOneApp(app_id, [](const AppUpdate& update) {}));
}

// Reinstalling an existing user-installed app should not overwrite manifest
// data, but will add the OEM install reason.
// TODO(b/261632289): temporarily disabled while refactoring is in progress.
TEST_F(WebAppPreloadInstallerTest, DISABLED_InstallOverUserApp) {
  constexpr char kStartUrl[] = "https://www.example.com/";
  constexpr char kManifestUrl[] =
      "https://meltingpot.googleusercontent.com/manifest.json";
  constexpr char kUserAppName[] = "User Installed App";

  WebAppPreloadInstaller installer(profile());

  auto app_id = web_app::test::InstallDummyWebApp(profile(), kUserAppName,
                                                  GURL(kStartUrl));

  proto::AppProvisioningListAppsResponse_App app;
  app.set_name("OEM Installed app");
  app.set_platform(proto::AppProvisioningListAppsResponse::PLATFORM_WEB);
  app.set_install_reason(
      proto::AppProvisioningListAppsResponse::INSTALL_REASON_OEM);

  auto* web_extras = app.mutable_web_extras();
  web_extras->set_manifest_id(kStartUrl);
  web_extras->set_manifest_url(kManifestUrl);

  base::test::TestFuture<bool> result;
  installer.InstallApp(PreloadAppDefinition(app), result.GetCallback());
  ASSERT_TRUE(result.Get());

  bool found =
      app_registry_cache().ForOneApp(app_id, [&](const AppUpdate& update) {
        EXPECT_EQ(update.Name(), kUserAppName);
        EXPECT_EQ(update.InstallReason(), InstallReason::kOem);
      });
  ASSERT_TRUE(found);
}

TEST_F(WebAppPreloadInstallerTest, GetAppId) {
  WebAppPreloadInstaller installer(profile());

  proto::AppProvisioningListAppsResponse_App app;
  app.set_platform(proto::AppProvisioningListAppsResponse::PLATFORM_WEB);
  app.mutable_web_extras()->set_manifest_id("https://cursive.apps.chrome/");

  ASSERT_EQ(installer.GetAppId(PreloadAppDefinition(app)),
            "apignacaigpffemhdbhmnajajaccbckh");
}

}  // namespace apps
