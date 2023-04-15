// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/shell.h"
#include "base/run_loop.h"
#include "chrome/browser/app_mode/test/accelerator_helpers.h"
#include "chrome/browser/ash/app_mode/app_session_ash.h"
#include "chrome/browser/ash/app_mode/kiosk_app_types.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_manager.h"
#include "chrome/browser/ash/login/app_mode/test/ash_accelerator_helpers.h"
#include "chrome/browser/ash/login/app_mode/test/test_browser_closed_waiter.h"
#include "chrome/browser/ash/login/app_mode/test/web_kiosk_base_test.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

// Verify accelerators do not work in Kiosk sessions in Ash.
class WebKioskAcceleratorTest : public WebKioskBaseTest {};

IN_PROC_BROWSER_TEST_F(WebKioskAcceleratorTest, AcceleratorsDontCloseSession) {
  InitializeRegularOnlineKiosk();
  SelectFirstBrowser();
  ASSERT_EQ(BrowserList::GetInstance()->size(), 1u);
  ASSERT_FALSE(chrome::PressCloseTabAccelerator(browser()));
  ASSERT_FALSE(chrome::PressCloseWindowAccelerator(browser()));
  ASSERT_FALSE(ash::PressSignOutAccelerator());
  base::RunLoop loop;
  loop.RunUntilIdle();
  ASSERT_EQ(BrowserList::GetInstance()->size(), 1u);
  ASSERT_FALSE(WebKioskAppManager::Get()->app_session()->is_shutting_down());
}

// Verify accelerators work in regular non-kiosk sessions in Ash.
class NonKioskAcceleratorTest : public InProcessBrowserTest {};

IN_PROC_BROWSER_TEST_F(NonKioskAcceleratorTest, CloseTabAccelerator) {
  ASSERT_EQ(BrowserList::GetInstance()->size(), 1u);
  ASSERT_TRUE(chrome::PressCloseTabAccelerator(browser()));
  TestBrowserClosedWaiter settings_browser_closed_waiter{browser()};
  settings_browser_closed_waiter.WaitUntilClosed();
  ASSERT_EQ(BrowserList::GetInstance()->size(), 0u);
}

IN_PROC_BROWSER_TEST_F(NonKioskAcceleratorTest, CloseWindowAccelerator) {
  ASSERT_EQ(BrowserList::GetInstance()->size(), 1u);
  ASSERT_TRUE(chrome::PressCloseWindowAccelerator(browser()));
  TestBrowserClosedWaiter settings_browser_closed_waiter{browser()};
  settings_browser_closed_waiter.WaitUntilClosed();
  ASSERT_EQ(BrowserList::GetInstance()->size(), 0u);
}

IN_PROC_BROWSER_TEST_F(NonKioskAcceleratorTest, SignOutAccelerator) {
  ASSERT_EQ(BrowserList::GetInstance()->size(), 1u);
  ASSERT_TRUE(ash::PressSignOutAccelerator());
  TestBrowserClosedWaiter settings_browser_closed_waiter{browser()};
  settings_browser_closed_waiter.WaitUntilClosed();
  ASSERT_EQ(BrowserList::GetInstance()->size(), 0u);
}

}  // namespace ash
