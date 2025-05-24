// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

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
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
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

using policy::DeveloperToolsPolicyHandler::Availability::kAllowed;
using policy::DeveloperToolsPolicyHandler::Availability::kDisallowed;

namespace ash {

using kiosk::test::CreatePopupBrowser;
using kiosk::test::CreateRegularBrowser;
using kiosk::test::CurrentProfile;
using kiosk::test::DidKioskCloseNewWindow;
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
    ASSERT_TRUE(WaitKioskLaunched());
    SelectFirstBrowser();
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
    EXPECT_EQ(BrowserList::GetInstance()->size(), 2u);
    histogram.ExpectBucketCount(chromeos::kKioskNewBrowserWindowHistogram,
                                window_type, 1);
    histogram.ExpectTotalCount(chromeos::kKioskNewBrowserWindowHistogram, 1);
  }

  void ExpectOnlyKioskAppOpen() const {
    // The initial browser should exist in the web kiosk session.
    ASSERT_EQ(BrowserList::GetInstance()->size(), 1u);
    Browser* kiosk_browser = BrowserList::GetInstance()->get(0);
    ASSERT_EQ(kiosk_browser->tab_strip_model()->count(), 1);
    content::WebContents* contents =
        kiosk_browser->tab_strip_model()->GetActiveWebContents();
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

  Browser* EmulateOpenNewWindowShortcutPressedAndReturnNewBrowser() const {
    EmulateOpenNewWindowShortcutPressed();
    EXPECT_FALSE(DidKioskCloseNewWindow());
    return BrowserList::GetInstance()->GetLastActive();
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
        CreatePopupBrowser(CurrentProfile(), browser()->app_name());
    EXPECT_FALSE(DidKioskCloseNewWindow());
    return popup_browser;
  }

  bool IsLactActiveBrowserResizable() {
    BrowserWindow* lact_active_window =
        BrowserList::GetInstance()->GetLastActive()->window();
    views::Widget* widget = views::Widget::GetWidgetForNativeWindow(
        lact_active_window->GetNativeWindow());
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

  EXPECT_EQ(BrowserList::GetInstance()->size(), 3u);
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
  EXPECT_FALSE(IsLactActiveBrowserResizable());

  DevToolsWindowTesting::OpenDevToolsWindowSync(browser(),
                                                /*is_docked=*/false);
  EXPECT_TRUE(IsLactActiveBrowserResizable());

  EmulateOpenNewWindowShortcutPressed();
  EXPECT_FALSE(DidKioskCloseNewWindow());
  EXPECT_TRUE(IsLactActiveBrowserResizable());
}

IN_PROC_BROWSER_TEST_F(KioskTroubleshootingToolsTest,
                       NewWindowDisallowedNoShow) {
  // Explicitly open a new window to make sure it will be closed.
  CreateRegularBrowser(CurrentProfile());
  EXPECT_TRUE(DidKioskCloseNewWindow());

  histogram.ExpectBucketCount(
      chromeos::kKioskNewBrowserWindowHistogram,
      chromeos::KioskBrowserWindowType::kClosedRegularBrowser, 1);
  histogram.ExpectTotalCount(chromeos::kKioskNewBrowserWindowHistogram, 1);
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

  Browser* newly_opened_browser =
      EmulateOpenNewWindowShortcutPressedAndReturnNewBrowser();

  ExpectOpenBrowser(
      chromeos::KioskBrowserWindowType::kOpenedTroubleshootingNormalBrowser);
  int initial_number_of_tabs = newly_opened_browser->tab_strip_model()->count();

  ui_test_utils::NavigateToURLWithDisposition(
      newly_opened_browser, GURL("https://www.google.com/"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  EXPECT_EQ(newly_opened_browser->tab_strip_model()->count(),
            initial_number_of_tabs + 1);
}

IN_PROC_BROWSER_TEST_F(KioskTroubleshootingToolsTest, SwitchWindowsForward) {
  UpdateTroubleshootingToolsPolicy(/*enable=*/true);

  EXPECT_TRUE(browser()->window()->IsActive());
  Browser* newly_opened_browser =
      EmulateOpenNewWindowShortcutPressedAndReturnNewBrowser();

  // When new window is opened, it becomes active.
  EXPECT_TRUE(newly_opened_browser->window()->IsActive());
  EXPECT_FALSE(browser()->window()->IsActive());

  EmulateSwitchWindowsForwardShortcutPressed();

  // The main window should be active again.
  EXPECT_TRUE(browser()->window()->IsActive());
  EXPECT_FALSE(newly_opened_browser->window()->IsActive());
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
  EXPECT_TRUE(browser()->window()->IsActive());
  Browser* newly_opened_browser =
      EmulateOpenNewWindowShortcutPressedAndReturnNewBrowser();

  // When new window is opened, it becomes active.
  EXPECT_TRUE(newly_opened_browser->window()->IsActive());
  EXPECT_FALSE(browser()->window()->IsActive());

  EmulateSwitchWindowsBackwardShortcutPressed();

  // The main window should be active again.
  EXPECT_TRUE(browser()->window()->IsActive());
  EXPECT_FALSE(newly_opened_browser->window()->IsActive());
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

}  // namespace ash
