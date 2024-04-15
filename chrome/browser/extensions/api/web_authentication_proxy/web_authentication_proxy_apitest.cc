// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/extensions/api/web_authentication_proxy/remote_session_state_change.h"
#include "chrome/browser/extensions/api/web_authentication_proxy/web_authentication_proxy_service.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_management_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/browsertest_util.h"
#include "extensions/browser/permissions_manager.h"
#include "extensions/common/extension.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
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

  // Sets the test case name to execute. The test name is the name of a function
  // in the `availableTests` array defined in the extension service worker JS.
  void SetJsTestName(const std::string& name) { SetCustomArg(name); }

  void SetTestDomainToNavigate(const std::string& domain) {
    test_domain_ = domain;
  }

  bool NavigateAndCallIsUVPAA() {
    if (!ui_test_utils::NavigateToURL(
            browser(), https_test_server_.GetURL(test_domain_, "/page.html"))) {
      ADD_FAILURE() << "Failed to navigate to test URL";
    }
    return content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                           "PublicKeyCredential."
                           "isUserVerifyingPlatformAuthenticatorAvailable();")
        .ExtractBool();
  }

  content::EvalJsResult NavigateAndCallMakeCredential(Browser* browser) {
    if (!ui_test_utils::NavigateToURL(
            browser, https_test_server_.GetURL(test_domain_, "/page.html"))) {
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
    return content::EvalJs(browser->tab_strip_model()->GetActiveWebContents(),
                           kMakeCredentialJs);
  }

  content::EvalJsResult NavigateAndCallMakeCredential() {
    return NavigateAndCallMakeCredential(browser());
  }

  bool NavigateAndCallMakeCredentialThenCancel() {
    if (!ui_test_utils::NavigateToURL(
            browser(), https_test_server_.GetURL(test_domain_, "/page.html"))) {
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
            browser(), https_test_server_.GetURL(test_domain_, "/page.html"))) {
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
            browser(), https_test_server_.GetURL(test_domain_, "/page.html"))) {
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

  bool ProxyIsActive() { return ProxyIsActiveForContext(profile()); }

  bool ProxyIsActiveForContext(content::BrowserContext* context) {
    WebAuthenticationProxyService* proxy =
        WebAuthenticationProxyService::GetIfProxyAttached(context);
    return proxy && proxy->IsActive(url::Origin::CreateFromNormalizedTuple(
                        "https", test_domain_, 443));
  }

  const Extension* ProxyForContext(content::BrowserContext* context) {
    return WebAuthenticationProxyService::GetIfProxyAttached(context)
        ->GetActiveRequestProxy();
  }

  std::string test_domain_{kTestDomain};
  base::FilePath extension_dir_;
  net::EmbeddedTestServer https_test_server_{
      net::EmbeddedTestServer::TYPE_HTTPS};
};

IN_PROC_BROWSER_TEST_F(WebAuthenticationProxyApiTest, AttachDetach) {
  SetJsTestName("attachDetach");
  EXPECT_TRUE(RunExtensionTest("web_authentication_proxy/main"));
}

// TODO(crbug.com/40808644): Flaky on all platforms
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

  browsertest_util::StopServiceWorkerForExtensionGlobalScope(profile(),
                                                             extension->id());

  // Write to the magic file to trigger the event.
  ResultCatcher result_catcher;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath dir;
    ASSERT_TRUE(WebAuthenticationProxyRemoteSessionStateChangeNotifier::
                    GetSessionStateChangeDir(&dir));
    ASSERT_TRUE(base::CreateDirectory(dir));
    ASSERT_TRUE(base::WriteFile(dir.AppendASCII(extension->id()), ""));
  }
  EXPECT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
}

// An extension with manifest value `"incognito": "spanning"` (the default) that
// attached in a main profile should also be attached in associated incognito
// profiles.
IN_PROC_BROWSER_TEST_F(WebAuthenticationProxyApiTest, IncognitoSpanning) {
  SetJsTestName("incognitoSpanning");

  // Load the extension and wait for the service worker to call `attach()`.
  ExtensionTestMessageListener ready_listener("ready");
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("web_authentication_proxy/main"),
      {.allow_in_incognito = true, .wait_for_registration_stored = true});
  ASSERT_TRUE(extension);
  ASSERT_TRUE(ready_listener.WaitUntilSatisfied());

  // The proxy should be active in the test browser profile.
  EXPECT_TRUE(ProxyIsActiveForContext(profile()));
  EXPECT_EQ(ProxyForContext(profile()), extension);
  EXPECT_EQ(NavigateAndCallMakeCredential(browser()).ExtractString(),
            kTestCredentialId);

  // And it should also be active in an incognito profile created from the main
  // profile.
  Browser* incognito_browser = CreateIncognitoBrowser();
  EXPECT_TRUE(ProxyIsActiveForContext(incognito_browser->profile()));
  EXPECT_EQ(ProxyForContext(incognito_browser->profile()), extension);
  EXPECT_EQ(NavigateAndCallMakeCredential(incognito_browser).ExtractString(),
            kTestCredentialId);

  // After the extension is unloaded, it should be detached from the regular and
  // incognito profiles.
  UnloadExtension(extension->id());
  EXPECT_FALSE(ProxyIsActiveForContext(profile()));
  EXPECT_FALSE(ProxyIsActiveForContext(incognito_browser->profile()));
}

// An extension with manifest value `"incognito": "spanning"` (the default) but
// that isn't permitted to run in incognito should not be considered attached in
// incognito, even though it is attached in the regular profile.
IN_PROC_BROWSER_TEST_F(WebAuthenticationProxyApiTest, IncognitoNotAllowed) {
  SetJsTestName("incognitoSpanning");

  // Load the extension and wait for the service worker to call `attach()`.
  ExtensionTestMessageListener ready_listener("ready");
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("web_authentication_proxy/main"),
      {.allow_in_incognito = false, .wait_for_registration_stored = true});
  ASSERT_TRUE(extension) << message_;
  ASSERT_TRUE(ready_listener.WaitUntilSatisfied());

  // The proxy should be active in the test browser profile.
  EXPECT_TRUE(ProxyIsActiveForContext(profile()));
  EXPECT_EQ(ProxyForContext(profile()), extension);

  // The proxy service in incognito is the same as in the original profile. But
  // because the extension isn't allowed to run in incognito, it doesn't get to
  // proxy requests.
  Browser* incognito_browser = CreateIncognitoBrowser();
  EXPECT_FALSE(ProxyIsActiveForContext(incognito_browser->profile()));
}

// A split mode extension can be active in regular and incognito profiles.
IN_PROC_BROWSER_TEST_F(WebAuthenticationProxyApiTest,
                       SplitIncognitoAndRegular) {
  SetJsTestName("incognitoAndRegular");

  // Load the extension and wait for the regular split service worker to call
  // `attach()`.
  ExtensionTestMessageListener regular_ready_listener("regular ready");
  ExtensionTestMessageListener incognito_ready_listener("incognito ready");
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("web_authentication_proxy/incognito_split"),
      {.allow_in_incognito = true, .wait_for_registration_stored = true});
  ASSERT_TRUE(extension) << message_;
  ASSERT_TRUE(regular_ready_listener.WaitUntilSatisfied());

  // The proxy should be active in the "regular" profile.
  EXPECT_TRUE(ProxyIsActiveForContext(profile()));
  EXPECT_EQ(ProxyForContext(profile()), extension);
  EXPECT_EQ(NavigateAndCallMakeCredential(browser()).ExtractString(),
            kTestCredentialId);

  // The incognito split also called attach and should therefore be active.
  Browser* incognito_browser = CreateIncognitoBrowser();
  ASSERT_TRUE(incognito_ready_listener.WaitUntilSatisfied());
  EXPECT_TRUE(ProxyIsActiveForContext(incognito_browser->profile()));
  EXPECT_EQ(ProxyForContext(incognito_browser->profile()), extension);
  EXPECT_EQ(NavigateAndCallMakeCredential(incognito_browser).ExtractString(),
            kTestCredentialId);

  UnloadExtension(extension->id());
  EXPECT_FALSE(ProxyIsActiveForContext(profile()));
  EXPECT_FALSE(ProxyIsActiveForContext(incognito_browser->profile()));
}

// A split mode extension that is active in a regular profile is not necessarily
// active in an associated incognito profile.
IN_PROC_BROWSER_TEST_F(WebAuthenticationProxyApiTest, SplitRegularOnly) {
  SetJsTestName("regularOnly");

  // Load the extension and wait for the split service worker to load in regular
  // and incognito.
  ExtensionTestMessageListener regular_ready_listener("regular ready");
  ExtensionTestMessageListener incognito_ready_listener("incognito ready");
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("web_authentication_proxy/incognito_split"),
      {.allow_in_incognito = true, .wait_for_registration_stored = true});
  ASSERT_TRUE(extension) << message_;
  ASSERT_TRUE(regular_ready_listener.WaitUntilSatisfied());

  // The proxy should be active in the "regular" profile, but not incognito.
  EXPECT_TRUE(ProxyIsActiveForContext(profile()));
  EXPECT_EQ(ProxyForContext(profile()), extension);
  EXPECT_EQ(NavigateAndCallMakeCredential(browser()).ExtractString(),
            kTestCredentialId);

  Browser* incognito_browser = CreateIncognitoBrowser();
  ASSERT_TRUE(incognito_ready_listener.WaitUntilSatisfied());
  EXPECT_FALSE(ProxyIsActiveForContext(incognito_browser->profile()));

  UnloadExtension(extension->id());
  EXPECT_FALSE(ProxyIsActiveForContext(profile()));
  EXPECT_FALSE(ProxyIsActiveForContext(incognito_browser->profile()));
}

// A split mode extension that is active in an incognito profile is not
// necessarily active in the regular parent profile.
IN_PROC_BROWSER_TEST_F(WebAuthenticationProxyApiTest, SplitIncognitoOnly) {
  SetJsTestName("incognitoOnly");

  // Load the extension and wait for the split service worker to load in regular
  // and incognito.
  ExtensionTestMessageListener regular_ready_listener("regular ready");
  ExtensionTestMessageListener incognito_ready_listener("incognito ready");
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("web_authentication_proxy/incognito_split"),
      {.allow_in_incognito = true, .wait_for_registration_stored = true});
  ASSERT_TRUE(extension) << message_;
  ASSERT_TRUE(regular_ready_listener.WaitUntilSatisfied());

  // The proxy should not be active in the "regular" profile, but should be
  // active in incognito.
  EXPECT_FALSE(ProxyIsActiveForContext(profile()));

  Browser* incognito_browser = CreateIncognitoBrowser();
  ASSERT_TRUE(incognito_ready_listener.WaitUntilSatisfied());
  EXPECT_TRUE(ProxyIsActiveForContext(incognito_browser->profile()));
  EXPECT_EQ(ProxyForContext(incognito_browser->profile()), extension);
  EXPECT_EQ(NavigateAndCallMakeCredential(incognito_browser).ExtractString(),
            kTestCredentialId);

  UnloadExtension(extension->id());
  EXPECT_FALSE(ProxyIsActiveForContext(profile()));
  EXPECT_FALSE(ProxyIsActiveForContext(incognito_browser->profile()));
}

// A split mode extension should reattach after the incognito window is
// destroyed and recreated.
IN_PROC_BROWSER_TEST_F(WebAuthenticationProxyApiTest, SplitModeDestruction) {
  SetJsTestName("incognitoOnly");

  // Load the extension and wait for the split service worker to load in regular
  // and incognito.
  ExtensionTestMessageListener regular_ready_listener("regular ready");
  ExtensionTestMessageListener incognito_ready_listener("incognito ready");
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("web_authentication_proxy/incognito_split"),
      {.allow_in_incognito = true, .wait_for_registration_stored = true});
  ASSERT_TRUE(extension) << message_;
  ASSERT_TRUE(regular_ready_listener.WaitUntilSatisfied());

  // Open an incognito browser and wait for the extension to attach.
  Browser* incognito_browser = CreateIncognitoBrowser();
  ASSERT_TRUE(incognito_ready_listener.WaitUntilSatisfied());
  EXPECT_TRUE(ProxyIsActiveForContext(incognito_browser->profile()));
  EXPECT_EQ(ProxyForContext(incognito_browser->profile()), extension);

  // Close the browser, then recreate it. The extension should re-attach
  // automatically.
  CloseBrowserSynchronously(incognito_browser);

  incognito_ready_listener.Reset();
  incognito_browser = CreateIncognitoBrowser();
  ASSERT_TRUE(incognito_ready_listener.WaitUntilSatisfied());
  EXPECT_TRUE(ProxyIsActiveForContext(incognito_browser->profile()));
  EXPECT_EQ(ProxyForContext(incognito_browser->profile()), extension);
}

// The webAuthenticationproxy API does not consider user host permissions.
IN_PROC_BROWSER_TEST_F(WebAuthenticationProxyApiTest, UserHostPermissions) {
  SetJsTestName("policyBlockedHosts");

  // Set up a user-restricted host.
  const auto user_blocked_host =
      url::Origin::Create(GURL("https://blocked.b.test"));
  PermissionsManager::Get(profile())->AddUserRestrictedSite(user_blocked_host);

  ExtensionTestMessageListener ready_listener("ready");
  ASSERT_TRUE(LoadExtension(extension_dir_)) << message_;
  ASSERT_TRUE(ready_listener.WaitUntilSatisfied());
  ASSERT_TRUE(ProxyIsActive());

  // The proxy should function normally on a user blocked host.
  SetTestDomainToNavigate("blocked.b.test");
  WebAuthenticationProxyService* proxy =
      WebAuthenticationProxyService::GetIfProxyAttached(profile());
  ASSERT_TRUE(proxy);
  EXPECT_TRUE(proxy->IsActive(user_blocked_host));
  EXPECT_TRUE(NavigateAndCallIsUVPAA());
}

class WebAuthenticationProxyApiTestWithPolicyOverride
    : public WebAuthenticationProxyApiTest {
 protected:
  void SetUpInProcessBrowserTestFixture() override {
    ExtensionApiTest::SetUpInProcessBrowserTestFixture();
    // Set up a mock policy provider.
    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy_provider_.SetAutoRefresh();
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);
  }

  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
};

IN_PROC_BROWSER_TEST_F(WebAuthenticationProxyApiTestWithPolicyOverride,
                       BlockedHosts) {
  SetJsTestName("policyBlockedHosts");

  ExtensionTestMessageListener ready_listener("ready");
  ASSERT_TRUE(LoadExtension(extension_dir_)) << message_;
  ASSERT_TRUE(ready_listener.WaitUntilSatisfied());
  ASSERT_TRUE(ProxyIsActive());

  {
    // Disable the proxy on *.b.test via `runtime_blocked_hosts`, but exempt
    // allowed.b.test via `runtime_allowed_hosts`.
    ExtensionManagementPolicyUpdater pref(&policy_provider_);
    pref.AddPolicyBlockedHost("*", "*://*.b.test");
    pref.AddPolicyAllowedHost("*", "*://allowed.b.test");
  }

  // `IsActive()` should consider the blocked/allowed hosts policy.
  constexpr struct {
    const char* domain;
    bool expect_proxy_active;
  } kTestCases[] = {
      {"a.test", true},
      {"b.test", false},
      {"foo.b.test", false},
      {"allowed.b.test", true},
  };
  for (const auto& test : kTestCases) {
    SetTestDomainToNavigate(test.domain);
    auto origin =
        url::Origin::Create(GURL((base::StrCat({"https://", test.domain}))));
    SCOPED_TRACE(testing::Message() << "origin=" << origin);
    WebAuthenticationProxyService* proxy =
        WebAuthenticationProxyService::GetIfProxyAttached(profile());
    ASSERT_TRUE(proxy);
    EXPECT_EQ(proxy->IsActive(origin), test.expect_proxy_active);

    // If the proxy is active, the SW JS stubs IsUVPAA to return true.
    EXPECT_EQ(NavigateAndCallIsUVPAA(), test.expect_proxy_active);
  }
}

}  // namespace
}  // namespace extensions
