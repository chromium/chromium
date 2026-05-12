// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_browsertest.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/extension_registrar.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/themes/theme_service.h"
#endif

namespace extensions {
namespace {

using RendererInitializationTest = ExtensionBrowserTest;

// Test that opening a window with an extension recorded as active, then
// unloading the extension, all before the renderer is fully initialized,
// doesn't crash. This addresses crbug.com/40434302, where messages could be
// sent out of order if an extension unloaded before the activation message was
// sent.
IN_PROC_BROWSER_TEST_F(RendererInitializationTest,
                       TestRendererStartupWithConflictingMessages) {
  // Load up an extension an begin opening an URL to a page within it. Since
  // this will be an extension tab, the extension will be active within that
  // process.
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("simple_with_file"));
  ASSERT_TRUE(extension);

  // Open a new tab.
  NavigateToURLInNewTab(GURL("about:blank"));
  content::WebContents* web_contents = GetActiveWebContents();

  // Start loading a file inside the extension.
  GURL url = extension->GetResourceURL("file.html");
  web_contents->GetController().LoadURL(
      url, content::Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());

  // Without waiting for the tab to finish, unload the extension.
  extension_registrar()->RemoveExtension(extension->id(),
                                         UnloadedExtensionReason::TERMINATE);

  // Wait for the web contents to stop loading.
  content::WaitForLoadStop(web_contents);
  EXPECT_EQ(url, web_contents->GetLastCommittedURL());
  ASSERT_FALSE(web_contents->IsCrashed());
}

// Android does not support themes.
#if !BUILDFLAG(IS_ANDROID)
// Tests that loading a file from a theme in a tab doesn't crash anything.
// Another part of crbug.com/40434302 and related.
IN_PROC_BROWSER_TEST_F(RendererInitializationTest,
                       TestRendererInitializationWithThemesTab) {
  // Don't create "Cached Theme.pak" in the extension directory, so as not to
  // modify the source tree.
  ThemeService::DisableThemePackForTesting();

  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("theme"));
  ASSERT_TRUE(extension);
  ASSERT_TRUE(extension->is_theme());
  GURL url = extension->GetResourceURL("manifest.json");
  content::WebContents* web_contents = GetActiveWebContents();
  ASSERT_TRUE(NavigateToURL(web_contents, url));
  // Wait for the web contents to stop loading.
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));
  EXPECT_EQ(url, web_contents->GetLastCommittedURL());
  ASSERT_FALSE(web_contents->IsCrashed());
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace
}  // namespace extensions
