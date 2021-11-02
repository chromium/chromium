// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
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
    extension_dir_ =
        test_data_dir_.AppendASCII("web_authentication_proxy/main");
    https_test_server_.ServeFilesFromDirectory(extension_dir_);
    ASSERT_TRUE(https_test_server_.Start());
  }

  void SetJsTestName(const std::string& name) { SetCustomArg(name); }

  bool NavigateAndCallIsUVPAA() {
    if (!ui_test_utils::NavigateToURL(
            browser(), https_test_server_.GetURL("/page.html"))) {
      ADD_FAILURE() << "Failed to navigate to test URL";
    }
    return content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                           "PublicKeyCredential."
                           "isUserVerifyingPlatformAuthenticatorAvailable();")
        .ExtractBool();
  }

  base::FilePath extension_dir_;
  net::EmbeddedTestServer https_test_server_{
      net::EmbeddedTestServer::TYPE_HTTPS};
};

IN_PROC_BROWSER_TEST_F(WebAuthenticationProxyApiTest, AttachDetach) {
  SetJsTestName("attachDetach");
  EXPECT_TRUE(RunExtensionTest("web_authentication_proxy/main"));
}

IN_PROC_BROWSER_TEST_F(WebAuthenticationProxyApiTest, AttachReload) {
  SetJsTestName("attachReload");
  // Load an extension that immediately attaches.
  ResultCatcher catcher;
  const Extension* extension = LoadExtension(extension_dir_);
  ASSERT_TRUE(extension) << message_;
  ASSERT_TRUE(catcher.GetNextResult());

  // Extension should be able to re-attach after a reload.
  ReloadExtension(extension->id());
  ASSERT_TRUE(catcher.GetNextResult());
}

IN_PROC_BROWSER_TEST_F(WebAuthenticationProxyApiTest, AttachSecondExtension) {
  SetJsTestName("attachSecondExtension");
  ResultCatcher catcher;

  // Load the extension and wait for it to attach.
  ExtensionTestMessageListener attach_listener("attached", true);
  ASSERT_TRUE(LoadExtension(extension_dir_)) << message_;
  ASSERT_TRUE(attach_listener.WaitUntilSatisfied());

  // Load a second extension and watch it fail to attach.
  ExtensionTestMessageListener attach_fail_listener("attachFailed", true);
  const Extension* second_extension = LoadExtension(
      test_data_dir_.AppendASCII("web_authentication_proxy/second"));
  ASSERT_TRUE(second_extension) << message_;
  ASSERT_TRUE(attach_fail_listener.WaitUntilSatisfied());

  // Tell the first extension to detach. Then tell the second extension to
  // attach.
  ExtensionTestMessageListener detach_listener("detached", true);
  attach_listener.Reply("");
  ASSERT_TRUE(detach_listener.WaitUntilSatisfied());
  attach_fail_listener.Reply("");
  EXPECT_TRUE(catcher.GetNextResult());

  // Disable the second extension and tell the first extension to re-attach.
  UnloadExtension(second_extension->id());
  detach_listener.Reply("");
  EXPECT_TRUE(catcher.GetNextResult());
}

IN_PROC_BROWSER_TEST_F(WebAuthenticationProxyApiTest, IsUVPAA) {
  SetJsTestName("isUvpaa");
  // Load the extension and wait for its proxy event handler to be installed.
  ExtensionTestMessageListener ready_listener("ready", false);
  ASSERT_TRUE(LoadExtension(extension_dir_)) << message_;
  ASSERT_TRUE(ready_listener.WaitUntilSatisfied());

  // The extension sets the result for isUvpaa to `false` and `true` for
  // two different requests.
  for (const bool expected : {false, true}) {
    // The extension verifies it receives the proper requests.
    ResultCatcher result_catcher;
    bool is_uvpaa = NavigateAndCallIsUVPAA();
    EXPECT_EQ(is_uvpaa, expected);
    EXPECT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
  }
}

IN_PROC_BROWSER_TEST_F(WebAuthenticationProxyApiTest,
                       CallIsUVPAAWhileNotAttached) {
  SetJsTestName("isUvpaaNotAttached");
  ResultCatcher result_catcher;

  // Load the extension and wait for it to initialize.
  ExtensionTestMessageListener ready_listener("ready", true);
  ASSERT_TRUE(LoadExtension(extension_dir_)) << message_;
  ASSERT_TRUE(ready_listener.WaitUntilSatisfied());

  // Call isUvpaa() and tell the extension that there is a result. The extension
  // JS verifies that its event listener wasn't called, because it didn't attach
  // itself.
  NavigateAndCallIsUVPAA();  // Actual result is ignored.
  ready_listener.Reply("");
  EXPECT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
}

IN_PROC_BROWSER_TEST_F(WebAuthenticationProxyApiTest, IsUVPAAResolvesOnDetach) {
  SetJsTestName("isUvpaaResolvesOnDetach");
  ResultCatcher result_catcher;

  ExtensionTestMessageListener ready_listener("ready", true);
  ASSERT_TRUE(LoadExtension(extension_dir_)) << message_;
  ASSERT_TRUE(ready_listener.WaitUntilSatisfied());

  // Call isUvpaa() and tell the extension that there is a result. The extension
  // never resolves the request but detaches itself.
  EXPECT_EQ(false, NavigateAndCallIsUVPAA());
  ready_listener.Reply("");
  EXPECT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
}

}  // namespace
}  // namespace extensions
