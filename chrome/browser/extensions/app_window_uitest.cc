// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/app_window/native_app_window.h"

namespace extensions {

namespace {

class AppWindowTest : public PlatformAppBrowserTest {
 protected:
  void CheckAlwaysOnTopToFullscreen(AppWindow* window) {
    ASSERT_EQ(ui::ZOrderLevel::kFloatingWindow,
              window->GetBaseWindow()->GetZOrderLevel());

    // The always-on-top property should be temporarily disabled when the window
    // enters fullscreen.
    window->Fullscreen();
    EXPECT_EQ(ui::ZOrderLevel::kNormal,
              window->GetBaseWindow()->GetZOrderLevel());

    // From the API's point of view, the always-on-top property is enabled.
    EXPECT_TRUE(window->IsAlwaysOnTop());

    // The always-on-top property is restored when the window exits fullscreen.
    window->Restore();
    EXPECT_EQ(ui::ZOrderLevel::kFloatingWindow,
              window->GetBaseWindow()->GetZOrderLevel());
  }

  void CheckNormalToFullscreen(AppWindow* window) {
    // If the always-on-top property is false, it should remain this way when
    // entering and exiting fullscreen mode.
    ASSERT_EQ(ui::ZOrderLevel::kNormal,
              window->GetBaseWindow()->GetZOrderLevel());
    window->Fullscreen();
    EXPECT_EQ(ui::ZOrderLevel::kNormal,
              window->GetBaseWindow()->GetZOrderLevel());
    window->Restore();
    EXPECT_EQ(ui::ZOrderLevel::kNormal,
              window->GetBaseWindow()->GetZOrderLevel());
  }

  void CheckFullscreenToAlwaysOnTop(AppWindow* window) {
    ASSERT_TRUE(window->GetBaseWindow()->IsFullscreenOrPending());

    // Now enable always-on-top at runtime and ensure the property does not get
    // applied immediately because the window is in fullscreen mode.
    window->SetAlwaysOnTop(true);
    EXPECT_EQ(ui::ZOrderLevel::kNormal,
              window->GetBaseWindow()->GetZOrderLevel());

    // From the API's point of view, the always-on-top property is enabled.
    EXPECT_TRUE(window->IsAlwaysOnTop());

    // Ensure the always-on-top property is applied when exiting fullscreen.
    window->Restore();
    EXPECT_EQ(ui::ZOrderLevel::kFloatingWindow,
              window->GetBaseWindow()->GetZOrderLevel());
  }
};

}  // namespace

// Tests are flaky on Mac as transitioning to fullscreen is not instantaneous
// and throws errors when entering/exiting fullscreen too quickly.
#if BUILDFLAG(IS_MAC)
#define MAYBE_InitAlwaysOnTopToFullscreen DISABLED_InitAlwaysOnTopToFullscreen
#else
#define MAYBE_InitAlwaysOnTopToFullscreen InitAlwaysOnTopToFullscreen
#endif

// Tests a window created with always-on-top enabled and ensures that the
// property is temporarily switched off when entering fullscreen mode.
IN_PROC_BROWSER_TEST_F(AppWindowTest, MAYBE_InitAlwaysOnTopToFullscreen) {
  AppWindow* window = CreateTestAppWindow("{ \"alwaysOnTop\": true }");
  ASSERT_TRUE(window);
  CheckAlwaysOnTopToFullscreen(window);

  window->SetAlwaysOnTop(false);
  CheckNormalToFullscreen(window);

  CloseAppWindow(window);
}

#if BUILDFLAG(IS_MAC)
#define MAYBE_RuntimeAlwaysOnTopToFullscreen \
  DISABLED_RuntimeAlwaysOnTopToFullscreen
#else
#define MAYBE_RuntimeAlwaysOnTopToFullscreen RuntimeAlwaysOnTopToFullscreen
#endif

// Tests a window with always-on-top enabled at runtime and ensures that the
// property is temporarily switched off when entering fullscreen mode.
IN_PROC_BROWSER_TEST_F(AppWindowTest, MAYBE_RuntimeAlwaysOnTopToFullscreen) {
  AppWindow* window = CreateTestAppWindow("{}");
  ASSERT_TRUE(window);
  CheckNormalToFullscreen(window);

  window->SetAlwaysOnTop(true);
  CheckAlwaysOnTopToFullscreen(window);

  CloseAppWindow(window);
}

#if BUILDFLAG(IS_MAC)
#define MAYBE_InitFullscreenToAlwaysOnTop DISABLED_InitFullscreenToAlwaysOnTop
#else
#define MAYBE_InitFullscreenToAlwaysOnTop InitFullscreenToAlwaysOnTop
#endif

// Tests a window created initially in fullscreen mode and ensures that the
// always-on-top property does not get applied until it exits fullscreen.
IN_PROC_BROWSER_TEST_F(AppWindowTest, MAYBE_InitFullscreenToAlwaysOnTop) {
  AppWindow* window = CreateTestAppWindow("{ \"state\": \"fullscreen\" }");
  ASSERT_TRUE(window);
  CheckFullscreenToAlwaysOnTop(window);

  CloseAppWindow(window);
}

#if BUILDFLAG(IS_MAC)
#define MAYBE_RuntimeFullscreenToAlwaysOnTop \
  DISABLED_RuntimeFullscreenToAlwaysOnTop
#else
#define MAYBE_RuntimeFullscreenToAlwaysOnTop RuntimeFullscreenToAlwaysOnTop
#endif

// Tests a window that enters fullscreen mode at runtime and ensures that the
// always-on-top property does not get applied until it exits fullscreen.
IN_PROC_BROWSER_TEST_F(AppWindowTest, MAYBE_RuntimeFullscreenToAlwaysOnTop) {
  AppWindow* window = CreateTestAppWindow("{}");
  ASSERT_TRUE(window);

  window->Fullscreen();
  CheckFullscreenToAlwaysOnTop(window);

  CloseAppWindow(window);
}

#if BUILDFLAG(IS_MAC)
#define MAYBE_InitFullscreenAndAlwaysOnTop DISABLED_InitFullscreenAndAlwaysOnTop
#else
#define MAYBE_InitFullscreenAndAlwaysOnTop InitFullscreenAndAlwaysOnTop
#endif

// Tests a window created with both fullscreen and always-on-top enabled. Ensure
// that always-on-top is only applied when the window exits fullscreen.
IN_PROC_BROWSER_TEST_F(AppWindowTest, MAYBE_InitFullscreenAndAlwaysOnTop) {
  AppWindow* window = CreateTestAppWindow(
      "{ \"alwaysOnTop\": true, \"state\": \"fullscreen\" }");
  ASSERT_TRUE(window);

  EXPECT_TRUE(window->GetBaseWindow()->IsFullscreenOrPending());
  EXPECT_EQ(ui::ZOrderLevel::kNormal,
            window->GetBaseWindow()->GetZOrderLevel());

  // From the API's point of view, the always-on-top property is enabled.
  EXPECT_TRUE(window->IsAlwaysOnTop());

  window->Restore();
  EXPECT_EQ(ui::ZOrderLevel::kFloatingWindow,
            window->GetBaseWindow()->GetZOrderLevel());

  CloseAppWindow(window);
}

#if BUILDFLAG(IS_MAC)
#define MAYBE_DisableAlwaysOnTopInFullscreen \
  DISABLED_DisableAlwaysOnTopInFullscreen
#else
#define MAYBE_DisableAlwaysOnTopInFullscreen DisableAlwaysOnTopInFullscreen
#endif

// Tests a window created with always-on-top enabled, but then disabled while
// in fullscreen mode. After exiting fullscreen, always-on-top should remain
// disabled.
IN_PROC_BROWSER_TEST_F(AppWindowTest, MAYBE_DisableAlwaysOnTopInFullscreen) {
  AppWindow* window = CreateTestAppWindow("{ \"alwaysOnTop\": true }");
  ASSERT_TRUE(window);

  // Disable always-on-top while in fullscreen mode.
  window->Fullscreen();
  EXPECT_EQ(ui::ZOrderLevel::kNormal,
            window->GetBaseWindow()->GetZOrderLevel());
  window->SetAlwaysOnTop(false);
  EXPECT_EQ(ui::ZOrderLevel::kNormal,
            window->GetBaseWindow()->GetZOrderLevel());

  // Ensure that always-on-top remains disabled.
  window->Restore();
  EXPECT_EQ(ui::ZOrderLevel::kNormal,
            window->GetBaseWindow()->GetZOrderLevel());

  CloseAppWindow(window);
}

// Tests a window created with showInShelf property enabled is indeed marked
// as shown in shelf in AppWindow.
IN_PROC_BROWSER_TEST_F(AppWindowTest, InitShowInShelf) {
  AppWindow* window =
      CreateTestAppWindow("{ \"showInShelf\": true , \"id\": \"window\" }");
  ASSERT_TRUE(window);

  // Ensure that the window created is marked as shown in shelf.
  EXPECT_TRUE(window->show_in_shelf());

  CloseAppWindow(window);
}

}  // namespace extensions
