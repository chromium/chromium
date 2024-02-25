// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/apps/app_preload_service/app_preload_service.h"
#include "chrome/browser/apps/app_preload_service/app_preload_service_factory.h"
#include "chrome/browser/apps/app_preload_service/proto/app_preload.pb.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"

namespace apps {

namespace {
constexpr char kDefaultManifestUrl[] = "/manifest.json";

static constexpr char kFirstLoginFlowHistogramSuccessName[] =
    "AppPreloadService.FirstLoginFlowTime.Success";
static constexpr char kFirstLoginFlowHistogramFailureName[] =
    "AppPreloadService.FirstLoginFlowTime.Failure";
}  // namespace

class AppPreloadServiceBrowserTest : public InProcessBrowserTest {
 public:
  AppPreloadServiceBrowserTest()
      : startup_check_resetter_(
            AppPreloadService::DisablePreloadsOnStartupForTesting()) {
    feature_list_.InitWithFeatures(
        {/*enabled_features=*/features::kAppPreloadService},
        /*disabled_features=*/{});
    AppPreloadServiceFactory::SkipApiKeyCheckForTesting(true);
  }

  void SetUpOnMainThread() override {
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

  void TearDown() override {
    AppPreloadServiceFactory::SkipApiKeyCheckForTesting(false);
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    if (manifest_responses_.contains(request.relative_url)) {
      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_code(net::HTTP_OK);
      response->set_content_type("application/json");
      response->set_content(manifest_responses_[request.relative_url]);
      return response;
    }
    if (request.relative_url == "/v1/app-preload?alt=proto" &&
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

  // Sets the test server response for `relative_url` to the given JSON
  // `manifest` string.
  void SetManifestResponse(const std::string& relative_url,
                           const std::string& manifest) {
    manifest_responses_[relative_url] = manifest;
  }

  void SetAppProvisioningResponse(proto::AppPreloadListResponse response) {
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
  std::map<std::string, std::string> manifest_responses_;
  std::optional<proto::AppPreloadListResponse> apps_proto_;
  base::AutoReset<bool> startup_check_resetter_;
};

IN_PROC_BROWSER_TEST_F(AppPreloadServiceBrowserTest, OemWebAppInstall) {
  base::HistogramTester histograms;
  proto::AppPreloadListResponse response;
  auto* app = response.add_apps_to_install();
  app->set_name("Example App");
  app->set_package_id("web:https://www.example.com/id");
  app->set_install_reason(proto::AppPreloadListResponse::INSTALL_REASON_OEM);

  app->mutable_web_extras()->set_manifest_url(
      https_server()->GetURL(kDefaultManifestUrl).spec());
  app->mutable_web_extras()->set_original_manifest_url(
      "https://www.example.com/");

  const std::string kManifest = AddIconToManifest(R"({
    "id": "id",
    "name": "Example App",
    "start_url": "/index.html",
    "icons": $1
  })");

  SetAppProvisioningResponse(response);
  SetManifestResponse(kDefaultManifestUrl, kManifest);

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

  histograms.ExpectTotalCount(kFirstLoginFlowHistogramSuccessName, 1);
  histograms.ExpectTotalCount(kFirstLoginFlowHistogramFailureName, 0);
}

IN_PROC_BROWSER_TEST_F(AppPreloadServiceBrowserTest, DefaultAppInstall) {
  proto::AppPreloadListResponse response;
  auto* app = response.add_apps_to_install();
  app->set_name("Peanut Types");
  app->set_package_id("web:https://peanuttypes.com/app");
  app->set_install_reason(
      proto::AppPreloadListResponse::INSTALL_REASON_DEFAULT);

  app->mutable_web_extras()->set_manifest_url(
      https_server()->GetURL(kDefaultManifestUrl).spec());
  app->mutable_web_extras()->set_original_manifest_url(
      "https://peanuttypes.com/app");

  const std::string kManifest = AddIconToManifest(R"({
    "name": "Example App",
    "start_url": "/app",
    "icons": $1
  })");

  SetManifestResponse(kDefaultManifestUrl, kManifest);
  SetAppProvisioningResponse(response);

  base::test::TestFuture<bool> result;
  auto* service = AppPreloadService::Get(profile());
  service->StartFirstLoginFlowForTesting(result.GetCallback());
  ASSERT_TRUE(result.Get());

  auto app_id =
      web_app::GenerateAppId(std::nullopt, GURL("https://peanuttypes.com/app"));
  bool found =
      app_registry_cache().ForOneApp(app_id, [](const AppUpdate& update) {
        EXPECT_EQ(update.InstallReason(), InstallReason::kDefault);
      });
  ASSERT_TRUE(found);
}

IN_PROC_BROWSER_TEST_F(AppPreloadServiceBrowserTest, IgnoreTestAppInstall) {
  proto::AppPreloadListResponse response;
  auto* app = response.add_apps_to_install();
  app->set_name("Peanut Types");
  app->set_package_id("web:https://peanuttypes.com/app");
  app->set_install_reason(proto::AppPreloadListResponse::INSTALL_REASON_TEST);

  app->mutable_web_extras()->set_manifest_url(
      https_server()->GetURL(kDefaultManifestUrl).spec());
  app->mutable_web_extras()->set_original_manifest_url(
      "https://peanuttypes.com/app");

  SetAppProvisioningResponse(response);
  // No call to SetManifestResponse, so if installation was attempted, it would
  // fail.

  base::test::TestFuture<bool> result;
  auto* service = AppPreloadService::Get(profile());
  service->StartFirstLoginFlowForTesting(result.GetCallback());
  ASSERT_TRUE(result.Get());
}

// Verifies that user-installed apps are not skipped, and are marked as OEM
// installed.
IN_PROC_BROWSER_TEST_F(AppPreloadServiceBrowserTest, InstallOverUserApp) {
  constexpr char kResolvedManifestId[] = "https://www.example.com/manifest_id";
  constexpr char kOriginalManifestUrl[] =
      "https://www.example.com/manifest.json";
  constexpr char kUserAppName[] = "User Installed App";
  const std::string kManifest = AddIconToManifest(R"({
    "id": "manifest_id",
    "name": "OEM Installed app",
    "start_url": "/",
    "icons": $1
  })");

  auto app_id = web_app::test::InstallDummyWebApp(profile(), kUserAppName,
                                                  GURL(kResolvedManifestId));

  proto::AppPreloadListResponse response;
  auto* app = response.add_apps_to_install();

  app->set_name("OEM Installed app");
  app->set_package_id(base::StrCat({"web:", kResolvedManifestId}));
  app->set_install_reason(proto::AppPreloadListResponse::INSTALL_REASON_OEM);
  app->mutable_web_extras()->set_manifest_url(
      https_server()->GetURL(kDefaultManifestUrl).spec());
  app->mutable_web_extras()->set_original_manifest_url(kOriginalManifestUrl);

  SetAppProvisioningResponse(response);
  SetManifestResponse(kDefaultManifestUrl, kManifest);

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

// Verifies that multiple OEM apps can be installed at once.
IN_PROC_BROWSER_TEST_F(AppPreloadServiceBrowserTest, InstallMultipleOemApps) {
  constexpr char kOriginalManifestUrl1[] = "https://www.foo.com/manifest.json";
  constexpr char kOriginalManifestUrl2[] = "https://www.bar.com/manifest.json";

  proto::AppPreloadListResponse response;
  auto* app1 = response.add_apps_to_install();

  app1->set_name("Foo");
  app1->set_package_id("web:https://www.foo.com/");
  app1->set_install_reason(proto::AppPreloadListResponse::INSTALL_REASON_OEM);
  app1->mutable_web_extras()->set_manifest_url(
      https_server()->GetURL("/manifest/foo.json").spec());
  app1->mutable_web_extras()->set_original_manifest_url(kOriginalManifestUrl1);

  const std::string kManifest1 = AddIconToManifest(R"({
    "name": "Foo",
    "start_url": "/",
    "icons": $1
  })");

  SetManifestResponse("/manifest/foo.json", kManifest1);

  auto* app2 = response.add_apps_to_install();

  app2->set_name("Bar");
  app2->set_package_id("web:https://www.bar.com/");
  app2->set_install_reason(proto::AppPreloadListResponse::INSTALL_REASON_OEM);
  app2->mutable_web_extras()->set_manifest_url(
      https_server()->GetURL("/manifest/bar.json").spec());
  app2->mutable_web_extras()->set_original_manifest_url(kOriginalManifestUrl2);

  const std::string kManifest2 = AddIconToManifest(R"({
    "name": "Bar",
    "start_url": "/",
    "icons": $1
  })");

  SetManifestResponse("/manifest/bar.json", kManifest2);

  SetAppProvisioningResponse(response);

  base::test::TestFuture<bool> result;
  auto* service = AppPreloadService::Get(profile());
  service->StartFirstLoginFlowForTesting(result.GetCallback());
  ASSERT_TRUE(result.Get());

  auto app_id1 =
      web_app::GenerateAppId(std::nullopt, GURL("https://www.foo.com/"));
  bool found =
      app_registry_cache().ForOneApp(app_id1, [](const AppUpdate& update) {
        EXPECT_EQ(update.Name(), "Foo");
        EXPECT_EQ(update.InstallReason(), InstallReason::kOem);
      });
  ASSERT_TRUE(found);

  auto app_id2 =
      web_app::GenerateAppId(std::nullopt, GURL("https://www.bar.com/"));
  found = app_registry_cache().ForOneApp(app_id2, [](const AppUpdate& update) {
    EXPECT_EQ(update.Name(), "Bar");
    EXPECT_EQ(update.InstallReason(), InstallReason::kOem);
  });
  ASSERT_TRUE(found);
}

// Verifies that failed installations are retried on the next login flow, and
// already installed apps are ignored.
IN_PROC_BROWSER_TEST_F(AppPreloadServiceBrowserTest, RetryFailedApps) {
  base::HistogramTester histograms;
  constexpr char kOriginalManifestUrl1[] = "https://www.foo.com/manifest.json";
  constexpr char kOriginalManifestUrl2[] = "https://www.bar.com/manifest.json";

  proto::AppPreloadListResponse response;
  auto* app1 = response.add_apps_to_install();

  app1->set_name("Foo");
  app1->set_package_id("web:https://www.foo.com/");
  app1->set_install_reason(proto::AppPreloadListResponse::INSTALL_REASON_OEM);
  app1->mutable_web_extras()->set_manifest_url(
      https_server()->GetURL("/manifest/foo.json").spec());
  app1->mutable_web_extras()->set_original_manifest_url(kOriginalManifestUrl1);

  const std::string kManifest1 = AddIconToManifest(R"({
    "name": "Foo",
    "start_url": "/",
    "icons": $1
  })");

  auto* app2 = response.add_apps_to_install();

  app2->set_name("Bar");
  app2->set_package_id("web:https://www.bar.com/");
  app2->set_install_reason(proto::AppPreloadListResponse::INSTALL_REASON_OEM);
  app2->mutable_web_extras()->set_manifest_url(
      https_server()->GetURL("/manifest/bar.json").spec());
  app2->mutable_web_extras()->set_original_manifest_url(kOriginalManifestUrl2);

  const std::string kManifest2 = AddIconToManifest(R"({
    "name": "Bar",
    "start_url": "/",
    "icons": $1
  })");

  SetAppProvisioningResponse(response);

  // foo.json installs successfully but bar.json gives an error.
  SetManifestResponse("/manifest/foo.json", kManifest1);
  SetManifestResponse("/manifest/bar.json", "");

  base::test::TestFuture<bool> result;
  auto* service = AppPreloadService::Get(profile());
  service->StartFirstLoginFlowForTesting(result.GetCallback());
  ASSERT_FALSE(result.Get());

  histograms.ExpectTotalCount(kFirstLoginFlowHistogramSuccessName, 0);
  histograms.ExpectTotalCount(kFirstLoginFlowHistogramFailureName, 1);

  // bar.json should be retried, and will now succeed. foo.json is skipped
  // (ignoring the error it would give), and so the whole flow is successful.
  SetManifestResponse("/manifest/foo.json", "");
  SetManifestResponse("/manifest/bar.json", kManifest2);

  base::test::TestFuture<bool> result2;
  service->StartFirstLoginFlowForTesting(result2.GetCallback());
  ASSERT_TRUE(result2.Get());

  // Both apps should now be installed.
  auto app_id1 =
      web_app::GenerateAppId(std::nullopt, GURL("https://www.foo.com/"));
  bool found = app_registry_cache().ForOneApp(app_id1, [](const AppUpdate&) {});
  ASSERT_TRUE(found);

  auto app_id2 =
      web_app::GenerateAppId(std::nullopt, GURL("https://www.bar.com/"));
  found = app_registry_cache().ForOneApp(app_id2, [](const AppUpdate&) {});
  ASSERT_TRUE(found);

  histograms.ExpectTotalCount(kFirstLoginFlowHistogramSuccessName, 1);
  histograms.ExpectTotalCount(kFirstLoginFlowHistogramFailureName, 1);
}

IN_PROC_BROWSER_TEST_F(AppPreloadServiceBrowserTest, InstallNoApp) {
  proto::AppPreloadListResponse response;
  SetAppProvisioningResponse(response);
  base::test::TestFuture<bool> result;
  auto* service = AppPreloadService::Get(profile());
  service->StartFirstLoginFlowForTesting(result.GetCallback());
  ASSERT_TRUE(result.Get());
}

class AppPreloadServiceWithTestAppsBrowserTest
    : public AppPreloadServiceBrowserTest {
 private:
  base::test::ScopedFeatureList feature_list_{kAppPreloadServiceEnableTestApps};
};

// When kAppPreloadServiceEnableTestApps is enabled, apps with the "test"
// install reason should be installed.
IN_PROC_BROWSER_TEST_F(AppPreloadServiceWithTestAppsBrowserTest,
                       InstallTestApp) {
  proto::AppPreloadListResponse response;
  auto* app = response.add_apps_to_install();
  app->set_name("Peanut Types");
  app->set_package_id("web:https://peanuttypes.com/app");
  app->set_install_reason(proto::AppPreloadListResponse::INSTALL_REASON_TEST);

  app->mutable_web_extras()->set_manifest_url(
      https_server()->GetURL(kDefaultManifestUrl).spec());
  app->mutable_web_extras()->set_original_manifest_url(
      "https://peanuttypes.com/app");

  const std::string kManifest = AddIconToManifest(R"({
    "name": "Peanut Types",
    "start_url": "/app",
    "icons": $1
  })");

  SetAppProvisioningResponse(response);
  SetManifestResponse(kDefaultManifestUrl, kManifest);

  base::test::TestFuture<bool> result;
  auto* service = AppPreloadService::Get(profile());
  service->StartFirstLoginFlowForTesting(result.GetCallback());
  ASSERT_TRUE(result.Get());

  auto app_id =
      web_app::GenerateAppId(std::nullopt, GURL("https://peanuttypes.com/app"));
  bool found = app_registry_cache().ForOneApp(app_id, [](const AppUpdate&) {});
  ASSERT_TRUE(found);
}

}  // namespace apps
