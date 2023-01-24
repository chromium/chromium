// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/headless/headless_mode_browsertest.h"

#include "chrome/browser/ui/browser_window.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/platform_window/platform_window.h"
#include "ui/platform_window/platform_window_delegate.h"
#include "ui/platform_window/platform_window_init_properties.h"

namespace headless {

namespace {

// Mock platform window delegate for platform window creation.
class MockPlatformWindowDelegate : public ui::PlatformWindowDelegate {
 public:
  MockPlatformWindowDelegate() = default;

  MockPlatformWindowDelegate(const MockPlatformWindowDelegate&) = delete;
  MockPlatformWindowDelegate& operator=(const MockPlatformWindowDelegate&) =
      delete;

  ~MockPlatformWindowDelegate() override = default;

  // ui::PlatformWindowDelegate:
  void OnBoundsChanged(const BoundsChange& bounds) override {}
  void OnAcceleratedWidgetAvailable(gfx::AcceleratedWidget widget) override {}
  void OnDamageRect(const gfx::Rect& damaged_region) override {}
  void DispatchEvent(ui::Event* event) override {}
  void OnCloseRequest() override {}
  void OnClosed() override {}
  void OnWindowStateChanged(ui::PlatformWindowState old_state,
                            ui::PlatformWindowState new_state) override {}
  void OnLostCapture() override {}
  void OnWillDestroyAcceleratedWidget() override {}
  void OnAcceleratedWidgetDestroyed() override {}
  void OnActivationChanged(bool active) override {}
  void OnMouseEnter() override {}
};

IN_PROC_BROWSER_TEST_F(HeadlessModeBrowserTest, OzonePlatformHeadless) {
  // On Linux, the Native Headless Chrome uses Ozone/Headless.
  ASSERT_NE(ui::OzonePlatform::GetInstance(), nullptr);
  EXPECT_EQ(ui::OzonePlatform::GetPlatformNameForTest(), "headless");
}

IN_PROC_BROWSER_TEST_F(HeadlessModeBrowserTest, PlatformWindowCantCapture) {
  ASSERT_TRUE(browser()->window()->GetNativeWindow()->IsVisible());
  // Ozone/Headless uses StubWindow which is the only PlatformWindow
  // implementation that does not respect capture setting.
  MockPlatformWindowDelegate platform_window_delegate;
  std::unique_ptr<ui::PlatformWindow> platform_window =
      ui::OzonePlatform::GetInstance()->CreatePlatformWindow(
          &platform_window_delegate,
          ui::PlatformWindowInitProperties(gfx::Rect(0, 0)));

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
  // On Linux, the Native Headless Chrome browser window exists and is
  // visible.
  EXPECT_TRUE(browser()->window()->IsVisible());
}

}  // namespace

}  // namespace headless
