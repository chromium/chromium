// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace {

class WebAuthenticationProxyApiTest : public ExtensionApiTest {
 protected:
  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    test_data_dir_ = test_data_dir_.AppendASCII("web_authentication_proxy");
    https_test_server_.ServeFilesFromDirectory(test_data_dir_);
    ASSERT_TRUE(https_test_server_.Start());
  }

  net::EmbeddedTestServer https_test_server_{
      net::EmbeddedTestServer::TYPE_HTTPS};
};

IN_PROC_BROWSER_TEST_F(WebAuthenticationProxyApiTest, IsUVPAA) {
  // Load the extension and wait for its proxy event handler to be installed.
  ExtensionTestMessageListener ready_listener("ready", false);
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("is_uvpaa")))
      << message_;
  ASSERT_TRUE(ready_listener.WaitUntilSatisfied());

  // Navigate to a new URL and check that the proxying works.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_test_server_.GetURL("/is_uvpaa/page.html")));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  // The extension sets the result for isUvpaa to `false` and `true` for
  // two different requests.
  for (const bool expected : {false, true}) {
    // The extension verifies it receives the proper requests.
    ResultCatcher result_catcher;
    bool is_uvpaa =
        content::EvalJs(web_contents,
                        "PublicKeyCredential."
                        "isUserVerifyingPlatformAuthenticatorAvailable();")
            .ExtractBool();
    EXPECT_EQ(is_uvpaa, expected);
    EXPECT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
  }
}

}  // namespace
}  // namespace extensions
