// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/focus_cycler.h"
#include "ash/login/login_screen_controller.h"
#include "ash/shelf/login_shelf_view.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/test/ash_pixel_diff_test_helper.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_util.h"
#include "components/session_manager/session_manager_types.h"

namespace ash {

class LoginShelfViewPixelTest : public AshTestBase {
 public:
  LoginShelfViewPixelTest() { PrepareForPixelDiffTest(); }
  LoginShelfViewPixelTest(const LoginShelfViewPixelTest&) = delete;
  LoginShelfViewPixelTest& operator=(const LoginShelfViewPixelTest&) = delete;
  ~LoginShelfViewPixelTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    pixel_test_helper_.InitSkiaGoldPixelDiff(
        /*screenshot_prefix=*/"login_shelf_view_pixel");

    // Show the login screen.
    GetSessionControllerClient()->ShowMultiProfileLogin();
    GetSessionControllerClient()->SetSessionState(
        session_manager::SessionState::LOGIN_PRIMARY);
    Shell::Get()->login_screen_controller()->ShowLoginScreen();
  }

  AshPixelDiffTestHelper pixel_test_helper_;
};

// Verifies that the UI is expected when the login shelf shutdown button has
// the focus.
TEST_F(LoginShelfViewPixelTest, FocusOnShutdownButton) {
  views::View* shutdown_button =
      GetPrimaryShelf()->shelf_widget()->GetLoginShelfView()->GetViewByID(
          LoginShelfView::kShutdown);
  views::Widget* shutdown_button_widget = shutdown_button->GetWidget();

  // Focus on the shutdown button.
  Shell::Get()->focus_cycler()->FocusWidget(shutdown_button_widget);
  shutdown_button_widget->Activate();
  shutdown_button_widget->GetFocusManager()->SetFocusedView(shutdown_button);

  EXPECT_TRUE(
      pixel_test_helper_.ComparePrimaryFullScreen("focus_on_shutdown_button"));
}

}  // namespace ash
