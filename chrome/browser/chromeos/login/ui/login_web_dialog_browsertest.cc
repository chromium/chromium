// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/ui/login_web_dialog.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/test/widget_test.h"

namespace chromeos {

using LoginWebDialogTest = InProcessBrowserTest;

// Tests that LoginWebDialog is not minimizable.
IN_PROC_BROWSER_TEST_F(LoginWebDialogTest, CannotMinimize) {
  LoginWebDialog* dialog = new LoginWebDialog(
      browser()->profile(), nullptr, browser()->window()->GetNativeWindow(),
      base::string16(), GURL());
  dialog->Show();
  aura::Window* window = dialog->get_dialog_window_for_test();
  ASSERT_TRUE(window);
  EXPECT_EQ(0, window->GetProperty(aura::client::kResizeBehaviorKey) &
                   aura::client::kResizeBehaviorCanMinimize);
}

// Tests that LoginWebDialog can be closed by 'Shift + BrowserBack' accelerator.
IN_PROC_BROWSER_TEST_F(LoginWebDialogTest, CloseDialogByAccelerator) {
  LoginWebDialog* dialog = new LoginWebDialog(
      browser()->profile(), nullptr, browser()->window()->GetNativeWindow(),
      base::string16(), GURL());
  dialog->Show();
  gfx::NativeWindow window = dialog->get_dialog_window_for_test();
  ASSERT_TRUE(window);
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window);
  views::test::WidgetClosingObserver closing_observer(widget);
  ui::test::EventGenerator generator(window->GetRootWindow());
  generator.PressKey(ui::VKEY_BROWSER_BACK, ui::EF_SHIFT_DOWN);
  closing_observer.Wait();
  EXPECT_TRUE(closing_observer.widget_closed());
}

// Tests that LoginWebDialog does not crash with missing parent window.
IN_PROC_BROWSER_TEST_F(LoginWebDialogTest, NoParentWindow) {
  LoginWebDialog* dialog = new LoginWebDialog(
      browser()->profile(), nullptr, nullptr, base::string16(), GURL());
  dialog->Show();
  aura::Window* window = dialog->get_dialog_window_for_test();
  ASSERT_TRUE(window);
}

}  // namespace chromeos
