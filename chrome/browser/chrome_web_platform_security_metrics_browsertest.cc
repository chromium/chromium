// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/strings/string_piece.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/platform_thread.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/cross_origin_opener_policy.mojom.h"
#include "third_party/blink/public/common/features.h"

namespace {
const int kWasmPageSize = 1 << 16;

// Path to a response that passes Private Network Access checks.
constexpr char kPnaPath[] =
    "/set-header"
    "?Access-Control-Allow-Origin: *"
    "&Access-Control-Allow-Private-Network: true";

// Web platform security features are implemented by content/ and blink/.
// However, since ContentBrowserClientImpl::LogWebFeatureForCurrentPage() is
// currently left blank in content/, metrics logging can't be tested from
// content/. So it is tested from chrome/ instead.
class ChromeWebPlatformSecurityMetricsBrowserTest
    : public InProcessBrowserTest {
 public:
  using WebFeature = blink::mojom::WebFeature;

  ChromeWebPlatformSecurityMetricsBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS),
        http_server_(net::EmbeddedTestServer::TYPE_HTTP) {
    features_.InitWithFeatures(
        {
            // Enabled:
            network::features::kCrossOriginOpenerPolicy,
            // SharedArrayBuffer is needed for these tests.
            features::kSharedArrayBuffer,
            // Some LNA worker feature relies on this.
            // TODO(https://crbug.com/1430451): Remove this once LNA for workers
            // metric logging doesn't rely on kPlzDedicatedWorker
            blink::features::kPlzDedicatedWorker,
        },
        {
            // Disabled because some subtests set document.domain and this
            // feature flag prevents that:
            blink::features::kOriginAgentClusterDefaultEnabled,
        });
  }

  content::WebContents* web_contents() const {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  void set_monitored_feature(WebFeature feature) {
    monitored_feature_ = feature;
  }

  void LoadIFrame(const GURL& url) {
    LoadIFrameInWebContents(web_contents(), url);
  }

  content::WebContents* OpenPopup(const GURL& url) {
    content::WebContentsAddedObserver new_tab_observer;
    EXPECT_TRUE(
        content::ExecJs(web_contents(), "window.open('" + url.spec() + "')"));
    content::WebContents* web_contents = new_tab_observer.GetWebContents();
    EXPECT_TRUE(content::WaitForLoadStop(web_contents));
    return web_contents;
  }

  void LoadIFrameInWebContents(content::WebContents* web_contents,
                               const GURL& url) {
    EXPECT_EQ(true, content::EvalJs(web_contents, content::JsReplace(R"(
      new Promise(resolve => {
        let iframe = document.createElement("iframe");
        iframe.src = $1;
        iframe.onload = () => resolve(true);
        document.body.appendChild(iframe);
      });
    )",
                                                                     url)));
  }

  void ExpectHistogramIncreasedBy(int count) {
    expected_count_ += count;
    histogram_.ExpectBucketCount("Blink.UseCounter.Features",
                                 monitored_feature_, expected_count_);
  }

  net::EmbeddedTestServer& https_server() { return https_server_; }
  net::EmbeddedTestServer& http_server() { return http_server_; }

  // Fetch the Blink.UseCounter.Features histogram in every renderer process
  // until reaching, but not exceeding, |expected_count|.
  void CheckCounter(WebFeature feature, int expected_count) {
    CheckHistogramCount("Blink.UseCounter.Features", feature, expected_count);
  }

  // Fetch the Blink.UseCounter.MainFrame.Features histogram in every renderer
  // process until reaching, but not exceeding, |expected_count|.
  void CheckCounterMainFrame(WebFeature feature, int expected_count) {
    CheckHistogramCount("Blink.UseCounter.MainFrame.Features", feature,
                        expected_count);
  }

  // Fetch the |histogram|'s |bucket| in every renderer process until reaching,
  // but not exceeding, |expected_count|.
  template <typename T>
  void CheckHistogramCount(base::StringPiece histogram,
                           T bucket,
                           int expected_count) {
    while (true) {
      content::FetchHistogramsFromChildProcesses();
      metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

      int count = histogram_.GetBucketCount(histogram, bucket);
      CHECK_LE(count, expected_count);
      if (count == expected_count)
        return;

      base::PlatformThread::Sleep(base::Milliseconds(5));
    }
  }

 private:
  void SetUpOnMainThread() final {
    host_resolver()->AddRule("*", "127.0.0.1");

    https_server_.AddDefaultHandlers(GetChromeTestDataDir());
    http_server_.AddDefaultHandlers(GetChromeTestDataDir());

    // Add content/test/data for cross_site_iframe_factory.html
    https_server_.ServeFilesFromSourceDirectory("content/test/data");
    http_server_.ServeFilesFromSourceDirectory("content/test/data");

    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
    ASSERT_TRUE(https_server_.Start());
    ASSERT_TRUE(http_server_.Start());
    EXPECT_TRUE(content::NavigateToURL(web_contents(), GURL("about:blank")));
  }

  void SetUpCommandLine(base::CommandLine* command_line) final {
    // For https_server()
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  net::EmbeddedTestServer https_server_;
  net::EmbeddedTestServer http_server_;
  int expected_count_ = 0;
  base::HistogramTester histogram_;
  WebFeature monitored_feature_;
  base::test::ScopedFeatureList features_;
};

// Return the child of `parent`.
// Precondition: the number of children must be one.
content::RenderFrameHost* GetChild(content::RenderFrameHost& parent) {
  content::RenderFrameHost* child_rfh = nullptr;
  parent.ForEachRenderFrameHost([&](content::RenderFrameHost* rfh) {
    if (&parent == rfh->GetParent()) {
      CHECK(!child_rfh) << "Multiple children found";
      child_rfh = rfh;
    }
  });
  CHECK(child_rfh) << "No children found";
  return child_rfh;
}

// Check the kCrossOriginOpenerPolicyReporting feature usage. No header => 0
// count.
IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       CrossOriginOpenerPolicyReportingNoHeader) {
  set_monitored_feature(WebFeature::kCrossOriginOpenerPolicyReporting);
  GURL url = https_server().GetURL("a.com", "/title1.html");
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url));
  ExpectHistogramIncreasedBy(0);
}

IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       PrivateNetworkAccessIgnoredCrossSitePreflightError) {
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      https_server().GetURL(
          "a.com",
          "/private_network_access/no-favicon-treat-as-public-address.html")));

  ASSERT_EQ(true, content::EvalJs(
                      web_contents(),
                      content::JsReplace(
                          "fetch($1).then(response => response.ok)",
                          https_server().GetURL("b.com", "/cors-ok.txt"))));

  CheckCounter(WebFeature::kPrivateNetworkAccessPreflightWarning, 1);
  CheckCounter(
      WebFeature::kPrivateNetworkAccessIgnoredCrossOriginPreflightError, 1);
  CheckCounter(WebFeature::kPrivateNetworkAccessIgnoredCrossSitePreflightError,
               1);
}

IN_PROC_BROWSER_TEST_F(
    ChromeWebPlatformSecurityMetricsBrowserTest,
    PrivateNetworkAccessIgnoredCrossOriginSameSitePreflightError) {
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      https_server().GetURL(
          "a.com",
          "/private_network_access/no-favicon-treat-as-public-address.html")));

  ASSERT_EQ(true, content::EvalJs(web_contents(),
                                  content::JsReplace(
                                      "fetch($1).then(response => response.ok)",
                                      https_server().GetURL("subdomain.a.com",
                                                            "/cors-ok.txt"))));

  CheckCounter(WebFeature::kPrivateNetworkAccessPreflightWarning, 1);
  CheckCounter(
      WebFeature::kPrivateNetworkAccessIgnoredCrossOriginPreflightError, 1);
  CheckCounter(WebFeature::kPrivateNetworkAccessIgnoredCrossSitePreflightError,
               0);
}

IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       PrivateNetworkAccessSameOriginNoIgnoredPreflightError) {
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      https_server().GetURL(
          "a.com",
          "/private_network_access/no-favicon-treat-as-public-address.html")));

  ASSERT_EQ(true, content::EvalJs(
                      web_contents(),
                      content::JsReplace(
                          "fetch($1).then(response => response.ok)",
                          https_server().GetURL("a.com", "/cors-ok.txt"))));

  CheckCounter(WebFeature::kPrivateNetworkAccessPreflightWarning, 0);
  CheckCounter(
      WebFeature::kPrivateNetworkAccessIgnoredCrossOriginPreflightError, 0);
  CheckCounter(WebFeature::kPrivateNetworkAccessIgnoredCrossSitePreflightError,
               0);
}

// This test verifies that when a secure context served from the public address
// space loads a resource from the private network, the correct WebFeature is
// use-counted.
IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       PrivateNetworkAccessFetchWithPreflight) {
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      https_server().GetURL(
          "a.com",
          "/private_network_access/no-favicon-treat-as-public-address.html")));

  ASSERT_EQ(true,
            content::EvalJs(
                web_contents(),
                content::JsReplace("fetch($1).then(response => response.ok)",
                                   https_server().GetURL("b.com", kPnaPath))));

  CheckCounter(WebFeature::kAddressSpacePublicSecureContextEmbeddedLocal, 1);
  CheckCounter(WebFeature::kPrivateNetworkAccessPreflightSuccess, 1);
}

// This test verifies that when a preflight request is sent ahead of a private
// network request, the server replies with Access-Control-Allow-Origin but
// without Access-Control-Allow-Private-Network, and enforcement is not enabled,
// the correct WebFeature is use-counted to reflect the suppressed error.
IN_PROC_BROWSER_TEST_F(
    ChromeWebPlatformSecurityMetricsBrowserTest,
    PrivateNetworkAccessFetchWithPreflightRepliedWithoutLNAHeaders) {
  ASSERT_EQ(true, content::NavigateToURL(
                      web_contents(),
                      https_server().GetURL(
                          "a.com",
                          "/private_network_access/"
                          "no-favicon-treat-as-public-address.html")));

  // The server does not reply with valid CORS headers, so the preflight fails.
  // The enforcement feature is not enabled however, so the error is suppressed.
  // Instead, a warning is shown in DevTools and a WebFeature use-counted.
  ASSERT_EQ(true, content::EvalJs(
                      web_contents(),
                      content::JsReplace(
                          "fetch($1).then(response => response.ok)",
                          https_server().GetURL("b.com", "/cors-ok.txt"))));

  CheckCounter(WebFeature::kAddressSpacePublicSecureContextEmbeddedLocal, 1);
  CheckCounter(WebFeature::kPrivateNetworkAccessPreflightWarning, 1);
}

IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       LocalNetworkAccessFetchInWorker) {
  ASSERT_EQ(true,
            content::NavigateToURL(
                web_contents(), https_server().GetURL("a.com",
                                                      "/private_network_access/"
                                                      "no-favicon.html")));

  base::StringPiece kScriptTemplate = R"(
    (async () => {
      const worker = new Worker("/workers/fetcher_treat_as_public.js");

      const messagePromise = new Promise((resolve) => {
        const listener = (event) => resolve(event.data);
        worker.addEventListener("message", listener, { once: true });
      });

      worker.postMessage($1);

      const { error, ok } = await messagePromise;
      if (error !== undefined) {
        throw(error);
      }

      return ok;
    })()
  )";

  ASSERT_EQ(true,
            content::EvalJs(web_contents(),
                            content::JsReplace(kScriptTemplate,
                                               https_server().GetURL(
                                                   "b.com", "/cors-ok.txt"))));

  CheckCounter(WebFeature::kPrivateNetworkAccessWithinWorker, 1);
  CheckCounter(WebFeature::kPrivateNetworkAccessPreflightWarning, 1);
}

IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       LocalNetworkAccessFetchInSharedWorker) {
  ASSERT_EQ(true,
            content::NavigateToURL(
                web_contents(), https_server().GetURL("a.com",
                                                      "/private_network_access/"
                                                      "no-favicon.html")));

  base::StringPiece kScriptTemplate = R"(
    (async () => {
      const worker = await new Promise((resolve, reject) => {
        const worker =
            new SharedWorker("/workers/shared_fetcher_treat_as_public.js");
        worker.port.addEventListener("message", () => resolve(worker));
        worker.addEventListener("error", reject);
        worker.port.start();
      });

      const messagePromise = new Promise((resolve) => {
        const listener = (event) => resolve(event.data);
        worker.port.addEventListener("message", listener, { once: true });
      });

      worker.port.postMessage($1);

      const { error, ok } = await messagePromise;
      if (error !== undefined) {
        throw(error);
      }

      return ok;
    })()
  )";
  ASSERT_EQ(true,
            content::EvalJs(web_contents(),
                            content::JsReplace(kScriptTemplate,
                                               https_server().GetURL(
                                                   "b.com", "/cors-ok.txt"))));

  CheckCounter(WebFeature::kPrivateNetworkAccessWithinWorker, 1);
  CheckCounter(WebFeature::kPrivateNetworkAccessPreflightWarning, 1);
}

// Check the kCrossOriginOpenerPolicyReporting feature usage. COOP-Report-Only +
// HTTP => 0 count.
IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       CrossOriginOpenerPolicyReportingReportOnlyHTTP) {
  set_monitored_feature(WebFeature::kCrossOriginOpenerPolicyReporting);
  GURL url = http_server().GetURL("a.com",
                                  "/set-header?"
                                  "Cross-Origin-Opener-Policy-Report-Only: "
                                  "same-origin; report-to%3d\"a\"");
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url));
  ExpectHistogramIncreasedBy(0);
}

// Check the kCrossOriginOpenerPolicyReporting feature usage. COOP-Report-Only +
// HTTPS => 1 count.
IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       CrossOriginOpenerPolicyReportingReportOnlyHTTPS) {
  set_monitored_feature(WebFeature::kCrossOriginOpenerPolicyReporting);
  GURL url = https_server().GetURL("a.com",
                                   "/set-header?"
                                   "Cross-Origin-Opener-Policy-Report-Only: "
                                   "same-origin; report-to%3d\"a\"");
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url));
  ExpectHistogramIncreasedBy(1);
}

// Check the kCrossOriginOpenerPolicyReporting feature usage. COOP + HTPS => 1
// count.
IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       CrossOriginOpenerPolicyReportingCOOPHTTPS) {
  set_monitored_feature(WebFeature::kCrossOriginOpenerPolicyReporting);
  GURL url = https_server().GetURL("a.com",
                                   "/set-header?"
                                   "Cross-Origin-Opener-Policy: "
                                   "same-origin; report-to%3d\"a\"");
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url));
  ExpectHistogramIncreasedBy(1);
}

// Check the kCrossOriginOpenerPolicyReporting feature usage. COOP + COOP-RO  +
// HTTPS => 1 count.
IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       CrossOriginOpenerPolicyReportingCOOPAndReportOnly) {
  set_monitored_feature(WebFeature::kCrossOriginOpenerPolicyReporting);
  GURL url = https_server().GetURL("a.com",
                                   "/set-header?"
                                   "Cross-Origin-Opener-Policy: "
                                   "same-origin; report-to%3d\"a\"&"
                                   "Cross-Origin-Opener-Policy-Report-Only: "
                                   "same-origin; report-to%3d\"a\"");
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url));
  ExpectHistogramIncreasedBy(1);
}

// Check the kCrossOriginOpenerPolicyReporting feature usage. No report
// endpoints defined => 0 count.
IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       CrossOriginOpenerPolicyReportingNoEndpoint) {
  set_monitored_feature(WebFeature::kCrossOriginOpenerPolicyReporting);
  GURL url = https_server().GetURL(
      "a.com",
      "/set-header?"
      "Cross-Origin-Opener-Policy: same-origin&"
      "Cross-Origin-Opener-Policy-Report-Only: same-origin");
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url));
  ExpectHistogramIncreasedBy(0);
}

// Check the kCrossOriginOpenerPolicyReporting feature usage. Main frame
// (COOP-RO), subframe (COOP-RO) => 1 count.
IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       CrossOriginOpenerPolicyReportingMainFrameAndSubframe) {
  set_monitored_feature(WebFeature::kCrossOriginOpenerPolicyReporting);
  GURL url = https_server().GetURL("a.com",
                                   "/set-header?"
                                   "Cross-Origin-Opener-Policy-Report-Only: "
                                   "same-origin; report-to%3d\"a\"");
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url));
  LoadIFrame(url);
  ExpectHistogramIncreasedBy(1);
}

// Check the kCrossOriginOpenerPolicyReporting feature usage. Main frame
// (no-headers), subframe (COOP-RO) => 0 count.
IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       CrossOriginOpenerPolicyReportingUsageSubframeOnly) {
  set_monitored_feature(WebFeature::kCrossOriginOpenerPolicyReporting);
  GURL main_document_url = https_server().GetURL("a.com", "/title1.html");
  GURL sub_document_url =
      https_server().GetURL("a.com",
                            "/set-header?"
                            "Cross-Origin-Opener-Policy-Report-Only: "
                            "same-origin; report-to%3d\"a\"");
  EXPECT_TRUE(content::NavigateToURL(web_contents(), main_document_url));
  LoadIFrame(sub_document_url);
  ExpectHistogramIncreasedBy(0);
}

// Check kCrossOriginSubframeWithoutEmbeddingControl reporting. Same-origin
// iframe (no headers) => 0 count.
IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       CrossOriginSubframeWithoutEmbeddingControlSameOrigin) {
  set_monitored_feature(
      WebFeature::kCrossOriginSubframeWithoutEmbeddingControl);
  GURL url = https_server().GetURL("a.com", "/title1.html");
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url));
  LoadIFrame(url);
  ExpectHistogramIncreasedBy(0);
}

// Check kCrossOriginSubframeWithoutEmbeddingControl reporting. Cross-origin
// iframe (no headers) => 0 count.
IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       CrossOriginSubframeWithoutEmbeddingControlNoHeaders) {
  set_monitored_feature(
      WebFeature::kCrossOriginSubframeWithoutEmbeddingControl);
  GURL main_document_url = https_server().GetURL("a.com", "/title1.html");
  GURL sub_document_url = https_server().GetURL("b.com", "/title1.html");
  EXPECT_TRUE(content::NavigateToURL(web_contents(), main_document_url));
  LoadIFrame(sub_document_url);
  ExpectHistogramIncreasedBy(1);
}

// Check kCrossOriginSubframeWithoutEmbeddingControl reporting. Cross-origin
// iframe (CSP frame-ancestors) => 0 count.
IN_PROC_BROWSER_TEST_F(
    ChromeWebPlatformSecurityMetricsBrowserTest,
    CrossOriginSubframeWithoutEmbeddingControlFrameAncestors) {
  set_monitored_feature(
      WebFeature::kCrossOriginSubframeWithoutEmbeddingControl);
  GURL main_document_url = https_server().GetURL("a.com", "/title1.html");
  url::Origin main_document_origin = url::Origin::Create(main_document_url);
  std::string csp_header = "Content-Security-Policy: frame-ancestors 'self' *;";
  GURL sub_document_url =
      https_server().GetURL("b.com", "/set-header?" + csp_header);
  EXPECT_TRUE(content::NavigateToURL(web_contents(), main_document_url));
  LoadIFrame(sub_document_url);
  ExpectHistogramIncreasedBy(0);
}

// Check kCrossOriginSubframeWithoutEmbeddingControl reporting. Cross-origin
// iframe (blocked by CSP header) => 0 count.
IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       CrossOriginSubframeWithoutEmbeddingControlNoEmbedding) {
  set_monitored_feature(
      WebFeature::kCrossOriginSubframeWithoutEmbeddingControl);
  GURL main_document_url = https_server().GetURL("a.com", "/title1.html");
  GURL sub_document_url =
      https_server().GetURL("b.com",
                            "/set-header?"
                            "Content-Security-Policy: frame-ancestors 'self';");
  EXPECT_TRUE(content::NavigateToURL(web_contents(), main_document_url));
  LoadIFrame(sub_document_url);
  ExpectHistogramIncreasedBy(0);
}

// Check kCrossOriginSubframeWithoutEmbeddingControl reporting. Cross-origin
// iframe (other CSP header) => 1 count.
IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       CrossOriginSubframeWithoutEmbeddingControlOtherCSP) {
  set_monitored_feature(
      WebFeature::kCrossOriginSubframeWithoutEmbeddingControl);
  GURL main_document_url = https_server().GetURL("a.com", "/title1.html");
  GURL sub_document_url =
      https_server().GetURL("b.com",
                            "/set-header?"
                            "Content-Security-Policy: script-src 'self';");
  EXPECT_TRUE(content::NavigateToURL(web_contents(), main_document_url));
  LoadIFrame(sub_document_url);
  ExpectHistogramIncreasedBy(1);
}

// Check kEmbeddedCrossOriginFrameWithoutFrameAncestorsOrXFO feature usage.
// This should increment in cases where a cross-origin frame is embedded which
// does not assert either X-Frame-Options or CSP's frame-ancestors.
IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       EmbeddingOptIn) {
  set_monitored_feature(
      WebFeature::kEmbeddedCrossOriginFrameWithoutFrameAncestorsOrXFO);
  GURL main_document_url = https_server().GetURL("a.com", "/title1.html");

  struct TestCase {
    const char* name;
    const char* host;
    const char* header;
    bool expect_counter;
  } cases[] = {{
                   "Same-origin, no XFO, no frame-ancestors",
                   "a.com",
                   nullptr,
                   false,
               },
               {
                   "Cross-origin, no XFO, no frame-ancestors",
                   "b.com",
                   nullptr,
                   true,
               },
               {
                   "Same-origin, yes XFO, no frame-ancestors",
                   "a.com",
                   "X-Frame-Options: ALLOWALL",
                   false,
               },
               {
                   "Cross-origin, yes XFO, no frame-ancestors",
                   "b.com",
                   "X-Frame-Options: ALLOWALL",
                   false,
               },
               {
                   "Same-origin, no XFO, yes frame-ancestors",
                   "a.com",
                   "Content-Security-Policy: frame-ancestors *",
                   false,
               },
               {
                   "Cross-origin, no XFO, yes frame-ancestors",
                   "b.com",
                   "Content-Security-Policy: frame-ancestors *",
                   false,
               }};

  for (auto test : cases) {
    SCOPED_TRACE(test.name);
    EXPECT_TRUE(content::NavigateToURL(web_contents(), main_document_url));

    std::string path = "/set-header?";
    if (test.header)
      path += test.header;
    GURL url = https_server().GetURL(test.host, path);
    LoadIFrame(url);

    ExpectHistogramIncreasedBy(test.expect_counter ? 1 : 0);
  }
}

IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       NonCrossOriginIsolatedCheckSabConstructor) {
  GURL url = https_server().GetURL("a.com", "/empty.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), url));
  EXPECT_EQ(true, content::EvalJs(web_contents(),
                                  "'SharedArrayBuffer' in globalThis"));
  CheckCounter(WebFeature::kV8SharedArrayBufferConstructedWithoutIsolation, 0);
  CheckCounter(WebFeature::kV8SharedArrayBufferConstructed, 0);
}

IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       NonCrossOriginIsolatedSabSizeZero) {
  GURL url = https_server().GetURL("a.com", "/empty.html");
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url));
  EXPECT_EQ(true, content::ExecJs(web_contents(), "new SharedArrayBuffer(0)"));
  CheckCounter(WebFeature::kV8SharedArrayBufferConstructedWithoutIsolation, 1);
  CheckCounter(WebFeature::kV8SharedArrayBufferConstructed, 0);
}

IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       NonCrossOriginIsolatedSab) {
  GURL url = https_server().GetURL("a.com", "/empty.html");
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url));
  EXPECT_EQ(true,
            content::ExecJs(web_contents(), "new SharedArrayBuffer(8192)"));
  CheckCounter(WebFeature::kV8SharedArrayBufferConstructedWithoutIsolation, 1);
  CheckCounter(WebFeature::kV8SharedArrayBufferConstructed, 0);
}

IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       CrossOriginIsolatedSab) {
  GURL url =
      https_server().GetURL("a.com",
                            "/set-header"
                            "?Cross-Origin-Opener-Policy: same-origin"
                            "&Cross-Origin-Embedder-Policy: require-corp");
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url));
  EXPECT_EQ(true,
            content::ExecJs(web_contents(), "new SharedArrayBuffer(8192)"));
  CheckCounter(WebFeature::kV8SharedArrayBufferConstructedWithoutIsolation, 0);
  CheckCounter(WebFeature::kV8SharedArrayBufferConstructed, 1);
}

IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       WasmMemorySharingCrossSite) {
  GURL main_url = https_server().GetURL("a.com", "/empty.html");
  GURL sub_url = https_server().GetURL("b.com", "/empty.html");

  EXPECT_TRUE(content::NavigateToURL(web_contents(), main_url));
  LoadIFrame(sub_url);

  content::RenderFrameHost* main_document =
      web_contents()->GetPrimaryMainFrame();
  content::RenderFrameHost* sub_document = ChildFrameAt(main_document, 0);

  EXPECT_EQ(true, content::ExecJs(main_document, R"(
    received_memory = undefined;
    addEventListener("message", event => {
      received_memory = event.data;
    });
  )"));

  EXPECT_EQ(true, content::ExecJs(sub_document, R"(
    const memory = new WebAssembly.Memory({
      initial:1,
      maximum:1,
      shared:true
    });
    parent.postMessage(memory, "*");
  )"));

  // It doesn't exist yet a warning or an error being dispatched for failing to
  // send a WebAssembly.Memory. This test simply wait.
  EXPECT_EQ("Success: Nothing received", content::EvalJs(main_document, R"(
    new Promise(async resolve => {
      await new Promise(r => setTimeout(r, 1000));
      if (received_memory)
        resolve("Failure: Received Webassembly Memory");
      else
        resolve("Success: Nothing received");
    });
  )"));

  CheckCounter(WebFeature::kV8SharedArrayBufferConstructedWithoutIsolation, 1);
  CheckCounter(WebFeature::kV8SharedArrayBufferConstructed, 0);
}

IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       WasmMemorySharingCrossOrigin) {
  GURL main_url = https_server().GetURL("a.a.com", "/empty.html");
  GURL sub_url = https_server().GetURL("b.a.com", "/empty.html");

  EXPECT_TRUE(content::NavigateToURL(web_contents(), main_url));
  LoadIFrame(sub_url);

  content::RenderFrameHost* main_document =
      web_contents()->GetPrimaryMainFrame();
  content::RenderFrameHost* sub_document = ChildFrameAt(main_document, 0);

  EXPECT_EQ(true, content::ExecJs(main_document, R"(
    addEventListener("message", event => {
      received_memory = event.data;
    });
  )"));

  EXPECT_EQ(true, content::ExecJs(sub_document, R"(
    const memory = new WebAssembly.Memory({
      initial:1,
      maximum:1,
      shared:true
    });
    parent.postMessage(memory, "*");
  )"));

  EXPECT_EQ(1 * kWasmPageSize, content::EvalJs(main_document, R"(
    new Promise(async resolve => {
      while (!received_memory)
        await new Promise(r => setTimeout(r, 10));
      resolve(received_memory.buffer.byteLength);
    });
  )"));

  CheckCounter(WebFeature::kV8SharedArrayBufferConstructedWithoutIsolation, 1);
  CheckCounter(WebFeature::kV8SharedArrayBufferConstructed, 0);
}

IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       WasmMemorySharingSameOrigin) {
  GURL main_url = https_server().GetURL("a.com", "/empty.html");
  GURL sub_url = https_server().GetURL("a.com", "/empty.html");

  EXPECT_TRUE(content::NavigateToURL(web_contents(), main_url));
  LoadIFrame(sub_url);

  content::RenderFrameHost* main_document =
      web_contents()->GetPrimaryMainFrame();
  content::RenderFrameHost* sub_document = ChildFrameAt(main_document, 0);

  EXPECT_EQ(true, content::ExecJs(main_document, R"(
    received_memory = undefined;
    addEventListener("message", event => {
      received_memory = event.data;
    });
  )"));

  EXPECT_EQ(true, content::ExecJs(sub_document, R"(
    const memory = new WebAssembly.Memory({
      initial:1,
      maximum:1,
      shared:true
    });
    parent.postMessage(memory, "*");
  )"));

  EXPECT_EQ(1 * kWasmPageSize, content::EvalJs(main_document, R"(
    new Promise(async resolve => {
      while (!received_memory)
        await new Promise(r => setTimeout(r, 10));
      resolve(received_memory.buffer.byteLength);
    });
  )"));

  CheckCounter(WebFeature::kV8SharedArrayBufferConstructedWithoutIsolation, 1);
  CheckCounter(WebFeature::kV8SharedArrayBufferConstructed, 0);
}

IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       WasmMemorySharingCrossOriginBeforeSetDocumentDomain) {
  GURL main_url = https_server().GetURL("sub.a.com", "/empty.html");
  GURL sub_url = https_server().GetURL("a.com", "/empty.html");

  EXPECT_TRUE(content::NavigateToURL(web_contents(), main_url));
  LoadIFrame(sub_url);

  content::RenderFrameHost* main_document =
      web_contents()->GetPrimaryMainFrame();
  content::RenderFrameHost* sub_document = ChildFrameAt(main_document, 0);

  EXPECT_EQ(true, content::ExecJs(main_document, R"(
    document.domain = "a.com";
    received_memory = undefined;
    addEventListener("message", event => {
      received_memory = event.data;
    });
  )"));

  EXPECT_EQ(true, content::ExecJs(sub_document, R"(
    document.domain = "a.com";
    const memory = new WebAssembly.Memory({
      initial:1,
      maximum:1,
      shared:true
    });
    parent.postMessage(memory, "*");
  )"));

  EXPECT_EQ(1 * kWasmPageSize, content::EvalJs(main_document, R"(
    new Promise(async resolve => {
      while (!received_memory)
        await new Promise(r => setTimeout(r, 10));
      resolve(received_memory.buffer.byteLength);
    });
  )"));

  CheckCounter(WebFeature::kV8SharedArrayBufferConstructedWithoutIsolation, 1);
  CheckCounter(WebFeature::kV8SharedArrayBufferConstructed, 0);
}

IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       WasmMemorySharingCrossOriginAfterSetDocumentDomain) {
  GURL main_url = https_server().GetURL("sub.a.com", "/empty.html");
  GURL sub_url = https_server().GetURL("sub.a.com", "/empty.html");

  EXPECT_TRUE(content::NavigateToURL(web_contents(), main_url));
  LoadIFrame(sub_url);

  content::RenderFrameHost* main_document =
      web_contents()->GetPrimaryMainFrame();
  content::RenderFrameHost* sub_document = ChildFrameAt(main_document, 0);

  EXPECT_EQ(true, content::ExecJs(main_document, R"(
    document.domain = "a.com";
    received_memory = undefined;
    addEventListener("message", event => {
      received_memory = event.data;
    });
  )"));

  EXPECT_EQ(true, content::ExecJs(sub_document, R"(
    document.domain = "sub.a.com";
    const memory = new WebAssembly.Memory({
      initial:1,
      maximum:1,
      shared:true
    });
    parent.postMessage(memory, "*");
  )"));

  EXPECT_EQ(1 * kWasmPageSize, content::EvalJs(main_document, R"(
    new Promise(async resolve => {
      while (!received_memory)
        await new Promise(r => setTimeout(r, 10));
      resolve(received_memory.buffer.byteLength);
    });
  )"));

  CheckCounter(WebFeature::kV8SharedArrayBufferConstructedWithoutIsolation, 1);
  CheckCounter(WebFeature::kV8SharedArrayBufferConstructed, 0);
}

IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       WasmMemorySharingCrossOriginIsolated) {
  GURL url =
      https_server().GetURL("a.com",
                            "/set-header"
                            "?Cross-Origin-Opener-Policy: same-origin"
                            "&Cross-Origin-Embedder-Policy: require-corp");
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url));
  LoadIFrame(url);

  content::RenderFrameHost* main_document =
      web_contents()->GetPrimaryMainFrame();
  content::RenderFrameHost* sub_document = ChildFrameAt(main_document, 0);

  EXPECT_EQ(true, content::ExecJs(main_document, R"(
    addEventListener("message", event => {
      received_memory = event.data;
    });
  )"));

  EXPECT_EQ(true, content::ExecJs(sub_document, R"(
    const memory = new WebAssembly.Memory({
      initial:1,
      maximum:1,
      shared:true
    });
    parent.postMessage(memory, "*");
  )"));

  EXPECT_EQ(1 * kWasmPageSize, content::EvalJs(main_document, R"(
    new Promise(async resolve => {
      while (!received_memory)
        await new Promise(r => setTimeout(r, 10));
      resolve(received_memory.buffer.byteLength);
    });
  )"));

  CheckCounter(WebFeature::kV8SharedArrayBufferConstructedWithoutIsolation, 0);
  CheckCounter(WebFeature::kV8SharedArrayBufferConstructed, 1);
}

IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       WasmModuleSharingCrossSite) {
  GURL main_url = https_server().GetURL("a.com", "/empty.html");
  GURL sub_url = https_server().GetURL("b.com", "/empty.html");

  EXPECT_TRUE(content::NavigateToURL(web_contents(), main_url));
  LoadIFrame(sub_url);

  content::RenderFrameHost* main_document =
      web_contents()->GetPrimaryMainFrame();
  content::RenderFrameHost* sub_document = ChildFrameAt(main_document, 0);

  EXPECT_EQ(true, content::ExecJs(main_document, R"(
    received_module = undefined;
    addEventListener("message", event => {
      received_module = event.data;
    });
  )"));

  EXPECT_EQ(true, content::ExecJs(sub_document, R"(
    let module = new WebAssembly.Module(new Uint8Array([
      0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00]));
    parent.postMessage(module, "*");
  )"));

  // It doesn't exist yet a warning or an error being dispatched for failing to
  // send a WebAssembly.Module. This test simply wait.
  EXPECT_EQ("Success: Nothing received", content::EvalJs(main_document, R"(
    new Promise(async resolve => {
      await new Promise(r => setTimeout(r, 1000));
      if (received_module)
        resolve("Failure: Received Webassembly module");
      else
        resolve("Success: Nothing received");
    });
  )"));

  CheckCounter(WebFeature::kV8SharedArrayBufferConstructedWithoutIsolation, 0);
  CheckCounter(WebFeature::kV8SharedArrayBufferConstructed, 0);

  // TODO(ahaas): Check the histogram for:
  // - kWasmModuleSharing
  // - kCrossOriginWasmModuleSharing
}

IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       WasmModuleSharingSameSite) {
  GURL main_url = https_server().GetURL("a.a.com", "/empty.html");
  GURL sub_url = https_server().GetURL("b.a.com", "/empty.html");

  EXPECT_TRUE(content::NavigateToURL(web_contents(), main_url));
  LoadIFrame(sub_url);

  content::RenderFrameHost* main_document =
      web_contents()->GetPrimaryMainFrame();
  content::RenderFrameHost* sub_document = ChildFrameAt(main_document, 0);

  EXPECT_EQ(true, content::ExecJs(main_document, R"(
    received_module = undefined;
    addEventListener("message", event => {
      received_module = event.data;
    });
  )"));

  EXPECT_EQ(true, content::ExecJs(sub_document, R"(
    let module = new WebAssembly.Module(new Uint8Array([
      0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00]));
    parent.postMessage(module, "*");
  )"));

  // It doesn't exist yet a warning or an error being dispatched for failing to
  // send a WebAssembly.Module. This test simply wait.
  EXPECT_EQ("Success: Nothing received", content::EvalJs(main_document, R"(
    new Promise(async resolve => {
      await new Promise(r => setTimeout(r, 1000));
      if (received_module)
        resolve("Failure: Received Webassembly module");
      else
        resolve("Success: Nothing received");
    });
  )"));

  CheckCounter(WebFeature::kV8SharedArrayBufferConstructedWithoutIsolation, 0);
  CheckCounter(WebFeature::kV8SharedArrayBufferConstructed, 0);
}

IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       WasmModuleSharingSameOrigin) {
  GURL main_url = https_server().GetURL("a.com", "/empty.html");
  GURL sub_url = https_server().GetURL("a.com", "/empty.html");

  EXPECT_TRUE(content::NavigateToURL(web_contents(), main_url));
  LoadIFrame(sub_url);

  content::RenderFrameHost* main_document =
      web_contents()->GetPrimaryMainFrame();
  content::RenderFrameHost* sub_document = ChildFrameAt(main_document, 0);

  EXPECT_EQ(true, content::ExecJs(main_document, R"(
    received_module = undefined;
    addEventListener("message", event => {
      received_module = event.data;
    });
  )"));

  EXPECT_EQ(true, content::ExecJs(sub_document, R"(
    let module = new WebAssembly.Module(new Uint8Array([
      0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00]));
    parent.postMessage(module, "*");
  )"));

  EXPECT_EQ(true, content::EvalJs(main_document, R"(
    new Promise(async resolve => {
      while (!received_module)
        await new Promise(r => setTimeout(r, 10));
      resolve(true);
    });
  )"));

  CheckCounter(WebFeature::kV8SharedArrayBufferConstructedWithoutIsolation, 0);
  CheckCounter(WebFeature::kV8SharedArrayBufferConstructed, 0);

  // TODO(ahaas): Check the histogram for:
  // - kWasmModuleSharing
  // - kCrossOriginWasmModuleSharing
}

IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       WasmModuleSharingSameSiteBeforeSetDocumentDomain) {
  GURL main_url = https_server().GetURL("sub.a.com", "/empty.html");
  GURL sub_url = https_server().GetURL("a.com", "/empty.html");

  EXPECT_TRUE(content::NavigateToURL(web_contents(), main_url));
  LoadIFrame(sub_url);

  content::RenderFrameHost* main_document =
      web_contents()->GetPrimaryMainFrame();
  content::RenderFrameHost* sub_document = ChildFrameAt(main_document, 0);

  EXPECT_EQ(true, content::ExecJs(main_document, R"(
    document.domain = "a.com";
    received_module = undefined;
    addEventListener("message", event => {
      received_module = event.data;
    });
  )"));

  EXPECT_EQ(true, content::ExecJs(sub_document, R"(
    document.domain = "a.com";
    let module = new WebAssembly.Module(new Uint8Array([
      0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00]));
    parent.postMessage(module, "*");
  )"));

  // It doesn't exist yet a warning or an error being dispatched for failing to
  // send a WebAssembly.Module. This test simply wait.
  EXPECT_EQ("Success: Nothing received", content::EvalJs(main_document, R"(
    new Promise(async resolve => {
      await new Promise(r => setTimeout(r, 1000));
      if (received_module)
        resolve("Failure: Received Webassembly module");
      else
        resolve("Success: Nothing received");
    });
  )"));

  CheckCounter(WebFeature::kV8SharedArrayBufferConstructedWithoutIsolation, 0);
  CheckCounter(WebFeature::kV8SharedArrayBufferConstructed, 0);
}

IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       WasmModuleSharingSameSiteAfterSetDocumentDomain) {
  GURL main_url = https_server().GetURL("sub.a.com", "/empty.html");
  GURL sub_url = https_server().GetURL("sub.a.com", "/empty.html");

  EXPECT_TRUE(content::NavigateToURL(web_contents(), main_url));
  LoadIFrame(sub_url);

  content::RenderFrameHost* main_document =
      web_contents()->GetPrimaryMainFrame();
  content::RenderFrameHost* sub_document = ChildFrameAt(main_document, 0);

  EXPECT_EQ(true, content::ExecJs(main_document, R"(
    document.domain = "a.com";
    received_module = undefined;
    addEventListener("message", event => {
      received_module = event.data;
    });
  )"));

  EXPECT_EQ(true, content::ExecJs(sub_document, R"(
    document.domain = "sub.a.com";
    let module = new WebAssembly.Module(new Uint8Array([
      0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00]));
    parent.postMessage(module, "*");
  )"));

  EXPECT_EQ(true, content::EvalJs(main_document, R"(
    new Promise(async resolve => {
      while (!received_module)
        await new Promise(r => setTimeout(r, 10));
      resolve(true);
    });
  )"));

  CheckCounter(WebFeature::kV8SharedArrayBufferConstructedWithoutIsolation, 0);
  CheckCounter(WebFeature::kV8SharedArrayBufferConstructed, 0);

  // TODO(ahaas): Check the histogram for:
  // - kWasmModuleSharing
  // - kCrossOriginWasmModuleSharing
}

// Check that two pages with same-origin documents do not get reported when the
// COOP status is the same.
IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       SameOriginDocumentsWithSameCOOPStatus) {
  set_monitored_feature(
      WebFeature::kSameOriginDocumentsWithDifferentCOOPStatus);
  GURL main_document_url = https_server().GetURL("a.com",
                                                 "/set-header?"
                                                 "Cross-Origin-Opener-Policy: "
                                                 "same-origin-allow-popups");
  EXPECT_TRUE(content::NavigateToURL(web_contents(), main_document_url));
  OpenPopup(main_document_url);
  ExpectHistogramIncreasedBy(0);
}

// Check that two pages with same-origin documents do get reported when the
// COOP status is not the same and they are in the same browsing context group.
IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       SameOriginDocumentsWithDifferentCOOPStatus) {
  set_monitored_feature(
      WebFeature::kSameOriginDocumentsWithDifferentCOOPStatus);
  GURL main_document_url = https_server().GetURL("a.com",
                                                 "/set-header?"
                                                 "Cross-Origin-Opener-Policy: "
                                                 "same-origin-allow-popups");
  GURL no_coop_url = https_server().GetURL("a.com", "/empty.html");
  EXPECT_TRUE(content::NavigateToURL(web_contents(), main_document_url));
  OpenPopup(no_coop_url);
  ExpectHistogramIncreasedBy(1);
}

// Check that two pages with same-origin documents do not get reported when the
// COOP status is not the same but they are in different browsing context
// groups.
IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       SameOriginDocumentsWithDifferentCOOPStatusBCGSwitch) {
  set_monitored_feature(
      WebFeature::kSameOriginDocumentsWithDifferentCOOPStatus);
  GURL main_document_url = https_server().GetURL("a.com",
                                                 "/set-header?"
                                                 "Cross-Origin-Opener-Policy: "
                                                 "same-origin-allow-popups");
  GURL coop_same_origin_url =
      https_server().GetURL("a.com",
                            "/set-header?"
                            "Cross-Origin-Opener-Policy: "
                            "same-origin");
  EXPECT_TRUE(content::NavigateToURL(web_contents(), main_document_url));
  OpenPopup(coop_same_origin_url);
  ExpectHistogramIncreasedBy(0);
}

// Check that two pages with two different COOP status are not reported when
// their documents are cross-origin.
IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       CrossOriginDocumentsWithNoCOOPStatus) {
  set_monitored_feature(
      WebFeature::kSameOriginDocumentsWithDifferentCOOPStatus);
  GURL main_document_url = https_server().GetURL("a.com",
                                                 "/set-header?"
                                                 "Cross-Origin-Opener-Policy: "
                                                 "same-origin-allow-popups");
  GURL no_coop_url = https_server().GetURL("b.com", "/empty.html");
  EXPECT_TRUE(content::NavigateToURL(web_contents(), main_document_url));
  OpenPopup(no_coop_url);
  ExpectHistogramIncreasedBy(0);
}

// Check that a COOP same-origin-allow-popups page with a cross-origin iframe
// that opens a popup to the same origin document gets reported.
IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       COOPSameOriginAllowPopupsIframeAndPopup) {
  set_monitored_feature(
      WebFeature::kSameOriginDocumentsWithDifferentCOOPStatus);
  GURL main_document_url = https_server().GetURL("a.com",
                                                 "/set-header?"
                                                 "Cross-Origin-Opener-Policy: "
                                                 "same-origin-allow-popups");
  GURL no_coop_url = https_server().GetURL("b.com", "/empty.html");
  EXPECT_TRUE(content::NavigateToURL(web_contents(), main_document_url));
  LoadIFrame(no_coop_url);
  OpenPopup(no_coop_url);
  ExpectHistogramIncreasedBy(1);
}

// Check that an iframe that is same-origin with its opener of a different COOP
// status gets reported.
IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       SameOriginIframeInCrossOriginPopupWithCOOP) {
  set_monitored_feature(
      WebFeature::kSameOriginDocumentsWithDifferentCOOPStatus);
  GURL main_document_url = https_server().GetURL("a.com",
                                                 "/set-header?"
                                                 "Cross-Origin-Opener-Policy: "
                                                 "same-origin-allow-popups");
  GURL no_coop_url = https_server().GetURL("b.com", "/empty.html");
  GURL same_origin_url = https_server().GetURL("a.com", "/empty.html");
  EXPECT_TRUE(content::NavigateToURL(web_contents(), main_document_url));
  content::WebContents* popup = OpenPopup(no_coop_url);
  LoadIFrameInWebContents(popup, same_origin_url);
  ExpectHistogramIncreasedBy(1);
}

// Check that two same-origin iframes in pages with different COOP status gets
// reported.
IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       IFramesWithDifferentCOOPStatus) {
  set_monitored_feature(
      WebFeature::kSameOriginDocumentsWithDifferentCOOPStatus);
  GURL main_document_url = https_server().GetURL("a.com",
                                                 "/set-header?"
                                                 "Cross-Origin-Opener-Policy: "
                                                 "same-origin-allow-popups");
  GURL popup_url = https_server().GetURL("b.com", "/empty.html");
  GURL iframe_url = https_server().GetURL("c.com", "/empty.html");
  EXPECT_TRUE(content::NavigateToURL(web_contents(), main_document_url));
  LoadIFrame(iframe_url);
  content::WebContents* popup = OpenPopup(popup_url);
  LoadIFrameInWebContents(popup, iframe_url);
  ExpectHistogramIncreasedBy(1);
}

// Check that when two pages both have frames that are same-origin with a
// document in the other page and have different COOP status, the metrics is
// only recorded once.
IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       SameOriginDifferentCOOPStatusRecordedOnce) {
  set_monitored_feature(
      WebFeature::kSameOriginDocumentsWithDifferentCOOPStatus);
  GURL main_document_url = https_server().GetURL("a.com",
                                                 "/set-header?"
                                                 "Cross-Origin-Opener-Policy: "
                                                 "same-origin-allow-popups");
  GURL popup_url = https_server().GetURL("b.com", "/empty.html");
  GURL same_origin_url = https_server().GetURL("a.com", "/empty.html");
  EXPECT_TRUE(content::NavigateToURL(web_contents(), main_document_url));
  content::WebContents* popup = OpenPopup(popup_url);
  LoadIFrame(popup_url);
  LoadIFrameInWebContents(popup, same_origin_url);
  ExpectHistogramIncreasedBy(1);
}

// Check that when two pages COOP same-origin-allow-popups have frames that are
// same-origin with a COOP unsafe-none, the metrcis is recorded twice (once per
// COOP same-origin-allow-popups page).
IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       SameOriginDifferentCOOPStatusTwoCOOPPages) {
  set_monitored_feature(
      WebFeature::kSameOriginDocumentsWithDifferentCOOPStatus);
  GURL main_document_url = https_server().GetURL("a.com",
                                                 "/set-header?"
                                                 "Cross-Origin-Opener-Policy: "
                                                 "same-origin-allow-popups");
  GURL same_origin_url = https_server().GetURL("a.com", "/empty.html");
  EXPECT_TRUE(content::NavigateToURL(web_contents(), main_document_url));
  OpenPopup(main_document_url);
  OpenPopup(same_origin_url);
  ExpectHistogramIncreasedBy(2);
}

IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       CoepNoneMainFrame) {
  GURL url = https_server().GetURL("a.com",
                                   "/set-header?"
                                   "Cross-Origin-Embedder-Policy: unsafe-none");
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url));
  CheckCounter(WebFeature::kCrossOriginEmbedderPolicyRequireCorp, 0);
  CheckCounter(WebFeature::kCrossOriginEmbedderPolicyCredentialless, 0);
  CheckCounter(WebFeature::kCrossOriginEmbedderPolicyRequireCorpReportOnly, 0);
  CheckCounter(WebFeature::kCrossOriginEmbedderPolicyCredentiallessReportOnly,
               0);
}

IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       CoepCredentiallessMainFrame) {
  GURL url =
      https_server().GetURL("a.com",
                            "/set-header?"
                            "Cross-Origin-Embedder-Policy: credentialless");
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url));
  CheckCounter(WebFeature::kCrossOriginEmbedderPolicyRequireCorp, 0);
  CheckCounter(WebFeature::kCrossOriginEmbedderPolicyCredentialless, 1);
  CheckCounter(WebFeature::kCrossOriginEmbedderPolicyRequireCorpReportOnly, 0);
  CheckCounter(WebFeature::kCrossOriginEmbedderPolicyCredentiallessReportOnly,
               0);

  CheckCounterMainFrame(WebFeature::kCrossOriginEmbedderPolicyCredentialless,
                        1);
}

IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       CoepRequireCorpMainFrame) {
  GURL url =
      https_server().GetURL("a.com",
                            "/set-header?"
                            "Cross-Origin-Embedder-Policy: require-corp");
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url));
  CheckCounter(WebFeature::kCrossOriginEmbedderPolicyRequireCorp, 1);
  CheckCounter(WebFeature::kCrossOriginEmbedderPolicyCredentialless, 0);
  CheckCounter(WebFeature::kCrossOriginEmbedderPolicyRequireCorpReportOnly, 0);
  CheckCounter(WebFeature::kCrossOriginEmbedderPolicyCredentiallessReportOnly,
               0);

  CheckCounterMainFrame(WebFeature::kCrossOriginEmbedderPolicyRequireCorp, 1);
}

IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       CoepReportOnlyCredentiallessMainFrame) {
  GURL url = https_server().GetURL(
      "a.com",
      "/set-header?"
      "Cross-Origin-Embedder-Policy-Report-Only: credentialless");
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url));
  CheckCounter(WebFeature::kCrossOriginEmbedderPolicyRequireCorp, 0);
  CheckCounter(WebFeature::kCrossOriginEmbedderPolicyCredentialless, 0);
  CheckCounter(WebFeature::kCrossOriginEmbedderPolicyRequireCorpReportOnly, 0);
  CheckCounter(WebFeature::kCrossOriginEmbedderPolicyCredentiallessReportOnly,
               1);
}

IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       CoepReportOnlyRequireCorpMainFrame) {
  GURL url = https_server().GetURL(
      "a.com",
      "/set-header?"
      "Cross-Origin-Embedder-Policy-Report-Only: require-corp");
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url));
  CheckCounter(WebFeature::kCrossOriginEmbedderPolicyRequireCorp, 0);
  CheckCounter(WebFeature::kCrossOriginEmbedderPolicyCredentialless, 0);
  CheckCounter(WebFeature::kCrossOriginEmbedderPolicyRequireCorpReportOnly, 1);
  CheckCounter(WebFeature::kCrossOriginEmbedderPolicyCredentiallessReportOnly,
               0);
}

IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       CoopAndCoepIsolatedMainFrame) {
  GURL url =
      https_server().GetURL("a.com",
                            "/set-header?"
                            "Cross-Origin-Embedder-Policy: credentialless&"
                            "Cross-Origin-Opener-Policy: same-origin");
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url));
  CheckCounter(WebFeature::kCoopAndCoepIsolated, 1);
  CheckCounter(WebFeature::kCoopAndCoepIsolatedReportOnly, 0);
}

IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       CoopAndCoepIsolatedEnforcedReportOnlyMainFrame) {
  GURL url = https_server().GetURL(
      "a.com",
      "/set-header?"
      "Cross-Origin-Embedder-Policy: credentialless&"
      "Cross-Origin-Embedder-Policy-Report-Only: credentialless&"
      "Cross-Origin-Opener-Policy: same-origin&"
      "Cross-Origin-Opener-Policy-Report-Only: same-origin");
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url));
  CheckCounter(WebFeature::kCoopAndCoepIsolated, 1);
  CheckCounter(WebFeature::kCoopAndCoepIsolatedReportOnly, 1);
}

IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       CoopAndCoepIsolatedMainFrameReportOnly) {
  GURL url = https_server().GetURL(
      "a.com",
      "/set-header?"
      "Cross-Origin-Embedder-Policy: credentialless&"
      "Cross-Origin-Opener-Policy-Report-Only: same-origin");
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url));
  CheckCounter(WebFeature::kCoopAndCoepIsolated, 0);
  CheckCounter(WebFeature::kCoopAndCoepIsolatedReportOnly, 1);
}

IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       CoopAndCoepIsolatedIframe) {
  GURL main_url = https_server().GetURL("a.com", "/set-header?");
  EXPECT_TRUE(content::NavigateToURL(web_contents(), main_url));
  GURL child_url =
      https_server().GetURL("a.com",
                            "/set-header?"
                            "Cross-Origin-Embedder-Policy: credentialless&"
                            "Cross-Origin-Opener-Policy: same-origin");
  LoadIFrame(child_url);
  EXPECT_TRUE(content::WaitForLoadStop(web_contents()));
  CheckCounter(WebFeature::kCoopAndCoepIsolated, 0);
  CheckCounter(WebFeature::kCoopAndCoepIsolatedReportOnly, 0);
}

IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       CoepRequireCorpEmbedsCredentialless) {
  GURL main_url =
      https_server().GetURL("a.com",
                            "/set-header?"
                            "Cross-Origin-Embedder-Policy: require-corp");
  EXPECT_TRUE(content::NavigateToURL(web_contents(), main_url));
  CheckCounter(WebFeature::kCrossOriginEmbedderPolicyCredentialless, 0);
  CheckCounter(WebFeature::kCrossOriginEmbedderPolicyRequireCorp, 1);
  GURL child_url =
      https_server().GetURL("a.com",
                            "/set-header?"
                            "Cross-Origin-Embedder-Policy: credentialless");
  LoadIFrame(child_url);
  EXPECT_TRUE(content::WaitForLoadStop(web_contents()));
  CheckCounter(WebFeature::kCrossOriginEmbedderPolicyCredentialless, 1);
  CheckCounter(WebFeature::kCrossOriginEmbedderPolicyRequireCorp, 1);
  CheckCounterMainFrame(WebFeature::kCrossOriginEmbedderPolicyCredentialless,
                        0);
  CheckCounterMainFrame(WebFeature::kCrossOriginEmbedderPolicyRequireCorp, 1);
}

IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       CoepCredentiallessEmbedsRequireCorp) {
  GURL main_url =
      https_server().GetURL("a.com",
                            "/set-header?"
                            "Cross-Origin-Embedder-Policy: credentialless");
  EXPECT_TRUE(content::NavigateToURL(web_contents(), main_url));
  CheckCounter(WebFeature::kCrossOriginEmbedderPolicyCredentialless, 1);
  CheckCounter(WebFeature::kCrossOriginEmbedderPolicyRequireCorp, 0);
  GURL child_url =
      https_server().GetURL("a.com",
                            "/set-header?"
                            "Cross-Origin-Embedder-Policy: require-corp");
  LoadIFrame(child_url);
  CheckCounter(WebFeature::kCrossOriginEmbedderPolicyCredentialless, 1);
  CheckCounter(WebFeature::kCrossOriginEmbedderPolicyRequireCorp, 1);
  CheckCounterMainFrame(WebFeature::kCrossOriginEmbedderPolicyCredentialless,
                        1);
  CheckCounterMainFrame(WebFeature::kCrossOriginEmbedderPolicyRequireCorp, 0);
}

IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       CoepNoneSharedWorker) {
  GURL main_page_url = https_server().GetURL("a.test", "/empty.html");
  GURL worker_url =
      https_server().GetURL("a.test",
                            "/set-header?"
                            "Cross-Origin-Embedder-Policy: unsafe-none");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), main_page_url));
  EXPECT_TRUE(content::ExecJs(
      web_contents(),
      content::JsReplace("worker = new SharedWorker($1)", worker_url)));
  CheckCounter(WebFeature::kCoepNoneSharedWorker, 1);
  CheckCounter(WebFeature::kCoepCredentiallessSharedWorker, 0);
  CheckCounter(WebFeature::kCoepRequireCorpSharedWorker, 0);
}

IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       CoepCredentiallessSharedWorker) {
  GURL main_page_url = https_server().GetURL("a.test", "/empty.html");
  GURL worker_url =
      https_server().GetURL("a.test",
                            "/set-header?"
                            "Cross-Origin-Embedder-Policy: credentialless");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), main_page_url));
  EXPECT_TRUE(content::ExecJs(
      web_contents(),
      content::JsReplace("worker = new SharedWorker($1)", worker_url)));
  CheckCounter(WebFeature::kCoepNoneSharedWorker, 0);
  CheckCounter(WebFeature::kCoepCredentiallessSharedWorker, 1);
  CheckCounter(WebFeature::kCoepRequireCorpSharedWorker, 0);
}

IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       CoepRequireCorpSharedWorker) {
  GURL main_page_url = https_server().GetURL("a.test", "/empty.html");
  GURL worker_url =
      https_server().GetURL("a.test",
                            "/set-header?"
                            "Cross-Origin-Embedder-Policy: require-corp");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), main_page_url));
  EXPECT_TRUE(content::ExecJs(
      web_contents(),
      content::JsReplace("worker = new SharedWorker($1)", worker_url)));
  CheckCounter(WebFeature::kCoepNoneSharedWorker, 0);
  CheckCounter(WebFeature::kCoepCredentiallessSharedWorker, 0);
  CheckCounter(WebFeature::kCoepRequireCorpSharedWorker, 1);
}

IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       WindowProxyAccess) {
  GURL url =
      https_server().GetURL("a.com", "/cross_site_iframe_factory.html?a(a,b)");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
  content::RenderFrameHost* same_origin_subframe =
      ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);
  content::RenderFrameHost* cross_origin_subframe =
      ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 1);

  struct TestCase {
    const char* name;
    const char* property;
    WebFeature property_access;
    WebFeature property_access_from_other_page;
  } cases[] = {
      {
          "blur",
          "window.top.blur()",
          WebFeature::kWindowProxyCrossOriginAccessBlur,
          WebFeature::kWindowProxyCrossOriginAccessFromOtherPageBlur,
      },
      {
          "closed",
          "window.top.closed",
          WebFeature::kWindowProxyCrossOriginAccessClosed,
          WebFeature::kWindowProxyCrossOriginAccessFromOtherPageClosed,
      },
      {
          "focus",
          "window.top.focus()",
          WebFeature::kWindowProxyCrossOriginAccessFocus,
          WebFeature::kWindowProxyCrossOriginAccessFromOtherPageFocus,
      },
      {
          "frames",
          "window.top.frames",
          WebFeature::kWindowProxyCrossOriginAccessFrames,
          WebFeature::kWindowProxyCrossOriginAccessFromOtherPageFrames,
      },
      {
          "length",
          "window.top.length",
          WebFeature::kWindowProxyCrossOriginAccessLength,
          WebFeature::kWindowProxyCrossOriginAccessFromOtherPageLength,
      },
      {
          "location get",
          "window.top.location",
          WebFeature::kWindowProxyCrossOriginAccessLocation,
          WebFeature::kWindowProxyCrossOriginAccessFromOtherPageLocation,
      },
      {
          "opener get",
          "window.top.opener",
          WebFeature::kWindowProxyCrossOriginAccessOpener,
          WebFeature::kWindowProxyCrossOriginAccessFromOtherPageOpener,
      },
      {
          "parent",
          "window.top.parent",
          WebFeature::kWindowProxyCrossOriginAccessParent,
          WebFeature::kWindowProxyCrossOriginAccessFromOtherPageParent,
      },
      {
          "postMessage",
          "window.top.postMessage('','*')",
          WebFeature::kWindowProxyCrossOriginAccessPostMessage,
          WebFeature::kWindowProxyCrossOriginAccessFromOtherPagePostMessage,
      },
      {
          "self",
          "window.top.self",
          WebFeature::kWindowProxyCrossOriginAccessSelf,
          WebFeature::kWindowProxyCrossOriginAccessFromOtherPageSelf,
      },
      {
          "top",
          "window.top.top",
          WebFeature::kWindowProxyCrossOriginAccessTop,
          WebFeature::kWindowProxyCrossOriginAccessFromOtherPageTop,
      },
      {
          "window",
          "window.top.window",
          WebFeature::kWindowProxyCrossOriginAccessWindow,
          WebFeature::kWindowProxyCrossOriginAccessFromOtherPageWindow,
      }};

  for (auto test : cases) {
    SCOPED_TRACE(test.name);
    // Check that a same-origin access does not register use counters.
    EXPECT_TRUE(content::ExecJs(same_origin_subframe, test.property));
    CheckCounter(test.property_access, 0);
    CheckCounter(test.property_access_from_other_page, 0);

    // Check that a cross-origin access register use counters.
    EXPECT_TRUE(content::ExecJs(cross_origin_subframe, test.property));
    CheckCounter(test.property_access, 1);
    CheckCounter(test.property_access_from_other_page, 0);
  }
}

IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       WindowProxyAccessCloseSameOrigin) {
  GURL url =
      https_server().GetURL("a.com", "/cross_site_iframe_factory.html?a(a)");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // Check that a same-origin access does not register use counters.
  content::RenderFrameHost* same_origin_subframe =
      ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);
  EXPECT_TRUE(content::ExecJs(same_origin_subframe, "window.top.close()"));
  CheckCounter(WebFeature::kWindowProxyCrossOriginAccessClose, 0);
  CheckCounter(WebFeature::kWindowProxyCrossOriginAccessFromOtherPageClose, 0);
}

IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       WindowProxyAccessCloseCrossOrigin) {
  GURL url =
      https_server().GetURL("a.com", "/cross_site_iframe_factory.html?a(b)");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // Check that a cross-origin access register use counters.
  content::RenderFrameHost* cross_origin_subframe =
      ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);
  EXPECT_TRUE(content::ExecJs(cross_origin_subframe, "window.top.close()"));
  CheckCounter(WebFeature::kWindowProxyCrossOriginAccessClose, 1);
  CheckCounter(WebFeature::kWindowProxyCrossOriginAccessFromOtherPageClose, 0);
}

IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       WindowProxyAccessIndexedGetter) {
  GURL url =
      https_server().GetURL("a.com", "/cross_site_iframe_factory.html?a(a,b)");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // Check that a same-origin access does not register use counters.
  content::RenderFrameHost* same_origin_subframe =
      ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);
  EXPECT_TRUE(content::ExecJs(same_origin_subframe, "window.top[0]"));
  CheckCounter(WebFeature::kWindowProxyCrossOriginAccessIndexedGetter, 0);
  CheckCounter(
      WebFeature::kWindowProxyCrossOriginAccessFromOtherPageIndexedGetter, 0);

  // Check that a cross-origin access register use counters.
  content::RenderFrameHost* cross_origin_subframe =
      ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 1);
  EXPECT_TRUE(content::ExecJs(cross_origin_subframe, "window.top[0]"));
  CheckCounter(WebFeature::kWindowProxyCrossOriginAccessIndexedGetter, 1);
  CheckCounter(
      WebFeature::kWindowProxyCrossOriginAccessFromOtherPageIndexedGetter, 0);

  // A failed access should not register the use counter.
  EXPECT_FALSE(content::ExecJs(cross_origin_subframe, "window.top[2]"));
  CheckCounter(WebFeature::kWindowProxyCrossOriginAccessIndexedGetter, 1);
  CheckCounter(
      WebFeature::kWindowProxyCrossOriginAccessFromOtherPageIndexedGetter, 0);
}

IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       WindowProxyAccessLocationSetSameOrigin) {
  GURL url =
      https_server().GetURL("a.com", "/cross_site_iframe_factory.html?a(a,b)");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // Check that a same-origin access does not register use counters.
  content::RenderFrameHost* same_origin_subframe =
      ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);
  EXPECT_TRUE(
      content::ExecJs(same_origin_subframe,
                      content::JsReplace("window.top.location = $1", url)));
  CheckCounter(WebFeature::kWindowProxyCrossOriginAccessLocation, 0);
  CheckCounter(WebFeature::kWindowProxyCrossOriginAccessFromOtherPageLocation,
               0);
}

IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       WindowProxyAccessLocationSetCrossOrigin) {
  GURL url =
      https_server().GetURL("a.com", "/cross_site_iframe_factory.html?a(a,b)");
  GURL fragment_url = https_server().GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a,b)#foo");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // Check that a cross-origin access register use counters.
  content::RenderFrameHost* cross_origin_subframe =
      ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 1);
  EXPECT_TRUE(content::ExecJs(
      cross_origin_subframe,
      content::JsReplace("window.top.location = $1", fragment_url)));

  CheckCounter(WebFeature::kWindowProxyCrossOriginAccessLocation, 1);
  CheckCounter(WebFeature::kWindowProxyCrossOriginAccessFromOtherPageLocation,
               0);
}

IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       WindowProxyAccessNamedGetter) {
  GURL url = https_server().GetURL("a.test", "/iframe_about_blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
  GURL cross_origin_url = https_server().GetURL("b.test", "/empty.html");
  LoadIFrame(cross_origin_url);

  // Check that a same-origin access does not register use counters.
  content::RenderFrameHost* same_origin_subframe =
      ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);
  EXPECT_TRUE(content::ExecJs(same_origin_subframe,
                              "window.top['about_blank_iframe']"));
  CheckCounter(WebFeature::kWindowProxyCrossOriginAccessNamedGetter, 0);
  CheckCounter(
      WebFeature::kWindowProxyCrossOriginAccessFromOtherPageNamedGetter, 0);

  // Check that a cross-origin access register use counters.
  content::RenderFrameHost* cross_origin_subframe =
      ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 1);
  EXPECT_TRUE(content::ExecJs(cross_origin_subframe,
                              "window.top['about_blank_iframe']"));
  CheckCounter(WebFeature::kWindowProxyCrossOriginAccessNamedGetter, 1);
  CheckCounter(
      WebFeature::kWindowProxyCrossOriginAccessFromOtherPageNamedGetter, 0);

  // A failed access should not register the use counter.
  EXPECT_FALSE(
      content::ExecJs(cross_origin_subframe, "window.top['wrongName']"));
  CheckCounter(WebFeature::kWindowProxyCrossOriginAccessNamedGetter, 1);
  CheckCounter(
      WebFeature::kWindowProxyCrossOriginAccessFromOtherPageNamedGetter, 0);
}

IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       WindowProxyAccessOpenerSet) {
  GURL url =
      https_server().GetURL("a.com", "/cross_site_iframe_factory.html?a(a,b)");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // Check that a same-origin access does not register use counters.
  content::RenderFrameHost* same_origin_subframe =
      ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);
  EXPECT_TRUE(content::ExecJs(same_origin_subframe, "window.top.opener = ''"));
  CheckCounter(WebFeature::kWindowProxyCrossOriginAccessOpener, 0);
  CheckCounter(WebFeature::kWindowProxyCrossOriginAccessFromOtherPageOpener, 0);

  // Check that a cross-origin access doesn't register use counters because it
  // is blocked by the same-origin policy.
  content::RenderFrameHost* cross_origin_subframe =
      ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 1);
  EXPECT_FALSE(
      content::ExecJs(cross_origin_subframe, "window.top.opener = ''"));
  CheckCounter(WebFeature::kWindowProxyCrossOriginAccessOpener, 0);
  CheckCounter(WebFeature::kWindowProxyCrossOriginAccessFromOtherPageOpener, 0);
}

IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       WindowProxyAccessFromOtherPage) {
  GURL url = https_server().GetURL("a.com", "/empty.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  content::WebContents* same_origin_popup = OpenPopup(url);

  GURL cross_origin_url = https_server().GetURL("b.test", "/empty.html");
  content::WebContents* cross_origin_popup = OpenPopup(cross_origin_url);

  struct TestCase {
    const char* name;
    const char* property;
    WebFeature property_access;
    WebFeature property_access_from_other_page;
  } cases[] = {
      {
          "blur",
          "window.opener.blur()",
          WebFeature::kWindowProxyCrossOriginAccessBlur,
          WebFeature::kWindowProxyCrossOriginAccessFromOtherPageBlur,
      },
      {
          "closed",
          "window.opener.closed",
          WebFeature::kWindowProxyCrossOriginAccessClosed,
          WebFeature::kWindowProxyCrossOriginAccessFromOtherPageClosed,
      },
      {
          "focus",
          "window.opener.focus()",
          WebFeature::kWindowProxyCrossOriginAccessFocus,
          WebFeature::kWindowProxyCrossOriginAccessFromOtherPageFocus,
      },
      {
          "frames",
          "window.opener.frames",
          WebFeature::kWindowProxyCrossOriginAccessFrames,
          WebFeature::kWindowProxyCrossOriginAccessFromOtherPageFrames,
      },
      {
          "length",
          "window.opener.length",
          WebFeature::kWindowProxyCrossOriginAccessLength,
          WebFeature::kWindowProxyCrossOriginAccessFromOtherPageLength,
      },
      {
          "location get",
          "window.opener.location",
          WebFeature::kWindowProxyCrossOriginAccessLocation,
          WebFeature::kWindowProxyCrossOriginAccessFromOtherPageLocation,
      },
      {
          "opener get",
          "window.opener.opener",
          WebFeature::kWindowProxyCrossOriginAccessOpener,
          WebFeature::kWindowProxyCrossOriginAccessFromOtherPageOpener,
      },
      {
          "parent",
          "window.opener.parent",
          WebFeature::kWindowProxyCrossOriginAccessParent,
          WebFeature::kWindowProxyCrossOriginAccessFromOtherPageParent,
      },
      {
          "postMessage",
          "window.opener.postMessage('','*')",
          WebFeature::kWindowProxyCrossOriginAccessPostMessage,
          WebFeature::kWindowProxyCrossOriginAccessFromOtherPagePostMessage,
      },
      {
          "self",
          "window.opener.self",
          WebFeature::kWindowProxyCrossOriginAccessSelf,
          WebFeature::kWindowProxyCrossOriginAccessFromOtherPageSelf,
      },
      {
          "top",
          "window.opener.top",
          WebFeature::kWindowProxyCrossOriginAccessTop,
          WebFeature::kWindowProxyCrossOriginAccessFromOtherPageTop,
      }};

  for (auto test : cases) {
    SCOPED_TRACE(test.name);
    // Check that a same-origin access does not register use counters.
    EXPECT_TRUE(content::ExecJs(same_origin_popup, test.property));
    CheckCounter(test.property_access, 0);
    CheckCounter(test.property_access_from_other_page, 0);

    // Check that a cross-origin access register use counters.
    EXPECT_TRUE(content::ExecJs(cross_origin_popup, test.property));
    CheckCounter(test.property_access, 1);
    CheckCounter(test.property_access_from_other_page, 1);
  }
}

IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       WindowProxyAccessFromOtherPageCloseSameOrigin) {
  GURL url = https_server().GetURL("a.test", "/empty.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // Check that a same-origin access does not register use counters.
  content::WebContents* same_origin_popup = OpenPopup(url);
  EXPECT_TRUE(content::ExecJs(same_origin_popup, "window.opener.close()"));
  CheckCounter(WebFeature::kWindowProxyCrossOriginAccessClose, 0);
  CheckCounter(WebFeature::kWindowProxyCrossOriginAccessFromOtherPageClose, 0);
}

IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       WindowProxyAccessFromOtherPageCloseCrossOrigin) {
  GURL url = https_server().GetURL("a.test", "/empty.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // Check that a cross-origin access register use counters.
  GURL cross_origin_url = https_server().GetURL("b.test", "/empty.html");
  content::WebContents* cross_origin_popup = OpenPopup(cross_origin_url);
  EXPECT_TRUE(content::ExecJs(cross_origin_popup, "window.opener.close()"));
  CheckCounter(WebFeature::kWindowProxyCrossOriginAccessClose, 1);
  CheckCounter(WebFeature::kWindowProxyCrossOriginAccessFromOtherPageClose, 1);
}

IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       WindowProxyAccessFromOtherPageIndexedGetter) {
  GURL url = https_server().GetURL("a.test", "/iframe.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // Check that a same-origin access does not register use counters.
  content::WebContents* same_origin_popup = OpenPopup(url);
  EXPECT_TRUE(content::ExecJs(same_origin_popup, "window.opener[0]"));
  CheckCounter(WebFeature::kWindowProxyCrossOriginAccessIndexedGetter, 0);
  CheckCounter(
      WebFeature::kWindowProxyCrossOriginAccessFromOtherPageIndexedGetter, 0);

  // Check that a cross-origin access register use counters.
  GURL cross_origin_url = https_server().GetURL("b.test", "/empty.html");
  content::WebContents* cross_origin_popup = OpenPopup(cross_origin_url);
  EXPECT_TRUE(content::ExecJs(cross_origin_popup, "window.opener[0]"));
  CheckCounter(WebFeature::kWindowProxyCrossOriginAccessIndexedGetter, 1);
  CheckCounter(
      WebFeature::kWindowProxyCrossOriginAccessFromOtherPageIndexedGetter, 1);

  // A failed access should not register the use counter.
  EXPECT_FALSE(content::ExecJs(cross_origin_popup, "window.opener[1]"));
  CheckCounter(WebFeature::kWindowProxyCrossOriginAccessIndexedGetter, 1);
  CheckCounter(
      WebFeature::kWindowProxyCrossOriginAccessFromOtherPageIndexedGetter, 1);
}

IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       WindowProxyAccessFromOtherPageLocationSetSameOrigin) {
  GURL url = https_server().GetURL("a.test", "/empty.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // Check that a same-origin access does not register use counters.
  content::WebContents* same_origin_popup = OpenPopup(url);
  EXPECT_TRUE(
      content::ExecJs(same_origin_popup,
                      content::JsReplace("window.opener.location = $1", url)));
  CheckCounter(WebFeature::kWindowProxyCrossOriginAccessLocation, 0);
  CheckCounter(WebFeature::kWindowProxyCrossOriginAccessFromOtherPageLocation,
               0);
}

IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       WindowProxyAccessFromOtherPageLocationSetCrossOrigin) {
  GURL url = https_server().GetURL("a.test", "/empty.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // Check that a cross-origin access register use counters.
  GURL cross_origin_url = https_server().GetURL("b.test", "/empty.html");
  content::WebContents* cross_origin_popup = OpenPopup(cross_origin_url);
  EXPECT_TRUE(
      content::ExecJs(cross_origin_popup,
                      content::JsReplace("window.opener.location = $1", url)));
  CheckCounter(WebFeature::kWindowProxyCrossOriginAccessLocation, 1);
  CheckCounter(WebFeature::kWindowProxyCrossOriginAccessFromOtherPageLocation,
               1);
}

IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       WindowProxyAccessFromOtherPageNamedGetter) {
  GURL url = https_server().GetURL("a.test", "/iframe_about_blank.html");
  GURL cross_origin_url = https_server().GetURL("b.test", "/empty.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // Check that a same-origin access does not register use counters.
  content::WebContents* same_origin_popup = OpenPopup(url);
  EXPECT_TRUE(content::ExecJs(same_origin_popup,
                              "window.opener['about_blank_iframe']"));
  CheckCounter(WebFeature::kWindowProxyCrossOriginAccessNamedGetter, 0);
  CheckCounter(
      WebFeature::kWindowProxyCrossOriginAccessFromOtherPageNamedGetter, 0);

  // Check that a cross-origin access register use counters.
  content::WebContents* cross_origin_popup = OpenPopup(cross_origin_url);
  EXPECT_TRUE(content::ExecJs(cross_origin_popup,
                              "window.opener['about_blank_iframe']"));
  CheckCounter(WebFeature::kWindowProxyCrossOriginAccessNamedGetter, 1);
  CheckCounter(
      WebFeature::kWindowProxyCrossOriginAccessFromOtherPageNamedGetter, 1);

  // A failed access should not register the use counter.
  EXPECT_FALSE(
      content::ExecJs(cross_origin_popup, "window.opener['wrongName']"));
  CheckCounter(WebFeature::kWindowProxyCrossOriginAccessNamedGetter, 1);
  CheckCounter(
      WebFeature::kWindowProxyCrossOriginAccessFromOtherPageNamedGetter, 1);
}

IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       WindowProxyAccessFromOtherPageOpenerSet) {
  GURL url = https_server().GetURL("a.test", "/empty.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // Check that a same-origin access does not register use counters.
  content::WebContents* same_origin_popup = OpenPopup(url);
  EXPECT_TRUE(content::ExecJs(same_origin_popup, "window.opener.opener = ''"));
  CheckCounter(WebFeature::kWindowProxyCrossOriginAccessOpener, 0);
  CheckCounter(WebFeature::kWindowProxyCrossOriginAccessFromOtherPageOpener, 0);

  // Check that a cross-origin access doesn't register use counters because it
  // is blocked by the same-origin policy.
  GURL cross_origin_url = https_server().GetURL("b.test", "/empty.html");
  content::WebContents* cross_origin_popup = OpenPopup(cross_origin_url);
  EXPECT_FALSE(
      content::ExecJs(cross_origin_popup, "window.opener.opener = ''"));
  CheckCounter(WebFeature::kWindowProxyCrossOriginAccessOpener, 0);
  CheckCounter(WebFeature::kWindowProxyCrossOriginAccessFromOtherPageOpener, 0);
}

IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       WindowProxyAccessFromOtherPageWindow) {
  GURL url = https_server().GetURL("a.test", "/empty.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // Check that a same-origin access does not register use counters.
  content::WebContents* same_origin_popup = OpenPopup(url);
  EXPECT_TRUE(content::ExecJs(same_origin_popup, "window.opener.window"));
  CheckCounter(WebFeature::kWindowProxyCrossOriginAccessWindow, 0);
  CheckCounter(WebFeature::kWindowProxyCrossOriginAccessFromOtherPageWindow, 0);

  // Check that a cross-origin access register use counters.
  GURL cross_origin_url = https_server().GetURL("b.test", "/empty.html");
  content::WebContents* cross_origin_popup = OpenPopup(cross_origin_url);
  EXPECT_TRUE(content::ExecJs(cross_origin_popup, "window.opener.window"));
  CheckCounter(WebFeature::kWindowProxyCrossOriginAccessWindow, 1);
  CheckCounter(WebFeature::kWindowProxyCrossOriginAccessFromOtherPageWindow, 1);
}

IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       AnonymousIframeInitialEmptyDocumentControl) {
  GURL url = https_server().GetURL("a.test", "/empty.html");
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url));
  EXPECT_TRUE(content::ExecJs(web_contents(), R"(
    const iframe = document.createElement("iframe");
    iframe.credentialless = false;
    document.body.appendChild(iframe);
  )"));
  CheckCounter(WebFeature::kAnonymousIframe, 0);
  CheckHistogramCount("Navigation.AnonymousIframeIsSandboxed", false, 0);
  CheckHistogramCount("Navigation.AnonymousIframeIsSandboxed", true, 0);
}

IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       AnonymousIframeInitialEmptyDocument) {
  GURL url = https_server().GetURL("a.test", "/empty.html");
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url));
  EXPECT_TRUE(content::ExecJs(web_contents(), R"(
    const iframe = document.createElement("iframe");
    iframe.credentialless = true;
    document.body.appendChild(iframe);
  )"));
  CheckCounter(WebFeature::kAnonymousIframe, 1);
  CheckHistogramCount("Navigation.AnonymousIframeIsSandboxed", false, 1);
  CheckHistogramCount("Navigation.AnonymousIframeIsSandboxed", true, 0);
}

IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       AnonymousIframeNavigationControl) {
  GURL url = https_server().GetURL("a.test", "/empty.html");
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url));
  EXPECT_TRUE(content::ExecJs(web_contents(), R"(
    new Promise(resolve => {
      let iframe = document.createElement("iframe");
      iframe.src = location.href;
      iframe.credentialless = false;
      iframe.onload = resolve;
      document.body.appendChild(iframe);
    });
  )"));
  CheckCounter(WebFeature::kAnonymousIframe, 0);
  CheckHistogramCount("Navigation.AnonymousIframeIsSandboxed", false, 0);
  CheckHistogramCount("Navigation.AnonymousIframeIsSandboxed", true, 0);
}

IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       AnonymousIframeNavigation) {
  GURL url = https_server().GetURL("a.test", "/empty.html");
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url));
  EXPECT_TRUE(content::ExecJs(web_contents(), R"(
    new Promise(resolve => {
      let iframe = document.createElement("iframe");
      iframe.src = location.href;
      iframe.credentialless = true;
      iframe.onload = resolve;
      document.body.appendChild(iframe);
    });
  )"));
  CheckCounter(WebFeature::kAnonymousIframe, 1);
  CheckHistogramCount("Navigation.AnonymousIframeIsSandboxed", false, 1);
  CheckHistogramCount("Navigation.AnonymousIframeIsSandboxed", true, 0);
}

IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       AnonymousIframeIsSandboxedControl) {
  GURL url = https_server().GetURL("a.test", "/empty.html");
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url));
  EXPECT_TRUE(content::ExecJs(web_contents(), R"(
    new Promise(resolve => {
      let iframe = document.createElement("iframe");
      iframe.src = location.href;
      iframe.onload = resolve;
      document.body.appendChild(iframe);
    });
  )"));
  CheckCounter(WebFeature::kAnonymousIframe, 0);
  CheckHistogramCount("Navigation.AnonymousIframeIsSandboxed", false, 0);
  CheckHistogramCount("Navigation.AnonymousIframeIsSandboxed", true, 0);
}

IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       AnonymousIframeIsSandboxed) {
  GURL url = https_server().GetURL("a.test", "/empty.html");
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url));
  EXPECT_TRUE(content::ExecJs(web_contents(), R"(
    const createIframe = sandbox => {
      let iframe = document.createElement("iframe");
      iframe.src = location.href;
      iframe.credentialless = true;
      if (sandbox)
        iframe.sandbox = "";
      document.body.appendChild(iframe);
      return new Promise(resolve => iframe.onload = resolve);
    };
    Promise.all([
      createIframe(false),
      createIframe(false),
      createIframe(false),
      createIframe(true),
      createIframe(true),
    ]);
  )"));
  CheckCounter(WebFeature::kAnonymousIframe, 1);
  CheckHistogramCount("Navigation.AnonymousIframeIsSandboxed", false, 3);
  CheckHistogramCount("Navigation.AnonymousIframeIsSandboxed", true, 2);
}

IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest, BlobUrl) {
  GURL url = https_server().GetURL("a.test", "/empty.html");
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url));
  EXPECT_TRUE(content::ExecJs(web_contents(), R"(
    new Promise(resolve => {
      const iframe = document.createElement("iframe");
      const blob = new Blob(["test"], {type: "text/html"});
      const url = URL.createObjectURL(blob);
      iframe.src = url;
      iframe.onload = resolve;
      document.body.appendChild(iframe);
    });
  )"));
  CheckHistogramCount("Navigation.BlobUrl", true, 1);
  CheckHistogramCount("Navigation.BlobUrl", false, 3);
  CheckHistogramCount("Navigation.BlobUrl.MainFrame", true, 0);
  CheckHistogramCount("Navigation.BlobUrl.MainFrame", false, 1);
  CheckHistogramCount("Navigation.BlobUrl.Sandboxed", true, 0);
  CheckHistogramCount("Navigation.BlobUrl.Sandboxed", false, 1);
}

IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       BlobUrlFromDataUrl) {
  EXPECT_TRUE(
      content::NavigateToURL(web_contents(), GURL("data:text/html,test")));
  EXPECT_TRUE(content::ExecJs(web_contents(), R"(
    const blob = new Blob(["test"], {type: "text/html"});
    const url = URL.createObjectURL(blob);
    location.href = url;
  )"));
  CheckHistogramCount("Navigation.BlobUrl", true, 1);
  CheckHistogramCount("Navigation.BlobUrl", false, 3);
  CheckHistogramCount("Navigation.BlobUrl.MainFrame", true, 1);
  CheckHistogramCount("Navigation.BlobUrl.MainFrame", false, 0);
  CheckHistogramCount("Navigation.BlobUrl.Sandboxed", true, 1);
  CheckHistogramCount("Navigation.BlobUrl.Sandboxed", false, 0);
}

IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       BlobUrlPopup) {
  GURL url = https_server().GetURL("a.test", "/empty.html");

  EXPECT_TRUE(content::NavigateToURL(web_contents(), url));
  CheckHistogramCount("Navigation.BlobUrl", true, 0);
  CheckHistogramCount("Navigation.BlobUrl", false, 3);
  CheckHistogramCount("Navigation.BlobUrl.MainFrame", true, 0);
  CheckHistogramCount("Navigation.BlobUrl.MainFrame", false, 0);
  CheckHistogramCount("Navigation.BlobUrl.Sandboxed", true, 0);
  CheckHistogramCount("Navigation.BlobUrl.Sandboxed", false, 0);

  EXPECT_TRUE(content::ExecJs(web_contents(), R"(
    const blob = new Blob(["test"], {type: "text/html"});
    const url = URL.createObjectURL(blob);
    window.open(url, '_blank', 'noopener');
  )"));
  CheckHistogramCount("Navigation.BlobUrl", true, 1);
  CheckHistogramCount("Navigation.BlobUrl", false, 3);
  CheckHistogramCount("Navigation.BlobUrl.MainFrame", true, 1);
  CheckHistogramCount("Navigation.BlobUrl.MainFrame", false, 0);
  CheckHistogramCount("Navigation.BlobUrl.Sandboxed", true, 0);
  CheckHistogramCount("Navigation.BlobUrl.Sandboxed", false, 1);
}

using SameDocumentCrossOriginInitiatorTest =
    ChromeWebPlatformSecurityMetricsBrowserTest;

IN_PROC_BROWSER_TEST_F(SameDocumentCrossOriginInitiatorTest, SameOrigin) {
  const GURL parent_url = https_server().GetURL("a.test", "/empty.html");
  const GURL child_url = https_server().GetURL("a.test", "/empty.html");
  EXPECT_TRUE(content::NavigateToURL(web_contents(), parent_url));
  LoadIFrame(child_url);
  CheckCounter(WebFeature::kSameDocumentCrossOriginInitiator, 0);
  EXPECT_TRUE(content::ExecJs(
      web_contents(), "document.querySelector('iframe').src += '#foo';"));
  CheckCounter(WebFeature::kSameDocumentCrossOriginInitiator, 0);
}

IN_PROC_BROWSER_TEST_F(SameDocumentCrossOriginInitiatorTest, SameSite) {
  const GURL parent_url = https_server().GetURL("a.a.test", "/empty.html");
  const GURL child_url = https_server().GetURL("b.a.test", "/empty.html");
  EXPECT_TRUE(content::NavigateToURL(web_contents(), parent_url));
  LoadIFrame(child_url);
  CheckCounter(WebFeature::kSameDocumentCrossOriginInitiator, 0);
  EXPECT_TRUE(content::ExecJs(
      web_contents(), "document.querySelector('iframe').src += '#foo';"));
  EXPECT_TRUE(content::WaitForLoadStop(web_contents()));
  // TODO(https://crbug.com/1408429) It seems the initiator origin is wrong,
  // e.g. `child_url` instead of `parent_url`, causing the metrics not to be
  // recorded.
  CheckCounter(WebFeature::kSameDocumentCrossOriginInitiator, 0);
}

IN_PROC_BROWSER_TEST_F(SameDocumentCrossOriginInitiatorTest, CrossOrigin) {
  const GURL parent_url = https_server().GetURL("a.test", "/empty.html");
  const GURL child_url = https_server().GetURL("b.test", "/empty.html");
  EXPECT_TRUE(content::NavigateToURL(web_contents(), parent_url));
  LoadIFrame(child_url);
  CheckCounter(WebFeature::kSameDocumentCrossOriginInitiator, 0);
  EXPECT_TRUE(content::ExecJs(
      web_contents(), "document.querySelector('iframe').src += '#foo';"));
  EXPECT_TRUE(content::WaitForLoadStop(web_contents()));
  CheckCounter(WebFeature::kSameDocumentCrossOriginInitiator, 1);
}

IN_PROC_BROWSER_TEST_F(SameDocumentCrossOriginInitiatorTest,
                       SameOriginInitiated) {
  const GURL parent_url = https_server().GetURL("a.test", "/empty.html");
  const GURL child_url = https_server().GetURL("b.test", "/empty.html");
  EXPECT_TRUE(content::NavigateToURL(web_contents(), parent_url));
  LoadIFrame(child_url);
  CheckCounter(WebFeature::kSameDocumentCrossOriginInitiator, 0);
  EXPECT_TRUE(
      content::ExecJs(GetChild(*(web_contents()->GetPrimaryMainFrame())),
                      "location.href += '#foo';"));
  EXPECT_TRUE(content::WaitForLoadStop(web_contents()));
  CheckCounter(WebFeature::kSameDocumentCrossOriginInitiator, 0);
}

IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       JavascriptUrlNavigationInIFrame) {
  GURL url = https_server().GetURL("a.test", "/empty.html");
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url));
  EXPECT_TRUE(content::ExecJs(web_contents(), R"(
    new Promise(resolve => {
      let iframe = document.createElement("iframe");
      iframe.src = 'javascript:1';
      iframe.onload = resolve;
      document.body.appendChild(iframe);
    });
  )"));
  CheckCounter(WebFeature::kExecutedEmptyJavaScriptURLFromFrame, 0);
  CheckCounter(WebFeature::kExecutedJavaScriptURLFromFrame, 1);
}

IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       EmptyStringJavascriptUrlNavigationInIFrame) {
  GURL url = https_server().GetURL("a.test", "/empty.html");
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url));
  EXPECT_TRUE(content::ExecJs(web_contents(), R"(
    new Promise(resolve => {
      let iframe = document.createElement("iframe");
      iframe.src = 'javascript:""';
      iframe.onload = resolve;
      document.body.appendChild(iframe);
    });
  )"));
  CheckCounter(WebFeature::kExecutedEmptyJavaScriptURLFromFrame, 1);
  CheckCounter(WebFeature::kExecutedJavaScriptURLFromFrame, 1);
}

IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       JavascriptUrlNavigationInTopFrame) {
  GURL url = https_server().GetURL("a.test", "/empty.html");
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url));
  EXPECT_TRUE(content::ExecJs(web_contents(), R"(
    location.href = 'javascript:""';
  )"));
  CheckCounter(WebFeature::kExecutedEmptyJavaScriptURLFromFrame, 0);
  CheckCounter(WebFeature::kExecutedJavaScriptURLFromFrame, 0);
}

IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       DanglingMarkupInIframeName) {
  GURL url = https_server().GetURL("a.test", "/empty.html");
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url));
  EXPECT_TRUE(content::ExecJs(web_contents(), R"(
    new Promise(resolve => {
      let iframe = document.createElement("iframe");
      iframe.src = '/empty.html';
      iframe.name = "<\n>";
      iframe.onload = resolve;
      document.body.appendChild(iframe);
    });
  )"));
  CheckCounter(WebFeature::kDanglingMarkupInWindowName, 1);
  CheckCounter(WebFeature::kDanglingMarkupInWindowNameNotEndsWithNewLineOrGT,
               0);
  CheckCounter(WebFeature::kDanglingMarkupInWindowNameNotEndsWithGT, 0);
  CheckCounter(WebFeature::kDanglingMarkupInTarget, 0);
  CheckCounter(WebFeature::kDanglingMarkupInTargetNotEndsWithGT, 0);
  CheckCounter(WebFeature::kDanglingMarkupInTargetNotEndsWithNewLineOrGT, 0);
}

IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       DanglingMarkupInNameWithGreaterThan) {
  GURL url = https_server().GetURL("a.test", "/empty.html");
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url));
  EXPECT_TRUE(content::ExecJs(web_contents(), R"(
    new Promise(resolve => {
      let iframe = document.createElement("iframe");
      iframe.src = '/empty.html';
      iframe.name = "<\n";
      iframe.onload = resolve;
      document.body.appendChild(iframe);
    });
  )"));
  CheckCounter(WebFeature::kDanglingMarkupInWindowName, 1);
  CheckCounter(WebFeature::kDanglingMarkupInWindowNameNotEndsWithNewLineOrGT,
               0);
  CheckCounter(WebFeature::kDanglingMarkupInWindowNameNotEndsWithGT, 1);
  CheckCounter(WebFeature::kDanglingMarkupInTarget, 0);
  CheckCounter(WebFeature::kDanglingMarkupInTargetNotEndsWithGT, 0);
  CheckCounter(WebFeature::kDanglingMarkupInTargetNotEndsWithNewLineOrGT, 0);
}

IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       DanglingMarkupInNameWithNewLineOrGreaterThan) {
  GURL url = https_server().GetURL("a.test", "/empty.html");
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url));
  EXPECT_TRUE(content::ExecJs(web_contents(), R"(
    new Promise(resolve => {
      let iframe = document.createElement("iframe");
      iframe.src = '/empty.html';
      iframe.name = "<\ntest";
      iframe.onload = resolve;
      document.body.appendChild(iframe);
    });
  )"));

  CheckCounter(WebFeature::kDanglingMarkupInWindowName, 1);
  CheckCounter(WebFeature::kDanglingMarkupInWindowNameNotEndsWithNewLineOrGT,
               1);
  CheckCounter(WebFeature::kDanglingMarkupInWindowNameNotEndsWithGT, 1);
  CheckCounter(WebFeature::kDanglingMarkupInTarget, 0);
  CheckCounter(WebFeature::kDanglingMarkupInTargetNotEndsWithGT, 0);
  CheckCounter(WebFeature::kDanglingMarkupInTargetNotEndsWithNewLineOrGT, 0);
}

IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       DanglingMarkupInTarget) {
  GURL url = https_server().GetURL("a.test", "/empty.html");
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url));
  EXPECT_TRUE(content::ExecJs(web_contents(), R"(
    let link = document.createElement("a");
    link.href = '/empty.html';
    link.target = "<\n>";
    document.body.appendChild(link);
    link.click();
  )"));

  CheckCounter(WebFeature::kDanglingMarkupInWindowName, 0);
  CheckCounter(WebFeature::kDanglingMarkupInWindowNameNotEndsWithNewLineOrGT,
               0);
  CheckCounter(WebFeature::kDanglingMarkupInWindowNameNotEndsWithGT, 0);
  CheckCounter(WebFeature::kDanglingMarkupInTarget, 1);
  CheckCounter(WebFeature::kDanglingMarkupInTargetNotEndsWithGT, 0);
  CheckCounter(WebFeature::kDanglingMarkupInTargetNotEndsWithNewLineOrGT, 0);
}

IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       DanglingMarkupInTargetWithNewLineOrGreaterThan) {
  GURL url = https_server().GetURL("a.test", "/empty.html");
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url));
  EXPECT_TRUE(content::ExecJs(web_contents(), R"(
    let link = document.createElement("a");
    link.href = '/empty.html';
    link.target = "<\n";
    document.body.appendChild(link);
    link.click();
  )"));

  CheckCounter(WebFeature::kDanglingMarkupInWindowName, 0);
  CheckCounter(WebFeature::kDanglingMarkupInWindowNameNotEndsWithNewLineOrGT,
               0);
  CheckCounter(WebFeature::kDanglingMarkupInWindowNameNotEndsWithGT, 0);
  CheckCounter(WebFeature::kDanglingMarkupInTarget, 1);
  CheckCounter(WebFeature::kDanglingMarkupInTargetNotEndsWithGT, 1);
  CheckCounter(WebFeature::kDanglingMarkupInTargetNotEndsWithNewLineOrGT, 0);

  EXPECT_TRUE(content::ExecJs(web_contents(), R"(
    let base = document.createElement("base");
    base.target = "<\ntest";
    document.body.appendChild(base);
    let link = document.createElement("a");
    link.href = '/empty.html';
    document.body.appendChild(link);
    link.click();
  )"));
  CheckCounter(WebFeature::kDanglingMarkupInWindowName, 0);
  CheckCounter(WebFeature::kDanglingMarkupInWindowNameNotEndsWithNewLineOrGT,
               0);
  CheckCounter(WebFeature::kDanglingMarkupInWindowNameNotEndsWithGT, 0);
  CheckCounter(WebFeature::kDanglingMarkupInTarget, 2);
  CheckCounter(WebFeature::kDanglingMarkupInTargetNotEndsWithGT, 2);
  CheckCounter(WebFeature::kDanglingMarkupInTargetNotEndsWithNewLineOrGT, 1);
}

IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       DocumentOpenAliasedOriginDocumentDomain) {
  GURL url = https_server().GetURL("sub.a.test", "/empty.html");
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url));
  EXPECT_TRUE(content::ExecJs(web_contents(), R"(
    const iframe = document.createElement("iframe");
    iframe.src = location.href;
    iframe.onload = () => {
      iframe.contentDocument.write("<div></div>");
      document.domain = "a.test";
    };
    document.body.appendChild(iframe);
  )"));

  CheckCounter(WebFeature::kDocumentOpenAliasedOriginDocumentDomain, 1);
}

IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       CrossWindowAccessToHTMLDocument) {
  EXPECT_TRUE(content::NavigateToURL(web_contents(),
                                     https_server().GetURL("/empty.html")));

  LoadIFrame(https_server().GetURL("/hello.html"));
  CheckCounter(WebFeature::kCrossWindowAccessToBrowserGeneratedDocument, 0);

  EXPECT_TRUE(content::ExecJs(web_contents(), R"(
    window.frames[0].contentDocument;
  )"));

  // Plain HTML should not count as a browser-generated document.
  CheckCounter(WebFeature::kCrossWindowAccessToBrowserGeneratedDocument, 0);
}

IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       CrossWindowAccessToXHTMLDocument) {
  EXPECT_TRUE(content::NavigateToURL(web_contents(),
                                     https_server().GetURL("/empty.html")));

  LoadIFrame(https_server().GetURL("/security/minimal.xhtml"));

  CheckCounter(WebFeature::kCrossWindowAccessToBrowserGeneratedDocument, 0);

  EXPECT_TRUE(content::ExecJs(web_contents(), R"(
    window.frames[0].contentDocument;
  )"));

  // XHTML should not count as a browser-generated document, even though it is
  // technically XML.
  CheckCounter(WebFeature::kCrossWindowAccessToBrowserGeneratedDocument, 0);
}

IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       CrossWindowAccessToSVGDocument) {
  EXPECT_TRUE(content::NavigateToURL(web_contents(),
                                     https_server().GetURL("/empty.html")));

  LoadIFrame(https_server().GetURL("/circle.svg"));

  CheckCounter(WebFeature::kCrossWindowAccessToBrowserGeneratedDocument, 0);

  EXPECT_TRUE(content::ExecJs(web_contents(), R"(
    window.frames[0].contentDocument;
  )"));

  // SVG should not count as a browser-generated document, even though it is
  // technically XML.
  CheckCounter(WebFeature::kCrossWindowAccessToBrowserGeneratedDocument, 0);
}

IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       CrossWindowAccessToImageDocument) {
  EXPECT_TRUE(content::NavigateToURL(web_contents(),
                                     https_server().GetURL("/empty.html")));

  LoadIFrame(https_server().GetURL("/image.jpg"));

  CheckCounter(WebFeature::kCrossWindowAccessToBrowserGeneratedDocument, 0);

  EXPECT_TRUE(content::ExecJs(web_contents(), R"(
    window.frames[0].contentDocument;
  )"));
  CheckCounter(WebFeature::kCrossWindowAccessToBrowserGeneratedDocument, 1);
}

IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       CrossWindowAccessToMediaDocument) {
  EXPECT_TRUE(content::NavigateToURL(web_contents(),
                                     https_server().GetURL("/empty.html")));

  LoadIFrame(https_server().GetURL("/media/bear.mp4"));

  CheckCounter(WebFeature::kCrossWindowAccessToBrowserGeneratedDocument, 0);

  EXPECT_TRUE(content::ExecJs(web_contents(), R"(
    window.frames[0].contentDocument;
  )"));
  CheckCounter(WebFeature::kCrossWindowAccessToBrowserGeneratedDocument, 1);
}

IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       CrossWindowAccessToTextDocument) {
  EXPECT_TRUE(content::NavigateToURL(web_contents(),
                                     https_server().GetURL("/empty.html")));

  LoadIFrame(https_server().GetURL("/site_isolation/valid.json"));

  CheckCounter(WebFeature::kCrossWindowAccessToBrowserGeneratedDocument, 0);

  EXPECT_TRUE(content::ExecJs(web_contents(), R"(
    window.frames[0].contentDocument;
  )"));
  CheckCounter(WebFeature::kCrossWindowAccessToBrowserGeneratedDocument, 1);
}

IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       CrossWindowAccessToXMLDocument) {
  EXPECT_TRUE(content::NavigateToURL(web_contents(),
                                     https_server().GetURL("/empty.html")));

  LoadIFrame(https_server().GetURL("/site_isolation/valid.xml"));

  CheckCounter(WebFeature::kCrossWindowAccessToBrowserGeneratedDocument, 0);

  EXPECT_TRUE(content::ExecJs(web_contents(), R"(
    window.frames[0].contentDocument;
  )"));
  CheckCounter(WebFeature::kCrossWindowAccessToBrowserGeneratedDocument, 1);
}

IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       CrossWindowAccessToPluginDocument) {
  EXPECT_TRUE(content::NavigateToURL(web_contents(),
                                     https_server().GetURL("/empty.html")));

  LoadIFrame(https_server().GetURL("/site_isolation/fake.pdf"));

  CheckCounter(WebFeature::kCrossWindowAccessToBrowserGeneratedDocument, 0);

  // This should throw a `SecurityError` according to the spec, but does not due
  // to https://crbug.com/1257611.
  EXPECT_TRUE(content::ExecJs(web_contents(), R"(
    window.frames[0].contentDocument;
  )"));

  // We would like to count such accesses for the purposes of estimating the
  // impact of fixing https://crbug.com/1257611, but it does not seem to be as
  // easy as for other document classes. The enclosing document does not seem to
  // count as a "plugin document".
  CheckCounter(WebFeature::kCrossWindowAccessToBrowserGeneratedDocument, 0);

  // Accessing the inner frame throws a `SecurityError`, however.
  EXPECT_EQ("SecurityError", content::EvalJs(web_contents(), R"(
    (() => {
      try {
        window.frames[0].frames[0].contentDocument;
      } catch (e) {
        return e.name;
      }
      return "success";
    })()
  )"));
}

IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       CSPEESameOriginBlanketEnforcement) {
  GURL url = https_server().GetURL("a.test", "/empty.html");

  EXPECT_TRUE(content::NavigateToURL(web_contents(), url));
  EXPECT_TRUE(content::ExecJs(web_contents(), R"(
    const iframe = document.createElement("iframe");
    iframe.csp = "script-src 'none'";
    iframe.src = location.href;
    document.body.appendChild(iframe);
  )"));
  CheckCounter(WebFeature::kCSPEESameOriginBlanketEnforcement, 1);
}

IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       CSPEECrossOrigin) {
  GURL url = https_server().GetURL("a.test", "/empty.html");
  GURL cross_origin_url = https_server().GetURL("b.test", "/empty.html");

  EXPECT_TRUE(content::NavigateToURL(web_contents(), url));
  EXPECT_TRUE(
      content::ExecJs(web_contents(), content::JsReplace(R"(
    const iframe = document.createElement("iframe");
    iframe.csp = "script-src 'none'";
    iframe.src = $1;
    document.body.appendChild(iframe);
  )",
                                                         cross_origin_url)));
  CheckCounter(WebFeature::kCSPEESameOriginBlanketEnforcement, 0);
}

IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       CSPEESameOriginWithAllowCSPHeader) {
  GURL url = http_server().GetURL("a.test",
                                  "/set-header?"
                                  "Allow-CSP-From: *");

  EXPECT_TRUE(content::NavigateToURL(web_contents(), url));
  EXPECT_TRUE(content::ExecJs(web_contents(), content::JsReplace(R"(
    const iframe = document.createElement("iframe");
    iframe.csp = "script-src 'none'";
    iframe.src = $1;
    document.body.appendChild(iframe);
  )",
                                                                 url)));
  CheckCounter(WebFeature::kCSPEESameOriginBlanketEnforcement, 0);
}

IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       CSPEESameOriginWithSameCSPHeader) {
  GURL url = http_server().GetURL("a.test",
                                  "/set-header?"
                                  "Content-Security-Policy: img-src 'none'");

  EXPECT_TRUE(content::NavigateToURL(web_contents(), url));
  EXPECT_TRUE(content::ExecJs(web_contents(), content::JsReplace(R"(
    const iframe = document.createElement("iframe");
    iframe.csp = "img-src 'none'";
    iframe.src = $1;
    document.body.appendChild(iframe);
  )",
                                                                 url)));
  CheckCounter(WebFeature::kCSPEESameOriginBlanketEnforcement, 0);
}

// TODO(arthursonzogni): Add basic test(s) for the WebFeatures:
// [ ] CrossOriginOpenerPolicySameOrigin
// [ ] CrossOriginOpenerPolicySameOriginAllowPopups
// [X] CoopAndCoepIsolated
//
// Added by:
// https://chromium-review.googlesource.com/c/chromium/src/+/2122140

}  // namespace
