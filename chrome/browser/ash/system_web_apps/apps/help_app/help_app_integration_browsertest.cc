// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <string>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/webui/help_app_ui/buildflags.h"
#include "ash/webui/help_app_ui/help_app_manager.h"
#include "ash/webui/help_app_ui/help_app_manager_factory.h"
#include "ash/webui/help_app_ui/search/search.mojom.h"
#include "ash/webui/help_app_ui/search/search_handler.h"
#include "ash/webui/help_app_ui/url_constants.h"
#include "ash/webui/web_applications/test/sandboxed_web_ui_test_base.h"
#include "base/command_line.h"
#include "base/containers/to_vector.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_install/app_install_service_ash.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/apps/app_service/metrics/app_service_metrics.h"
#include "chrome/browser/ash/first_run/first_run.h"
#include "chrome/browser/ash/release_notes/release_notes_notification.h"
#include "chrome/browser/ash/release_notes/release_notes_storage.h"
#include "chrome/browser/ash/system_web_apps/apps/help_app/help_app_notification_controller.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/ash/system_web_apps/test_support/system_web_app_browsertest_base.h"
#include "chrome/browser/ash/system_web_apps/test_support/system_web_app_integration_test.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/ui/ash/system/system_tray_client_impl.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/webui/ash/system_web_dialog/system_web_dialog_delegate.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/language/core/browser/pref_names.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/package_id.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "components/user_manager/user_names.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/idle/idle.h"
#include "ui/base/idle/scoped_set_idle_state.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/screen.h"
#include "ui/display/types/display_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"
#include "url/url_constants.h"

namespace ash {

namespace {
constexpr char kExploreUpdatesPageUrl[] =
    "chrome://help-app/updates?launchSource=version-update";

class HelpAppIntegrationTest : public SystemWebAppIntegrationTest {
 public:
  HelpAppIntegrationTest()
      : https_server_{std::make_unique<net::EmbeddedTestServer>(
            net::EmbeddedTestServer::TYPE_HTTPS)} {
    scoped_feature_list_.InitWithFeatures(
        {chromeos::features::kUploadOfficeToCloud,
         features::kReleaseNotesNotificationAllChannels,
         features::kHelpAppLauncherSearch},
        {features::kHelpAppOpensInsteadOfReleaseNotesNotification});
    https_server()->AddDefaultHandlers(GetChromeTestDataDir());
  }

  // Setting up our own HTTPS `EmbeddedTestServer` because the superclass's
  // `embedded_test_server()` is HTTP.
  net::EmbeddedTestServer* https_server() { return https_server_.get(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
};

using HelpAppAllProfilesIntegrationTest = HelpAppIntegrationTest;

content::WebContents* GetActiveWebContents() {
  return chrome::FindLastActive()->tab_strip_model()->GetActiveWebContents();
}

class HelpAppIntegrationTestWithAutoTriggerDisabled
    : public HelpAppIntegrationTest {
 public:
  HelpAppIntegrationTestWithAutoTriggerDisabled() {
    scoped_feature_list_.InitAndDisableFeature(
        features::kHelpAppAutoTriggerInstallDialog);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class HelpAppIntegrationTestWithFirstRunEnabled
    : public HelpAppIntegrationTest {
 public:
  HelpAppIntegrationTestWithFirstRunEnabled() = default;

 private:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    HelpAppIntegrationTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(ash::switches::kForceFirstRunUI);
  }
};

class HelpAppIntegrationTestWithHelpAppOpensInsteadOfReleaseNotesNotification
    : public HelpAppIntegrationTest {
 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kHelpAppOpensInsteadOfReleaseNotesNotification};
};

class HelpAppIntegrationTestWithBirchFeatureEnabled
    : public HelpAppIntegrationTest {
 public:
  HelpAppIntegrationTestWithBirchFeatureEnabled() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {features::kHelpAppOpensInsteadOfReleaseNotesNotification,
         features::kForestFeature},
        /*disabled_features=*/{});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace

// Test that the Help App installs and launches correctly. Runs some spot
// checks on the manifest.
IN_PROC_BROWSER_TEST_P(HelpAppIntegrationTest, HelpAppV2) {
  const GURL url(kChromeUIHelpAppURL);
  EXPECT_NO_FATAL_FAILURE(
      ExpectSystemWebAppValid(SystemWebAppType::HELP, url, "Explore"));
}

// Test that the Help App is searchable by additional strings.
IN_PROC_BROWSER_TEST_P(HelpAppIntegrationTest, HelpAppV2SearchInLauncher) {
  WaitForTestSystemAppInstall();
  auto* system_app = GetManager().GetSystemApp(SystemWebAppType::HELP);
  EXPECT_THAT(base::ToVector(system_app->GetAdditionalSearchTerms(),
                             &l10n_util::GetStringUTF8),
              testing::ElementsAre("Get Help", "Perks", "Offers"));
}

// Test that the Help App has a minimum window size of 600x320.
IN_PROC_BROWSER_TEST_P(HelpAppIntegrationTest, HelpAppV2MinWindowSize) {
  WaitForTestSystemAppInstall();
  auto* system_app = GetManager().GetSystemApp(SystemWebAppType::HELP);
  EXPECT_EQ(system_app->GetMinimumWindowSize(), gfx::Size(600, 320));
}

// Test that the Help App has a default size of 960x600 and is in the center of
// the screen.
IN_PROC_BROWSER_TEST_P(HelpAppIntegrationTest, HelpAppV2DefaultWindowBounds) {
  WaitForTestSystemAppInstall();
  Browser* browser;
  LaunchApp(SystemWebAppType::HELP, &browser);
  gfx::Rect work_area =
      display::Screen::GetScreen()->GetDisplayForNewWindows().work_area();
  int x = (work_area.width() - 960) / 2;
  int y = (work_area.height() - 600) / 2;
  EXPECT_EQ(browser->window()->GetBounds(), gfx::Rect(x, y, 960, 600));
}

// Test that the Help App logs metric when launching the app using the
// AppServiceProxy.
IN_PROC_BROWSER_TEST_P(HelpAppIntegrationTest, HelpAppV2AppServiceMetrics) {
  WaitForTestSystemAppInstall();
  base::HistogramTester histogram_tester;

  // Using AppServiceProxy gives more coverage of the launch path and ensures
  // the metric is not recorded twice.
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile());
  content::TestNavigationObserver navigation_observer(
      GURL("chrome://help-app/"));
  navigation_observer.StartWatchingNewWebContents();

  proxy->Launch(*GetManager().GetAppIdForSystemApp(SystemWebAppType::HELP),
                ui::EF_NONE, apps::LaunchSource::kFromKeyboard,
                std::make_unique<apps::WindowInfo>(display::kDefaultDisplayId));

  navigation_observer.Wait();
  histogram_tester.ExpectUniqueSample("Apps.DefaultAppLaunch.FromKeyboard",
                                      apps::DefaultAppName::kHelpApp, 1);
  histogram_tester.ExpectUniqueSample("Discover.Overall.AppLaunched",
                                      apps::LaunchSource::kFromKeyboard, 1);
}

// Test that the Help App can log metrics in the untrusted frame.
IN_PROC_BROWSER_TEST_P(HelpAppIntegrationTest, HelpAppV2InAppMetrics) {
  WaitForTestSystemAppInstall();
  content::WebContents* web_contents = LaunchApp(SystemWebAppType::HELP);

  base::UserActionTester user_action_tester;

  constexpr char kScript[] = R"(
    chrome.metricsPrivate.recordUserAction("Discover.Help.TabClicked");
  )";

  EXPECT_EQ(0, user_action_tester.GetActionCount("Discover.Help.TabClicked"));
  EXPECT_EQ(nullptr,
            SandboxedWebUiAppTestBase::EvalJsInAppFrame(web_contents, kScript));
  EXPECT_EQ(1, user_action_tester.GetActionCount("Discover.Help.TabClicked"));
}

IN_PROC_BROWSER_TEST_P(HelpAppIntegrationTest,
                       HasCorrectThemeAndBackgroundColor) {
  WaitForTestSystemAppInstall();
  webapps::AppId app_id =
      *GetManager().GetAppIdForSystemApp(SystemWebAppType::HELP);
  web_app::WebAppRegistrar& registrar =
      web_app::WebAppProvider::GetForTest(profile())->registrar_unsafe();

  EXPECT_EQ(registrar.GetAppThemeColor(app_id), SK_ColorWHITE);
  EXPECT_EQ(registrar.GetAppBackgroundColor(app_id), SK_ColorWHITE);
  EXPECT_EQ(registrar.GetAppDarkModeThemeColor(app_id), gfx::kGoogleGrey900);
  EXPECT_EQ(registrar.GetAppDarkModeBackgroundColor(app_id),
            gfx::kGoogleGrey900);
}

IN_PROC_BROWSER_TEST_P(HelpAppAllProfilesIntegrationTest, HelpAppV2ShowHelp) {
  WaitForTestSystemAppInstall();

  GURL expected_url = GURL("chrome://help-app/");
  content::TestNavigationObserver navigation_observer(expected_url);
  navigation_observer.StartWatchingNewWebContents();

  chrome::ShowHelp(browser(), chrome::HELP_SOURCE_KEYBOARD);

#if BUILDFLAG(ENABLE_CROS_HELP_APP)
  EXPECT_NO_FATAL_FAILURE(navigation_observer.Wait());
  // Help app should have opened at the expected page.
  EXPECT_EQ(expected_url, GetActiveWebContents()->GetVisibleURL());
#else
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(GURL(chrome::kChromeHelpViaKeyboardURL),
            GetActiveWebContents()->GetVisibleURL());
#endif
}

// Test that first run experience opens Help App with launch source query param.
IN_PROC_BROWSER_TEST_P(HelpAppIntegrationTestWithFirstRunEnabled,
                       HelpAppV2FirstRunLaunch) {
  WaitForTestSystemAppInstall();
  base::HistogramTester histogram_tester;
  GURL expected_trusted_frame_url =
      GURL("chrome://help-app?launchSource=first-run");
  content::TestNavigationObserver navigation_observer(
      expected_trusted_frame_url);
  navigation_observer.StartWatchingNewWebContents();

  // Then call the launch method used by the first run experience.
  ash::first_run::LaunchHelpApp(profile());

#if BUILDFLAG(ENABLE_CROS_HELP_APP)
  EXPECT_NO_FATAL_FAILURE(navigation_observer.Wait());
  // The Help app trusted frame should have opened at the expected page.
  EXPECT_EQ(expected_trusted_frame_url,
            GetActiveWebContents()->GetVisibleURL());

  // The Help app untrusted frame should contain the same query param.
  EXPECT_EQ("chrome-untrusted://help-app/?launchSource=first-run",
            SandboxedWebUiAppTestBase::EvalJsInAppFrame(
                GetActiveWebContents(), "window.location.href"));

  histogram_tester.ExpectUniqueSample("Apps.DefaultAppLaunch.FromFirstRun",
                                      apps::DefaultAppName::kHelpApp, 1);
  histogram_tester.ExpectUniqueSample("Discover.Overall.AppLaunched",
                                      apps::LaunchSource::kFromFirstRun, 1);
#else
  // We just have the original browser. No new app opens.
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  histogram_tester.ExpectUniqueSample("Apps.DefaultAppLaunch.FromFirstRun",
                                      apps::DefaultAppName::kHelpApp, 0);
  histogram_tester.ExpectUniqueSample("Discover.Overall.AppLaunched",
                                      apps::LaunchSource::kFromFirstRun, 0);
#endif
}

// Test that launching the Help App's release notes opens the app on the Release
// Notes page.
IN_PROC_BROWSER_TEST_P(HelpAppAllProfilesIntegrationTest,
                       HelpAppV2LaunchReleaseNotes) {
  WaitForTestSystemAppInstall();

  // There should be 1 browser window initially.
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());

  const GURL expected_url("chrome://help-app/updates");
  content::TestNavigationObserver navigation_observer(expected_url);
  navigation_observer.StartWatchingNewWebContents();

  chrome::LaunchReleaseNotes(profile(), apps::LaunchSource::kFromOtherApp);
#if BUILDFLAG(ENABLE_CROS_HELP_APP)
  // If no navigation happens, then this test will time out due to the wait.
  navigation_observer.Wait();

  // There should be two browser windows, one regular and one for the newly
  // opened app.
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());

  // The opened window should be showing the url with attached WebUI.
  // The inner frame should be the pathname for the release notes pathname.
  EXPECT_EQ("chrome-untrusted://help-app/updates",
            SandboxedWebUiAppTestBase::EvalJsInAppFrame(
                GetActiveWebContents(), "window.location.href"));
#else
  // Nothing should happen on non-branded builds.
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
#endif
}

// Test that launching the Help App's release notes logs metrics.
IN_PROC_BROWSER_TEST_P(HelpAppIntegrationTest, HelpAppV2ReleaseNotesMetrics) {
  WaitForTestSystemAppInstall();

  const GURL expected_url("chrome://help-app/updates");
  content::TestNavigationObserver navigation_observer(expected_url);
  navigation_observer.StartWatchingNewWebContents();

  base::UserActionTester user_action_tester;
  chrome::LaunchReleaseNotes(profile(), apps::LaunchSource::kFromOtherApp);
#if BUILDFLAG(ENABLE_CROS_HELP_APP)
  // If no navigation happens, then this test will time out due to the wait.
  navigation_observer.Wait();

  EXPECT_EQ(1,
            user_action_tester.GetActionCount("ReleaseNotes.ShowReleaseNotes"));
#else
  EXPECT_EQ(0,
            user_action_tester.GetActionCount("ReleaseNotes.ShowReleaseNotes"));
#endif
}

// Test that clicking the release notes notification opens Help App.
IN_PROC_BROWSER_TEST_P(HelpAppIntegrationTest,
                       HelpAppV2LaunchReleaseNotesFromNotification) {
  // TODO(http://b/349164737): Re-enable this test with forest feature enabled.
  if (ash::features::IsForestFeatureEnabled()) {
    GTEST_SKIP() << "Skipping test body for Forest Feature.";
  }

  WaitForTestSystemAppInstall();
  base::UserActionTester user_action_tester;
  auto display_service =
      std::make_unique<NotificationDisplayServiceTester>(/*profile=*/nullptr);
  auto release_notes_notification =
      std::make_unique<ReleaseNotesNotification>(profile());
  auto release_notes_storage = std::make_unique<ReleaseNotesStorage>(profile());

  // Force the release notes notification to show up.
  profile()->GetPrefs()->SetInteger(
      prefs::kHelpAppNotificationLastShownMilestone, 20);
  release_notes_notification->MaybeShowReleaseNotes();
  // Assert that the notification really is there.
  auto notifications = display_service->GetDisplayedNotificationsForType(
      NotificationHandler::Type::TRANSIENT);
  ASSERT_EQ(1u, notifications.size());
  ASSERT_EQ("show_release_notes_notification", notifications[0].id());

  GURL expected_url = GURL("chrome://help-app/updates");
  content::TestNavigationObserver navigation_observer(expected_url);
  navigation_observer.StartWatchingNewWebContents();
  // Then click.
  display_service->SimulateClick(NotificationHandler::Type::TRANSIENT,
                                 "show_release_notes_notification",
                                 std::nullopt, std::nullopt);

  EXPECT_EQ(
      1, user_action_tester.GetActionCount("ReleaseNotes.NotificationShown"));
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "ReleaseNotes.LaunchedNotification"));
#if BUILDFLAG(ENABLE_CROS_HELP_APP)
  EXPECT_NO_FATAL_FAILURE(navigation_observer.Wait());
  // Help app should have opened at the expected page.
  EXPECT_EQ(expected_url, GetActiveWebContents()->GetVisibleURL());
  EXPECT_EQ(1,
            user_action_tester.GetActionCount("ReleaseNotes.ShowReleaseNotes"));
#else
  // We just have the original browser. No new app opens.
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(0,
            user_action_tester.GetActionCount("ReleaseNotes.ShowReleaseNotes"));
#endif
}

// Test that the background page can trigger the release notes notification.
IN_PROC_BROWSER_TEST_P(HelpAppIntegrationTest,
                       HelpAppV2ReleaseNotesNotificationFromBackground) {
  WaitForTestSystemAppInstall();
  content::WebContents* web_contents = LaunchApp(SystemWebAppType::HELP);
  auto display_service =
      std::make_unique<NotificationDisplayServiceTester>(/*profile=*/nullptr);
  base::UserActionTester user_action_tester;
  profile()->GetPrefs()->SetInteger(
      prefs::kHelpAppNotificationLastShownMilestone, 20);
  EXPECT_EQ(profile()->GetPrefs()->GetInteger(
                prefs::kReleaseNotesSuggestionChipTimesLeftToShow),
            0);

  // Script that simulates what the Help App background page would do to show
  // the release notes notification.
  constexpr char kScript[] = R"(
    (async () => {
      const delegate = window.customLaunchData.delegate;
      await delegate.maybeShowReleaseNotesNotification();
      return true;
    })();
  )";
  // Use EvalJs instead of EvalJsInAppFrame because the script needs to
  // run in the same world as the page's code.
  EXPECT_EQ(true,
            content::EvalJs(
                SandboxedWebUiAppTestBase::GetAppFrame(web_contents), kScript));
  if (features::IsForestFeatureEnabled()) {
    EXPECT_EQ(profile()->GetPrefs()->GetInteger(
                  prefs::kReleaseNotesSuggestionChipTimesLeftToShow),
              0);
  } else {
    EXPECT_EQ(profile()->GetPrefs()->GetInteger(
                  prefs::kReleaseNotesSuggestionChipTimesLeftToShow),
              3);
  }

  Browser* browser = chrome::FindBrowserWithTab(web_contents);
  // Close the web contents we just created to simulate what would happen in
  // production with a background page. This helps us ensure that our
  // notification shows up and can be interacted with even after the web ui
  // that triggered it has died.
  web_contents->Close();
  // Wait until the browser with the web contents closes.
  ui_test_utils::WaitForBrowserToClose(browser);
  // Assert that the notification really is there.
  auto notifications = display_service->GetDisplayedNotificationsForType(
      NotificationHandler::Type::TRANSIENT);
  if (features::IsForestFeatureEnabled()) {
    ASSERT_EQ(0u, notifications.size());
  } else {
    ASSERT_EQ(1u, notifications.size());
    ASSERT_EQ("show_release_notes_notification", notifications[0].id());
  }
  // Click on the notification.
  GURL expected_url = GURL("chrome://help-app/updates");
  content::TestNavigationObserver navigation_observer(expected_url);
  navigation_observer.StartWatchingNewWebContents();
  display_service->SimulateClick(NotificationHandler::Type::TRANSIENT,
                                 "show_release_notes_notification",
                                 std::nullopt, std::nullopt);
#if BUILDFLAG(ENABLE_CROS_HELP_APP)
  if (!features::IsForestFeatureEnabled()) {
    EXPECT_NO_FATAL_FAILURE(navigation_observer.Wait());
    EXPECT_EQ(expected_url, GetActiveWebContents()->GetVisibleURL());
  }
#else
  // We just have the original browser. No new app opens.
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
#endif
}

IN_PROC_BROWSER_TEST_P(
    HelpAppIntegrationTestWithHelpAppOpensInsteadOfReleaseNotesNotification,
    OpensHelpApp) {
  WaitForTestSystemAppInstall();
  base::HistogramTester histogram_tester;
  GURL expected_trusted_frame_url = GURL(kExploreUpdatesPageUrl);
  content::TestNavigationObserver navigation_observer(
      expected_trusted_frame_url);
  navigation_observer.StartWatchingNewWebContents();
  auto display_service =
      std::make_unique<NotificationDisplayServiceTester>(/*profile=*/nullptr);

  profile()->GetPrefs()->SetInteger(
      prefs::kHelpAppNotificationLastShownMilestone, 20);
  std::make_unique<HelpAppNotificationController>(profile())
      ->MaybeShowReleaseNotesNotification();

  // The release notes notification should not appear.
  auto notifications = display_service->GetDisplayedNotificationsForType(
      NotificationHandler::Type::TRANSIENT);
  EXPECT_EQ(0u, notifications.size());
  // The release notes suggestion chip should not appear.
  EXPECT_EQ(profile()->GetPrefs()->GetInteger(
                prefs::kReleaseNotesSuggestionChipTimesLeftToShow),
            0);
#if BUILDFLAG(ENABLE_CROS_HELP_APP)
  if (!features::IsForestFeatureEnabled()) {
    EXPECT_NO_FATAL_FAILURE(navigation_observer.Wait());
    EXPECT_EQ(expected_trusted_frame_url,
              GetActiveWebContents()->GetVisibleURL());
    histogram_tester.ExpectUniqueSample("Discover.Overall.AppLaunched",
                                        apps::LaunchSource::kFromOsLogin, 1);
  }
#else
  // We just have the original browser. No new app opens.
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  histogram_tester.ExpectUniqueSample("Discover.Overall.AppLaunched",
                                      apps::LaunchSource::kFromOsLogin, 0);
#endif
}

IN_PROC_BROWSER_TEST_P(HelpAppIntegrationTestWithBirchFeatureEnabled,
                       HelpAppRemainsClosed) {
  WaitForTestSystemAppInstall();
  base::HistogramTester histogram_tester;
  GURL expected_trusted_frame_url = GURL(kExploreUpdatesPageUrl);
  content::TestNavigationObserver navigation_observer(
      expected_trusted_frame_url);
  navigation_observer.StartWatchingNewWebContents();
  auto display_service =
      std::make_unique<NotificationDisplayServiceTester>(/*profile=*/nullptr);

  profile()->GetPrefs()->SetInteger(
      prefs::kHelpAppNotificationLastShownMilestone, 20);
  std::make_unique<HelpAppNotificationController>(profile())
      ->MaybeShowReleaseNotesNotification();

  // The release notes notification should not appear.
  auto notifications = display_service->GetDisplayedNotificationsForType(
      NotificationHandler::Type::TRANSIENT);
  EXPECT_EQ(0u, notifications.size());
  // The release notes suggestion chip should not appear.
  EXPECT_EQ(profile()->GetPrefs()->GetInteger(
                prefs::kReleaseNotesSuggestionChipTimesLeftToShow),
            0);
#if BUILDFLAG(ENABLE_CROS_HELP_APP)
  // No new app should open because of birch flag.
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  histogram_tester.ExpectUniqueSample(
      "Discover.Overall.AppLaunched",
      apps::LaunchSource::kFromReleaseNotesNotification, 0);
#else
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  histogram_tester.ExpectUniqueSample(
      "Discover.Overall.AppLaunched",
      apps::LaunchSource::kFromReleaseNotesNotification, 0);
#endif
}

// Test that the Help App does a navigation on launch even when it was already
// open with the same URL.
IN_PROC_BROWSER_TEST_P(HelpAppIntegrationTest, HelpAppV2NavigateOnRelaunch) {
  WaitForTestSystemAppInstall();

  // There should initially be a single browser window.
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());

  Browser* browser;
  content::WebContents* web_contents =
      LaunchApp(SystemWebAppType::HELP, &browser);

  // There should be two browser windows, one regular and one for the newly
  // opened app.
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());

  content::TestNavigationObserver navigation_observer(web_contents);
  LaunchAppWithoutWaiting(SystemWebAppType::HELP);
  // If no navigation happens, then this test will time out due to the wait.
  navigation_observer.Wait();

  // LaunchApp should navigate the existing window and not open any new windows.
  EXPECT_EQ(browser, chrome::FindLastActive());
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());
}

// Test direct navigation to a subpage.
IN_PROC_BROWSER_TEST_P(HelpAppIntegrationTest, HelpAppV2DirectNavigation) {
  WaitForTestSystemAppInstall();
  auto params = LaunchParamsForApp(SystemWebAppType::HELP);
  params.override_url = GURL("chrome://help-app/help/");

  content::WebContents* web_contents = LaunchApp(std::move(params));

  // The inner frame should have the same pathname as the launch URL.
  EXPECT_EQ("chrome-untrusted://help-app/help/",
            SandboxedWebUiAppTestBase::EvalJsInAppFrame(
                web_contents, "window.location.href"));
}

// Test that the Help App can open the feedback dialog.
//
// Flaky on Linux Chromium OS ASan LSan Tests (1)
//
// TODO(crbug.com/40940376): Reenable it.
IN_PROC_BROWSER_TEST_P(HelpAppIntegrationTest,
                       DISABLED_HelpAppV2OpenFeedbackDialog) {
  WaitForTestSystemAppInstall();
  content::WebContents* web_contents = LaunchApp(SystemWebAppType::HELP);

  // Script that tells the Help App to open the feedback dialog.
  constexpr char kScript[] = R"(
    (async () => {
      const res = await window.customLaunchData.delegate.openFeedbackDialog();
      return res === null;
    })();
  )";
  // A null string result means no error in opening feedback.
  EXPECT_EQ(true,
            content::EvalJs(
                SandboxedWebUiAppTestBase::GetAppFrame(web_contents), kScript));
}

// Test that the Help App can open the on device app controls part section in OS
// Settings.
IN_PROC_BROWSER_TEST_P(HelpAppIntegrationTest,
                       HelpAppV2ShowOnDeviceAppControls) {
  WaitForTestSystemAppInstall();
  content::WebContents* web_contents = LaunchApp(SystemWebAppType::HELP);

  // There should be two browser windows, one regular and one for the newly
  // opened help app.
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());

  const GURL expected_url("chrome://os-settings/apps");
  content::TestNavigationObserver navigation_observer(expected_url);
  navigation_observer.StartWatchingNewWebContents();

  // Script that tells the Help App to show on device app controls.
  constexpr char kScript[] = R"(
    (async () => {
      await window.customLaunchData.delegate.showOnDeviceAppControls();
    })();
  )";
  // Trigger the script, then wait for settings to open. Use ExecJs
  // instead of EvalJsInAppFrame because the script needs to run in the same
  // world as the page's code.
  EXPECT_TRUE(content::ExecJs(
      SandboxedWebUiAppTestBase::GetAppFrame(web_contents), kScript));
  navigation_observer.Wait();

  // Settings should be active in a new window.
  EXPECT_EQ(3u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(expected_url, GetActiveWebContents()->GetVisibleURL());
}

// Test that the Help App opens the OS Settings family link page.
IN_PROC_BROWSER_TEST_P(HelpAppIntegrationTest, HelpAppV2ShowParentalControls) {
  WaitForTestSystemAppInstall();
  content::WebContents* web_contents = LaunchApp(SystemWebAppType::HELP);

  // There should be two browser windows, one regular and one for the newly
  // opened help app.
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());

  const GURL expected_url("chrome://os-settings/osPeople");
  content::TestNavigationObserver navigation_observer(expected_url);
  navigation_observer.StartWatchingNewWebContents();

  // Script that tells the Help App to show parental controls.
  constexpr char kScript[] = R"(
    (async () => {
      await window.customLaunchData.delegate.showParentalControls();
    })();
  )";
  // Trigger the script, then wait for settings to open. Use ExecJs
  // instead of EvalJsInAppFrame because the script needs to run in the same
  // world as the page's code.
  EXPECT_TRUE(content::ExecJs(
      SandboxedWebUiAppTestBase::GetAppFrame(web_contents), kScript));
  navigation_observer.Wait();

  // Settings should be active in a new window.
  EXPECT_EQ(3u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(expected_url, GetActiveWebContents()->GetVisibleURL());
}

// Test that the Help App's `openUrlInBrowserAndTriggerInstallDialog` can open
// valid URLs if the `kHelpAppAutoTriggerInstallDialog` feature is disabled.
IN_PROC_BROWSER_TEST_P(HelpAppIntegrationTestWithAutoTriggerDisabled,
                       HelpAppV2CanOpenValidHttpsUrlsInBrowser) {
  ASSERT_TRUE(https_server()->Start());
  const GURL test_url = https_server()->GetURL("/title1.html");

  ASSERT_TRUE(test_url.SchemeIs(url::kHttpsScheme));

  // There should be only be one regular browser with one tab.
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(1, browser()->tab_strip_model()->GetTabCount());

  WaitForTestSystemAppInstall();
  content::WebContents* web_contents = LaunchApp(SystemWebAppType::HELP);

  // There should be two browser windows, one regular and one for the newly
  // opened help app.
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());

  content::TestNavigationObserver navigation_observer(test_url);
  navigation_observer.StartWatchingNewWebContents();

  // Script that tells the Help App to call the
  // openUrlInBrowserAndTriggerInstallDialog Mojo function.
  constexpr char kScript[] = R"(
    (async () => {
      const delegate = window.customLaunchData.delegate;
      await delegate.openUrlInBrowserAndTriggerInstallDialog($1);
    })();
  )";
  // Trigger the script, then wait for the URL to open in a new tab. Use
  // ExecJs instead of EvalJsInAppFrame because the script needs to run
  // in the same world as the page's code.
  EXPECT_TRUE(
      content::ExecJs(SandboxedWebUiAppTestBase::GetAppFrame(web_contents),
                      content::JsReplace(kScript, test_url),
                      content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));
  navigation_observer.Wait();

  // There should still be two browser windows.
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());
  // The regular browser should only have 2 tabs.
  EXPECT_EQ(2, browser()->tab_strip_model()->GetTabCount());
  // After opening the URL, the regular browser should be the most recently
  // active browser.
  EXPECT_EQ(browser(), chrome::FindLastActive());
  // The active tab should be the `test_url` we opened.
  EXPECT_EQ(test_url, GetActiveWebContents()->GetVisibleURL());
}

// Test that the Help App's `openUrlInBrowserAndTriggerInstallDialog` navigates
// and triggers the install dialog by default.
IN_PROC_BROWSER_TEST_P(HelpAppIntegrationTest,
                       HelpAppV2CanTriggerInstallDialogForValidHttpsUrls) {
  ASSERT_TRUE(https_server()->Start());
  const GURL test_url =
      https_server()->GetURL("/banners/manifest_test_page.html");

  ASSERT_TRUE(test_url.SchemeIs(url::kHttpsScheme));

  // There should be only be one regular browser with one tab.
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(1, browser()->tab_strip_model()->GetTabCount());

  WaitForTestSystemAppInstall();
  content::WebContents* web_contents = LaunchApp(SystemWebAppType::HELP);

  // There should be two browser windows, one regular and one for the newly
  // opened help app.
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());

  content::TestNavigationObserver navigation_observer(test_url);
  navigation_observer.StartWatchingNewWebContents();

  std::string dialog_name =
      base::FeatureList::IsEnabled(::features::kWebAppUniversalInstall)
          ? "WebAppSimpleInstallDialog"
          : "PWAConfirmationBubbleView";
  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey(),
                                       dialog_name);

  // Script that tells the Help App to call the
  // OpenUrlInBrowserAndTriggerInstallDialog Mojo function.
  constexpr char kScript[] = R"(
    (async () => {
      const delegate = window.customLaunchData.delegate;
      await delegate.openUrlInBrowserAndTriggerInstallDialog($1);
    })();
  )";
  // Trigger the script, then wait for the URL to open in a new tab. Use
  // ExecJs instead of EvalJsInAppFrame because the script needs to run in the
  // same world as the page's code.
  EXPECT_TRUE(
      content::ExecJs(SandboxedWebUiAppTestBase::GetAppFrame(web_contents),
                      content::JsReplace(kScript, test_url)));
  navigation_observer.Wait();

  // There should still be two browser windows.
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());
  // The regular browser should only have 2 tabs.
  EXPECT_EQ(2, browser()->tab_strip_model()->GetTabCount());
  // After opening the URL, the regular browser should be the most recently
  // active browser.
  EXPECT_EQ(browser(), chrome::FindLastActive());
  // The active tab should be the `test_url` we opened.
  EXPECT_EQ(test_url, GetActiveWebContents()->GetVisibleURL());

  // Wait for the PWA install dialog to show up.
  views::Widget* widget = waiter.WaitIfNeededAndGet();
  ASSERT_NE(widget, nullptr);
}

// Test that the Help App's `openUrlInBrowserAndTriggerInstallDialog` crashes
// for invalid URLs.
IN_PROC_BROWSER_TEST_P(HelpAppIntegrationTest,
                       HelpAppV2CrashesForInvalidUrlsInBrowser) {
  // There should be only be one regular browser with one tab.
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  // The regular browser should only have 1 tab.
  EXPECT_EQ(1, browser()->tab_strip_model()->GetTabCount());
  // The tab should be the default "about:blank" URL.
  EXPECT_TRUE(GetActiveWebContents()->GetVisibleURL().IsAboutBlank());

  WaitForTestSystemAppInstall();

  // Script that tells the Help App to call the
  // `OpenUrlInBrowserAndTriggerInstallDialog` Mojo function.
  constexpr char kScript[] = R"(
    (async () => {
      const delegate = window.customLaunchData.delegate;
      await delegate.openUrlInBrowserAndTriggerInstallDialog($1);
    })();
  )";
  std::string invalid_urls[] = {"",
                                "test",
                                "www.test.com",
                                "http://test.com",
                                "data:,Hello%2C%20World%21",
                                "file:///home/foo.html",
                                "javascript:alert('Hello World')"};
  for (const std::string& test_url : invalid_urls) {
    // Launch a new Help app window per test URL.
    Browser* help_app_browser;
    content::WebContents* web_contents =
        LaunchApp(SystemWebAppType::HELP, &help_app_browser);

    // There should be two browser windows, one regular and one for the newly
    // opened help app.
    EXPECT_EQ(2u, chrome::GetTotalBrowserCount());
    auto* frame = SandboxedWebUiAppTestBase::GetAppFrame(web_contents);

    // Test that calls with invalid URLs crash the renderer process.
    {
      content::ScopedAllowRendererCrashes scoped_allow_renderer_crashes;
      content::RenderFrameDeletedObserver crash_observer(frame);

      content::ExecuteScriptAsync(frame, content::JsReplace(kScript, test_url));

      crash_observer.WaitUntilDeleted();
    }
    EXPECT_TRUE(web_contents->IsCrashed());

    // The Help app renderer process crashed. Close the browser window so that
    // we can relaunch it in another browser window.
    chrome::CloseWindow(help_app_browser);
    ui_test_utils::WaitForBrowserToClose(help_app_browser);

    // There should only be 1 regular browser.
    EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
    // The regular browser should still only have 1 tab.
    EXPECT_EQ(1, browser()->tab_strip_model()->GetTabCount());
    // The tab should still be the default "about:blank" URL.
    EXPECT_TRUE(GetActiveWebContents()->GetVisibleURL().IsAboutBlank());
  }
}

// Test that the Help App delegate can open the Microsoft 365 setup flow.
IN_PROC_BROWSER_TEST_P(HelpAppIntegrationTest, HelpAppV2CanOpenMS365Setup) {
  WaitForTestSystemAppInstall();
  content::WebContents* web_contents = LaunchApp(SystemWebAppType::HELP);

  // Script that tells the Help App to call the `LaunchMicrosoft365Setup` Mojo
  // function.
  constexpr char kScript[] = R"(
    (async () => {
      const delegate = window.customLaunchData.delegate;
      await delegate.launchMicrosoft365Setup();
      return true;
    })();
  )";
  EXPECT_EQ(true,
            content::EvalJs(
                SandboxedWebUiAppTestBase::GetAppFrame(web_contents), kScript));

  ash::SystemWebDialogDelegate* dialog =
      ash::SystemWebDialogDelegate::FindInstance(
          chrome::kChromeUICloudUploadURL);
  EXPECT_TRUE(dialog);
}

// Test that the Help App delegate can update the index for launcher search.
// Also test searching using the help app search handler.
IN_PROC_BROWSER_TEST_P(HelpAppIntegrationTest,
                       HelpAppV2UpdateLauncherSearchIndexAndSearch) {
  WaitForTestSystemAppInstall();
  content::WebContents* web_contents = LaunchApp(SystemWebAppType::HELP);

  // Script that adds a data item to the launcher search index.
  constexpr char kScript[] = R"(
    (async () => {
      const delegate = window.customLaunchData.delegate;
      await delegate.updateLauncherSearchIndex([{
        id: 'test-id',
        title: 'Title',
        mainCategoryName: 'Help',
        tags: ['verycomplicatedsearchquery'],
        urlPathWithParameters: 'help/sub/3399763/id/6318213',
        locale: ''
      }]);
      return true;
    })();
  )";

  // Use ExtractBool to make the script wait until the update completes.
  EXPECT_EQ(true,
            content::EvalJs(
                SandboxedWebUiAppTestBase::GetAppFrame(web_contents), kScript));

  // Search using the search handler to confirm that the update happened.
  base::RunLoop run_loop;
  help_app::HelpAppManagerFactory::GetForBrowserContext(profile())
      ->search_handler()
      ->Search(u"verycomplicatedsearchquery",
               /*max_num_results=*/3u,
               base::BindLambdaForTesting(
                   [&](std::vector<help_app::mojom::SearchResultPtr>
                           search_results) {
                     EXPECT_EQ(search_results.size(), 1u);
                     EXPECT_EQ(search_results[0]->id, "test-id");
                     EXPECT_EQ(search_results[0]->title, u"Title");
                     EXPECT_EQ(search_results[0]->main_category, u"Help");
                     EXPECT_EQ(search_results[0]->locale, "");
                     EXPECT_GT(search_results[0]->relevance_score, 0.01);
                     run_loop.QuitClosure().Run();
                   }));
  run_loop.Run();
}

// Test that the Help App delegate filters out invalid search concepts when
// updating the launcher search index.
IN_PROC_BROWSER_TEST_P(HelpAppIntegrationTest,
                       HelpAppV2UpdateLauncherSearchIndexFilterInvalid) {
  WaitForTestSystemAppInstall();
  base::HistogramTester histogram_tester;
  content::WebContents* web_contents = LaunchApp(SystemWebAppType::HELP);

  // Script that adds a data item to the launcher search index.
  constexpr char kScript[] = R"(
    (async () => {
      const delegate = window.customLaunchData.delegate;
      await delegate.updateLauncherSearchIndex([
        {
          id: '6318213',  // Fix connection problems.
          title: 'Article 1: Invalid',
          mainCategoryName: 'Help',
          tags: ['verycomplicatedsearchquery'],
          urlPathWithParameters: '',  // Invalid because empty field.
          locale: '',
        },
        {
          id: 'test-id-2',
          title: 'Article 2: Valid',
          mainCategoryName: 'Help',
          tags: ['verycomplicatedsearchquery'],
          urlPathWithParameters: 'help/',
          locale: '',
        },
        {
          id: '1700055',  // Open, save, or delete files.
          title: 'Article 3: Invalid',
          mainCategoryName: 'Help',
          tags: [''],  // Invalid because no non-empty tags.
          urlPathWithParameters: 'help/',
          locale: '',
        },
      ]);
      return true;
    })();
  )";

  // Use ExtractBool to make the script wait until the update completes.
  EXPECT_EQ(true,
            content::EvalJs(
                SandboxedWebUiAppTestBase::GetAppFrame(web_contents), kScript));

  // These hash values can be found in the enum in the google-internal histogram
  // file.
  histogram_tester.ExpectBucketCount(
      "Discover.LauncherSearch.InvalidConceptInUpdate", -20424143, 1);
  histogram_tester.ExpectBucketCount(
      "Discover.LauncherSearch.InvalidConceptInUpdate", 395626524, 1);

  // Search using the search handler to confirm that only the valid article was
  // added to the index.
  base::RunLoop run_loop;
  help_app::HelpAppManagerFactory::GetForBrowserContext(profile())
      ->search_handler()
      ->Search(u"verycomplicatedsearchquery",
               /*max_num_results=*/3u,
               base::BindLambdaForTesting(
                   [&](std::vector<help_app::mojom::SearchResultPtr>
                           search_results) {
                     EXPECT_EQ(search_results.size(), 1u);
                     EXPECT_EQ(search_results[0]->id, "test-id-2");
                     run_loop.QuitClosure().Run();
                   }));
  run_loop.Run();
}

// Test that the Help App background task works.
// It should open and update the index for launcher search, then close.
IN_PROC_BROWSER_TEST_P(HelpAppIntegrationTest,
                       HelpAppV2BackgroundTaskUpdatesLauncherSearchIndex) {
  WaitForTestSystemAppInstall();
  ui::ScopedSetIdleState idle(ui::IDLE_STATE_IDLE);

  const GURL bg_task_url("chrome://help-app/background");
  content::TestNavigationObserver navigation_observer(bg_task_url);
  navigation_observer.StartWatchingNewWebContents();

  // Wait for system apps background tasks to start.
  base::RunLoop run_loop;
  SystemWebAppManager::GetForTest(browser()->profile())
      ->on_tasks_started()
      .Post(FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();

  auto& tasks = GetManager().GetBackgroundTasksForTesting();

  // Find the help app's background task.
  const auto& help_task = base::ranges::find(
      tasks, bg_task_url, &SystemWebAppBackgroundTask::url_for_testing);
  ASSERT_NE(help_task, tasks.end());

  auto* timer = help_task->get()->get_timer_for_testing();
  EXPECT_EQ(SystemWebAppBackgroundTask::INITIAL_WAIT,
            help_task->get()->get_state_for_testing());
  // The "Immediate" timer waits for several minutes, and it's hard to mock time
  // properly in a browser test, so just fire the timer now. We're not testing
  // that base::Timer works.
  timer->FireNow();

  // Wait for the task to launch the background page.
  navigation_observer.Wait();

  // Store web_content while the page is open.
  content::WebContents* web_contents =
      help_task->get()->web_contents_for_testing();
  // Wait until the background page closes.
  content::WebContentsDestroyedWatcher destroyed_watcher(web_contents);
  destroyed_watcher.Wait();

  EXPECT_EQ(help_task->get()->opened_count_for_testing(), 1u);

#if !BUILDFLAG(ENABLE_CROS_HELP_APP)
  // This part only works in non-branded builds because it uses fake data added
  // by the mock app.
  // Search using the search handler to confirm that the update happened.
  base::RunLoop search_run_loop;
  help_app::HelpAppManagerFactory::GetForBrowserContext(profile())
      ->search_handler()
      ->Search(u"verycomplicatedsearchquery",
               /*max_num_results=*/1u,
               base::BindLambdaForTesting(
                   [&](std::vector<help_app::mojom::SearchResultPtr>
                           search_results) {
                     ASSERT_EQ(search_results.size(), 1u);
                     EXPECT_EQ(search_results[0]->id, "mock-app-test-id");
                     EXPECT_EQ(search_results[0]->title, u"Title");
                     EXPECT_EQ(search_results[0]->main_category, u"Help");
                     EXPECT_EQ(search_results[0]->locale, "");
                     EXPECT_EQ(search_results[0]->url_path_with_parameters,
                               "help/sub/3399763/");
                     EXPECT_GT(search_results[0]->relevance_score, 0.01);
                     search_run_loop.QuitClosure().Run();
                   }));
  search_run_loop.Run();
#endif
}

IN_PROC_BROWSER_TEST_P(HelpAppIntegrationTest, HelpAppV2CanOpenAlmanacScheme) {
  WaitForTestSystemAppInstall();
  content::WebContents* web_contents = LaunchApp(SystemWebAppType::HELP);

  base::test::TestFuture<apps::PackageId> future;
  apps::AppInstallServiceAsh::InstallAppCallbackForTesting() =
      future.GetCallback();
  constexpr char kScript[] = R"(
    (() => {
      location.href = 'almanac://install-app?package_id=web:test';
      return true;
    })();
  )";
  EXPECT_EQ(true,
            content::EvalJs(
                SandboxedWebUiAppTestBase::GetAppFrame(web_contents), kScript));
  EXPECT_EQ(future.Get<apps::PackageId>(),
            apps::PackageId::FromString("web:test"));
}

IN_PROC_BROWSER_TEST_P(HelpAppIntegrationTest, HelpAppV2CanOpenCrosAppsScheme) {
  WaitForTestSystemAppInstall();
  content::WebContents* web_contents = LaunchApp(SystemWebAppType::HELP);

  base::test::TestFuture<apps::PackageId> future;
  apps::AppInstallServiceAsh::InstallAppCallbackForTesting() =
      future.GetCallback();
  constexpr char kScript[] = R"(
    (() => {
      location.href = 'cros-apps://install-app?package_id=web:test';
      return true;
    })();
  )";
  EXPECT_EQ(true,
            content::EvalJs(
                SandboxedWebUiAppTestBase::GetAppFrame(web_contents), kScript));
  EXPECT_EQ(future.Get<apps::PackageId>(),
            apps::PackageId::FromString("web:test"));
}

// Test that the Help App opens when Gesture help requested.
IN_PROC_BROWSER_TEST_P(HelpAppAllProfilesIntegrationTest, HelpAppOpenGestures) {
  WaitForTestSystemAppInstall();
  base::HistogramTester histogram_tester;

  GURL expected_url = GURL("chrome://help-app/help/sub/3399710/id/9739838");
  content::TestNavigationObserver navigation_observer(expected_url);
  navigation_observer.StartWatchingNewWebContents();
  SystemTrayClientImpl::Get()->ShowGestureEducationHelp();

  EXPECT_NO_FATAL_FAILURE(navigation_observer.Wait());
  EXPECT_EQ(expected_url, GetActiveWebContents()->GetVisibleURL());

  histogram_tester.ExpectUniqueSample("Apps.DefaultAppLaunch.FromOtherApp",
                                      apps::DefaultAppName::kHelpApp, 1);
  histogram_tester.ExpectUniqueSample("Discover.Overall.AppLaunched",
                                      apps::LaunchSource::kFromOtherApp, 1);
}

// Test that the Help App opens from keyboard shortcut.
IN_PROC_BROWSER_TEST_P(HelpAppAllProfilesIntegrationTest,
                       HelpAppOpenKeyboardShortcut) {
  WaitForTestSystemAppInstall();
  base::HistogramTester histogram_tester;

  // The /? key is OEM_2 on a US standard keyboard.
  GURL expected_url;
#if BUILDFLAG(ENABLE_CROS_HELP_APP)
  expected_url = GURL("chrome://help-app");
#else
  expected_url = GURL(chrome::kChromeHelpViaKeyboardURL);
#endif
  content::TestNavigationObserver navigation_observer(expected_url);
  navigation_observer.StartWatchingNewWebContents();
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_OEM_2, /*control=*/true,
      /*shift=*/false, /*alt=*/false, /*command=*/false));
  navigation_observer.Wait();

#if BUILDFLAG(ENABLE_CROS_HELP_APP)
  // Default browser tab and Help app are open.
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());
  EXPECT_EQ("chrome://help-app/", GetActiveWebContents()->GetVisibleURL());
  histogram_tester.ExpectUniqueSample("Apps.DefaultAppLaunch.FromKeyboard",
                                      apps::DefaultAppName::kHelpApp, 1);
  histogram_tester.ExpectUniqueSample("Discover.Overall.AppLaunched",
                                      apps::LaunchSource::kFromKeyboard, 1);
#else
  // We just have the one browser. Navigates chrome.
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(GURL(chrome::kChromeHelpViaKeyboardURL),
            GetActiveWebContents()->GetVisibleURL());
  histogram_tester.ExpectUniqueSample("Apps.DefaultAppLaunch.FromKeyboard",
                                      apps::DefaultAppName::kHelpApp, 0);
  histogram_tester.ExpectUniqueSample("Discover.Overall.AppLaunched",
                                      apps::LaunchSource::kFromKeyboard, 0);
#endif
}

// Test that the Help App opens in a new window if try to navigate there in a
// browser.
IN_PROC_BROWSER_TEST_P(HelpAppAllProfilesIntegrationTest,
                       HelpAppCapturesBrowserNavigation) {
  WaitForTestSystemAppInstall();
  content::TestNavigationObserver navigation_observer(
      GURL("chrome://help-app"));
  navigation_observer.StartWatchingNewWebContents();
  ASSERT_EQ(1u, chrome::GetTotalBrowserCount());

  // Try to navigate to the help app in the browser.
  ui_test_utils::SendToOmniboxAndSubmit(browser(), "chrome://help-app");
  navigation_observer.Wait();

  // We now have two browsers, one for the chrome window, one for the Help app.
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(GURL("chrome://help-app"), GetActiveWebContents()->GetVisibleURL());
}

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    HelpAppIntegrationTest);

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    HelpAppIntegrationTestWithAutoTriggerDisabled);

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    HelpAppIntegrationTestWithFirstRunEnabled);

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_ALL_PROFILE_TYPES_P(
    HelpAppAllProfilesIntegrationTest);

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    HelpAppIntegrationTestWithHelpAppOpensInsteadOfReleaseNotesNotification);

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    HelpAppIntegrationTestWithBirchFeatureEnabled);
}  // namespace ash
