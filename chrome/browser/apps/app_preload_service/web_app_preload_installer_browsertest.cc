// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "chrome/browser/apps/app_preload_service/preload_app_definition.h"
#include "chrome/browser/apps/app_preload_service/proto/app_preload.pb.h"
#include "chrome/browser/apps/app_preload_service/web_app_preload_installer.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
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

class WebAppPreloadInstallerBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    https_server_.RegisterRequestHandler(
        base::BindRepeating(&WebAppPreloadInstallerBrowserTest::HandleRequest,
                            base::Unretained(this)));
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

  proto::AppPreloadListResponse_App CreateValidPreloadApp(
      const std::string& name,
      const std::string& package_id,
      const std::string& original_manifest_url,
      const std::string& test_manifest_url) {
    proto::AppPreloadListResponse_App app;
    app.set_name(name);
    app.set_package_id(package_id);
    app.set_install_reason(proto::AppPreloadListResponse::INSTALL_REASON_OEM);

    auto* web_extras = app.mutable_web_extras();
    web_extras->set_original_manifest_url(original_manifest_url);
    web_extras->set_manifest_url(
        https_server()->GetURL(test_manifest_url).spec());
    return app;
  }

  void VerifyAppInstalled(webapps::AppId app_id, const std::string& app_name) {
    bool found = app_registry_cache().ForOneApp(
        app_id, [app_name](const AppUpdate& update) {
          EXPECT_EQ(update.Name(), app_name);
          EXPECT_EQ(update.InstallReason(), InstallReason::kOem);
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

IN_PROC_BROWSER_TEST_F(WebAppPreloadInstallerBrowserTest, InstallOneOemApp) {
  WebAppPreloadInstaller installer(profile());

  proto::AppPreloadListResponse_App app = CreateValidPreloadApp(
      "Example App", "web:https://www.example.com/index.html",
      "https://www.example.com/manifest.json", "/manifest.json");

  constexpr char kManifestTemplate[] = R"({
    "name": "Example App",
    "start_url": "/index.html",
    "scope": "/",
    "icons": $1
  })";
  SetManifestResponse(AddIconToManifest(kManifestTemplate));

  base::HistogramTester histograms;
  base::test::TestFuture<bool> result;
  installer.InstallAllApps({PreloadAppDefinition(app)}, result.GetCallback());
  ASSERT_TRUE(result.Get());

  auto app_id = web_app::GenerateAppId(
      absl::nullopt, GURL("https://www.example.com/index.html"));
  VerifyAppInstalled(app_id, "Example App");

  histograms.ExpectBucketCount("AppPreloadService.WebAppInstall.InstallResult",
                               WebAppPreloadResult::kSuccess, 1);
  histograms.ExpectBucketCount(
      "AppPreloadService.WebAppInstall.CommandResultCode",
      webapps::InstallResultCode::kSuccessNewInstall, 1);
}

IN_PROC_BROWSER_TEST_F(WebAppPreloadInstallerBrowserTest,
                       InstallMultipleOemApps) {
  WebAppPreloadInstaller installer(profile());

  proto::AppPreloadListResponse_App app = CreateValidPreloadApp(
      "Example App", "web:https://www.example.com/index.html",
      "https://www.example.com/manifest.json", "/manifest.json");
  proto::AppPreloadListResponse_App app2 = CreateValidPreloadApp(
      "Example App2", "web:https://www.example2.com/index.html",
      "https://www.example2.com/manifest2.json", "/manifest2.json");

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
  installer.InstallAllApps(
      {PreloadAppDefinition(app), PreloadAppDefinition(app2)},
      result.GetCallback());
  ASSERT_TRUE(result.Get());

  auto app_id = web_app::GenerateAppId(
      absl::nullopt, GURL("https://www.example.com/index.html"));
  VerifyAppInstalled(app_id, "Example App");

  auto app_id2 = web_app::GenerateAppId(
      absl::nullopt, GURL("https://www.example2.com/index.html"));
  VerifyAppInstalled(app_id2, "Example App2");

  histograms.ExpectBucketCount("AppPreloadService.WebAppInstall.InstallResult",
                               WebAppPreloadResult::kSuccess, 2);
  histograms.ExpectBucketCount(
      "AppPreloadService.WebAppInstall.CommandResultCode",
      webapps::InstallResultCode::kSuccessNewInstall, 2);
}

IN_PROC_BROWSER_TEST_F(WebAppPreloadInstallerBrowserTest,
                       InstallWithManifestId) {
  WebAppPreloadInstaller installer(profile());

  proto::AppPreloadListResponse_App app = CreateValidPreloadApp(
      "Example App", "web:https://www.example.com/index.html",
      "https://www.example.com/manifest.json", "/manifest.json");
  app.set_package_id("web:https://www.example.com/manifest_id");

  SetManifestResponse(AddIconToManifest(R"({
    "id": "manifest_id",
    "name": "Example App",
    "start_url": "/index.html",
    "scope": "/",
    "icons": $1
  })"));

  base::test::TestFuture<bool> result;
  installer.InstallAllApps({PreloadAppDefinition(app)}, result.GetCallback());
  ASSERT_TRUE(result.Get());

  // The generated app ID should take the manifest ID into account.
  auto app_id = web_app::GenerateAppId(
      "manifest_id", GURL("https://www.example.com/index.html"));
  ASSERT_TRUE(
      app_registry_cache().ForOneApp(app_id, [](const AppUpdate& update) {}));
}

// Reinstalling an existing user-installed app should not overwrite manifest
// data, but will add the OEM install reason.
IN_PROC_BROWSER_TEST_F(WebAppPreloadInstallerBrowserTest, InstallOverUserApp) {
  constexpr char kStartUrl[] = "https://www.example.com/";
  constexpr char kOriginalManifestUrl[] =
      "https://www.example.com/manifest.json";
  constexpr char kUserAppName[] = "User Installed App";

  WebAppPreloadInstaller installer(profile());

  auto app_id = web_app::test::InstallDummyWebApp(profile(), kUserAppName,
                                                  GURL(kStartUrl));

  proto::AppPreloadListResponse_App app = CreateValidPreloadApp(
      "OEM Installed app", base::StrCat({"web:", kStartUrl}),
      kOriginalManifestUrl, "/manifest.json");

  SetManifestResponse(AddIconToManifest(R"({
    "name": "OEM Installed app",
    "start_url": "/",
    "icons": $1
  })"));

  base::test::TestFuture<bool> result;
  installer.InstallAllApps({PreloadAppDefinition(app)}, result.GetCallback());
  ASSERT_TRUE(result.Get());

  VerifyAppInstalled(app_id, kUserAppName);
}

// The manifest id in the proto does not match the calculated manifest id.
IN_PROC_BROWSER_TEST_F(WebAppPreloadInstallerBrowserTest,
                       InstallMismatchedDataManifestId) {
  WebAppPreloadInstaller installer(profile());

  proto::AppPreloadListResponse_App app = CreateValidPreloadApp(
      "Example App", "web:https://www.example.com/manifest_id",
      "https://www.example.com/manifest.json", "/manifest.json");

  SetManifestResponse(AddIconToManifest(R"({
    "name": "Example App",
    "start_url": "/index.html",
    "scope": "/",
    "icons": $1
  })"));

  base::HistogramTester histograms;
  base::test::TestFuture<bool> result;
  installer.InstallAllApps({PreloadAppDefinition(app)}, result.GetCallback());
  ASSERT_FALSE(result.Get());

  auto app_id = web_app::GenerateAppId(
      absl::nullopt, GURL("https://www.example.com/index.html"));
  bool found =
      app_registry_cache().ForOneApp(app_id, [](const AppUpdate& update) {});
  ASSERT_FALSE(found);

  histograms.ExpectBucketCount("AppPreloadService.WebAppInstall.InstallResult",
                               WebAppPreloadResult::kWebAppInstallError, 1);
  histograms.ExpectBucketCount(
      "AppPreloadService.WebAppInstall.CommandResultCode",
      webapps::InstallResultCode::kExpectedAppIdCheckFailed, 1);
}

IN_PROC_BROWSER_TEST_F(WebAppPreloadInstallerBrowserTest,
                       ManifestFileIsNotJSON) {
  WebAppPreloadInstaller installer(profile());

  proto::AppPreloadListResponse_App app = CreateValidPreloadApp(
      "Example App", "web:https://www.example.com/manifest_id",
      "https://www.example.com/manifest.json", "/manifest.json");
  SetManifestResponse("INVALID");

  base::test::TestFuture<bool> result;
  installer.InstallAllApps({PreloadAppDefinition(app)}, result.GetCallback());
  ASSERT_FALSE(result.Get());

  auto app_id = web_app::GenerateAppId(
      absl::nullopt, GURL("https://www.example.com/index.html"));
  bool found =
      app_registry_cache().ForOneApp(app_id, [](const AppUpdate& update) {});
  ASSERT_FALSE(found);
}

IN_PROC_BROWSER_TEST_F(WebAppPreloadInstallerBrowserTest,
                       ManifestFileIsHasMissingFields) {
  WebAppPreloadInstaller installer(profile());

  proto::AppPreloadListResponse_App app = CreateValidPreloadApp(
      "Example App", "web:https://www.example.com/manifest_id",
      "https://www.example.com/manifest.json", "/manifest.json");
  SetManifestResponse(R"({
    "is_valid": "no."
  })");

  base::HistogramTester histograms;
  base::test::TestFuture<bool> result;
  installer.InstallAllApps({PreloadAppDefinition(app)}, result.GetCallback());
  ASSERT_FALSE(result.Get());

  auto app_id = web_app::GenerateAppId(
      absl::nullopt, GURL("https://www.example.com/index.html"));
  bool found =
      app_registry_cache().ForOneApp(app_id, [](const AppUpdate& update) {});
  ASSERT_FALSE(found);

  histograms.ExpectBucketCount(
      "AppPreloadService.WebAppInstall.CommandResultCode",
      webapps::InstallResultCode::kNotValidManifestForWebApp, 1);
}

IN_PROC_BROWSER_TEST_F(WebAppPreloadInstallerBrowserTest,
                       ManifestWithFailingIcons) {
  WebAppPreloadInstaller installer(profile());

  proto::AppPreloadListResponse_App app = CreateValidPreloadApp(
      "Example App", "web:https://www.example.com/manifest_id",
      "https://www.example.com/manifest.json", "/manifest.json");
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
  installer.InstallAllApps({PreloadAppDefinition(app)}, result.GetCallback());
  ASSERT_FALSE(result.Get());

  histograms.ExpectBucketCount(
      "AppPreloadService.WebAppInstall.CommandResultCode",
      webapps::InstallResultCode::kIconDownloadingFailed, 1);
}

IN_PROC_BROWSER_TEST_F(WebAppPreloadInstallerBrowserTest, InstallNoApps) {
  WebAppPreloadInstaller installer(profile());
  base::test::TestFuture<bool> result;
  std::vector<PreloadAppDefinition> apps;
  installer.InstallAllApps(apps, result.GetCallback());
  ASSERT_TRUE(result.Get());
}

}  // namespace apps
