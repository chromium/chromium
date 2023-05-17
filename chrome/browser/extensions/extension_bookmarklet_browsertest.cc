// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_action_test_util.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/extension_action.h"
#include "extensions/browser/extension_action_manager.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace extensions {
namespace {

// Tests that the chrome-extension scheme disallows running Javascript URLs.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest,
                       ChromeExtensionSchemeNotAllowJavascript) {
  ASSERT_TRUE(embedded_test_server()->Start());

  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("simple_with_file"));
  ASSERT_TRUE(extension);

  // Navigate to the extension's page.
  const GURL extension_file_url(extension->GetResourceURL("file.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), extension_file_url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  const std::u16string expected_title = u"foo";
  ASSERT_EQ(expected_title, web_contents->GetTitle());

  // Attempt to set the page title via Javascript. Don't try to block since
  // the javascript URL won't actually navigate anywhere.
  const GURL script_url("javascript:void(document.title='Bad Title')");
  NavigateToURLWithDisposition(browser(), script_url,
                               WindowOpenDisposition::CURRENT_TAB,
                               ui_test_utils::BROWSER_TEST_NO_WAIT);
  // Force serialization with the renderer by executing a no-op script.
  EXPECT_EQ(true, content::EvalJs(web_contents, "true"));

  // Expect the title hasn't changed since the javascript URL was blocked
  // from executing.
  EXPECT_EQ(expected_title, web_contents->GetTitle());
}

}  // namespace
}  // namespace extensions
