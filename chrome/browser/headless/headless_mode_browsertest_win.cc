// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/headless/headless_mode_browsertest.h"

#include <set>

#include "base/strings/stringprintf.h"
#include "chrome/browser/headless/headless_mode_browsertest_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_switches.h"
#include "ui/compositor/layer_type.h"
#include "ui/display/screen.h"
#include "ui/display/win/screen_win_headless.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_win.h"
#include "ui/views/widget/widget.h"

namespace headless {

// A class to expose protected methods for testing purposes.
class DesktopWindowTreeHostWinWrapper : public views::DesktopWindowTreeHostWin {
 public:
  HWND GetHWND() const { return DesktopWindowTreeHostWin::GetHWND(); }
  gfx::Rect GetWindowBoundsInScreen() const override {
    return DesktopWindowTreeHostWin::GetWindowBoundsInScreen();
  }
};

namespace test {

bool IsPlatformWindowVisible(views::Widget* widget) {
  CHECK(widget);

  gfx::NativeWindow native_window = widget->GetNativeWindow();
  CHECK(native_window);

  aura::WindowTreeHost* host = native_window->GetHost();
  CHECK(host);

  gfx::AcceleratedWidget accelerated_widget = host->GetAcceleratedWidget();
  CHECK(::IsWindow(accelerated_widget));

  return !!::IsWindowVisible(accelerated_widget);
}

gfx::Rect GetPlatformWindowExpectedBounds(views::Widget* widget) {
  CHECK(widget);

  gfx::NativeWindow native_window = widget->GetNativeWindow();
  CHECK(native_window);

  DesktopWindowTreeHostWinWrapper* host =
      static_cast<DesktopWindowTreeHostWinWrapper*>(native_window->GetHost());
  CHECK(host);

  return host->GetWindowBoundsInScreen();
}

aura::Window* CreateAuraWindow(aura::Window* parent, const gfx::Rect& bounds) {
  CHECK(parent);
  aura::test::TestWindowDelegate* delegate =
      aura::test::TestWindowDelegate::CreateSelfDestroyingDelegate();
  aura::Window* window = new aura::Window(delegate);
  window->Init(ui::LayerType::LAYER_NOT_DRAWN);
  window->SetBounds(bounds);
  window->Show();
  parent->AddChild(window);
  return window;
}

}  // namespace test

namespace {

INSTANTIATE_TEST_SUITE_P(HeadlessModeBrowserTestWithStartWindowMode,
                         HeadlessModeBrowserTestWithStartWindowMode,
                         testing::Values(kStartWindowNormal,
                                         kStartWindowMaximized,
                                         kStartWindowFullscreen));

IN_PROC_BROWSER_TEST_P(HeadlessModeBrowserTestWithStartWindowMode,
                       BrowserDesktopWindowHidden) {
  // On Windows, the Native Headless Chrome browser window exists and is
  // visible, while the underlying platform window is hidden.
  EXPECT_TRUE(browser()->window()->IsVisible());

  DesktopWindowTreeHostWinWrapper* desktop_window_tree_host =
      static_cast<DesktopWindowTreeHostWinWrapper*>(
          browser()->window()->GetNativeWindow()->GetHost());
  EXPECT_FALSE(::IsWindowVisible(desktop_window_tree_host->GetHWND()));
}

IN_PROC_BROWSER_TEST_F(HeadlessModeBrowserTest,
                       ToggleFullscreenWindowVisibility) {
  DesktopWindowTreeHostWinWrapper* desktop_window_tree_host =
      static_cast<DesktopWindowTreeHostWinWrapper*>(
          browser()->window()->GetNativeWindow()->GetHost());
  HWND desktop_window_hwnd = desktop_window_tree_host->GetHWND();

  // Verify initial state.
  ASSERT_FALSE(browser()->window()->IsFullscreen());
  EXPECT_TRUE(browser()->window()->IsVisible());
  EXPECT_FALSE(::IsWindowVisible(desktop_window_hwnd));

  // Verify fullscreen state.
  ui_test_utils::ToggleFullscreenModeAndWait(browser());
  ASSERT_TRUE(browser()->window()->IsFullscreen());
  EXPECT_TRUE(browser()->window()->IsVisible());
  EXPECT_FALSE(::IsWindowVisible(desktop_window_hwnd));

  // Verify back to normal state.
  ui_test_utils::ToggleFullscreenModeAndWait(browser());
  ASSERT_FALSE(browser()->window()->IsFullscreen());
  EXPECT_TRUE(browser()->window()->IsVisible());
  EXPECT_FALSE(::IsWindowVisible(desktop_window_hwnd));
}

IN_PROC_BROWSER_TEST_F(HeadlessModeBrowserTest,
                       MinimizedRestoredWindowVisibility) {
  DesktopWindowTreeHostWinWrapper* desktop_window_tree_host =
      static_cast<DesktopWindowTreeHostWinWrapper*>(
          browser()->window()->GetNativeWindow()->GetHost());
  HWND desktop_window_hwnd = desktop_window_tree_host->GetHWND();

  // Verify initial state.
  ASSERT_FALSE(browser()->window()->IsMinimized());
  EXPECT_TRUE(browser()->window()->IsVisible());
  EXPECT_FALSE(::IsWindowVisible(desktop_window_hwnd));

  // Verify minimized state.
  browser()->window()->Minimize();
  ASSERT_TRUE(browser()->window()->IsMinimized());
  EXPECT_TRUE(browser()->window()->IsVisible());
  EXPECT_FALSE(::IsWindowVisible(desktop_window_hwnd));

  // Verify restored state.
  browser()->window()->Restore();
  ASSERT_FALSE(browser()->window()->IsMinimized());
  EXPECT_TRUE(browser()->window()->IsVisible());
  EXPECT_FALSE(::IsWindowVisible(desktop_window_hwnd));
}

IN_PROC_BROWSER_TEST_F(HeadlessModeBrowserTest,
                       MaximizedRestoredWindowVisibility) {
  DesktopWindowTreeHostWinWrapper* desktop_window_tree_host =
      static_cast<DesktopWindowTreeHostWinWrapper*>(
          browser()->window()->GetNativeWindow()->GetHost());
  HWND desktop_window_hwnd = desktop_window_tree_host->GetHWND();

  // Verify initial state.
  ASSERT_FALSE(browser()->window()->IsMaximized());
  EXPECT_TRUE(browser()->window()->IsVisible());
  EXPECT_FALSE(::IsWindowVisible(desktop_window_hwnd));

  // Verify maximized state.
  browser()->window()->Maximize();
  ASSERT_TRUE(browser()->window()->IsMaximized());
  EXPECT_TRUE(browser()->window()->IsVisible());
  EXPECT_FALSE(::IsWindowVisible(desktop_window_hwnd));

  // Verify restored state.
  browser()->window()->Restore();
  ASSERT_FALSE(browser()->window()->IsMaximized());
  EXPECT_TRUE(browser()->window()->IsVisible());
  EXPECT_FALSE(::IsWindowVisible(desktop_window_hwnd));
}

IN_PROC_BROWSER_TEST_F(HeadlessModeBrowserTestWithWindowSize, LargeWindowSize) {
  DesktopWindowTreeHostWinWrapper* desktop_window_tree_host =
      static_cast<DesktopWindowTreeHostWinWrapper*>(
          browser()->window()->GetNativeWindow()->GetHost());
  HWND desktop_window_hwnd = desktop_window_tree_host->GetHWND();

  // Expect the platform window size to be smaller than the requested window
  // size due to Windows clamping the window dimensions to the monitor work
  // area.
  RECT platform_window_rect;
  CHECK(::GetWindowRect(desktop_window_hwnd, &platform_window_rect));
  EXPECT_LT(gfx::Rect(platform_window_rect).width(), kWindowSize.width());
  EXPECT_LT(gfx::Rect(platform_window_rect).height(), kWindowSize.height());

  // Expect the reported browser window size to be the same as the requested
  // window size.
  gfx::Rect bounds = browser()->window()->GetBounds();
  EXPECT_EQ(bounds.size(), kWindowSize);
}

IN_PROC_BROWSER_TEST_F(HeadlessModeBrowserTestWithWindowSizeAndScale,
                       WindowSizeWithScale) {
  DesktopWindowTreeHostWinWrapper* desktop_window_tree_host =
      static_cast<DesktopWindowTreeHostWinWrapper*>(
          browser()->window()->GetNativeWindow()->GetHost());
  HWND desktop_window_hwnd = desktop_window_tree_host->GetHWND();

  // Expect the platform window size to be larger than the requested window size
  // due to scaling.
  RECT platform_window_rect;
  CHECK(::GetWindowRect(desktop_window_hwnd, &platform_window_rect));
  EXPECT_GT(gfx::Rect(platform_window_rect).width(), kWindowSize.width());
  EXPECT_GT(gfx::Rect(platform_window_rect).height(), kWindowSize.height());

  // Expect the reported browser window size to be the same as the requested
  // window size.
  gfx::Rect bounds = browser()->window()->GetBounds();
  EXPECT_EQ(bounds.size(), kWindowSize);
}

// display::win::ScreenWinHeadless tests -------------------------------------

class HeadlessModeBrowserTestWithScreenInfo : public HeadlessModeBrowserTest {
 public:
  HeadlessModeBrowserTestWithScreenInfo() = default;
  ~HeadlessModeBrowserTestWithScreenInfo() override = default;

  virtual std::string GetScreenInfo() { return "{}"; }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    HeadlessModeBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(::switches::kScreenInfo, GetScreenInfo());
  }

  display::win::ScreenWinHeadless* screen() const {
    return static_cast<display::win::ScreenWinHeadless*>(
        display::Screen::GetScreen());
  }
};

IN_PROC_BROWSER_TEST_F(HeadlessModeBrowserTestWithScreenInfo,
                       GetCursorScreenPoint) {
  EXPECT_TRUE(screen()->GetCursorScreenPoint().IsOrigin());

  static constexpr gfx::Point kPoint(123, 456);
  screen()->SetCursorScreenPointForTesting(kPoint);
  EXPECT_EQ(screen()->GetCursorScreenPoint(), kPoint);
}

IN_PROC_BROWSER_TEST_F(HeadlessModeBrowserTestWithScreenInfo,
                       GetWindowAtScreenPoint) {
  // Try off screen point.
  EXPECT_FALSE(screen()->GetWindowAtScreenPoint(gfx::Point(-100, -200)));

  // Try window at screen center.
  static constexpr gfx::Point kScreenCenter(800 / 2, 600 / 2);
  aura::Window* window = screen()->GetWindowAtScreenPoint(kScreenCenter);
  ASSERT_TRUE(window);
  EXPECT_TRUE(window->GetBoundsInScreen().Contains(kScreenCenter));

  // Try overlapping child window.
  gfx::Rect child_bounds = window->bounds();
  child_bounds.Inset(42);
  aura::Window* child_window = test::CreateAuraWindow(window, child_bounds);
  EXPECT_EQ(screen()->GetWindowAtScreenPoint(kScreenCenter), child_window);

  // Try overlapping hidden child window.
  child_window->Hide();
  EXPECT_NE(screen()->GetWindowAtScreenPoint(kScreenCenter), child_window);
  EXPECT_EQ(screen()->GetWindowAtScreenPoint(kScreenCenter), window);
}

IN_PROC_BROWSER_TEST_F(HeadlessModeBrowserTestWithScreenInfo,
                       GetWindowAtScreenPointOnSiblingWindows) {
  static constexpr gfx::Point kScreenCenter(800 / 2, 600 / 2);
  aura::Window* window = screen()->GetWindowAtScreenPoint(kScreenCenter);

  gfx::Rect child_bounds = window->bounds();
  child_bounds.Inset(42);

  // Try overlapping child window.
  aura::Window* child_window = test::CreateAuraWindow(window, child_bounds);
  EXPECT_EQ(screen()->GetWindowAtScreenPoint(kScreenCenter), child_window);

  // Try overlapping sibling window.
  aura::Window* child_window2 = test::CreateAuraWindow(window, child_bounds);
  EXPECT_EQ(screen()->GetWindowAtScreenPoint(kScreenCenter), child_window2);
}

IN_PROC_BROWSER_TEST_F(HeadlessModeBrowserTestWithScreenInfo,
                       GetWindowAtScreenPointOnChildWindowTree) {
  static constexpr gfx::Point kScreenCenter(800 / 2, 600 / 2);
  aura::Window* window = screen()->GetWindowAtScreenPoint(kScreenCenter);

  gfx::Rect child_bounds = window->bounds();
  child_bounds.Inset(42);

  // Try overlapping child window.
  aura::Window* child_window = test::CreateAuraWindow(window, child_bounds);
  EXPECT_EQ(screen()->GetWindowAtScreenPoint(kScreenCenter), child_window);

  // Try overlapping grandchild window.
  aura::Window* child_window2 =
      test::CreateAuraWindow(child_window, child_bounds);
  EXPECT_EQ(screen()->GetWindowAtScreenPoint(kScreenCenter), child_window2);
}

IN_PROC_BROWSER_TEST_F(HeadlessModeBrowserTestWithScreenInfo,
                       GetLocalProcessWindowAtPoint) {
  // Get window at screen center.
  static constexpr gfx::Point kScreenCenter(800 / 2, 600 / 2);
  aura::Window* window = screen()->GetWindowAtScreenPoint(kScreenCenter);
  ASSERT_TRUE(window);
  ASSERT_TRUE(window->GetBoundsInScreen().Contains(kScreenCenter));

  std::set<gfx::NativeWindow> ignore;

  // Try overlapping child window.
  gfx::Rect child_bounds = window->bounds();
  child_bounds.Inset(42);
  aura::Window* child_window = test::CreateAuraWindow(window, child_bounds);
  EXPECT_EQ(screen()->GetLocalProcessWindowAtPoint(kScreenCenter, ignore),
            child_window->GetRootWindow());

  // Try ignored overlapping child window.
  ignore.insert(child_window);
  EXPECT_EQ(screen()->GetLocalProcessWindowAtPoint(kScreenCenter, ignore),
            window->GetRootWindow());
}

class HeadlessModeBrowserTest2ndScreen
    : public HeadlessModeBrowserTestWithScreenInfo {
 public:
  HeadlessModeBrowserTest2ndScreen() = default;
  ~HeadlessModeBrowserTest2ndScreen() override = default;

  std::string GetScreenInfo() override { return "{}{}"; }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    HeadlessModeBrowserTestWithScreenInfo::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(::switches::kWindowPosition, "800,0");
  }
};

IN_PROC_BROWSER_TEST_F(HeadlessModeBrowserTest2ndScreen,
                       GetWindowAt2ndScreenPoint) {
  ASSERT_EQ(display::Screen::GetScreen()->GetNumDisplays(), 2);

  // Try off 2nd screen point.
  EXPECT_FALSE(screen()->GetWindowAtScreenPoint(gfx::Point(800 - 100, 0)));

  // Try window at the 2nd screen center.
  static constexpr gfx::Point kScreenCenter(800 + 800 / 2, 600 / 2);
  aura::Window* window = screen()->GetWindowAtScreenPoint(kScreenCenter);
  ASSERT_TRUE(window);
  EXPECT_TRUE(window->GetBoundsInScreen().Contains(kScreenCenter));
}

}  // namespace

}  // namespace headless
