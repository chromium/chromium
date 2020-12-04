// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/cross_origin_opener_policy.mojom.h"

// Web platform security features are implemented by content/ and blink/.
// However, since ContentBrowserClientImpl::LogWebFeatureForCurrentPage() is
// currently left blank in content/, metrics logging can't be tested from
// content/. So it is tested from chrome/ instead.
class ChromeWebPlatformSecurityMetricsBrowserTest
    : public InProcessBrowserTest {
 public:
  ChromeWebPlatformSecurityMetricsBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS),
        http_server_(net::EmbeddedTestServer::TYPE_HTTP) {
    features_.InitWithFeatures(
        {
            // Enabled:
            network::features::kCrossOriginOpenerPolicy,
            network::features::kCrossOriginEmbedderPolicy,
            network::features::kCrossOriginOpenerPolicyReporting,
        },
        {});
  }

  content::WebContents* web_contents() const {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  void set_monitored_feature(blink::mojom::WebFeature feature) {
    monitored_feature_ = feature;
  }

  void LoadIFrame(const GURL& url) {
    EXPECT_TRUE(content::ExecJs(web_contents(), content::JsReplace(R"(
      new Promise(resolve => {
        let iframe = document.createElement("iframe");
        iframe.src = $1;
        iframe.onload = resolve;
        document.body.appendChild(iframe);
      });
    )",
                                                                   url)));
  }

  void ExpectHistogramIncreasedBy(int count) {
    expected_count_ += count;
    EXPECT_TRUE(content::NavigateToURL(web_contents(), GURL("about:blank")));
    histogram_.ExpectBucketCount("Blink.UseCounter.Features",
                                 monitored_feature_, expected_count_);
  }

  net::EmbeddedTestServer& https_server() { return https_server_; }
  net::EmbeddedTestServer& http_server() { return http_server_; }

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
  blink::mojom::WebFeature monitored_feature_;
  base::test::ScopedFeatureList features_;
};

// Check the kCrossOriginOpenerPolicyReporting feature usage. No header => 0
// count.
IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       CrossOriginOpenerPolicyReportingNoHeader) {
  set_monitored_feature(
      blink::mojom::WebFeature::kCrossOriginOpenerPolicyReporting);
  GURL url = https_server().GetURL("a.com", "/title1.html");
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url));
  ExpectHistogramIncreasedBy(0);
}

// Check the kCrossOriginOpenerPolicyReporting feature usage. COOP-Report-Only +
// HTTP => 0 count.
IN_PROC_BROWSER_TEST_F(ChromeWebPlatformSecurityMetricsBrowserTest,
                       CrossOriginOpenerPolicyReportingReportOnlyHTTP) {
  set_monitored_feature(
      blink::mojom::WebFeature::kCrossOriginOpenerPolicyReporting);
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
  set_monitored_feature(
      blink::mojom::WebFeature::kCrossOriginOpenerPolicyReporting);
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
  set_monitored_feature(
      blink::mojom::WebFeature::kCrossOriginOpenerPolicyReporting);
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
  set_monitored_feature(
      blink::mojom::WebFeature::kCrossOriginOpenerPolicyReporting);
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
  set_monitored_feature(
      blink::mojom::WebFeature::kCrossOriginOpenerPolicyReporting);
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
  set_monitored_feature(
      blink::mojom::WebFeature::kCrossOriginOpenerPolicyReporting);
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
  set_monitored_feature(
      blink::mojom::WebFeature::kCrossOriginOpenerPolicyReporting);
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
      blink::mojom::WebFeature::kCrossOriginSubframeWithoutEmbeddingControl);
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
      blink::mojom::WebFeature::kCrossOriginSubframeWithoutEmbeddingControl);
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
      blink::mojom::WebFeature::kCrossOriginSubframeWithoutEmbeddingControl);
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
      blink::mojom::WebFeature::kCrossOriginSubframeWithoutEmbeddingControl);
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
      blink::mojom::WebFeature::kCrossOriginSubframeWithoutEmbeddingControl);
  GURL main_document_url = https_server().GetURL("a.com", "/title1.html");
  GURL sub_document_url =
      https_server().GetURL("b.com",
                            "/set-header?"
                            "Content-Security-Policy: script-src 'self';");
  EXPECT_TRUE(content::NavigateToURL(web_contents(), main_document_url));
  LoadIFrame(sub_document_url);
  ExpectHistogramIncreasedBy(1);
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
