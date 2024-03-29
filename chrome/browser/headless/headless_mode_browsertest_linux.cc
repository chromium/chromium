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
#include "ui/gfx/native_widget_types.h"
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

IN_PROC_BROWSER_TEST_F(HeadlessModeBrowserTestWithWindowSize, LargeWindowSize) {
  ui::PlatformWindow* platform_window = GetPlatformWindow(browser());

  // Expect the platform window size to be the same as the requested window
  // size because Ozone/headless is not clamping the platform window dimensions
  // to the screen size which in this case is default 800x600 pixels.
  gfx::Rect pixel_bounds = platform_window->GetBoundsInPixels();
  EXPECT_EQ(pixel_bounds.width(), kWindowSize.width());
  EXPECT_EQ(pixel_bounds.height(), kWindowSize.height());

  // Expect the reported browser window size to be the same as the requested
  // window size.
  gfx::Rect bounds = browser()->window()->GetBounds();
  EXPECT_EQ(bounds.size(), kWindowSize);
}

IN_PROC_BROWSER_TEST_F(HeadlessModeBrowserTestWithWindowSizeAndScale,
                       WindowSizeWithScale) {
  ui::PlatformWindow* platform_window = GetPlatformWindow(browser());

  // Expect the platform window size in pixels to be larger than the requested
  // window size due to scaling.
  gfx::Rect pixel_bounds = platform_window->GetBoundsInPixels();
  EXPECT_GT(pixel_bounds.width(), kWindowSize.width());
  EXPECT_GT(pixel_bounds.height(), kWindowSize.height());

  // Expect the platform window size in DIPs to be the same as the requested
  // window size despite the scaling.
  gfx::Rect dip_bounds = platform_window->GetBoundsInDIP();
  EXPECT_EQ(dip_bounds.width(), kWindowSize.width());
  EXPECT_EQ(dip_bounds.height(), kWindowSize.height());

  // Expect the reported browser window size to be the same as the requested
  // window size despite the scaling.
  gfx::Rect bounds = browser()->window()->GetBounds();
  EXPECT_EQ(bounds.size(), kWindowSize);
}

}  // namespace

}  // namespace headless
