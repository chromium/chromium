// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_preload_service/web_app_preload_installer.h"

#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_future.h"
#include "base/test/values_test_util.h"
#include "chrome/browser/apps/app_preload_service/preload_app_definition.h"
#include "chrome/browser/apps/app_preload_service/proto/app_provisioning.pb.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/test/base/testing_profile.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "content/public/test/browser_task_environment.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"

namespace apps {

class WebAppPreloadInstallerTest : public testing::Test {
 public:
  void SetUp() override {
    testing::Test::SetUp();

    TestingProfile::Builder profile_builder;
    profile_builder.SetSharedURLLoaderFactory(
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &url_loader_factory_));
    profile_ = profile_builder.Build();

    web_app::test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  Profile* profile() { return profile_.get(); }

  AppRegistryCache& app_registry_cache() {
    auto* proxy = AppServiceProxyFactory::GetForProfile(profile());
    return proxy->AppRegistryCache();
  }

  network::TestURLLoaderFactory url_loader_factory_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
};

TEST_F(WebAppPreloadInstallerTest, InstallOemApp) {
  WebAppPreloadInstaller installer(profile());

  proto::AppProvisioningListAppsResponse_App app;
  app.set_name("Example App");
  app.set_package_id("web:https://www.example.com/index.html");
  app.set_install_reason(
      proto::AppProvisioningListAppsResponse::INSTALL_REASON_OEM);

  auto* web_extras = app.mutable_web_extras();
  web_extras->set_original_manifest_url(
      "https://www.example.com/manifest.json");
  web_extras->set_manifest_url(
      "https://meltingpot.googleusercontent.com/manifest.json");

  url_loader_factory_.AddResponse(
      "https://meltingpot.googleusercontent.com/manifest.json", R"({
    "name": "Example App",
    "start_url": "/index.html",
    "scope": "/"
  })");

  base::test::TestFuture<bool> result;
  installer.InstallApp(PreloadAppDefinition(app), result.GetCallback());
  ASSERT_TRUE(result.Get());

  auto app_id = web_app::GenerateAppId(
      absl::nullopt, GURL("https://www.example.com/index.html"));
  bool found =
      app_registry_cache().ForOneApp(app_id, [](const AppUpdate& update) {
        EXPECT_EQ(update.Name(), "Example App");
        EXPECT_EQ(update.InstallReason(), InstallReason::kOem);
      });
  ASSERT_TRUE(found);
}

TEST_F(WebAppPreloadInstallerTest, InstallWithManifestId) {
  WebAppPreloadInstaller installer(profile());

  proto::AppProvisioningListAppsResponse_App app;
  app.set_name("Example App");
  app.set_package_id("web:https://www.example.com/manifest_id");
  app.set_install_reason(
      proto::AppProvisioningListAppsResponse::INSTALL_REASON_OEM);

  auto* web_extras = app.mutable_web_extras();
  web_extras->set_original_manifest_url(
      "https://www.example.com/manifest.json");
  web_extras->set_manifest_url(
      "https://meltingpot.googleusercontent.com/manifest.json");

  url_loader_factory_.AddResponse(
      "https://meltingpot.googleusercontent.com/manifest.json", R"({
    "id": "manifest_id",
    "name": "Example App",
    "start_url": "/index.html",
    "scope": "/"
  })");

  base::test::TestFuture<bool> result;
  installer.InstallApp(PreloadAppDefinition(app), result.GetCallback());
  ASSERT_TRUE(result.Get());

  // The generated app ID should take the manifest ID into account.
  auto app_id = web_app::GenerateAppId(
      "manifest_id", GURL("https://www.example.com/index.html"));
  ASSERT_TRUE(
      app_registry_cache().ForOneApp(app_id, [](const AppUpdate& update) {}));
}

// Reinstalling an existing user-installed app should not overwrite manifest
// data, but will add the OEM install reason.
TEST_F(WebAppPreloadInstallerTest, InstallOverUserApp) {
  constexpr char kStartUrl[] = "https://www.example.com/";
  constexpr char kManifestUrl[] =
      "https://meltingpot.googleusercontent.com/manifest.json";
  constexpr char kOriginalManifestUrl[] =
      "https://www.example.com/manifest.json";
  constexpr char kUserAppName[] = "User Installed App";

  WebAppPreloadInstaller installer(profile());

  auto app_id = web_app::test::InstallDummyWebApp(profile(), kUserAppName,
                                                  GURL(kStartUrl));

  proto::AppProvisioningListAppsResponse_App app;
  app.set_name("OEM Installed app");
  app.set_package_id(base::StrCat({"web:", kStartUrl}));
  app.set_install_reason(
      proto::AppProvisioningListAppsResponse::INSTALL_REASON_OEM);

  auto* web_extras = app.mutable_web_extras();
  web_extras->set_manifest_url(kManifestUrl);
  web_extras->set_original_manifest_url(kOriginalManifestUrl);

  url_loader_factory_.AddResponse(
      "https://meltingpot.googleusercontent.com/manifest.json", R"({
    "name": "OEM Installed app",
    "start_url": "/"
  })");

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
  app.set_package_id("web:https://cursive.apps.chrome/");

  ASSERT_EQ(installer.GetAppId(PreloadAppDefinition(app)),
            "apignacaigpffemhdbhmnajajaccbckh");
}

TEST_F(WebAppPreloadInstallerTest, ManifestToWebAppInstallInfo) {
  constexpr char manifest[] = R"({
    "id": "https://example.com/manifest_id",
    "name": "Peanut Types",
    "start_url": "/index.html",
    "scope": "/"
  })";
  constexpr char manifest_url[] = "https://example.com/manifest.json";
  constexpr char document_url[] = "https://example.com/";

  base::Value parsed_manifest = base::test::ParseJson(manifest);
  auto install_info = WebAppPreloadInstaller::ManifestToWebAppInstallInfo(
      GURL(document_url), GURL(manifest_url), parsed_manifest.GetDict());

  ASSERT_TRUE(install_info);
  EXPECT_EQ(install_info->manifest_id, "manifest_id");
  EXPECT_EQ(install_info->title, u"Peanut Types");
  EXPECT_EQ(install_info->start_url, "https://example.com/index.html");
  EXPECT_EQ(install_info->scope, "https://example.com/");
  EXPECT_EQ(install_info->display_mode, blink::mojom::DisplayMode::kStandalone);
  EXPECT_EQ(install_info->user_display_mode,
            web_app::mojom::UserDisplayMode::kStandalone);
}

TEST_F(WebAppPreloadInstallerTest, ManifestToWebAppInstallInfoRelativeId) {
  constexpr char manifest[] = R"({
    "id": "/manifest_id",
    "name": "Peanut Types",
    "start_url": "/index.html",
    "scope": "/"
  })";
  constexpr char manifest_url[] = "https://example.com/manifest.json";
  constexpr char document_url[] = "https://example.com/";

  base::Value parsed_manifest = base::test::ParseJson(manifest);
  auto install_info = WebAppPreloadInstaller::ManifestToWebAppInstallInfo(
      GURL(document_url), GURL(manifest_url), parsed_manifest.GetDict());

  ASSERT_TRUE(install_info);
  EXPECT_EQ(install_info->manifest_id, "manifest_id");
}

TEST_F(WebAppPreloadInstallerTest,
       ManifestToWebAppInstallInfoInvalidManifestUrl) {
  constexpr char manifest[] = R"({
    "id": "https://example.com/manifest_id",
    "name": "Peanut Types",
    "start_url": "/index.html",
    "scope": "/"
  })";
  constexpr char manifest_url[] = "invalid";
  constexpr char document_url[] = "https://example.com/";

  base::Value parsed_manifest = base::test::ParseJson(manifest);
  auto install_info = WebAppPreloadInstaller::ManifestToWebAppInstallInfo(
      GURL(document_url), GURL(manifest_url), parsed_manifest.GetDict());

  EXPECT_FALSE(install_info);
}

TEST_F(WebAppPreloadInstallerTest,
       ManifestToWebAppInstallInfoInvalidDocumentUrl) {
  constexpr char manifest[] = R"({
    "id": "https://example.com/manifest_id",
    "name": "Peanut Types",
    "start_url": "/index.html",
    "scope": "/"
  })";
  constexpr char manifest_url[] = "invalid";
  constexpr char document_url[] = "https://example.com/";

  base::Value parsed_manifest = base::test::ParseJson(manifest);
  auto install_info = WebAppPreloadInstaller::ManifestToWebAppInstallInfo(
      GURL(document_url), GURL(manifest_url), parsed_manifest.GetDict());

  EXPECT_FALSE(install_info);
}

TEST_F(WebAppPreloadInstallerTest,
       ManifestToWebAppInstallInfoNoNameInManifest) {
  constexpr char manifest[] = R"({
    "id": "https://example.com/manifest_id",
    "start_url": "/index.html",
    "scope": "/"
  })";
  constexpr char manifest_url[] = "https://example.com/manifest.json";
  constexpr char document_url[] = "https://example.com/";

  base::Value parsed_manifest = base::test::ParseJson(manifest);
  auto install_info = WebAppPreloadInstaller::ManifestToWebAppInstallInfo(
      GURL(document_url), GURL(manifest_url), parsed_manifest.GetDict());

  EXPECT_FALSE(install_info);
}

TEST_F(WebAppPreloadInstallerTest, ManifestToWebAppInstallInfoNoManifestId) {
  constexpr char manifest[] = R"({
    "name": "Peanut Types",
    "start_url": "/index.html",
    "scope": "/"
  })";
  constexpr char manifest_url[] = "https://example.com/manifest.json";
  constexpr char document_url[] = "https://example.com/";

  base::Value parsed_manifest = base::test::ParseJson(manifest);
  auto install_info = WebAppPreloadInstaller::ManifestToWebAppInstallInfo(
      GURL(document_url), GURL(manifest_url), parsed_manifest.GetDict());

  ASSERT_TRUE(install_info);
  EXPECT_FALSE(install_info->manifest_id);
  EXPECT_EQ(install_info->title, u"Peanut Types");
  EXPECT_EQ(install_info->start_url, "https://example.com/index.html");
  EXPECT_EQ(install_info->scope, "https://example.com/");
  EXPECT_EQ(install_info->display_mode, blink::mojom::DisplayMode::kStandalone);
  EXPECT_EQ(install_info->user_display_mode,
            web_app::mojom::UserDisplayMode::kStandalone);
}

TEST_F(WebAppPreloadInstallerTest, ManifestToWebAppInstallInfoInvalidStartUrl) {
  constexpr char manifest[] = R"({
    "id": "https://example.com/manifest_id",
    "name": "Peanut Types",
    "start_url": "https://exampleCDN.com/index.html",
    "scope": "/"
  })";
  constexpr char manifest_url[] = "https://exampleCDN.com/manifest.json";
  constexpr char document_url[] = "https://example.com/";

  base::Value parsed_manifest = base::test::ParseJson(manifest);
  auto install_info = WebAppPreloadInstaller::ManifestToWebAppInstallInfo(
      GURL(document_url), GURL(manifest_url), parsed_manifest.GetDict());

  EXPECT_FALSE(install_info);
}

TEST_F(WebAppPreloadInstallerTest, ManifestToWebAppInstallInfoInvalidScope) {
  constexpr char manifest[] = R"({
    "id": "https://example.com/manifest_id",
    "name": "Peanut Types",
    "start_url": "/index.html",
    "scope": "https://otherexample.com/"
  })";
  constexpr char manifest_url[] = "https://example.com/manifest.json";
  constexpr char document_url[] = "https://example.com/";

  base::Value parsed_manifest = base::test::ParseJson(manifest);
  auto install_info = WebAppPreloadInstaller::ManifestToWebAppInstallInfo(
      GURL(document_url), GURL(manifest_url), parsed_manifest.GetDict());

  ASSERT_TRUE(install_info);
  EXPECT_EQ(install_info->scope, install_info->start_url);
}

TEST_F(WebAppPreloadInstallerTest, ManifestToWebAppInstallInfoNoScope) {
  constexpr char manifest[] = R"({
    "id": "https://example.com/manifest_id",
    "name": "Peanut Types",
    "start_url": "/index.html",
  })";
  constexpr char manifest_url[] = "https://example.com/manifest.json";
  constexpr char document_url[] = "https://example.com/";

  base::Value parsed_manifest = base::test::ParseJson(manifest);
  auto install_info = WebAppPreloadInstaller::ManifestToWebAppInstallInfo(
      GURL(document_url), GURL(manifest_url), parsed_manifest.GetDict());

  ASSERT_TRUE(install_info);
  EXPECT_EQ(install_info->scope, install_info->start_url);
}

// The manifest id in the proto does not match the calculated manifest id.
TEST_F(WebAppPreloadInstallerTest, InstallMismatchedDataManifestId) {
  WebAppPreloadInstaller installer(profile());

  proto::AppProvisioningListAppsResponse_App app;
  app.set_name("Example App");
  app.set_package_id("web:https://www.example.com/manifest_id");
  app.set_install_reason(
      proto::AppProvisioningListAppsResponse::INSTALL_REASON_OEM);

  auto* web_extras = app.mutable_web_extras();
  web_extras->set_original_manifest_url(
      "https://www.example.com/manifest.json");
  web_extras->set_manifest_url(
      "https://meltingpot.googleusercontent.com/manifest.json");

  url_loader_factory_.AddResponse(
      "https://meltingpot.googleusercontent.com/manifest.json", R"({
    "name": "Example App",
    "start_url": "/index.html",
    "scope": "/"
  })");

  base::test::TestFuture<bool> result;
  installer.InstallApp(PreloadAppDefinition(app), result.GetCallback());
  ASSERT_FALSE(result.Get());

  auto app_id = web_app::GenerateAppId(
      absl::nullopt, GURL("https://www.example.com/index.html"));
  bool found =
      app_registry_cache().ForOneApp(app_id, [](const AppUpdate& update) {});
  ASSERT_FALSE(found);
}

TEST_F(WebAppPreloadInstallerTest, ManifestFileIsNotJSON) {
  WebAppPreloadInstaller installer(profile());

  proto::AppProvisioningListAppsResponse_App app;
  app.set_name("Example App");
  app.set_package_id("web:https://www.example.com/manifest_id");
  app.set_install_reason(
      proto::AppProvisioningListAppsResponse::INSTALL_REASON_OEM);

  auto* web_extras = app.mutable_web_extras();
  web_extras->set_original_manifest_url(
      "https://www.example.com/manifest.json");
  web_extras->set_manifest_url(
      "https://meltingpot.googleusercontent.com/manifest.json");

  url_loader_factory_.AddResponse(
      "https://meltingpot.googleusercontent.com/manifest.json", R"({
    INVALID
  })");

  base::test::TestFuture<bool> result;
  installer.InstallApp(PreloadAppDefinition(app), result.GetCallback());
  ASSERT_FALSE(result.Get());

  auto app_id = web_app::GenerateAppId(
      absl::nullopt, GURL("https://www.example.com/index.html"));
  bool found =
      app_registry_cache().ForOneApp(app_id, [](const AppUpdate& update) {});
  ASSERT_FALSE(found);
}

TEST_F(WebAppPreloadInstallerTest, ManifestFileIsHasMissingFields) {
  WebAppPreloadInstaller installer(profile());

  proto::AppProvisioningListAppsResponse_App app;
  app.set_name("Example App");
  app.set_package_id("web:https://www.example.com/manifest_id");
  app.set_install_reason(
      proto::AppProvisioningListAppsResponse::INSTALL_REASON_OEM);

  auto* web_extras = app.mutable_web_extras();
  web_extras->set_original_manifest_url(
      "https://www.example.com/manifest.json");
  web_extras->set_manifest_url(
      "https://meltingpot.googleusercontent.com/manifest.json");

  url_loader_factory_.AddResponse(
      "https://meltingpot.googleusercontent.com/manifest.json", R"({
    "is_valid": "no."
  })");

  base::test::TestFuture<bool> result;
  installer.InstallApp(PreloadAppDefinition(app), result.GetCallback());
  ASSERT_FALSE(result.Get());

  auto app_id = web_app::GenerateAppId(
      absl::nullopt, GURL("https://www.example.com/index.html"));
  bool found =
      app_registry_cache().ForOneApp(app_id, [](const AppUpdate& update) {});
  ASSERT_FALSE(found);
}

}  // namespace apps
