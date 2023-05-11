// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ash/accessibility/magnifier/fullscreen_magnifier_controller.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/accessibility/accessibility_test_utils.h"
#include "chrome/browser/ash/accessibility/html_test_utils.h"
#include "chrome/browser/ash/accessibility/magnification_manager.h"
#include "chrome/browser/ash/accessibility/magnifier_animation_waiter.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/accessibility_notification_waiter.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/browsertest_util.h"
#include "extensions/browser/extension_host_test_helper.h"
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

class FullscreenMagnifierControllerTest : public InProcessBrowserTest {
 protected:
  FullscreenMagnifierControllerTest() {}

  FullscreenMagnifierControllerTest(const FullscreenMagnifierControllerTest&) =
      delete;
  FullscreenMagnifierControllerTest& operator=(
      const FullscreenMagnifierControllerTest&) = delete;

  ~FullscreenMagnifierControllerTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Make screens sufficiently wide to host 2 browsers side by side.
    command_line->AppendSwitchASCII("ash-host-window-bounds", "1200x800");
  }

  void SetUpOnMainThread() override {
    console_observer_ = std::make_unique<ExtensionConsoleErrorObserver>(
        browser()->profile(), extension_misc::kAccessibilityCommonExtensionId);

    aura::Window* root_window = Shell::Get()->GetPrimaryRootWindow();
    generator_ = std::make_unique<ui::test::EventGenerator>(root_window);
    AccessibilityManager::Get()->SetMagnifierBoundsObserverForTest(
        base::BindRepeating(
            &FullscreenMagnifierControllerTest::MagnifierBoundsChanged,
            weak_ptr_factory_.GetWeakPtr()));
  }

  // Loads a page with the given URL and then starts up Magnifier.
  void LoadURLAndMagnifier(const std::string& url) {
    content::AccessibilityNotificationWaiter waiter(
        GetWebContents(), ui::kAXModeComplete, ax::mojom::Event::kLoadComplete);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(url)));
    ASSERT_TRUE(waiter.WaitForNotification());

    LoadMagnifier();
  }

  void LoadMagnifier() {
    extensions::ExtensionHostTestHelper host_helper(
        browser()->profile(), extension_misc::kAccessibilityCommonExtensionId);
    MagnificationManager::Get()->SetMagnifierEnabled(true);

    // FullscreenMagnifierController moves the magnifier window with animation
    // when the magnifier is first enabled. It will move the mouse cursor
    // when the animation completes. Wait until the animation completes, so that
    // the mouse movement won't affect the position of magnifier window later.
    MagnifierAnimationWaiter magnifier_waiter(
        GetFullscreenMagnifierController());
    magnifier_waiter.Wait();
    host_helper.WaitForHostCompletedFirstLoad();

    // Start in a known location, centered on the screen.
    MoveMagnifierWindow(600, 400);
    ASSERT_EQ(GetViewPort().CenterPoint(), gfx::Point(600, 400));

    WaitForMagnifierJSReady();

    // Confirms that magnifier is enabled.
    ASSERT_TRUE(MagnificationManager::Get()->IsMagnifierEnabled());
    // Check default scale is as expected.
    EXPECT_EQ(2.0f, GetFullscreenMagnifierController()->GetScale());
  }

  void WaitForMagnifierJSReady() {
    base::ScopedAllowBlockingForTesting allow_blocking;
    std::string script = base::StringPrintf(R"JS(
      (async function() {
        window.accessibilityCommon.setFeatureLoadCallbackForTest('magnifier',
            () => {
              window.accessibilityCommon.magnifier_.setIsInitializingForTest(
                  false);
              chrome.test.sendScriptResult('ready');
            });
      })();
    )JS");
    base::Value result =
        extensions::browsertest_util::ExecuteScriptInBackgroundPage(
            browser()->profile(),
            extension_misc::kAccessibilityCommonExtensionId, script);
    ASSERT_EQ("ready", result);
  }

  void MoveMagnifierWindow(int x_center, int y_center) {
    gfx::Rect bounds = GetViewPort();
    GetFullscreenMagnifierController()->MoveWindow(
        x_center - bounds.width() / 2, y_center - bounds.height() / 2,
        /*animate=*/false);
    WaitForMagnifierBoundsChangedTo(gfx::Point(x_center, y_center));
  }

  void WaitForMagnifierBoundsChangedTo(gfx::Point center_point) {
    while (GetViewPort().CenterPoint() != center_point) {
      WaitForMagnifierBoundsChanged();
    }
  }

  void WaitForMagnifierBoundsChanged() {
    base::RunLoop loop;
    bounds_changed_waiter_ = loop.QuitClosure();
    loop.Run();
  }

  void MagnifierBoundsChanged() {
    if (!bounds_changed_waiter_)
      return;

    std::move(bounds_changed_waiter_).Run();

    // Wait for any additional animation to complete.
    MagnifierAnimationWaiter magnifier_waiter(
        GetFullscreenMagnifierController());
    magnifier_waiter.Wait();
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  void SetFocusOnElement(const std::string& element_id) {
    EXPECT_TRUE(
        ExecJs(GetWebContents(),
               base::StringPrintf(R"(document.getElementById('%s').focus();)",
                                  element_id.c_str())));
  }

  std::unique_ptr<ui::test::EventGenerator> generator_;

 private:
  std::unique_ptr<ExtensionConsoleErrorObserver> console_observer_;
  base::OnceClosure bounds_changed_waiter_;
  base::WeakPtrFactory<FullscreenMagnifierControllerTest> weak_ptr_factory_{
      this};
};

IN_PROC_BROWSER_TEST_F(FullscreenMagnifierControllerTest,
                       FollowFocusOnWebButton) {
  LoadURLAndMagnifier(std::string(kDataURIPrefix) + kTestHtmlContent);

  // Move magnifier window to exclude the button.
  const gfx::Rect button_bounds =
      GetControlBoundsInRoot(GetWebContents(), "test_button");
  MoveMagnifierWindow(button_bounds.right() + GetViewPort().width(),
                      button_bounds.bottom() + GetViewPort().height());
  const gfx::Rect view_port_before_focus = GetViewPort();
  EXPECT_FALSE(view_port_before_focus.Contains(button_bounds));

  // Set the focus on the button.
  SetFocusOnElement("test_button");
  WaitForMagnifierBoundsChanged();

  // Verify the magnifier window has moved to contain the button.
  const gfx::Rect view_port_after_focus = GetViewPort();
  EXPECT_TRUE(view_port_after_focus.Contains(button_bounds));
}

IN_PROC_BROWSER_TEST_F(FullscreenMagnifierControllerTest,
                       MovesContinuouslyWithMouse) {
  browser()->profile()->GetPrefs()->SetInteger(
      prefs::kAccessibilityScreenMagnifierMouseFollowingMode,
      static_cast<int>(MagnifierMouseFollowingMode::kContinuous));
  LoadMagnifier();

  // Screen resolution 1200x800.
  gfx::Point center_point(600, 400);
  EXPECT_EQ(GetViewPort().CenterPoint(), center_point);

  for (int i = 0; i < 3; i++) {
    // Move left and down. The viewport should move towards the mouse but not
    // all the way to it.
    gfx::Point mouse_point = gfx::Point(500 - i * 50, 500 + i * 50);
    generator_->MoveMouseTo(mouse_point);
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
  browser()->profile()->GetPrefs()->SetInteger(
      prefs::kAccessibilityScreenMagnifierMouseFollowingMode,
      static_cast<int>(MagnifierMouseFollowingMode::kEdge));
  LoadMagnifier();

  // Screen resolution 1200x800.
  gfx::Point initial_center = GetViewPort().CenterPoint();
  gfx::Point mouse_point(550, 450);
  generator_->MoveMouseTo(mouse_point);
  // No need to wait: Without going through the extension loop needed for focus
  // observation, the movement is all within the same process.
  EXPECT_EQ(GetViewPort().CenterPoint(), initial_center);

  // Move left and down. The viewport should not move as we are still within it.
  gfx::Point new_mouse_point = gfx::Point(500, 500);
  generator_->MoveMouseTo(new_mouse_point);
  EXPECT_EQ(GetViewPort().CenterPoint(), initial_center);

  // Move mouse to the left/bottom edge. The viewport should move in that
  // direction. Note we have to scale the mouse point based on the magnifer
  // scale to actually reach the edge of the viewport.
  new_mouse_point = gfx::Point(0, 800);
  generator_->MoveMouseTo(new_mouse_point);
  EXPECT_GT(GetViewPort().CenterPoint().x(), new_mouse_point.x());
  EXPECT_LT(GetViewPort().CenterPoint().x(), initial_center.x());
  EXPECT_LT(GetViewPort().CenterPoint().y(), new_mouse_point.y());
  EXPECT_GT(GetViewPort().CenterPoint().y(), initial_center.y());
}

IN_PROC_BROWSER_TEST_F(FullscreenMagnifierControllerTest,
                       ChangeZoomWithAccelerator) {
  LoadMagnifier();

  // Press keyboard shortcut to zoom in. Default zoom is 2.0.
  generator_->PressAndReleaseKey(ui::VKEY_BRIGHTNESS_UP,
                                 ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN);
  float scale = GetFullscreenMagnifierController()->GetScale();
  EXPECT_LT(2.0f, scale);

  // Keyboard shortcut to zoom out.
  generator_->PressAndReleaseKey(ui::VKEY_BRIGHTNESS_DOWN,
                                 ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN);
  // Note the scale might not be 2.0 again.
  EXPECT_GT(scale, GetFullscreenMagnifierController()->GetScale());
}

IN_PROC_BROWSER_TEST_F(FullscreenMagnifierControllerTest, ChangeZoomWithPrefs) {
  LoadMagnifier();

  // Change the bounds pref.
  browser()->profile()->GetPrefs()->SetDouble(
      prefs::kAccessibilityScreenMagnifierScale, 4.0);
  EXPECT_EQ(4.0, GetFullscreenMagnifierController()->GetScale());

  browser()->profile()->GetPrefs()->SetDouble(
      prefs::kAccessibilityScreenMagnifierScale, 10.5);
  EXPECT_EQ(10.5, GetFullscreenMagnifierController()->GetScale());
}

// TODO(crbug.com/261462562): Add centered mouse following mode browsertest.

}  // namespace ash
