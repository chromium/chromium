// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <list>
#include <vector>

#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/system_web_apps/test/system_web_app_browsertest_base.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/test_launcher_utils.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/core/serialized_navigation_entry_test_helper.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "ui/wm/core/wm_core_switches.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/autotest_desks_api.h"
#include "ash/public/cpp/desks_helper.h"
#endif

#if defined(USE_AURA)
#include "ui/aura/client/aura_constants.h"
#endif

namespace {
const char* test_app_name1 = "TestApp1";
const char* test_app_name2 = "TestApp2";
}  // namespace

// SessionRestoreTestChromeOS with boolean test param for testing with
// Bento, which contains desks restore feature, enabled and disabled.
class SessionRestoreTestChromeOS : public InProcessBrowserTest,
                                   public ::testing::WithParamInterface<bool> {
 public:
  ~SessionRestoreTestChromeOS() override {}

 protected:
  void SetUpDefaultCommandLine(base::CommandLine* command_line) override {
    if (GetParam())
      scoped_feature_list_.InitAndEnableFeature(ash::features::kBento);
    base::CommandLine default_command_line(base::CommandLine::NO_PROGRAM);
    InProcessBrowserTest::SetUpDefaultCommandLine(&default_command_line);

    // Animations have caused crashes in session restore in the past but are
    // usually disabled in tests. Remove --wm-window-animations-disabled to
    // re-enable animations.
    test_launcher_utils::RemoveCommandLineSwitch(
        default_command_line, wm::switches::kWindowAnimationsDisabled,
        command_line);
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

  bool IsBentoEnabled() const { return GetParam(); }

  Profile* profile() { return browser()->profile(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Thse tests are in pairs. The PRE_ test creates some browser windows and
// the following test confirms that the correct windows are restored after a
// restart.

IN_PROC_BROWSER_TEST_P(SessionRestoreTestChromeOS, PRE_RestoreBrowserWindows) {
  // One browser window is always created by default.
  EXPECT_TRUE(browser());
  // Create a second normal browser window.
  CreateBrowserWithParams(Browser::CreateParams(profile(), true));
  // Create a third incognito browser window which should not get restored.
  CreateBrowserWithParams(
      Browser::CreateParams(profile()->GetPrimaryOTRProfile(), true));
  TurnOnSessionRestore();
}

IN_PROC_BROWSER_TEST_P(SessionRestoreTestChromeOS, RestoreBrowserWindows) {
  size_t total_count = 0;
  size_t incognito_count = 0;
  for (auto* browser : *BrowserList::GetInstance()) {
    ++total_count;
    if (browser->profile()->IsOffTheRecord())
      ++incognito_count;
  }
  EXPECT_EQ(2u, total_count);
  EXPECT_EQ(0u, incognito_count);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)

// Assigns three browser windows to three different desks. Assign a fourth
// browser window to all desks if Bento is enabled.
IN_PROC_BROWSER_TEST_P(SessionRestoreTestChromeOS,
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
  Browser* browser_desk1 =
      CreateBrowserWithParams(Browser::CreateParams(profile(), true));
  browser_desk1->SetWindowUserTitle("1");
  if (IsBentoEnabled()) {
    browser_desk1->window()->GetNativeWindow()->SetProperty(
        aura::client::kWindowWorkspaceKey, 1);
  }

  // Create a third normal browser window in the third desk
  // specified with params.initial_workspace.
  Browser::CreateParams browser_desk2_params =
      Browser::CreateParams(profile(), true);
  if (IsBentoEnabled())
    browser_desk2_params.initial_workspace = "2";
  Browser* browser_desk2 = CreateBrowserWithParams(browser_desk2_params);
  browser_desk2->SetWindowUserTitle("2");

  // Create a fourth browser window and make it visible on all desks if Bento is
  // enabled.
  if (IsBentoEnabled()) {
    ash::AutotestDesksApi().ActivateDeskAtIndex(0, base::DoNothing());

    Browser::CreateParams visible_on_all_desks_browser_params =
        Browser::CreateParams(profile(), true);
    visible_on_all_desks_browser_params
        .initial_visible_on_all_workspaces_state = true;
    Browser* visible_on_all_desks_browser =
        CreateBrowserWithParams(visible_on_all_desks_browser_params);

    auto* visible_on_all_desks_window =
        visible_on_all_desks_browser->window()->GetNativeWindow();
    ASSERT_TRUE(visible_on_all_desks_window->GetProperty(
        aura::client::kVisibleOnAllWorkspacesKey));
    ASSERT_TRUE(ash::DesksHelper::Get()->BelongsToActiveDesk(
        visible_on_all_desks_window));
  }

  TurnOnSessionRestore();
}

// Verifies that three windows restored to their right desk after restored. Also
// verifies that the fourth window is visible on all desks after being restored
// if Bento is enabled.
IN_PROC_BROWSER_TEST_P(SessionRestoreTestChromeOS,
                       RestoreBrowserWindowsToDesks) {
  auto* browser_list = BrowserList::GetInstance();
  ASSERT_EQ(IsBentoEnabled() ? 4u : 3u, browser_list->size());

  // The first, second and third browser should restore to the first, second
  // and third desk, consecutively.
  for (int i = 0; i < 3; i++) {
    auto* browser = browser_list->get(i);
    int desk_index = 0;
    if (IsBentoEnabled()) {
      ASSERT_TRUE(base::StringToInt(browser->initial_workspace(), &desk_index));
      // Verify that browser i_th with title i, has initial_workspace equals
      // to desk i_th.
      ASSERT_EQ(i, desk_index);
      ASSERT_EQ(base::NumberToString(i), browser->user_title());
    }

    // Check that a browser window is restored to the right desk, desk i_th
    // if Bento desks restore is enabled. Otherwiser it should restores to the
    // default first desk.
    ASSERT_TRUE(ash::AutotestDesksApi().IsWindowInDesk(
        browser->window()->GetNativeWindow(),
        IsBentoEnabled() ? desk_index : 0));
    int workspace = browser->window()->GetNativeWindow()->GetProperty(
        aura::client::kWindowWorkspaceKey);
    ASSERT_EQ(desk_index,
              workspace == aura::client::kUnassignedWorkspace ? 0 : workspace);
  }

  // If Bento is enabled, there should be a fourth browser that should be
  // visible on all desks.
  if (IsBentoEnabled()) {
    auto* visible_on_all_desks_browser = browser_list->get(3);
    auto* visible_on_all_desks_window =
        visible_on_all_desks_browser->window()->GetNativeWindow();
    ASSERT_TRUE(visible_on_all_desks_window->GetProperty(
        aura::client::kVisibleOnAllWorkspacesKey));
    // Visible on all desks windows should always reside on the active desk,
    // even if there is a desk switch.
    ASSERT_TRUE(ash::DesksHelper::Get()->BelongsToActiveDesk(
        visible_on_all_desks_window));
  }
}
#endif

IN_PROC_BROWSER_TEST_P(SessionRestoreTestChromeOS, PRE_RestoreAppsV1) {
  // Create a trusted app.
  CreateBrowserWithParams(CreateParamsForApp(test_app_name1, true));
  // Create a second trusted app with two windows.
  CreateBrowserWithParams(CreateParamsForApp(test_app_name2, true));
  CreateBrowserWithParams(CreateParamsForApp(test_app_name2, true));
  // Create a third untrusted (child) app3. This should not get restored.
  CreateBrowserWithParams(CreateParamsForApp(test_app_name2, false));

  TurnOnSessionRestore();
}

IN_PROC_BROWSER_TEST_P(SessionRestoreTestChromeOS, RestoreAppsV1) {
  size_t total_count = 0;
  size_t app1_count = 0;
  size_t app2_count = 0;
  for (auto* browser : *BrowserList::GetInstance()) {
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

IN_PROC_BROWSER_TEST_P(SessionRestoreTestChromeOS, PRE_RestoreAppsPopup) {
  // Create a trusted app popup.
  CreateBrowserWithParams(CreateParamsForAppPopup(test_app_name1, true));
  // Create a second trusted app popup with two windows.
  CreateBrowserWithParams(CreateParamsForAppPopup(test_app_name2, true));
  CreateBrowserWithParams(CreateParamsForAppPopup(test_app_name2, true));
  // Create a third untrusted (child) app popup. This should not get restored.
  CreateBrowserWithParams(CreateParamsForAppPopup(test_app_name2, false));

  TurnOnSessionRestore();
}

IN_PROC_BROWSER_TEST_P(SessionRestoreTestChromeOS, RestoreAppsPopup) {
  size_t total_count = 0;
  size_t app1_count = 0;
  size_t app2_count = 0;
  for (auto* browser : *BrowserList::GetInstance()) {
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

IN_PROC_BROWSER_TEST_P(SessionRestoreTestChromeOS, PRE_RestoreNoDevtools) {
  // Create devtools.
  CreateBrowserWithParams(Browser::CreateParams::CreateForDevTools(profile()));

  TurnOnSessionRestore();
}

IN_PROC_BROWSER_TEST_P(SessionRestoreTestChromeOS, RestoreNoDevtools) {
  size_t total_count = 0;
  size_t devtools_count = 0;
  for (auto* browser : *BrowserList::GetInstance()) {
    ++total_count;
    if (browser->is_type_devtools())
      ++devtools_count;
  }
  EXPECT_EQ(1u, total_count);
  EXPECT_EQ(0u, devtools_count);
}

IN_PROC_BROWSER_TEST_P(SessionRestoreTestChromeOS, PRE_RestoreMaximized) {
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

IN_PROC_BROWSER_TEST_P(SessionRestoreTestChromeOS, RestoreMaximized) {
  size_t total_count = 0;
  size_t app1_maximized_count = 0;
  size_t app2_maximized_count = 0;
  size_t total_maximized_count = 0;
  for (auto* browser : *BrowserList::GetInstance()) {
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
IN_PROC_BROWSER_TEST_P(SessionRestoreTestChromeOS, PRE_RestoreMinimized) {
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

IN_PROC_BROWSER_TEST_P(SessionRestoreTestChromeOS, RestoreMinimized) {
  size_t total_count = 0;
  size_t minimized_count = 0;
  for (auto* browser : *BrowserList::GetInstance()) {
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

IN_PROC_BROWSER_TEST_P(SessionRestoreTestChromeOS, PRE_OmitTerminalApp) {
  const std::string terminal_app_name =
      web_app::GenerateApplicationNameFromAppId(
          crostini::kCrostiniTerminalSystemAppId);
  CreateBrowserWithParams(CreateParamsForApp(test_app_name1, true));
  CreateBrowserWithParams(CreateParamsForApp(terminal_app_name, true));
  TurnOnSessionRestore();
}

IN_PROC_BROWSER_TEST_P(SessionRestoreTestChromeOS, OmitTerminalApp) {
  const std::string terminal_app_name =
      web_app::GenerateApplicationNameFromAppId(
          crostini::kCrostiniTerminalSystemAppId);
  size_t total_count = 0;
  for (auto* browser : *BrowserList::GetInstance()) {
    ++total_count;
    EXPECT_NE(terminal_app_name, browser->app_name());
  }
  // We should only count browser() and test_app_name1.
  EXPECT_EQ(2u, total_count);
}

class SystemWebAppSessionRestoreTestChromeOS
    : public web_app::SystemWebAppManagerBrowserTest {
 public:
  SystemWebAppSessionRestoreTestChromeOS()
      : SystemWebAppManagerBrowserTest(/*install_mock=*/false) {
    maybe_installation_ =
        web_app::TestSystemWebAppInstallation::SetUpStandaloneSingleWindowApp();
    maybe_installation_->set_update_policy(
        web_app::SystemWebAppManager::UpdatePolicy::kOnVersionChange);
  }

  ~SystemWebAppSessionRestoreTestChromeOS() override = default;

 protected:
  size_t GetNumBrowsers() { return BrowserList::GetInstance()->size(); }
};

IN_PROC_BROWSER_TEST_P(SystemWebAppSessionRestoreTestChromeOS,
                       PRE_OmitSystemWebApps) {
  // Wait for the app to install, launch, and load, otherwise the app might not
  // be restored.
  WaitForTestSystemAppInstall();
  LaunchApp(GetMockAppType());

  auto app_params = Browser::CreateParams::CreateForApp(
      test_app_name1, true, gfx::Rect(), browser()->profile(), true);
  Browser* app_browser = Browser::Create(app_params);
  AddBlankTabAndShow(app_browser);

  // There should be three browsers:
  //   1. The SWA browser
  //   2. The |test_app_name1| browser
  //   3. The main browser window
  EXPECT_EQ(3u, GetNumBrowsers());

  SessionStartupPref::SetStartupPref(
      browser()->profile(), SessionStartupPref(SessionStartupPref::LAST));
}

IN_PROC_BROWSER_TEST_P(SystemWebAppSessionRestoreTestChromeOS,
                       OmitSystemWebApps) {
  // There should only be two browsers:
  //  1. The |test_app_name1| browser
  //  2. The main browser window
  EXPECT_EQ(2u, GetNumBrowsers());
  for (auto* browser : *BrowserList::GetInstance()) {
    EXPECT_TRUE(browser->app_name().empty() ||
                browser->app_name() == test_app_name1);
  }
}

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    SystemWebAppSessionRestoreTestChromeOS);
INSTANTIATE_TEST_SUITE_P(All, SessionRestoreTestChromeOS, ::testing::Bool());
