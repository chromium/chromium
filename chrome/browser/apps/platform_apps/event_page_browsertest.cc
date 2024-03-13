// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_host_test_helper.h"
#include "extensions/test/extension_test_message_listener.h"

using extensions::Extension;
using extensions::PlatformAppBrowserTest;

namespace {

class AppEventPageTest : public PlatformAppBrowserTest {
 protected:
  void TestUnloadEventPage(const char* app_path) {
    // Load and launch the app.
    const Extension* extension = LoadAndLaunchPlatformApp(app_path, "launched");
    ASSERT_TRUE(extension);

    extensions::ExtensionHostTestHelper host_helper(profile(), extension->id());
    // Close the app window.
    EXPECT_EQ(1U, GetAppWindowCount());
    extensions::AppWindow* app_window = GetFirstAppWindow();
    ASSERT_TRUE(app_window);
    CloseAppWindow(app_window);

    // Verify that the event page is destroyed.
    host_helper.WaitForHostDestroyed();
  }
};

}  // namespace

// Tests that an app's event page will eventually be unloaded. The onSuspend
// event handler of this app does not make any API calls.
IN_PROC_BROWSER_TEST_F(AppEventPageTest, OnSuspendNoApiUse) {
  TestUnloadEventPage("event_page/suspend_simple");
}

// Tests that an app's event page will eventually be unloaded. The onSuspend
// event handler of this app calls a chrome.storage API function.
// See: http://crbug.com/296834
IN_PROC_BROWSER_TEST_F(AppEventPageTest, OnSuspendUseStorageApi) {
  TestUnloadEventPage("event_page/suspend_storage_api");
}
