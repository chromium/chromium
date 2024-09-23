// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_preload_service/preload_app_definition.h"

#include <memory>

#include "base/strings/to_string.h"
#include "chrome/browser/apps/app_preload_service/proto/app_preload.pb.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"
#include "url/gurl.h"

namespace apps {

namespace {

// Returns a sample valid web App response proto. Tests should overwrite the
// individual fields that they need to verify.
proto::AppPreloadListResponse_App CreateTestWebApp() {
  proto::AppPreloadListResponse_App app;
  app.set_name("Test app");
  app.set_package_id("web:https://example.com/path/to/manifest_id");
  app.mutable_web_extras()->set_manifest_url("https://example.com");
  return app;
}
}  // namespace

class PreloadAppDefinitionTest : public testing::Test {
 protected:
  PreloadAppDefinitionTest() = default;
};

TEST_F(PreloadAppDefinitionTest, GetNameWhenNotSet) {
  proto::AppPreloadListResponse_App app;

  auto app_def = PreloadAppDefinition(app);
  ASSERT_EQ(app_def.GetName(), "");
}

TEST_F(PreloadAppDefinitionTest, GetName) {
  const std::string test_name = "test_app_name";
  proto::AppPreloadListResponse_App app;

  app.set_name(test_name);
  auto app_def = PreloadAppDefinition(app);
  ASSERT_EQ(app_def.GetName(), test_name);
}

TEST_F(PreloadAppDefinitionTest, GetPlatformWhenNotSet) {
  proto::AppPreloadListResponse_App app;

  auto app_def = PreloadAppDefinition(app);
  ASSERT_EQ(app_def.GetPlatform(), PackageType::kUnknown);
}

TEST_F(PreloadAppDefinitionTest, GetPlatformMalformedPackageId) {
  proto::AppPreloadListResponse_App app;
  app.set_package_id(":");

  auto app_def = PreloadAppDefinition(app);
  ASSERT_EQ(app_def.GetPlatform(), PackageType::kUnknown);
}

TEST_F(PreloadAppDefinitionTest, GetPlatformWeb) {
  proto::AppPreloadListResponse_App app;
  app.set_package_id("web:https://example.com/");

  auto app_def = PreloadAppDefinition(app);
  ASSERT_EQ(app_def.GetPlatform(), PackageType::kWeb);
}

TEST_F(PreloadAppDefinitionTest, IsOemAppWhenNotSet) {
  proto::AppPreloadListResponse_App app;

  auto app_def = PreloadAppDefinition(app);
  ASSERT_FALSE(app_def.IsOemApp());
}

TEST_F(PreloadAppDefinitionTest, IsOemApp) {
  proto::AppPreloadListResponse_App app;

  app.set_install_reason(proto::AppPreloadListResponse::INSTALL_REASON_OEM);
  auto app_def = PreloadAppDefinition(app);
  ASSERT_TRUE(app_def.IsOemApp());
}

TEST_F(PreloadAppDefinitionTest, IsNotOemApp) {
  proto::AppPreloadListResponse_App app;

  app.set_install_reason(proto::AppPreloadListResponse::INSTALL_REASON_DEFAULT);
  auto app_def = PreloadAppDefinition(app);
  ASSERT_FALSE(app_def.IsOemApp());
}

TEST_F(PreloadAppDefinitionTest, IsTestApp) {
  proto::AppPreloadListResponse_App app;
  app.set_install_reason(proto::AppPreloadListResponse::INSTALL_REASON_TEST);

  PreloadAppDefinition app_def(app);
  ASSERT_TRUE(app_def.IsTestApp());
}

TEST_F(PreloadAppDefinitionTest, IsNotTestApp) {
  proto::AppPreloadListResponse_App app;
  app.set_install_reason(proto::AppPreloadListResponse::INSTALL_REASON_OEM);

  PreloadAppDefinition app_def(app);
  ASSERT_FALSE(app_def.IsTestApp());
}

TEST_F(PreloadAppDefinitionTest, GetInstallSurface) {
  proto::AppPreloadListResponse_App app;

  app.set_install_reason(proto::AppPreloadListResponse::INSTALL_REASON_OEM);
  EXPECT_EQ(PreloadAppDefinition(app).GetInstallSurface(),
            AppInstallSurface::kAppPreloadServiceOem);

  app.set_install_reason(proto::AppPreloadListResponse::INSTALL_REASON_DEFAULT);
  EXPECT_EQ(PreloadAppDefinition(app).GetInstallSurface(),
            AppInstallSurface::kAppPreloadServiceDefault);
}

TEST_F(PreloadAppDefinitionTest, GetWebAppManifestUrlWebsite) {
  proto::AppPreloadListResponse_App app = CreateTestWebApp();
  app.mutable_web_extras()->set_manifest_url(
      "https://meltingpot.googleusercontent.com/manifest.json");

  PreloadAppDefinition app_def(app);

  GURL manifest_url = app_def.GetWebAppManifestUrl();

  ASSERT_TRUE(manifest_url.is_valid());
  ASSERT_EQ(manifest_url.spec(),
            "https://meltingpot.googleusercontent.com/manifest.json");
}

TEST_F(PreloadAppDefinitionTest, GetWebAppManifestUrlLocalFile) {
  proto::AppPreloadListResponse_App app = CreateTestWebApp();
  app.mutable_web_extras()->set_manifest_url(
      "file:///usr/var/share/aps/peanut_manifest.json");

  PreloadAppDefinition app_def(app);

  GURL manifest_url = app_def.GetWebAppManifestUrl();

  ASSERT_TRUE(manifest_url.is_valid());
  ASSERT_EQ(manifest_url.spec(),
            "file:///usr/var/share/aps/peanut_manifest.json");
}

TEST_F(PreloadAppDefinitionTest, GetWebAppManifestUrlInvalidUrl) {
  proto::AppPreloadListResponse_App app = CreateTestWebApp();
  app.mutable_web_extras()->set_manifest_url("invalid url");

  PreloadAppDefinition app_def(app);

  ASSERT_FALSE(app_def.GetWebAppManifestUrl().is_valid());
}

TEST_F(PreloadAppDefinitionTest, GetWebAppManifestUrlEmpty) {
  proto::AppPreloadListResponse_App app = CreateTestWebApp();
  app.mutable_web_extras()->set_manifest_url("");

  PreloadAppDefinition app_def(app);

  ASSERT_TRUE(app_def.GetWebAppManifestUrl().is_empty());
}

TEST_F(PreloadAppDefinitionTest, GetWebAppOriginalManifestUrl) {
  proto::AppPreloadListResponse_App app = CreateTestWebApp();
  app.mutable_web_extras()->set_original_manifest_url(
      "https://www.example.com/app/manifest.json");

  PreloadAppDefinition app_def(app);

  GURL manifest_url = app_def.GetWebAppOriginalManifestUrl();

  ASSERT_TRUE(manifest_url.is_valid());
  ASSERT_EQ(manifest_url.spec(), "https://www.example.com/app/manifest.json");
}

TEST_F(PreloadAppDefinitionTest, GetWebAppOriginalManifestUrlInvalidUrl) {
  proto::AppPreloadListResponse_App app = CreateTestWebApp();
  app.mutable_web_extras()->set_original_manifest_url("invalid url");

  PreloadAppDefinition app_def(app);

  ASSERT_FALSE(app_def.GetWebAppOriginalManifestUrl().is_valid());
}

TEST_F(PreloadAppDefinitionTest, GetWebAppOriginalManifestUrlNotSpecified) {
  proto::AppPreloadListResponse_App app = CreateTestWebApp();

  PreloadAppDefinition app_def(app);

  ASSERT_TRUE(app_def.GetWebAppOriginalManifestUrl().is_empty());
}

TEST_F(PreloadAppDefinitionTest, GetWebAppManifestId) {
  proto::AppPreloadListResponse_App app = CreateTestWebApp();
  app.set_package_id("web:https://example.com/path/of/manifest_id");

  PreloadAppDefinition app_def(app);

  ASSERT_EQ(app_def.GetWebAppManifestId().spec(),
            "https://example.com/path/of/manifest_id");
}

TEST_F(PreloadAppDefinitionTest, ToAppInstallData) {
  proto::AppPreloadListResponse_App app;
  app.set_name("Example App");
  app.set_package_id("web:https://www.example.com/index.html");
  app.set_install_reason(proto::AppPreloadListResponse::INSTALL_REASON_OEM);
  auto* web_extras = app.mutable_web_extras();
  web_extras->set_original_manifest_url(
      "https://www.example.com/manifest.json");
  web_extras->set_manifest_url("https://cdn.com/manifest.json");

  AppInstallData expectation(
      PackageId(PackageType::kWeb, "https://www.example.com/index.html"));
  expectation.name = "Example App";
  WebAppInstallData& web_app_expecatation =
      expectation.app_type_data.emplace<WebAppInstallData>();
  web_app_expecatation.original_manifest_url =
      GURL("https://www.example.com/manifest.json");
  web_app_expecatation.proxied_manifest_url =
      GURL("https://cdn.com/manifest.json");
  web_app_expecatation.document_url = GURL("https://www.example.com/");

  EXPECT_EQ(
      base::ToString(PreloadAppDefinition(std::move(app)).ToAppInstallData()),
      base::ToString(expectation));
}

}  // namespace apps
