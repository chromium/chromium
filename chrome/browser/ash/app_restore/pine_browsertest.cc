// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_restore/full_restore_service.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/shell.h"
#include "ash/test/ash_test_util.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_test_util.h"
#include "ash/wm/window_restore/pine_contents_view.h"
#include "ash/wm/window_restore/window_restore_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/app_restore/app_restore_test_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/ash_test_util.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"

namespace ash::full_restore {

namespace {

// Retrieve the "Restore" button from the pine dialog, if we are in a overview
// pine session.
const PillButton* GetPineDialogRestoreButton() {
  OverviewGrid* overview_grid =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  if (!overview_grid) {
    return nullptr;
  }

  // Retrieve the "Restore" button from the pine dialog.
  views::Widget* pine_widget = overview_grid->pine_widget_for_testing();
  if (!pine_widget) {
    return nullptr;
  }

  PineContentsView* pine_contents_view =
      views::AsViewClass<PineContentsView>(pine_widget->GetContentsView());
  CHECK(pine_contents_view);
  return pine_contents_view->restore_button_for_testing();
}

}  // namespace

// Class used to wait for multiple browser windows to be created.
class BrowsersWaiter : public BrowserListObserver {
 public:
  explicit BrowsersWaiter(int expected_count)
      : expected_count_(expected_count) {
    BrowserList::AddObserver(this);
  }
  BrowsersWaiter(const BrowsersWaiter&) = delete;
  BrowsersWaiter& operator=(const BrowsersWaiter&) = delete;
  ~BrowsersWaiter() override { BrowserList::RemoveObserver(this); }

  void Wait() { run_loop_.Run(); }

  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override {
    ++current_count_;
    if (current_count_ == expected_count_) {
      run_loop_.Quit();
    }
  }

 private:
  int current_count_ = 0;
  const int expected_count_;
  base::RunLoop run_loop_;
};

class PineBrowserTest : public InProcessBrowserTest {
 public:
  PineBrowserTest() {
    switches::SetIgnoreForestSecretKeyForTest(true);
    set_launch_browser_for_testing(nullptr);
  }
  PineBrowserTest(const PineBrowserTest&) = delete;
  PineBrowserTest& operator=(const PineBrowserTest&) = delete;
  ~PineBrowserTest() override {
    switches::SetIgnoreForestSecretKeyForTest(false);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    // Set the restore pref setting as "Ask every time". This will ensure the
    // pine dialog comes up on the next session.
    ProfileManager::GetActiveUserProfile()->GetPrefs()->SetInteger(
        prefs::kRestoreAppsAndPagesPrefName,
        static_cast<int>(RestoreOption::kAskEveryTime));
  }

 private:
  base::test::ScopedFeatureList feature_list_{features::kForestFeature};
};

// Creates 2 browser windows that will be restored in the main test.
IN_PROC_BROWSER_TEST_F(PineBrowserTest, PRE_LaunchBrowsers) {
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
IN_PROC_BROWSER_TEST_F(PineBrowserTest, LaunchBrowsers) {
  EXPECT_TRUE(BrowserList::GetInstance()->empty());

  // Verify we have entered overview. The restore button will be null if we
  // failed to enter overview.
  WaitForOverviewEnterAnimation();
  const PillButton* restore_button = GetPineDialogRestoreButton();
  ASSERT_TRUE(restore_button);

  // Click the "Restore" button and verify we have launched 2 browsers.
  BrowsersWaiter waiter(/*expected_count=*/2);
  test::Click(restore_button, /*flag=*/0);
  // TODO(sammiequon): Change `BrowsersWaiter::Wait()` to return a vector of
  // browsers and verify their tabs and pages as well.
  waiter.Wait();
  EXPECT_EQ(2u, BrowserList::GetInstance()->size());
}

// Creates SWAs that will be restored in the main test.
IN_PROC_BROWSER_TEST_F(PineBrowserTest, PRE_LaunchSWA) {
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
IN_PROC_BROWSER_TEST_F(PineBrowserTest, LaunchSWA) {
  EXPECT_TRUE(BrowserList::GetInstance()->empty());

  test::InstallSystemAppsForTesting(ProfileManager::GetActiveUserProfile());

  // Verify we have entered overview. The restore button will be null if we
  // failed to enter overview.
  WaitForOverviewEnterAnimation();
  const PillButton* restore_button = GetPineDialogRestoreButton();
  ASSERT_TRUE(restore_button);

  // Click the "Restore" button.
  BrowsersWaiter waiter(/*expected_count=*/2);
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
IN_PROC_BROWSER_TEST_F(PineBrowserTest, PRE_LaunchBrowsersToDesks) {
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
IN_PROC_BROWSER_TEST_F(PineBrowserTest, LaunchBrowsersToDesks) {
  EXPECT_TRUE(BrowserList::GetInstance()->empty());

  // Verify we have entered overview. The restore button will be null if we
  // failed to enter overview.
  WaitForOverviewEnterAnimation();
  const PillButton* restore_button = GetPineDialogRestoreButton();
  ASSERT_TRUE(restore_button);

  // Click the "Restore" button and verify we have launched 3 browsers.
  BrowsersWaiter waiter(/*expected_count=*/3);
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

}  // namespace ash::full_restore
