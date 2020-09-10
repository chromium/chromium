// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <list>
#include <vector>

#include "base/command_line.h"
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
#include "chrome/browser/web_applications/system_web_app_manager_browsertest.h"
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

namespace {
const char* test_app_name1 = "TestApp1";
const char* test_app_name2 = "TestApp2";
}  // namespace

class SessionRestoreTestChromeOS : public InProcessBrowserTest {
 public:
  ~SessionRestoreTestChromeOS() override {}

 protected:
  void SetUpDefaultCommandLine(base::CommandLine* command_line) override {
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
    Browser* browser = new Browser(params);
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
  CreateBrowserWithParams(
      Browser::CreateParams(profile()->GetPrimaryOTRProfile(), true));
  TurnOnSessionRestore();
}

IN_PROC_BROWSER_TEST_F(SessionRestoreTestChromeOS, RestoreBrowserWindows) {
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

IN_PROC_BROWSER_TEST_F(SessionRestoreTestChromeOS, PRE_RestoreNoDevtools) {
  // Create devtools.
  CreateBrowserWithParams(Browser::CreateParams::CreateForDevTools(profile()));

  TurnOnSessionRestore();
}

IN_PROC_BROWSER_TEST_F(SessionRestoreTestChromeOS, RestoreNoDevtools) {
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

IN_PROC_BROWSER_TEST_F(SessionRestoreTestChromeOS, RestoreMaximized) {
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

IN_PROC_BROWSER_TEST_F(SessionRestoreTestChromeOS, RestoreMinimized) {
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

IN_PROC_BROWSER_TEST_F(SessionRestoreTestChromeOS, PRE_OmitTerminalApp) {
  const std::string terminal_app_name =
      web_app::GenerateApplicationNameFromAppId(
          crostini::kCrostiniTerminalSystemAppId);
  CreateBrowserWithParams(CreateParamsForApp(test_app_name1, true));
  CreateBrowserWithParams(CreateParamsForApp(terminal_app_name, true));
  TurnOnSessionRestore();
}

IN_PROC_BROWSER_TEST_F(SessionRestoreTestChromeOS, OmitTerminalApp) {
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
        web_app::TestSystemWebAppInstallation::SetUpStandaloneSingleWindowApp(
            install_from_web_app_info());
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
  Browser* app_browser = new Browser(app_params);
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

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_MANIFEST_INSTALL_P(
    SystemWebAppSessionRestoreTestChromeOS);
