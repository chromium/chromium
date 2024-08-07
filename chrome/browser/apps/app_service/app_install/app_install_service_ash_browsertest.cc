// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_install/app_install_service_ash.h"

#include <optional>

#include "ash/constants/ash_switches.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "chrome/browser/apps/app_service/app_install/app_install.pb.h"
#include "chrome/browser/apps/app_service/app_install/test_app_install_server.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/ash/app_install/app_install_dialog_test_helpers.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/services/app_service/public/cpp/package_id.h"
#include "components/user_manager/user_names.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace apps {

class AppInstallServiceAshBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    ASSERT_TRUE(app_install_server_.SetUp());
  }

  TestAppInstallServer* app_install_server() { return &app_install_server_; }

 private:
  TestAppInstallServer app_install_server_;
};

IN_PROC_BROWSER_TEST_F(AppInstallServiceAshBrowserTest,
                       InstallArcAppOpensPlayStore) {
  GURL play_install_url(
      "https://play.google.com/store/apps/details?id=com.android.chrome");
  app_install_server()->SetUpInstallUrlResponse(
      PackageId(PackageType::kArc, "com.android.chrome"), play_install_url);

  base::HistogramTester histogram_tester;

  content::TestNavigationObserver navigation_observer(play_install_url);
  navigation_observer.StartWatchingNewWebContents();

  AppServiceProxyFactory::GetForProfile(browser()->profile())
      ->AppInstallService()
      .InstallApp(AppInstallSurface::kAppInstallUriUnknown,
                  PackageId(PackageType::kArc, "com.android.chrome"),
                  /*anchor_window=*/std::nullopt, base::DoNothing());

  navigation_observer.Wait();

  AppInstallResult expected_result = AppInstallResult::kUnknown;
  histogram_tester.ExpectUniqueSample("Apps.AppInstallService.AppInstallResult",
                                      expected_result, 1);
  histogram_tester.ExpectUniqueSample(
      "Apps.AppInstallService.AppInstallResult.AppInstallUriUnknown",
      expected_result, 1);
}

IN_PROC_BROWSER_TEST_F(AppInstallServiceAshBrowserTest, InstallGfnAppOpensGfn) {
  GURL gfn_install_url("https://play.geforcenow.com/games?game-id=test");
  app_install_server()->SetUpInstallUrlResponse(
      PackageId(PackageType::kGeForceNow, "test"), gfn_install_url);

  base::HistogramTester histogram_tester;

  content::TestNavigationObserver navigation_observer(gfn_install_url);
  navigation_observer.StartWatchingNewWebContents();

  AppServiceProxyFactory::GetForProfile(browser()->profile())
      ->AppInstallService()
      .InstallApp(AppInstallSurface::kAppInstallUriUnknown,
                  PackageId(PackageType::kGeForceNow, "test"),
                  /*anchor_window=*/std::nullopt, base::DoNothing());

  navigation_observer.Wait();

  AppInstallResult expected_result = AppInstallResult::kUnknown;
  histogram_tester.ExpectUniqueSample("Apps.AppInstallService.AppInstallResult",
                                      expected_result, 1);
  histogram_tester.ExpectUniqueSample(
      "Apps.AppInstallService.AppInstallResult.AppInstallUriUnknown",
      expected_result, 1);
}

IN_PROC_BROWSER_TEST_F(AppInstallServiceAshBrowserTest,
                       InstallAndroidWithNoInstallUrlShowsError) {
  std::string test_package_id = "android:com.android.chrome";
  base::HistogramTester histograms;

  // Set up an invalid payload -- an Android app response with no Install URL.
  proto::AppInstallResponse response;
  proto::AppInstallResponse_AppInstance& instance =
      *response.mutable_app_instance();
  instance.set_package_id(test_package_id);
  instance.set_name("Test app");

  app_install_server()->SetUpResponse(test_package_id, response);

  base::test::TestFuture<void> completion_future;

  content::TestNavigationObserver navigation_observer_dialog(
      (GURL(chrome::kChromeUIAppInstallDialogURL)));
  navigation_observer_dialog.StartWatchingNewWebContents();

  AppServiceProxyFactory::GetForProfile(browser()->profile())
      ->AppInstallService()
      .InstallApp(AppInstallSurface::kAppInstallUriUnknown,
                  PackageId::FromString(test_package_id).value(),
                  /*anchor_window=*/std::nullopt,
                  completion_future.GetCallback());

  navigation_observer_dialog.Wait();
  content::WebContents* contents = ash::app_install::GetWebContentsFromDialog();
  ASSERT_TRUE(contents);
  EXPECT_EQ(ash::app_install::GetDialogTitle(contents), "App not available");

  ASSERT_TRUE(completion_future.Wait());
  histograms.ExpectUniqueSample(
      "Apps.AppInstallService.AppInstallResult.AppInstallUriUnknown",
      AppInstallResult::kAppDataCorrupted, 1);
}

IN_PROC_BROWSER_TEST_F(AppInstallServiceAshBrowserTest,
                       InstallWebsiteShowsInstallDialog) {
  auto [app_id, package_id] = app_install_server()->SetUpWebsiteResponse();

  content::TestNavigationObserver navigation_observer(
      (GURL(chrome::kChromeUIAppInstallDialogURL)));
  navigation_observer.StartWatchingNewWebContents();

  AppServiceProxyFactory::GetForProfile(browser()->profile())
      ->AppInstallService()
      .InstallApp(AppInstallSurface::kAppInstallUriMall, package_id,
                  /*anchor_window=*/std::nullopt, base::DoNothing());

  navigation_observer.Wait();

  EXPECT_THAT(ash::app_install::GetDialogTitle(
                  ash::app_install::GetWebContentsFromDialog()),
              testing::StartsWith("Install app"));
}

IN_PROC_BROWSER_TEST_F(AppInstallServiceAshBrowserTest,
                       InstallWebsiteFindsInstalledWebApp) {
  auto [app_id, package_id] = app_install_server()->SetUpWebsiteResponse();

  web_app::test::InstallDummyWebApp(browser()->profile(), "Test app",
                                    GURL(package_id.identifier()));

  content::TestNavigationObserver navigation_observer(
      (GURL(chrome::kChromeUIAppInstallDialogURL)));
  navigation_observer.StartWatchingNewWebContents();

  AppServiceProxyFactory::GetForProfile(browser()->profile())
      ->AppInstallService()
      .InstallApp(AppInstallSurface::kAppInstallUriMall, package_id,
                  /*anchor_window=*/std::nullopt, base::DoNothing());

  navigation_observer.Wait();

  content::WebContents* web_contents =
      ash::app_install::GetWebContentsFromDialog();

  // The dialog should recognize that the app is already installed, even though
  // the installed app is an "app", and we are requesting to install a
  // "shortcut".
  EXPECT_EQ(ash::app_install::GetDialogTitle(web_contents),
            "App is already installed");
}

class AppInstallServiceAshGuestBrowserTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  static std::string ParamToString(testing::TestParamInfo<bool> param) {
    return param.param ? "Guest" : "NonGuest";
  }

  AppInstallServiceAshGuestBrowserTest() = default;

  bool is_guest() const { return GetParam(); }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);

    if (is_guest()) {
      command_line->AppendSwitch(::switches::kIncognito);
      command_line->AppendSwitch(ash::switches::kGuestSession);
      command_line->AppendSwitchASCII(ash::switches::kLoginProfile, "user");
      command_line->AppendSwitchASCII(ash::switches::kLoginUser,
                                      user_manager::kGuestUserName);
    }
  }
};

INSTANTIATE_TEST_SUITE_P(All,
                         AppInstallServiceAshGuestBrowserTest,
                         testing::Bool(),
                         AppInstallServiceAshGuestBrowserTest::ParamToString);

IN_PROC_BROWSER_TEST_P(AppInstallServiceAshGuestBrowserTest, InstallApp) {
  base::HistogramTester histogram_tester;
  base::RunLoop run_loop;
  AppServiceProxyFactory::GetForProfile(browser()->profile())
      ->AppInstallService()
      .InstallApp(AppInstallSurface::kAppInstallUriUnknown,
                  PackageId(PackageType::kWeb, "test"),
                  /*anchor_window=*/std::nullopt, run_loop.QuitClosure());
  run_loop.Run();

  AppInstallResult expected_result =
      is_guest() ? AppInstallResult::kUserTypeNotPermitted
                 // Check that the install is attempted and not discarded for
                 // the non-guest user. No Almanac mock has been set up so it
                 // will fail at the app data fetching stage.
                 : AppInstallResult::kAlmanacFetchFailed;
  histogram_tester.ExpectUniqueSample("Apps.AppInstallService.AppInstallResult",
                                      expected_result, 1);
  histogram_tester.ExpectUniqueSample(
      "Apps.AppInstallService.AppInstallResult.AppInstallUriUnknown",
      expected_result, 1);
}

}  // namespace apps
