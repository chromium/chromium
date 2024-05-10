// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_install/app_install_navigation_throttle.h"

#include "base/functional/callback.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/test/repeating_test_future.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_install/app_install.pb.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_registry_cache_waiter.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/browser_instance/browser_app_instance_tracker.h"
#include "chrome/browser/chromeos/crosapi/test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/web_applications/web_app_dialogs.h"
#include "chrome/browser/ui/web_applications/web_app_launch_process.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/crosapi/mojom/test_controller.mojom-test-utils.h"
#include "components/services/app_service/public/cpp/package_id.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/test_controller.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/apps/app_service/app_install/app_install_service_ash.h"
#endif

namespace apps {

class AppInstallNavigationThrottleBrowserTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  class AutoAcceptInstallDialogScope {
   public:
    explicit AutoAcceptInstallDialogScope(bool is_ash_dialog_enabled)
        : is_ash_dialog_enabled_(is_ash_dialog_enabled) {
      if (is_ash_dialog_enabled_) {
        crosapi::mojom::TestControllerAsyncWaiter(crosapi::GetTestController())
            .SetAppInstallDialogAutoAccept(true);
      } else {
        web_app::SetAutoAcceptPWAInstallConfirmationForTesting(true);
      }
    }

    ~AutoAcceptInstallDialogScope() {
      if (is_ash_dialog_enabled_) {
        crosapi::mojom::TestControllerAsyncWaiter(crosapi::GetTestController())
            .SetAppInstallDialogAutoAccept(false);
      } else {
        web_app::SetAutoAcceptPWAInstallConfirmationForTesting(false);
      }
    }

   private:
    const bool is_ash_dialog_enabled_;
  };

  static std::string ParamToString(testing::TestParamInfo<bool> param) {
    return param.param ? "AshDialogEnabled" : "AshDialogDisabled";
  }

  AppInstallNavigationThrottleBrowserTest() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    feature_list_.InitWithFeatureState(
        chromeos::features::kCrosWebAppInstallDialog, is_ash_dialog_enabled());
#endif
  }

  bool is_ash_dialog_enabled() const { return GetParam(); }

  void SetUpOnMainThread() override {
    if (!crosapi::AshSupportsCapabilities({"b/304680258"})) {
      GTEST_SKIP() << "Unsupported Ash version.";
    }

#if BUILDFLAG(IS_CHROMEOS_LACROS)
    // Lacros has no way to disable the dialog, so we only run tests with the
    // dialog enabled.
    ASSERT_TRUE(is_ash_dialog_enabled());

    if (!crosapi::AshSupportsCapabilities({"b/331715712", "b/339106891"})) {
      GTEST_SKIP() << "Unsupported Ash version.";
    }

    if (crosapi::GetInterfaceVersion<crosapi::mojom::TestController>() <
        int{crosapi::mojom::TestController::MethodMinVersions::
                kSetAppInstallDialogAutoAcceptMinVersion}) {
      GTEST_SKIP() << "Unsupported Ash version.";
    }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &AppInstallNavigationThrottleBrowserTest::HandleRequest,
        base::Unretained(this)));
    ASSERT_TRUE(embedded_test_server()->Start());

    crosapi::mojom::TestControllerAsyncWaiter(crosapi::GetTestController())
        .SetAlmanacEndpointUrlForTesting(
            embedded_test_server()->GetURL("/").spec());

    apps::AppTypeInitializationWaiter(browser()->profile(), apps::AppType::kWeb)
        .Await();
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    if (request.GetURL() != embedded_test_server()->GetURL("/v1/app-install")) {
      return nullptr;
    }

    auto http_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    proto::AppInstallRequest app_request;
    if (!app_request.ParseFromString(request.content)) {
      http_response->set_code(net::HTTP_BAD_REQUEST);
      return std::move(http_response);
    }

    auto it = app_install_map_.find(app_request.package_id());
    if (it == app_install_map_.end()) {
      http_response->set_code(net::HTTP_NOT_FOUND);
      return std::move(http_response);
    }

    http_response->set_code(net::HTTP_OK);
    http_response->set_content(it->second);
    return std::move(http_response);
  }

  base::test::ScopedFeatureList feature_list_;
  std::map<std::string, std::string> app_install_map_;
  base::AutoReset<bool> feature_scope_ =
      chromeos::features::SetAppInstallServiceUriEnabledForTesting();

  struct SetupIds {
    webapps::AppId app_id;
    PackageId package_id;
  };
  SetupIds SetupDefaultServerResponse() {
    GURL start_url = embedded_test_server()->GetURL("/web_apps/basic.html");
    GURL manifest_url = embedded_test_server()->GetURL("/web_apps/basic.json");
    webapps::ManifestId manifest_id = start_url;
    webapps::AppId app_id = web_app::GenerateAppIdFromManifestId(manifest_id);
    PackageId package_id(apps::PackageType::kWeb, manifest_id.spec());

    // Set Almanac server payload.
    app_install_map_[package_id.ToString()] = [&] {
      proto::AppInstallResponse response;
      proto::AppInstallResponse_AppInstance& instance =
          *response.mutable_app_instance();
      instance.set_package_id(package_id.ToString());
      instance.set_name("Test");
      proto::AppInstallResponse_WebExtras& web_extras =
          *instance.mutable_web_extras();
      web_extras.set_document_url(start_url.spec());
      web_extras.set_original_manifest_url(manifest_url.spec());
      web_extras.set_scs_url(manifest_url.spec());
      return response.SerializeAsString();
    }();

    return {app_id, package_id};
  }
};

IN_PROC_BROWSER_TEST_P(AppInstallNavigationThrottleBrowserTest,
                       UrlTriggeredInstallation) {
  base::HistogramTester histograms;

  auto [app_id, package_id] = SetupDefaultServerResponse();

  auto* proxy = AppServiceProxyFactory::GetForProfile(browser()->profile());
  ASSERT_TRUE(proxy->AppRegistryCache().IsAppTypeInitialized(AppType::kWeb));

  // Make install prompts auto accept for this block.
  {
    AutoAcceptInstallDialogScope auto_accept_scope(is_ash_dialog_enabled());

    // Open install-app URI.
    EXPECT_EQ(browser()->tab_strip_model()->count(), 1);
    EXPECT_TRUE(content::ExecJs(
        browser()->tab_strip_model()->GetActiveWebContents(),
        base::StringPrintf(
            "window.open('cros-apps://install-app?package_id=%s');",
            package_id.ToString().c_str())));

    // This should trigger the sequence:
    // - AppInstallNavigationThrottle
    // - AppInstallServiceAsh
    // - NavigateAndTriggerInstallDialogCommand

    // Await install to complete.
    web_app::WebAppTestInstallObserver(browser()->profile())
        .BeginListeningAndWait({app_id});

    if (!is_ash_dialog_enabled()) {
      // Check that window.open() didn't leave an extra about:blank tab lying
      // around, there should only be the original about:blank tab and the
      // install page tab.
      EXPECT_EQ(browser()->tab_strip_model()->count(), 2);
    }
  }

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // These metrics are emitted on lacros only.
  histograms.ExpectBucketCount("Apps.AppInstallParentWindowFound", true, 1);
  histograms.ExpectBucketCount("Apps.AppInstallParentWindowFound", false, 0);
#endif
}

IN_PROC_BROWSER_TEST_P(AppInstallNavigationThrottleBrowserTest,
                       InstallUrlFallback) {
  base::HistogramTester histograms;

  // Set up payload
  GURL install_url = embedded_test_server()->GetURL("/web_apps/basic.html");
  app_install_map_["unknown package id format"] = [&] {
    proto::AppInstallResponse response;
    proto::AppInstallResponse_AppInstance& instance =
        *response.mutable_app_instance();
    instance.set_install_url(install_url.spec());
    return response.SerializeAsString();
  }();

  {
    content::TestNavigationObserver observer(install_url);
    observer.StartWatchingNewWebContents();

    // Open unknown install-app URI.
    EXPECT_TRUE(content::ExecJs(
        browser()->tab_strip_model()->GetActiveWebContents(),
        "window.open('cros-apps://"
        "install-app?package_id=unknown%20package%20id%20format');"));

    // Expect install URL to be opened.
    observer.WaitForNavigationFinished();
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // These metrics are emitted on Ash only.
  histograms.ExpectBucketCount("Apps.AppInstallService.AppInstallResult",
                               AppInstallResult::kInstallUrlFallback, 1);
  histograms.ExpectBucketCount(
      "Apps.AppInstallService.AppInstallResult.AppInstallUriUnknown",
      AppInstallResult::kInstallUrlFallback, 1);
#endif
}

IN_PROC_BROWSER_TEST_P(AppInstallNavigationThrottleBrowserTest, LegacyScheme) {
  base::HistogramTester histograms;

  auto [app_id, package_id] = SetupDefaultServerResponse();

  auto* proxy = AppServiceProxyFactory::GetForProfile(browser()->profile());
  ASSERT_TRUE(proxy->AppRegistryCache().IsAppTypeInitialized(AppType::kWeb));

  // Make install prompts auto accept.
  AutoAcceptInstallDialogScope auto_accept_scope(is_ash_dialog_enabled());

  // Open install-app URI.
  EXPECT_EQ(browser()->tab_strip_model()->count(), 1);
  EXPECT_TRUE(content::ExecJs(
      browser()->tab_strip_model()->GetActiveWebContents(),
      base::StringPrintf("window.open('almanac://install-app?package_id=%s');",
                         package_id.ToString().c_str())));

  // This should trigger the sequence:
  // - AppInstallNavigationThrottle
  // - AppInstallServiceAsh
  // - NavigateAndTriggerInstallDialogCommand

  // Await install to complete.
  web_app::WebAppTestInstallObserver(browser()->profile())
      .BeginListeningAndWait({app_id});
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
// On lacros, window tracking is async so a parent window for anchoring the
// dialog might not be found. This test verifies that the dialog opening and app
// installation still works in that situation.
IN_PROC_BROWSER_TEST_P(AppInstallNavigationThrottleBrowserTest,
                       InstallationWithoutParentWindow) {
  base::HistogramTester histograms;

  auto [app_id, package_id] = SetupDefaultServerResponse();

  // Force BrowserAppInstanceTracker to forget about the current window. This
  // will cause the dialog to have no parent, and is more reliable than trying
  // to get the browser to close with the right timing.
  auto* proxy = AppServiceProxyFactory::GetForProfile(browser()->profile());
  ASSERT_TRUE(proxy);
  ASSERT_TRUE(proxy->BrowserAppInstanceTracker());
  proxy->BrowserAppInstanceTracker()->RemoveBrowserForTesting(browser());

  // Sanity check app registry is started and app isn't already installed.
  ASSERT_TRUE(proxy->AppRegistryCache().IsAppTypeInitialized(AppType::kWeb));
  ASSERT_FALSE(proxy->AppRegistryCache().ForOneApp(
      app_id, [](const apps::AppUpdate& update) {}));

  // Make install prompts auto accept.
  AutoAcceptInstallDialogScope auto_accept_scope(is_ash_dialog_enabled());

  // Open install-app URI.
  EXPECT_TRUE(content::ExecJs(
      browser()->tab_strip_model()->GetActiveWebContents(),
      base::StringPrintf(
          "window.location.href='cros-apps://install-app?package_id=%s'",
          package_id.ToString().c_str())));

  // This should trigger the sequence:
  // - AppInstallNavigationThrottle
  // - AppInstallServiceAsh
  // - NavigateAndTriggerInstallDialogCommand

  // Await install to complete.
  web_app::WebAppTestInstallObserver(browser()->profile())
      .BeginListeningAndWait({app_id});

  // These metrics are emitted on lacros only.
  histograms.ExpectBucketCount("Apps.AppInstallParentWindowFound", true, 0);
  histograms.ExpectBucketCount("Apps.AppInstallParentWindowFound", false, 1);
}
#endif

INSTANTIATE_TEST_SUITE_P(
    All,
    AppInstallNavigationThrottleBrowserTest,
#if BUILDFLAG(IS_CHROMEOS_ASH)
    testing::Bool(),
#else
    // Lacros has no way to disable the dialog, so we only
    // run tests with the dialog enabled.
    testing::Values(true),
#endif
    AppInstallNavigationThrottleBrowserTest::ParamToString);

class AppInstallNavigationThrottleUserGestureBrowserTest
    : public InProcessBrowserTest {
 public:
  base::AutoReset<bool> feature_scope_ =
      chromeos::features::SetAppInstallServiceUriEnabledForTesting();
};

IN_PROC_BROWSER_TEST_F(AppInstallNavigationThrottleUserGestureBrowserTest,
                       IgnoresNonUserGesture) {
  base::test::TestFuture<bool> future;
  AppInstallNavigationThrottle::MaybeCreateCallbackForTesting() =
      future.GetCallback();

  content::ExecuteScriptAsyncWithoutUserGesture(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "location.href = 'cros-apps://install-app?package_id=web:test';");

  EXPECT_FALSE(future.Get());

  // window.open() is another method of opening the cros-apps:// URI however it
  // is already blocked if there is no user gesture.
}

}  // namespace apps
