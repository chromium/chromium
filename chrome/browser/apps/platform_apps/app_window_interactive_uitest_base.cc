// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/platform_apps/app_window_interactive_uitest_base.h"

#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "components/keep_alive_registry/keep_alive_registry.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "ui/display/display_switches.h"

FullscreenChangeWaiter::FullscreenChangeWaiter(
    extensions::NativeAppWindow* window)
    : window_(window), initial_fullscreen_state_(window_->IsFullscreen()) {}

void FullscreenChangeWaiter::Wait() {
  while (initial_fullscreen_state_ == window_->IsFullscreen())
    content::RunAllPendingInMessageLoop();
}

// Some of these tests fail if run on a machine with a non 1.0 device scale
// factor, so make the test override the device scale factor.
void AppWindowInteractiveTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  extensions::PlatformAppBrowserTest::SetUpCommandLine(command_line);
  command_line->AppendSwitchASCII(switches::kForceDeviceScaleFactor, "1");
}

bool AppWindowInteractiveTest::RunAppWindowInteractiveTest(
    const char* testName) {
  ExtensionTestMessageListener launched_listener("Launched",
                                                 ReplyBehavior::kWillReply);
  LoadAndLaunchPlatformApp("window_api_interactive", &launched_listener);

  extensions::ResultCatcher catcher;
  launched_listener.Reply(testName);

  if (!catcher.GetNextResult()) {
    message_ = catcher.message();
    return false;
  }

  return true;
}

bool AppWindowInteractiveTest::SimulateKeyPress(ui::KeyboardCode key) {
  return ui_test_utils::SendKeyPressToWindowSync(
      GetFirstAppWindow()->GetNativeWindow(), key, false, false, false, false);
}

void AppWindowInteractiveTest::WaitUntilKeyFocus() {
  ExtensionTestMessageListener key_listener("KeyReceived");

  while (!key_listener.was_satisfied()) {
    ASSERT_TRUE(SimulateKeyPress(ui::VKEY_Z));
    content::RunAllPendingInMessageLoop();
  }
}
