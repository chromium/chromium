// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "ash/accessibility/magnifier/fullscreen_magnifier_controller.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "build/build_config.h"
#include "chrome/browser/ash/accessibility/accessibility_feature_browsertest.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/accessibility/accessibility_test_utils.h"
#include "chrome/browser/ash/accessibility/automation_test_utils.h"
#include "chrome/browser/ash/accessibility/fullscreen_magnifier_test_helper.h"
#include "chrome/browser/ash/accessibility/magnification_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/accessibility_notification_waiter.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/browsertest_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/test/event_generator.h"

namespace ash {

namespace {

const char kDataURIPrefix[] = "data:text/html;charset=utf-8,";
const char kTestHtmlContent[] =
    "<body style=\"margin-top:0;margin-left:0\">"
    "<button type=\"button\" name=\"test_button_1\" id=\"test_button\" "
    "style=\"margin-left:0;margin-top:0;width:100;height:50\">"
    "Big Button 1</button>"
    "</body>";

FullscreenMagnifierController* GetFullscreenMagnifierController() {
  return Shell::Get()->fullscreen_magnifier_controller();
}

gfx::Rect GetViewPort() {
  return GetFullscreenMagnifierController()->GetViewportRect();
}

}  // namespace

class FullscreenMagnifierControllerTest
    : public AccessibilityFeatureBrowserTest {
 public:
  FullscreenMagnifierControllerTest() = default;
  FullscreenMagnifierControllerTest(const FullscreenMagnifierControllerTest&) =
      delete;
  FullscreenMagnifierControllerTest& operator=(
      const FullscreenMagnifierControllerTest&) = delete;
  ~FullscreenMagnifierControllerTest() override = default;

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Make screens sufficiently wide to host 2 browsers side by side.
    command_line->AppendSwitchASCII("ash-host-window-bounds", "1200x800");
  }

  void SetUpOnMainThread() override {
    console_observer_ = std::make_unique<ExtensionConsoleErrorObserver>(
        GetProfile(), extension_misc::kAccessibilityCommonExtensionId);

    aura::Window* root_window = Shell::Get()->GetPrimaryRootWindow();
    generator_ = std::make_unique<ui::test::EventGenerator>(root_window);
    // Start in a known location, centered on the screen.
    helper_ =
        std::make_unique<FullscreenMagnifierTestHelper>(gfx::Point(600, 400));

    AccessibilityFeatureBrowserTest::SetUpOnMainThread();
  }

  ui::test::EventGenerator* generator() { return generator_.get(); }
  FullscreenMagnifierTestHelper* helper() { return helper_.get(); }

 private:
  std::unique_ptr<ui::test::EventGenerator> generator_;
  std::unique_ptr<FullscreenMagnifierTestHelper> helper_;
  std::unique_ptr<ExtensionConsoleErrorObserver> console_observer_;
};

IN_PROC_BROWSER_TEST_F(FullscreenMagnifierControllerTest,
                       FollowFocusOnWebButton) {
  helper()->LoadMagnifier(GetProfile());

  AutomationTestUtils utils(extension_misc::kAccessibilityCommonExtensionId);
  utils.SetUpTestSupport();
  const std::string url = std::string(kDataURIPrefix) + kTestHtmlContent;
  NavigateToUrl(GURL(url));
  utils.WaitForPageLoad(url);

  // Move magnifier window to exclude the button.
  const gfx::Rect button_bounds =
      utils.GetNodeBoundsInRoot("Big Button 1", "button");
  helper()->MoveMagnifierWindow(
      button_bounds.right() + GetViewPort().width(),
      button_bounds.bottom() + GetViewPort().height());
  const gfx::Rect view_port_before_focus = GetViewPort();
  EXPECT_FALSE(view_port_before_focus.Contains(button_bounds));

  // Set the focus on the button.
  utils.SetFocusOnNode("Big Button 1", "button");

  // Verify the magnifier window has moved to contain the button.
  helper()->WaitForMagnifierBoundsChanged();
  EXPECT_TRUE(GetViewPort().Contains(button_bounds));
}

IN_PROC_BROWSER_TEST_F(FullscreenMagnifierControllerTest,
                       AnimatesToFollowKeyboardFocus) {
  helper()->LoadMagnifier(GetProfile());

  AutomationTestUtils utils(extension_misc::kAccessibilityCommonExtensionId);
  utils.SetUpTestSupport();
  const std::string url = std::string(kDataURIPrefix) + kTestHtmlContent;
  NavigateToUrl(GURL(url));
  utils.WaitForPageLoad(url);

  // Move magnifier window to exclude the button.
  const gfx::Rect button_bounds =
      utils.GetNodeBoundsInRoot("Big Button 1", "button");
  helper()->MoveMagnifierWindow(
      button_bounds.right() + GetViewPort().width(),
      button_bounds.bottom() + GetViewPort().height());
  const gfx::Rect view_port_before_focus = GetViewPort();
  EXPECT_FALSE(view_port_before_focus.Contains(button_bounds));

  MagnifierAnimationWaiter magnifier_waiter(GetFullscreenMagnifierController());

  // Set the focus on the button.
  utils.SetFocusOnNode("Big Button 1", "button");

  // After the some animation, we should have arrived at the right viewport.
  do {
    magnifier_waiter.Wait();
  } while (!GetViewPort().Contains(button_bounds));
}

IN_PROC_BROWSER_TEST_F(FullscreenMagnifierControllerTest,
                       MovesContinuouslyWithMouse) {
  GetProfile()->GetPrefs()->SetInteger(
      prefs::kAccessibilityScreenMagnifierMouseFollowingMode,
      static_cast<int>(MagnifierMouseFollowingMode::kContinuous));
  helper()->LoadMagnifier(GetProfile());

  // Screen resolution 1200x800.
  gfx::Point center_point(600, 400);
  EXPECT_EQ(GetViewPort().CenterPoint(), center_point);

  for (int i = 0; i < 3; i++) {
    // Move left and down. The viewport should move towards the mouse but not
    // all the way to it.
    gfx::Point mouse_point = gfx::Point(500 - i * 50, 500 + i * 50);
    generator()->MoveMouseTo(mouse_point);
    // No need to wait: Without going through the extension loop needed for
    // focus observation, the movement is all within the same process.
    EXPECT_GT(GetViewPort().CenterPoint().x(), mouse_point.x());
    EXPECT_LT(GetViewPort().CenterPoint().x(), center_point.x());
    EXPECT_LT(GetViewPort().CenterPoint().y(), mouse_point.y());
    EXPECT_GT(GetViewPort().CenterPoint().y(), center_point.y());
    center_point = GetViewPort().CenterPoint();
  }
}

IN_PROC_BROWSER_TEST_F(FullscreenMagnifierControllerTest,
                       MovesWithMouseAtEdge) {
  GetProfile()->GetPrefs()->SetInteger(
      prefs::kAccessibilityScreenMagnifierMouseFollowingMode,
      static_cast<int>(MagnifierMouseFollowingMode::kEdge));
  helper()->LoadMagnifier(GetProfile());

  // Screen resolution 1200x800.
  gfx::Point initial_center = GetViewPort().CenterPoint();
  gfx::Point mouse_point(550, 450);
  generator()->MoveMouseTo(mouse_point);
  // No need to wait: Without going through the extension loop needed for focus
  // observation, the movement is all within the same process.
  EXPECT_EQ(GetViewPort().CenterPoint(), initial_center);

  // Move left and down. The viewport should not move as we are still within it.
  gfx::Point new_mouse_point = gfx::Point(500, 500);
  generator()->MoveMouseTo(new_mouse_point);
  EXPECT_EQ(GetViewPort().CenterPoint(), initial_center);

  // Move mouse to the left/bottom edge. The viewport should move in that
  // direction. Note we have to scale the mouse point based on the magnifer
  // scale to actually reach the edge of the viewport.
  new_mouse_point = gfx::Point(0, 800);
  generator()->MoveMouseTo(new_mouse_point);
  EXPECT_GT(GetViewPort().CenterPoint().x(), new_mouse_point.x());
  EXPECT_LT(GetViewPort().CenterPoint().x(), initial_center.x());
  EXPECT_LT(GetViewPort().CenterPoint().y(), new_mouse_point.y());
  EXPECT_GT(GetViewPort().CenterPoint().y(), initial_center.y());
}

IN_PROC_BROWSER_TEST_F(FullscreenMagnifierControllerTest,
                       ChangeZoomWithAccelerator) {
  helper()->LoadMagnifier(GetProfile());

  // Press keyboard shortcut to zoom in. Default zoom is 2.0.
  generator()->PressAndReleaseKeyAndModifierKeys(
      ui::VKEY_BRIGHTNESS_UP, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN);
  float scale = GetFullscreenMagnifierController()->GetScale();
  EXPECT_LT(2.0f, scale);

  // Keyboard shortcut to zoom out.
  generator()->PressAndReleaseKeyAndModifierKeys(
      ui::VKEY_BRIGHTNESS_DOWN, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN);
  // Note the scale might not be 2.0 again.
  EXPECT_GT(scale, GetFullscreenMagnifierController()->GetScale());
}

IN_PROC_BROWSER_TEST_F(FullscreenMagnifierControllerTest, ChangeZoomWithPrefs) {
  helper()->LoadMagnifier(GetProfile());

  // Change the bounds pref.
  GetProfile()->GetPrefs()->SetDouble(prefs::kAccessibilityScreenMagnifierScale,
                                      4.0);
  EXPECT_EQ(4.0, GetFullscreenMagnifierController()->GetScale());

  GetProfile()->GetPrefs()->SetDouble(prefs::kAccessibilityScreenMagnifierScale,
                                      10.5);
  EXPECT_EQ(10.5, GetFullscreenMagnifierController()->GetScale());
}

// TODO(crbug.com/261462562): Add centered mouse following mode browsertest.

}  // namespace ash
