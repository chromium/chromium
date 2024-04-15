// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/version_info/channel.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/extension_features.h"
#include "net/dns/mock_host_resolver.h"

namespace extensions {

class FaviconApiTest : public ExtensionApiTest {
 public:
  FaviconApiTest() = default;

 protected:
  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(StartEmbeddedTestServer());
  }
};

// TODO(crbug.com/40836446): Test is flaky on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_Extension DISABLED_Extension
#else
#define MAYBE_Extension Extension
#endif

// Fetch favicon from an extension with the correct permission.
IN_PROC_BROWSER_TEST_F(FaviconApiTest, MAYBE_Extension) {
  // Cache the favicon by loading a test page, then fetch the favicon.
  GURL page_url = embedded_test_server()->GetURL(
      "www.example.com", "/extensions/favicon/test_file.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), page_url));

  ASSERT_TRUE(
      RunExtensionTest("favicon/extension", {.extension_url = "test.html"}))
      << message_;
}

// Fetch favicon when an extension doesn't have the necessary permission.
IN_PROC_BROWSER_TEST_F(FaviconApiTest, Permission) {
  ASSERT_TRUE(RunExtensionTest("favicon/permission_missing",
                               {.extension_url = "test.html"}))
      << message_;
}

}  // namespace extensions
