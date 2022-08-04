// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/focus_cycler.h"
#include "ash/login/ui/login_test_base.h"
#include "ash/shelf/login_shelf_view.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/test/ash_pixel_diff_test_helper.h"

namespace ash {

class LoginShelfViewPixelTest : public LoginTestBase {
 public:
  LoginShelfViewPixelTest() { PrepareForPixelDiffTest(); }
  LoginShelfViewPixelTest(const LoginShelfViewPixelTest&) = delete;
  LoginShelfViewPixelTest& operator=(const LoginShelfViewPixelTest&) = delete;
  ~LoginShelfViewPixelTest() override = default;

  // Returns the screenshot name prefix.
  virtual const char* GetScreenshotPrefix() const {
    return "login_shelf_view_pixel";
  }

  // LoginTestBase:
  void SetUp() override {
    LoginTestBase::SetUp();
    pixel_test_helper_.InitSkiaGoldPixelDiff(GetScreenshotPrefix());

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

// Used to verify the login shelf features with a policy wallpaper.
class LoginShelfWithPolicyWallpaperPixelTest : public LoginShelfViewPixelTest {
 public:
  LoginShelfWithPolicyWallpaperPixelTest() {
    pixel_test::InitParams init_params;
    init_params.wallpaper_init_type = pixel_test::WallpaperInitType::kPolicy;
    SetPixelTestInitParam(init_params);
  }
  LoginShelfWithPolicyWallpaperPixelTest(
      const LoginShelfWithPolicyWallpaperPixelTest&) = delete;
  LoginShelfWithPolicyWallpaperPixelTest& operator=(
      const LoginShelfWithPolicyWallpaperPixelTest&) = delete;
  ~LoginShelfWithPolicyWallpaperPixelTest() override = default;

  // LoginShelfViewPixelTest:
  const char* GetScreenshotPrefix() const override {
    return "login_shelf_view_policy_wallpaper_pixel";
  }
};

// Verifies that focusing on the login shelf widget with a policy wallpaper
// works as expected (see https://crbug.com/1197052).
TEST_F(LoginShelfWithPolicyWallpaperPixelTest, FocusOnShutdownButton) {
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
