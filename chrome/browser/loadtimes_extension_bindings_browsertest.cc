// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

class LoadtimesExtensionBindingsTest : public InProcessBrowserTest {
 public:
  LoadtimesExtensionBindingsTest() {}

  void CompareBeforeAndAfter() {
    // TODO(simonjam): There's a race on whether or not first paint is populated
    // before we read them. We ought to test that too. Until the race is fixed,
    // zero it out so the test is stable.
    content::WebContents* contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    ASSERT_TRUE(content::ExecuteScript(
        contents,
        "window.before.firstPaintAfterLoadTime = 0;"
        "window.before.firstPaintTime = 0;"
        "window.after.firstPaintAfterLoadTime = 0;"
        "window.after.firstPaintTime = 0;"));

    std::string before;
    std::string after;
    ASSERT_TRUE(content::ExecuteScriptAndExtractString(
        contents,
        "window.domAutomationController.send("
        "    JSON.stringify(before))",
        &before));
    ASSERT_TRUE(content::ExecuteScriptAndExtractString(
        contents,
        "window.domAutomationController.send("
        "    JSON.stringify(after))",
        &after));
    EXPECT_EQ(before, after);
  }
};

IN_PROC_BROWSER_TEST_F(LoadtimesExtensionBindingsTest,
                       LoadTimesSameAfterClientInDocNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL plain_url = embedded_test_server()->GetURL("/simple.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), plain_url));
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(content::ExecuteScript(
      contents, "window.before = window.chrome.loadTimes()"));
  ASSERT_TRUE(content::ExecuteScript(
      contents, "window.location.href = window.location + \"#\""));
  ASSERT_TRUE(content::ExecuteScript(
      contents, "window.after = window.chrome.loadTimes()"));
  CompareBeforeAndAfter();
}

IN_PROC_BROWSER_TEST_F(LoadtimesExtensionBindingsTest,
                       LoadTimesSameAfterUserInDocNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL plain_url = embedded_test_server()->GetURL("/simple.html");
  GURL hash_url(plain_url.spec() + "#");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), plain_url));
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(content::ExecuteScript(
      contents, "window.before = window.chrome.loadTimes()"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), hash_url));
  ASSERT_TRUE(content::ExecuteScript(
      contents, "window.after = window.chrome.loadTimes()"));
  CompareBeforeAndAfter();
}
