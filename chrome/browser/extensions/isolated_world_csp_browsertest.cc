// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "url/gurl.h"

namespace extensions {
namespace {

enum class ManifestVersion { kMV2, kMV3 };

class IsolatedWorldCspBrowserTest
    : public ExtensionApiTest,
      public testing::WithParamInterface<ManifestVersion> {
 public:
  // ExtensionApiTest override.
  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();

    // Override the path used for loading the extension.
    test_data_dir_ = test_data_dir_.AppendASCII("isolated_world_csp");

    embedded_test_server()->ServeFilesFromDirectory(test_data_dir_);
    ASSERT_TRUE(StartEmbeddedTestServer());

    // Map all hosts to localhost.
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  const char* GetExtensionName() const {
    return GetParam() == ManifestVersion::kMV2 ? "mv2" : "mv3";
  }
};

#if BUILDFLAG(IS_ANDROID)
// Android only supports manifest V3.
INSTANTIATE_TEST_SUITE_P(ManifestVersion,
                         IsolatedWorldCspBrowserTest,
                         testing::Values(ManifestVersion::kMV3));
#else
INSTANTIATE_TEST_SUITE_P(ManifestVersion,
                         IsolatedWorldCspBrowserTest,
                         testing::Values(ManifestVersion::kMV2,
                                         ManifestVersion::kMV3));
#endif

// Test that a Manifest V2 content script can use eval by bypassing the main
// world CSP, but a Manifest V3 script cannot.
IN_PROC_BROWSER_TEST_P(IsolatedWorldCspBrowserTest, Eval) {
  GURL url = embedded_test_server()->GetURL("eval.com",
                                            "/page_with_script_src_csp.html");
  ASSERT_TRUE(
      RunExtensionTest(GetExtensionName(), {.page_url = url.spec().c_str()}))
      << message_;
}

// Test that a Manifest V2 content script can navigate to a javascript url by
// bypassing the main world CSP but a Manifest V3 script cannot.
IN_PROC_BROWSER_TEST_P(IsolatedWorldCspBrowserTest, JavascriptUrl) {
  if (GetParam() == ManifestVersion::kMV2) {
    GURL url = embedded_test_server()->GetURL("js-url.com",
                                              "/page_with_script_src_csp.html");
    ASSERT_TRUE(RunExtensionTest("mv2", {.page_url = url.spec().c_str()}))
        << message_;
  } else {
    // We wait on a console message which will be raised on an unsuccessful
    // navigation to a javascript url since there isn't any other clean way to
    // assert that the navigation didn't succeed.
    content::WebContents* web_contents = GetActiveWebContents();
    content::WebContentsConsoleObserver console_observer(web_contents);
    console_observer.SetPattern(
        "Running the JavaScript URL violates the following Content Security "
        "Policy directive *");

    GURL url = embedded_test_server()->GetURL("js-url.com",
                                              "/page_with_script_src_csp.html");
    ASSERT_TRUE(RunExtensionTest("mv3", {.page_url = url.spec().c_str()}))
        << message_;
    ASSERT_TRUE(console_observer.Wait());

    // Also ensure the page title wasn't changed.
    EXPECT_EQ(u"Page With CSP", web_contents->GetTitle());
  }
}

// Test that a Manifest V2 content script can execute a remote script even if
// it is disallowed by the main world CSP, but a Manifest V3 script cannot.
IN_PROC_BROWSER_TEST_P(IsolatedWorldCspBrowserTest, RemoteScriptSrc) {
  GURL url = embedded_test_server()->GetURL("remote-script.com",
                                            "/page_with_script_src_csp.html");
  ASSERT_TRUE(
      RunExtensionTest(GetExtensionName(), {.page_url = url.spec().c_str()}))
      << message_;
}

// Test that a Manifest V2 content script can execute a remote script even if
// it is disallowed by the main world Integrity Policy but a Manifest V3
// extension can.
IN_PROC_BROWSER_TEST_P(IsolatedWorldCspBrowserTest, IntegrityPolicy) {
  GURL url = embedded_test_server()->GetURL("remote-script.com",
                                            "/page_with_integrity_policy.html");
  ASSERT_TRUE(
      RunExtensionTest(GetExtensionName(), {.page_url = url.spec().c_str()}))
      << message_;
}

}  // namespace
}  // namespace extensions
