// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "content/public/test/browser_test.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"

class ExtensionPointerLockTest : public extensions::PlatformAppBrowserTest {
 public:
  bool RunExtensionPointerLockTest(const char* app_path) {
    ExtensionTestMessageListener launched_listener("Launched", true);
    LoadAndLaunchPlatformApp(app_path, &launched_listener);

    extensions::ResultCatcher catcher;

    if (!ui_test_utils::ShowAndFocusNativeWindow(
            GetFirstAppWindow()->GetNativeWindow())) {
      message_ = "Can't focus window";
      return false;
    }

    launched_listener.Reply("");

    if (!catcher.GetNextResult()) {
      message_ = catcher.message();
      return false;
    }

    return true;
  }
};

IN_PROC_BROWSER_TEST_F(ExtensionPointerLockTest,
                       ExtensionPointerLockAccessFail) {
  // Test that pointer lock cannot be accessed from an extension without
  // permission.
  ASSERT_TRUE(RunExtensionPointerLockTest("pointer_lock/no_permission"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionPointerLockTest,
                       ExtensionPointerLockAccessPass) {
  // Test that pointer lock can be accessed from an extension with permission.
  ASSERT_TRUE(RunExtensionPointerLockTest("pointer_lock/has_permission"))
      << message_;
}
