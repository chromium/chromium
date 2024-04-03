// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <climits>
#include <memory>
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
#include "components/services/app_service/public/cpp/file_handler.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

namespace {

using web_app::WebAppInstallInfo;

class PWAProtocolTestWithoutApp : public DevToolsProtocolTestBase {
 public:
  void SetUpOnMainThread() override { AttachToBrowserTarget(); }
};

class PWAProtocolTest : public PWAProtocolTestWithoutApp {
 public:
  void SetUp() override {
    embedded_test_server()->AddDefaultHandlers(GetChromeTestDataDir());
    test_server_closer_ = embedded_test_server()->StartAndReturnHandle();
    PWAProtocolTestWithoutApp::SetUp();
  }

  void SetUpOnMainThread() override {
    override_registration_ =
        web_app::OsIntegrationTestOverrideImpl::OverrideForTesting();
    PWAProtocolTestWithoutApp::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    web_app::test::UninstallAllWebApps(browser()->profile());
    override_registration_.reset();
    PWAProtocolTestWithoutApp::TearDownOnMainThread();
  }

 protected:
  webapps::AppId CreateWebApp(
      base::FunctionRef<void(WebAppInstallInfo&)> init) const {
    GURL start_url = url();
    std::unique_ptr<WebAppInstallInfo> web_app_info =
        WebAppInstallInfo::CreateWithStartUrlForTesting(start_url);
    web_app_info->title = u"test web app";
    init(*web_app_info);
    return web_app::test::InstallWebApp(browser()->profile(),
                                        std::move(web_app_info));
  }

  webapps::AppId CreateWebApp() const {
    return CreateWebApp([](WebAppInstallInfo& web_app_info) {});
  }

  GURL url() const {
    return embedded_test_server()->GetURL("app.com", "/google/google.html");
  }

 private:
  net::test_server::EmbeddedTestServerHandle test_server_closer_;
  std::unique_ptr<web_app::OsIntegrationTestOverrideImpl::BlockingRegistration>
      override_registration_;
};

IN_PROC_BROWSER_TEST_F(PWAProtocolTestWithoutApp, CannotFindApp) {
  base::Value::Dict params;
  params.Set("manifestId", "ThisIsNotAValidManifestId");
  const base::Value::Dict* result =
      SendCommandSync("PWA.getOsAppState", std::move(params));
  ASSERT_FALSE(result);
  ASSERT_TRUE(error());
  // Expect the input manifestId to be carried over by the error message.
  ASSERT_NE(error()->DebugString().find("ThisIsNotAValidManifestId"),
            std::string::npos);
}

IN_PROC_BROWSER_TEST_F(PWAProtocolTest, GetOsAppState) {
  CreateWebApp();
  base::Value::Dict params;
  params.Set("manifestId", url().spec());
  const base::Value::Dict* result =
      SendCommandSync("PWA.getOsAppState", std::move(params));
  ASSERT_TRUE(result);
  ASSERT_EQ(*result->FindInt("badgeCount"), 0);
  ASSERT_TRUE(result->FindList("fileHandlers")->empty());
}

IN_PROC_BROWSER_TEST_F(PWAProtocolTest, GetOsAppStateWithBadge) {
  webapps::AppId app_id = CreateWebApp();
  ukm::TestUkmRecorder test_recorder;
  badging::BadgeManagerFactory::GetForProfile(browser()->profile())
      ->SetBadgeForTesting(app_id, 11, &test_recorder);
  base::Value::Dict params;
  params.Set("manifestId", url().spec());
  const base::Value::Dict* result =
      SendCommandSync("PWA.getOsAppState", std::move(params));
  ASSERT_TRUE(result);
  ASSERT_EQ(*result->FindInt("badgeCount"), 11);
}

IN_PROC_BROWSER_TEST_F(PWAProtocolTest, GetOsAppStateWithZeroBadge) {
  webapps::AppId app_id = CreateWebApp();
  ukm::TestUkmRecorder test_recorder;
  badging::BadgeManagerFactory::GetForProfile(browser()->profile())
      ->SetBadgeForTesting(app_id, 0, &test_recorder);
  base::Value::Dict params;
  params.Set("manifestId", url().spec());
  const base::Value::Dict* result =
      SendCommandSync("PWA.getOsAppState", std::move(params));
  ASSERT_TRUE(result);
  ASSERT_EQ(*result->FindInt("badgeCount"), 0);
}

IN_PROC_BROWSER_TEST_F(PWAProtocolTest, GetOsAppStateWithBadgeOverInt) {
  webapps::AppId app_id = CreateWebApp();
  ukm::TestUkmRecorder test_recorder;
  badging::BadgeManagerFactory::GetForProfile(browser()->profile())
      ->SetBadgeForTesting(app_id, static_cast<uint64_t>(INT_MAX) + 1,
                           &test_recorder);
  base::Value::Dict params;
  params.Set("manifestId", url().spec());
  const base::Value::Dict* result =
      SendCommandSync("PWA.getOsAppState", std::move(params));
  ASSERT_TRUE(result);
  ASSERT_EQ(*result->FindInt("badgeCount"), INT_MAX);
}

IN_PROC_BROWSER_TEST_F(PWAProtocolTest, GetOsAppStateWithFileHandler) {
  webapps::AppId app_id = CreateWebApp([this](WebAppInstallInfo& web_app_info) {
    apps::FileHandler file_handler;
    file_handler.action = url().Resolve("/file_handler");
    apps::FileHandler::AcceptEntry entry;
    entry.mime_type = "image/jpeg";
    entry.file_extensions.insert(".jpg");
    entry.file_extensions.insert(".jpeg");
    file_handler.accept.push_back(entry);
    web_app_info.file_handlers.push_back(file_handler);
  });
  base::Value::Dict params;
  params.Set("manifestId", url().spec());
  const base::Value::Dict* result =
      SendCommandSync("PWA.getOsAppState", std::move(params));
  ASSERT_TRUE(result);
  ASSERT_EQ(result->FindList("fileHandlers")->size(), 1UL);
  const auto& handler = result->FindList("fileHandlers")->front().DebugString();
  ASSERT_NE(handler.find("/file_handler"), std::string::npos);
  ASSERT_NE(handler.find("image/jpeg"), std::string::npos);
  ASSERT_NE(handler.find(".jpg"), std::string::npos);
  ASSERT_NE(handler.find(".jpeg"), std::string::npos);
}

}  // namespace
