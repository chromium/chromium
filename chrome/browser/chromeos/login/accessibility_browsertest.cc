// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/docked_magnifier_controller.h"
#include "ash/public/cpp/keyboard/keyboard_controller.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/accessibility/magnification_manager.h"
#include "chrome/browser/chromeos/login/test/oobe_base_test.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/ui/webui_login_view.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/view_observer.h"

namespace chromeos {

class DockedMagnifierVirtualKeyboardTest
    : public OobeBaseTest,
      public views::ViewObserver,
      public ChromeKeyboardControllerClient::Observer {
 public:
  DockedMagnifierVirtualKeyboardTest() = default;
  ~DockedMagnifierVirtualKeyboardTest() override = default;

 protected:
  ChromeKeyboardControllerClient* keyboard_controller() {
    return ChromeKeyboardControllerClient::Get();
  }

  MagnificationManager* magnification_manager() {
    return MagnificationManager::Get();
  }

  WebUILoginView* webui_login_view() {
    return LoginDisplayHost::default_host()->GetWebUILoginView();
  }

  gfx::Rect GetOobeBounds() { return webui_login_view()->GetBoundsInScreen(); }

  void ShowDockedMagnifier() {
    magnification_manager()->SetDockedMagnifierEnabled(true);
    ASSERT_TRUE(magnification_manager()->IsDockedMagnifierEnabled());
    ASSERT_GT(GetMagnifierHeight(), 0);
  }

  void HideDockedMagnifier() {
    magnification_manager()->SetDockedMagnifierEnabled(false);
    ASSERT_FALSE(magnification_manager()->IsDockedMagnifierEnabled());
    ASSERT_EQ(GetMagnifierHeight(), 0);
  }

  int GetMagnifierHeight() {
    return ash::DockedMagnifierController::Get()
        ->GetMagnifierHeightForTesting();
  }

  void ShowKeyboard() {
    AccessibilityManager::Get()->EnableVirtualKeyboard(true);
    keyboard_controller()->ShowKeyboard();
    keyboard_controller()->SetKeyboardLocked(true);
    WaitForBoundsToChange();
    ASSERT_GT(GetKeyboardHeight(), 0);
  }

  void HideKeyboard() {
    keyboard_controller()->HideKeyboard(ash::HideReason::kUser);
    ASSERT_EQ(GetKeyboardHeight(), 0);
  }

  int GetKeyboardHeight() { return keyboard_bounds_.height(); }

  void WaitForBoundsToChange() {
    ASSERT_FALSE(run_loop_);
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
    run_loop_.reset();
  }

 private:
  // views::ViewObserver:
  void OnViewBoundsChanged(views::View* observed_view) override {
    ASSERT_EQ(observed_view, webui_login_view());
    if (run_loop_)
      run_loop_->Quit();
  }

  // ChromeKeyboardControllerClient::Observer:
  void OnKeyboardOccludedBoundsChanged(
      const gfx::Rect& screen_bounds) override {
    keyboard_bounds_ = screen_bounds;
  }

  // OobeBaseTest:
  void SetUpOnMainThread() override {
    OobeBaseTest::SetUpOnMainThread();
    webui_login_view()->views::View::AddObserver(this);
    keyboard_controller()->AddObserver(this);
  }

  void TearDownOnMainThread() override {
    webui_login_view()->views::View::RemoveObserver(this);
    keyboard_controller()->RemoveObserver(this);
    OobeBaseTest::TearDownOnMainThread();
  }

  std::unique_ptr<base::RunLoop> run_loop_;
  gfx::Rect keyboard_bounds_;
};

IN_PROC_BROWSER_TEST_F(DockedMagnifierVirtualKeyboardTest, WelcomeScreen) {
  const gfx::Rect original_bounds = GetOobeBounds();
  gfx::Rect expected_bounds(original_bounds);

  ShowDockedMagnifier();
  expected_bounds.Inset(0, GetMagnifierHeight(), 0, 0);
  EXPECT_EQ(expected_bounds, GetOobeBounds());

  ShowKeyboard();
  expected_bounds.Inset(0, 0, 0, GetKeyboardHeight());
  EXPECT_EQ(expected_bounds, GetOobeBounds());

  expected_bounds.Inset(0, -GetMagnifierHeight(), 0, 0);
  HideDockedMagnifier();
  EXPECT_EQ(expected_bounds, GetOobeBounds());

  HideKeyboard();
  EXPECT_EQ(original_bounds, GetOobeBounds());
}

}  // namespace chromeos
