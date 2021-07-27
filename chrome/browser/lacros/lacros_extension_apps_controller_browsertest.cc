// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/lacros_extension_apps_controller.h"

#include "base/containers/contains.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/lacros/lacros_extension_apps_utility.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

class LacrosExtensionAppsControllerTest
    : public extensions::ExtensionBrowserTest {
 public:
  content::WebContents* GetActiveWebContents() {
    Browser* browser =
        chrome::FindTabbedBrowser(profile(), /*match_original_profiles=*/false);
    return browser->tab_strip_model()->GetActiveWebContents();
  }
};

// Test opening native settings for the app.
IN_PROC_BROWSER_TEST_F(LacrosExtensionAppsControllerTest, OpenNativeSettings) {
  const extensions::Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("platform_apps/minimal"));

  // It doesn't matter what the URL is, it shouldn't be related to the
  // extension.
  ASSERT_FALSE(
      base::Contains(GetActiveWebContents()->GetURL().spec(), extension->id()));

  // Send the message to open native settings.
  std::string muxed_id =
      lacros_extension_apps_utility::MuxId(profile(), extension);
  LacrosExtensionAppsController controller;
  controller.OpenNativeSettings(muxed_id);

  // Now the URL should be on a settings page that has the extension id.
  ASSERT_TRUE(
      base::Contains(GetActiveWebContents()->GetURL().spec(), extension->id()));
}

}  // namespace
