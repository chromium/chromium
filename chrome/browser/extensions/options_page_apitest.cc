// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/bind.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/value_builder.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/test_extension_dir.h"

namespace extensions {

// Used to simulate a click on the first element named 'Options'.
static const char kScriptClickOptionButton[] =
    "(function() { "
    "  var button = document.querySelector('.options-link');"
    "  button.click();"
    "})();";

// Test that an extension with an options page makes an 'Options' button appear
// on chrome://extensions, and that clicking the button opens a new tab with the
// extension's options page.
// Disabled because of flakiness. See http://crbug.com/174934.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, DISABLED_OptionsPage) {
  ExtensionRegistry* registry = ExtensionRegistry::Get(browser()->profile());
  size_t installed_extensions = registry->enabled_extensions().size();
  // Install an extension with an options page.
  const Extension* extension =
      InstallExtension(test_data_dir_.AppendASCII("options.crx"), 1);
  ASSERT_TRUE(extension);
  EXPECT_EQ(installed_extensions + 1, registry->enabled_extensions().size());

  // Go to the Extension Settings page and click the Options button.
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUIExtensionsURL));
  TabStripModel* tab_strip = browser()->tab_strip_model();
  ui_test_utils::TabAddedWaiter tab_add(browser());
  // NOTE: Currently the above script needs to execute in an iframe. The
  // selector for that iframe may break if the layout of the extensions
  // page changes.
  content::RenderFrameHost* frame = content::FrameMatchingPredicate(
      tab_strip->GetActiveWebContents(),
      base::Bind(&content::FrameHasSourceUrl,
                 GURL(chrome::kChromeUIExtensionsURL)));
  EXPECT_TRUE(content::ExecuteScript(
      frame,
      kScriptClickOptionButton));
  tab_add.Wait();
  EXPECT_EQ(2, tab_strip->count());

  EXPECT_EQ(extension->GetResourceURL("options.html"),
            tab_strip->GetWebContentsAt(1)->GetURL());
}

// Tests that navigating directly to chrome://extensions?options=<id> to an
// extension with an embedded options page loads that extension's options page.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest,
                       LoadChromeExtensionsWithOptionsParamWhenEmbedded) {
  TestExtensionDir extension_dir;
  extension_dir.WriteFile(FILE_PATH_LITERAL("options.html"),
                          "<script src=\"options.js\"></script>\n");
  extension_dir.WriteFile(
      FILE_PATH_LITERAL("options.js"),
      "chrome.tabs.getCurrent(function(tab) {\n"
      "  chrome.test.sendMessage(tab ? 'tab' : 'embedded');\n"
      "});\n");
  extension_dir.WriteManifest(
      DictionaryBuilder()
          .Set("manifest_version", 2)
          .Set("name", "Extension for options param test")
          .Set("options_ui",
               DictionaryBuilder().Set("page", "options.html").Build())
          .Set("version", "1")
          .ToJSON());

  ExtensionTestMessageListener listener(false /* will_reply */);
  scoped_refptr<const Extension> extension =
      InstallExtension(extension_dir.Pack(), 1);
  ASSERT_TRUE(extension.get());
  ui_test_utils::NavigateToURL(
      browser(), GURL("chrome://extensions?options=" + extension->id()));
  ASSERT_TRUE(listener.WaitUntilSatisfied());
  ASSERT_EQ("embedded", listener.message());
}

}  // namespace extensions
