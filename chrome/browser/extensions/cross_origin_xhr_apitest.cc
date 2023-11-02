// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"

class CrossOriginXHR : public extensions::ExtensionApiTest {
 public:
  void SetUpOnMainThread() override {
    extensions::ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*.com", "127.0.0.1");
    ASSERT_TRUE(StartEmbeddedTestServer());
  }
};

IN_PROC_BROWSER_TEST_F(CrossOriginXHR, BackgroundPage) {
  ASSERT_TRUE(RunExtensionTest("cross_origin_xhr/background_page")) << message_;
}

IN_PROC_BROWSER_TEST_F(CrossOriginXHR, ContentScript) {
  ASSERT_TRUE(RunExtensionTest("cross_origin_xhr/content_script")) << message_;
}

// Tests that an extension frame can xhr a file url if it has file access and
// "<all_urls>" host permissions.
IN_PROC_BROWSER_TEST_F(CrossOriginXHR, FileAccessAllURLs) {
  ASSERT_TRUE(RunExtensionTest("cross_origin_xhr/file_access_all_urls", {},
                               {.allow_file_access = true}))
      << message_;
}

// Tests that an extension frame can't xhr a file url if it has no file access
// even with the "<all_urls>" host permissions.
IN_PROC_BROWSER_TEST_F(CrossOriginXHR, NoFileAccessAllURLs) {
  ASSERT_TRUE(RunExtensionTest("cross_origin_xhr/no_file_access_all_urls"))
      << message_;
}

// Ensures that an extension tab having no corresponding background page can xhr
// a file URL. Regression test for crbug.com/1179732.
IN_PROC_BROWSER_TEST_F(CrossOriginXHR, FileAccessNoBackgroundPage) {
  ASSERT_TRUE(RunExtensionTest(
      "cross_origin_xhr/file_access_no_background_page",
      {.extension_url = "test.html"}, {.allow_file_access = true}))
      << message_;
}

// Tests that an extension frame can't xhr a file url if it does not have host
// permissions to the file scheme even though it has file access.
IN_PROC_BROWSER_TEST_F(CrossOriginXHR, FileAccessNoHosts) {
  ASSERT_TRUE(RunExtensionTest("cross_origin_xhr/file_access_no_hosts", {},
                               {.allow_file_access = true}))
      << message_;
}
