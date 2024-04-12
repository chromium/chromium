// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <climits>
#include <memory>
#include <string>
#include <vector>

#include "base/functional/function_ref.h"
#include "base/values.h"
#include "chrome/browser/badging/badge_manager.h"
#include "chrome/browser/badging/badge_manager_factory.h"
#include "chrome/browser/devtools/protocol/devtools_protocol_test_support.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/web_applications/proto/web_app_os_integration_state.pb.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

namespace {

using web_app::WebAppInstallInfo;

class PWAProtocolTestWithoutApp : public DevToolsProtocolTestBase {
 public:
  void SetUpOnMainThread() override {
    DevToolsProtocolTestBase::SetUpOnMainThread();
    AttachToBrowserTarget();
  }

 protected:
  void ReattachToWebContents() {
    DetachProtocolClient();
    Attach();
  }
};

class PWAProtocolTest : public PWAProtocolTestWithoutApp {
 public:
  void SetUp() override {
    embedded_test_server()->AddDefaultHandlers(GetChromeTestDataDir());
    test_server_closer_ = embedded_test_server()->StartAndReturnHandle();
    // This is strange, but the tests are running in the SetUp(), so the
    // embedded_test_server() needs to be started first.
    PWAProtocolTestWithoutApp::SetUp();
  }

  void SetUpOnMainThread() override {
    PWAProtocolTestWithoutApp::SetUpOnMainThread();
    override_registration_ =
        web_app::OsIntegrationTestOverrideImpl::OverrideForTesting();
  }

  void TearDownOnMainThread() override {
    web_app::test::UninstallAllWebApps(browser()->profile());
    override_registration_.reset();
    PWAProtocolTestWithoutApp::TearDownOnMainThread();
  }

 protected:
  webapps::AppId InstallWebApp(
      base::FunctionRef<void(WebAppInstallInfo&)> init) const {
    std::unique_ptr<WebAppInstallInfo> web_app_info =
        WebAppInstallInfo::CreateWithStartUrlForTesting(InstallableWebAppUrl());
    // The title needs to match the web app to avoid triggering an update.
    web_app_info->title = u"Basic web app";
    init(*web_app_info);
    return web_app::test::InstallWebApp(browser()->profile(),
                                        std::move(web_app_info));
  }

  webapps::AppId InstallWebApp() const {
    return InstallWebApp([](WebAppInstallInfo& web_app_info) {});
  }

  GURL InstallableWebAppUrl() const {
    return embedded_test_server()->GetURL("/web_apps/basic.html");
  }

  GURL UninstallableWebAppUrl() const {
    return embedded_test_server()->GetURL(
        "/web_apps/title_appname_prefix.html");
  }

 private:
  net::test_server::EmbeddedTestServerHandle test_server_closer_;
  std::unique_ptr<web_app::OsIntegrationTestOverrideImpl::BlockingRegistration>
      override_registration_;
};

IN_PROC_BROWSER_TEST_F(PWAProtocolTestWithoutApp, GetOsAppState_CannotFindApp) {
  base::Value::Dict params;
  params.Set("manifestId", "ThisIsNotAValidManifestId");
  const base::Value::Dict* result =
      SendCommandSync("PWA.getOsAppState", std::move(params));
  ASSERT_FALSE(result);
  ASSERT_TRUE(error());
  // Expect the input manifestId to be carried over by the error message.
  const std::string& message = *error()->FindString("message");
  ASSERT_NE(message.find("ThisIsNotAValidManifestId"), std::string::npos);
}

IN_PROC_BROWSER_TEST_F(PWAProtocolTest, GetOsAppState) {
  InstallWebApp();
  base::Value::Dict params;
  params.Set("manifestId", InstallableWebAppUrl().spec());
  const base::Value::Dict* result =
      SendCommandSync("PWA.getOsAppState", std::move(params));
  ASSERT_TRUE(result);
  ASSERT_EQ(*result->FindInt("badgeCount"), 0);
  ASSERT_TRUE(result->FindList("fileHandlers")->empty());
}

IN_PROC_BROWSER_TEST_F(PWAProtocolTest, GetOsAppState_WithBadge) {
  webapps::AppId app_id = InstallWebApp();
  ukm::TestUkmRecorder test_recorder;
  badging::BadgeManagerFactory::GetForProfile(browser()->profile())
      ->SetBadgeForTesting(app_id, 11, &test_recorder);
  base::Value::Dict params;
  params.Set("manifestId", InstallableWebAppUrl().spec());
  const base::Value::Dict* result =
      SendCommandSync("PWA.getOsAppState", std::move(params));
  ASSERT_TRUE(result);
  ASSERT_EQ(*result->FindInt("badgeCount"), 11);
}

IN_PROC_BROWSER_TEST_F(PWAProtocolTest, GetOsAppState_WithZeroBadge) {
  webapps::AppId app_id = InstallWebApp();
  ukm::TestUkmRecorder test_recorder;
  badging::BadgeManagerFactory::GetForProfile(browser()->profile())
      ->SetBadgeForTesting(app_id, 0, &test_recorder);
  base::Value::Dict params;
  params.Set("manifestId", InstallableWebAppUrl().spec());
  const base::Value::Dict* result =
      SendCommandSync("PWA.getOsAppState", std::move(params));
  ASSERT_TRUE(result);
  ASSERT_EQ(*result->FindInt("badgeCount"), 0);
}

IN_PROC_BROWSER_TEST_F(PWAProtocolTest, GetOsAppState_WithBadgeOverInt) {
  webapps::AppId app_id = InstallWebApp();
  ukm::TestUkmRecorder test_recorder;
  badging::BadgeManagerFactory::GetForProfile(browser()->profile())
      ->SetBadgeForTesting(app_id, static_cast<uint64_t>(INT_MAX) + 1,
                           &test_recorder);
  base::Value::Dict params;
  params.Set("manifestId", InstallableWebAppUrl().spec());
  const base::Value::Dict* result =
      SendCommandSync("PWA.getOsAppState", std::move(params));
  ASSERT_TRUE(result);
  ASSERT_EQ(*result->FindInt("badgeCount"), INT_MAX);
}

IN_PROC_BROWSER_TEST_F(PWAProtocolTest, GetOsAppState_WithFileHandler) {
  webapps::AppId app_id =
      InstallWebApp([this](WebAppInstallInfo& web_app_info) {
        apps::FileHandler file_handler;
        file_handler.action = InstallableWebAppUrl().Resolve("/file_handler");
        apps::FileHandler::AcceptEntry entry;
        entry.mime_type = "image/jpeg";
        entry.file_extensions.insert(".jpg");
        entry.file_extensions.insert(".jpeg");
        file_handler.accept.push_back(entry);
        web_app_info.file_handlers.push_back(file_handler);
      });
  base::Value::Dict params;
  params.Set("manifestId", InstallableWebAppUrl().spec());
  const base::Value::Dict* result =
      SendCommandSync("PWA.getOsAppState", std::move(params));
  ASSERT_TRUE(result);
  ASSERT_EQ(result->FindList("fileHandlers")->size(), 1UL);
  const auto& handler = result->FindList("fileHandlers")->front().DebugString();
  // Check if several fields exist instead of repeating the conversions.
  ASSERT_NE(handler.find("/file_handler"), std::string::npos);
  ASSERT_NE(handler.find("image/jpeg"), std::string::npos);
  ASSERT_NE(handler.find(".jpg"), std::string::npos);
  ASSERT_NE(handler.find(".jpeg"), std::string::npos);
}

IN_PROC_BROWSER_TEST_F(PWAProtocolTest, GetProcessedManifest_CannotFindApp) {
  base::Value::Dict params;
  params.Set("manifestId", "ThisIsNotAValidManifestId");
  const base::Value::Dict* result =
      SendCommandSync("Page.getAppManifest", std::move(params));
  ASSERT_FALSE(result);
  ASSERT_TRUE(error());
  const std::string& message = *error()->FindString("message");
  // Expect the input manifest id to be carried over by the error message, but
  // now the API won't work on browser target at all.
  ASSERT_NE(message.find("Page.getAppManifest"), std::string::npos);
}

IN_PROC_BROWSER_TEST_F(PWAProtocolTest,
                       GetProcessedManifest_CannotFindApp_WithoutManfiestId) {
  const base::Value::Dict* result =
      SendCommandSync("Page.getAppManifest", base::Value::Dict{});
  ASSERT_FALSE(result);
  ASSERT_TRUE(error());
}

IN_PROC_BROWSER_TEST_F(PWAProtocolTest,
                       GetProcessedManifest_WithoutManifestId) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), InstallableWebAppUrl()));
  EXPECT_TRUE(content::WaitForLoadStop(web_contents()));
  ReattachToWebContents();
  const base::Value::Dict* result =
      SendCommandSync("Page.getAppManifest", base::Value::Dict{});
  ASSERT_TRUE(result);
  result = result->FindDict("manifest");
  ASSERT_TRUE(result);
  ASSERT_EQ(*result->FindString("id"), InstallableWebAppUrl().spec());
  const auto& manifest = result->DebugString();
  // Check if several fields exist instead of repeating the conversions.
  ASSERT_NE(manifest.find("/web_apps/basic-48.png"), std::string::npos);
  ASSERT_NE(manifest.find("/web_apps/basic-192.png"), std::string::npos);
  ASSERT_NE(manifest.find("preferRelatedApplications"), std::string::npos);
  ASSERT_NE(manifest.find("kStandalone"), std::string::npos);
}

IN_PROC_BROWSER_TEST_F(PWAProtocolTest, GetProcessedManifest_WithManifestId) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), InstallableWebAppUrl()));
  EXPECT_TRUE(content::WaitForLoadStop(web_contents()));
  ReattachToWebContents();
  base::Value::Dict params;
  params.Set("manifestId", InstallableWebAppUrl().spec());
  const base::Value::Dict* result =
      SendCommandSync("Page.getAppManifest", std::move(params));
  ASSERT_TRUE(result);
  result = result->FindDict("manifest");
  ASSERT_TRUE(result);
  ASSERT_EQ(*result->FindString("id"), InstallableWebAppUrl().spec());
}

IN_PROC_BROWSER_TEST_F(PWAProtocolTest, GetProcessedManifest_MismatchId) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), InstallableWebAppUrl()));
  EXPECT_TRUE(content::WaitForLoadStop(web_contents()));
  ReattachToWebContents();
  base::Value::Dict params;
  params.Set("manifestId", "ThisIsNotAValidManifestId");
  const base::Value::Dict* result =
      SendCommandSync("Page.getAppManifest", std::move(params));
  ASSERT_FALSE(result);
  ASSERT_TRUE(error());
  // Expect the input manifest id and original manifest id to be carried over by
  // the error message.
  const std::string& message = *error()->FindString("message");
  ASSERT_NE(message.find(InstallableWebAppUrl().spec()), std::string::npos);
  ASSERT_NE(message.find("ThisIsNotAValidManifestId"), std::string::npos);
}

IN_PROC_BROWSER_TEST_F(PWAProtocolTest,
                       GetProcessedManifest_NotOnPage_WithManifestId) {
  base::Value::Dict params;
  params.Set("manifestId", InstallableWebAppUrl().spec());
  const base::Value::Dict* result =
      SendCommandSync("Page.getAppManifest", std::move(params));
  ASSERT_FALSE(result);
  ASSERT_TRUE(error());
  const std::string& message = *error()->FindString("message");
  // Expect the input manifest id to be carried over by the error message, but
  // now the API won't work on browser target at all.
  ASSERT_NE(message.find("Page.getAppManifest"), std::string::npos);
}

IN_PROC_BROWSER_TEST_F(PWAProtocolTest, GetProcessedManifest_NotInstallable) {
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), UninstallableWebAppUrl()));
  EXPECT_TRUE(content::WaitForLoadStop(web_contents()));
  ReattachToWebContents();
  const base::Value::Dict* result =
      SendCommandSync("Page.getAppManifest", base::Value::Dict{});
  ASSERT_TRUE(result);
  result = result->FindDict("manifest");
  ASSERT_TRUE(result);
  ASSERT_EQ(*result->FindString("id"), UninstallableWebAppUrl().spec());
  ASSERT_EQ(*result->FindString("startUrl"), UninstallableWebAppUrl().spec());
  ASSERT_EQ(*result->FindString("scope"),
            embedded_test_server()->GetURL("/web_apps/").spec());
}

}  // namespace
