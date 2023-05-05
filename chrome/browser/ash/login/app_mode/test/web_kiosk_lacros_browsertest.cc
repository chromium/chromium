// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/app_mode/test/web_kiosk_lacros_base_test.h"

#include "chrome/browser/ash/login/app_mode/test/new_aura_window_watcher.h"
#include "chrome/browser/ash/login/app_mode/test/web_kiosk_base_test.h"
#include "content/public/test/browser_test.h"

namespace ash {

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
