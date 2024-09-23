// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

namespace extensions {

// Test that opening a window with an extension recorded as active, then
// unloading the extension, all before the renderer is fully initialized,
// doesn't crash. This addresses crbug.com/528026, where messages could be sent
// out of order if an extension unloaded before the activation message was sent.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest,
                       TestRendererStartupWithConflictingMessages) {
  // Load up an extension an begin opening an URL to a page within it. Since
  // this will be an extension tab, the extension will be active within that
  // process.
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("simple_with_file"));
  ASSERT_TRUE(extension);
  GURL url = extension->GetResourceURL("file.html");
  browser()->OpenURL(
      content::OpenURLParams(url, content::Referrer(),
                             WindowOpenDisposition::NEW_FOREGROUND_TAB,
                             ui::PAGE_TRANSITION_TYPED, false),
      /*navigation_handle_callback=*/{});
  // Without waiting for the tab to finish, unload the extension.
  extension_service()->UnloadExtension(extension->id(),
                                       UnloadedExtensionReason::TERMINATE);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  // Wait for the web contents to stop loading.
  content::WaitForLoadStop(web_contents);
  EXPECT_EQ(url, web_contents->GetLastCommittedURL());
  ASSERT_FALSE(web_contents->IsCrashed());
}

// Tests that loading a file from a theme in a tab doesn't crash anything.
// Another part of crbug.com/528026 and related.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest,
                       TestRendererInitializationWithThemesTab) {
  // Don't create "Cached Theme.pak" in the extension directory, so as not to
  // modify the source tree.
  ThemeService::DisableThemePackForTesting();

  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("theme"));
  ASSERT_TRUE(extension);
  ASSERT_TRUE(extension->is_theme());
  GURL url = extension->GetResourceURL("manifest.json");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  // Wait for the web contents to stop loading.
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));
  EXPECT_EQ(url, web_contents->GetLastCommittedURL());
  ASSERT_FALSE(web_contents->IsCrashed());
}

}  // namespace extensions
