// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_preload_service/web_app_preload_installer.h"

#include "base/test/values_test_util.h"
#include "chrome/browser/apps/app_preload_service/preload_app_definition.h"
#include "chrome/browser/apps/app_preload_service/proto/app_provisioning.pb.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"

namespace apps {

class WebAppPreloadInstallerTest : public testing::Test {
 public:
  void SetUp() override {
    testing::Test::SetUp();

    profile_ = std::make_unique<TestingProfile>();

    web_app::test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  Profile* profile() { return profile_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
};

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

}  // namespace apps
