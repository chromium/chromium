// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <climits>
#include <memory>
#include <string>
#include <vector>

#include "base/functional/function_ref.h"
#include "base/strings/string_util.h"
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
using web_app::WebAppProvider;
using webapps::ManifestId;

class PWAProtocolTestWithoutApp : public DevToolsProtocolTestBase {
 public:
  void SetUpOnMainThread() override {
    DevToolsProtocolTestBase::SetUpOnMainThread();
    AttachToBrowserTarget();
  }

 protected:
  void LoadWebContents(GURL url) {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    EXPECT_TRUE(content::WaitForLoadStop(web_contents()));
  }

  void ReattachToWebContents(GURL url) {
    LoadWebContents(std::move(url));
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

  // For basic.html, the manifest-id equals to its start-url.
  ManifestId InstallableWebAppManifestId() const {
    return InstallableWebAppUrl();
  }

  GURL NotInstallableWebAppUrl() const {
    return embedded_test_server()->GetURL(
        "/web_apps/title_appname_prefix.html");
  }

  // The default manifest uses the original url if it's not a web-app without
  // a manifest link.
  ManifestId NotInstallableWebAppManifestId() const {
    return NotInstallableWebAppUrl();
  }

  GURL HasManifestIdWebAppUrl() const {
    return embedded_test_server()->GetURL("/web_apps/has_manifest_id.html");
  }

  ManifestId HasManifestIdWebAppManifestId() const {
    return embedded_test_server()->GetURL(
        "/web_apps/has_manifest_id_unique_id");
  }

  GURL GetInstallableSiteWithManifest(std::string_view json_path) const {
    return embedded_test_server()->GetURL(
        std::string("/web_apps/get_manifest.html?").append(json_path));
  }

  bool AppExists(const ManifestId& manifest_id) {
    auto* provider = WebAppProvider::GetForTest(browser()->profile());
    CHECK(provider);
    return provider->registrar_unsafe().IsInstalled(
        web_app::GenerateAppIdFromManifestId(manifest_id));
  }

  void InstallFromManifest() {
    base::Value::Dict params;
    params.Set("manifestId", InstallableWebAppManifestId().spec());
    ASSERT_TRUE(SendCommandSync("PWA.install", std::move(params)));
    ASSERT_TRUE(AppExists(InstallableWebAppManifestId()));
  }

  void InstallFromUrl() {
    base::Value::Dict params;
    params.Set("manifestId", InstallableWebAppManifestId().spec());
    params.Set("installUrlOrBundleUrl", InstallableWebAppUrl().spec());
    ASSERT_TRUE(SendCommandSync("PWA.install", std::move(params)));
    ASSERT_TRUE(AppExists(InstallableWebAppManifestId()));
  }

  static GURL UpperCaseScheme(const GURL& origin) {
    std::string spec{origin.spec()};
    for (size_t i = 0; i < origin.scheme().length(); i++) {
      spec[i] = base::ToUpperASCII(spec[i]);
    }
    return GURL{spec};
  }

 private:
  net::test_server::EmbeddedTestServerHandle test_server_closer_;
  std::unique_ptr<web_app::OsIntegrationTestOverrideImpl::BlockingRegistration>
      override_registration_;
};

IN_PROC_BROWSER_TEST_F(PWAProtocolTestWithoutApp, GetOsAppState_CannotFindApp) {
  base::Value::Dict params;
  params.Set("manifestId", "ThisIsNotAValidManifestId");
  ASSERT_FALSE(SendCommandSync("PWA.getOsAppState", std::move(params)));
  ASSERT_TRUE(error());
  // Expect the input manifestId to be carried over by the error message.
  const std::string& message = *error()->FindString("message");
  ASSERT_NE(message.find("ThisIsNotAValidManifestId"), std::string::npos);
}

IN_PROC_BROWSER_TEST_F(PWAProtocolTest, GetOsAppState) {
  InstallWebApp();
  base::Value::Dict params;
  params.Set("manifestId", InstallableWebAppManifestId().spec());
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
  params.Set("manifestId", InstallableWebAppManifestId().spec());
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
  params.Set("manifestId", InstallableWebAppManifestId().spec());
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
  params.Set("manifestId", InstallableWebAppManifestId().spec());
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
  params.Set("manifestId", InstallableWebAppManifestId().spec());
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
  ASSERT_FALSE(SendCommandSync("Page.getAppManifest", std::move(params)));
  ASSERT_TRUE(error());
  const std::string& message = *error()->FindString("message");
  // Expect the input manifest id to be carried over by the error message, but
  // now the API won't work on browser target at all.
  ASSERT_NE(message.find("Page.getAppManifest"), std::string::npos);
}

IN_PROC_BROWSER_TEST_F(PWAProtocolTest,
                       GetProcessedManifest_CannotFindApp_WithoutManfiestId) {
  ASSERT_FALSE(SendCommandSync("Page.getAppManifest", base::Value::Dict{}));
  ASSERT_TRUE(error());
}

IN_PROC_BROWSER_TEST_F(PWAProtocolTest,
                       GetProcessedManifest_WithoutManifestId) {
  ReattachToWebContents(InstallableWebAppUrl());
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
  ReattachToWebContents(InstallableWebAppUrl());
  base::Value::Dict params;
  params.Set("manifestId", InstallableWebAppManifestId().spec());
  const base::Value::Dict* result =
      SendCommandSync("Page.getAppManifest", std::move(params));
  ASSERT_TRUE(result);
  result = result->FindDict("manifest");
  ASSERT_TRUE(result);
  ASSERT_EQ(*result->FindString("id"), InstallableWebAppUrl().spec());
}

IN_PROC_BROWSER_TEST_F(PWAProtocolTest, GetProcessedManifest_MismatchId) {
  ReattachToWebContents(InstallableWebAppUrl());
  base::Value::Dict params;
  params.Set("manifestId", "ThisIsNotAValidManifestId");
  ASSERT_FALSE(SendCommandSync("Page.getAppManifest", std::move(params)));
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
  params.Set("manifestId", InstallableWebAppManifestId().spec());
  ASSERT_FALSE(SendCommandSync("Page.getAppManifest", std::move(params)));
  ASSERT_TRUE(error());
  const std::string& message = *error()->FindString("message");
  // Expect the input manifest id to be carried over by the error message, but
  // now the API won't work on browser target at all.
  ASSERT_NE(message.find("Page.getAppManifest"), std::string::npos);
}

IN_PROC_BROWSER_TEST_F(PWAProtocolTest, GetProcessedManifest_NotInstallable) {
  ReattachToWebContents(NotInstallableWebAppUrl());
  const base::Value::Dict* result =
      SendCommandSync("Page.getAppManifest", base::Value::Dict{});
  ASSERT_TRUE(result);
  result = result->FindDict("manifest");
  ASSERT_TRUE(result);
  ASSERT_EQ(*result->FindString("id"), NotInstallableWebAppUrl().spec());
  ASSERT_EQ(*result->FindString("startUrl"), NotInstallableWebAppUrl().spec());
  ASSERT_EQ(*result->FindString("scope"),
            embedded_test_server()->GetURL("/web_apps/").spec());
}

IN_PROC_BROWSER_TEST_F(PWAProtocolTest, Uninstall) {
  ASSERT_FALSE(AppExists(InstallableWebAppManifestId()));
  InstallWebApp();
  ASSERT_TRUE(AppExists(InstallableWebAppManifestId()));

  base::Value::Dict params;
  params.Set("manifestId", InstallableWebAppManifestId().spec());
  SendCommandSync("PWA.uninstall", std::move(params));
  ASSERT_TRUE(result());
  ASSERT_FALSE(error());

  ASSERT_FALSE(AppExists(InstallableWebAppManifestId()));
}

IN_PROC_BROWSER_TEST_F(PWAProtocolTest, Uninstall_CannotFindApp) {
  ASSERT_FALSE(AppExists(InstallableWebAppManifestId()));
  base::Value::Dict params;
  params.Set("manifestId", InstallableWebAppManifestId().spec());
  // Treat uninstalling nonexisting apps as a success.
  ASSERT_TRUE(SendCommandSync("PWA.uninstall", std::move(params)));
  ASSERT_FALSE(AppExists(InstallableWebAppManifestId()));
}

IN_PROC_BROWSER_TEST_F(PWAProtocolTest, Uninstall_MissingManifestId) {
  ASSERT_FALSE(SendCommandSync("PWA.uninstall", base::Value::Dict{}));
}

IN_PROC_BROWSER_TEST_F(PWAProtocolTest, Install_FromManifest) {
  ASSERT_FALSE(AppExists(InstallableWebAppManifestId()));
  ReattachToWebContents(InstallableWebAppUrl());
  InstallFromManifest();
}

IN_PROC_BROWSER_TEST_F(PWAProtocolTest, Install_FromManifest_Twice) {
  ASSERT_FALSE(AppExists(InstallableWebAppManifestId()));
  ReattachToWebContents(InstallableWebAppUrl());
  InstallFromManifest();
  // Install a same application twice won't trigger an error.
  InstallFromManifest();
}

IN_PROC_BROWSER_TEST_F(PWAProtocolTest, Install_FromManifest_NoWebContents) {
  // Only load the WebContents without attaching the devtools session to it.
  // By default, the devtools is being attached to the browser in the
  // PWAProtocolTestWithoutApp::SetUpOnMainThread.
  // So the PWAHandler cannot install the webapp with only the manifest-id.
  LoadWebContents(InstallableWebAppUrl());
  base::Value::Dict params;
  params.Set("manifestId", InstallableWebAppManifestId().spec());
  ASSERT_FALSE(SendCommandSync("PWA.install", std::move(params)));
  ASSERT_TRUE(error());
  const std::string& message = *error()->FindString("message");
  ASSERT_NE(message.find(InstallableWebAppUrl().spec()), std::string::npos);
  ASSERT_FALSE(AppExists(InstallableWebAppManifestId()));
}

IN_PROC_BROWSER_TEST_F(PWAProtocolTest, Install_FromManifest_InvalidStartUrl) {
  const GURL url{GetInstallableSiteWithManifest("invalid_start_url.json")};
  ReattachToWebContents(url);
  base::Value::Dict params;
  params.Set("manifestId", url.spec());
  ASSERT_FALSE(SendCommandSync("PWA.install", std::move(params)));
  ASSERT_TRUE(error());
  const std::string& message = *error()->FindString("message");
  ASSERT_NE(message.find(url.spec()), std::string::npos);
  ASSERT_FALSE(AppExists(url));
  ASSERT_FALSE(AppExists(ManifestId{"http://different.origin/is-invalid"}));
}

IN_PROC_BROWSER_TEST_F(PWAProtocolTest,
                       Install_FromManifest_InconsistentAppId) {
  const GURL url{GetInstallableSiteWithManifest("basic.json")};
  ReattachToWebContents(url);
  base::Value::Dict params;
  params.Set("manifestId", url.spec());
  ASSERT_FALSE(SendCommandSync("PWA.install", std::move(params)));
  ASSERT_TRUE(error());
  const std::string& message = *error()->FindString("message");
  ASSERT_NE(message.find(url.spec()), std::string::npos);
  ASSERT_NE(message.find(GetInstallableSiteWithManifest("basic.json").spec()),
            std::string::npos);
  ASSERT_FALSE(AppExists(url));
  ASSERT_FALSE(AppExists(InstallableWebAppManifestId()));
}

IN_PROC_BROWSER_TEST_F(PWAProtocolTest, Install_FromManifest_HasManifestId) {
  ReattachToWebContents(HasManifestIdWebAppUrl());
  base::Value::Dict params;
  params.Set("manifestId", HasManifestIdWebAppManifestId().spec());
  ASSERT_TRUE(SendCommandSync("PWA.install", std::move(params)));
  ASSERT_TRUE(AppExists(HasManifestIdWebAppManifestId()));
  ASSERT_FALSE(AppExists(HasManifestIdWebAppUrl()));
}

IN_PROC_BROWSER_TEST_F(PWAProtocolTest,
                       Install_FromManifest_HasManifestId_UrlAsManifestId) {
  ReattachToWebContents(HasManifestIdWebAppUrl());
  base::Value::Dict params;
  params.Set("manifestId", HasManifestIdWebAppUrl().spec());
  ASSERT_FALSE(SendCommandSync("PWA.install", std::move(params)));
  const std::string& message = *error()->FindString("message");
  ASSERT_NE(message.find(HasManifestIdWebAppUrl().spec()), std::string::npos);
  ASSERT_FALSE(AppExists(HasManifestIdWebAppManifestId()));
  ASSERT_FALSE(AppExists(HasManifestIdWebAppUrl()));
}

IN_PROC_BROWSER_TEST_F(PWAProtocolTest, Install_FromUrl) {
  InstallFromUrl();
}

IN_PROC_BROWSER_TEST_F(PWAProtocolTest, Install_FromUrl_Twice) {
  InstallFromUrl();
  // Install a same application twice won't trigger an error.
  InstallFromUrl();
}

IN_PROC_BROWSER_TEST_F(PWAProtocolTest, Install_FromManifest_FromUrl) {
  ASSERT_FALSE(AppExists(InstallableWebAppManifestId()));
  ReattachToWebContents(InstallableWebAppUrl());
  InstallFromManifest();
  // Install a same application twice won't trigger an error.
  InstallFromUrl();
}

IN_PROC_BROWSER_TEST_F(PWAProtocolTest, Install_FromUrl_FromManifest) {
  ASSERT_FALSE(AppExists(InstallableWebAppManifestId()));
  InstallFromUrl();
  // Install a same application twice won't trigger an error.
  ReattachToWebContents(InstallableWebAppUrl());
  InstallFromManifest();
}

IN_PROC_BROWSER_TEST_F(PWAProtocolTest, Install_FromUrl_UpperCase) {
  base::Value::Dict params;
  params.Set("manifestId",
             UpperCaseScheme(InstallableWebAppManifestId()).spec());
  params.Set("installUrlOrBundleUrl",
             UpperCaseScheme(InstallableWebAppUrl()).spec());
  ASSERT_TRUE(SendCommandSync("PWA.install", std::move(params)));
  ASSERT_TRUE(AppExists(InstallableWebAppManifestId()));
}

IN_PROC_BROWSER_TEST_F(PWAProtocolTest, Install_FromUrl_Unreachable) {
  base::Value::Dict params;
  params.Set("manifestId", InstallableWebAppManifestId().spec());
  params.Set("installUrlOrBundleUrl", "http://hello/this/is/not/existing");
  ASSERT_FALSE(SendCommandSync("PWA.install", std::move(params)));
  const std::string& message = *error()->FindString("message");
  ASSERT_NE(message.find(InstallableWebAppUrl().spec()), std::string::npos);
  ASSERT_NE(message.find("http://hello/this/is/not/existing"),
            std::string::npos);
  ASSERT_FALSE(AppExists(InstallableWebAppManifestId()));
}

IN_PROC_BROWSER_TEST_F(PWAProtocolTest, Install_FromUrl_UnmatchManifestId) {
  base::Value::Dict params;
  params.Set("manifestId", NotInstallableWebAppUrl().spec());
  params.Set("installUrlOrBundleUrl", InstallableWebAppUrl().spec());
  ASSERT_FALSE(SendCommandSync("PWA.install", std::move(params)));
  const std::string& message = *error()->FindString("message");
  ASSERT_NE(message.find(InstallableWebAppUrl().spec()), std::string::npos);
  ASSERT_NE(message.find(NotInstallableWebAppUrl().spec()), std::string::npos);
  ASSERT_FALSE(AppExists(InstallableWebAppManifestId()));
  ASSERT_FALSE(AppExists(NotInstallableWebAppManifestId()));
}

// TODO(crbug.com/331214986): May want a test to trigger the installation
// failure when installing from the url.

IN_PROC_BROWSER_TEST_F(PWAProtocolTest,
                       Install_FromUrl_ValidManifestId_DifferentInstallUrl) {
  const GURL url{GetInstallableSiteWithManifest("basic.json")};
  base::Value::Dict params;
  params.Set("manifestId", InstallableWebAppManifestId().spec());
  params.Set("installUrlOrBundleUrl", url.spec());
  ASSERT_TRUE(SendCommandSync("PWA.install", std::move(params)));
  ASSERT_FALSE(AppExists(url));
  ASSERT_TRUE(AppExists(InstallableWebAppManifestId()));
}

IN_PROC_BROWSER_TEST_F(PWAProtocolTest, Install_FromUrl_InconsistentAppId) {
  const GURL url{GetInstallableSiteWithManifest("basic.json")};
  base::Value::Dict params;
  params.Set("manifestId", url.spec());
  params.Set("installUrlOrBundleUrl", InstallableWebAppUrl().spec());
  ASSERT_FALSE(SendCommandSync("PWA.install", std::move(params)));
  ASSERT_TRUE(error());
  const std::string& message = *error()->FindString("message");
  ASSERT_NE(message.find(url.spec()), std::string::npos);
  ASSERT_FALSE(AppExists(url));
  ASSERT_FALSE(AppExists(InstallableWebAppManifestId()));
}

IN_PROC_BROWSER_TEST_F(PWAProtocolTest, Install_FromUrl_NoScheme) {
  base::Value::Dict params;
  params.Set("manifestId", InstallableWebAppManifestId().spec());
  params.Set("installUrlOrBundleUrl", "localhost/");
  ASSERT_FALSE(SendCommandSync("PWA.install", std::move(params)));
  ASSERT_TRUE(error());
  const std::string& message = *error()->FindString("message");
  ASSERT_NE(message.find("localhost/"), std::string::npos);
  ASSERT_FALSE(AppExists(InstallableWebAppManifestId()));
}

IN_PROC_BROWSER_TEST_F(PWAProtocolTest, Install_FromUrl_UnsupportedScheme) {
  base::Value::Dict params;
  params.Set("manifestId", InstallableWebAppManifestId().spec());
  params.Set("installUrlOrBundleUrl", "ftp://localhost/");
  ASSERT_FALSE(SendCommandSync("PWA.install", std::move(params)));
  ASSERT_TRUE(error());
  const std::string& message = *error()->FindString("message");
  ASSERT_NE(message.find("ftp"), std::string::npos);
  ASSERT_NE(message.find("ftp://localhost/"), std::string::npos);
  ASSERT_FALSE(AppExists(InstallableWebAppManifestId()));
}

IN_PROC_BROWSER_TEST_F(PWAProtocolTest, Install_FromUrl_HasManifestId) {
  base::Value::Dict params;
  params.Set("manifestId", HasManifestIdWebAppManifestId().spec());
  params.Set("installUrlOrBundleUrl", HasManifestIdWebAppUrl().spec());
  ASSERT_TRUE(SendCommandSync("PWA.install", std::move(params)));
  ASSERT_TRUE(AppExists(HasManifestIdWebAppManifestId()));
  ASSERT_FALSE(AppExists(HasManifestIdWebAppUrl()));
}

IN_PROC_BROWSER_TEST_F(PWAProtocolTest, Install_Uninstall) {
  ASSERT_FALSE(AppExists(InstallableWebAppManifestId()));
  ReattachToWebContents(InstallableWebAppUrl());
  base::Value::Dict params;
  params.Set("manifestId", InstallableWebAppManifestId().spec());

  ASSERT_TRUE(SendCommandSync("PWA.install", params.Clone()));
  ASSERT_TRUE(AppExists(InstallableWebAppManifestId()));

  ASSERT_TRUE(SendCommandSync("PWA.uninstall", params.Clone()));
  ASSERT_FALSE(AppExists(InstallableWebAppManifestId()));
}

}  // namespace
