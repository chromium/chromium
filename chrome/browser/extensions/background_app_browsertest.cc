// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/containers/contains.h"
#include "base/gtest_prod_util.h"
#include "chrome/browser/background/background_mode_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/test_extension_registry_observer.h"

namespace extensions {

using BackgroundAppBrowserTest = ExtensionBrowserTest;

// Tests that if we reload a background app, we don't get a popup bubble
// telling us that a new background app has been installed.
IN_PROC_BROWSER_TEST_F(BackgroundAppBrowserTest, ReloadBackgroundApp) {
  BackgroundModeManager* manager = g_browser_process->background_mode_manager();
  // Load our background extension
  EXPECT_EQ(0, manager->client_installed_notifications_for_test());
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("background_app"));
  EXPECT_EQ(1, manager->client_installed_notifications_for_test());
  ASSERT_FALSE(extension == NULL);

  // Reload our background extension
  ReloadExtension(extension->id());

  // Ensure that we did not see another "Background extension loaded" dialog.
  EXPECT_EQ(1, manager->client_installed_notifications_for_test());
}

// Make sure that the background mode notification is sent for an app install,
// but not again on browser restart. Regression test for
// https://crbug.com/1008890
IN_PROC_BROWSER_TEST_F(BackgroundAppBrowserTest, PRE_InstallBackgroundApp) {
  InstallExtension(test_data_dir_.AppendASCII("background_app"), 1);
  EXPECT_EQ(1, g_browser_process->background_mode_manager()
                   ->client_installed_notifications_for_test());
}

IN_PROC_BROWSER_TEST_F(BackgroundAppBrowserTest, InstallBackgroundApp) {
  // Verify the installed extension is still here.
  const ExtensionSet& extensions = extension_registry()->enabled_extensions();
  EXPECT_TRUE(base::Contains(extensions,
                             "A simple app with background permission set.",
                             &Extension::description));
  // Verify the installed extension did not pop up a background mode
  // notification.
  EXPECT_EQ(0, g_browser_process->background_mode_manager()
                   ->client_installed_notifications_for_test());
}

}  // namespace extensions
