// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_context.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "content/public/test/browser_test.h"
#include "ui/base/ui_base_features.h"
#include "ui/display/types/display_constants.h"

namespace extensions {

IN_PROC_BROWSER_TEST_F(ExtensionApiTest,
                       ExtensionFullscreenAccessFail) {
  // Test that fullscreen cannot be accessed from an extension without
  // permission.
  ASSERT_TRUE(RunExtensionTest("fullscreen/no_permission",
                               {.launch_as_platform_app = true}))
      << message_;
}

#if BUILDFLAG(IS_MAC)
// Fails on MAC: http://crbug.com/480370
#define MAYBE_ExtensionFullscreenAccessPass \
    DISABLED_ExtensionFullscreenAccessPass
#else
#define MAYBE_ExtensionFullscreenAccessPass ExtensionFullscreenAccessPass
#endif  // BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, MAYBE_ExtensionFullscreenAccessPass) {
  // Test that fullscreen can be accessed from an extension with permission.
  ASSERT_TRUE(RunExtensionTest("fullscreen/has_permission",
                               {.launch_as_platform_app = true}))
      << message_;
}

#if BUILDFLAG(IS_MAC)
// Entering fullscreen is flaky on Mac: http://crbug.com/824517
#define MAYBE_FocusWindowDoesNotExitFullscreen \
    DISABLED_FocusWindowDoesNotExitFullscreen
#else
#define MAYBE_FocusWindowDoesNotExitFullscreen FocusWindowDoesNotExitFullscreen
#endif
IN_PROC_BROWSER_TEST_F(ExtensionApiTest,
                       MAYBE_FocusWindowDoesNotExitFullscreen) {
  browser()->exclusive_access_manager()->context()->EnterFullscreen(
      GURL(), EXCLUSIVE_ACCESS_BUBBLE_TYPE_BROWSER_FULLSCREEN_EXIT_INSTRUCTION,
      display::kInvalidDisplayId);
  ASSERT_TRUE(browser()->window()->IsFullscreen());
  ASSERT_TRUE(RunExtensionTest("window_update/focus")) << message_;
  ASSERT_TRUE(browser()->window()->IsFullscreen());
}

// Fails flakily: crbug.com/335640705.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest,
                       DISABLED_UpdateWindowSizeExitsFullscreen) {
  browser()->exclusive_access_manager()->context()->EnterFullscreen(
      GURL(), EXCLUSIVE_ACCESS_BUBBLE_TYPE_BROWSER_FULLSCREEN_EXIT_INSTRUCTION,
      display::kInvalidDisplayId);
  ASSERT_TRUE(RunExtensionTest("window_update/sizing")) << message_;
  ASSERT_FALSE(browser()->window()->IsFullscreen());
}

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS_LACROS)
// Fails on MAC: http://crbug.com/480370
// Flaky on Lacros: crbug.com/345576612.
#define MAYBE_DisplayModeWindowIsInFullscreen \
  DISABLED_DisplayModeWindowIsInFullscreen
#else
#define MAYBE_DisplayModeWindowIsInFullscreen DisplayModeWindowIsInFullscreen
#endif

IN_PROC_BROWSER_TEST_F(ExtensionApiTest,
                       MAYBE_DisplayModeWindowIsInFullscreen) {
  ASSERT_TRUE(RunExtensionTest("fullscreen/mq_display_mode",
                               {.launch_as_platform_app = true}))
      << message_;
}

}  // namespace extensions
