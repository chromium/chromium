// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/shell.h"
#include "ash/style/system_dialog_delegate_view.h"
#include "ash/test/ash_test_util.h"
#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_grid_test_api.h"
#include "ash/wm/overview/overview_test_util.h"
#include "ash/wm/window_restore/informed_restore_constants.h"
#include "ash/wm/window_restore/informed_restore_contents_data.h"
#include "ash/wm/window_restore/informed_restore_contents_view.h"
#include "ash/wm/window_restore/informed_restore_controller.h"
#include "ash/wm/window_restore/informed_restore_test_api.h"
#include "ash/wm/window_restore/window_restore_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/app_restore/app_restore_test_util.h"
#include "chrome/browser/ash/app_restore/full_restore_app_launch_handler.h"
#include "chrome/browser/ash/app_restore/full_restore_service.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/webui/ash/settings/pref_names.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ash/util/ash_test_util.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "ui/views/test/widget_test.h"

namespace ash::full_restore {

namespace {

const InformedRestoreContentsView* GetInformedRestoreContentsView() {
  OverviewGrid* overview_grid =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  if (!overview_grid) {
    return nullptr;
  }

  views::Widget* informed_restore_widget =
      OverviewGridTestApi(overview_grid).informed_restore_widget();
  if (!informed_restore_widget) {
    return nullptr;
  }

  return views::AsViewClass<InformedRestoreContentsView>(
      informed_restore_widget->GetContentsView());
}

// Retrieve the "Restore" button from the informed restore dialog, if we are in
// an informed restore session.
const PillButton* GetInformedRestoreDialogRestoreButton() {
  const auto* contents_view =
      GetInformedRestoreContentsView();
  return contents_view
             ? static_cast<const PillButton*>(contents_view->GetViewByID(
                   informed_restore::kRestoreButtonID))
             : nullptr;
}

const PillButton* GetInformedRestoreDialogCancelButton() {
  const auto* contents_view =
      GetInformedRestoreContentsView();
  return contents_view
             ? static_cast<const PillButton*>(contents_view->GetViewByID(
                   informed_restore::kCancelButtonID))
             : nullptr;
}

}  // namespace

class InformedRestoreTest : public InProcessBrowserTest {
 public:
  InformedRestoreTest() {
    set_launch_browser_for_testing(nullptr);

    feature_list_.InitWithFeatures(
        {features::kForestFeature, features::kSanitize}, {});
  }
  InformedRestoreTest(const InformedRestoreTest&) = delete;
  InformedRestoreTest& operator=(const InformedRestoreTest&) = delete;
  ~InformedRestoreTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    // Set the restore pref setting as "Ask every time". This will ensure the
    // informed restore dialog comes up on the next session.
    auto* prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
    prefs->SetInteger(prefs::kRestoreAppsAndPagesPrefName,
                      static_cast<int>(RestoreOption::kAskEveryTime));
    prefs->SetBoolean(prefs::kShowInformedRestoreOnboarding, false);
  }

 protected:
  base::HistogramTester histogram_tester_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Creates 2 browser windows that will be restored in the main test.
IN_PROC_BROWSER_TEST_F(InformedRestoreTest, PRE_LaunchBrowsers) {
  EXPECT_TRUE(BrowserList::GetInstance()->empty());

  Profile* profile = ProfileManager::GetActiveUserProfile();
  CreateBrowser(profile);
  CreateBrowser(profile);
  EXPECT_EQ(2u, BrowserList::GetInstance()->size());

  // Immediate save to full restore file to bypass the 2.5 second throttle.
  AppLaunchInfoSaveWaiter::Wait();
}

// Verify that with two elements in the full restore file, we enter overview on
// login. Then when we click the restore button, we restore two browsers.
IN_PROC_BROWSER_TEST_F(InformedRestoreTest, LaunchBrowsers) {
  EXPECT_TRUE(BrowserList::GetInstance()->empty());

  // Verify we have entered overview. The restore button will be null if we
  // failed to enter overview.
  WaitForOverviewEnterAnimation();
  const PillButton* restore_button = GetInformedRestoreDialogRestoreButton();
  ASSERT_TRUE(restore_button);

  // Click the "Restore" button and verify we have launched 2 browsers.
  test::BrowsersWaiter waiter(/*expected_count=*/2);
  test::Click(restore_button, /*flag=*/0);
  waiter.Wait();
  EXPECT_EQ(2u, BrowserList::GetInstance()->size());

  histogram_tester_.ExpectBucketCount("Apps.FullRestoreWindowCount2", 2, 1);
  histogram_tester_.ExpectUniqueSample("Ash.FirstWebContentsProfile.Recorded",
                                       false, 1);
}

// Creates SWAs that will be restored in the main test.
IN_PROC_BROWSER_TEST_F(InformedRestoreTest, PRE_LaunchSWA) {
  EXPECT_TRUE(BrowserList::GetInstance()->empty());

  // Create two SWAs, files and settings.
  Profile* profile = ProfileManager::GetActiveUserProfile();
  test::InstallSystemAppsForTesting(profile);
  test::CreateSystemWebApp(profile, SystemWebAppType::FILE_MANAGER);
  test::CreateSystemWebApp(profile, SystemWebAppType::SETTINGS);
  EXPECT_EQ(2u, BrowserList::GetInstance()->size());

  // Immediate save to full restore file to bypass the 2.5 second throttle.
  AppLaunchInfoSaveWaiter::Wait();
}

// Verify that with two elements in the full restore file, we enter overview on
// login. Then when we click the restore button, we restore SWAs.
IN_PROC_BROWSER_TEST_F(InformedRestoreTest, LaunchSWA) {
  EXPECT_TRUE(BrowserList::GetInstance()->empty());

  test::InstallSystemAppsForTesting(ProfileManager::GetActiveUserProfile());

  // Verify we have entered overview. The restore button will be null if we
  // failed to enter overview.
  WaitForOverviewEnterAnimation();
  const PillButton* restore_button = GetInformedRestoreDialogRestoreButton();
  ASSERT_TRUE(restore_button);

  // Click the "Restore" button.
  test::BrowsersWaiter waiter(/*expected_count=*/2);
  test::Click(restore_button, /*flag=*/0);
  waiter.Wait();

  // Verify that two browsers are launched and they are the file manager and
  // settings SWAs.
  auto* browser_list = BrowserList::GetInstance();
  EXPECT_EQ(2u, browser_list->size());
  EXPECT_TRUE(base::ranges::any_of(*browser_list, [](Browser* browser) {
    return IsBrowserForSystemWebApp(browser, SystemWebAppType::FILE_MANAGER);
  }));
  EXPECT_TRUE(base::ranges::any_of(*browser_list, [](Browser* browser) {
    return IsBrowserForSystemWebApp(browser, SystemWebAppType::SETTINGS);
  }));
}

// Creates 3 browser windows on 3 different desks that will be restored in the
// main test.
IN_PROC_BROWSER_TEST_F(InformedRestoreTest, PRE_LaunchBrowsersToDesks) {
  EXPECT_TRUE(BrowserList::GetInstance()->empty());

  Profile* profile = ProfileManager::GetActiveUserProfile();
  Browser* browser1 = CreateBrowser(profile);
  Browser* browser2 = CreateBrowser(profile);
  Browser* browser3 = CreateBrowser(profile);
  EXPECT_EQ(3u, BrowserList::GetInstance()->size());

  // Add two desks for a total of three. The browsers were all created on the
  // active desk.
  auto* desks_controller = DesksController::Get();
  desks_controller->NewDesk(DesksCreationRemovalSource::kKeyboard);
  desks_controller->NewDesk(DesksCreationRemovalSource::kKeyboard);
  ASSERT_EQ(3u, desks_controller->desks().size());
  for (Browser* browser : {browser1, browser2, browser3}) {
    ASSERT_TRUE(desks_controller->BelongsToActiveDesk(
        browser->window()->GetNativeWindow()));
  }

  // Move some windows so there is one window on each desk.
  aura::Window* primary_root = Shell::GetPrimaryRootWindow();
  desks_controller->MoveWindowFromActiveDeskTo(
      browser2->window()->GetNativeWindow(),
      desks_controller->GetDeskAtIndex(1), primary_root,
      DesksMoveWindowFromActiveDeskSource::kShortcut);
  desks_controller->MoveWindowFromActiveDeskTo(
      browser3->window()->GetNativeWindow(),
      desks_controller->GetDeskAtIndex(2), primary_root,
      DesksMoveWindowFromActiveDeskSource::kShortcut);

  const std::vector<std::unique_ptr<Desk>>& desks = desks_controller->desks();
  ASSERT_EQ(3u, desks.size());
  EXPECT_EQ(1u, desks[0]->windows().size());
  EXPECT_EQ(1u, desks[1]->windows().size());
  EXPECT_EQ(1u, desks[2]->windows().size());

  // Immediate save to full restore file to bypass the 2.5 second throttle.
  AppLaunchInfoSaveWaiter::Wait();
}

// Tests that the three browser windows are restored to their old desks.
IN_PROC_BROWSER_TEST_F(InformedRestoreTest, DISABLED_LaunchBrowsersToDesks) {
  EXPECT_TRUE(BrowserList::GetInstance()->empty());

  // Verify we have entered overview. The restore button will be null if we
  // failed to enter overview.
  WaitForOverviewEnterAnimation();
  const PillButton* restore_button = GetInformedRestoreDialogRestoreButton();
  ASSERT_TRUE(restore_button);

  // Click the "Restore" button and verify we have launched 3 browsers.
  test::BrowsersWaiter waiter(/*expected_count=*/3);
  test::Click(restore_button, /*flag=*/0);
  waiter.Wait();

  // Ensure overview animation is finished as overview UI windows go into the
  // active desk container.
  WaitForOverviewExitAnimation();

  // Verify that each desk has one window.
  auto* desks_controller = DesksController::Get();
  const std::vector<std::unique_ptr<Desk>>& desks = desks_controller->desks();
  ASSERT_EQ(3u, desks.size());
  EXPECT_EQ(1u, desks[0]->windows().size());
  EXPECT_EQ(1u, desks[1]->windows().size());
  EXPECT_EQ(1u, desks[2]->windows().size());
}

IN_PROC_BROWSER_TEST_F(InformedRestoreTest, PRE_WindowStates) {
  EXPECT_TRUE(BrowserList::GetInstance()->empty());

  Profile* profile = ProfileManager::GetActiveUserProfile();
  Browser* browser_maximized = CreateBrowser(profile);
  Browser* browser_minimized = CreateBrowser(profile);
  Browser* browser_fullscreened = CreateBrowser(profile);
  Browser* browser_floated = CreateBrowser(profile);
  Browser* browser_snapped = CreateBrowser(profile);
  EXPECT_EQ(5u, BrowserList::GetInstance()->size());

  WindowState::Get(browser_maximized->window()->GetNativeWindow())->Maximize();

  // Also maximize `browser_minimized` before minimizing so we can test the
  // pre-minimized state as well.
  WindowState::Get(browser_minimized->window()->GetNativeWindow())->Maximize();
  WindowState::Get(browser_minimized->window()->GetNativeWindow())->Minimize();

  // Fullscreen a window. This should not be restored as full restore does not
  // support restoring fullscreen state.
  const WMEvent fullscreen_event(WM_EVENT_FULLSCREEN);
  WindowState::Get(browser_fullscreened->window()->GetNativeWindow())
      ->OnWMEvent(&fullscreen_event);

  const WMEvent float_event(WM_EVENT_FLOAT);
  WindowState::Get(browser_floated->window()->GetNativeWindow())
      ->OnWMEvent(&float_event);

  const WindowSnapWMEvent snap_event(WM_EVENT_SNAP_PRIMARY);
  WindowState::Get(browser_snapped->window()->GetNativeWindow())
      ->OnWMEvent(&snap_event);

  // Immediate save to full restore file to bypass the 2.5 second throttle.
  AppLaunchInfoSaveWaiter::Wait();
}

// TODO(crbug.com/330516096): Test is flaky.
// Tests that the browser windows are restored to their old window states.
IN_PROC_BROWSER_TEST_F(InformedRestoreTest, DISABLED_WindowStates) {
  EXPECT_TRUE(BrowserList::GetInstance()->empty());

  // Verify we have entered overview. The restore button will be null if we
  // failed to enter overview.
  WaitForOverviewEnterAnimation();
  const PillButton* restore_button = GetInformedRestoreDialogRestoreButton();
  ASSERT_TRUE(restore_button);

  // Click the "Restore" button and verify we have launched 5 browsers.
  test::BrowsersWaiter waiter(/*expected_count=*/5);
  test::Click(restore_button, /*flag=*/0);
  waiter.Wait();

  auto* browser_list = BrowserList::GetInstance();
  EXPECT_EQ(5u, browser_list->size());

  // Test that there is a maximized, floated and snapped window.
  EXPECT_TRUE(base::ranges::any_of(*browser_list, [](Browser* browser) {
    return WindowState::Get(browser->window()->GetNativeWindow())
        ->IsMaximized();
  }));
  EXPECT_TRUE(base::ranges::any_of(*browser_list, [](Browser* browser) {
    return WindowState::Get(browser->window()->GetNativeWindow())->IsFloated();
  }));
  EXPECT_TRUE(base::ranges::any_of(*browser_list, [](Browser* browser) {
    return WindowState::Get(browser->window()->GetNativeWindow())->IsSnapped();
  }));

  // Test that there is no fullscreen window as full restore does not restore
  // fullscreen state.
  EXPECT_TRUE(base::ranges::none_of(*browser_list, [](Browser* browser) {
    return WindowState::Get(browser->window()->GetNativeWindow())
        ->IsFullscreen();
  }));

  // Test the pre-minimized state of the minimized browser window. When we
  // unminimize it, it should be maximized state.
  auto it = base::ranges::find_if(*browser_list, [](Browser* browser) {
    return WindowState::Get(browser->window()->GetNativeWindow())
        ->IsMinimized();
  });
  ASSERT_NE(it, browser_list->end());
  auto* window_state = WindowState::Get((*it)->window()->GetNativeWindow());
  window_state->Unminimize();
  EXPECT_TRUE(window_state->IsMaximized());
}

IN_PROC_BROWSER_TEST_F(InformedRestoreTest, PRE_ClickCancelButton) {
  EXPECT_TRUE(BrowserList::GetInstance()->empty());

  Profile* profile = ProfileManager::GetActiveUserProfile();
  CreateBrowser(profile);
  CreateBrowser(profile);
  EXPECT_EQ(2u, BrowserList::GetInstance()->size());

  // Immediate save to full restore file to bypass the 2.5 second throttle.
  AppLaunchInfoSaveWaiter::Wait();
}

// Verify that with two elements in the full restore file, if we click cancel no
// browsers are launched.
IN_PROC_BROWSER_TEST_F(InformedRestoreTest, ClickCancelButton) {
  EXPECT_TRUE(BrowserList::GetInstance()->empty());

  // Verify we have entered overview. The cancel button will be null if we
  // failed to enter overview.
  WaitForOverviewEnterAnimation();
  const PillButton* cancel_button = GetInformedRestoreDialogCancelButton();
  ASSERT_TRUE(cancel_button);

  // Click the cancel button. We spin the run loop because launching browsers is
  // async. Verify that no browsers are launched.
  test::Click(cancel_button, /*flag=*/0);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(BrowserList::GetInstance()->empty());
}

IN_PROC_BROWSER_TEST_F(InformedRestoreTest, PRE_TabInfoWithinLimit) {
  EXPECT_TRUE(BrowserList::GetInstance()->empty());

  Browser* browser = CreateBrowser(ProfileManager::GetActiveUserProfile());
  EXPECT_EQ(1u, BrowserList::GetInstance()->size());

  // Create four more urls in addition to the default "about:blank" tab. That
  // tab will be last in the tab strip.
  const std::vector<GURL> urls{
      GURL("https://www.youtube.com/"), GURL("https://www.google.com/"),
      GURL("https://www.waymo.com/"), GURL("https://x.company/")};
  for (int i = 0; i < static_cast<int>(urls.size()); ++i) {
    content::TestNavigationObserver navigation_observer(urls[i]);
    navigation_observer.StartWatchingNewWebContents();
    chrome::AddTabAt(browser, urls[i], /*index=*/i,
                     /*foreground=*/false);
    navigation_observer.Wait();
  }

  // Activate the third tab (waymo.com) so it becomes the most recent tab.
  browser->tab_strip_model()->ActivateTabAt(2);

  // Immediate save to full restore file to bypass the 2.5 second throttle.
  AppLaunchInfoSaveWaiter::Wait();
}

// Verify that the tab info that is sent to ash shell is as expected, when the
// most recent active tab is one of the first five tabs.
IN_PROC_BROWSER_TEST_F(InformedRestoreTest, TabInfoWithinLimit) {
  EXPECT_TRUE(BrowserList::GetInstance()->empty());

  // The informed restore dialog is built based on the values in this data
  // structure.
  const InformedRestoreContentsData* contents_data =
      Shell::Get()->informed_restore_controller()->contents_data();
  ASSERT_TRUE(contents_data);
  const InformedRestoreContentsData::AppsInfos& apps_infos =
      contents_data->apps_infos;

  ASSERT_EQ(1u, apps_infos.size());
  ASSERT_EQ(5u, apps_infos[0].tab_urls.size());

  // As it was the most recently activated tab, waymo.com should appear first,
  // with the other four tabs appearing afterwards in order.
  EXPECT_EQ(GURL("https://www.waymo.com/"), apps_infos[0].tab_urls[0]);
  EXPECT_EQ(GURL("https://www.youtube.com/"), apps_infos[0].tab_urls[1]);
  EXPECT_EQ(GURL("https://www.google.com/"), apps_infos[0].tab_urls[2]);
  EXPECT_EQ(GURL("https://x.company/"), apps_infos[0].tab_urls[3]);
  EXPECT_EQ(GURL(url::kAboutBlankURL), apps_infos[0].tab_urls[4]);
}

IN_PROC_BROWSER_TEST_F(InformedRestoreTest, PRE_TabInfoOutsideLimit) {
  EXPECT_TRUE(BrowserList::GetInstance()->empty());

  Browser* browser = CreateBrowser(ProfileManager::GetActiveUserProfile());
  EXPECT_EQ(1u, BrowserList::GetInstance()->size());

  // Create six more urls in addition to the default "about:blank" tab. That tab
  // will be last in the tab strip.
  const std::vector<GURL> urls{
      GURL("https://www.youtube.com/"), GURL("https://www.google.com/"),
      GURL("https://www.waymo.com/"),   GURL("https://x.company/"),
      GURL("https://docs.google.com/"), GURL("https://www.chromium.org/")};
  for (int i = 0; i < static_cast<int>(urls.size()); ++i) {
    content::TestNavigationObserver navigation_observer(urls[i]);
    navigation_observer.StartWatchingNewWebContents();
    chrome::AddTabAt(browser, urls[i], /*index=*/i,
                     /*foreground=*/false);
    navigation_observer.Wait();
  }

  // Activate the sixth tab (chromium.org) so it becomes the most recent tab.
  browser->tab_strip_model()->ActivateTabAt(5);

  // Immediate save to full restore file to bypass the 2.5 second throttle.
  AppLaunchInfoSaveWaiter::Wait();
}

// Verify that the tab info that is sent to ash shell is as expected, when the
// most recent active tab is outside of the first five tabs.
IN_PROC_BROWSER_TEST_F(InformedRestoreTest, TabInfoOutsideLimit) {
  EXPECT_TRUE(BrowserList::GetInstance()->empty());

  // The informed restore dialog is built based on the values in this data
  // structure.
  const InformedRestoreContentsData* contents_data =
      Shell::Get()->informed_restore_controller()->contents_data();
  ASSERT_TRUE(contents_data);
  const InformedRestoreContentsData::AppsInfos& apps_infos =
      contents_data->apps_infos;

  // Even though there were seven tabs, we limit the number of tab URLs to five
  // before `InformedRestoreContentsData` is created.
  ASSERT_EQ(1u, apps_infos.size());
  ASSERT_EQ(5u, apps_infos[0].tab_urls.size());

  // As it was the most recently activated tab, chromium.org should appear
  // first, with the first four tabs in the tab strip appearing afterwards in
  // order.
  EXPECT_EQ(GURL("https://www.chromium.org/"), apps_infos[0].tab_urls[0]);
  EXPECT_EQ(GURL("https://www.youtube.com/"), apps_infos[0].tab_urls[1]);
  EXPECT_EQ(GURL("https://www.google.com/"), apps_infos[0].tab_urls[2]);
  EXPECT_EQ(GURL("https://www.waymo.com/"), apps_infos[0].tab_urls[3]);
  EXPECT_EQ(GURL("https://x.company/"), apps_infos[0].tab_urls[4]);
}

IN_PROC_BROWSER_TEST_F(InformedRestoreTest, PRE_AppInfo) {
  EXPECT_TRUE(BrowserList::GetInstance()->empty());

  // Create multiple SWAs that will be added to the restore data. Each SWA is
  // activated when it is created, so the Print Management app should be the
  // most recently active app, and the Media app should be the least recently
  // active app.
  Profile* profile = ProfileManager::GetActiveUserProfile();
  test::InstallSystemAppsForTesting(profile);
  test::CreateSystemWebApp(profile, SystemWebAppType::MEDIA);
  test::CreateSystemWebApp(profile, SystemWebAppType::SETTINGS);
  test::CreateSystemWebApp(profile, SystemWebAppType::CAMERA);
  test::CreateSystemWebApp(profile, SystemWebAppType::PRINT_MANAGEMENT);
  auto* browser_list = BrowserList::GetInstance();
  ASSERT_EQ(4u, browser_list->size());

  // Activate the Camera app so it appears at the front of the activation list.
  browser_list->get(2u)->window()->Activate();
  ASSERT_EQ(browser_list->GetLastActive(), browser_list->get(2u));

  // Immediate save to full restore file to bypass the 2.5 second throttle.
  AppLaunchInfoSaveWaiter::Wait();
}

// Verify that the app info that is sent to ash shell is as expected, with the
// apps appearing in order from most recently used to least recently used.
IN_PROC_BROWSER_TEST_F(InformedRestoreTest, AppInfo) {
  EXPECT_TRUE(BrowserList::GetInstance()->empty());

  // The informed restore dialog is built based on the values in this data
  // structure.
  const InformedRestoreContentsData* contents_data =
      Shell::Get()->informed_restore_controller()->contents_data();
  ASSERT_TRUE(contents_data);
  const InformedRestoreContentsData::AppsInfos& apps_infos =
      contents_data->apps_infos;

  // The Camera app should appear first, and the rest of the apps should appear
  // in the reverse of the order they were created. We can check each entry
  // against a known SWA ID to verify. See
  // `ash/constants/web_app_id_constants.h` for more IDs.
  ASSERT_EQ(4u, apps_infos.size());

  // Camera
  EXPECT_EQ("njfbnohfdkmbmnjapinfcopialeghnmh", apps_infos[0].app_id);
  // Print Management
  EXPECT_EQ("fglkccnmnaankjodgccmiodmlkpaiodc", apps_infos[1].app_id);
  // Settings
  EXPECT_EQ("odknhmnlageboeamepcngndbggdpaobj", apps_infos[2].app_id);
  // Media
  EXPECT_EQ("jhdjimmaggjajfjphpljagpgkidjilnj", apps_infos[3].app_id);
}

IN_PROC_BROWSER_TEST_F(InformedRestoreTest, PRE_Update) {
  // Need at least one window for restore data.
  Profile* profile = ProfileManager::GetActiveUserProfile();
  CreateBrowser(profile);
  EXPECT_EQ(1u, BrowserList::GetInstance()->size());

  // Prepare for the main test body by setting the version to one that will be
  // less.
  profile->GetPrefs()->SetString(prefs::kInformedRestoreLastVersion, "0.0.0.0");

  // Immediate save to full restore file to bypass the 2.5 second throttle.
  AppLaunchInfoSaveWaiter::Wait();
}

// Verify that the app info that is sent to ash shell is dialog type update.
IN_PROC_BROWSER_TEST_F(InformedRestoreTest, Update) {
  EXPECT_TRUE(BrowserList::GetInstance()->empty());

  const InformedRestoreContentsData* contents_data =
      Shell::Get()->informed_restore_controller()->contents_data();
  ASSERT_TRUE(contents_data);
  EXPECT_EQ(InformedRestoreContentsData::DialogType::kUpdate,
            contents_data->dialog_type);
}

IN_PROC_BROWSER_TEST_F(InformedRestoreTest, PRE_ReenterInformedRestoreSession) {
  EXPECT_TRUE(BrowserList::GetInstance()->empty());
  CreateBrowser(ProfileManager::GetActiveUserProfile());
  EXPECT_EQ(1u, BrowserList::GetInstance()->size());

  // Immediate save to full restore file to bypass the 2.5 second throttle.
  AppLaunchInfoSaveWaiter::Wait();
}

// Test that if we exit overview and reenter without opening a new window, we
// see the informed restore dialog again.
IN_PROC_BROWSER_TEST_F(InformedRestoreTest, ReenterInformedRestoreSession) {
  EXPECT_TRUE(BrowserList::GetInstance()->empty());

  // Verify we have entered overview with the informed restore dialog.
  WaitForOverviewEnterAnimation();
  ASSERT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());
  EXPECT_TRUE(GetInformedRestoreDialogRestoreButton());
  EXPECT_TRUE(Shell::Get()->informed_restore_controller()->contents_data());

  // Exit overview without clicking restore or cancel.
  ToggleOverview();
  WaitForOverviewExitAnimation();
  EXPECT_TRUE(Shell::Get()->informed_restore_controller()->contents_data());

  // Reenter overview. Test that the dialog is still visible.
  ToggleOverview();
  WaitForOverviewEnterAnimation();
  EXPECT_TRUE(GetInformedRestoreDialogRestoreButton());

  // Open a new window using the accelerator. This should delete the informed
  // restore dialog data and the next overview enter will not show the dialog.
  ToggleOverview();
  WaitForOverviewExitAnimation();
  test::BrowsersWaiter waiter(/*expected_count=*/1);
  ASSERT_TRUE(Shell::Get()->accelerator_controller()->PerformActionIfEnabled(
      AcceleratorAction::kNewWindow, {}));
  waiter.Wait();
  EXPECT_FALSE(Shell::Get()->informed_restore_controller()->contents_data());

  // Reentering overview this time should not show the dialog.
  ToggleOverview();
  WaitForOverviewEnterAnimation();
  EXPECT_FALSE(GetInformedRestoreDialogRestoreButton());
}

class InformedRestoreOnboardingTest : public InformedRestoreTest {
 public:
  InformedRestoreOnboardingTest() = default;
  InformedRestoreOnboardingTest(const InformedRestoreOnboardingTest&) = delete;
  InformedRestoreOnboardingTest& operator=(
      const InformedRestoreOnboardingTest&) = delete;
  ~InformedRestoreOnboardingTest() override = default;

  void SetUpDefaultCommandLine(base::CommandLine* command_line) override {
    InformedRestoreTest::SetUpDefaultCommandLine(command_line);

    // Onboarding dialog is gated by this switch so it doesn't affect other
    // browser tests.
    command_line->RemoveSwitch(::switches::kNoFirstRun);
  }
};

IN_PROC_BROWSER_TEST_F(InformedRestoreOnboardingTest, PRE_RestoreOff) {
  auto* prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  prefs->SetInteger(prefs::kRestoreAppsAndPagesPrefName,
                    static_cast<int>(RestoreOption::kDoNotRestore));
  prefs->SetBoolean(prefs::kShowInformedRestoreOnboarding, true);
}

// Tests that when Restore is off, we show the onboarding dialog.
IN_PROC_BROWSER_TEST_F(InformedRestoreOnboardingTest, RestoreOff) {
  // The first time after rebooting, we show the onboarding dialog.
  auto* onboarding_dialog = InformedRestoreTestApi().GetOnboardingDialog();
  ASSERT_TRUE(onboarding_dialog);

  // Press the accept button.
  test::Click(onboarding_dialog->GetAcceptButtonForTesting(), /*flag=*/0);
  views::test::WidgetDestroyedWaiter(onboarding_dialog->GetWidget()).Wait();
  EXPECT_FALSE(InformedRestoreTestApi().GetOnboardingDialog());

  // Verify we do not enter overview.
  EXPECT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());

  // Verify the restore pref is updated.
  EXPECT_EQ(static_cast<int>(RestoreOption::kAskEveryTime),
            ProfileManager::GetActiveUserProfile()->GetPrefs()->GetInteger(
                prefs::kRestoreAppsAndPagesPrefName));
}

IN_PROC_BROWSER_TEST_F(InformedRestoreOnboardingTest, PRE_NoRestoreData) {
  auto* prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  EXPECT_EQ(static_cast<int>(RestoreOption::kAskEveryTime),
            prefs->GetInteger(prefs::kRestoreAppsAndPagesPrefName));
  prefs->SetBoolean(prefs::kShowInformedRestoreOnboarding, true);
}

// Tests that when Restore is 'Ask every time' and there is no restore data, we
// show the onboarding dialog.
IN_PROC_BROWSER_TEST_F(InformedRestoreOnboardingTest, NoRestoreData) {
  // The first time after rebooting, we show the onboarding dialog.
  auto* onboarding_dialog = InformedRestoreTestApi().GetOnboardingDialog();
  ASSERT_TRUE(onboarding_dialog);

  // Press the accept button.
  test::Click(onboarding_dialog->GetAcceptButtonForTesting(), /*flag=*/0);
  views::test::WidgetDestroyedWaiter(onboarding_dialog->GetWidget()).Wait();
  EXPECT_FALSE(InformedRestoreTestApi().GetOnboardingDialog());

  // Verify we do not enter overview.
  EXPECT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());
}

IN_PROC_BROWSER_TEST_F(InformedRestoreOnboardingTest, PRE_Onboarding) {
  // The restore pref setting is 'Ask every time' by default.
  auto* prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  EXPECT_EQ(static_cast<int>(RestoreOption::kAskEveryTime),
            prefs->GetInteger(prefs::kRestoreAppsAndPagesPrefName));
  prefs->SetBoolean(prefs::kShowInformedRestoreOnboarding, true);

  Profile* profile = ProfileManager::GetActiveUserProfile();
  CreateBrowser(profile);
  EXPECT_EQ(1u, BrowserList::GetInstance()->size());

  // Immediate save to full restore file to bypass the 2.5 second throttle.
  AppLaunchInfoSaveWaiter::Wait();
}

// Tests that when Restore is 'Ask every time' and there is restore data, we
// show the onboarding dialog.
IN_PROC_BROWSER_TEST_F(InformedRestoreOnboardingTest, Onboarding) {
  // The first time after rebooting, we show the onboarding dialog.
  auto* onboarding_dialog = InformedRestoreTestApi().GetOnboardingDialog();
  ASSERT_TRUE(onboarding_dialog);

  // Press the accept button.
  test::Click(onboarding_dialog->GetAcceptButtonForTesting(), /*flag=*/0);
  views::test::WidgetDestroyedWaiter(onboarding_dialog->GetWidget()).Wait();
  EXPECT_FALSE(InformedRestoreTestApi().GetOnboardingDialog());

  // Verify we have entered overview. The restore button will be null if
  // we failed to enter overview.
  WaitForOverviewEnterAnimation();
  const PillButton* restore_button = GetInformedRestoreDialogRestoreButton();
  ASSERT_TRUE(restore_button);

  // Click the "Restore" button and verify we have launched 1 browser.
  test::BrowsersWaiter waiter(/*expected_count=*/1);
  test::Click(restore_button, /*flag=*/0);
  waiter.Wait();
  EXPECT_EQ(1u, BrowserList::GetInstance()->size());

  // Attempt to show the dialog again. Since we've already shown it, we
  // don't show it again.
  Shell::Get()
      ->informed_restore_controller()
      ->MaybeShowInformedRestoreOnboarding(
          /*restore_on=*/true);
  EXPECT_FALSE(InformedRestoreTestApi().GetOnboardingDialog());
}

IN_PROC_BROWSER_TEST_F(InformedRestoreOnboardingTest, PRE_Sanitized) {
  // The restore pref setting is 'Ask every time' by default.
  auto* prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  EXPECT_EQ(static_cast<int>(RestoreOption::kAskEveryTime),
            prefs->GetInteger(prefs::kRestoreAppsAndPagesPrefName));
  prefs->SetBoolean(prefs::kShowInformedRestoreOnboarding, true);
  prefs->SetBoolean(settings::prefs::kSanitizeCompleted, true);

  Profile* profile = ProfileManager::GetActiveUserProfile();
  CreateBrowser(profile);
  EXPECT_EQ(1u, BrowserList::GetInstance()->size());

  // Immediate save to full restore file to bypass the 2.5 second throttle.
  AppLaunchInfoSaveWaiter::Wait();
}

// Tests that when the previous session was sanitized, we do not show any
// informed restore UI.
IN_PROC_BROWSER_TEST_F(InformedRestoreOnboardingTest, Sanitized) {
  auto* onboarding_dialog = InformedRestoreTestApi().GetOnboardingDialog();
  EXPECT_FALSE(onboarding_dialog);

  // The informed restore dialog is built based on the values in this data
  // structure. Test that it is null since we aren't planning on showing the
  // dialog if the previous session was sanitized.
  const InformedRestoreContentsData* contents_data =
      Shell::Get()->informed_restore_controller()->contents_data();
  EXPECT_FALSE(contents_data);
}

}  // namespace ash::full_restore
