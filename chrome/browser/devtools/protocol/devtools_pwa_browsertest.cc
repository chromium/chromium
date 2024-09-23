// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <climits>
#include <initializer_list>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include "base/base_paths.h"
#include "base/check.h"
#include "base/functional/function_ref.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/badging/badge_manager.h"
#include "chrome/browser/badging/badge_manager_factory.h"
#include "chrome/browser/devtools/protocol/devtools_protocol_test_support.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/web_applications/proto/web_app_os_integration_state.pb.h"
#include "chrome/browser/web_applications/proto/web_app_proto_package.pb.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/devtools_agent_host.h"
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
    ASSERT_TRUE(content::WaitForLoadStop(web_contents()));
  }

  void ReattachToWebContents(GURL url) {
    LoadWebContents(std::move(url));
    DetachProtocolClient();
    Attach();
  }

  bool ErrorMessageContains(std::initializer_list<std::string> pieces) const {
    if (!error()) {
      return false;
    }
    const std::string& message = *error()->FindString("message");
    for (const auto& piece : pieces) {
      if (message.find(piece) == std::string::npos) {
        return false;
      }
    }
    return true;
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
    test_data_path_ =
        base::PathService::CheckedGet(base::DIR_SRC_TEST_DATA_ROOT);
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
    EXPECT_TRUE(SendCommandSync(
        "PWA.install",
        base::Value::Dict{}.Set("manifestId",
                                InstallableWebAppManifestId().spec())));
    EXPECT_TRUE(AppExists(InstallableWebAppManifestId()));
  }

  void InstallFromUrl(const ManifestId& manifest_id, const GURL& url) {
    EXPECT_TRUE(SendCommandSync("PWA.install",
                                base::Value::Dict{}
                                    .Set("manifestId", manifest_id.spec())
                                    .Set("installUrlOrBundleUrl", url.spec())));
    EXPECT_TRUE(AppExists(manifest_id));
  }

  void InstallFromMatchingUrlAndManifestId(
      const GURL& install_url_and_manifest_id) {
    InstallFromUrl(install_url_and_manifest_id, install_url_and_manifest_id);
  }

  void InstallFromUrl() {
    InstallFromUrl(InstallableWebAppManifestId(), InstallableWebAppUrl());
  }

  static GURL UpperCaseScheme(const GURL& origin) {
    std::string spec{origin.spec()};
    for (size_t i = 0; i < origin.scheme().length(); i++) {
      spec[i] = base::ToUpperASCII(spec[i]);
    }
    return GURL{spec};
  }

  void AssertActiveWebContentsBelongToApp(const GURL& url,
                                          const webapps::AppId& app_id) {
    content::WebContents* contents =
        chrome::FindLastActive()->tab_strip_model()->GetActiveWebContents();
    EXPECT_TRUE(contents);
    EXPECT_TRUE(content::WaitForLoadStop(contents));
    EXPECT_EQ(contents->GetLastCommittedURL(), url);
    const std::string* contents_app_id =
        web_app::WebAppTabHelper::GetAppId(contents);
    ASSERT_TRUE(contents_app_id);
    EXPECT_EQ(*contents_app_id, app_id);
  }

  void AssertActiveWebContentsBelongToApp(const ManifestId& manifest_id) {
    AssertActiveWebContentsBelongToApp(
        manifest_id, web_app::GenerateAppIdFromManifestId(manifest_id));
  }

  base::Value::List AbsolutePaths(std::initializer_list<std::string> paths) {
    base::Value::List result{};
    for (const auto& path : paths) {
      result.Append(
          test_data_path_.Append(FILE_PATH_LITERAL("chrome/test/data"))
              .AppendASCII(path)
              .AsUTF8Unsafe());
    }
    return result;
  }

  bool AttachToLaunchFilesInAppResult() {
    const base::Value::List* ids = result()->FindList("targetIds");
    if (ids == nullptr || ids->size() != 1 || !ids->front().is_string()) {
      return false;
    }
    return SendCommandSync(
        "Target.attachToTarget",
        base::Value::Dict{}.Set("targetId", ids->front().GetString()));
  }

  using AppUserSettings =
      std::tuple<web_app::proto::LinkCapturingUserPreference,
                 web_app::mojom::UserDisplayMode>;

  AppUserSettings GetAppUserSettings(const ManifestId& manifest_id) {
    auto* provider = WebAppProvider::GetForTest(browser()->profile());
    CHECK(provider);
    const auto* web_app = provider->registrar_unsafe().GetAppById(
        web_app::GenerateAppIdFromManifestId(manifest_id));
    CHECK(web_app);
    return {web_app->user_link_capturing_preference(),
            web_app->user_display_mode()};
  }

 private:
  net::test_server::EmbeddedTestServerHandle test_server_closer_;
  std::unique_ptr<web_app::OsIntegrationTestOverrideImpl::BlockingRegistration>
      override_registration_;
  base::FilePath test_data_path_;
};

IN_PROC_BROWSER_TEST_F(PWAProtocolTestWithoutApp, GetOsAppState_CannotFindApp) {
  ASSERT_FALSE(SendCommandSync(
      "PWA.getOsAppState",
      base::Value::Dict{}.Set("manifestId", "ThisIsNotAValidManifestId")));
  // Expect the input manifestId to be carried over by the error message.
  ASSERT_TRUE(ErrorMessageContains({"ThisIsNotAValidManifestId"}));
}

IN_PROC_BROWSER_TEST_F(PWAProtocolTest, GetOsAppState) {
  InstallWebApp();
  const base::Value::Dict* result =
      SendCommandSync("PWA.getOsAppState",
                      base::Value::Dict{}.Set(
                          "manifestId", InstallableWebAppManifestId().spec()));
  ASSERT_TRUE(result);
  ASSERT_EQ(*result->FindInt("badgeCount"), 0);
  ASSERT_TRUE(result->FindList("fileHandlers")->empty());
}

IN_PROC_BROWSER_TEST_F(PWAProtocolTest, GetOsAppState_WithBadge) {
  webapps::AppId app_id = InstallWebApp();
  ukm::TestUkmRecorder test_recorder;
  badging::BadgeManagerFactory::GetForProfile(browser()->profile())
      ->SetBadgeForTesting(app_id, 11, &test_recorder);
  const base::Value::Dict* result =
      SendCommandSync("PWA.getOsAppState",
                      base::Value::Dict{}.Set(
                          "manifestId", InstallableWebAppManifestId().spec()));
  ASSERT_TRUE(result);
  ASSERT_EQ(*result->FindInt("badgeCount"), 11);
}

IN_PROC_BROWSER_TEST_F(PWAProtocolTest, GetOsAppState_WithZeroBadge) {
  webapps::AppId app_id = InstallWebApp();
  ukm::TestUkmRecorder test_recorder;
  badging::BadgeManagerFactory::GetForProfile(browser()->profile())
      ->SetBadgeForTesting(app_id, 0, &test_recorder);
  const base::Value::Dict* result =
      SendCommandSync("PWA.getOsAppState",
                      base::Value::Dict{}.Set(
                          "manifestId", InstallableWebAppManifestId().spec()));
  ASSERT_TRUE(result);
  ASSERT_EQ(*result->FindInt("badgeCount"), 0);
}

IN_PROC_BROWSER_TEST_F(PWAProtocolTest, GetOsAppState_WithBadgeOverInt) {
  webapps::AppId app_id = InstallWebApp();
  ukm::TestUkmRecorder test_recorder;
  badging::BadgeManagerFactory::GetForProfile(browser()->profile())
      ->SetBadgeForTesting(app_id, static_cast<uint64_t>(INT_MAX) + 1,
                           &test_recorder);
  const base::Value::Dict* result =
      SendCommandSync("PWA.getOsAppState",
                      base::Value::Dict{}.Set(
                          "manifestId", InstallableWebAppManifestId().spec()));
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
  const base::Value::Dict* result =
      SendCommandSync("PWA.getOsAppState",
                      base::Value::Dict{}.Set(
                          "manifestId", InstallableWebAppManifestId().spec()));
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
  ASSERT_FALSE(SendCommandSync(
      "Page.getAppManifest",
      base::Value::Dict{}.Set("manifestId", "ThisIsNotAValidManifestId")));
  ASSERT_TRUE(ErrorMessageContains({"Page.getAppManifest"}));
  // The error message should also carry the input manifest id, but now the API
  // won't work on browser target at all.
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
  const base::Value::Dict* result =
      SendCommandSync("Page.getAppManifest",
                      base::Value::Dict{}.Set(
                          "manifestId", InstallableWebAppManifestId().spec()));
  ASSERT_TRUE(result);
  result = result->FindDict("manifest");
  ASSERT_TRUE(result);
  ASSERT_EQ(*result->FindString("id"), InstallableWebAppUrl().spec());
}

IN_PROC_BROWSER_TEST_F(PWAProtocolTest, GetProcessedManifest_IconWithNoSizes) {
  ReattachToWebContents(
      GetInstallableSiteWithManifest("icon_with_no_sizes.json"));
  const base::Value::Dict* result =
      SendCommandSync("Page.getAppManifest",
                      base::Value::Dict{}.Set(
                          "manifestId", InstallableWebAppManifestId().spec()));
  ASSERT_TRUE(result);
  auto* manifest = result->FindDict("manifest");
  ASSERT_TRUE(manifest);
  auto* icons = manifest->FindList("icons");
  ASSERT_TRUE(icons);
  ASSERT_EQ(icons->size(), 1u);
  auto* icon = (*icons)[0].GetIfDict();
  ASSERT_TRUE(icon);
  auto* sizes = icon->FindString("sizes");
  ASSERT_TRUE(sizes);
  EXPECT_EQ(*sizes, "");
}

IN_PROC_BROWSER_TEST_F(PWAProtocolTest, GetProcessedManifest_MismatchId) {
  ReattachToWebContents(InstallableWebAppUrl());
  ASSERT_FALSE(SendCommandSync(
      "Page.getAppManifest",
      base::Value::Dict{}.Set("manifestId", "ThisIsNotAValidManifestId")));
  // Expect the input manifest id and original manifest id to be carried over by
  // the error message.
  ASSERT_TRUE(ErrorMessageContains(
      {InstallableWebAppUrl().spec(), "ThisIsNotAValidManifestId"}));
}

IN_PROC_BROWSER_TEST_F(PWAProtocolTest,
                       GetProcessedManifest_NotOnPage_WithManifestId) {
  ASSERT_FALSE(
      SendCommandSync("Page.getAppManifest",
                      base::Value::Dict{}.Set(
                          "manifestId", InstallableWebAppManifestId().spec())));
  ASSERT_TRUE(ErrorMessageContains({"Page.getAppManifest"}));
  // The error message should also carry the input manifest id, but now the API
  // won't work on browser target at all.
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

  SendCommandSync("PWA.uninstall",
                  base::Value::Dict{}.Set(
                      "manifestId", InstallableWebAppManifestId().spec()));
  ASSERT_TRUE(result());
  ASSERT_FALSE(error());

  ASSERT_FALSE(AppExists(InstallableWebAppManifestId()));
}

IN_PROC_BROWSER_TEST_F(PWAProtocolTest, Uninstall_CannotFindApp) {
  ASSERT_FALSE(AppExists(InstallableWebAppManifestId()));
  // Treat uninstalling nonexisting apps as a success.
  ASSERT_TRUE(
      SendCommandSync("PWA.uninstall",
                      base::Value::Dict{}.Set(
                          "manifestId", InstallableWebAppManifestId().spec())));
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
  ASSERT_FALSE(SendCommandSync(
      "PWA.install", base::Value::Dict{}.Set(
                         "manifestId", InstallableWebAppManifestId().spec())));
  ASSERT_TRUE(ErrorMessageContains({InstallableWebAppUrl().spec()}));
  ASSERT_FALSE(AppExists(InstallableWebAppManifestId()));
}

IN_PROC_BROWSER_TEST_F(PWAProtocolTest, Install_FromManifest_InvalidStartUrl) {
  const GURL url{GetInstallableSiteWithManifest("invalid_start_url.json")};
  ReattachToWebContents(url);
  ASSERT_FALSE(SendCommandSync(
      "PWA.install", base::Value::Dict{}.Set("manifestId", url.spec())));
  ASSERT_TRUE(ErrorMessageContains({url.spec()}));
  ASSERT_FALSE(AppExists(url));
  ASSERT_FALSE(AppExists(ManifestId{"http://different.origin/is-invalid"}));
}

IN_PROC_BROWSER_TEST_F(PWAProtocolTest,
                       Install_FromManifest_InconsistentAppId) {
  const GURL url{GetInstallableSiteWithManifest("basic.json")};
  ReattachToWebContents(url);
  ASSERT_FALSE(SendCommandSync(
      "PWA.install", base::Value::Dict{}.Set("manifestId", url.spec())));
  ASSERT_TRUE(ErrorMessageContains(
      {url.spec(), GetInstallableSiteWithManifest("basic.json").spec()}));
  ASSERT_FALSE(AppExists(url));
  ASSERT_FALSE(AppExists(InstallableWebAppManifestId()));
}

IN_PROC_BROWSER_TEST_F(PWAProtocolTest, Install_FromManifest_HasManifestId) {
  ReattachToWebContents(HasManifestIdWebAppUrl());
  ASSERT_TRUE(SendCommandSync(
      "PWA.install",
      base::Value::Dict{}.Set("manifestId",
                              HasManifestIdWebAppManifestId().spec())));
  ASSERT_TRUE(AppExists(HasManifestIdWebAppManifestId()));
  ASSERT_FALSE(AppExists(HasManifestIdWebAppUrl()));
}

IN_PROC_BROWSER_TEST_F(PWAProtocolTest,
                       Install_FromManifest_HasManifestId_UrlAsManifestId) {
  ReattachToWebContents(HasManifestIdWebAppUrl());
  ASSERT_FALSE(SendCommandSync(
      "PWA.install",
      base::Value::Dict{}.Set("manifestId", HasManifestIdWebAppUrl().spec())));
  ASSERT_TRUE(ErrorMessageContains({HasManifestIdWebAppUrl().spec()}));
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
  ASSERT_TRUE(SendCommandSync(
      "PWA.install",
      base::Value::Dict{}
          .Set("manifestId",
               UpperCaseScheme(InstallableWebAppManifestId()).spec())
          .Set("installUrlOrBundleUrl",
               UpperCaseScheme(InstallableWebAppUrl()).spec())));
  ASSERT_TRUE(AppExists(InstallableWebAppManifestId()));
}

IN_PROC_BROWSER_TEST_F(PWAProtocolTest, Install_FromUrl_Unreachable) {
  ASSERT_FALSE(SendCommandSync(
      "PWA.install",
      base::Value::Dict{}
          .Set("manifestId", InstallableWebAppManifestId().spec())
          .Set("installUrlOrBundleUrl", "http://hello/this/is/not/existing")));
  ASSERT_TRUE(ErrorMessageContains(
      {InstallableWebAppUrl().spec(), "http://hello/this/is/not/existing"}));
  ASSERT_FALSE(AppExists(InstallableWebAppManifestId()));
}

IN_PROC_BROWSER_TEST_F(PWAProtocolTest, Install_FromUrl_UnmatchManifestId) {
  ASSERT_FALSE(SendCommandSync(
      "PWA.install",
      base::Value::Dict{}
          .Set("manifestId", NotInstallableWebAppUrl().spec())
          .Set("installUrlOrBundleUrl", InstallableWebAppUrl().spec())));
  ASSERT_TRUE(ErrorMessageContains(
      {InstallableWebAppUrl().spec(), NotInstallableWebAppUrl().spec()}));
  ASSERT_FALSE(AppExists(InstallableWebAppManifestId()));
  ASSERT_FALSE(AppExists(NotInstallableWebAppManifestId()));
}

// TODO(crbug.com/331214986): May want a test to trigger the installation
// failure when installing from the url.

IN_PROC_BROWSER_TEST_F(PWAProtocolTest,
                       Install_FromUrl_ValidManifestId_DifferentInstallUrl) {
  const GURL url{GetInstallableSiteWithManifest("basic.json")};
  ASSERT_TRUE(SendCommandSync(
      "PWA.install",
      base::Value::Dict{}
          .Set("manifestId", InstallableWebAppManifestId().spec())
          .Set("installUrlOrBundleUrl", url.spec())));
  ASSERT_FALSE(AppExists(url));
  ASSERT_TRUE(AppExists(InstallableWebAppManifestId()));
}

IN_PROC_BROWSER_TEST_F(PWAProtocolTest, Install_FromUrl_InconsistentAppId) {
  const GURL url{GetInstallableSiteWithManifest("basic.json")};
  ASSERT_FALSE(SendCommandSync(
      "PWA.install",
      base::Value::Dict{}
          .Set("manifestId", url.spec())
          .Set("installUrlOrBundleUrl", InstallableWebAppUrl().spec())));
  ASSERT_TRUE(ErrorMessageContains({url.spec()}));
  ASSERT_FALSE(AppExists(url));
  ASSERT_FALSE(AppExists(InstallableWebAppManifestId()));
}

IN_PROC_BROWSER_TEST_F(PWAProtocolTest, Install_FromUrl_NoScheme) {
  ASSERT_FALSE(SendCommandSync(
      "PWA.install",
      base::Value::Dict{}
          .Set("manifestId", InstallableWebAppManifestId().spec())
          .Set("installUrlOrBundleUrl", "localhost/")));
  ASSERT_TRUE(ErrorMessageContains({"localhost/"}));
  ASSERT_FALSE(AppExists(InstallableWebAppManifestId()));
}

IN_PROC_BROWSER_TEST_F(PWAProtocolTest, Install_FromUrl_UnsupportedScheme) {
  ASSERT_FALSE(SendCommandSync(
      "PWA.install",
      base::Value::Dict{}
          .Set("manifestId", InstallableWebAppManifestId().spec())
          .Set("installUrlOrBundleUrl", "ftp://localhost/")));
  ASSERT_TRUE(ErrorMessageContains({"ftp", "ftp://localhost/"}));
  ASSERT_FALSE(AppExists(InstallableWebAppManifestId()));
}

IN_PROC_BROWSER_TEST_F(PWAProtocolTest, Install_FromUrl_HasManifestId) {
  ASSERT_TRUE(SendCommandSync(
      "PWA.install",
      base::Value::Dict{}
          .Set("manifestId", HasManifestIdWebAppManifestId().spec())
          .Set("installUrlOrBundleUrl", HasManifestIdWebAppUrl().spec())));
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

IN_PROC_BROWSER_TEST_F(PWAProtocolTest, Launch) {
  InstallFromUrl();
  ASSERT_TRUE(SendCommandSync(
      "PWA.launch", base::Value::Dict{}.Set(
                        "manifestId", InstallableWebAppManifestId().spec())));
  ASSERT_FALSE(error());
  AssertActiveWebContentsBelongToApp(InstallableWebAppUrl());
}

IN_PROC_BROWSER_TEST_F(PWAProtocolTest, Launch_ReturnsAttachableTargetId) {
  InstallFromUrl();
  const base::Value::Dict* result = SendCommandSync(
      "PWA.launch", base::Value::Dict{}.Set(
                        "manifestId", InstallableWebAppManifestId().spec()));
  ASSERT_TRUE(result);
  ASSERT_TRUE(SendCommandSync(
      "Target.attachToTarget",
      base::Value::Dict{}.Set("targetId", *result->FindString("targetId"))));
}

IN_PROC_BROWSER_TEST_F(PWAProtocolTest, Launch_AutoAttach) {
  InstallFromUrl();
  ASSERT_TRUE(
      SendCommandSync("Target.setAutoAttach",
                      base::Value::Dict{}
                          .Set("autoAttach", true)
                          .Set("waitForDebuggerOnStart", true)
                          .Set("filter", base::Value::List{}
                                             .Append(base::Value::Dict{}
                                                         .Set("type", "tab")
                                                         .Set("exclude", false))
                                             .Append(base::Value::Dict{}
                                                         .Set("type", "page")
                                                         .Set("exclude", true)))
                          .Set("flatten", true)));
  ASSERT_TRUE(SendCommandSync(
      "PWA.launch", base::Value::Dict{}.Set(
                        "manifestId", InstallableWebAppManifestId().spec())));
  ASSERT_TRUE(HasExistingNotificationMatching(
      [expected_target_id = *result()->FindString("targetId")](
          const base::Value::Dict& notification) {
        if (*notification.FindString("method") != "Target.attachedToTarget") {
          return false;
        }
        const std::string* target_id =
            notification.FindStringByDottedPath("params.targetInfo.targetId");
        const std::string* type =
            notification.FindStringByDottedPath("params.targetInfo.type");
        return target_id && *target_id == expected_target_id && type &&
               *type == content::DevToolsAgentHost::kTypeTab;
      }));
}

IN_PROC_BROWSER_TEST_F(PWAProtocolTest, Launch_NoApp) {
  ASSERT_FALSE(SendCommandSync(
      "PWA.launch", base::Value::Dict{}.Set(
                        "manifestId", InstallableWebAppManifestId().spec())));
  ASSERT_TRUE(ErrorMessageContains({InstallableWebAppManifestId().spec()}));
}

IN_PROC_BROWSER_TEST_F(PWAProtocolTest, Launch_InFullScreenMode) {
  GURL url{embedded_test_server()->GetURL("/web_apps/display_fullscreen.html")};
  InstallFromMatchingUrlAndManifestId(url);
  ASSERT_TRUE(SendCommandSync(
      "PWA.launch", base::Value::Dict{}.Set("manifestId", url.spec())));
  ASSERT_FALSE(error());
  AssertActiveWebContentsBelongToApp(url);
}

IN_PROC_BROWSER_TEST_F(PWAProtocolTest, Launch_FromUrl) {
  InstallFromUrl();
  ASSERT_TRUE(SendCommandSync(
      "PWA.launch", base::Value::Dict{}
                        .Set("manifestId", InstallableWebAppManifestId().spec())
                        .Set("url", InstallableWebAppUrl().spec())));
  ASSERT_FALSE(error());
  AssertActiveWebContentsBelongToApp(InstallableWebAppUrl());
}

IN_PROC_BROWSER_TEST_F(PWAProtocolTest, Launch_FromUrl_InScope) {
  InstallFromUrl();
  GURL url{
      embedded_test_server()->GetURL("/web_apps/different_start_url.html")};
  ASSERT_TRUE(SendCommandSync(
      "PWA.launch", base::Value::Dict{}
                        .Set("manifestId", InstallableWebAppManifestId().spec())
                        .Set("url", url.spec())));
  ASSERT_FALSE(error());
  AssertActiveWebContentsBelongToApp(
      url, web_app::GenerateAppIdFromManifestId(InstallableWebAppManifestId()));
}

IN_PROC_BROWSER_TEST_F(PWAProtocolTest, Launch_FromUrl_NoApp) {
  ASSERT_FALSE(SendCommandSync(
      "PWA.launch", base::Value::Dict{}
                        .Set("manifestId", InstallableWebAppManifestId().spec())
                        .Set("url", InstallableWebAppUrl().spec())));
  ASSERT_TRUE(ErrorMessageContains(
      {InstallableWebAppManifestId().spec(), InstallableWebAppUrl().spec()}));
}

IN_PROC_BROWSER_TEST_F(PWAProtocolTest, Launch_FromUrl_InvalidUrl) {
  InstallFromUrl();
  ASSERT_FALSE(SendCommandSync(
      "PWA.launch", base::Value::Dict{}
                        .Set("manifestId", InstallableWebAppManifestId().spec())
                        .Set("url", "invalid-url@@@invalid/url")));
  ASSERT_TRUE(ErrorMessageContains({"invalid-url@@@invalid/url"}));
}

IN_PROC_BROWSER_TEST_F(PWAProtocolTest, Launch_FromUrl_OutOfScopeUrl) {
  InstallFromUrl();
  ASSERT_FALSE(SendCommandSync(
      "PWA.launch", base::Value::Dict{}
                        .Set("manifestId", InstallableWebAppManifestId().spec())
                        .Set("url", "https://www.google.com/")));
  ASSERT_TRUE(ErrorMessageContains({"https://www.google.com/"}));
}

IN_PROC_BROWSER_TEST_F(PWAProtocolTest, LaunchFilesInApp) {
  GURL url{embedded_test_server()->GetURL("/web_apps/file_handler_index.html")};
  InstallFromMatchingUrlAndManifestId(url);
  ASSERT_TRUE(
      SendCommandSync("PWA.launchFilesInApp",
                      base::Value::Dict{}
                          .Set("manifestId", url.spec())
                          .Set("files", AbsolutePaths({"cors-ok.txt"}))));
  ASSERT_TRUE(AttachToLaunchFilesInAppResult());
  AssertActiveWebContentsBelongToApp(
      embedded_test_server()->GetURL("/web_apps/file_handler_action.html"),
      web_app::GenerateAppIdFromManifestId(url));
}

IN_PROC_BROWSER_TEST_F(PWAProtocolTest, LaunchFilesInApp_MultipleFiles) {
  GURL url{embedded_test_server()->GetURL("/web_apps/file_handler_index.html")};
  InstallFromMatchingUrlAndManifestId(url);
  ASSERT_TRUE(SendCommandSync(
      "PWA.launchFilesInApp",
      base::Value::Dict{}
          .Set("manifestId", url.spec())
          .Set("files",
               AbsolutePaths({"cors-ok.txt", "download-autoopen.txt"}))));
  ASSERT_TRUE(AttachToLaunchFilesInAppResult());
  AssertActiveWebContentsBelongToApp(
      embedded_test_server()->GetURL("/web_apps/file_handler_action.html"),
      web_app::GenerateAppIdFromManifestId(url));
}

IN_PROC_BROWSER_TEST_F(PWAProtocolTest, LaunchFilesInApp_RepeatedFiles) {
  GURL url{embedded_test_server()->GetURL("/web_apps/file_handler_index.html")};
  InstallFromMatchingUrlAndManifestId(url);
  ASSERT_TRUE(SendCommandSync(
      "PWA.launchFilesInApp",
      base::Value::Dict{}
          .Set("manifestId", url.spec())
          .Set("files", AbsolutePaths({"cors-ok.txt", "cors-ok.txt"}))));
  ASSERT_TRUE(AttachToLaunchFilesInAppResult());
  AssertActiveWebContentsBelongToApp(
      embedded_test_server()->GetURL("/web_apps/file_handler_action.html"),
      web_app::GenerateAppIdFromManifestId(url));
}

IN_PROC_BROWSER_TEST_F(PWAProtocolTest, LaunchFilesInApp_MultipleTypes) {
  GURL url{embedded_test_server()->GetURL(
      "/webapps_integration/file_handler/basic.html")};
  InstallFromMatchingUrlAndManifestId(url);
  ASSERT_TRUE(SendCommandSync(
      "PWA.launchFilesInApp",
      base::Value::Dict{}
          .Set("manifestId", url.spec())
          .Set("files",
               AbsolutePaths({"cors-ok.txt", "web_apps/basic-192.png"}))));
  ASSERT_TRUE(result()->FindList("targetIds")->size() == 2);
}

IN_PROC_BROWSER_TEST_F(PWAProtocolTest,
                       LaunchFilesInApp_OneTypeMultipleFilesPerRequest) {
  GURL url{embedded_test_server()->GetURL(
      "/webapps_integration/file_handler/basic.html")};
  InstallFromMatchingUrlAndManifestId(url);
  ASSERT_TRUE(SendCommandSync(
      "PWA.launchFilesInApp",
      base::Value::Dict{}
          .Set("manifestId", url.spec())
          .Set("files",
               AbsolutePaths({"cors-ok.txt", "download-autoopen.txt"}))));
  ASSERT_TRUE(AttachToLaunchFilesInAppResult());
  // Multiple-Clients launch type should open one page per file.
  ASSERT_TRUE(SendCommandSync(
      "PWA.launchFilesInApp",
      base::Value::Dict{}
          .Set("manifestId", url.spec())
          .Set("files", AbsolutePaths({"web_apps/basic-192.png",
                                       "web_apps/basic-48.png"}))));
  ASSERT_TRUE(result()->FindList("targetIds")->size() == 2);
}

IN_PROC_BROWSER_TEST_F(PWAProtocolTest,
                       LaunchFilesInApp_MultipleTypesMultipleFiles) {
  GURL url{embedded_test_server()->GetURL(
      "/webapps_integration/file_handler/basic.html")};
  InstallFromMatchingUrlAndManifestId(url);
  ASSERT_TRUE(SendCommandSync(
      "PWA.launchFilesInApp",
      base::Value::Dict{}
          .Set("manifestId", url.spec())
          .Set("files",
               AbsolutePaths({"web_apps/basic-192.png", "web_apps/basic-48.png",
                              "cors-ok.txt", "download-autoopen.txt"}))));
  ASSERT_TRUE(result()->FindList("targetIds")->size() == 3);
}

IN_PROC_BROWSER_TEST_F(PWAProtocolTest, LaunchFilesInApp_PartiallyUnsupported) {
  GURL url{embedded_test_server()->GetURL(
      "/webapps_integration/file_handler/basic.html")};
  InstallFromMatchingUrlAndManifestId(url);
  ASSERT_TRUE(SendCommandSync(
      "PWA.launchFilesInApp",
      base::Value::Dict{}
          .Set("manifestId", url.spec())
          .Set("files",
               AbsolutePaths({"web_apps/basic-192.png", "web_apps/basic-48.png",
                              "web_apps/basic.html", "cors-ok.txt",
                              "download-autoopen.txt"}))));
  ASSERT_TRUE(result()->FindList("targetIds")->size() == 3);
}

// TODO(crbug.com/331214986): May want a test to trigger the LaunchFilesInApp
// failure in LaunchApp callback.

IN_PROC_BROWSER_TEST_F(PWAProtocolTest, LaunchFilesInApp_PartiallyFailed) {
  GURL url{embedded_test_server()->GetURL(
      "/webapps_integration/file_handler/basic.html")};
  InstallFromMatchingUrlAndManifestId(url);
  // The action for "jpg" is out of scope, and should be ignored.
  ASSERT_TRUE(SendCommandSync(
      "PWA.launchFilesInApp",
      base::Value::Dict{}
          .Set("manifestId", url.spec())
          .Set("files",
               AbsolutePaths({"web_apps/basic-192.png", "web_apps/basic-48.png",
                              "web_apps/basic.html", "cors-ok.txt",
                              "android/watch.jpg", "download-autoopen.txt"}))));
  ASSERT_TRUE(result()->FindList("targetIds")->size() == 3);
}

// The existence of the files should be checked somewhere, but now the launch
// app command still succeed. So this test does not check the return value of
// the command, but only ensure it won't crash the browser.
IN_PROC_BROWSER_TEST_F(PWAProtocolTest, LaunchFilesInApp_NonexistentFiles) {
  GURL url{embedded_test_server()->GetURL(
      "/webapps_integration/file_handler/basic.html")};
  InstallFromMatchingUrlAndManifestId(url);
  SendCommandSync("PWA.launchFilesInApp",
                  base::Value::Dict{}
                      .Set("manifestId", url.spec())
                      .Set("files", "/hey/this/file/should/not/exist.txt"));
}

IN_PROC_BROWSER_TEST_F(PWAProtocolTest, LaunchFilesInApp_NoFileHandlers) {
  InstallFromUrl();
  ASSERT_FALSE(SendCommandSync(
      "PWA.launchFilesInApp",
      base::Value::Dict{}
          .Set("manifestId", InstallableWebAppManifestId().spec())
          .Set("files", AbsolutePaths({"cors-ok.txt"}))));
  ASSERT_TRUE(ErrorMessageContains({InstallableWebAppManifestId().spec()}));
}

// This scenario does not reach the handler itself, but it's worth ensuring that
// running PWA.launchFilesInApp without "files" parameter would always fail.
IN_PROC_BROWSER_TEST_F(PWAProtocolTest, LaunchFilesInApp_NoFilesField) {
  GURL url{embedded_test_server()->GetURL(
      "/webapps_integration/file_handler/basic.html")};
  InstallFromMatchingUrlAndManifestId(url);
  ASSERT_FALSE(
      SendCommandSync("PWA.launchFilesInApp",
                      base::Value::Dict{}.Set("manifestId", url.spec())));
  ASSERT_TRUE(error());
}

IN_PROC_BROWSER_TEST_F(PWAProtocolTest, LaunchFilesInApp_NoFile) {
  GURL url{embedded_test_server()->GetURL(
      "/webapps_integration/file_handler/basic.html")};
  InstallFromMatchingUrlAndManifestId(url);
  ASSERT_FALSE(SendCommandSync("PWA.launchFilesInApp",
                               base::Value::Dict{}
                                   .Set("manifestId", url.spec())
                                   .Set("files", base::Value::List{})));
  ASSERT_TRUE(ErrorMessageContains({url.spec()}));
}

IN_PROC_BROWSER_TEST_F(PWAProtocolTest, LaunchFilesInApp_UnsupportedFile) {
  GURL url{embedded_test_server()->GetURL("/web_apps/file_handler_index.html")};
  InstallFromMatchingUrlAndManifestId(url);
  ASSERT_FALSE(SendCommandSync("PWA.launchFilesInApp",
                               base::Value::Dict{}
                                   .Set("manifestId", url.spec())
                                   .Set("files", AbsolutePaths({"file.png"}))));
  ASSERT_TRUE(ErrorMessageContains({url.spec()}));
}

IN_PROC_BROWSER_TEST_F(PWAProtocolTest, OpenCurrentPageInApp) {
  // The current browser will be taken over by the web app and the
  // uninstallation will also close it.
  set_agent_host_can_close();
  InstallFromUrl();
  ReattachToWebContents(InstallableWebAppUrl());
  ASSERT_TRUE(
      SendCommandSync("PWA.openCurrentPageInApp",
                      base::Value::Dict{}.Set(
                          "manifestId", InstallableWebAppManifestId().spec())));
}

IN_PROC_BROWSER_TEST_F(PWAProtocolTest, OpenCurrentPageInApp_NoWebContents) {
  InstallFromUrl();
  ASSERT_FALSE(
      SendCommandSync("PWA.openCurrentPageInApp",
                      base::Value::Dict{}.Set(
                          "manifestId", InstallableWebAppManifestId().spec())));
  ASSERT_TRUE(ErrorMessageContains({InstallableWebAppManifestId().spec()}));
}

IN_PROC_BROWSER_TEST_F(PWAProtocolTest, OpenCurrentPageInApp_NotInstalled) {
  ReattachToWebContents(InstallableWebAppUrl());
  ASSERT_FALSE(
      SendCommandSync("PWA.openCurrentPageInApp",
                      base::Value::Dict{}.Set(
                          "manifestId", InstallableWebAppManifestId().spec())));
  ASSERT_TRUE(ErrorMessageContains({InstallableWebAppManifestId().spec()}));
}

IN_PROC_BROWSER_TEST_F(PWAProtocolTest,
                       OpenCurrentPageInApp_DifferentManifestIdUrl) {
  set_agent_host_can_close();
  ManifestId manifest_id{embedded_test_server()->GetURL(
      "/webapps_integration/standalone/basic.html")};
  GURL url{embedded_test_server()->GetURL(
      "/webapps_integration/standalone/basic.html?manifest=basic.json")};
  InstallFromUrl(manifest_id, url);
  ReattachToWebContents(url);
  ASSERT_TRUE(SendCommandSync(
      "PWA.openCurrentPageInApp",
      base::Value::Dict{}.Set("manifestId", manifest_id.spec())));
}

// This test should fail since web apps with browser display mode shouldn't be
// reparentable to match the behavior of post-installation reparenting. But now
// the check only applies in WebAppInstallFinalizer::CanReparentTab.
// TODO(crbug.com/339453521): Find a proper way to check the display mode.
IN_PROC_BROWSER_TEST_F(PWAProtocolTest,
                       DISABLED_OpenCurrentPageInApp_NotReparentable) {
  ManifestId manifest_id{embedded_test_server()->GetURL(
      "/webapps_integration/standalone/basic.html")};
  GURL url{embedded_test_server()->GetURL(
      "/webapps_integration/standalone/"
      "basic.html?manifest=manifest_browser.json")};
  InstallFromUrl(manifest_id, url);
  ReattachToWebContents(url);
  ASSERT_FALSE(SendCommandSync(
      "PWA.openCurrentPageInApp",
      base::Value::Dict{}.Set("manifestId", manifest_id.spec())));
  ASSERT_TRUE(ErrorMessageContains({manifest_id.spec(), url.spec()}));
}

IN_PROC_BROWSER_TEST_F(PWAProtocolTest,
                       OpenCurrentPageInApp_StillAttachedInNewAppWindow) {
  set_agent_host_can_close();
  InstallFromUrl();
  ReattachToWebContents(InstallableWebAppUrl());
  auto* web_contents_before =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(
      SendCommandSync("PWA.openCurrentPageInApp",
                      base::Value::Dict{}.Set(
                          "manifestId", InstallableWebAppManifestId().spec())));

  const webapps::AppId app_id =
      web_app::GenerateAppIdFromManifestId(InstallableWebAppManifestId());
  Browser* app_browser = web_app::AppBrowserController::FindForWebApp(
      *browser()->profile(), app_id);
  auto* web_contents_after =
      app_browser->tab_strip_model()->GetActiveWebContents();
  EXPECT_NE(app_browser, browser());
  EXPECT_EQ(web_contents_before, web_contents_after);
  // Use a page target API to verify the WebContents is still attached.
  ASSERT_TRUE(
      SendCommandSync("Page.getAppManifest",
                      base::Value::Dict{}.Set(
                          "manifestId", InstallableWebAppManifestId().spec())));
}

#if BUILDFLAG(IS_MAC)
// Only macos needs a shortcut to open the page in app.
IN_PROC_BROWSER_TEST_F(PWAProtocolTest, OpenCurrentPageInApp_NoShortcut) {
  // Unlike the PWA.install, web_app::test::InstallWebApp won't create
  // shortcuts.
  InstallWebApp();
  ASSERT_FALSE(
      SendCommandSync("PWA.openCurrentPageInApp",
                      base::Value::Dict{}.Set(
                          "manifestId", InstallableWebAppManifestId().spec())));
  ASSERT_TRUE(ErrorMessageContains({InstallableWebAppManifestId().spec()}));
}
#endif

IN_PROC_BROWSER_TEST_F(PWAProtocolTest, ChangeAppUserSettings_ChangeNothing) {
  InstallFromUrl();
  auto user_settings_before_change =
      GetAppUserSettings(InstallableWebAppManifestId());
  ASSERT_TRUE(
      SendCommandSync("PWA.changeAppUserSettings",
                      base::Value::Dict{}.Set(
                          "manifestId", InstallableWebAppManifestId().spec())));
  EXPECT_EQ(user_settings_before_change,
            GetAppUserSettings(InstallableWebAppManifestId()));
}

#if BUILDFLAG(IS_CHROMEOS)
// Setting linkCapturing on ChromeOS is not supported yet.
// TODO(crbug.com/339453269): Implement setting linkCapturing on ChromeOS.
#define DISABLE_ON_CHROMEOS(x) DISABLED_##x
#else
#define DISABLE_ON_CHROMEOS(x) x
#endif

IN_PROC_BROWSER_TEST_F(PWAProtocolTest,
                       DISABLE_ON_CHROMEOS(ChangeAppUserSettings_NoApp)) {
  ASSERT_FALSE(SendCommandSync(
      "PWA.changeAppUserSettings",
      base::Value::Dict{}
          .Set("manifestId", InstallableWebAppManifestId().spec())
          .Set("linkCapturing", true)
          .Set("displayMode", "standalone")));
  EXPECT_TRUE(ErrorMessageContains({InstallableWebAppManifestId().spec()}));
}

// Unlike the NoApp test above, even without changes, the API should check the
// existence of the app and returns an error.
IN_PROC_BROWSER_TEST_F(PWAProtocolTest, ChangeAppUserSettings_NoAppNoChange) {
  ASSERT_FALSE(
      SendCommandSync("PWA.changeAppUserSettings",
                      base::Value::Dict{}.Set(
                          "manifestId", InstallableWebAppManifestId().spec())));
  EXPECT_TRUE(ErrorMessageContains({InstallableWebAppManifestId().spec()}));
}

IN_PROC_BROWSER_TEST_F(
    PWAProtocolTest,
    DISABLE_ON_CHROMEOS(
        ChangeAppUserSettings_ChangeLinkCapturingAndDisplayMode)) {
  InstallFromUrl();
  ASSERT_TRUE(SendCommandSync(
      "PWA.changeAppUserSettings",
      base::Value::Dict{}
          .Set("manifestId", InstallableWebAppManifestId().spec())
          .Set("linkCapturing", true)
          .Set("displayMode", "standalone")));
  EXPECT_EQ(
      GetAppUserSettings(InstallableWebAppManifestId()),
      std::make_tuple(
          web_app::proto::LinkCapturingUserPreference::CAPTURE_SUPPORTED_LINKS,
          web_app::mojom::UserDisplayMode::kStandalone));
}

IN_PROC_BROWSER_TEST_F(PWAProtocolTest,
                       ChangeAppUserSettings_ChangeToStandaloneDisplayMode) {
  InstallFromUrl();
  ASSERT_TRUE(SendCommandSync(
      "PWA.changeAppUserSettings",
      base::Value::Dict{}
          .Set("manifestId", InstallableWebAppManifestId().spec())
          .Set("displayMode", "standalone")));
  EXPECT_EQ(std::get<web_app::mojom::UserDisplayMode>(
                GetAppUserSettings(InstallableWebAppManifestId())),
            web_app::mojom::UserDisplayMode::kStandalone);
}

// Even though supporting on ChromeOS hasn't been implemented, it should not
// crash.
IN_PROC_BROWSER_TEST_F(PWAProtocolTest, ChangeAppUserSettings_NotCrash) {
  InstallFromUrl();
  SendCommandSync("PWA.changeAppUserSettings",
                  base::Value::Dict{}
                      .Set("manifestId", InstallableWebAppManifestId().spec())
                      .Set("linkCapturing", true)
                      .Set("displayMode", "standalone"));
}

IN_PROC_BROWSER_TEST_F(PWAProtocolTest,
                       ChangeAppUserSettings_UnknownDisplayMode) {
  InstallFromUrl();
  auto user_settings_before_change =
      GetAppUserSettings(InstallableWebAppManifestId());
  ASSERT_FALSE(SendCommandSync(
      "PWA.changeAppUserSettings",
      base::Value::Dict{}
          .Set("manifestId", InstallableWebAppManifestId().spec())
          .Set("linkCapturing", true)
          .Set("displayMode", "hello")));
  EXPECT_TRUE(ErrorMessageContains(
      {InstallableWebAppManifestId().spec(), "displayMode", "hello"}));
  EXPECT_EQ(user_settings_before_change,
            GetAppUserSettings(InstallableWebAppManifestId()));
}

IN_PROC_BROWSER_TEST_F(
    PWAProtocolTest,
    DISABLE_ON_CHROMEOS(ChangeAppUserSettings_DoNotCapture)) {
  InstallFromUrl();
  ASSERT_TRUE(SendCommandSync(
      "PWA.changeAppUserSettings",
      base::Value::Dict{}
          .Set("manifestId", InstallableWebAppManifestId().spec())
          .Set("linkCapturing", false)));
  EXPECT_EQ(std::get<web_app::proto::LinkCapturingUserPreference>(
                GetAppUserSettings(InstallableWebAppManifestId())),
            web_app::proto::LinkCapturingUserPreference::
                DO_NOT_CAPTURE_SUPPORTED_LINKS);
}

// This scenario does not reach the handler itself, but it's worth ensuring that
// running PWA.changeAppUserSettings without manifestId would always fail.
// The same concept applies to other APIs, but since they are using same
// implementation, the test won't be repeated.
IN_PROC_BROWSER_TEST_F(PWAProtocolTest, ChangeAppUserSettings_NoManifestId) {
  InstallFromUrl();
  ASSERT_FALSE(
      SendCommandSync("PWA.changeAppUserSettings", base::Value::Dict{}));
  EXPECT_TRUE(error());
}

}  // namespace
