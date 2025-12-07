// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/headless/headless_mode_browsertest.h"

#include "chrome/browser/headless/headless_mode_browsertest_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host_platform.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/platform_window/platform_window.h"
#include "ui/views/widget/widget.h"

namespace headless {

namespace test {

bool IsPlatformWindowVisible(views::Widget* widget) {
  CHECK(widget);

  gfx::NativeWindow native_window = widget->GetNativeWindow();
  CHECK(native_window);

  aura::WindowTreeHostPlatform* host =
      aura::WindowTreeHostPlatform::GetHostForWindow(native_window);
  CHECK(host);

  ui::PlatformWindow* platform_window = host->platform_window();
  CHECK(platform_window);

  return platform_window->IsVisible();
}

gfx::Rect GetPlatformWindowExpectedBounds(views::Widget* widget) {
  CHECK(widget);

  gfx::NativeWindow native_window = widget->GetNativeWindow();
  CHECK(native_window);

  aura::WindowTreeHostPlatform* host =
      aura::WindowTreeHostPlatform::GetHostForWindow(native_window);
  CHECK(host);

  ui::PlatformWindow* platform_window = host->platform_window();
  CHECK(platform_window);

  return platform_window->GetBoundsInPixels();
}

}  // namespace test

namespace {

ui::PlatformWindow* GetPlatformWindow(Browser* browser) {
  DCHECK(browser);
  auto* window_tree_host_platform =
      aura::WindowTreeHostPlatform::GetHostForWindow(
          browser->window()->GetNativeWindow());
  return window_tree_host_platform->platform_window();
}

IN_PROC_BROWSER_TEST_F(HeadlessModeBrowserTest, OzonePlatformHeadless) {
  // On Linux, new headless Chrome uses Ozone/Headless.
  ASSERT_NE(ui::OzonePlatform::GetInstance(), nullptr);
  EXPECT_EQ(ui::OzonePlatform::GetPlatformNameForTest(), "headless");
}

IN_PROC_BROWSER_TEST_F(HeadlessModeBrowserTest, PlatformWindowCantCapture) {
  // Ozone/headless uses StubWindow which is the only PlatformWindow
  // implementation that does not recognize capture setting.
  ui::PlatformWindow* platform_window = GetPlatformWindow(browser());
  platform_window->SetCapture();
  EXPECT_FALSE(platform_window->HasCapture());
}

INSTANTIATE_TEST_SUITE_P(HeadlessModeBrowserTestWithStartWindowMode,
                         HeadlessModeBrowserTestWithStartWindowMode,
                         testing::Values(kStartWindowNormal,
                                         kStartWindowMaximized,
                                         kStartWindowFullscreen));

IN_PROC_BROWSER_TEST_P(HeadlessModeBrowserTestWithStartWindowMode,
                       BrowserDesktopWindowVisibility) {
  // On Linux, new headless Chrome browser window exists and is visible.
  EXPECT_TRUE(browser()->window()->IsVisible());
}

}  // namespace

}  // namespace headless
