// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ash/accessibility/magnifier/fullscreen_magnifier_controller.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "chrome/browser/ash/accessibility/html_test_utils.h"
#include "chrome/browser/ash/accessibility/magnification_manager.h"
#include "chrome/browser/ash/accessibility/magnifier_animation_waiter.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

const char kDataURIPrefix[] = "data:text/html;charset=utf-8,";
const char kTestHtmlContent[] =
    "<body style=\"margin-top:0;margin-left:0\">"
    "<button type=\"button\" name=\"test_button_1\" id=\"test_button\" "
    "style=\"margin-left:200;margin-top:200;width:100;height:50\">"
    "Big Button 1</button>"
    "</body>";

FullscreenMagnifierController* GetFullscreenMagnifierController() {
  return Shell::Get()->fullscreen_magnifier_controller();
}

bool IsMagnifierEnabled() {
  return MagnificationManager::Get()->IsMagnifierEnabled();
}

void SetMagnifierEnabled(bool enabled) {
  MagnificationManager::Get()->SetMagnifierEnabled(true);
}

void MoveMagnifierWindow(int x, int y) {
  GetFullscreenMagnifierController()->MoveWindow(x, y, false);
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
    SetMagnifierEnabled(true);

    // Confirms that magnifier is enabled.
    EXPECT_TRUE(IsMagnifierEnabled());
    EXPECT_EQ(2.0f, GetFullscreenMagnifierController()->GetScale());

    // FullscreenMagnifierController moves the magnifier window with animation
    // when the magnifier is set to be enabled. It will move the mouse cursor
    // when the animation completes. Wait until the animation completes, so that
    // the mouse movement won't affect the position of magnifier window later.
    MagnifierAnimationWaiter waiter(GetFullscreenMagnifierController());
    waiter.Wait();
    base::RunLoop().RunUntilIdle();
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  void SetFocusOnElement(const std::string& element_id) {
    ExecuteScript(GetWebContents(),
                  "document.getElementById('" + element_id + "').focus();");
  }
};

// Test is flaky on ChromeOS: crbug.com/1150753
IN_PROC_BROWSER_TEST_F(FullscreenMagnifierControllerTest,
                       DISABLED_FollowFocusOnWebButtonContained) {
  DCHECK(IsMagnifierEnabled());
  ASSERT_NO_FATAL_FAILURE(EXPECT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(std::string(kDataURIPrefix) + kTestHtmlContent))));

  // Move magnifier window to contain the button.
  const gfx::Rect button_bounds =
      GetControlBoundsInRoot(GetWebContents(), "test_button");
  MoveMagnifierWindow(button_bounds.x() - 100, button_bounds.y() - 100);
  const gfx::Rect view_port_before_focus = GetViewPort();
  EXPECT_TRUE(view_port_before_focus.Contains(button_bounds));

  // Set the focus on the button.
  SetFocusOnElement("test_button");

  // Verify the magnifier window is not moved and still contains the button.
  const gfx::Rect view_port_after_focus = GetViewPort();
  EXPECT_TRUE(view_port_after_focus.Contains(button_bounds));
  EXPECT_EQ(view_port_before_focus, view_port_after_focus);
}

}  // namespace ash
