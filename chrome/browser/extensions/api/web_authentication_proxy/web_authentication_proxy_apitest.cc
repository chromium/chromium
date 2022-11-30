// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/extensions/api/web_authentication_proxy/remote_session_state_change.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/browsertest_util.h"
#include "extensions/common/extension.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace {

//  base64url('test') = 'dGVzdA'. This matches the credential ID of
//  `MAKE_CREDENTIAL_RESPONSE_JSON` in the JS tests.
constexpr char kTestCredentialId[] = "dGVzdA";

// Domain to serve files from because WebAuthn won't let us scope credentials to
// localhost. Must be from `net::EmbeddedTestServer::CERT_TEST_NAMES`.
constexpr char kTestDomain[] = "a.test";

constexpr char kJsErrorPrefix[] = "a JavaScript error: \"";

MATCHER_P(IsJsError, name, "") {
  return base::StartsWith(arg.error, base::StrCat({kJsErrorPrefix, name}));
}

MATCHER_P2(IsJsErrorWithMessage, name, message, "") {
  return base::StrCat({kJsErrorPrefix, name, ": ", message, "\"\n"}) ==
         arg.error;
}

class WebAuthenticationProxyApiTest : public ExtensionApiTest {
 protected:
  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    extension_dir_ =
        test_data_dir_.AppendASCII("web_authentication_proxy/main");
    host_resolver()->AddRule("*", "127.0.0.1");
    https_test_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_test_server_.ServeFilesFromDirectory(extension_dir_);
    ASSERT_TRUE(https_test_server_.Start());
  }

  void SetJsTestName(const std::string& name) { SetCustomArg(name); }

  bool NavigateAndCallIsUVPAA() {
    if (!ui_test_utils::NavigateToURL(
            browser(), https_test_server_.GetURL(kTestDomain, "/page.html"))) {
      ADD_FAILURE() << "Failed to navigate to test URL";
    }
    return content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                           "PublicKeyCredential."
                           "isUserVerifyingPlatformAuthenticatorAvailable();")
        .ExtractBool();
  }

  content::EvalJsResult NavigateAndCallMakeCredential() {
    if (!ui_test_utils::NavigateToURL(
            browser(), https_test_server_.GetURL(kTestDomain, "/page.html"))) {
      ADD_FAILURE() << "Failed to navigate to test URL";
    }
    constexpr char kMakeCredentialJs[] =
        R"((async () => {
              let credential = await navigator.credentials.create({publicKey: {
                rp: {'id': 'a.test', 'name': 'A'},
                challenge: new ArrayBuffer(),
                user: {displayName : 'A', name: 'A', id: new ArrayBuffer()},
                pubKeyCredParams: [],
              }});
              return credential.id;
            })();)";
    return content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                           kMakeCredentialJs);
  }

  bool NavigateAndCallMakeCredentialThenCancel() {
    if (!ui_test_utils::NavigateToURL(
            browser(), https_test_server_.GetURL(kTestDomain, "/page.html"))) {
      ADD_FAILURE() << "Failed to navigate to test URL";
      return false;
    }
    constexpr char kMakeCredentialJs[] =
        R"((async () => {
              let abort = new AbortController();
              let createPromise = navigator.credentials.create({publicKey: {
                  rp: {'id': 'a.test', 'name': 'A'},
                  challenge: new ArrayBuffer(),
                  user: {displayName : 'A', name: 'A', id: new ArrayBuffer()},
                  pubKeyCredParams: [],
                },
                signal: abort.signal});
              abort.abort();
              let err = await createPromise;
              return err;
            })();)";
    return content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                           kMakeCredentialJs)
               .error.find("AbortError") >= 0;
  }

  content::EvalJsResult NavigateAndCallGetAssertion() {
    if (!ui_test_utils::NavigateToURL(
            browser(), https_test_server_.GetURL(kTestDomain, "/page.html"))) {
      ADD_FAILURE() << "Failed to navigate to test URL";
    }
    constexpr char kGetAssertionJs[] =
        R"((async () => {
              let credential = await navigator.credentials.get({publicKey: {
                challenge: new ArrayBuffer(),
                rpId: 'a.test',
              }});
              return credential.id;
            })();)";
    return content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                           kGetAssertionJs);
  }

  bool NavigateAndCallGetAssertionThenCancel() {
    if (!ui_test_utils::NavigateToURL(
            browser(), https_test_server_.GetURL(kTestDomain, "/page.html"))) {
      ADD_FAILURE() << "Failed to navigate to test URL";
      return false;
    }
    constexpr char kGetAssertionJs[] =
        R"((async () => {
              let abort = new AbortController();
              let getPromise = navigator.credentials.get({publicKey: {
                  challenge: new ArrayBuffer(),
                  rpId: 'a.test',
                },
                signal: abort.signal});
              abort.abort();
              let err = await getPromise;
              return err;
            })();)";
    return content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                           kGetAssertionJs)
               .error.find("AbortError") >= 0;
  }

  base::FilePath extension_dir_;
  net::EmbeddedTestServer https_test_server_{
      net::EmbeddedTestServer::TYPE_HTTPS};
};

IN_PROC_BROWSER_TEST_F(WebAuthenticationProxyApiTest, AttachDetach) {
  SetJsTestName("attachDetach");
  EXPECT_TRUE(RunExtensionTest("web_authentication_proxy/main"));
}

// TODO(crbug.com/1276042): Flaky on all platforms
IN_PROC_BROWSER_TEST_F(WebAuthenticationProxyApiTest, DISABLED_AttachReload) {
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
  ExtensionTestMessageListener attach_listener("attached",
                                               ReplyBehavior::kWillReply);
  ASSERT_TRUE(LoadExtension(extension_dir_)) << message_;
  ASSERT_TRUE(attach_listener.WaitUntilSatisfied());

  // Load a second extension and watch it fail to attach.
  ExtensionTestMessageListener attach_fail_listener("attachFailed",
                                                    ReplyBehavior::kWillReply);
  const Extension* second_extension = LoadExtension(
      test_data_dir_.AppendASCII("web_authentication_proxy/second"));
  ASSERT_TRUE(second_extension) << message_;
  ASSERT_TRUE(attach_fail_listener.WaitUntilSatisfied());

  // Tell the first extension to detach. Then tell the second extension to
  // attach.
  ExtensionTestMessageListener detach_listener("detached",
                                               ReplyBehavior::kWillReply);
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
  ExtensionTestMessageListener ready_listener("ready");
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
  ExtensionTestMessageListener ready_listener("ready",
                                              ReplyBehavior::kWillReply);
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

  ExtensionTestMessageListener ready_listener("ready",
                                              ReplyBehavior::kWillReply);
  ASSERT_TRUE(LoadExtension(extension_dir_)) << message_;
  ASSERT_TRUE(ready_listener.WaitUntilSatisfied());

  // Call isUvpaa() and tell the extension that there is a result. The extension
  // never resolves the request but detaches itself.
  EXPECT_EQ(false, NavigateAndCallIsUVPAA());
  ready_listener.Reply("");
  EXPECT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
}

IN_PROC_BROWSER_TEST_F(WebAuthenticationProxyApiTest, MakeCredential) {
  SetJsTestName("makeCredential");
  ResultCatcher result_catcher;

  ExtensionTestMessageListener ready_listener("ready");
  ASSERT_TRUE(LoadExtension(extension_dir_)) << message_;
  ASSERT_TRUE(ready_listener.WaitUntilSatisfied());

  EXPECT_EQ(NavigateAndCallMakeCredential().ExtractString(), kTestCredentialId);
  EXPECT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
}

IN_PROC_BROWSER_TEST_F(WebAuthenticationProxyApiTest, MakeCredentialError) {
  SetJsTestName("makeCredentialError");
  ResultCatcher result_catcher;

  ExtensionTestMessageListener request_listener("nextRequest");
  ExtensionTestMessageListener error_listener("nextError",
                                              ReplyBehavior::kWillReply);
  ASSERT_TRUE(LoadExtension(extension_dir_)) << message_;

  // The JS side listens for DOMError names to pass to completeCreateRequest().
  // The DOMError observed by the client-side JS that made the WebAuthn request
  // should match.
  constexpr const char* kDomErrorNames[] = {
      // clang-format off
      "NotAllowedError",
      "InvalidStateError",
      "OperationError",
      "NotSupportedError",
      "AbortError",
      "NotReadableError",
      "SecurityError",
      // clang-format on
  };
  for (auto* error_name : kDomErrorNames) {
    ASSERT_TRUE(error_listener.WaitUntilSatisfied());
    error_listener.Reply(error_name);
    error_listener.Reset();
    ASSERT_TRUE(request_listener.WaitUntilSatisfied());
    request_listener.Reset();
    // `TEST_ERROR_MESSAGE` in `main/test.js`.
    constexpr char kErrorMessage[] = "test error message";
    EXPECT_THAT(NavigateAndCallMakeCredential(),
                IsJsErrorWithMessage(error_name, kErrorMessage));
  }

  // Tell the JS side to stop expecting more errors and end the test.
  ASSERT_TRUE(error_listener.WaitUntilSatisfied());
  error_listener.Reply("");
  EXPECT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
}

IN_PROC_BROWSER_TEST_F(WebAuthenticationProxyApiTest,
                       MakeCredentialResolvesOnDetach) {
  SetJsTestName("makeCredentialResolvesOnDetach");
  ResultCatcher result_catcher;

  ExtensionTestMessageListener ready_listener("ready",
                                              ReplyBehavior::kWillReply);
  ASSERT_TRUE(LoadExtension(extension_dir_)) << message_;
  ASSERT_TRUE(ready_listener.WaitUntilSatisfied());

  // Call makeCredential() and tell the extension that there is a result. The
  // extension never resolves the request but detaches itself.
  EXPECT_THAT(NavigateAndCallMakeCredential(), IsJsError("AbortError"));
  ready_listener.Reply("");
  EXPECT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
}

IN_PROC_BROWSER_TEST_F(WebAuthenticationProxyApiTest, MakeCredentialCancel) {
  SetJsTestName("makeCredentialCancel");
  ResultCatcher result_catcher;

  ExtensionTestMessageListener ready_listener("ready");
  ASSERT_TRUE(LoadExtension(extension_dir_)) << message_;
  ASSERT_TRUE(ready_listener.WaitUntilSatisfied());

  ExtensionTestMessageListener request_listener("request",
                                                ReplyBehavior::kWillReply);
  EXPECT_TRUE(NavigateAndCallMakeCredentialThenCancel());
  ASSERT_TRUE(request_listener.WaitUntilSatisfied());
  request_listener.Reply("");

  EXPECT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
}

IN_PROC_BROWSER_TEST_F(WebAuthenticationProxyApiTest, GetAssertion) {
  SetJsTestName("getAssertion");
  ResultCatcher result_catcher;

  ExtensionTestMessageListener ready_listener("ready");
  ASSERT_TRUE(LoadExtension(extension_dir_)) << message_;
  ASSERT_TRUE(ready_listener.WaitUntilSatisfied());

  EXPECT_EQ(NavigateAndCallGetAssertion().ExtractString(), kTestCredentialId);
  EXPECT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
}

IN_PROC_BROWSER_TEST_F(WebAuthenticationProxyApiTest, GetAssertionError) {
  SetJsTestName("getAssertionError");
  ResultCatcher result_catcher;

  ExtensionTestMessageListener request_listener("nextRequest");
  ExtensionTestMessageListener error_listener("nextError",
                                              ReplyBehavior::kWillReply);
  ASSERT_TRUE(LoadExtension(extension_dir_)) << message_;

  // The JS side listens for DOMError names to pass to completeCreateRequest().
  // The DOMError observed by the client-side JS that made the WebAuthn request
  // should match.
  constexpr const char* kDomErrorNames[] = {
      // clang-format off
      "NotAllowedError",
      "InvalidStateError",
      "OperationError",
      "NotSupportedError",
      "AbortError",
      "NotReadableError",
      "SecurityError",
      // clang-format on
  };
  for (auto* error_name : kDomErrorNames) {
    ASSERT_TRUE(error_listener.WaitUntilSatisfied());
    error_listener.Reply(error_name);
    error_listener.Reset();
    ASSERT_TRUE(request_listener.WaitUntilSatisfied());
    request_listener.Reset();
    // `TEST_ERROR_MESSAGE` in `main/test.js`.
    constexpr char kErrorMessage[] = "test error message";
    EXPECT_THAT(NavigateAndCallGetAssertion(),
                IsJsErrorWithMessage(error_name, kErrorMessage));
  }

  // Tell the JS side to stop expecting more errors and end the test.
  ASSERT_TRUE(error_listener.WaitUntilSatisfied());
  error_listener.Reply("");
  EXPECT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
}

IN_PROC_BROWSER_TEST_F(WebAuthenticationProxyApiTest,
                       GetAssertionResolvesOnDetach) {
  SetJsTestName("getAssertionResolvesOnDetach");
  ResultCatcher result_catcher;

  ExtensionTestMessageListener ready_listener("ready",
                                              ReplyBehavior::kWillReply);
  ASSERT_TRUE(LoadExtension(extension_dir_)) << message_;
  ASSERT_TRUE(ready_listener.WaitUntilSatisfied());

  // Call getAssertion() and tell the extension that there is a result. The
  // extension never resolves the request but detaches itself.
  EXPECT_THAT(NavigateAndCallGetAssertion(), IsJsError("AbortError"));
  ready_listener.Reply("");
  EXPECT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
}

IN_PROC_BROWSER_TEST_F(WebAuthenticationProxyApiTest, GetAssertionCancel) {
  SetJsTestName("getAssertionCancel");
  ResultCatcher result_catcher;

  ExtensionTestMessageListener ready_listener("ready");
  ASSERT_TRUE(LoadExtension(extension_dir_)) << message_;
  ASSERT_TRUE(ready_listener.WaitUntilSatisfied());

  ExtensionTestMessageListener request_listener("request",
                                                ReplyBehavior::kWillReply);
  EXPECT_TRUE(NavigateAndCallGetAssertionThenCancel());
  ASSERT_TRUE(request_listener.WaitUntilSatisfied());
  request_listener.Reply("");

  EXPECT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
}

IN_PROC_BROWSER_TEST_F(WebAuthenticationProxyApiTest,
                       RemoteSessionStateChange) {
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII(
                        "web_authentication_proxy/remote_session_state_change"),
                    {.wait_for_registration_stored = true});
  ASSERT_TRUE(extension) << message_;

  browsertest_util::StopServiceWorkerForExtensionGlobalScope(
      browser()->profile(), extension->id());

  // Write to the magic file to trigger the event.
  ResultCatcher result_catcher;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath dir;
    ASSERT_TRUE(WebAuthenticationProxyRemoteSessionStateChangeNotifier::
                    GetSessionStateChangeDir(&dir));
    ASSERT_TRUE(base::CreateDirectory(dir));
    ASSERT_EQ(base::WriteFile(dir.AppendASCII(extension->id()), nullptr, 0), 0);
  }
  EXPECT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
}

}  // namespace
}  // namespace extensions
