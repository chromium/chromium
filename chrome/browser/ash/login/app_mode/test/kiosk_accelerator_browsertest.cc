// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/shell.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "chrome/browser/app_mode/test/accelerator_helpers.h"
#include "chrome/browser/ash/app_mode/app_session_ash.h"
#include "chrome/browser/ash/app_mode/kiosk_app_types.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_manager.h"
#include "chrome/browser/ash/login/app_mode/test/ash_accelerator_helpers.h"
#include "chrome/browser/ash/login/app_mode/test/test_browser_closed_waiter.h"
#include "chrome/browser/ash/login/app_mode/test/web_kiosk_base_test.h"
#include "chrome/browser/ash/login/test/test_predicate_waiter.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

int WindowWidth(content::WebContents* web_contents) {
  return content::EvalJs(web_contents, "window.innerWidth").ExtractInt();
}

int WindowWidthAfterChange(content::WebContents* web_contents,
                           int initial_width) {
  test::TestPredicateWaiter(
      base::BindRepeating(
          [](content::WebContents* web_contents, int initial_width) {
            return initial_width != WindowWidth(web_contents);
          },
          web_contents, initial_width))
      .Wait();
  return WindowWidth(web_contents);
}

}  // anonymous namespace

// Verifies accelerator behavior in Kiosk sessions in Ash.
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

IN_PROC_BROWSER_TEST_F(WebKioskAcceleratorTest, ZoomAccelerators) {
  InitializeRegularOnlineKiosk();
  SelectFirstBrowser();

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  ASSERT_NE(browser_view, nullptr);
  content::WebContents* web_contents = browser_view->GetActiveWebContents();
  ASSERT_NE(web_contents, nullptr);

  int initial_width = WindowWidthAfterChange(web_contents, 0);
  ASSERT_GT(initial_width, 0);

  // Zoom in, content becomes larger and window width becomes smaller.
  browser_view->AcceleratorPressed(
      ui::Accelerator(ui::VKEY_ADD, ui::EF_CONTROL_DOWN));
  int zoomed_in_width = WindowWidthAfterChange(web_contents, initial_width);
  ASSERT_LT(zoomed_in_width, initial_width);

  // Restore zoom, window width becomes |initial_width|.
  browser_view->AcceleratorPressed(
      ui::Accelerator(ui::VKEY_0, ui::EF_CONTROL_DOWN));
  int restored_width = WindowWidthAfterChange(web_contents, zoomed_in_width);
  ASSERT_EQ(restored_width, initial_width);

  // Zoom out, content becomes smaller and window width becomes larger.
  browser_view->AcceleratorPressed(
      ui::Accelerator(ui::VKEY_SUBTRACT, ui::EF_CONTROL_DOWN));
  int zoomed_out_width = WindowWidthAfterChange(web_contents, zoomed_in_width);
  ASSERT_GT(zoomed_out_width, initial_width);
}

// Verifies accelerators work in regular non-kiosk sessions in Ash.
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
