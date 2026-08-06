// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/with_feature_override.h"
#include "build/build_config.h"
#include "chrome/test/payments/payment_app_install_util.h"
#include "chrome/test/payments/payment_request_platform_browsertest_base.h"
#include "components/payments/core/features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/url_loader_monitor.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/public/cpp/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/blink/public/common/features_generated.h"
#include "url/gurl.h"

using testing::AllOf;
using testing::HasSubstr;

namespace payments {
namespace {

using IconInstall = test::PaymentAppInstallUtil::IconInstall;

constexpr char kDefaultPaymentPageContent[] = R"(
  <!DOCTYPE html>
  <html>
    <head>
      <script src="util.js"></script>
      <script src="can_make_payment_query.js"></script>
      <title>Payment Request Connection Allowlist Test</title>
      <meta charset="utf-8">
    </head>
    <body>
      <pre id="result"></pre>
    </body>
  </html>
)";

constexpr char kOpenWindowMerchantPage[] = R"(
  <!DOCTYPE html>
  <html>
    <head>
      <title>Payment Request Open Window Test</title>
      <meta charset="utf-8">
    </head>
    <body>
      <script>
        async function testOpenWindow(methodName, url) {
          try {
            const request = new PaymentRequest(
                [{supportedMethods: methodName, data: {url}}],
                {total: {label: 'Total',
                         amount: {currency: 'USD', value: '0.01'}}});
            const response = await request.show();
            await response.complete('success');
            return response.details.error || 'success';
          } catch (e) {
            return e.message;
          }
        }
      </script>
    </body>
  </html>
)";

constexpr char kOpenWindowServiceWorkerScript[] = R"(
  self.addEventListener('canmakepayment', (evt) => {
    evt.respondWith(true);
  });
  self.addEventListener('paymentrequest', (evt) => {
    const url = evt.methodData[0].data.url;
    const methodName = evt.methodData[0].supportedMethods;
    evt.respondWith((async () => {
      try {
        const windowClient = await evt.openWindow(url);
        if (windowClient) {
          return {methodName, details: {}};
        } else {
          return {methodName, details: {error: 'open_window_failed'}};
        }
      } catch (error) {
        return {methodName, details: {error: error.message}};
      }
    })());
  });
)";
struct ResponseEntry {
  std::string content;
  absl::flat_hash_map<std::string, std::string> headers;
  net::HttpStatusCode code = net::HTTP_OK;
};

// Tests requests from `PaymentManifestDownloader` are checked against the
// initiator frame's connection allowlist.
//
// Note: `PaymentManifestDownloader` has two url loader factories:
// - The url loader factory associated with the initiator frame. It has the
// frame's network restriction id and checks the request url. It is used for the
// initial HEAD request when downloading a payment method manifest if the
// `PaymentRequestUseRendererUrlLoader` feature is enabled.
// - The url loader factory for the browser process. It does not have the
// frame's network restriction id. The connection allowlist check is done inside
// the `PaymentManifestDownloader`, before it reaches the url loader factory. It
// is used for:
//   1. Initial HEAD request if the `PaymentRequestUseRendererUrlLoader` feature
//      is disabled.
//   2. Following redirects for the initial payment method manifest request.
//   3. The subsequent GET request to download the actual payment method
//   manifest content after the Link header has been resolved.
//   4. Downloading web app manifests.
class PaymentRequestConnectionAllowlistBrowserTest
    : public base::test::WithFeatureOverride,
      public PaymentRequestPlatformBrowserTestBase {
 protected:
  PaymentRequestConnectionAllowlistBrowserTest()
      : base::test::WithFeatureOverride(
            features::kPaymentRequestUseRendererUrlLoader) {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {network::features::kConnectionAllowlists,
         features::kAllowJITInstallationWhenAppIconIsMissing},
        /*disabled_features=*/{});
    SetBypassUserInteractionForTesting();
  }

  ~PaymentRequestConnectionAllowlistBrowserTest() override = default;

  void SetUpOnMainThread() override {
    https_server()->RegisterRequestHandler(base::BindRepeating(
        &PaymentRequestConnectionAllowlistBrowserTest::ServeResponses,
        base::Unretained(this)));
    PaymentRequestPlatformBrowserTestBase::SetUpOnMainThread();
  }

  void RegisterResponse(const std::string& relative_url,
                        ResponseEntry&& entry) {
    response_map_[relative_url] = std::move(entry);
  }

  void ExpectCanMakePayment(bool expected, const GURL& url) {
    EXPECT_EQ(expected, content::EvalJs(GetActiveWebContents(),
                                        content::JsReplace(
                                            "checkCanMakePayment($1)", url)));
  }

 private:
  std::unique_ptr<net::test_server::HttpResponse> ServeResponses(
      const net::test_server::HttpRequest& request) {
    auto it = response_map_.find(request.relative_url);
    if (it == response_map_.end()) {
      it = response_map_.find(request.GetURL().path());
    }
    if (it != response_map_.end()) {
      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_code(it->second.code);
      response->set_content(it->second.content);
      for (const auto& [key, value] : it->second.headers) {
        if (key == "Content-Type") {
          response->set_content_type(value);
        } else {
          response->AddCustomHeader(key, value);
        }
      }
      return response;
    }

    return nullptr;
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  absl::flat_hash_map<std::string, ResponseEntry> response_map_;
};

// Test that cross-origin payment manifest download from "b.com" is allowed when
// Connection-Allowlist is set to (response-origin "*://b.com:*/*").
IN_PROC_BROWSER_TEST_P(PaymentRequestConnectionAllowlistBrowserTest,
                       HeadRequestAllowed) {
  RegisterResponse("/payment_request_connection_allowlist_cross_origin.html",
                   ResponseEntry(kDefaultPaymentPageContent,
                                 {{"Connection-Allowlist",
                                   R"((response-origin "*://b.com:*/*"))"}}));
  RegisterResponse(
      "/nickpay.test/pay",
      ResponseEntry(
          R"({ "default_applications": ["app.json"] })",
          {{"Content-Type", "application/json"},
           {"Link", "</nickpay.test/pay>; rel=\"payment-method-manifest\""}}));

  content::URLLoaderMonitor monitor;
  content::WebContentsConsoleObserver console_observer(GetActiveWebContents());
  console_observer.SetPattern("*ERR_NETWORK_ACCESS_REVOKED*");
  NavigateTo("a.com",
             "/payment_request_connection_allowlist_cross_origin.html");

  GURL payment_url = https_server()->GetURL("b.com", "/nickpay.test/pay");
  ExpectCanMakePayment(true, payment_url);

  EXPECT_TRUE(console_observer.messages().empty());
  monitor.WaitForUrls({payment_url});
  EXPECT_EQ(monitor.WaitForRequestCompletion(payment_url).error_code, net::OK);
}

// Test that cross-origin payment manifest download from "b.com" is blocked when
// Connection-Allowlist is set to (response-origin).
IN_PROC_BROWSER_TEST_P(PaymentRequestConnectionAllowlistBrowserTest,
                       HeadRequestBlocked) {
  RegisterResponse(
      "/payment_request_connection_allowlist_same_origin.html",
      ResponseEntry(kDefaultPaymentPageContent,
                    {{"Connection-Allowlist", "(response-origin)"}}));
  RegisterResponse(
      "/nickpay.test/pay",
      ResponseEntry(
          R"({ "default_applications": ["app.json"] })",
          {{"Content-Type", "application/json"},
           {"Link", "</nickpay.test/pay>; rel=\"payment-method-manifest\""}}));

  content::URLLoaderMonitor monitor;
  content::WebContentsConsoleObserver console_observer(GetActiveWebContents());
  console_observer.SetPattern("*ERR_NETWORK_ACCESS_REVOKED*");

  NavigateTo("a.com", "/payment_request_connection_allowlist_same_origin.html");

  GURL payment_url = https_server()->GetURL("b.com", "/nickpay.test/pay");
  ExpectCanMakePayment(false, payment_url);

  ASSERT_TRUE(console_observer.Wait());
  EXPECT_THAT(
      console_observer.GetMessageAt(0u),
      AllOf(HasSubstr("Unable to download payment manifest"),
            HasSubstr("ERR_NETWORK_ACCESS_REVOKED"), HasSubstr("(-33)")));
  if (IsParamFeatureEnabled()) {
    // When feature `PaymentRequestUseRendererUrlLoader` is enabled, the request
    // went through the url loader factory associated with the initiator render
    // frame host. The connection allowlist check by the url loader factory
    // blocks the request and fails it with the net error code.
    monitor.WaitForUrls({payment_url});
    EXPECT_EQ(monitor.WaitForRequestCompletion(payment_url).error_code,
              net::ERR_NETWORK_ACCESS_REVOKED);
  } else {
    // Otherwise, the request would have gone through the url loader factory
    // for the browser process. That url loader factory does not have access to
    // the connection allowlist. The connection allowlist check is done in
    // `PaymentManifestDownloader`. It blocks the request before it reaches the
    // url loader factory.
    EXPECT_FALSE(monitor.GetRequestInfo(payment_url).has_value());
  }
}

// Test that payment manifest download from "a.com", which is redirected to
// "sub.a.com" is allowed when Connection-Allowlist is set to (response-origin
// "*://sub.a.com:*/*") and `redirects=allow`.
IN_PROC_BROWSER_TEST_P(PaymentRequestConnectionAllowlistBrowserTest,
                       HeadRequestRedirectAllowed) {
  GURL target_url = https_server()->GetURL("sub.a.com", "/nickpay.test/pay");
  GURL redirect_url =
      https_server()->GetURL("a.com", "/server-redirect?" + target_url.spec());

  RegisterResponse(
      "/payment_request_connection_allowlist_redirects_allow.html",
      ResponseEntry(
          kDefaultPaymentPageContent,
          {{"Connection-Allowlist",
            R"((response-origin "*://sub.a.com:*/*"); redirects=allow)"}}));
  RegisterResponse(
      "/nickpay.test/pay",
      ResponseEntry(
          R"({ "default_applications": ["app.json"] })",
          {{"Content-Type", "application/json"},
           {"Link", "</nickpay.test/pay>; rel=\"payment-method-manifest\""}}));
  RegisterResponse(
      redirect_url.PathForRequest(),
      ResponseEntry("", {{"Location", target_url.spec()}}, net::HTTP_FOUND));

  content::URLLoaderMonitor monitor;
  content::WebContentsConsoleObserver console_observer(GetActiveWebContents());
  console_observer.SetPattern("*ERR_UNSAFE_REDIRECT*");

  NavigateTo("a.com",
             "/payment_request_connection_allowlist_redirects_allow.html");

  ExpectCanMakePayment(true, redirect_url);

  EXPECT_TRUE(console_observer.messages().empty());
  monitor.WaitForUrls({target_url});
  EXPECT_EQ(monitor.WaitForRequestCompletion(target_url).error_code, net::OK);
}

// Test that payment manifest download from "a.com", which is redirected to
// "sub.a.com" is blocked when Connection-Allowlist is set to (response-origin
// "*://sub.a.com:*/*") and `redirects=block`.
IN_PROC_BROWSER_TEST_P(PaymentRequestConnectionAllowlistBrowserTest,
                       HeadRequestRedirectBlocked) {
  GURL target_url = https_server()->GetURL("sub.a.com", "/nickpay.test/pay");
  GURL redirect_url =
      https_server()->GetURL("a.com", "/server-redirect?" + target_url.spec());

  RegisterResponse(
      "/payment_request_connection_allowlist_redirects_block.html",
      ResponseEntry(
          kDefaultPaymentPageContent,
          {{"Connection-Allowlist",
            R"((response-origin "*://sub.a.com:*/*"); redirects=block)"}}));
  RegisterResponse(
      "/nickpay.test/pay",
      ResponseEntry(
          R"({ "default_applications": ["app.json"] })",
          {{"Content-Type", "application/json"},
           {"Link", "</nickpay.test/pay>; rel=\"payment-method-manifest\""}}));
  RegisterResponse(
      redirect_url.PathForRequest(),
      ResponseEntry("", {{"Location", target_url.spec()}}, net::HTTP_FOUND));

  content::URLLoaderMonitor monitor;
  content::WebContentsConsoleObserver console_observer(GetActiveWebContents());
  console_observer.SetPattern("*ERR_UNSAFE_REDIRECT*");

  NavigateTo("a.com",
             "/payment_request_connection_allowlist_redirects_block.html");

  ExpectCanMakePayment(false, redirect_url);

  ASSERT_TRUE(console_observer.Wait());
  EXPECT_THAT(console_observer.GetMessageAt(0u),
              AllOf(HasSubstr("Unable to download payment manifest"),
                    HasSubstr("ERR_UNSAFE_REDIRECT"), HasSubstr("(-311)")));
  monitor.WaitForUrls({redirect_url});

  // When feature `PaymentRequestUseRendererUrlLoader` is enabled, the initial
  // HEAD request went through the url loader factory associated with the
  // initiator render frame host, which has the network restrictions ID. The
  // network service connection allowlist check blocks the redirect and fails
  // the request with `net::ERR_UNSAFE_REDIRECT`.
  // Otherwise, the url loader factory for browser process is used, and the
  // redirect is blocked by the connection allowlist check in
  // `PaymentManifestDownloader`. The request is cancelled, which destroys the
  // loader. Then the loader cancels the request and results in
  // `net::ERR_ABORTED`.
  EXPECT_EQ(
      monitor.WaitForRequestCompletion(redirect_url).error_code,
      IsParamFeatureEnabled() ? net::ERR_UNSAFE_REDIRECT : net::ERR_ABORTED);
}

// Test that payment method manifest download is allowed when both the initial
// HEAD request and subsequent GET requests are allowed by the connection
// allowlist.
IN_PROC_BROWSER_TEST_P(PaymentRequestConnectionAllowlistBrowserTest,
                       PaymentMethodManifestAllowed) {
  RegisterResponse(
      "/payment_request_connection_allowlist_manifest_allowed.html",
      ResponseEntry(kDefaultPaymentPageContent, {{"Connection-Allowlist", R"(
            (
              response-origin
              "*://b.com:*/manifest-with-link"
              "*://b.com:*/nickpay.test/pay"
              "*://b.com:*/nickpay.test/app.json"
            )
          )"}}));
  RegisterResponse(
      "/manifest-with-link",
      ResponseEntry(
          "",
          {{"Link", "</nickpay.test/pay>; rel=\"payment-method-manifest\""}}));
  RegisterResponse(
      "/nickpay.test/pay",
      ResponseEntry(
          R"({ "default_applications": ["app.json"] })",
          {{"Content-Type", "application/json"},
           {"Link", "</nickpay.test/pay>; rel=\"payment-method-manifest\""}}));
  RegisterResponse("/nickpay.test/app.json",
                   ResponseEntry(
                       R"({
            "name": "Nick Pay",
            "icons": [{
              "src": "icon.png",
              "sizes": "40x40",
              "type": "image/png"
            }],
            "serviceworker": {
              "src": "app.js"
            }
          })",
                       {{"Content-Type", "application/json"}}));

  content::URLLoaderMonitor monitor;
  content::WebContentsConsoleObserver console_observer(GetActiveWebContents());
  console_observer.SetPattern("*ERR_NETWORK_ACCESS_REVOKED*");

  NavigateTo("a.com",
             "/payment_request_connection_allowlist_manifest_allowed.html");

  GURL method_url = https_server()->GetURL("b.com", "/manifest-with-link");
  GURL manifest_url = https_server()->GetURL("b.com", "/nickpay.test/pay");
  GURL app_manifest_url =
      https_server()->GetURL("b.com", "/nickpay.test/app.json");

  // canMakePayment should return true because all manifests are successfully
  // downloaded.
  ExpectCanMakePayment(true, method_url);

  EXPECT_TRUE(console_observer.messages().empty());
  monitor.WaitForUrls({method_url, manifest_url, app_manifest_url});
  EXPECT_EQ(monitor.WaitForRequestCompletion(method_url).error_code, net::OK);
  EXPECT_EQ(monitor.WaitForRequestCompletion(manifest_url).error_code, net::OK);
  EXPECT_EQ(monitor.WaitForRequestCompletion(app_manifest_url).error_code,
            net::OK);
}

// Test that payment method manifest download is blocked when the subsequent
// GET request is not allowed by the connection allowlist. Only the initial HEAD
// request is allowed.
IN_PROC_BROWSER_TEST_P(PaymentRequestConnectionAllowlistBrowserTest,
                       PaymentMethodManifestBlocked) {
  RegisterResponse(
      "/payment_request_connection_allowlist_manifest_blocked.html",
      ResponseEntry(
          kDefaultPaymentPageContent,
          {{"Connection-Allowlist",
            R"((response-origin "*://b.com:*/manifest-with-link"))"}}));
  RegisterResponse(
      "/manifest-with-link",
      ResponseEntry(
          "",
          {{"Link", "</nickpay.test/pay>; rel=\"payment-method-manifest\""}}));
  RegisterResponse(
      "/nickpay.test/pay",
      ResponseEntry(
          R"({ "default_applications": ["app.json"] })",
          {{"Content-Type", "application/json"},
           {"Link", "</nickpay.test/pay>; rel=\"payment-method-manifest\""}}));

  content::URLLoaderMonitor monitor;
  content::WebContentsConsoleObserver console_observer(GetActiveWebContents());
  console_observer.SetPattern("*ERR_NETWORK_ACCESS_REVOKED*");

  NavigateTo("a.com",
             "/payment_request_connection_allowlist_manifest_blocked.html");

  GURL method_url = https_server()->GetURL("b.com", "/manifest-with-link");
  GURL manifest_url = https_server()->GetURL("b.com", "/nickpay.test/pay");

  // canMakePayment should return false because subsequent manifest download is
  // blocked.
  ExpectCanMakePayment(false, method_url);

  ASSERT_TRUE(console_observer.Wait());
  EXPECT_THAT(
      console_observer.GetMessageAt(0u),
      AllOf(HasSubstr("Unable to download payment manifest"),
            HasSubstr("ERR_NETWORK_ACCESS_REVOKED"), HasSubstr("(-33)")));

  monitor.WaitForUrls({method_url});
  EXPECT_EQ(monitor.WaitForRequestCompletion(method_url).error_code, net::OK);

  // Payment manifest request is through the url loader factory for the browser
  // process, which does not have the network restriction id and does not check
  // the connection allowlist. The connection allowlist is checked in
  // `PaymentManifestDownloader::InitiateDownload`, which blocks the request
  // before it reaches the url loader factory. So the url loader monitor will
  // not observe the request.
  EXPECT_FALSE(monitor.GetRequestInfo(manifest_url).has_value());
}

// Test that web app manifest download is allowed when both the payment method
// manifest and web app manifest are allowed by the connection allowlist.
IN_PROC_BROWSER_TEST_P(PaymentRequestConnectionAllowlistBrowserTest,
                       WebAppManifestAllowed) {
  RegisterResponse(
      "/payment_request_connection_allowlist_webapp_allowed.html",
      ResponseEntry(kDefaultPaymentPageContent, {{"Connection-Allowlist", R"(
            (
              response-origin
              "*://b.com:*/connection_allowlist_jit/method-manifest.json"
              "*://b.com:*/connection_allowlist_jit/app-manifest.json")
            )"}}));
  RegisterResponse(
      "/connection_allowlist_jit/method-manifest.json",
      ResponseEntry(
          R"({ "default_applications": ["app-manifest.json"] })",
          {{"Content-Type", "application/json"},
           {"Access-Control-Allow-Origin", "*"},
           {"Link",
            "<method-manifest.json>; rel=\"payment-method-manifest\""}}));
  RegisterResponse("/connection_allowlist_jit/app-manifest.json",
                   ResponseEntry(
                       R"({
            "name": "JIT Test Pay",
            "short_name": "Test",
            "serviceworker": {
              "src": "sw.js",
              "scope": "/connection_allowlist_jit/"
            }
          })",
                       {{"Content-Type", "application/json"},
                        {"Access-Control-Allow-Origin", "*"}}));

  content::URLLoaderMonitor monitor;
  content::WebContentsConsoleObserver console_observer(GetActiveWebContents());
  console_observer.SetPattern("*ERR_NETWORK_ACCESS_REVOKED*");

  NavigateTo("a.com",
             "/payment_request_connection_allowlist_webapp_allowed.html");

  GURL method_url = https_server()->GetURL(
      "b.com", "/connection_allowlist_jit/method-manifest.json");
  GURL app_manifest_url = https_server()->GetURL(
      "b.com", "/connection_allowlist_jit/app-manifest.json");

  // canMakePayment should return true because all manifests are successfully
  // downloaded.
  ExpectCanMakePayment(true, method_url);

  EXPECT_TRUE(console_observer.messages().empty());
  monitor.WaitForUrls({method_url, app_manifest_url});
  EXPECT_EQ(monitor.WaitForRequestCompletion(method_url).error_code, net::OK);
  EXPECT_EQ(monitor.WaitForRequestCompletion(app_manifest_url).error_code,
            net::OK);
}

// Test that web app manifest download is blocked when its URL is not allowed by
// the connection allowlist. Only the payment method manifest is allowed.
IN_PROC_BROWSER_TEST_P(PaymentRequestConnectionAllowlistBrowserTest,
                       WebAppManifestBlocked) {
  RegisterResponse(
      "/payment_request_connection_allowlist_webapp_blocked.html",
      ResponseEntry(kDefaultPaymentPageContent, {{"Connection-Allowlist", R"(
            (
              response-origin
              "*://b.com:*/connection_allowlist_jit/method-manifest.json"
            )
          )"}}));
  RegisterResponse(
      "/connection_allowlist_jit/method-manifest.json",
      ResponseEntry(
          R"({ "default_applications": ["app-manifest.json"] })",
          {{"Content-Type", "application/json"},
           {"Access-Control-Allow-Origin", "*"},
           {"Link",
            "<method-manifest.json>; rel=\"payment-method-manifest\""}}));
  RegisterResponse("/connection_allowlist_jit/app-manifest.json",
                   ResponseEntry(
                       R"({
            "name": "JIT Test Pay",
            "short_name": "Test",
            "serviceworker": {
              "src": "sw.js",
              "scope": "/connection_allowlist_jit/"
            }
          })",
                       {{"Content-Type", "application/json"},
                        {"Access-Control-Allow-Origin", "*"}}));

  content::URLLoaderMonitor monitor;
  content::WebContentsConsoleObserver console_observer(GetActiveWebContents());
  console_observer.SetPattern("*ERR_NETWORK_ACCESS_REVOKED*");

  NavigateTo("a.com",
             "/payment_request_connection_allowlist_webapp_blocked.html");

  GURL method_url = https_server()->GetURL(
      "b.com", "/connection_allowlist_jit/method-manifest.json");
  GURL app_manifest_url = https_server()->GetURL(
      "b.com", "/connection_allowlist_jit/app-manifest.json");

  // canMakePayment should return false because web app manifest download is
  // blocked.
  ExpectCanMakePayment(false, method_url);

  ASSERT_TRUE(console_observer.Wait());
  EXPECT_THAT(
      console_observer.GetMessageAt(0u),
      AllOf(HasSubstr("Unable to download payment manifest"),
            HasSubstr("ERR_NETWORK_ACCESS_REVOKED"), HasSubstr("(-33)")));

  monitor.WaitForUrls({method_url});
  EXPECT_EQ(monitor.WaitForRequestCompletion(method_url).error_code, net::OK);

  // Web app manifest request is through the url loader factory for the browser
  // process, which does not have the network restriction id and does not check
  // the connection allowlist. The connection allowlist is checked in
  // `PaymentManifestDownloader::InitiateDownload`, which blocks the request
  // before it reaches the url loader factory. So the url loader monitor will
  // not observe the request.
  EXPECT_FALSE(monitor.GetRequestInfo(app_manifest_url).has_value());
}

// Test that payment icon download is allowed when the URL is allowed by the
// connection allowlist.
IN_PROC_BROWSER_TEST_P(PaymentRequestConnectionAllowlistBrowserTest,
                       PaymentIconAllowed) {
  RegisterResponse(
      "/payment_request_connection_allowlist_icon_allowed.html",
      ResponseEntry(kDefaultPaymentPageContent, {{"Connection-Allowlist", R"(
            (
              response-origin
              "*://b.com:*/manifest-with-link"
              "*://b.com:*/nickpay.test/pay"
              "*://b.com:*/nickpay.test/app.json"
              "*://b.com:*/nickpay.test/icon.png"
            )
          )"}}));
  RegisterResponse(
      "/manifest-with-link",
      ResponseEntry(
          "",
          {{"Link", "</nickpay.test/pay>; rel=\"payment-method-manifest\""}}));
  RegisterResponse(
      "/nickpay.test/pay",
      ResponseEntry(
          R"({ "default_applications": ["app.json"] })",
          {{"Content-Type", "application/json"},
           {"Link", "</nickpay.test/pay>; rel=\"payment-method-manifest\""}}));
  RegisterResponse("/nickpay.test/app.json",
                   ResponseEntry(
                       R"({
                          "name": "Nick Pay",
                          "icons": [{
                            "src": "icon.png",
                            "sizes": "40x40",
                            "type": "image/png"
                          }],
                          "serviceworker": {
                            "src": "app.js"
                          }
                       })",
                       {{"Content-Type", "application/json"}}));
  RegisterResponse(
      "/nickpay.test/icon.png",
      ResponseEntry("icon_content", {{"Content-Type", "image/png"}}));

  content::URLLoaderMonitor monitor;
  content::WebContentsConsoleObserver console_observer(GetActiveWebContents());
  console_observer.SetPattern("*ERR_NETWORK_ACCESS_REVOKED*");

  NavigateTo("a.com",
             "/payment_request_connection_allowlist_icon_allowed.html");

  GURL method_url = https_server()->GetURL("b.com", "/manifest-with-link");
  GURL manifest_url = https_server()->GetURL("b.com", "/nickpay.test/pay");
  GURL app_manifest_url =
      https_server()->GetURL("b.com", "/nickpay.test/app.json");
  GURL icon_url = https_server()->GetURL("b.com", "/nickpay.test/icon.png");

  ExpectCanMakePayment(true, method_url);

  EXPECT_TRUE(console_observer.messages().empty());
  monitor.WaitForUrls({method_url, manifest_url, app_manifest_url, icon_url});
  EXPECT_EQ(monitor.WaitForRequestCompletion(method_url).error_code, net::OK);
  EXPECT_EQ(monitor.WaitForRequestCompletion(manifest_url).error_code, net::OK);
  EXPECT_EQ(monitor.WaitForRequestCompletion(app_manifest_url).error_code,
            net::OK);
  EXPECT_EQ(monitor.WaitForRequestCompletion(icon_url).error_code, net::OK);
}

// Test that payment icon download is blocked when the URL is not allowed by the
// connection allowlist.
IN_PROC_BROWSER_TEST_P(PaymentRequestConnectionAllowlistBrowserTest,
                       PaymentIconBlocked) {
  RegisterResponse(
      "/payment_request_connection_allowlist_icon_blocked.html",
      ResponseEntry(kDefaultPaymentPageContent, {{"Connection-Allowlist", R"(
            (
              response-origin
              "*://b.com:*/manifest-with-link"
              "*://b.com:*/nickpay.test/pay"
              "*://b.com:*/nickpay.test/app.json"
            )
          )"}}));
  RegisterResponse(
      "/manifest-with-link",
      ResponseEntry(
          "",
          {{"Link", "</nickpay.test/pay>; rel=\"payment-method-manifest\""}}));
  RegisterResponse(
      "/nickpay.test/pay",
      ResponseEntry(
          R"({ "default_applications": ["app.json"] })",
          {{"Content-Type", "application/json"},
           {"Link", "</nickpay.test/pay>; rel=\"payment-method-manifest\""}}));
  RegisterResponse("/nickpay.test/app.json",
                   ResponseEntry(
                       R"({
                          "name": "Nick Pay",
                          "icons": [{
                            "src": "icon.png",
                            "sizes": "40x40",
                            "type": "image/png"
                          }],
                          "serviceworker": {
                            "src": "app.js"
                          }
                        })",
                       {{"Content-Type", "application/json"}}));
  RegisterResponse(
      "/nickpay.test/icon.png",
      ResponseEntry("icon_content", {{"Content-Type", "image/png"}}));

  content::URLLoaderMonitor monitor;

  NavigateTo("a.com",
             "/payment_request_connection_allowlist_icon_blocked.html");

  GURL method_url = https_server()->GetURL("b.com", "/manifest-with-link");
  GURL manifest_url = https_server()->GetURL("b.com", "/nickpay.test/pay");
  GURL app_manifest_url =
      https_server()->GetURL("b.com", "/nickpay.test/app.json");
  GURL icon_url = https_server()->GetURL("b.com", "/nickpay.test/icon.png");

  // canMakePayment still returns true because
  // `kAllowJITInstallationWhenAppIconIsMissing` is enabled. However, below the
  // URLLoaderMonitor verifies the icon download is blocked as expected.
  ExpectCanMakePayment(true, method_url);

  monitor.WaitForUrls({method_url, manifest_url, app_manifest_url, icon_url});
  EXPECT_EQ(monitor.WaitForRequestCompletion(method_url).error_code, net::OK);
  EXPECT_EQ(monitor.WaitForRequestCompletion(manifest_url).error_code, net::OK);
  EXPECT_EQ(monitor.WaitForRequestCompletion(app_manifest_url).error_code,
            net::OK);
  EXPECT_EQ(monitor.WaitForRequestCompletion(icon_url).error_code,
            net::ERR_NETWORK_ACCESS_REVOKED);
}

// Test that Payment Request API PaymentRequestEvent.openWindow() is allowed
// when the URL is allowed by the connection allowlist.
IN_PROC_BROWSER_TEST_P(PaymentRequestConnectionAllowlistBrowserTest,
                       OpenWindowAllowed) {
  RegisterResponse("/", ResponseEntry("", {}, net::HTTP_OK));

  // Merchant page on a.com.
  RegisterResponse(
      "/payment_request_connection_allowlist_open_window_allowed.html",
      ResponseEntry(kOpenWindowMerchantPage, {}));

  // Payment app service worker on b.com. Attempts to open window.html via
  // PaymentRequestEvent.openWindow(). Resolves payment with 'success' if
  // opened, otherwise the error message.
  RegisterResponse("/connection_allowlist_open_window/sw.js",
                   ResponseEntry(kOpenWindowServiceWorkerScript,
                                 {{"Content-Type", "application/javascript"},
                                  {"Connection-Allowlist", R"(
                                    (
                                      "*://b.com:*/*/window.html"
                                    )
                                  )"}}));

  // Target to be opened by the service worker via PaymentRequestEvent:
  // openWindow().
  std::string window_relative_url{
      "/connection_allowlist_open_window/window.html"};
  RegisterResponse(
      window_relative_url,
      ResponseEntry("<!DOCTYPE html><html><body>Window</body></html>",
                    {{"Content-Type", "text/html"}}));

  NavigateTo("a.com",
             "/payment_request_connection_allowlist_open_window_allowed.html");

  std::string payment_method;
  InstallPaymentApp("b.com", "/connection_allowlist_open_window/sw.js",
                    &payment_method);

  GURL window_url = https_server()->GetURL("b.com", window_relative_url);
  content::URLLoaderMonitor monitor({window_url});

  // Verify PaymentRequestEvent: openWindow() succeeds because window.html
  // matches the service worker's allowlist.
  EXPECT_EQ("success", content::EvalJs(GetActiveWebContents(),
                                       content::JsReplace(
                                           "testOpenWindow($1, $2)",
                                           payment_method, window_url.spec())));
  // Verify `window_url` was requested. We check `GetRequestInfo()` instead of
  // `WaitForRequestCompletion()` because `testOpenWindow()` completes the
  // payment request right after `openWindow()` resolves, closing the payment
  // handler window before `URLLoaderMonitor` may have received `OnComplete()`.
  monitor.WaitForUrls({window_url});
  EXPECT_TRUE(monitor.GetRequestInfo(window_url).has_value());
}

// Test that Payment Request API PaymentRequestEvent.openWindow() is blocked
// when the URL is not allowed by the connection allowlist.
IN_PROC_BROWSER_TEST_P(PaymentRequestConnectionAllowlistBrowserTest,
                       OpenWindowBlocked) {
  RegisterResponse("/", ResponseEntry("", {}, net::HTTP_OK));

  // Merchant page on a.com.
  RegisterResponse(
      "/payment_request_connection_allowlist_open_window_blocked.html",
      ResponseEntry(kOpenWindowMerchantPage, {}));

  // Payment app service worker on b.com. The service worker has an empty
  // connection allowlist, which means no request is allowed.
  RegisterResponse("/connection_allowlist_open_window/sw.js",
                   ResponseEntry(kOpenWindowServiceWorkerScript,
                                 {{"Content-Type", "application/javascript"},
                                  {"Connection-Allowlist", "()"}}));

  // Target to be opened by the service worker via
  // PaymentRequestEvent.openWindow().
  std::string window_relative_url{
      "/connection_allowlist_open_window/window.html"};
  RegisterResponse(
      window_relative_url,
      ResponseEntry("<!DOCTYPE html><html><body>Window</body></html>",
                    {{"Content-Type", "text/html"}}));

  NavigateTo("a.com",
             "/payment_request_connection_allowlist_open_window_blocked.html");

  std::string payment_method;
  InstallPaymentApp("b.com", "/connection_allowlist_open_window/sw.js",
                    &payment_method);

  GURL window_url = https_server()->GetURL("b.com", window_relative_url);
  content::URLLoaderMonitor monitor({window_url});

  // Verify PaymentRequestEvent.openWindow() resolves to null and returns
  // the error message because window.html is not allowed by service worker's
  // allowlist.
  EXPECT_THAT(
      content::EvalJs(GetActiveWebContents(),
                      content::JsReplace("testOpenWindow($1, $2)",
                                         payment_method, window_url.spec()))
          .ExtractString(),
      HasSubstr(" is blocked by Connection Allowlist."));

  // The monitor never receives the request because it is blocked by the
  // connection allowlist check in
  // `ServiceWorkerVersion::OpenPaymentHandlerWindow`. It stops the request
  // before it reaches the URL loader. So it does not complete with net error
  // `ERR_NETWORK_ACCESS_REVOKED`.
  EXPECT_FALSE(monitor.GetRequestInfo(window_url).has_value());
}

// Test that service worker's connection allowlist's redirect directive has no
// effect on Payment Request API PaymentRequestEvent: openWindow() when there is
// a redirect.
IN_PROC_BROWSER_TEST_P(PaymentRequestConnectionAllowlistBrowserTest,
                       NoEffectOnOpenWindowRedirect) {
  RegisterResponse("/", ResponseEntry("", {}, net::HTTP_OK));

  // Merchant page on a.com.
  RegisterResponse(
      "/payment_request_connection_allowlist_open_window_redirect_allowed.html",
      ResponseEntry(kOpenWindowMerchantPage, {}));

  // Payment app service worker on b.com. Attempts to open redirect.html via
  // PaymentRequestEvent: openWindow(), which is then redirected to window.html.
  // Resolves payment with 'success' if opened, otherwise the error message.
  // Note the service worker's connection allowlist specifies `redirects=block`.
  // However, it should not have effect on PaymentRequestEvent: openWindow().
  RegisterResponse("/connection_allowlist_open_window/sw.js",
                   ResponseEntry(kOpenWindowServiceWorkerScript,
                                 {{"Content-Type", "application/javascript"},
                                  {"Connection-Allowlist", R"(
                                    (
                                      response-origin
                                      "*://b.com:*/*/redirect.html"
                                    ); redirects=block
                                  )"}}));

  GURL target_window_url = https_server()->GetURL(
      "b.com", "/connection_allowlist_open_window/window.html");
  RegisterResponse("/connection_allowlist_open_window/redirect.html",
                   ResponseEntry("", {{"Location", target_window_url.spec()}},
                                 net::HTTP_FOUND));

  // Target window document on b.com.
  RegisterResponse(
      "/connection_allowlist_open_window/window.html",
      ResponseEntry("<!DOCTYPE html><html><body>Window</body></html>",
                    {{"Content-Type", "text/html"}}));

  NavigateTo("a.com",
             "/payment_request_connection_allowlist_open_window_redirect_"
             "allowed.html");

  std::string payment_method;
  InstallPaymentApp("b.com", "/connection_allowlist_open_window/sw.js",
                    &payment_method);

  GURL redirect_url = https_server()->GetURL(
      "b.com", "/connection_allowlist_open_window/redirect.html");
  content::URLLoaderMonitor monitor({redirect_url});

  // Verify PaymentRequestEvent: openWindow() succeeds because the initial
  // request URL redirect.html matches the service worker's connection
  // allowlist. And the connection allowlist's redirect directive has no effect
  // for PaymentRequestEvent: openWindow().
  EXPECT_EQ(
      "success",
      content::EvalJs(GetActiveWebContents(),
                      content::JsReplace("testOpenWindow($1, $2)",
                                         payment_method, redirect_url.spec())));
  // Verify `redirect_url` was requested. We check `GetRequestInfo()` instead
  // of `WaitForRequestCompletion()` because `testOpenWindow()` completes the
  // payment request right after `openWindow()` resolves, closing the payment
  // handler window. Closing the window destroys its `WebContents` and in-flight
  // URL loaders before `URLLoaderMonitor` may have received `OnComplete()` from
  // the network service.
  monitor.WaitForUrls({redirect_url});
  EXPECT_TRUE(monitor.GetRequestInfo(redirect_url).has_value());
}

IN_PROC_BROWSER_TEST_P(PaymentRequestConnectionAllowlistBrowserTest,
                       PaymentHandlerServiceWorkerRegistrationAllowed) {
  RegisterResponse(
      "/payment_request.html",
      ResponseEntry{"<html><body>Hello</body></html>",
                    {{"Connection-Allowlist", "(response-origin)"}}});
  RegisterResponse(
      "/sw_allowed.js",
      ResponseEntry{
          "self.addEventListener('install', e => self.skipWaiting());",
          {{"Content-Type", "text/javascript"}}});

  GURL main_url = https_server()->GetURL("a.test", "/payment_request.html");
  GURL service_worker_url = https_server()->GetURL("a.test", "/sw_allowed.js");

  EXPECT_TRUE(content::NavigateToURL(GetActiveWebContents(), main_url));
  content::URLLoaderMonitor monitor;

  // Install a payment app, which registers a service worker using the URL.
  // Payment service worker registration succeeded because the URL is allowed by
  // the connection allowlist.
  ASSERT_TRUE(
      test::PaymentAppInstallUtil::InstallPaymentAppForPaymentMethodIdentifier(
          *GetActiveWebContents()->GetPrimaryMainFrame(), service_worker_url,
          "https://bobpay.com", IconInstall::kWithIcon));

  monitor.WaitForUrls({service_worker_url});
  EXPECT_EQ(monitor.WaitForRequestCompletion(service_worker_url).error_code,
            net::OK);
}

IN_PROC_BROWSER_TEST_P(PaymentRequestConnectionAllowlistBrowserTest,
                       PaymentHandlerServiceWorkerRegistrationBlocked) {
  RegisterResponse("/payment_request.html",
                   ResponseEntry{"<html><body>Hello</body></html>",
                                 {{"Connection-Allowlist", "()"}}});
  RegisterResponse(
      "/sw_denied.js",
      ResponseEntry{
          "self.addEventListener('install', e => self.skipWaiting());",
          {{"Content-Type", "text/javascript"}}});

  GURL main_url = https_server()->GetURL("a.test", "/payment_request.html");
  GURL service_worker_url = https_server()->GetURL("a.test", "/sw_denied.js");

  EXPECT_TRUE(content::NavigateToURL(GetActiveWebContents(), main_url));
  content::URLLoaderMonitor monitor;

  // Install a payment app, which registers a service worker using the URL.
  // Payment service worker registration failed because the URL is blocked by
  // the connection allowlist.
  ASSERT_FALSE(
      test::PaymentAppInstallUtil::InstallPaymentAppForPaymentMethodIdentifier(
          *GetActiveWebContents()->GetPrimaryMainFrame(), service_worker_url,
          "https://bobpay.com", IconInstall::kWithIcon));

  monitor.WaitForUrls({service_worker_url});
  EXPECT_EQ(monitor.WaitForRequestCompletion(service_worker_url).error_code,
            net::ERR_NETWORK_ACCESS_REVOKED);
}

IN_PROC_BROWSER_TEST_P(PaymentRequestConnectionAllowlistBrowserTest,
                       PaymentHandlerServiceWorkerRegistrationAllowedInIframe) {
  RegisterResponse(
      "/payment_request.html",
      ResponseEntry{"<html><body><iframe id=\"test\" name=\"test\"></iframe>"
                    "</body></html>",
                    {{"Connection-Allowlist", "()"}}});
  RegisterResponse(
      "/payment_request_iframe.html",
      ResponseEntry{"<html><body>Hello</body></html>",
                    {{"Connection-Allowlist", "(response-origin)"}}});
  RegisterResponse(
      "/sw_allowed.js",
      ResponseEntry{
          "self.addEventListener('install', e => self.skipWaiting());",
          {{"Content-Type", "text/javascript"}}});

  GURL main_url = https_server()->GetURL("a.test", "/payment_request.html");
  GURL iframe_url =
      https_server()->GetURL("a.test", "/payment_request_iframe.html");
  GURL service_worker_url = https_server()->GetURL("a.test", "/sw_allowed.js");

  EXPECT_TRUE(content::NavigateToURL(GetActiveWebContents(), main_url));
  EXPECT_TRUE(
      content::NavigateIframeToURL(GetActiveWebContents(), "test", iframe_url));
  content::RenderFrameHost* iframe = content::FrameMatchingPredicate(
      GetActiveWebContents()->GetPrimaryPage(),
      base::BindRepeating(&content::FrameHasSourceUrl, iframe_url));
  ASSERT_TRUE(iframe);
  content::URLLoaderMonitor monitor;

  // Install a payment app, which registers a service worker using the URL.
  // Payment service worker registration succeeded because the URL is allowed by
  // the connection allowlist.
  ASSERT_TRUE(
      test::PaymentAppInstallUtil::InstallPaymentAppForPaymentMethodIdentifier(
          *iframe, service_worker_url, "https://bobpay.com",
          IconInstall::kWithIcon));

  monitor.WaitForUrls({service_worker_url});
  EXPECT_EQ(monitor.WaitForRequestCompletion(service_worker_url).error_code,
            net::OK);
}

IN_PROC_BROWSER_TEST_P(PaymentRequestConnectionAllowlistBrowserTest,
                       PaymentHandlerServiceWorkerRegistrationBlockedInIframe) {
  RegisterResponse(
      "/payment_request.html",
      ResponseEntry{"<html><body><iframe id=\"test\" name=\"test\"></iframe>"
                    "</body></html>",
                    {{"Connection-Allowlist", "(response-origin)"}}});
  RegisterResponse("/payment_request_iframe.html",
                   ResponseEntry{"<html><body>Hello</body></html>",
                                 {{"Connection-Allowlist", "()"}}});
  RegisterResponse(
      "/sw_denied.js",
      ResponseEntry{
          "self.addEventListener('install', e => self.skipWaiting());",
          {{"Content-Type", "text/javascript"}}});

  GURL main_url = https_server()->GetURL("a.test", "/payment_request.html");
  GURL iframe_url =
      https_server()->GetURL("a.test", "/payment_request_iframe.html");
  GURL service_worker_url = https_server()->GetURL("a.test", "/sw_denied.js");

  EXPECT_TRUE(content::NavigateToURL(GetActiveWebContents(), main_url));
  EXPECT_TRUE(
      content::NavigateIframeToURL(GetActiveWebContents(), "test", iframe_url));
  content::RenderFrameHost* iframe = content::FrameMatchingPredicate(
      GetActiveWebContents()->GetPrimaryPage(),
      base::BindRepeating(&content::FrameHasSourceUrl, iframe_url));
  ASSERT_TRUE(iframe);
  content::URLLoaderMonitor monitor;

  // Install a payment app, which registers a service worker using the URL.
  // Payment service worker registration failed because the URL is blocked by
  // the connection allowlist.
  ASSERT_FALSE(
      test::PaymentAppInstallUtil::InstallPaymentAppForPaymentMethodIdentifier(
          *iframe, service_worker_url, "https://bobpay.com",
          IconInstall::kWithIcon));

  monitor.WaitForUrls({service_worker_url});
  EXPECT_EQ(monitor.WaitForRequestCompletion(service_worker_url).error_code,
            net::ERR_NETWORK_ACCESS_REVOKED);
}

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(
    PaymentRequestConnectionAllowlistBrowserTest);

}  // namespace
}  // namespace payments
