// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_install/web_app_installer.h"

#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "chrome/browser/apps/app_service/app_install/app_install_types.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "components/webapps/browser/install_result_code.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

namespace apps {

class WebAppInstallerBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    https_server_.RegisterRequestHandler(base::BindRepeating(
        &WebAppInstallerBrowserTest::HandleRequest, base::Unretained(this)));
    https_server_.AddDefaultHandlers(GetChromeTestDataDir());

    ASSERT_TRUE(https_server_.Start());

    // Icon URLs should remap to the test server.
    host_resolver()->AddRule("meltingpot.googleusercontent.com", "127.0.0.1");
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    if ((request.relative_url != "/manifest.json" &&
         request.relative_url != "/manifest2.json") ||
        manifest_.empty()) {
      return nullptr;
    }

    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(net::HTTP_OK);
    response->set_content_type("application/json");
    if (request.relative_url == "/manifest.json") {
      response->set_content(manifest_);

    } else {
      response->set_content(manifest2_);
    }
    return response;
  }

  std::string AddIconToManifest(const std::string& manifest_template) {
    GURL icon_url = https_server()->GetURL("meltingpot.googleusercontent.com",
                                           "/web_apps/blue-192.png");
    constexpr char kIconsBlock[] = R"([{
        "src": "$1",
        "sizes": "192x192",
        "type": "image/png"
      }])";
    std::string icon_value = base::ReplaceStringPlaceholders(
        kIconsBlock, {icon_url.spec()}, nullptr);
    return base::ReplaceStringPlaceholders(manifest_template, {icon_value},
                                           nullptr);
  }

  AppInstallData CreateInstallData(std::string_view name,
                                   std::string_view package_id,
                                   std::string_view original_manifest_url,
                                   std::string_view test_manifest_url) {
    AppInstallData data(PackageId::FromString(package_id).value());
    data.name = name;
    WebAppInstallData& web_app_data =
        data.app_type_data.emplace<WebAppInstallData>();
    web_app_data.original_manifest_url = GURL(original_manifest_url);
    web_app_data.proxied_manifest_url =
        https_server()->GetURL(test_manifest_url);
    web_app_data.document_url =
        web_app_data.original_manifest_url.GetWithEmptyPath();
    return data;
  }

  void VerifyAppInstalled(webapps::AppId app_id,
                          const std::string& app_name,
                          InstallReason expected_reason) {
    bool found = app_registry_cache().ForOneApp(
        app_id, [app_name, expected_reason](const AppUpdate& update) {
          EXPECT_EQ(update.Name(), app_name);
          EXPECT_EQ(update.InstallReason(), expected_reason);
        });
    ASSERT_TRUE(found);
  }

  void SetManifestResponse(std::string manifest) { manifest_ = manifest; }
  void SetManifest2Response(std::string manifest) { manifest2_ = manifest; }

  Profile* profile() { return browser()->profile(); }

  net::EmbeddedTestServer* https_server() { return &https_server_; }

  AppRegistryCache& app_registry_cache() {
    auto* proxy = AppServiceProxyFactory::GetForProfile(browser()->profile());
    return proxy->AppRegistryCache();
  }

 private:
  net::EmbeddedTestServer https_server_;
  std::string manifest_;
  std::string manifest2_;
};

IN_PROC_BROWSER_TEST_F(WebAppInstallerBrowserTest, InstallOneOemApp) {
  WebAppInstaller installer(profile());

  constexpr char kManifestTemplate[] = R"({
    "name": "Example App",
    "start_url": "/index.html",
    "scope": "/",
    "icons": $1
  })";
  SetManifestResponse(AddIconToManifest(kManifestTemplate));

  base::HistogramTester histograms;
  base::test::TestFuture<bool> result;
  installer.InstallApp(
      AppInstallSurface::kAppPreloadServiceOem,
      CreateInstallData("Example App", "web:https://www.example.com/index.html",
                        "https://www.example.com/manifest.json",
                        "/manifest.json"),
      result.GetCallback());
  ASSERT_TRUE(result.Get());

  auto app_id = web_app::GenerateAppId(
      std::nullopt, GURL("https://www.example.com/index.html"));
  VerifyAppInstalled(app_id, "Example App", InstallReason::kOem);

  histograms.ExpectBucketCount(
      "Apps.AppInstallService.WebAppInstaller.InstallResult",
      WebAppInstallResult::kSuccess, 1);
  histograms.ExpectBucketCount(
      "Apps.AppInstallService.WebAppInstaller.InstallResult."
      "AppPreloadServiceOem",
      WebAppInstallResult::kSuccess, 1);
  histograms.ExpectBucketCount(
      "Apps.AppInstallService.WebAppInstaller.CommandResultCode",
      webapps::InstallResultCode::kSuccessNewInstall, 1);
  histograms.ExpectBucketCount(
      "Apps.AppInstallService.WebAppInstaller.CommandResultCode."
      "AppPreloadServiceOem",
      webapps::InstallResultCode::kSuccessNewInstall, 1);
}

IN_PROC_BROWSER_TEST_F(WebAppInstallerBrowserTest, InstallOneDefaultApp) {
  WebAppInstaller installer(profile());

  constexpr char kManifestTemplate[] = R"({
    "name": "Example App",
    "start_url": "/index.html",
    "scope": "/",
    "icons": $1
  })";
  SetManifestResponse(AddIconToManifest(kManifestTemplate));

  base::HistogramTester histograms;
  base::test::TestFuture<bool> result;
  installer.InstallApp(
      AppInstallSurface::kAppPreloadServiceDefault,
      CreateInstallData("Example App", "web:https://www.example.com/index.html",
                        "https://www.example.com/manifest.json",
                        "/manifest.json"),
      result.GetCallback());
  ASSERT_TRUE(result.Get());

  auto app_id = web_app::GenerateAppId(
      std::nullopt, GURL("https://www.example.com/index.html"));
  VerifyAppInstalled(app_id, "Example App", InstallReason::kDefault);

  histograms.ExpectBucketCount(
      "Apps.AppInstallService.WebAppInstaller.InstallResult",
      WebAppInstallResult::kSuccess, 1);
  histograms.ExpectBucketCount(
      "Apps.AppInstallService.WebAppInstaller.InstallResult."
      "AppPreloadServiceDefault",
      WebAppInstallResult::kSuccess, 1);
  histograms.ExpectBucketCount(
      "Apps.AppInstallService.WebAppInstaller.CommandResultCode",
      webapps::InstallResultCode::kSuccessNewInstall, 1);
  histograms.ExpectBucketCount(
      "Apps.AppInstallService.WebAppInstaller.CommandResultCode."
      "AppPreloadServiceDefault",
      webapps::InstallResultCode::kSuccessNewInstall, 1);
}

IN_PROC_BROWSER_TEST_F(WebAppInstallerBrowserTest, InstallMultipleOemApps) {
  WebAppInstaller installer(profile());

  constexpr char kManifestTemplate[] = R"({
    "name": "Example App",
    "start_url": "/index.html",
    "scope": "/",
    "icons": $1
  })";
  constexpr char kManifestTemplate2[] = R"({
    "name": "Example App2",
    "start_url": "/index.html",
    "scope": "/",
    "icons": $1
  })";
  SetManifestResponse(AddIconToManifest(kManifestTemplate));
  SetManifest2Response(AddIconToManifest(kManifestTemplate2));

  base::HistogramTester histograms;
  base::test::TestFuture<bool> result;
  base::test::TestFuture<bool> result2;
  installer.InstallApp(
      AppInstallSurface::kAppPreloadServiceOem,
      CreateInstallData("Example App", "web:https://www.example.com/index.html",
                        "https://www.example.com/manifest.json",
                        "/manifest.json"),
      result.GetCallback());
  installer.InstallApp(
      AppInstallSurface::kAppPreloadServiceOem,
      CreateInstallData(
          "Example App2", "web:https://www.example2.com/index.html",
          "https://www.example2.com/manifest2.json", "/manifest2.json"),
      result2.GetCallback());
  ASSERT_TRUE(result.Get());
  ASSERT_TRUE(result2.Get());

  auto app_id = web_app::GenerateAppId(
      std::nullopt, GURL("https://www.example.com/index.html"));
  VerifyAppInstalled(app_id, "Example App", InstallReason::kOem);

  auto app_id2 = web_app::GenerateAppId(
      std::nullopt, GURL("https://www.example2.com/index.html"));
  VerifyAppInstalled(app_id2, "Example App2", InstallReason::kOem);

  histograms.ExpectBucketCount(
      "Apps.AppInstallService.WebAppInstaller.InstallResult",
      WebAppInstallResult::kSuccess, 2);
  histograms.ExpectBucketCount(
      "Apps.AppInstallService.WebAppInstaller.InstallResult."
      "AppPreloadServiceOem",
      WebAppInstallResult::kSuccess, 2);
  histograms.ExpectBucketCount(
      "Apps.AppInstallService.WebAppInstaller.CommandResultCode",
      webapps::InstallResultCode::kSuccessNewInstall, 2);
  histograms.ExpectBucketCount(
      "Apps.AppInstallService.WebAppInstaller.CommandResultCode."
      "AppPreloadServiceOem",
      webapps::InstallResultCode::kSuccessNewInstall, 2);
}

IN_PROC_BROWSER_TEST_F(WebAppInstallerBrowserTest, InstallWithManifestId) {
  WebAppInstaller installer(profile());

  SetManifestResponse(AddIconToManifest(R"({
    "id": "manifest_id",
    "name": "Example App",
    "start_url": "/index.html",
    "scope": "/",
    "icons": $1
  })"));

  base::test::TestFuture<bool> result;
  installer.InstallApp(
      AppInstallSurface::kAppPreloadServiceOem,
      CreateInstallData(
          "Example App", "web:https://www.example.com/manifest_id",
          "https://www.example.com/manifest.json", "/manifest.json"),
      result.GetCallback());
  ASSERT_TRUE(result.Get());

  // The generated app ID should take the manifest ID into account.
  auto app_id = web_app::GenerateAppId(
      "manifest_id", GURL("https://www.example.com/index.html"));
  ASSERT_TRUE(
      app_registry_cache().ForOneApp(app_id, [](const AppUpdate& update) {}));
}

// Reinstalling an existing user-installed app should not overwrite manifest
// data, but will add the OEM install reason.
IN_PROC_BROWSER_TEST_F(WebAppInstallerBrowserTest, InstallOverUserApp) {
  constexpr char kStartUrl[] = "https://www.example.com/";
  constexpr char kOriginalManifestUrl[] =
      "https://www.example.com/manifest.json";
  constexpr char kUserAppName[] = "User Installed App";

  WebAppInstaller installer(profile());

  auto app_id = web_app::test::InstallDummyWebApp(profile(), kUserAppName,
                                                  GURL(kStartUrl));

  SetManifestResponse(AddIconToManifest(R"({
    "name": "OEM Installed app",
    "start_url": "/",
    "icons": $1
  })"));

  base::test::TestFuture<bool> result;
  installer.InstallApp(
      AppInstallSurface::kAppPreloadServiceOem,
      CreateInstallData("OEM Installed app", base::StrCat({"web:", kStartUrl}),
                        kOriginalManifestUrl, "/manifest.json"),
      result.GetCallback());
  ASSERT_TRUE(result.Get());

  VerifyAppInstalled(app_id, kUserAppName, InstallReason::kOem);
}

// The manifest id in the proto does not match the calculated manifest id.
IN_PROC_BROWSER_TEST_F(WebAppInstallerBrowserTest,
                       InstallMismatchedDataManifestId) {
  WebAppInstaller installer(profile());

  SetManifestResponse(AddIconToManifest(R"({
    "name": "Example App",
    "start_url": "/index.html",
    "scope": "/",
    "icons": $1
  })"));

  base::HistogramTester histograms;
  base::test::TestFuture<bool> result;
  installer.InstallApp(
      AppInstallSurface::kAppPreloadServiceOem,
      CreateInstallData(
          "Example App", "web:https://www.example.com/manifest_id",
          "https://www.example.com/manifest.json", "/manifest.json"),
      result.GetCallback());
  ASSERT_FALSE(result.Get());

  auto app_id = web_app::GenerateAppId(
      std::nullopt, GURL("https://www.example.com/index.html"));
  bool found =
      app_registry_cache().ForOneApp(app_id, [](const AppUpdate& update) {});
  ASSERT_FALSE(found);

  histograms.ExpectBucketCount(
      "Apps.AppInstallService.WebAppInstaller.InstallResult",
      WebAppInstallResult::kWebAppInstallError, 1);
  histograms.ExpectBucketCount(
      "Apps.AppInstallService.WebAppInstaller.InstallResult."
      "AppPreloadServiceOem",
      WebAppInstallResult::kWebAppInstallError, 1);
  histograms.ExpectBucketCount(
      "Apps.AppInstallService.WebAppInstaller.CommandResultCode",
      webapps::InstallResultCode::kExpectedAppIdCheckFailed, 1);
  histograms.ExpectBucketCount(
      "Apps.AppInstallService.WebAppInstaller.CommandResultCode."
      "AppPreloadServiceOem",
      webapps::InstallResultCode::kExpectedAppIdCheckFailed, 1);
}

IN_PROC_BROWSER_TEST_F(WebAppInstallerBrowserTest, ManifestFileIsNotJSON) {
  WebAppInstaller installer(profile());

  SetManifestResponse("INVALID");

  base::test::TestFuture<bool> result;
  installer.InstallApp(
      AppInstallSurface::kAppPreloadServiceOem,
      CreateInstallData(
          "Example App", "web:https://www.example.com/manifest_id",
          "https://www.example.com/manifest.json", "/manifest.json"),
      result.GetCallback());
  ASSERT_FALSE(result.Get());

  auto app_id = web_app::GenerateAppId(
      std::nullopt, GURL("https://www.example.com/index.html"));
  bool found =
      app_registry_cache().ForOneApp(app_id, [](const AppUpdate& update) {});
  ASSERT_FALSE(found);
}

IN_PROC_BROWSER_TEST_F(WebAppInstallerBrowserTest,
                       ManifestFileIsHasMissingFields) {
  WebAppInstaller installer(profile());

  SetManifestResponse(R"({
    "is_valid": "no."
  })");

  base::HistogramTester histograms;
  base::test::TestFuture<bool> result;
  installer.InstallApp(
      AppInstallSurface::kAppPreloadServiceOem,
      CreateInstallData(
          "Example App", "web:https://www.example.com/manifest_id",
          "https://www.example.com/manifest.json", "/manifest.json"),
      result.GetCallback());
  ASSERT_FALSE(result.Get());

  auto app_id = web_app::GenerateAppId(
      std::nullopt, GURL("https://www.example.com/index.html"));
  bool found =
      app_registry_cache().ForOneApp(app_id, [](const AppUpdate& update) {});
  ASSERT_FALSE(found);

  histograms.ExpectBucketCount(
      "Apps.AppInstallService.WebAppInstaller.CommandResultCode",
      webapps::InstallResultCode::kNotValidManifestForWebApp, 1);
  histograms.ExpectBucketCount(
      "Apps.AppInstallService.WebAppInstaller.CommandResultCode."
      "AppPreloadServiceOem",
      webapps::InstallResultCode::kNotValidManifestForWebApp, 1);
}

IN_PROC_BROWSER_TEST_F(WebAppInstallerBrowserTest, ManifestWithFailingIcons) {
  WebAppInstaller installer(profile());

  constexpr char kManifestTemplate[] = R"({
    "name": "Example App",
    "start_url": "/index.html",
    "scope": "/",
    "icons": [{
      "src": "$1",
      "sizes": "96x96",
      "type": "image/png"
    }]
  })";

  // The image will fail to download, which will cause the installation to fail.
  GURL image_url =
      https_server()->GetURL("meltingpot.googleusercontent.com", "/404");

  SetManifestResponse(base::ReplaceStringPlaceholders(
      kManifestTemplate, {image_url.spec()}, nullptr));

  base::HistogramTester histograms;
  base::test::TestFuture<bool> result;
  installer.InstallApp(
      AppInstallSurface::kAppPreloadServiceOem,
      CreateInstallData(
          "Example App", "web:https://www.example.com/manifest_id",
          "https://www.example.com/manifest.json", "/manifest.json"),
      result.GetCallback());
  ASSERT_FALSE(result.Get());

  histograms.ExpectBucketCount(
      "Apps.AppInstallService.WebAppInstaller.CommandResultCode",
      webapps::InstallResultCode::kIconDownloadingFailed, 1);
  histograms.ExpectBucketCount(
      "Apps.AppInstallService.WebAppInstaller.CommandResultCode."
      "AppPreloadServiceOem",
      webapps::InstallResultCode::kIconDownloadingFailed, 1);
}

IN_PROC_BROWSER_TEST_F(WebAppInstallerBrowserTest, InstallWebsite) {
  WebAppInstaller installer(profile());
  SetManifestResponse(AddIconToManifest(R"({
    "name": "Example App",
    "start_url": "/",
    "icons": $1
  })"));

  AppInstallData data = CreateInstallData(
      "Example App", "website:https://www.example.com/",
      "https://www.example.com/manifest.json", "/manifest.json");
  // Unset user_window_override to request UserDisplayMode::kBrowser.
  absl::get<WebAppInstallData>(data.app_type_data).open_as_window = false;

  base::test::TestFuture<bool> result;
  installer.InstallApp(AppInstallSurface::kAppInstallUriUnknown, data,
                       result.GetCallback());
  ASSERT_TRUE(result.Get());

  // Verify that the app is set to open in a browser in App Service.
  auto app_id =
      web_app::GenerateAppId(std::nullopt, GURL("https://www.example.com/"));
  bool found =
      app_registry_cache().ForOneApp(app_id, [](const AppUpdate& update) {
        EXPECT_EQ(update.WindowMode(), apps::WindowMode::kBrowser);
      });
  ASSERT_TRUE(found);

  EXPECT_TRUE(web_app::WebAppProvider::GetForWebApps(profile())
                  ->registrar_unsafe()
                  .IsDiyApp(app_id));
}

IN_PROC_BROWSER_TEST_F(WebAppInstallerBrowserTest,
                       InstallWebsiteWithOpenInWindowOverride) {
  WebAppInstaller installer(profile());

  constexpr char kManifestTemplate[] = R"({
    "name": "Example App",
    "start_url": "/",
    "scope": "/",
    "icons": $1
  })";
  SetManifestResponse(AddIconToManifest(kManifestTemplate));

  AppInstallData data = CreateInstallData(
      "Example App", "website:https://www.example.com/",
      "https://www.example.com/manifest.json", "/manifest.json");
  // Unset user_window_override to request UserDisplayMode::kStandalone.
  absl::get<WebAppInstallData>(data.app_type_data).open_as_window = true;

  base::test::TestFuture<bool> result;
  installer.InstallApp(AppInstallSurface::kAppInstallUriUnknown, data,
                       result.GetCallback());
  ASSERT_TRUE(result.Get());

  // Verify that the app is set to open in a window in App Service.
  auto app_id =
      web_app::GenerateAppId(std::nullopt, GURL("https://www.example.com/"));
  bool found =
      app_registry_cache().ForOneApp(app_id, [](const AppUpdate& update) {
        EXPECT_EQ(update.WindowMode(), apps::WindowMode::kWindow);
      });
  ASSERT_TRUE(found);

  EXPECT_TRUE(web_app::WebAppProvider::GetForWebApps(profile())
                  ->registrar_unsafe()
                  .IsDiyApp(app_id));
}

}  // namespace apps
