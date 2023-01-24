// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/apps/app_preload_service/app_preload_service.h"
#include "chrome/browser/apps/app_preload_service/proto/app_provisioning.pb.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"

namespace apps {

class AppPreloadServiceBrowserTest : public InProcessBrowserTest {
 public:
  AppPreloadServiceBrowserTest() {
    feature_list_.InitWithFeatures(
        {/*enabled_features=*/features::kAppPreloadService},
        /*disabled_features=*/{});
  }

  void SetUpOnMainThread() override {
    // Note that App Preload Service runs as part of browser startup, so the
    // browser test SetUp() method will trigger a call to APS before any test
    // code runs. This call will fail as the EmbeddedTestServer will not be
    // started.
    InProcessBrowserTest::SetUpOnMainThread();

    https_server_.RegisterRequestHandler(base::BindRepeating(
        &AppPreloadServiceBrowserTest::HandleRequest, base::Unretained(this)));
    https_server_.AddDefaultHandlers(GetChromeTestDataDir());
    ASSERT_TRUE(https_server_.Start());

    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        ash::switches::kAlmanacApiUrl, https_server()->GetURL("/").spec());

    // Icon URLs should remap to the test server.
    host_resolver()->AddRule("meltingpot.googleusercontent.com", "127.0.0.1");
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    if (request.relative_url == "/manifest.json" && !manifest_.empty()) {
      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_code(net::HTTP_OK);
      response->set_content_type("application/json");
      response->set_content(manifest_);
      return response;
    }

    if (request.relative_url == "/v1/app_provisioning/apps?alt=proto" &&
        apps_proto_.has_value()) {
      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_code(net::HTTP_OK);
      response->set_content_type("application/x-protobuf");
      response->set_content(apps_proto_->SerializeAsString());
      return response;
    }

    return nullptr;
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

  void SetManifestResponse(std::string manifest) { manifest_ = manifest; }

  void SetAppProvisioningResponse(
      proto::AppProvisioningListAppsResponse response) {
    apps_proto_ = response;
  }

  Profile* profile() { return browser()->profile(); }

  net::EmbeddedTestServer* https_server() { return &https_server_; }

  AppRegistryCache& app_registry_cache() {
    auto* proxy = AppServiceProxyFactory::GetForProfile(browser()->profile());
    return proxy->AppRegistryCache();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  net::EmbeddedTestServer https_server_;
  std::string manifest_;
  absl::optional<proto::AppProvisioningListAppsResponse> apps_proto_;
};

IN_PROC_BROWSER_TEST_F(AppPreloadServiceBrowserTest, OemWebAppInstall) {
  proto::AppProvisioningListAppsResponse response;
  auto* app = response.add_apps_to_install();
  app->set_name("Example App");
  app->set_package_id("web:https://www.example.com/id");
  app->set_install_reason(
      proto::AppProvisioningListAppsResponse::INSTALL_REASON_OEM);

  app->mutable_web_extras()->set_manifest_url(
      https_server()->GetURL("/manifest.json").spec());
  app->mutable_web_extras()->set_original_manifest_url(
      "https://www.example.com/");

  SetAppProvisioningResponse(response);
  SetManifestResponse(AddIconToManifest(R"({
    "id": "id",
    "name": "Example App",
    "start_url": "/index.html",
    "icons": $1
  })"));

  base::test::TestFuture<bool> result;
  auto* service = AppPreloadService::Get(profile());
  service->StartFirstLoginFlowForTesting(result.GetCallback());
  ASSERT_TRUE(result.Get());

  auto app_id =
      web_app::GenerateAppId("id", GURL("https://www.example.com/index.html"));
  bool found =
      app_registry_cache().ForOneApp(app_id, [](const AppUpdate& update) {
        EXPECT_EQ(update.Name(), "Example App");
        EXPECT_EQ(update.InstallReason(), InstallReason::kOem);
        EXPECT_EQ(update.PublisherId(), "https://www.example.com/index.html");
      });
  ASSERT_TRUE(found);
}

IN_PROC_BROWSER_TEST_F(AppPreloadServiceBrowserTest, IgnoreDefaultAppInstall) {
  proto::AppProvisioningListAppsResponse response;
  auto* app = response.add_apps_to_install();
  app->set_name("Peanut Types");
  app->set_package_id("web:https://peanuttypes.com/app");
  app->set_install_reason(
      proto::AppProvisioningListAppsResponse::INSTALL_REASON_DEFAULT);

  app->mutable_web_extras()->set_manifest_url(
      https_server()->GetURL("/manifest.json").spec());
  app->mutable_web_extras()->set_original_manifest_url(
      "https://peanuttypes.com/app");

  SetAppProvisioningResponse(response);
  // No call to SetManifestResponse, so if installation was attempted, it would
  // fail.

  base::test::TestFuture<bool> result;
  auto* service = AppPreloadService::Get(profile());
  service->StartFirstLoginFlowForTesting(result.GetCallback());
  ASSERT_TRUE(result.Get());

  auto app_id = web_app::GenerateAppId(absl::nullopt,
                                       GURL("https://peanuttypes.com/app"));
  bool found = app_registry_cache().ForOneApp(app_id, [](const AppUpdate&) {});
  ASSERT_FALSE(found);
}

// Verifies that user-installed apps are not skipped, and are marked as OEM
// installed.
IN_PROC_BROWSER_TEST_F(AppPreloadServiceBrowserTest, InstallOverUserApp) {
  constexpr char kResolvedManifestId[] = "https://www.example.com/manifest_id";
  constexpr char kOriginalManifestUrl[] =
      "https://www.example.com/manifest.json";
  constexpr char kUserAppName[] = "User Installed App";
  constexpr char kManifest[] = R"({
    "id": "manifest_id",
    "name": "OEM Installed app",
    "start_url": "/",
    "icons": $1
  })";

  auto app_id = web_app::test::InstallDummyWebApp(profile(), kUserAppName,
                                                  GURL(kResolvedManifestId));

  proto::AppProvisioningListAppsResponse response;
  auto* app = response.add_apps_to_install();

  app->set_name("OEM Installed app");
  app->set_package_id(base::StrCat({"web:", kResolvedManifestId}));
  app->set_install_reason(
      proto::AppProvisioningListAppsResponse::INSTALL_REASON_OEM);
  app->mutable_web_extras()->set_manifest_url(
      https_server()->GetURL("/manifest.json").spec());
  app->mutable_web_extras()->set_original_manifest_url(kOriginalManifestUrl);

  SetAppProvisioningResponse(response);
  SetManifestResponse(AddIconToManifest(kManifest));

  base::test::TestFuture<bool> result;
  auto* service = AppPreloadService::Get(profile());
  service->StartFirstLoginFlowForTesting(result.GetCallback());
  ASSERT_TRUE(result.Get());

  bool found = AppServiceProxyFactory::GetForProfile(profile())
                   ->AppRegistryCache()
                   .ForOneApp(app_id, [](const AppUpdate& update) {
                     EXPECT_EQ(update.InstallReason(), InstallReason::kOem);
                   });
  ASSERT_TRUE(found);
}

}  // namespace apps
