// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/platform_thread.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/cross_origin_opener_policy.mojom.h"

namespace {
const int kWasmPageSize = 1 << 16;
}  // namespace

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
            network::features::kCrossOriginOpenerPolicyReporting,
            // SharedArrayBuffer is needed for these tests.
            features::kSharedArrayBuffer,
        },
        {});
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
    while (true) {
      content::FetchHistogramsFromChildProcesses();
      metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

      int count =
          histogram_.GetBucketCount("Blink.UseCounter.Features", feature);
      CHECK_LE(count, expected_count);
      if (count == expected_count)
        return;

      base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(5));
    }
  }

 private:
  void SetUpOnMainThread() final {
    host_resolver()->AddRule("*", "127.0.0.1");
    https_server_.AddDefaultHandlers(GetChromeTestDataDir());
    http_server_.AddDefaultHandlers(GetChromeTestDataDir());
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
    ASSERT_TRUE(https_server_.Start());
    ASSERT_TRUE(http_server_.Start());
    EXPECT_TRUE(content::NavigateToURL(web_contents(), GURL("about:blank")));
  }

  void SetUpCommandLine(base::CommandLine* command_line) final {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  net::EmbeddedTestServer https_server_;
  net::EmbeddedTestServer http_server_;
  int expected_count_ = 0;
  base::HistogramTester histogram_;
  WebFeature monitored_feature_;
  base::test::ScopedFeatureList features_;
};

// Check the kCrossOriginOpenerPolicyReporting feature usage. No header => 0
// count.
IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       CrossOriginOpenerPolicyReportingNoHeader) {
  set_monitored_feature(WebFeature::kCrossOriginOpenerPolicyReporting);
  GURL url = https_server().GetURL("a.com", "/title1.html");
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url));
  ExpectHistogramIncreasedBy(0);
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

  content::RenderFrameHost* main_document = web_contents()->GetAllFrames()[0];
  content::RenderFrameHost* sub_document = web_contents()->GetAllFrames()[1];

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

  content::RenderFrameHost* main_document = web_contents()->GetAllFrames()[0];
  content::RenderFrameHost* sub_document = web_contents()->GetAllFrames()[1];

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

  content::RenderFrameHost* main_document = web_contents()->GetAllFrames()[0];
  content::RenderFrameHost* sub_document = web_contents()->GetAllFrames()[1];

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

  content::RenderFrameHost* main_document = web_contents()->GetAllFrames()[0];
  content::RenderFrameHost* sub_document = web_contents()->GetAllFrames()[1];

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

  content::RenderFrameHost* main_document = web_contents()->GetAllFrames()[0];
  content::RenderFrameHost* sub_document = web_contents()->GetAllFrames()[1];

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

  content::RenderFrameHost* main_document = web_contents()->GetAllFrames()[0];
  content::RenderFrameHost* sub_document = web_contents()->GetAllFrames()[1];

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

  content::RenderFrameHost* main_document = web_contents()->GetAllFrames()[0];
  content::RenderFrameHost* sub_document = web_contents()->GetAllFrames()[1];

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

  CheckCounter(WebFeature::kWasmModuleSharing, 0);
  CheckCounter(WebFeature::kCrossOriginWasmModuleSharing, 0);
}

IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       WasmModuleSharingSameSite) {
  GURL main_url = https_server().GetURL("a.a.com", "/empty.html");
  GURL sub_url = https_server().GetURL("b.a.com", "/empty.html");

  EXPECT_TRUE(content::NavigateToURL(web_contents(), main_url));
  LoadIFrame(sub_url);

  content::RenderFrameHost* main_document = web_contents()->GetAllFrames()[0];
  content::RenderFrameHost* sub_document = web_contents()->GetAllFrames()[1];

  EXPECT_EQ(true, content::ExecJs(main_document, R"(
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

  CheckCounter(WebFeature::kWasmModuleSharing, 1);
  CheckCounter(WebFeature::kCrossOriginWasmModuleSharing, 1);
}

IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       WasmModuleSharingSameOrigin) {
  GURL main_url = https_server().GetURL("a.com", "/empty.html");
  GURL sub_url = https_server().GetURL("a.com", "/empty.html");

  EXPECT_TRUE(content::NavigateToURL(web_contents(), main_url));
  LoadIFrame(sub_url);

  content::RenderFrameHost* main_document = web_contents()->GetAllFrames()[0];
  content::RenderFrameHost* sub_document = web_contents()->GetAllFrames()[1];

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

  CheckCounter(WebFeature::kWasmModuleSharing, 1);
  CheckCounter(WebFeature::kCrossOriginWasmModuleSharing, 0);
}

IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       WasmModuleSharingSameSiteBeforeSetDocumentDomain) {
  GURL main_url = https_server().GetURL("sub.a.com", "/empty.html");
  GURL sub_url = https_server().GetURL("a.com", "/empty.html");

  EXPECT_TRUE(content::NavigateToURL(web_contents(), main_url));
  LoadIFrame(sub_url);

  content::RenderFrameHost* main_document = web_contents()->GetAllFrames()[0];
  content::RenderFrameHost* sub_document = web_contents()->GetAllFrames()[1];

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

  EXPECT_EQ(true, content::EvalJs(main_document, R"(
    new Promise(async resolve => {
      while (!received_module)
        await new Promise(r => setTimeout(r, 10));
      resolve(true);
    });
  )"));

  CheckCounter(WebFeature::kV8SharedArrayBufferConstructedWithoutIsolation, 0);
  CheckCounter(WebFeature::kV8SharedArrayBufferConstructed, 0);

  CheckCounter(WebFeature::kWasmModuleSharing, 1);
  CheckCounter(WebFeature::kCrossOriginWasmModuleSharing, 0);
}

IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       WasmModuleSharingSameSiteAfterSetDocumentDomain) {
  GURL main_url = https_server().GetURL("sub.a.com", "/empty.html");
  GURL sub_url = https_server().GetURL("sub.a.com", "/empty.html");

  EXPECT_TRUE(content::NavigateToURL(web_contents(), main_url));
  LoadIFrame(sub_url);

  content::RenderFrameHost* main_document = web_contents()->GetAllFrames()[0];
  content::RenderFrameHost* sub_document = web_contents()->GetAllFrames()[1];

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

  CheckCounter(WebFeature::kWasmModuleSharing, 1);
  CheckCounter(WebFeature::kCrossOriginWasmModuleSharing, 0);
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

// TODO(arthursonzogni): Add basic test(s) for the WebFeatures:
// - CrossOriginOpenerPolicySameOrigin
// - CrossOriginOpenerPolicySameOriginAllowPopups
// - CrossOriginEmbedderPolicyRequireCorp
// - CoopAndCoepIsolated
//
// Added by:
// https://chromium-review.googlesource.com/c/chromium/src/+/2122140
//
// In particular, it would be interesting knowing what happens with iframes?
// Are CoopCoepOriginIsolated nested document counted as CoopAndCoepIsolated?
// Not doing it would underestimate the usage metric.
