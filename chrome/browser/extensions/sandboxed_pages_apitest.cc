// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "content/public/test/browser_test.h"

namespace extensions {

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, SandboxedPages) {
  EXPECT_TRUE(RunExtensionSubtest("sandboxed_pages", "main.html")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, SandboxedPagesCSP) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  // This app attempts to load remote web content inside a sandboxed page.
  // Loading web content will fail because of CSP. In addition to that we will
  // show manifest warnings, hence ignore_manifest_warnings is set to true.
  ASSERT_TRUE(
      RunExtensionTest({.name = "sandboxed_pages_csp", .page_url = "main.html"},
                       {.ignore_manifest_warnings = true}))
      << message_;
}

}  // namespace extensions
