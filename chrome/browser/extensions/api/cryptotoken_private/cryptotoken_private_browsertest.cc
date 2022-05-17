// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_switches.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/with_feature_override.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/embedder_support/switches.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/test/permission_request_observer.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "device/fido/features.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/extension_features.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

constexpr char kCryptoTokenExtensionId[] = "kmendfapggjehodndflmmgagdbamhnfd";

// Origin for running tests with an Origin Trial token. Hostname needs to be
// from `net::EmbeddedTestServer::CERT_TEST_NAMES`.
constexpr char kOriginTrialOrigin[] = "https://a.test";

// Domain to serve files from. This should be different from `kOriginTrialToken`
// domain. Needs to be from `net::EmbeddedTestServer::CERT_TEST_NAMES`.
constexpr char kNonOriginTrialDomain[] = "b.test";

class CryptotokenBrowserTest : public base::test::WithFeatureOverride,
                               public InProcessBrowserTest {
 protected:
  CryptotokenBrowserTest()
      : base::test::WithFeatureOverride(
            extensions_features::kU2FSecurityKeyAPI) {
#if BUILDFLAG(IS_WIN)
    // Don't dispatch requests to the native Windows API.
    scoped_feature_list_.InitAndDisableFeature(device::kWebAuthUseNativeWinApi);
#endif
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // The public key for the default privatey key used by the
    // tools/origin_trials/generate_token.py tool.
    static constexpr char kOriginTrialTestPublicKey[] =
        "dRCs+TocuKkocNKa0AtZ4awrt9XKH2SQCI6o4FY6BNA=";
    command_line->AppendSwitchASCII(embedder_support::kOriginTrialPublicKey,
                                    kOriginTrialTestPublicKey);
  }

  void SetUp() override {
    // Make sure the Hangout Services component extension gets loaded.
    ComponentLoader::EnableBackgroundExtensionsForTesting();
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    url_loader_interceptor_ =
        std::make_unique<content::URLLoaderInterceptor>(base::BindRepeating(
            &CryptotokenBrowserTest::InterceptRequest, base::Unretained(this)));

    host_resolver()->AddRule("*", "127.0.0.1");

    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_.AddDefaultHandlers(GetChromeTestDataDir());
    ASSERT_TRUE(https_server_.Start());

    http_server_.AddDefaultHandlers(GetChromeTestDataDir());
    ASSERT_TRUE(http_server_.Start());
  }

  void TearDownOnMainThread() override { url_loader_interceptor_.reset(); }

  // Returns the frame to use when attempting to connect to Cryptotoken in the
  // methods below. Uses the main frame by default or
  // |frame_to_use_for_connecting_| if a test overrides it.
  content::RenderFrameHost* FrameToUseForConnecting() {
    return frame_to_use_for_connecting_ ? frame_to_use_for_connecting_.get()
                                        : browser()
                                              ->tab_strip_model()
                                              ->GetActiveWebContents()
                                              ->GetMainFrame();
  }

  void ExpectChromeRuntimeIsUndefined() {
    const std::string script = base::StringPrintf(
        R"(let port = chrome.runtime.connect('%s',
              {});)",
        kCryptoTokenExtensionId);
    const content::EvalJsResult result =
        content::EvalJs(FrameToUseForConnecting(), script);
    EXPECT_THAT(
        result.error,
        testing::StartsWith("a JavaScript error:\nTypeError: Cannot read "
                            "properties of undefined (reading 'connect')"));
  }

  void ExpectConnectSuccess() {
    const std::string script = base::StringPrintf(
        R"(new Promise((resolve) => {
          chrome.runtime.sendMessage('%s',
              {}, () => {
                resolve(chrome.runtime.lastError === undefined);
              });
      }))",
        kCryptoTokenExtensionId);
    const content::EvalJsResult result =
        content::EvalJs(FrameToUseForConnecting(), script);
    EXPECT_EQ(true, result);
  }

  void ExpectConnectFailure() {
    const std::string script = base::StringPrintf(
        R"(new Promise((resolve) => {
          chrome.runtime.sendMessage('%s',
              {}, () => {
                if (!chrome.runtime.lastError) {
                  resolve('chrome.runtime.lastError is undefined');
                } else {
                  resolve(chrome.runtime.lastError.message);
                }
              });
      }))",
        kCryptoTokenExtensionId);
    const content::EvalJsResult result =
        content::EvalJs(FrameToUseForConnecting(), script);
    EXPECT_EQ("Could not establish connection. Receiving end does not exist.",
              result);
  }

  // Indicates whether `ExpectSignSuccess()` should expect a U2F deprecation
  // prompt to be shown.
  enum class PromptExpectation {
    kNoPrompt,
    kShowPrompt,
  };

  std::string GenerateScriptRequestForAppId(const std::string& app_id) {
    return base::StringPrintf(
        R"(new Promise((resolve,reject) => {
          chrome.runtime.sendMessage('%s',
              {
                type: 'u2f_sign_request',
                appId: '%s',
                challenge: 'aGVsbG8gd29ybGQ',
                registeredKeys: [
                  {
                    version: 'U2F_V2',
                    keyHandle: 'aGVsbG8gd29ybGQ',
                    transports: ['usb'],
                    appId: '%s',
                  }
                ],
                timeoutSeconds: 3,
                requestId: 1
              },
              (args) => {
                  if (chrome.runtime.lastError !== undefined) {
                    resolve('runtime error: ' + chrome.runtime.lastError);
                    return;
                  }
                  if ('responseData' in args &&
                      'errorCode' in args.responseData) {
                    resolve('errorCode:'
                            + args.responseData.errorCode
                            + ",errorMessage:"
                            + args.responseData.errorMessage);
                    return;
                  }
                  reject(); // Requests can never succeed.
              });
      }))",
        kCryptoTokenExtensionId, app_id.c_str(), app_id.c_str());
  }

  void ExpectSignSuccess(const std::string& app_id,
                         PromptExpectation prompt_expectation) {
    content::WebContents* web_contents =
        content::WebContents::FromRenderFrameHost(FrameToUseForConnecting());
    if (prompt_expectation == PromptExpectation::kShowPrompt) {
      // Automatically resolve permission prompts shown by Cryptotoken on the
      // target frame.
      permissions::PermissionRequestManager* request_manager =
          permissions::PermissionRequestManager::FromWebContents(web_contents);
      request_manager->set_auto_response_for_test(
          permissions::PermissionRequestManager::DENY_ALL);
    }

    permissions::PermissionRequestObserver permission_request_observer(
        web_contents);
    const std::string script = GenerateScriptRequestForAppId(app_id);
    const content::EvalJsResult result =
        content::EvalJs(FrameToUseForConnecting(), script);
    if (prompt_expectation == PromptExpectation::kShowPrompt) {
      // Denied prompt results in a DEVICE_INELIGIBLE error.
      EXPECT_EQ("errorCode:4,errorMessage:The operation was not allowed",
                result);
      EXPECT_EQ(true, permission_request_observer.request_shown());
    } else {
      // Without a prompt, the request times out eventually. N.B. the
      // `timeoutSeconds` parameter above needs to be high enough to allow time
      // for the prompt to show in the kShowPrompt case, but not too high to
      // cause test stalls and timeouts
      EXPECT_EQ("errorCode:5,errorMessage:undefined", result);
      EXPECT_EQ(false, permission_request_observer.request_shown());
    }
  }

  net::EmbeddedTestServer http_server_{net::EmbeddedTestServer::TYPE_HTTP};
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
  raw_ptr<content::RenderFrameHost> frame_to_use_for_connecting_ = nullptr;

 private:
  // content::URLLoaderInterceptor callback
  bool InterceptRequest(content::URLLoaderInterceptor::RequestParams* params) {
    // The response for origin trial requests are injected so that we have a
    // stable port to use for generating the token.
    if (params->url_request.url.DeprecatedGetOriginAsURL() !=
        GURL(kOriginTrialOrigin)) {
      return false;
    }

    // Generated with `tools/origin_trials/generate_token.py --expire-days 5000
    // https://a.test U2FSecurityKeyAPI`
    constexpr char kOriginTrialToken[] =
        "A5xaE9lySzMBejK4rKNqBC86X9m2VhMwB/"
        "1FXWcHNhtPhdTK02TzChhjmD7p+"
        "kMn2tTO424RwoUWlFCLsd1tRAQAAABWeyJvcmlnaW4iOiAiaHR0cHM6Ly9hLnRlc3Q6NDQ"
        "zIiwgImZlYXR1cmUiOiAiVTJGU2VjdXJpdHlLZXlBUEkiLCAiZXhwaXJ5IjogMjA2MjcxM"
        "DYzNH0=";

    const std::string headers = base::StringPrintf(
        "HTTP/1.1 200 OK\nContent-Type: text/html; charset=utf-8\n"
        "Origin-Trial: %s\n\n",
        kOriginTrialToken);
    content::URLLoaderInterceptor::WriteResponse(
        headers,
        "<html><head></head><body>OK\n"
        "<iframe id=\"otIframe\"></iframe>"
        "</body></html>",
        params->client.get());
    return true;
  }

#if BUILDFLAG(IS_WIN)
  base::test::ScopedFeatureList scoped_feature_list_;
#endif
  std::unique_ptr<content::URLLoaderInterceptor> url_loader_interceptor_;
};

IN_PROC_BROWSER_TEST_P(CryptotokenBrowserTest, Connect) {
  // CryptoToken can only be connected to if the feature flag is enabled.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL(kNonOriginTrialDomain, "/empty.html")));
  if (IsParamFeatureEnabled()) {
    ExpectConnectSuccess();
  } else {
    ExpectConnectFailure();
  }
}

IN_PROC_BROWSER_TEST_P(CryptotokenBrowserTest, SignShowsPrompt) {
  if (!IsParamFeatureEnabled()) {
    // Can't connect with the API disabled.
    return;
  }
  GURL url = https_server_.GetURL(kNonOriginTrialDomain, "/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  std::string app_id = url::Origin::Create(url).Serialize();
  ExpectSignSuccess(app_id, PromptExpectation::kShowPrompt);
}

IN_PROC_BROWSER_TEST_P(CryptotokenBrowserTest, ConnectWithOriginTrial) {
  // Connection succeeds regardless of feature flag state with the origin trial.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(kOriginTrialOrigin)));
  ExpectConnectSuccess();
}

IN_PROC_BROWSER_TEST_P(CryptotokenBrowserTest,
                       SignWithOriginTrialDoesNotShowPrompt) {
  GURL url = GURL(kOriginTrialOrigin);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  std::string app_id = url::Origin::Create(url).Serialize();
  ExpectSignSuccess(app_id, PromptExpectation::kNoPrompt);
}

IN_PROC_BROWSER_TEST_P(CryptotokenBrowserTest,
                       ConnectWithOriginTrialInCrossOriginIframe) {
  // Cross-origin iframe can connect with a trial token even if the parent frame
  // is not enrolled in the trial.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL(kNonOriginTrialDomain, "/iframe.html")));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(content::NavigateIframeToURL(web_contents, "test",
                                           GURL(kOriginTrialOrigin)));
  frame_to_use_for_connecting_ = content::FrameMatchingPredicate(
      web_contents->GetPrimaryPage(),
      base::BindRepeating(&content::FrameIsChildOfMainFrame));
  ExpectConnectSuccess();
}

IN_PROC_BROWSER_TEST_P(
    CryptotokenBrowserTest,
    SignWithOriginTrialInCrossOriginIframeDoesNotShowPrompt) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL(kNonOriginTrialDomain, "/iframe.html")));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL url = GURL(kOriginTrialOrigin);
  ASSERT_TRUE(content::NavigateIframeToURL(web_contents, "test", url));
  frame_to_use_for_connecting_ = content::FrameMatchingPredicate(
      web_contents->GetPrimaryPage(),
      base::BindRepeating(&content::FrameIsChildOfMainFrame));

  // Also make sure no permissions are showing on the main frame either.
  permissions::PermissionRequestManager* request_manager =
      permissions::PermissionRequestManager::FromWebContents(web_contents);
  request_manager->set_auto_response_for_test(
      permissions::PermissionRequestManager::DENY_ALL);
  permissions::PermissionRequestObserver permission_request_observer(
      web_contents);

  std::string app_id = url::Origin::Create(url).Serialize();
  ExpectSignSuccess(app_id, PromptExpectation::kNoPrompt);
  EXPECT_FALSE(permission_request_observer.request_shown());
}

IN_PROC_BROWSER_TEST_P(CryptotokenBrowserTest,
                       OriginTrialDoesNotAffectChildIframes) {
  GURL parent_url = GURL(kOriginTrialOrigin);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), parent_url));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL child_url = https_server_.GetURL(kNonOriginTrialDomain, "/empty.html");
  ASSERT_TRUE(
      content::NavigateIframeToURL(web_contents, "otIframe", child_url));
  frame_to_use_for_connecting_ = content::FrameMatchingPredicate(
      web_contents->GetPrimaryPage(),
      base::BindRepeating(&content::FrameIsChildOfMainFrame));

  if (IsParamFeatureEnabled()) {
    // With the feature flag enabled, the U2F API is active; but the OT that
    // would suppress the prompt on the main frame, does not suppress the prompt
    // on the child frame.
    ExpectConnectSuccess();
    std::string app_id = url::Origin::Create(child_url).Serialize();
    ExpectSignSuccess(app_id, PromptExpectation::kShowPrompt);
  } else {
    // With the feature flag off, the U2F API is inactive on the child frame,
    // even though it is enabled via OT on the parent frame.
    ExpectConnectFailure();
  }
}

IN_PROC_BROWSER_TEST_P(CryptotokenBrowserTest, ConnectWithEnterprisePolicy) {
  // Connection succeeds regardless of feature flag state with the enterprise
  // policy overriding deprecation changes.
  browser()->profile()->GetPrefs()->Set(
      extensions::pref_names::kU2fSecurityKeyApiEnabled, base::Value(true));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL(kNonOriginTrialDomain, "/empty.html")));
  ExpectConnectSuccess();
}

IN_PROC_BROWSER_TEST_P(CryptotokenBrowserTest,
                       SignWithEnterprisePolicyDoesNotShowPrompt) {
  browser()->profile()->GetPrefs()->Set(
      extensions::pref_names::kU2fSecurityKeyApiEnabled, base::Value(true));
  GURL url = GURL(https_server_.GetURL(kNonOriginTrialDomain, "/empty.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  std::string app_id = url::Origin::Create(url).Serialize();
  ExpectSignSuccess(app_id, PromptExpectation::kNoPrompt);
}

IN_PROC_BROWSER_TEST_P(CryptotokenBrowserTest, InsecureOriginCannotConnect) {
  // Connections from insecure origins always fail.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), http_server_.GetURL(kNonOriginTrialDomain, "/empty.html")));
  ExpectChromeRuntimeIsUndefined();
}

// Verify that a page with an origin that is not deriveable from its URL, in
// this case because it uses a CSP sandbox, does not pass appid check.
IN_PROC_BROWSER_TEST_P(CryptotokenBrowserTest, SandboxedPageDoesNotSign) {
  if (!IsParamFeatureEnabled()) {
    // Can't connect with the API disabled.
    return;
  }
  GURL url = https_server_.GetURL(kNonOriginTrialDomain,
                                  "/cryptotoken/csp-sandbox.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  std::string app_id = url::Origin::Create(url).Serialize();

  const std::string script = GenerateScriptRequestForAppId(app_id);
  const content::EvalJsResult result =
      content::EvalJs(FrameToUseForConnecting(), script);
  EXPECT_EQ("errorCode:2,errorMessage:undefined", result);
}

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(CryptotokenBrowserTest);

}  // namespace
}  // namespace extensions
