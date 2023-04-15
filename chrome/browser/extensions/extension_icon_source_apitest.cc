// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/common/switches.h"
#include "net/dns/mock_host_resolver.h"
#include "url/gurl.h"

using ExtensionIconSourceTest = extensions::ExtensionApiTest;

IN_PROC_BROWSER_TEST_F(ExtensionIconSourceTest, IconsLoaded) {
  base::FilePath basedir = test_data_dir_.AppendASCII("icons");
  ASSERT_TRUE(LoadExtension(basedir.AppendASCII("extension_with_permission")));
  ASSERT_TRUE(LoadExtension(basedir.AppendASCII("extension_no_permission")));

  // Test that the icons are loaded and that the chrome://extension-icon
  // parameters work correctly.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      GURL("chrome-extension://gbmgkahjioeacddebbnengilkgbkhodg/index.html")));
  EXPECT_EQ(
      content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                      "document.title"),
      "Loaded");

  // Verify that the an extension can't load chrome://extension-icon icons
  // without the management permission.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      GURL("chrome-extension://apocjbpjpkghdepdngjlknfpmabcmlao/index.html")));
  EXPECT_EQ(
      content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                      "document.title"),
      "Not Loaded");
}

IN_PROC_BROWSER_TEST_F(ExtensionIconSourceTest, InvalidURL) {
  // Test that navigation to an invalid url works.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("chrome://extension-icon/invalid")));

  EXPECT_EQ(
      content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                      "document.title"),
      "invalid (96\xC3\x97"
      "96)");
}

IN_PROC_BROWSER_TEST_F(ExtensionIconSourceTest, IconsLoadedIncognito) {
  base::FilePath basedir = test_data_dir_.AppendASCII("icons");
  ASSERT_TRUE(LoadExtension(basedir.AppendASCII("extension_with_permission"),
                            {.allow_in_incognito = true}));
  ASSERT_TRUE(LoadExtension(basedir.AppendASCII("extension_no_permission"),
                            {.allow_in_incognito = true}));

  // Test that the icons are loaded and that the chrome://extension-icon
  // parameters work correctly.
  Browser* otr_browser = OpenURLOffTheRecord(
      browser()->profile(),
      GURL("chrome-extension://gbmgkahjioeacddebbnengilkgbkhodg/index.html"));
  EXPECT_EQ(
      content::EvalJs(otr_browser->tab_strip_model()->GetActiveWebContents(),
                      "document.title"),
      "Loaded");

  // Verify that the an extension can't load chrome://extension-icon icons
  // without the management permission.
  OpenURLOffTheRecord(
      browser()->profile(),
      GURL("chrome-extension://apocjbpjpkghdepdngjlknfpmabcmlao/index.html"));
  EXPECT_EQ(
      content::EvalJs(otr_browser->tab_strip_model()->GetActiveWebContents(),
                      "document.title"),
      "Not Loaded");
}
