// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <list>
#include <vector>

#include "base/command_line.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/test_launcher_utils.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/core/serialized_navigation_entry_test_helper.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_utils.h"
#include "ui/wm/core/wm_core_switches.h"

namespace {
const char* test_app_popup_name1 = "TestApp1";
const char* test_app_popup_name2 = "TestApp2";
}

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
    browser_list_.push_back(browser);
    return browser;
  }

  bool CloseBrowser(Browser* browser) {
    for (std::list<Browser*>::iterator iter = browser_list_.begin();
         iter != browser_list_.end(); ++iter) {
      if (*iter == browser) {
        CloseBrowserSynchronously(*iter);
        browser_list_.erase(iter);
        return true;
      }
    }
    return false;
  }

  Browser::CreateParams CreateParamsForApp(const std::string name,
                                           bool trusted) {
    return Browser::CreateParams::CreateForApp(name, trusted, gfx::Rect(),
                                               profile(), true);
  }

  // Turn on session restore before we restart.
  void TurnOnSessionRestore() {
    SessionStartupPref::SetStartupPref(
        browser()->profile(), SessionStartupPref(SessionStartupPref::LAST));
  }

  Profile* profile() { return browser()->profile(); }

  std::list<Browser*> browser_list_;
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
      Browser::CreateParams(profile()->GetOffTheRecordProfile(), true));
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
  // Create a trusted app popup.
  CreateBrowserWithParams(CreateParamsForApp(test_app_popup_name1, true));
  // Create a second trusted app with two popup windows.
  CreateBrowserWithParams(CreateParamsForApp(test_app_popup_name2, true));
  CreateBrowserWithParams(CreateParamsForApp(test_app_popup_name2, true));
  // Create a third untrusted (child) app3 popup. This should not get restored.
  CreateBrowserWithParams(CreateParamsForApp(test_app_popup_name2, false));

  TurnOnSessionRestore();
}

IN_PROC_BROWSER_TEST_F(SessionRestoreTestChromeOS, RestoreAppsV1) {
  size_t total_count = 0;
  size_t app1_count = 0;
  size_t app2_count = 0;
  for (auto* browser : *BrowserList::GetInstance()) {
    ++total_count;
    if (browser->app_name() == test_app_popup_name1)
      ++app1_count;
    if (browser->app_name() == test_app_popup_name2)
      ++app2_count;
  }
  EXPECT_EQ(1u, app1_count);
  EXPECT_EQ(2u, app2_count);  // Only the trusted app windows are restored.
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

  // Create two app popup windows and maximize the second one.
  Browser* app_browser1 =
      CreateBrowserWithParams(CreateParamsForApp(test_app_popup_name1, true));
  Browser* app_browser2 =
      CreateBrowserWithParams(CreateParamsForApp(test_app_popup_name1, true));
  app_browser2->window()->Maximize();

  EXPECT_FALSE(browser()->window()->IsMaximized());
  EXPECT_TRUE(browser2->window()->IsMaximized());
  EXPECT_FALSE(app_browser1->window()->IsMaximized());
  EXPECT_TRUE(app_browser2->window()->IsMaximized());

  TurnOnSessionRestore();
}

IN_PROC_BROWSER_TEST_F(SessionRestoreTestChromeOS, RestoreMaximized) {
  size_t total_count = 0;
  size_t maximized_count = 0;
  for (auto* browser : *BrowserList::GetInstance()) {
    ++total_count;
    if (browser->window()->IsMaximized())
      ++maximized_count;
  }
  EXPECT_EQ(4u, total_count);
  EXPECT_EQ(2u, maximized_count);
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
