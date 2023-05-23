// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_PLATFORM_APPS_APP_WINDOW_INTERACTIVE_UITEST_BASE_H_
#define CHROME_BROWSER_APPS_PLATFORM_APPS_APP_WINDOW_INTERACTIVE_UITEST_BASE_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "ui/events/keycodes/keyboard_codes.h"

#if BUILDFLAG(IS_MAC)
#include "ui/base/test/scoped_fake_nswindow_fullscreen.h"
#endif

namespace extensions {
class NativeAppWindow;
}

// Helper class that has to be created on the stack to check if the fullscreen
// setting of a NativeWindow has changed since the creation of the object.
class FullscreenChangeWaiter {
 public:
  explicit FullscreenChangeWaiter(extensions::NativeAppWindow* window);
  FullscreenChangeWaiter(const FullscreenChangeWaiter&) = delete;
  FullscreenChangeWaiter& operator=(const FullscreenChangeWaiter&) = delete;

  void Wait();

 private:
  raw_ptr<extensions::NativeAppWindow> window_;
  bool initial_fullscreen_state_;
};

// Interactive test class for testing app windows (fullscreen, show, hide,
// etc.). It can be subclassed to test platform specific implementations (eg.
// testing the Ash implementation of the window).
class AppWindowInteractiveTest : public extensions::PlatformAppBrowserTest {
 public:
  AppWindowInteractiveTest() = default;
  AppWindowInteractiveTest(const AppWindowInteractiveTest&) = delete;
  AppWindowInteractiveTest& operator=(const AppWindowInteractiveTest&) = delete;

  void SetUpCommandLine(base::CommandLine* command_line) override;

  bool RunAppWindowInteractiveTest(const char* testName);

  bool SimulateKeyPress(ui::KeyboardCode key);

  // This method will wait until the application is able to ack a key event.
  void WaitUntilKeyFocus();

  // This test is a method so that we can test with each frame type.
  void TestOuterBoundsHelper(const std::string& frame_type);

 private:
#if BUILDFLAG(IS_MAC)
  ui::test::ScopedFakeNSWindowFullscreen fake_fullscreen_;
#endif
};

#endif  // CHROME_BROWSER_APPS_PLATFORM_APPS_APP_WINDOW_INTERACTIVE_UITEST_BASE_H_
