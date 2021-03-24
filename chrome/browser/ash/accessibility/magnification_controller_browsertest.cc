// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ash/accessibility/magnifier/magnification_controller.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ash/accessibility/magnification_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

const char kDataURIPrefix[] = "data:text/html;charset=utf-8,";
const char kTestHtmlContent[] =
    "<body style=\"margin-top:0;margin-left:0\">"
    "<button type=\"button\" name=\"test_button_1\" id=\"test_button\" "
    "style=\"margin-left:200;margin-top:200;width:100;height:50\">"
    "Big Button 1</button>"
    "</body>";

aura::Window* GetRootWindow() {
  return Shell::GetPrimaryRootWindow();
}

MagnificationController* GetMagnificationController() {
  return Shell::Get()->magnification_controller();
}

bool IsMagnifierEnabled() {
  return MagnificationManager::Get()->IsMagnifierEnabled();
}

void SetMagnifierEnabled(bool enabled) {
  MagnificationManager::Get()->SetMagnifierEnabled(true);
}

void MoveMagnifierWindow(int x, int y) {
  GetMagnificationController()->MoveWindow(x, y, false);
}

gfx::Rect GetViewPort() {
  return GetMagnificationController()->GetViewportRect();
}

class MagnifierAnimationWaiter {
 public:
  explicit MagnifierAnimationWaiter(MagnificationController* controller)
      : controller_(controller) {}

  void Wait() {
    base::RepeatingTimer check_timer;
    check_timer.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(10), this,
                      &MagnifierAnimationWaiter::OnTimer);
    runner_ = new content::MessageLoopRunner;
    runner_->Run();
  }

 private:
  void OnTimer() {
    DCHECK(runner_.get());
    if (!controller_->IsOnAnimationForTesting()) {
      runner_->Quit();
    }
  }

  MagnificationController* controller_;  // not owned
  scoped_refptr<content::MessageLoopRunner> runner_;
  DISALLOW_COPY_AND_ASSIGN(MagnifierAnimationWaiter);
};

}  // namespace

class MagnificationControllerTest : public InProcessBrowserTest {
 protected:
  MagnificationControllerTest() {}
  ~MagnificationControllerTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Make screens sufficiently wide to host 2 browsers side by side.
    command_line->AppendSwitchASCII("ash-host-window-bounds", "1200x800");
  }

  void SetUpOnMainThread() override {
    SetMagnifierEnabled(true);

    // Confirms that magnifier is enabled.
    EXPECT_TRUE(IsMagnifierEnabled());
    EXPECT_EQ(2.0f, GetMagnificationController()->GetScale());

    // MagnificationController moves the magnifier window with animation
    // when the magnifier is set to be enabled. It will move the mouse cursor
    // when the animation completes. Wait until the animation completes, so that
    // the mouse movement won't affect the position of magnifier window later.
    MagnifierAnimationWaiter waiter(GetMagnificationController());
    waiter.Wait();
    base::RunLoop().RunUntilIdle();
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  void ExecuteScriptAndExtractInt(const std::string& script, int* result) {
    ASSERT_TRUE(content::ExecuteScriptAndExtractInt(
        GetWebContents(),
        "window.domAutomationController.send(" + script + ");", result));
  }

  void ExecuteScript(const std::string& script) {
    ASSERT_TRUE(content::ExecuteScript(GetWebContents(), script));
  }

  gfx::Rect GetControlBoundsInRoot(const std::string& field_id) {
    ExecuteScript("var element = document.getElementById('" + field_id +
                  "');"
                  "var bounds = element.getBoundingClientRect();");
    int top, left, width, height;
    ExecuteScriptAndExtractInt("bounds.top", &top);
    ExecuteScriptAndExtractInt("bounds.left", &left);
    ExecuteScriptAndExtractInt("bounds.width", &width);
    ExecuteScriptAndExtractInt("bounds.height", &height);
    gfx::Rect rect(top, left, width, height);

    content::RenderWidgetHostView* view =
        GetWebContents()->GetRenderWidgetHostView();
    gfx::Rect view_bounds_in_screen = view->GetViewBounds();
    gfx::Point origin = rect.origin();
    origin.Offset(view_bounds_in_screen.x(), view_bounds_in_screen.y());
    gfx::Rect rect_in_screen(origin.x(), origin.y(), rect.width(),
                             rect.height());
    ::wm::ConvertRectFromScreen(GetRootWindow(), &rect_in_screen);
    return rect_in_screen;
  }

  void SetFocusOnElement(const std::string& element_id) {
    ExecuteScript("document.getElementById('" + element_id + "').focus();");
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MagnificationControllerTest);
};

// Test is flaky on ChromeOS: crbug.com/1150753
#if BUILDFLAG(IS_CHROMEOS_ASH)
#define MAYBE_FollowFocusOnWebButtonContained \
  DISABLED_FollowFocusOnWebButtonContained
#else
#define MAYBE_FollowFocusOnWebButtonContained FollowFocusOnWebButtonContained
#endif
IN_PROC_BROWSER_TEST_F(MagnificationControllerTest,
                       MAYBE_FollowFocusOnWebButtonContained) {
  DCHECK(IsMagnifierEnabled());
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(
      browser(), GURL(std::string(kDataURIPrefix) + kTestHtmlContent)));

  // Move magnifier window to contain the button.
  const gfx::Rect button_bounds = GetControlBoundsInRoot("test_button");
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
