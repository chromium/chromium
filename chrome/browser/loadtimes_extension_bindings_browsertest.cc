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
    ASSERT_TRUE(content::ExecJs(contents,
                                "window.before.firstPaintAfterLoadTime = 0;"
                                "window.before.firstPaintTime = 0;"
                                "window.after.firstPaintAfterLoadTime = 0;"
                                "window.after.firstPaintTime = 0;"));

    std::string before =
        content::EvalJs(contents, "JSON.stringify(before)").ExtractString();
    std::string after =
        content::EvalJs(contents, "JSON.stringify(after)").ExtractString();
    EXPECT_EQ(before, after);
  }
};

IN_PROC_BROWSER_TEST_F(LoadtimesExtensionBindingsTest, LoadTimesSetup) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL plain_url = embedded_test_server()->GetURL("/simple.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), plain_url));
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(content::ExecJs(
      contents, "typeof(window.chrome.loadTimes().requestTime) === 'number'"));
  ASSERT_TRUE(content::ExecJs(
      contents,
      "typeof(window.chrome.loadTimes().connectionInfo) === 'string'"));
  ASSERT_TRUE(content::ExecJs(contents,
                              "typeof(window.chrome.csi().tran) === 'number'"));
}

// TODO: crbug.com/329102379 - The test is flaky on all platforms.
IN_PROC_BROWSER_TEST_F(LoadtimesExtensionBindingsTest,
                       DISABLED_LoadTimesSameAfterClientInDocNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL plain_url = embedded_test_server()->GetURL("/simple.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), plain_url));
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(
      content::ExecJs(contents, "window.before = window.chrome.loadTimes()"));
  ASSERT_TRUE(content::ExecJs(
      contents, "window.location.href = window.location + \"#\""));
  ASSERT_TRUE(
      content::ExecJs(contents, "window.after = window.chrome.loadTimes()"));
  CompareBeforeAndAfter();
}

// TODO: crbug.com/329102379 - The test is flaky on all platforms.
IN_PROC_BROWSER_TEST_F(LoadtimesExtensionBindingsTest,
                       DISABLED_LoadTimesSameAfterUserInDocNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL plain_url = embedded_test_server()->GetURL("/simple.html");
  GURL hash_url(plain_url.spec() + "#");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), plain_url));
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(
      content::ExecJs(contents, "window.before = window.chrome.loadTimes()"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), hash_url));
  ASSERT_TRUE(
      content::ExecJs(contents, "window.after = window.chrome.loadTimes()"));
  CompareBeforeAndAfter();
}
