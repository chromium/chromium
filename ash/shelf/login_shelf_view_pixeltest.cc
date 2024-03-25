// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/focus_cycler.h"
#include "ash/login/ui/lock_contents_view_test_api.h"
#include "ash/login/ui/lock_screen.h"
#include "ash/login/ui/login_test_base.h"
#include "ash/shelf/login_shelf_view.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/test/pixel/ash_pixel_test_init_params.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/constants/chromeos_features.h"

namespace ash {

class LoginShelfViewPixelTestBase : public LoginTestBase {
 public:
  LoginShelfViewPixelTestBase()
      : scoped_features_(chromeos::features::kJelly) {}

  // Focuses on the login shelf's shutdown button.
  void FocusOnShutdownButton() {
    views::View* shutdown_button =
        GetPrimaryShelf()->shelf_widget()->GetLoginShelfView()->GetViewByID(
            LoginShelfView::kShutdown);
    views::Widget* shutdown_button_widget = shutdown_button->GetWidget();

    Shell::Get()->focus_cycler()->FocusWidget(shutdown_button_widget);
    shutdown_button_widget->Activate();
    shutdown_button_widget->GetFocusManager()->SetFocusedView(shutdown_button);
  }

  // LoginTestBase:
  void SetUp() override {
    LoginTestBase::SetUp();
    // The wallpaper has been set when the pixel test is set up.
    ShowLoginScreen(/*set_wallpaper=*/false);

    SetUserCount(1);
    primary_big_user_view_ =
        LockContentsViewTestApi(
            LockScreen::TestApi(LockScreen::Get()).contents_view())
            .primary_big_view();
  }

  void TearDown() override {
    primary_big_user_view_ = nullptr;
    LoginTestBase::TearDown();
  }

  raw_ptr<views::View> primary_big_user_view_ = nullptr;

 private:
  // TODO(b/291622042): Remove this when the Jelly feature can no longer be
  // disabled.
  base::test::ScopedFeatureList scoped_features_;
};

class LoginShelfViewPixelTest : public LoginShelfViewPixelTestBase {
 public:
  // LoginShelfViewPixelTestBase:
  std::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    return pixel_test::InitParams();
  }
};

// Verifies that moving the focus by the tab key from the lock contents view
// to the login shelf works as expected.
TEST_F(LoginShelfViewPixelTest, FocusTraversalFromLockContents) {
  // Trigger the tab key. Verify that the login user expand button is focused.
  aura::Window* primary_shelf_window = GetPrimaryShelf()->GetWindow();
  PressAndReleaseKey(ui::VKEY_TAB);
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "focus_on_login_user_expand_button",
      /*revision_number=*/12, primary_big_user_view_.get(),
      primary_shelf_window));

  // Trigger the tab key. Check that the login shelf shutdown button is focused.
  PressAndReleaseKey(ui::VKEY_TAB);
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "focus_on_shutdown_button",
      /*revision_number=*/12, primary_big_user_view_.get(),
      primary_shelf_window));

  // Trigger the tab key. Check that the browser as guest button is focused.
  PressAndReleaseKey(ui::VKEY_TAB);
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "focus_on_browser_as_guest_button",
      /*revision_number=*/12, primary_big_user_view_.get(),
      primary_shelf_window));

  // Trigger the tab key. Check that the add person button is focused.
  PressAndReleaseKey(ui::VKEY_TAB);
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "focus_on_add_person_button",
      /*revision_number=*/12, primary_big_user_view_.get(),
      primary_shelf_window));
}

// Used to verify the login shelf features with a policy wallpaper.
TEST_F(LoginShelfViewPixelTest, FocusTraversalWithinShelf) {
  // Focus on the calendar view.
  FocusOnShutdownButton();
  PressAndReleaseKey(ui::VKEY_TAB);
  PressAndReleaseKey(ui::VKEY_TAB);
  PressAndReleaseKey(ui::VKEY_TAB);

  aura::Window* primary_shelf_window = GetPrimaryShelf()->GetWindow();
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "focus_on_calendar_view",
      /*revision_number=*/8, primary_shelf_window));

  // Focus on the time view.
  PressAndReleaseKey(ui::VKEY_TAB);
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "focus_on_time_view.rev_0",
      /*revision_number=*/8, primary_shelf_window));

  PressAndReleaseKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
  PressAndReleaseKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);

  // Move the focus back to the add person button.
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "refocus_on_login_shelf",
      /*revision_number=*/8, primary_shelf_window));
}

class LoginShelfWithPolicyWallpaperPixelTestWithRTL
    : public LoginShelfViewPixelTestBase,
      public testing::WithParamInterface<bool /*is_rtl=*/> {
 public:
  // LoginShelfViewPixelTestBase:
  std::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    pixel_test::InitParams init_params;
    init_params.wallpaper_init_type = pixel_test::WallpaperInitType::kPolicy;
    init_params.under_rtl = GetParam();
    return init_params;
  }
};

INSTANTIATE_TEST_SUITE_P(RTL,
                         LoginShelfWithPolicyWallpaperPixelTestWithRTL,
                         testing::Bool());

// Verifies that focusing on the login shelf widget with a policy wallpaper
// works as expected (see https://crbug.com/1197052).
// Test disabled due to flakiness b/293680827
TEST_P(LoginShelfWithPolicyWallpaperPixelTestWithRTL,
       DISABLED_FocusOnShutdownButton) {
  FocusOnShutdownButton();
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "focus_on_shutdown_button",
      /*revision_number=*/8, primary_big_user_view_.get(),
      GetPrimaryShelf()->GetWindow()));
}

}  // namespace ash
