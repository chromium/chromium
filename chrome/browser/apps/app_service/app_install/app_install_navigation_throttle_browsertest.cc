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
#include "chrome/browser/apps/app_service/app_install/test_app_install_server.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_registry_cache_waiter.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/browser_instance/browser_app_instance_tracker.h"
#include "chrome/browser/apps/link_capturing/link_capturing_feature_test_support.h"
#include "chrome/browser/chromeos/crosapi/test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/web_app_dialogs.h"
#include "chrome/browser/ui/web_applications/web_app_launch_process.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
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

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/test_controller.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/apps/app_service/app_install/app_install_service_ash.h"
#include "chrome/test/base/ui_test_utils.h"
#endif

namespace apps {

class AppInstallNavigationThrottleBrowserTest : public InProcessBrowserTest {
 public:
  class AutoAcceptInstallDialogScope {
   public:
    AutoAcceptInstallDialogScope() {
      crosapi::mojom::TestControllerAsyncWaiter(crosapi::GetTestController())
          .SetAppInstallDialogAutoAccept(true);
    }

    ~AutoAcceptInstallDialogScope() {
      crosapi::mojom::TestControllerAsyncWaiter(crosapi::GetTestController())
          .SetAppInstallDialogAutoAccept(false);
    }
  };

  void SetUpOnMainThread() override {
    if (!crosapi::AshSupportsCapabilities({"b/304680258"})) {
      GTEST_SKIP() << "Unsupported Ash version.";
    }

#if BUILDFLAG(IS_CHROMEOS_LACROS)
    if (!crosapi::AshSupportsCapabilities({"b/331715712", "b/339106891"})) {
      GTEST_SKIP() << "Unsupported Ash version.";
    }

    if (crosapi::GetInterfaceVersion<crosapi::mojom::TestController>() <
        int{crosapi::mojom::TestController::MethodMinVersions::
                kSetAppInstallDialogAutoAcceptMinVersion}) {
      GTEST_SKIP() << "Unsupported Ash version.";
    }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
    ASSERT_TRUE(app_install_server_.SetUp());

    apps::AppTypeInitializationWaiter(browser()->profile(), apps::AppType::kWeb)
        .Await();
  }

  TestAppInstallServer* app_install_server() { return &app_install_server_; }

 private:
  TestAppInstallServer app_install_server_;
};

IN_PROC_BROWSER_TEST_F(AppInstallNavigationThrottleBrowserTest,
                       JavaScriptTriggeredInstallation) {
  base::HistogramTester histograms;

  auto [app_id, package_id] = app_install_server()->SetUpWebAppResponse();

  auto* proxy = AppServiceProxyFactory::GetForProfile(browser()->profile());
  ASSERT_TRUE(proxy->AppRegistryCache().IsAppTypeInitialized(AppType::kWeb));

  // Make install prompts auto accept for this block.
  {
    AutoAcceptInstallDialogScope auto_accept_scope;

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
  }

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // These metrics are emitted on lacros only.
  histograms.ExpectBucketCount("Apps.AppInstallParentWindowFound", true, 1);
  histograms.ExpectBucketCount("Apps.AppInstallParentWindowFound", false, 0);
#endif
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(AppInstallNavigationThrottleBrowserTest,
                       OmniboxTriggeredInstallation) {
  base::HistogramTester histograms;

  auto [app_id, package_id] = app_install_server()->SetUpWebAppResponse();

  auto* proxy = AppServiceProxyFactory::GetForProfile(browser()->profile());
  ASSERT_TRUE(proxy->AppRegistryCache().IsAppTypeInitialized(AppType::kWeb));

  AutoAcceptInstallDialogScope auto_accept_scope;

  ui_test_utils::SendToOmniboxAndSubmit(
      browser(), base::StringPrintf("cros-apps://install-app?package_id=%s",
                                    package_id.ToString().c_str()));

  // This should trigger the sequence:
  // - AppInstallNavigationThrottle
  // - AppInstallServiceAsh
  // - NavigateAndTriggerInstallDialogCommand

  // Await install to complete.
  web_app::WebAppTestInstallObserver(browser()->profile())
      .BeginListeningAndWait({app_id});
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

IN_PROC_BROWSER_TEST_F(AppInstallNavigationThrottleBrowserTest,
                       GeForceNowInstall) {
  // Set up a mock GeForce NOW app.
  webapps::AppId app_id =
      web_app::test::InstallWebApp(browser()->profile(), []() {
        auto info = web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(
            GURL("https://play.geforcenow.com/"));
        info->user_display_mode = web_app::mojom::UserDisplayMode::kStandalone;
        return info;
      }());
  app_install_server()->SetUpInstallUrlResponse(
      PackageId(PackageType::kGeForceNow, "1234"),
      GURL("https://play.geforcenow.com/games?game-id=1234"));

  ui_test_utils::BrowserChangeObserver browser_observer(
      nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);

  // Open install-app URI with gfn package.
  EXPECT_EQ(browser()->tab_strip_model()->count(), 1);
  EXPECT_TRUE(content::ExecJs(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "window.open('cros-apps://install-app?package_id=gfn:1234');"));

  // Expect GeForce NOW app to be opened.
  EXPECT_TRUE(web_app::AppBrowserController::IsForWebApp(
      browser_observer.Wait(), app_id));
}

IN_PROC_BROWSER_TEST_F(AppInstallNavigationThrottleBrowserTest,
                       OpenGeforceNowInstallUriInNewWindow) {
  GURL geforce_now_url = GURL("https://play.geforcenow.com/games?game-id=1234");
  app_install_server()->SetUpInstallUrlResponse(
      PackageId(PackageType::kGeForceNow, "1234"), geforce_now_url);

  ASSERT_EQ(browser()->tab_strip_model()->count(), 1);

  content::TestNavigationObserver observer(geforce_now_url);
  observer.StartWatchingNewWebContents();

  NavigateParams params(browser()->profile(),
                        GURL("cros-apps://install-app?package_id=gfn:1234"),
                        ui::PAGE_TRANSITION_TYPED);
  Navigate(&params);

  observer.WaitForNavigationFinished();
  EXPECT_EQ(browser()->tab_strip_model()->count(), 2);
  EXPECT_EQ(browser()->tab_strip_model()->GetWebContentsAt(1)->GetVisibleURL(),
            geforce_now_url);
}

IN_PROC_BROWSER_TEST_F(AppInstallNavigationThrottleBrowserTest,
                       InstallUrlFallback) {
  base::HistogramTester histograms;

  // Set up payload
  GURL install_url = app_install_server()->GetUrl("/web_apps/basic.html");
  proto::AppInstallResponse response;
  proto::AppInstallResponse_AppInstance& instance =
      *response.mutable_app_instance();
  instance.set_install_url(install_url.spec());
  app_install_server()->SetUpResponse("unknown package id format", response);

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

IN_PROC_BROWSER_TEST_F(AppInstallNavigationThrottleBrowserTest, NonSpecialUrl) {
  base::HistogramTester histograms;

  auto [app_id, package_id] = app_install_server()->SetUpWebAppResponse();

  auto* proxy = AppServiceProxyFactory::GetForProfile(browser()->profile());
  ASSERT_TRUE(proxy->AppRegistryCache().IsAppTypeInitialized(AppType::kWeb));

  // Make install prompts auto accept.
  AutoAcceptInstallDialogScope auto_accept_scope;

  // Open install-app URI.
  EXPECT_EQ(browser()->tab_strip_model()->count(), 1);
  EXPECT_TRUE(content::ExecJs(
      browser()->tab_strip_model()->GetActiveWebContents(),
      base::StringPrintf("window.open('cros-apps:install-app?package_id=%s');",
                         package_id.ToString().c_str())));

  // This should trigger the sequence:
  // - AppInstallNavigationThrottle
  // - AppInstallServiceAsh
  // - NavigateAndTriggerInstallDialogCommand

  // Await install to complete.
  web_app::WebAppTestInstallObserver(browser()->profile())
      .BeginListeningAndWait({app_id});
}

IN_PROC_BROWSER_TEST_F(AppInstallNavigationThrottleBrowserTest, LegacyScheme) {
  base::HistogramTester histograms;

  auto [app_id, package_id] = app_install_server()->SetUpWebAppResponse();

  auto* proxy = AppServiceProxyFactory::GetForProfile(browser()->profile());
  ASSERT_TRUE(proxy->AppRegistryCache().IsAppTypeInitialized(AppType::kWeb));

  // Make install prompts auto accept.
  AutoAcceptInstallDialogScope auto_accept_scope;

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
IN_PROC_BROWSER_TEST_F(AppInstallNavigationThrottleBrowserTest,
                       InstallationWithoutParentWindow) {
  base::HistogramTester histograms;

  auto [app_id, package_id] = app_install_server()->SetUpWebAppResponse();

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
  AutoAcceptInstallDialogScope auto_accept_scope;

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

using AppInstallNavigationThrottleUserGestureBrowserTest = InProcessBrowserTest;

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
