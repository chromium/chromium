// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "extensions/test/result_catcher.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

namespace extensions {

// Tests that we throw errors when you try using extension APIs that aren't
// supported in content scripts.
// Timey-outy on mac. http://crbug.com/89116
#if defined(OS_MAC)
#define MAYBE_Stubs DISABLED_Stubs
#else
#define MAYBE_Stubs Stubs
#endif
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, MAYBE_Stubs) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ASSERT_TRUE(RunExtensionTest("stubs")) << message_;

  // Navigate to a simple http:// page, which should get the content script
  // injected and run the rest of the test.
  GURL url(embedded_test_server()->GetURL("/extensions/test_file.html"));
  ui_test_utils::NavigateToURL(browser(), url);

  ResultCatcher catcher;
  ASSERT_TRUE(catcher.GetNextResult());
}

// Tests that all API features that are available to a platform app actually
// can be used in an app. For example, this test will fail if a developer adds
// an API feature without providing a schema. http://crbug.com/369318
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, StubsApp) {
  ASSERT_TRUE(RunExtensionTest("stubs_app", {.launch_as_platform_app = true},
                               {.ignore_manifest_warnings = true}))
      << message_;
}

}  // namespace extensions
