// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_preload_service/preload_app_definition.h"

#include <memory>

#include "chrome/browser/apps/app_preload_service/proto/app_provisioning.pb.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"

namespace apps {

namespace {

// Returns a sample valid web App response proto. Tests should overwrite the
// individual fields that they need to verify.
proto::AppProvisioningResponse_App CreateTestWebApp() {
  proto::AppProvisioningResponse_App app;
  app.set_name("Test app");
  app.set_platform(proto::AppProvisioningResponse::PLATFORM_WEB);
  auto* web_extras = app.mutable_web_extras();
  web_extras->set_manifest_id("https://www.example.com/home");
  web_extras->set_start_url("https://www.example.com/home");
  web_extras->set_scope("https://www.example.com/");
  web_extras->set_display_mode(
      proto::AppProvisioningResponse::DISPLAY_MODE_STANDALONE);
  return app;
}
}  // namespace

class PreloadAppDefinitionTest : public testing::Test {
 protected:
  PreloadAppDefinitionTest() = default;
};

TEST_F(PreloadAppDefinitionTest, GetNameWhenNotSet) {
  proto::AppProvisioningResponse_App app;

  auto app_def = PreloadAppDefinition(app);
  ASSERT_EQ(app_def.GetName(), "");
}

TEST_F(PreloadAppDefinitionTest, GetName) {
  const std::string test_name = "test_app_name";
  proto::AppProvisioningResponse_App app;

  app.set_name(test_name);
  auto app_def = PreloadAppDefinition(app);
  ASSERT_EQ(app_def.GetName(), test_name);
}

TEST_F(PreloadAppDefinitionTest, GetPlatformWhenNotSet) {
  proto::AppProvisioningResponse_App app;

  auto app_def = PreloadAppDefinition(app);
  ASSERT_EQ(app_def.GetPlatform(), AppType::kUnknown);
}

TEST_F(PreloadAppDefinitionTest, GetPlatform) {
  proto::AppProvisioningResponse_App app;

  app.set_platform(proto::AppProvisioningResponse_Platform::
                       AppProvisioningResponse_Platform_PLATFORM_WEB);
  auto app_def = PreloadAppDefinition(app);
  ASSERT_EQ(app_def.GetPlatform(), AppType::kWeb);
}

TEST_F(PreloadAppDefinitionTest, IsOemAppWhenNotSet) {
  proto::AppProvisioningResponse_App app;

  auto app_def = PreloadAppDefinition(app);
  ASSERT_FALSE(app_def.IsOemApp());
}

TEST_F(PreloadAppDefinitionTest, IsOemApp) {
  proto::AppProvisioningResponse_App app;

  app.set_install_reason(
      proto::AppProvisioningResponse_InstallReason::
          AppProvisioningResponse_InstallReason_INSTALL_REASON_OEM);
  auto app_def = PreloadAppDefinition(app);
  ASSERT_TRUE(app_def.IsOemApp());
}

TEST_F(PreloadAppDefinitionTest, IsNotOemApp) {
  proto::AppProvisioningResponse_App app;

  app.set_install_reason(
      proto::AppProvisioningResponse_InstallReason::
          AppProvisioningResponse_InstallReason_INSTALL_REASON_DEFAULT);
  auto app_def = PreloadAppDefinition(app);
  ASSERT_FALSE(app_def.IsOemApp());
}

TEST_F(PreloadAppDefinitionTest, CreateWebAppInstallInfo) {
  proto::AppProvisioningResponse_App app;
  app.set_name("Test app");
  app.set_platform(proto::AppProvisioningResponse::PLATFORM_WEB);
  auto* web_extras = app.mutable_web_extras();
  web_extras->set_manifest_id("https://www.example.com/app_id");
  web_extras->set_start_url("https://www.example.com/home");
  web_extras->set_scope("https://www.example.com/");
  web_extras->set_display_mode(
      proto::AppProvisioningResponse::DISPLAY_MODE_STANDALONE);

  PreloadAppDefinition app_def(app);
  std::unique_ptr<WebAppInstallInfo> install_info =
      app_def.CreateWebAppInstallInfo();

  ASSERT_TRUE(install_info);
  EXPECT_EQ(u"Test app", install_info->title);
  EXPECT_EQ(GURL("https://www.example.com/home"), install_info->start_url);
  EXPECT_EQ("app_id", install_info->manifest_id);
  EXPECT_EQ(GURL("https://www.example.com/"), install_info->scope);
  EXPECT_EQ(blink::mojom::DisplayMode::kStandalone, install_info->display_mode);
}

TEST_F(PreloadAppDefinitionTest, CreateWebAppInstallInfoNoExtras) {
  proto::AppProvisioningResponse_App app;
  app.set_name("Foo bar");
  app.set_platform(proto::AppProvisioningResponse::PLATFORM_WEB);

  PreloadAppDefinition app_def(app);
  std::unique_ptr<WebAppInstallInfo> install_info =
      app_def.CreateWebAppInstallInfo();

  ASSERT_FALSE(install_info);
}

TEST_F(PreloadAppDefinitionTest, CreateWebAppInstallInfoInvalidStartUrl) {
  proto::AppProvisioningResponse_App app = CreateTestWebApp();
  // Start URL has no scheme and so cannot parse as a valid URL.
  app.mutable_web_extras()->set_start_url("www.foo.com");

  PreloadAppDefinition app_def(app);
  std::unique_ptr<WebAppInstallInfo> install_info =
      app_def.CreateWebAppInstallInfo();

  ASSERT_FALSE(install_info);
}

TEST_F(PreloadAppDefinitionTest, CreateWebAppInstallInfoInvalidScope) {
  proto::AppProvisioningResponse_App app = CreateTestWebApp();
  // Scope has no scheme and so cannot parse as a valid URL.
  app.mutable_web_extras()->set_scope("www.foo.com");

  PreloadAppDefinition app_def(app);
  std::unique_ptr<WebAppInstallInfo> install_info =
      app_def.CreateWebAppInstallInfo();

  ASSERT_FALSE(install_info);
}

TEST_F(PreloadAppDefinitionTest, CreateWebAppInstallInfoStartUrlOutsideScope) {
  proto::AppProvisioningResponse_App app = CreateTestWebApp();
  app.mutable_web_extras()->set_start_url("https://www.foo.com/");
  app.mutable_web_extras()->set_scope("https://www.foo.com/app/");

  PreloadAppDefinition app_def(app);
  std::unique_ptr<WebAppInstallInfo> install_info =
      app_def.CreateWebAppInstallInfo();

  ASSERT_FALSE(install_info);
}

TEST_F(PreloadAppDefinitionTest, CreateWebAppInstallInfoWithNoManifestId) {
  proto::AppProvisioningResponse_App app = CreateTestWebApp();
  app.mutable_web_extras()->clear_manifest_id();

  PreloadAppDefinition app_def(app);
  std::unique_ptr<WebAppInstallInfo> install_info =
      app_def.CreateWebAppInstallInfo();

  ASSERT_FALSE(install_info);
}

TEST_F(PreloadAppDefinitionTest, CreateWebAppInstallInfoWithInvalidManifestId) {
  proto::AppProvisioningResponse_App app = CreateTestWebApp();
  app.mutable_web_extras()->set_manifest_id("/app_id");

  PreloadAppDefinition app_def(app);
  std::unique_ptr<WebAppInstallInfo> install_info =
      app_def.CreateWebAppInstallInfo();

  ASSERT_FALSE(install_info);
}

TEST_F(PreloadAppDefinitionTest,
       CreateWebAppInstallInfoWithDifferentOriginManifestId) {
  proto::AppProvisioningResponse_App app = CreateTestWebApp();
  app.mutable_web_extras()->set_manifest_id("https://www.foo.com/bar");
  app.mutable_web_extras()->set_start_url("https://www.bar.com/foo");

  PreloadAppDefinition app_def(app);
  std::unique_ptr<WebAppInstallInfo> install_info =
      app_def.CreateWebAppInstallInfo();

  ASSERT_FALSE(install_info);
}

TEST_F(PreloadAppDefinitionTest,
       CreateWebAppInstallInfoWithUnknownDisplayMode) {
  proto::AppProvisioningResponse_App app = CreateTestWebApp();
  app.mutable_web_extras()->set_display_mode(
      proto::AppProvisioningResponse::DISPLAY_MODE_UNKNOWN);

  PreloadAppDefinition app_def(app);
  std::unique_ptr<WebAppInstallInfo> install_info =
      app_def.CreateWebAppInstallInfo();

  EXPECT_EQ(blink::mojom::DisplayMode::kStandalone, install_info->display_mode);
}

TEST_F(PreloadAppDefinitionTest, GetWebAppManifestId) {
  proto::AppProvisioningResponse_App app = CreateTestWebApp();
  app.mutable_web_extras()->set_manifest_id(
      "https://www.example.com/manifest_id/");

  PreloadAppDefinition app_def(app);

  ASSERT_EQ(app_def.GetWebAppManifestId(),
            "https://www.example.com/manifest_id/");
}

TEST_F(PreloadAppDefinitionTest, GetWebAppManifestIdNotSpecified) {
  proto::AppProvisioningResponse_App app;
  app.set_platform(proto::AppProvisioningResponse::PLATFORM_WEB);

  PreloadAppDefinition app_def(app);

  ASSERT_TRUE(app_def.GetWebAppManifestId().empty());
}

}  // namespace apps
