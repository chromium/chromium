// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "content/public/test/browser_test.h"
#include "extensions/buildflags/buildflags.h"
#include "net/dns/mock_host_resolver.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/ui/browser_window.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#endif

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

class ExtensionApiWindowsTest : public extensions::ExtensionApiTest {
 public:
  ExtensionApiWindowsTest() = default;
  ~ExtensionApiWindowsTest() override = default;
  ExtensionApiWindowsTest(const ExtensionApiWindowsTest&) = delete;
  ExtensionApiWindowsTest& operator=(const ExtensionApiWindowsTest&) = delete;

  void SetUpOnMainThread() override {
    extensions::ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(StartEmbeddedTestServer());
  }
};

// TODO(https://crbug.com/449095632): Enable these tests.
#if BUILDFLAG(ENABLE_EXTENSIONS)

// Flaky on Windows, Mac and Linux. http://crbug.com/41375473.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
#define MAYBE_UpdateWindowResize DISABLED_UpdateWindowResize
#else
#define MAYBE_UpdateWindowResize UpdateWindowResize
#endif
IN_PROC_BROWSER_TEST_F(ExtensionApiWindowsTest, MAYBE_UpdateWindowResize) {
  ASSERT_TRUE(RunExtensionTest("window_update/resize")) << message_;
}

#if BUILDFLAG(IS_WIN)
IN_PROC_BROWSER_TEST_F(ExtensionApiWindowsTest, FocusWindowDoesNotUnmaximize) {
  HWND window = browser()
                    ->GetWindow()
                    ->GetNativeWindow()
                    ->GetHost()
                    ->GetAcceleratedWidget();
  ::SendMessage(window, WM_SYSCOMMAND, SC_MAXIMIZE, 0);
  ASSERT_TRUE(RunExtensionTest("window_update/focus")) << message_;
  ASSERT_TRUE(::IsZoomed(window));
}
#endif  // BUILDFLAG(IS_WIN)

#if defined(USE_AURA) || BUILDFLAG(IS_MAC)
// Maximizing/fullscreen popup window doesn't work on aura's managed mode.
// See bug crbug.com/40162971.
// Mac: http://crbug.com/40113467
// On desktop Android, throws Java exception during minimize().
#define MAYBE_UpdateWindowShowState DISABLED_UpdateWindowShowState
#else
#define MAYBE_UpdateWindowShowState UpdateWindowShowState
#endif  // defined(USE_AURA) || BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(ExtensionApiWindowsTest, MAYBE_UpdateWindowShowState) {
  ASSERT_TRUE(RunExtensionTest("window_update/show_state")) << message_;
}

#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

// Android PiP windows extend AsyncInitializationActivity rather than
// ChromeActivity, so they do not create a ChromeAndroidTask or
// ExtensionWindowControllerBridge. Because of this, Android PiP windows are
// invisible to chrome.windows.getAll() and chrome.windows.update().
// If this changes, we should enable this test on Android.
#if !BUILDFLAG(IS_ANDROID)
// Verify that extensions cannot resize Picture-in-Picture windows beyond the
// set bounds (https://crbug.com/514080341).
IN_PROC_BROWSER_TEST_F(ExtensionApiWindowsTest,
                       Update_PictureInPictureWindowResize) {
  ASSERT_TRUE(RunExtensionTest("tabs/update_picture_in_picture",
                               {.extension_url = "test.html"}))
      << message_;
}
#endif  // !BUILDFLAG(IS_ANDROID)
