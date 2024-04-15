// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_install/app_install_service_ash.h"

#include <optional>

#include "ash/constants/ash_switches.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/services/app_service/public/cpp/package_id.h"
#include "components/user_manager/user_names.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"

namespace apps {

using AppInstallServiceAshBrowserTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(AppInstallServiceAshBrowserTest,
                       InstallArcAppOpensPlayStore) {
  base::HistogramTester histogram_tester;

  content::TestNavigationObserver navigation_observer(
      GURL("https://play.google.com/store/apps/details?id=com.android.chrome"));
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
  base::HistogramTester histogram_tester;

  content::TestNavigationObserver navigation_observer(
      GURL("https://play.geforcenow.com/games?game-id=test"));
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
