// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/check_deref.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_manager.h"
#include "chrome/browser/ash/login/app_mode/test/new_aura_window_watcher.h"
#include "chrome/browser/ash/login/app_mode/test/test_browser_closed_waiter.h"
#include "chrome/browser/ash/login/app_mode/test/web_kiosk_lacros_base_test.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_window.h"
#include "content/public/test/browser_test.h"

namespace ash {

namespace {

constexpr char kSettingsPage[] = "chrome://os-settings/manageAccessibility";

NavigateParams OpenBrowserWithUrl(
    const std::string& url,
    WindowOpenDisposition window_type = WindowOpenDisposition::CURRENT_TAB) {
  Profile* profile = ProfileManager::GetPrimaryUserProfile();

  NavigateParams params(profile, GURL(url), ui::PAGE_TRANSITION_AUTO_BOOKMARK);
  params.disposition = window_type;
  params.window_action = NavigateParams::SHOW_WINDOW;
  Navigate(&params);

  return params;
}

KioskSystemSession& session() {
  return CHECK_DEREF(WebKioskAppManager::Get()->kiosk_system_session());
}

bool DidSessionCloseNewWindow() {
  base::test::TestFuture<bool> future;
  session().SetOnHandleBrowserCallbackForTesting(future.GetRepeatingCallback());
  return future.Take();
}

void CloseBrowserAndWaitUntilHandled(Browser* browser) {
  TestBrowserClosedWaiter waiter{browser};
  browser->window()->Close();
  waiter.WaitUntilClosed();
}

}  // namespace

using WebKioskLacrosTest = WebKioskLacrosBaseTest;

IN_PROC_BROWSER_TEST_F(WebKioskLacrosTest, RegularOnlineKiosk) {
  if (!kiosk_ash_starter_.HasLacrosArgument()) {
    return;
  }
  NewAuraWindowWatcher watcher;
  InitializeRegularOnlineKiosk();
  aura::Window* window = watcher.WaitForWindow();

  EXPECT_TRUE(crosapi::browser_util::IsLacrosWindow(window));
  EXPECT_TRUE(crosapi::BrowserManager::Get()->IsRunning());
}

IN_PROC_BROWSER_TEST_F(WebKioskLacrosTest, ShouldCloseNewAshBrowserWindow) {
  if (!kiosk_ash_starter_.HasLacrosArgument()) {
    return;
  }
  InitializeRegularOnlineKiosk();

  OpenBrowserWithUrl("https://www.test.com");

  EXPECT_TRUE(DidSessionCloseNewWindow());
  EXPECT_FALSE(session().is_shutting_down());
}

IN_PROC_BROWSER_TEST_F(WebKioskLacrosTest, ShouldAllowSettingsWindow) {
  if (!kiosk_ash_starter_.HasLacrosArgument()) {
    return;
  }
  InitializeRegularOnlineKiosk();

  OpenBrowserWithUrl(kSettingsPage);

  EXPECT_FALSE(DidSessionCloseNewWindow());
}

IN_PROC_BROWSER_TEST_F(WebKioskLacrosTest,
                       ShouldNotEndSessionWhenSettingsWindowIsClosed) {
  if (!kiosk_ash_starter_.HasLacrosArgument()) {
    return;
  }

  InitializeRegularOnlineKiosk();
  OpenBrowserWithUrl(kSettingsPage);

  CloseBrowserAndWaitUntilHandled(BrowserList::GetInstance()->GetLastActive());

  EXPECT_FALSE(session().is_shutting_down());
}

IN_PROC_BROWSER_TEST_F(WebKioskLacrosTest, RecoverFromLacrosCrash) {
  if (!kiosk_ash_starter_.HasLacrosArgument()) {
    return;
  }
  InitializeRegularOnlineKiosk();

  NewAuraWindowWatcher watcher;

  crosapi::BrowserManager::Get()->KillLacrosForTesting();

  // Wait for a new Lacros window to be created after the crash.
  aura::Window* window = watcher.WaitForWindow();

  EXPECT_TRUE(crosapi::browser_util::IsLacrosWindow(window));
  EXPECT_TRUE(crosapi::BrowserManager::Get()->IsRunning());
}

}  // namespace ash
