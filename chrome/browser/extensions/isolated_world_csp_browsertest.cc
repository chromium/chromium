// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/ui/browser.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"

namespace extensions {

class IsolatedWorldCspBrowserTest : public ExtensionApiTest {
 public:
  // ExtensionApiTest override.
  void SetUpOnMainThread() override {
    extensions::ExtensionApiTest::SetUpOnMainThread();

    // Override the path used for loading the extension.
    test_data_dir_ = test_data_dir_.AppendASCII("isolated_world_csp");

    embedded_test_server()->ServeFilesFromDirectory(test_data_dir_);
    ASSERT_TRUE(StartEmbeddedTestServer());

    // Map all hosts to localhost.
    host_resolver()->AddRule("*", "127.0.0.1");
  }
};

// Test that a Manifest V2 content script can use eval by bypassing the main
// world CSP.
IN_PROC_BROWSER_TEST_F(IsolatedWorldCspBrowserTest, Eval_ManifestV2) {
  GURL url = embedded_test_server()->GetURL("eval.com",
                                            "/page_with_script_src_csp.html");
  ASSERT_TRUE(RunExtensionTest("mv2", {.page_url = url.spec().c_str()}))
      << message_;
}

// Test that a Manifest V3 content script can't use eval.
IN_PROC_BROWSER_TEST_F(IsolatedWorldCspBrowserTest, Eval_ManifestV3) {
  GURL url = embedded_test_server()->GetURL("eval.com",
                                            "/page_with_script_src_csp.html");
  ASSERT_TRUE(RunExtensionTest("mv3", {.page_url = url.spec().c_str()}))
      << message_;
}

// Test that a Manifest V2 content script can navigate to a javascript url by
// bypassing the main world CSP.
IN_PROC_BROWSER_TEST_F(IsolatedWorldCspBrowserTest, JavascriptUrl_ManifestV2) {
  GURL url = embedded_test_server()->GetURL("js-url.com",
                                            "/page_with_script_src_csp.html");
  ASSERT_TRUE(RunExtensionTest("mv2", {.page_url = url.spec().c_str()}))
      << message_;
}

// Test that a Manifest V3 content script can't navigate to a javascript url
// while in its isolated world.
IN_PROC_BROWSER_TEST_F(IsolatedWorldCspBrowserTest, JavascriptUrl_ManifestV3) {
  // We wait on a console message which will be raised on an unsuccessful
  // navigation to a javascript url since there isn't any other clean way to
  // assert that the navigation didn't succeed.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::WebContentsConsoleObserver console_observer(web_contents);
  console_observer.SetPattern(
      "Refused to run the JavaScript URL because it violates the following "
      "Content Security Policy directive: *");

  GURL url = embedded_test_server()->GetURL("js-url.com",
                                            "/page_with_script_src_csp.html");
  ASSERT_TRUE(RunExtensionTest("mv3", {.page_url = url.spec().c_str()}))
      << message_;
  ASSERT_TRUE(console_observer.Wait());

  // Also ensure the page title wasn't changed.
  EXPECT_EQ(u"Page With CSP", web_contents->GetTitle());
}

// Test that a Manifest V2 content script can execute a remote script even if
// it is disallowed by the main world CSP.
IN_PROC_BROWSER_TEST_F(IsolatedWorldCspBrowserTest,
                       RemoteScriptSrc_ManifestV2) {
  GURL url = embedded_test_server()->GetURL("remote-script.com",
                                            "/page_with_script_src_csp.html");
  ASSERT_TRUE(RunExtensionTest("mv2", {.page_url = url.spec().c_str()}))
      << message_;
}

// Test that a Manifest V3 content script can't execute a remote script even if
// is allowed by the main world CSP.
IN_PROC_BROWSER_TEST_F(IsolatedWorldCspBrowserTest,
                       RemoteScriptSrc_ManifestV3) {
  GURL url = embedded_test_server()->GetURL("remote-script.com",
                                            "/page_with_script_src_csp.html");
  ASSERT_TRUE(RunExtensionTest("mv3", {.page_url = url.spec().c_str()}))
      << message_;
}

}  // namespace extensions
