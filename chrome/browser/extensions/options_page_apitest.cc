// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/functional/bind.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/test_extension_dir.h"

namespace extensions {

// Test that an extension with an options page makes an 'Options' button appear
// on chrome://extensions, and that clicking the button opens a new tab with the
// extension's options page.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, OptionsPage) {
  TestExtensionDir extension_dir;
  extension_dir.WriteFile(FILE_PATH_LITERAL("options.html"),
                          "<html><body><div>Options Here</div></body></html>");

  extension_dir.WriteManifest(base::Value::Dict()
                                  .Set("manifest_version", 2)
                                  .Set("name", "Options Test")
                                  .Set("options_page", "options.html")
                                  .Set("version", "1"));

  scoped_refptr<const Extension> extension =
      InstallExtension(extension_dir.Pack(), 1);
  ASSERT_TRUE(extension.get());

  // Go to the Extension Settings page and click the button.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("chrome://extensions?id=" + extension->id())));
  TabStripModel* tab_strip = browser()->tab_strip_model();
  ui_test_utils::TabAddedWaiter tab_add(browser());

  // Used to simulate a click on the 'Extension options' link.
  // NOTE: This relies on the layout of the chrome://extensions page, and may
  // need to be updated if that layout changes.
  static constexpr char kScriptClickOptionButton[] = R"(
    (function() {
      var button = document.querySelector('extensions-manager').
                    shadowRoot.querySelector('extensions-detail-view').
                    shadowRoot.querySelector('#extensionsOptions');
      button.click();
    })();)";

  EXPECT_TRUE(content::ExecJs(tab_strip->GetActiveWebContents(),
                              kScriptClickOptionButton));
  tab_add.Wait();
  ASSERT_EQ(2, tab_strip->count());
  content::WebContents* tab = tab_strip->GetWebContentsAt(1);
  EXPECT_TRUE(content::WaitForLoadStop(tab));
  EXPECT_EQ(extension->GetResourceURL("options.html"),
            tab->GetLastCommittedURL());
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
      base::Value::Dict()
          .Set("manifest_version", 2)
          .Set("name", "Extension for options param test")
          .Set("options_ui", base::Value::Dict().Set("page", "options.html"))
          .Set("version", "1"));

  ExtensionTestMessageListener listener;
  scoped_refptr<const Extension> extension =
      InstallExtension(extension_dir.Pack(), 1);
  ASSERT_TRUE(extension.get());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("chrome://extensions?options=" + extension->id())));
  ASSERT_TRUE(listener.WaitUntilSatisfied());
  ASSERT_EQ("embedded", listener.message());
}

}  // namespace extensions
