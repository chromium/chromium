// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/apps/app_preload_service/web_app_preload_installer.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/crosapi/browser_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/web_applications/test/app_registry_cache_waiter.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/test/base/chromeos/ash_browser_test_starter.h"
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
#include "testing/gtest/include/gtest/gtest.h"

namespace apps {

class WebAppPreloadInstallerLacrosBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    if (ash_starter_.HasLacrosArgument()) {
      ASSERT_TRUE(ash_starter_.PrepareEnvironmentForLacros());
    }
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    if (ash_starter_.HasLacrosArgument()) {
      ash_starter_.StartLacros(this);
    }

    https_server_.RegisterRequestHandler(base::BindRepeating(
        &WebAppPreloadInstallerLacrosBrowserTest::HandleRequest,
        base::Unretained(this)));
    https_server_.AddDefaultHandlers(GetChromeTestDataDir());

    ASSERT_TRUE(https_server_.Start());
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    if (request.relative_url != "/manifest.json" || manifest_.empty()) {
      return nullptr;
    }

    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(net::HTTP_OK);
    response->set_content_type("application/json");
    response->set_content(manifest_);
    return response;
  }

  std::string AddIconToManifest(const std::string& manifest_template) {
    GURL icon_url = https_server()->GetURL("/web_apps/blue-192.png");
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

  void SetManifestResponse(std::string manifest) { manifest_ = manifest; }

  Profile* profile() { return browser()->profile(); }

  net::EmbeddedTestServer* https_server() { return &https_server_; }

  AppRegistryCache& app_registry_cache() {
    auto* proxy = AppServiceProxyFactory::GetForProfile(browser()->profile());
    return proxy->AppRegistryCache();
  }

 protected:
  test::AshBrowserTestStarter ash_starter_;

 private:
  net::EmbeddedTestServer https_server_;
  std::string manifest_;
};

IN_PROC_BROWSER_TEST_F(WebAppPreloadInstallerLacrosBrowserTest, InstallOemApp) {
  if (!ash_starter_.HasLacrosArgument()) {
    return;
  }
  // Assert Lacros is running.
  ASSERT_TRUE(crosapi::BrowserManager::Get()->IsRunning());

  proto::AppPreloadListResponse_App app;
  app.set_name("Example App");
  app.set_package_id("web:https://www.example.com/index.html");
  app.set_install_reason(proto::AppPreloadListResponse::INSTALL_REASON_OEM);

  auto* web_extras = app.mutable_web_extras();
  web_extras->set_original_manifest_url(
      "https://www.example.com/manifest.json");
  web_extras->set_manifest_url(https_server()->GetURL("/manifest.json").spec());

  constexpr char kManifestTemplate[] = R"({
    "name": "Example App",
    "start_url": "/index.html",
    "scope": "/",
    "icons": $1
  })";

  SetManifestResponse(AddIconToManifest(kManifestTemplate));

  base::HistogramTester histograms;
  base::test::TestFuture<bool> result;

  // Install the app.
  WebAppPreloadInstaller installer(profile());
  installer.InstallAllApps({PreloadAppDefinition(app)}, result.GetCallback());
  ASSERT_TRUE(result.Get());

  // Check the app is installed in app_registry_cache.
  auto app_id = web_app::GenerateAppId(
      absl::nullopt, GURL("https://www.example.com/index.html"));

  // Wait for update to be registered with the app registry cache.
  web_app::AppReadinessWaiter(profile(), app_id).Await();
  bool found =
      app_registry_cache().ForOneApp(app_id, [](const AppUpdate& update) {
        EXPECT_EQ(update.Name(), "Example App");
        EXPECT_EQ(update.InstallReason(), InstallReason::kOem);
      });
  ASSERT_TRUE(found);

  histograms.ExpectBucketCount("AppPreloadService.WebAppInstall.InstallResult",
                               WebAppPreloadResult::kSuccess, 1);
  histograms.ExpectBucketCount(
      "AppPreloadService.WebAppInstall.CommandResultCode",
      webapps::InstallResultCode::kSuccessNewInstall, 1);
}

}  // namespace apps
