// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <list>
#include <vector>

#include "base/command_line.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/string_number_conversions.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/ash/system_web_apps/test_support/system_web_app_browsertest_base.h"
#include "chrome/browser/ash/system_web_apps/test_support/test_system_web_app_installation.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_restore_test_helper.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/test_launcher_utils.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/core/serialized_navigation_entry_test_helper.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/wm/core/wm_core_switches.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/public/cpp/autotest_desks_api.h"
#include "chromeos/ui/wm/desks/desks_helper.h"
#endif

#if defined(USE_AURA)
#include "ui/aura/client/aura_constants.h"
#endif

namespace {
const char* test_app_name1 = "TestApp1";
const char* test_app_name2 = "TestApp2";

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Activates the desk at |index| and waits for its async operations to complete.
void SwitchToDesk(int index) {
  base::RunLoop run_loop;
  ASSERT_TRUE(ash::AutotestDesksApi().ActivateDeskAtIndex(
      index, run_loop.QuitClosure()));
  run_loop.Run();
}

// Removes all the inactive desks and waits for their async operations to
// complete.
void RemoveInactiveDesks() {
  const int kMaxDeskRemovalTries = 100;
  for (int i = 0; i < kMaxDeskRemovalTries; ++i) {
    base::RunLoop run_loop;
    if (!ash::AutotestDesksApi().RemoveActiveDesk(run_loop.QuitClosure()))
      return;
    run_loop.Run();
  }
  // This should not be reached.
  ADD_FAILURE();
}
#endif

}  // namespace

class SessionRestoreTestChromeOS : public InProcessBrowserTest {
 public:
  SessionRestoreTestChromeOS()
      : faster_animations_(
            ui::ScopedAnimationDurationScaleMode::ZERO_DURATION) {}
  ~SessionRestoreTestChromeOS() override {}

 protected:
  void SetUpDefaultCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpDefaultCommandLine(command_line);

    // Animations have caused crashes in session restore in the past but are
    // usually disabled in tests. Remove --wm-window-animations-disabled to
    // re-enable animations.
    command_line->RemoveSwitch(wm::switches::kWindowAnimationsDisabled);
  }

  Browser* CreateBrowserWithParams(Browser::CreateParams params) {
    Browser* browser = Browser::Create(params);
    AddBlankTabAndShow(browser);
    return browser;
  }

  Browser::CreateParams CreateParamsForApp(const std::string& name,
                                           bool trusted) {
    return Browser::CreateParams::CreateForApp(name, trusted, gfx::Rect(),
                                               profile(), true);
  }

  Browser::CreateParams CreateParamsForAppPopup(const std::string& name,
                                                bool trusted) {
    return Browser::CreateParams::CreateForAppPopup(name, trusted, gfx::Rect(),
                                                    profile(), true);
  }

  // Turn on session restore before we restart.
  void TurnOnSessionRestore() {
    SessionStartupPref::SetStartupPref(
        browser()->profile(), SessionStartupPref(SessionStartupPref::LAST));
  }

  Profile* profile() { return browser()->profile(); }

 private:
  ui::ScopedAnimationDurationScaleMode faster_animations_;
};

// Thse tests are in pairs. The PRE_ test creates some browser windows and
// the following test confirms that the correct windows are restored after a
// restart.

IN_PROC_BROWSER_TEST_F(SessionRestoreTestChromeOS, PRE_RestoreBrowserWindows) {
  // One browser window is always created by default.
  EXPECT_TRUE(browser());
  // Create a second normal browser window.
  CreateBrowserWithParams(Browser::CreateParams(profile(), true));
  // Create a third incognito browser window which should not get restored.
  CreateBrowserWithParams(Browser::CreateParams(
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true), true));
  TurnOnSessionRestore();
}

IN_PROC_BROWSER_TEST_F(SessionRestoreTestChromeOS, RestoreBrowserWindows) {
  size_t total_count = 0;
  size_t incognito_count = 0;
  for (Browser* browser : *BrowserList::GetInstance()) {
    ++total_count;
    if (browser->profile()->IsOffTheRecord())
      ++incognito_count;
  }
  EXPECT_EQ(2u, total_count);
  EXPECT_EQ(0u, incognito_count);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)

// Assigns three browser windows to three different desks.
IN_PROC_BROWSER_TEST_F(SessionRestoreTestChromeOS,
                       PRE_RestoreBrowserWindowsToDesks) {
  // Create two more desks so we have three desks in total.
  ash::AutotestDesksApi().CreateNewDesk();
  ash::AutotestDesksApi().CreateNewDesk();

  // A browser window is always created to the current desk, which
  // is the first desk by default.
  EXPECT_TRUE(browser());
  browser()->SetWindowUserTitle("0");

  // Create a second normal browser window in the second desk by
  // setting window workspace property.
  SwitchToDesk(1);
  Browser* browser_desk1 =
      CreateBrowserWithParams(Browser::CreateParams(profile(), true));
  browser_desk1->SetWindowUserTitle("1");
  browser_desk1->window()->GetNativeWindow()->SetProperty(
      aura::client::kWindowWorkspaceKey, 1);

  // Create a third normal browser window in the third desk
  // specified with params.initial_workspace.
  SwitchToDesk(2);
  Browser::CreateParams browser_desk2_params =
      Browser::CreateParams(profile(), true);
  browser_desk2_params.initial_workspace = "2";
  Browser* browser_desk2 = CreateBrowserWithParams(browser_desk2_params);
  browser_desk2->SetWindowUserTitle("2");

  TurnOnSessionRestore();
}

// Verifies that three windows restored to their right desk after restored. Also
// verifies that the fourth window is visible on all desks after being restored.
IN_PROC_BROWSER_TEST_F(SessionRestoreTestChromeOS,
                       RestoreBrowserWindowsToDesks) {
  auto* browser_list = BrowserList::GetInstance();
  ASSERT_EQ(3u, browser_list->size());

  // The first, second and third browser should restore to the first, second
  // and third desk, consecutively.
  for (int i = 0; i < 3; i++) {
    auto* browser = browser_list->get(i);
    int desk_index = 0;
    ASSERT_TRUE(base::StringToInt(browser->initial_workspace(), &desk_index));
    // Verify that browser i_th with title i, has initial_workspace equals to
    // desk i_th.
    ASSERT_EQ(i, desk_index);
    ASSERT_EQ(base::NumberToString(i), browser->user_title());

    // Check that a browser window is restored to the right desk i_th.
    ASSERT_TRUE(ash::AutotestDesksApi().IsWindowInDesk(
        browser->window()->GetNativeWindow(), desk_index));
    int workspace = browser->window()->GetNativeWindow()->GetProperty(
        aura::client::kWindowWorkspaceKey);
    ASSERT_EQ(desk_index,
              workspace == aura::client::kWindowWorkspaceUnassignedWorkspace
                  ? 0
                  : workspace);
  }

  RemoveInactiveDesks();
}

// Assigns a browser window to all desks.
IN_PROC_BROWSER_TEST_F(SessionRestoreTestChromeOS,
                       PRE_RestoreAllDesksBrowserWindow) {
  // Create two desks so we have three in total.
  ash::AutotestDesksApi().CreateNewDesk();
  ash::AutotestDesksApi().CreateNewDesk();

  // Create a browser that is visible on all desks.
  Browser::CreateParams visible_on_all_desks_browser_params =
      Browser::CreateParams(profile(), true);
  visible_on_all_desks_browser_params.initial_visible_on_all_workspaces_state =
      true;
  Browser* visible_on_all_desks_browser =
      CreateBrowserWithParams(visible_on_all_desks_browser_params);

  // Ensure the visible on all desks browser has the right properties.
  auto* visible_on_all_desks_window =
      visible_on_all_desks_browser->window()->GetNativeWindow();
  ASSERT_TRUE(visible_on_all_desks_window->GetProperty(
                  aura::client::kWindowWorkspaceKey) ==
              aura::client::kWindowWorkspaceVisibleOnAllWorkspaces);
  ASSERT_TRUE(chromeos::DesksHelper::Get(visible_on_all_desks_window)
                  ->BelongsToActiveDesk(visible_on_all_desks_window));

  // Check that there are two browsers, the default one and the visible on all
  // desks browser.
  auto* browser_list = BrowserList::GetInstance();
  ASSERT_EQ(2u, browser_list->size());

  TurnOnSessionRestore();
}

// Verifies that the visible on all desks browser window is restore properly.
IN_PROC_BROWSER_TEST_F(SessionRestoreTestChromeOS,
                       RestoreAllDesksBrowserWindow) {
  // There should be two browsers restored, the default browser and the all
  // desks browser.
  auto* browser_list = BrowserList::GetInstance();
  ASSERT_EQ(2u, browser_list->size());

  // Check that the visible on all desks browser is restored properly.
  auto* visible_on_all_desks_browser = browser_list->get(1);
  auto* visible_on_all_desks_window =
      visible_on_all_desks_browser->window()->GetNativeWindow();

  EXPECT_EQ("", visible_on_all_desks_browser->initial_workspace());

  EXPECT_TRUE(visible_on_all_desks_window->GetProperty(
                  aura::client::kWindowWorkspaceKey) ==
              aura::client::kWindowWorkspaceVisibleOnAllWorkspaces);
  // Visible on all desks windows should always reside on the active desk,
  // even if there is a desk switch.
  EXPECT_TRUE(chromeos::DesksHelper::Get(visible_on_all_desks_window)
                  ->BelongsToActiveDesk(visible_on_all_desks_window));

  RemoveInactiveDesks();
}
#endif

IN_PROC_BROWSER_TEST_F(SessionRestoreTestChromeOS, PRE_RestoreAppsV1) {
  // Create a trusted app.
  CreateBrowserWithParams(CreateParamsForApp(test_app_name1, true));
  // Create a second trusted app with two windows.
  CreateBrowserWithParams(CreateParamsForApp(test_app_name2, true));
  CreateBrowserWithParams(CreateParamsForApp(test_app_name2, true));
  // Create a third untrusted (child) app3. This should not get restored.
  CreateBrowserWithParams(CreateParamsForApp(test_app_name2, false));

  TurnOnSessionRestore();
}

IN_PROC_BROWSER_TEST_F(SessionRestoreTestChromeOS, RestoreAppsV1) {
  size_t total_count = 0;
  size_t app1_count = 0;
  size_t app2_count = 0;
  for (Browser* browser : *BrowserList::GetInstance()) {
    ++total_count;
    if (browser->app_name() == test_app_name1)
      ++app1_count;
    if (browser->app_name() == test_app_name2)
      ++app2_count;
  }
  EXPECT_EQ(1u, app1_count);
  EXPECT_EQ(2u, app2_count);   // Only the trusted app windows are restored.
  EXPECT_EQ(4u, total_count);  // Default browser() + 3 app windows
}

IN_PROC_BROWSER_TEST_F(SessionRestoreTestChromeOS, PRE_RestoreAppsPopup) {
  // Create a trusted app popup.
  CreateBrowserWithParams(CreateParamsForAppPopup(test_app_name1, true));
  // Create a second trusted app popup with two windows.
  CreateBrowserWithParams(CreateParamsForAppPopup(test_app_name2, true));
  CreateBrowserWithParams(CreateParamsForAppPopup(test_app_name2, true));
  // Create a third untrusted (child) app popup. This should not get restored.
  CreateBrowserWithParams(CreateParamsForAppPopup(test_app_name2, false));

  TurnOnSessionRestore();
}

IN_PROC_BROWSER_TEST_F(SessionRestoreTestChromeOS, RestoreAppsPopup) {
  size_t total_count = 0;
  size_t app1_count = 0;
  size_t app2_count = 0;
  for (Browser* browser : *BrowserList::GetInstance()) {
    ++total_count;
    if (browser->app_name() == test_app_name1)
      ++app1_count;
    if (browser->app_name() == test_app_name2)
      ++app2_count;
  }
  EXPECT_EQ(1u, app1_count);
  EXPECT_EQ(2u, app2_count);   // Only the trusted app windows are restored.
  EXPECT_EQ(4u, total_count);  // Default browser() + 3 app windows
}

IN_PROC_BROWSER_TEST_F(SessionRestoreTestChromeOS, PRE_RestoreNoDevtools) {
  // Create devtools.
  CreateBrowserWithParams(Browser::CreateParams::CreateForDevTools(profile()));

  TurnOnSessionRestore();
}

IN_PROC_BROWSER_TEST_F(SessionRestoreTestChromeOS, RestoreNoDevtools) {
  size_t total_count = 0;
  size_t devtools_count = 0;
  for (Browser* browser : *BrowserList::GetInstance()) {
    ++total_count;
    if (browser->is_type_devtools())
      ++devtools_count;
  }
  EXPECT_EQ(1u, total_count);
  EXPECT_EQ(0u, devtools_count);
}

IN_PROC_BROWSER_TEST_F(SessionRestoreTestChromeOS, PRE_RestoreMaximized) {
  // One browser window is always created by default.
  ASSERT_TRUE(browser());
  // Create a second browser window and maximize it.
  Browser* browser2 =
      CreateBrowserWithParams(Browser::CreateParams(profile(), true));
  browser2->window()->Maximize();

  // Create two app windows and maximize the second one.
  Browser* app_browser1 =
      CreateBrowserWithParams(CreateParamsForApp(test_app_name1, true));
  Browser* app_browser2 =
      CreateBrowserWithParams(CreateParamsForApp(test_app_name2, true));
  app_browser2->window()->Maximize();

  // Create two app popup windows and maximize the second one.
  Browser* app_popup_browser1 =
      CreateBrowserWithParams(CreateParamsForAppPopup(test_app_name1, true));
  Browser* app_popup_browser2 =
      CreateBrowserWithParams(CreateParamsForAppPopup(test_app_name2, true));
  app_popup_browser2->window()->Maximize();

  EXPECT_FALSE(browser()->window()->IsMaximized());
  EXPECT_TRUE(browser2->window()->IsMaximized());
  EXPECT_FALSE(app_browser1->window()->IsMaximized());
  EXPECT_TRUE(app_browser2->window()->IsMaximized());
  EXPECT_FALSE(app_popup_browser1->window()->IsMaximized());
  EXPECT_TRUE(app_popup_browser2->window()->IsMaximized());

  TurnOnSessionRestore();
}

// https://crbug.com/1216209
IN_PROC_BROWSER_TEST_F(SessionRestoreTestChromeOS, DISABLED_RestoreMaximized) {
  size_t total_count = 0;
  size_t app1_maximized_count = 0;
  size_t app2_maximized_count = 0;
  size_t total_maximized_count = 0;
  for (Browser* browser : *BrowserList::GetInstance()) {
    ++total_count;
    if (browser->window()->IsMaximized()) {
      ++total_maximized_count;
      if (browser->app_name() == test_app_name1)
        ++app1_maximized_count;
      if (browser->app_name() == test_app_name2)
        ++app2_maximized_count;
    }
  }
  EXPECT_EQ(6u, total_count);
  EXPECT_EQ(0u, app1_maximized_count);
  EXPECT_EQ(2u, app2_maximized_count);  // One TYPE_APP + One TYPE_APP_POPUP
  EXPECT_EQ(3u, total_maximized_count);
}

// Test for crash when restoring minimized windows. http://crbug.com/679513.
IN_PROC_BROWSER_TEST_F(SessionRestoreTestChromeOS, PRE_RestoreMinimized) {
  // One browser window is always created by default.
  ASSERT_TRUE(browser());
  browser()->window()->Minimize();

  Browser* browser2 =
      CreateBrowserWithParams(Browser::CreateParams(profile(), true));
  browser2->window()->Minimize();

  EXPECT_TRUE(browser()->window()->IsMinimized());
  EXPECT_TRUE(browser2->window()->IsMinimized());

  TurnOnSessionRestore();
}

// https://crbug.com/1216209
IN_PROC_BROWSER_TEST_F(SessionRestoreTestChromeOS, DISABLED_RestoreMinimized) {
  size_t total_count = 0;
  size_t minimized_count = 0;
  for (Browser* browser : *BrowserList::GetInstance()) {
    ++total_count;
    if (browser->window()->IsMinimized())
      ++minimized_count;
  }
  EXPECT_EQ(2u, total_count);
  // Chrome OS always activates the last browser windows on login to remind
  // users they have a browser running instead of just showing them an empty
  // desktop.
  EXPECT_NE(2u, minimized_count);
}

class SystemWebAppSessionRestoreTestChromeOS
    : public TestProfileTypeMixin<ash::SystemWebAppBrowserTestBase> {
 public:
  SystemWebAppSessionRestoreTestChromeOS() {
    auto installation =
        ash::TestSystemWebAppInstallation::SetUpStandaloneSingleWindowApp();
    installation->set_update_policy(
        ash::SystemWebAppManager::UpdatePolicy::kOnVersionChange);
    SetSystemWebAppInstallation(std::move(installation));
  }

  ~SystemWebAppSessionRestoreTestChromeOS() override = default;

 protected:
  SessionRestoreTestHelper waiter_;
};

IN_PROC_BROWSER_TEST_P(SystemWebAppSessionRestoreTestChromeOS,
                       PRE_OmitSystemWebApps) {
  // Wait for the app to install, launch, and load, otherwise the app might not
  // be restored.
  WaitForTestSystemAppInstall();
  LaunchApp(GetAppType());

  // Should have one SWA window and one default browser window.
  EXPECT_TRUE(ash::FindSystemWebAppBrowser(browser()->profile(), GetAppType()));
  EXPECT_EQ(2u, BrowserList::GetInstance()->size());

  SessionStartupPref::SetStartupPref(
      browser()->profile(), SessionStartupPref(SessionStartupPref::LAST));
}

IN_PROC_BROWSER_TEST_P(SystemWebAppSessionRestoreTestChromeOS,
                       OmitSystemWebApps) {
  waiter_.Wait();

  // Should have only one default browser window.
  //
  // Session restore doesn't go through system web app launch path, so system
  // web app utils like `FindSystemWebAppBrowser` might not recognize such
  // windows as a SWA browser window. Therefore we count the number of browser
  // windows here instead of trying to find one.
  EXPECT_EQ(1u, BrowserList::GetInstance()->size());
}

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    SystemWebAppSessionRestoreTestChromeOS);
