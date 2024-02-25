// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_future.h"
#include "chrome/browser/lacros/browser_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chromeos/test_util.h"
#include "chrome/browser/ui/lacros/window_utility.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/test/browser_test.h"
#include "ui/aura/window.h"

using OverviewBrowserTest = ChromeOSBrowserUITest;

// We enter overview mode with a single window. When we close the window,
// overview mode automatically exits. Check that there's no crash.
// This test is not safe to run in parallel with other lacros tests as overview
// mode applies to all processes.
IN_PROC_BROWSER_TEST_F(OverviewBrowserTest, NoCrashWithSingleWindow) {
  EnterOverviewMode();

  // Close the window by closing all tabs and wait for it to stop existing in
  // ash.
  std::string id = lacros_window_utility::GetRootWindowUniqueId(
      browser()->window()->GetNativeWindow()->GetRootWindow());
  browser()->tab_strip_model()->CloseAllTabs();
  ASSERT_TRUE(browser_test_util::WaitForWindowDestruction(id));
}

// We enter overview mode with 2 windows. We delete 1 window during overview
// mode. Then we exit overview mode.
// This test is not safe to run in parallel with other lacros tests as overview
// mode applies to all processes.
IN_PROC_BROWSER_TEST_F(OverviewBrowserTest, NoCrashTwoWindows) {
  // Create an incognito window and make it visible.
  Browser* incognito_browser = Browser::Create(Browser::CreateParams(
      browser()->profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true),
      true));
  AddBlankTabAndShow(incognito_browser);

  EnterOverviewMode();

  // Close the incognito window by closing all tabs and wait for it to stop
  // existing in ash.
  std::string incognito_id = lacros_window_utility::GetRootWindowUniqueId(
      incognito_browser->window()->GetNativeWindow()->GetRootWindow());
  incognito_browser->tab_strip_model()->CloseAllTabs();
  ASSERT_TRUE(browser_test_util::WaitForWindowDestruction(incognito_id));

  ExitOverviewMode();
}
