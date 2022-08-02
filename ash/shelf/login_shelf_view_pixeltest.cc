// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_test_base.h"
#include "ash/test/ash_pixel_diff_test_helper.h"

namespace ash {

class LoginShelfViewPixelTest : public LoginTestBase {
 public:
  LoginShelfViewPixelTest() { PrepareForPixelDiffTest(); }
  LoginShelfViewPixelTest(const LoginShelfViewPixelTest&) = delete;
  LoginShelfViewPixelTest& operator=(const LoginShelfViewPixelTest&) = delete;
  ~LoginShelfViewPixelTest() override = default;

  // LoginTestBase:
  void SetUp() override {
    LoginTestBase::SetUp();
    pixel_test_helper_.InitSkiaGoldPixelDiff(
        /*screenshot_prefix=*/"login_shelf_view_pixel");

    // The wallpaper has been set when the pixel test is set up.
    ShowLoginScreen(/*set_wallpaper=*/false);

    SetUserCount(1);
  }

  AshPixelDiffTestHelper pixel_test_helper_;
};

// Verifies that moving the focus by the tab key from the lock contents view
// to the login shelf works as expected.
TEST_F(LoginShelfViewPixelTest, FocusTraversalFromLockContents) {
  // Trigger the tab key. Verify that the login user expand button is focused.
  PressAndReleaseKey(ui::VKEY_TAB);
  EXPECT_TRUE(pixel_test_helper_.ComparePrimaryFullScreen(
      "focus_on_login_user_expand_button"));

  // Trigger the tab key. Check that the login shelf shutdown button is focused.
  PressAndReleaseKey(ui::VKEY_TAB);
  EXPECT_TRUE(
      pixel_test_helper_.ComparePrimaryFullScreen("focus_on_shutdown_button"));

  // Trigger the tab key. Check that the browser as guest button is focused.
  PressAndReleaseKey(ui::VKEY_TAB);
  EXPECT_TRUE(pixel_test_helper_.ComparePrimaryFullScreen(
      "focus_on_browser_as_guest_button"));

  // Trigger the tab key. Check that the add person button is focused.
  PressAndReleaseKey(ui::VKEY_TAB);
  EXPECT_TRUE(pixel_test_helper_.ComparePrimaryFullScreen(
      "focus_on_add_person_button"));
}

}  // namespace ash
