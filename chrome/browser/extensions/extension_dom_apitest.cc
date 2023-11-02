// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "extensions/test/result_catcher.h"

namespace extensions {

// Test that a content script allows access of the closed shadow root in
// content.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest,
                       OpenOrClosedShadowRootInContentScript) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  const GURL url = embedded_test_server()->GetURL(
      "/extensions/test_file_with_closed_shadow_root.html");

  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(
      "dom/closed_shadow_root_from_content_script")));

  ResultCatcher catcher;
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  EXPECT_TRUE(catcher.GetNextResult());
}

// Test that a background script allows access of the closed shadow root in
// background page.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest,
                       OpenOrClosedShadowRootInBackgroundPage) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("dom/closed_shadow_root_from_background_page"))
      << message_;
}

}  // namespace extensions
