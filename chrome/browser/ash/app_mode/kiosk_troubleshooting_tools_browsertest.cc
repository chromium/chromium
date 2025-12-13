// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "ash/shell.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ash/app_mode/kiosk_controller.h"
#include "chrome/browser/ash/app_mode/kiosk_system_session.h"
#include "chrome/browser/ash/app_mode/test/kiosk_mixin.h"
#include "chrome/browser/ash/app_mode/test/kiosk_test_utils.h"
#include "chrome/browser/chromeos/app_mode/kiosk_browser_window_handler.h"
#include "chrome/browser/devtools/devtools_window_testing.h"
#include "chrome/browser/policy/developer_tools_policy_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/views/task_manager_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/window_open_disposition.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "url/gurl.h"

using policy::DeveloperToolsPolicyHandler::Availability::kAllowed;
using policy::DeveloperToolsPolicyHandler::Availability::kDisallowed;

namespace ash {

using kiosk::test::CreatePopupBrowser;
using kiosk::test::CreateRegularBrowser;
using kiosk::test::CurrentProfile;
using kiosk::test::DidKioskCloseNewWindow;
using kiosk::test::DidKioskHideNewWindow;
using kiosk::test::WaitKioskLaunched;

namespace {

KioskSystemSession& GetKioskSystemSession() {
  return CHECK_DEREF(KioskController::Get().GetKioskSystemSession());
}

}  // namespace

// Test kiosk troubleshooting tools on web kiosk.
class KioskTroubleshootingToolsTest : public MixinBasedInProcessBrowserTest {
 public:
  KioskTroubleshootingToolsTest() = default;

  KioskTroubleshootingToolsTest(const KioskTroubleshootingToolsTest&) = delete;
  KioskTroubleshootingToolsTest& operator=(
      const KioskTroubleshootingToolsTest&) = delete;

  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
    event_generator_ = std::make_unique<ui::test::EventGenerator>(
        ash::Shell::Get()->GetPrimaryRootWindow());
    ui_test_utils::BrowserCreatedObserver browser_created_observer;
    ASSERT_TRUE(WaitKioskLaunched());
    SetBrowser(browser_created_observer.Wait());
    ExpectOnlyKioskAppOpen();
  }

  void UpdateTroubleshootingToolsPolicy(bool enable) const {
    CurrentProfile().GetPrefs()->SetBoolean(
        prefs::kKioskTroubleshootingToolsEnabled, enable);
  }

  void EnableDevTools() const {
    CurrentProfile().GetPrefs()->SetInteger(prefs::kDevToolsAvailability,
                                            static_cast<int>(kAllowed));
  }

  void ExpectOpenBrowser(chromeos::KioskBrowserWindowType window_type) const {
    EXPECT_EQ(chrome::GetTotalBrowserCount(), 2u);
    histogram.ExpectBucketCount(chromeos::kKioskNewBrowserWindowHistogram,
                                window_type, 1);
    histogram.ExpectTotalCount(chromeos::kKioskNewBrowserWindowHistogram, 1);
  }

  void ExpectOnlyKioskAppOpen() const {
    // The initial browser should exist in the web kiosk session.
    ASSERT_EQ(chrome::GetTotalBrowserCount(), 1u);
    BrowserWindowInterface* const kiosk_browser = browser();
    ASSERT_EQ(kiosk_browser->GetTabStripModel()->count(), 1);
    content::WebContents* const contents =
        kiosk_browser->GetTabStripModel()->GetActiveWebContents();
    ASSERT_TRUE(contents);
    if (contents->IsLoading()) {
      content::WaitForLoadStop(contents);
    }
    ASSERT_EQ(
        contents->GetLastCommittedURL(),
        kiosk_.GetDefaultServerUrl(KioskMixin::SimpleWebAppOption().url_path));
  }

  void EmulateOpenNewWindowShortcutPressed() const {
    // Ctrl + N
    ui::test::EmulateFullKeyPressReleaseSequence(
        event_generator_.get(), ui::VKEY_N, /*control=*/true, /*shift=*/false,
        /*alt=*/false, /*command=*/false);
  }

  BrowserWindowInterface*
  EmulateOpenNewWindowShortcutPressedAndReturnNewBrowser() const {
    EmulateOpenNewWindowShortcutPressed();
    EXPECT_FALSE(DidKioskCloseNewWindow());
    return GetLastActiveBrowserWindowInterfaceWithAnyProfile();
  }

  void EmulateOpenTaskManagerShortcutPressed() const {
    // Esc + Search
    ui::test::EmulateFullKeyPressReleaseSequence(
        event_generator_.get(), ui::VKEY_ESCAPE, /*control=*/false,
        /*shift=*/false,
        /*alt=*/false, /*command=*/true);
  }

  void EmulateSwitchWindowsForwardShortcutPressed() const {
    // Alt+Tab
    ui::test::EmulateFullKeyPressReleaseSequence(
        event_generator_.get(), ui::VKEY_TAB, /*control=*/false,
        /*shift=*/false,
        /*alt=*/true, /*command=*/false);
  }

  void EmulateSwitchWindowsBackwardShortcutPressed() const {
    // Shift+Alt+Tab
    ui::test::EmulateFullKeyPressReleaseSequence(
        event_generator_.get(), ui::VKEY_TAB, /*control=*/false,
        /*shift=*/false,
        /*alt=*/true, /*command=*/false);
  }

  Browser& OpenForAppPopupBrowser() const {
    CurrentProfile().GetPrefs()->SetBoolean(prefs::kNewWindowsInKioskAllowed,
                                            true);
    Browser& popup_browser =
        CreatePopupBrowser(CurrentProfile(), browser()->app_name(), GURL());
    EXPECT_FALSE(DidKioskCloseNewWindow());
    return popup_browser;
  }

  bool IsLastActiveBrowserResizable() {
    views::Widget* const widget = views::Widget::GetWidgetForNativeWindow(
        GetLastActiveBrowserWindowInterfaceWithAnyProfile()
            ->GetWindow()
            ->GetNativeWindow());
    return widget->widget_delegate()->CanResize();
  }

  task_manager::TaskManagerView* GetTaskManagerView() const {
    return task_manager::TaskManagerView::GetInstanceForTests();
  }

 protected:
  std::unique_ptr<ui::test::EventGenerator> event_generator_;
  base::HistogramTester histogram;

  KioskMixin kiosk_{
      &mixin_host_,
      KioskMixin::Config{/*name=*/{},
                         KioskMixin::AutoLaunchAccount{
                             KioskMixin::SimpleWebAppOption().account_id},
                         {KioskMixin::SimpleWebAppOption()}}};
};

IN_PROC_BROWSER_TEST_F(KioskTroubleshootingToolsTest,
                       DevToolsBasicShowAndShutdown) {
  UpdateTroubleshootingToolsPolicy(/*enable=*/true);
  EnableDevTools();
  DevToolsWindowTesting::OpenDevToolsWindowSync(browser(),
                                                /*is_docked=*/false);
  ExpectOpenBrowser(chromeos::KioskBrowserWindowType::kOpenedDevToolsBrowser);

  // Shut down the session when kiosk troubleshooting tools get disabled.
  UpdateTroubleshootingToolsPolicy(/*enable=*/false);
  EXPECT_TRUE(GetKioskSystemSession().is_shutting_down());
}

IN_PROC_BROWSER_TEST_F(KioskTroubleshootingToolsTest,
                       DevToolsDisallowedNoShow) {
  UpdateTroubleshootingToolsPolicy(/*enable=*/true);

  // Devtools are not enabled, but disabled by default.
  DevToolsWindowTesting::OpenDevToolsWindowSync(browser(),
                                                /*is_docked=*/false);

  ExpectOnlyKioskAppOpen();
  histogram.ExpectTotalCount(chromeos::kKioskNewBrowserWindowHistogram, 0);
}

IN_PROC_BROWSER_TEST_F(KioskTroubleshootingToolsTest,
                       DevToolsTroubleshootingDisabled) {
  EnableDevTools();
  DevToolsWindowTesting::OpenDevToolsWindowSync(browser(),
                                                /*is_docked=*/false);

  // Since the devtools are allowed, the devtools window is open, but
  // immediately gets closed, since the kiosk troubleshooting tools are
  // disabled by the kiosk policy.
  histogram.ExpectBucketCount(
      chromeos::kKioskNewBrowserWindowHistogram,
      chromeos::KioskBrowserWindowType::kClosedRegularBrowser, 1);
  histogram.ExpectTotalCount(chromeos::kKioskNewBrowserWindowHistogram, 1);
}

IN_PROC_BROWSER_TEST_F(KioskTroubleshootingToolsTest,
                       NewWindowBasicShowAndShutdown) {
  UpdateTroubleshootingToolsPolicy(/*enable=*/true);

  EmulateOpenNewWindowShortcutPressed();
  EXPECT_FALSE(DidKioskCloseNewWindow());

  ExpectOpenBrowser(
      chromeos::KioskBrowserWindowType::kOpenedTroubleshootingNormalBrowser);

  // Shut down the session when kiosk troubleshooting tools get disabled.
  UpdateTroubleshootingToolsPolicy(/*enable=*/false);
  EXPECT_TRUE(GetKioskSystemSession().is_shutting_down());
}

IN_PROC_BROWSER_TEST_F(KioskTroubleshootingToolsTest,
                       OpenAllTroubleshootingTools) {
  UpdateTroubleshootingToolsPolicy(/*enable=*/true);
  EnableDevTools();

  DevToolsWindowTesting::OpenDevToolsWindowSync(browser(),
                                                /*is_docked=*/false);

  EmulateOpenNewWindowShortcutPressed();
  EXPECT_FALSE(DidKioskCloseNewWindow());

  EXPECT_EQ(chrome::GetTotalBrowserCount(), 3u);
  histogram.ExpectBucketCount(
      chromeos::kKioskNewBrowserWindowHistogram,
      chromeos::KioskBrowserWindowType::kOpenedDevToolsBrowser, 1);
  histogram.ExpectBucketCount(
      chromeos::kKioskNewBrowserWindowHistogram,
      chromeos::KioskBrowserWindowType::kOpenedTroubleshootingNormalBrowser, 1);
  histogram.ExpectTotalCount(chromeos::kKioskNewBrowserWindowHistogram, 2);
}

IN_PROC_BROWSER_TEST_F(KioskTroubleshootingToolsTest,
                       AllTroubleshootingToolsAreResizable) {
  UpdateTroubleshootingToolsPolicy(/*enable=*/true);
  EnableDevTools();

  // The main browser should not be resizable.
  EXPECT_FALSE(IsLastActiveBrowserResizable());

  DevToolsWindowTesting::OpenDevToolsWindowSync(browser(),
                                                /*is_docked=*/false);
  EXPECT_TRUE(IsLastActiveBrowserResizable());

  EmulateOpenNewWindowShortcutPressed();
  EXPECT_FALSE(DidKioskCloseNewWindow());
  EXPECT_TRUE(IsLastActiveBrowserResizable());
}

IN_PROC_BROWSER_TEST_F(KioskTroubleshootingToolsTest,
                       NewWindowShortcutDisallowed) {
  EmulateOpenNewWindowShortcutPressed();
  base::RunLoop().RunUntilIdle();

  ExpectOnlyKioskAppOpen();
  // Since new window shortcut will not be proceed at all, `KioskBrowserSession`
  // will not handle a new window because it was never created.
  histogram.ExpectBucketCount(
      chromeos::kKioskNewBrowserWindowHistogram,
      chromeos::KioskBrowserWindowType::kClosedRegularBrowser, 0);
  histogram.ExpectTotalCount(chromeos::kKioskNewBrowserWindowHistogram, 0);
}

IN_PROC_BROWSER_TEST_F(KioskTroubleshootingToolsTest, NewWindowAddTab) {
  UpdateTroubleshootingToolsPolicy(/*enable=*/true);

  BrowserWindowInterface* const newly_opened_browser =
      EmulateOpenNewWindowShortcutPressedAndReturnNewBrowser();
  TabStripModel* const tab_strip_model =
      newly_opened_browser->GetTabStripModel();

  ExpectOpenBrowser(
      chromeos::KioskBrowserWindowType::kOpenedTroubleshootingNormalBrowser);
  const int initial_number_of_tabs = tab_strip_model->count();

  ui_test_utils::NavigateToURLWithDisposition(
      newly_opened_browser, GURL("https://www.google.com/"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  EXPECT_EQ(tab_strip_model->count(), initial_number_of_tabs + 1);
}

IN_PROC_BROWSER_TEST_F(KioskTroubleshootingToolsTest, SwitchWindowsForward) {
  UpdateTroubleshootingToolsPolicy(/*enable=*/true);

  ui::BaseWindow* const original_window = browser()->GetWindow();
  EXPECT_TRUE(original_window->IsActive());

  ui::BaseWindow* const newly_opened_window =
      EmulateOpenNewWindowShortcutPressedAndReturnNewBrowser()->GetWindow();

  // When new window is opened, it becomes active.
  EXPECT_TRUE(newly_opened_window->IsActive());
  EXPECT_FALSE(original_window->IsActive());

  EmulateSwitchWindowsForwardShortcutPressed();

  // The main window should be active again.
  EXPECT_TRUE(original_window->IsActive());
  EXPECT_FALSE(newly_opened_window->IsActive());
}

// TODO(crbug.com/1481017): Re-enable this test
#if defined(MEMORY_SANITIZER)
#define MAYBE_SwitchWindowsBackward DISABLED_SwitchWindowsBackward
#else
#define MAYBE_SwitchWindowsBackward SwitchWindowsBackward
#endif
IN_PROC_BROWSER_TEST_F(KioskTroubleshootingToolsTest,
                       MAYBE_SwitchWindowsBackward) {
  UpdateTroubleshootingToolsPolicy(/*enable=*/true);

  ui::BaseWindow* const original_window = browser()->GetWindow();
  EXPECT_TRUE(original_window->IsActive());

  ui::BaseWindow* const newly_opened_window =
      EmulateOpenNewWindowShortcutPressedAndReturnNewBrowser()->GetWindow();

  // When new window is opened, it becomes active.
  EXPECT_TRUE(newly_opened_window->IsActive());
  EXPECT_FALSE(original_window->IsActive());

  EmulateSwitchWindowsBackwardShortcutPressed();

  // The main window should be active again.
  EXPECT_TRUE(original_window->IsActive());
  EXPECT_FALSE(newly_opened_window->IsActive());
}

IN_PROC_BROWSER_TEST_F(KioskTroubleshootingToolsTest, SwitchWindowsDisallowed) {
  EXPECT_TRUE(browser()->window()->IsActive());

  // Enable another feature to allow opening two popup browsers to make sure
  // that switching between windows is still not available if the
  // troubleshooting policy is disabled.
  Browser& new_browser = OpenForAppPopupBrowser();

  // When new window is opened, it becomes active.
  EXPECT_TRUE(new_browser.window()->IsActive());
  EXPECT_FALSE(browser()->window()->IsActive());

  EmulateSwitchWindowsForwardShortcutPressed();

  // Active window remains the same.
  EXPECT_TRUE(new_browser.window()->IsActive());
  EXPECT_FALSE(browser()->window()->IsActive());

  EmulateSwitchWindowsBackwardShortcutPressed();

  // Active window remains the same.
  EXPECT_TRUE(new_browser.window()->IsActive());
  EXPECT_FALSE(browser()->window()->IsActive());
}

IN_PROC_BROWSER_TEST_F(
    KioskTroubleshootingToolsTest,
    TaskManagerShortcutShouldNotShowIfTroubleshootingIsDisabled) {
  EmulateOpenTaskManagerShortcutPressed();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(nullptr, GetTaskManagerView());
}

IN_PROC_BROWSER_TEST_F(
    KioskTroubleshootingToolsTest,
    TaskManagerShortcutShouldShowIfTroubleshootingIsEnabled) {
  UpdateTroubleshootingToolsPolicy(/*enable=*/true);

  EmulateOpenTaskManagerShortcutPressed();
  base::RunLoop().RunUntilIdle();

  EXPECT_NE(nullptr, GetTaskManagerView());
}

IN_PROC_BROWSER_TEST_F(KioskTroubleshootingToolsTest,
                       NewDisallowedWindowShouldBeClosedIfNavigationHappens) {
  // Explicitly open a new window to make sure it will be closed.
  CreateRegularBrowser(CurrentProfile(), GURL("https://www.test.com"));
  EXPECT_TRUE(DidKioskCloseNewWindow());

  histogram.ExpectBucketCount(
      chromeos::kKioskNewBrowserWindowHistogram,
      chromeos::KioskBrowserWindowType::kClosedRegularBrowser, 1);
  histogram.ExpectTotalCount(chromeos::kKioskNewBrowserWindowHistogram, 1);
}

IN_PROC_BROWSER_TEST_F(KioskTroubleshootingToolsTest,
                       NewDisallowedWindowShouldBeHiddenIfNoNavigationHappens) {
  // Explicitly open a new window to make sure it will be hidden.
  Browser& browser = CreateRegularBrowser(CurrentProfile(), GURL());
  EXPECT_TRUE(DidKioskHideNewWindow(&browser));

  histogram.ExpectTotalCount(chromeos::kKioskNewBrowserWindowHistogram, 0);
}

}  // namespace ash
