// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/metrics/metrics_memory_details.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

class TestMemoryDetails : public MetricsMemoryDetails {
 public:
  TestMemoryDetails() : MetricsMemoryDetails(base::DoNothing()) {}

  TestMemoryDetails(const TestMemoryDetails&) = delete;
  TestMemoryDetails& operator=(const TestMemoryDetails&) = delete;

  void StartFetchAndWait() {
    uma_ = std::make_unique<base::HistogramTester>();
    StartFetch();
    run_loop_.Run();
  }

  // Assumes we've just finished calling StartFetchAndWait(), and there is at
  // most one sample recorded for `metric`.
  void VerifyMetricResult(const std::string& metric, int value, int count) {
    std::vector<base::Bucket> histogram_for_metric =
        uma_->GetAllSamples(metric);
    if (histogram_for_metric.size()) {
      EXPECT_EQ(base::Bucket(value, count), histogram_for_metric[0])
          << " : metric = " << metric;
    } else {
      EXPECT_EQ(0, count) << " : metric = " << metric;
    }
  }

  // Returns a HistogramTester which observed the most recent call to
  // StartFetchAndWait().
  base::HistogramTester* uma() { return uma_.get(); }

 private:
  ~TestMemoryDetails() override = default;

  void OnDetailsAvailable() override {
    MetricsMemoryDetails::OnDetailsAvailable();
    // Exit the loop initiated by StartFetchAndWait().
    run_loop_.QuitWhenIdle();
  }

  std::unique_ptr<base::HistogramTester> uma_;
  base::RunLoop run_loop_;
};

class IsolatedSandboxedIframeBrowserTestBase : public InProcessBrowserTest {
 public:
  explicit IsolatedSandboxedIframeBrowserTestBase(
      bool enable_isolate_sandboxed_iframes)
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS),
        enable_isolate_sandboxed_iframes_(enable_isolate_sandboxed_iframes) {
    // To keep the tests easier to reason about, turn off both the spare
    // renderer process and process reuse for subframes in different
    // BrowsingInstances.
    if (enable_isolate_sandboxed_iframes_) {
      feature_list_.InitWithFeatures(
          /* enable_features */ {blink::features::kIsolateSandboxedIframes,
                                 features::kDisableProcessReuse},
          /* disable_features */ {features::kSpareRendererForSitePerProcess});
    } else {
      feature_list_.InitWithFeatures(
          /* enable_features */ {features::kDisableProcessReuse},
          /* disable_features */ {blink::features::kIsolateSandboxedIframes,
                                  features::kSpareRendererForSitePerProcess});
    }
  }

  IsolatedSandboxedIframeBrowserTestBase(
      const IsolatedSandboxedIframeBrowserTestBase&) = delete;
  IsolatedSandboxedIframeBrowserTestBase& operator=(
      const IsolatedSandboxedIframeBrowserTestBase&) = delete;

  ~IsolatedSandboxedIframeBrowserTestBase() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    mock_cert_verifier_.SetUpCommandLine(command_line);
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    https_server()->AddDefaultHandlers(GetChromeTestDataDir());
    ASSERT_TRUE(https_server()->Start());
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->StartAcceptingConnections();
  }

  void SetUpInProcessBrowserTestFixture() override {
    mock_cert_verifier_.SetUpInProcessBrowserTestFixture();
  }

  void TearDownInProcessBrowserTestFixture() override {
    mock_cert_verifier_.TearDownInProcessBrowserTestFixture();
  }

  void TearDownOnMainThread() override {
    ASSERT_TRUE(https_server()->ShutdownAndWaitUntilComplete());
  }

  net::EmbeddedTestServer* https_server() { return &https_server_; }

  void VerifyStartupMetrics() {
    scoped_refptr<TestMemoryDetails> details = new TestMemoryDetails();
    details->StartFetchAndWait();

    // Verify we're starting at zero.
    details->VerifyMetricResult("SiteIsolation.IsolatableSandboxedIframes",
                                0 /* value */, 1 /* count*/);
    details->VerifyMetricResult(
        "SiteIsolation.IsolatableSandboxedIframes.UniqueOrigins", 0 /* value */,
        1 /* count*/);
    details->VerifyMetricResult(
        "SiteIsolation.IsolatableSandboxedIframes.UniqueSites", 0 /* value */,
        1 /* count*/);
    details->VerifyMetricResult(
        "Memory.RenderProcessHost.Count.SandboxedIframeOverhead", 0 /* value */,
        1 /* count*/);
  }

  void VerifyMetrics(int isolatable_sandboxed_iframes_value,
                     int unique_origins_value,
                     int unique_sites_value,
                     int process_overhead_value) {
    scoped_refptr<TestMemoryDetails> details = new TestMemoryDetails();
    details->StartFetchAndWait();

    details->VerifyMetricResult("SiteIsolation.IsolatableSandboxedIframes",
                                isolatable_sandboxed_iframes_value,
                                1 /* count*/);
    details->VerifyMetricResult(
        "SiteIsolation.IsolatableSandboxedIframes.UniqueOrigins",
        unique_origins_value, 1 /* count*/);
    details->VerifyMetricResult(
        "SiteIsolation.IsolatableSandboxedIframes.UniqueSites",
        unique_sites_value, 1 /* count*/);
    details->VerifyMetricResult(
        "Memory.RenderProcessHost.Count.SandboxedIframeOverhead",
        process_overhead_value, 1 /* count*/);
  }

 private:
  content::ContentMockCertVerifier mock_cert_verifier_;
  net::EmbeddedTestServer https_server_;
  base::test::ScopedFeatureList feature_list_;
  bool enable_isolate_sandboxed_iframes_;
};  // class IsolatedSandboxedIframeBrowserTestBase

class IsolatedSandboxedIframeBrowserTest
    : public IsolatedSandboxedIframeBrowserTestBase {
 public:
  IsolatedSandboxedIframeBrowserTest()
      : IsolatedSandboxedIframeBrowserTestBase(
            true /* enable_isolate_sandboxed_iframes */) {}

  IsolatedSandboxedIframeBrowserTest(
      const IsolatedSandboxedIframeBrowserTest&) = delete;
  IsolatedSandboxedIframeBrowserTest& operator=(
      const IsolatedSandboxedIframeBrowserTest&) = delete;

  ~IsolatedSandboxedIframeBrowserTest() override = default;
};  // class IsolatedSandboxedIframeBrowserTest

class NotIsolatedSandboxedIframeBrowserTest
    : public IsolatedSandboxedIframeBrowserTestBase {
 public:
  NotIsolatedSandboxedIframeBrowserTest()
      : IsolatedSandboxedIframeBrowserTestBase(
            false /* enable_isolate_sandboxed_iframes */) {}

  NotIsolatedSandboxedIframeBrowserTest(
      const NotIsolatedSandboxedIframeBrowserTest&) = delete;
  NotIsolatedSandboxedIframeBrowserTest& operator=(
      const NotIsolatedSandboxedIframeBrowserTest&) = delete;

  ~NotIsolatedSandboxedIframeBrowserTest() override = default;
};  // class NotIsolatedSandboxedIframeBrowserTest

// Test that a single isolatable frame generates correct metrics.
IN_PROC_BROWSER_TEST_F(IsolatedSandboxedIframeBrowserTest, IsolatedSandbox) {
  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  // The child needs to have the same origin as the parent.
  GURL child_url(main_url);
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderFrameHost* frame_host = web_contents->GetPrimaryMainFrame();

  VerifyStartupMetrics();

  // Create sandboxed child frame, same-origin.
  {
    std::string js_str = base::StringPrintf(
        "var frame = document.createElement('iframe'); "
        "frame.sandbox = ''; "
        "frame.src = '%s'; "
        "document.body.appendChild(frame);",
        child_url.spec().c_str());
    EXPECT_TRUE(ExecJs(frame_host, js_str));
    ASSERT_TRUE(WaitForLoadStop(web_contents));
  }

  // Verify histograms are updated.
  int isolatable_sandboxed_iframes_value = 1;
  int unique_origins_value = 1;
  int unique_sites_value = 1;
  int process_overhead_value = 1;
  VerifyMetrics(isolatable_sandboxed_iframes_value, unique_origins_value,
                unique_sites_value, process_overhead_value);
}

// Test to verify that two isolatable frames from one origin generate the
// correct metrics.
IN_PROC_BROWSER_TEST_F(IsolatedSandboxedIframeBrowserTest,
                       IsolatedSandboxSiblingSubframes) {
  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  // The child needs to have the same origin as the parent.
  GURL child_url(main_url);
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderFrameHost* frame_host = web_contents->GetPrimaryMainFrame();

  VerifyStartupMetrics();

  // Create sandboxed child frame, same-origin.
  {
    std::string js_str = base::StringPrintf(
        "var frame1 = document.createElement('iframe'); "
        "frame1.sandbox = ''; "
        "frame1.src = '%s'; "
        "document.body.appendChild(frame1); "
        "var frame2 = document.createElement('iframe'); "
        "frame2.sandbox = ''; "
        "frame2.src = '%s'; "
        "document.body.appendChild(frame2);",
        child_url.spec().c_str(), child_url.spec().c_str());
    EXPECT_TRUE(ExecJs(frame_host, js_str));
    ASSERT_TRUE(WaitForLoadStop(web_contents));
  }

  // Verify histograms are updated.
  int isolatable_sandboxed_iframes_value = 2;
  int unique_origins_value = 1;
  int unique_sites_value = 1;
  int process_overhead_value = 1;
  VerifyMetrics(isolatable_sandboxed_iframes_value, unique_origins_value,
                unique_sites_value, process_overhead_value);
}

// A test to exercise the case where the number of origins, sites, and
// isolatable iframes are all different.
IN_PROC_BROWSER_TEST_F(IsolatedSandboxedIframeBrowserTest,
                       IsolatedSandbox3Frames2Origins1Site) {
  GURL main_url(embedded_test_server()->GetURL("foo.com", "/title1.html"));
  GURL child_url_a(embedded_test_server()->GetURL("a.foo.com", "/title1.html"));
  GURL child_url_b(embedded_test_server()->GetURL("b.foo.com", "/title1.html"));

  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderFrameHost* frame_host = web_contents->GetPrimaryMainFrame();

  VerifyStartupMetrics();

  // Create three sandboxed child frames, same-site but two unique origins.
  {
    std::string js_str = base::StringPrintf(
        "var frame1 = document.createElement('iframe'); "
        "frame1.sandbox = ''; "
        "frame1.src = '%s'; "
        "document.body.appendChild(frame1); "
        "var frame2 = document.createElement('iframe'); "
        "frame2.sandbox = ''; "
        "frame2.src = '%s'; "
        "document.body.appendChild(frame2);"
        "var frame3 = document.createElement('iframe'); "
        "frame3.sandbox = ''; "
        "frame3.src = '%s'; "
        "document.body.appendChild(frame3);",
        child_url_a.spec().c_str(), child_url_b.spec().c_str(),
        child_url_b.spec().c_str());
    EXPECT_TRUE(ExecJs(frame_host, js_str));
    ASSERT_TRUE(WaitForLoadStop(web_contents));
  }

  // Verify histograms are updated.
  int isolatable_sandboxed_iframes_value = 3;
  int unique_origins_value = 2;
  int unique_sandboxed_siteinfos_value = 1;
  int process_overhead_value = 1;
  if (blink::features::kIsolateSandboxedIframesGroupingParam.Get() ==
      blink::features::IsolateSandboxedIframesGrouping::kPerOrigin) {
    unique_sandboxed_siteinfos_value = 2;
    process_overhead_value = 2;
  }
  VerifyMetrics(isolatable_sandboxed_iframes_value, unique_origins_value,
                unique_sandboxed_siteinfos_value, process_overhead_value);
}

// A test to verify that the metrics for process overhead pick up multiple
// processes for sandboxed iframes associated with different sites.
IN_PROC_BROWSER_TEST_F(
    IsolatedSandboxedIframeBrowserTest,
    IsolatedSandboxOverheadMetricsForDifferentSiteSandboxFrames) {
  GURL main_url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL child_url_a(main_url_a);
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url_a));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderFrameHost* frame_host = web_contents->GetPrimaryMainFrame();

  VerifyStartupMetrics();

  // Create sandboxed child frame, same-origin.
  {
    std::string js_str = base::StringPrintf(
        "var frame = document.createElement('iframe'); "
        "frame.sandbox = ''; "
        "frame.src = '%s'; "
        "document.body.appendChild(frame);",
        child_url_a.spec().c_str());
    EXPECT_TRUE(ExecJs(frame_host, js_str));
    ASSERT_TRUE(WaitForLoadStop(web_contents));
  }

  // Open a second tab on a different site, with an isolatable sandboxed iframe.
  GURL main_url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));
  GURL child_url_b(main_url_b);
  ASSERT_TRUE(AddTabAtIndex(1, main_url_b, ui::PAGE_TRANSITION_TYPED));
  content::WebContents* web_contents_b =
      browser()->tab_strip_model()->GetWebContentsAt(1);

  // Create sandboxed child frame, same-origin.
  {
    std::string js_str = base::StringPrintf(
        "var frame = document.createElement('iframe'); "
        "frame.sandbox = ''; "
        "frame.src = '%s'; "
        "document.body.appendChild(frame);",
        child_url_b.spec().c_str());
    EXPECT_TRUE(ExecJs(web_contents_b->GetPrimaryMainFrame(), js_str));
    ASSERT_TRUE(WaitForLoadStop(web_contents_b));
  }

  // Verify histograms are updated.
  int isolatable_sandboxed_iframes_value = 2;
  int unique_origins_value = 2;
  int unique_sites_value = 2;
  int process_overhead_value = 2;
  VerifyMetrics(isolatable_sandboxed_iframes_value, unique_origins_value,
                unique_sites_value, process_overhead_value);
}

// Test to verify that a srcdoc sandboxed iframe generates the correct metrics.
IN_PROC_BROWSER_TEST_F(IsolatedSandboxedIframeBrowserTest,
                       IsolatedSandboxSrcdocSubframe) {
  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  VerifyStartupMetrics();

  // Create sandboxed child frame, with srcdoc content.
  std::string child_inner_text("srcdoc sandboxed subframe");
  {
    std::string js_str = base::StringPrintf(
        "var frame = document.createElement('iframe'); "
        "frame.sandbox = 'allow-scripts'; "
        "frame.srcdoc = '%s'; "
        "document.body.appendChild(frame);",
        child_inner_text.c_str());
    EXPECT_TRUE(ExecJs(web_contents->GetPrimaryMainFrame(), js_str));
    ASSERT_TRUE(WaitForLoadStop(web_contents));
  }

  // Verify histograms are updated.
  int isolatable_sandboxed_iframes_value = 1;
  int unique_origins_value = 1;
  int unique_sites_value = 1;
  int process_overhead_value = 1;
  VerifyMetrics(isolatable_sandboxed_iframes_value, unique_origins_value,
                unique_sites_value, process_overhead_value);
}

// Test to verify that a sandboxed iframes with about:blank doesn't get counted
// in the metrics.
IN_PROC_BROWSER_TEST_F(IsolatedSandboxedIframeBrowserTest,
                       NotIsolatedSandboxAboutBlankSubframe) {
  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  VerifyStartupMetrics();

  // Create sandboxed child frame, with about:blank content.
  {
    std::string js_str(
        "var frame = document.createElement('iframe'); "
        "frame.id = 'child_frame'; "
        "frame.sandbox = ''; "
        "frame.src = 'about:blank'; "
        "document.body.appendChild(frame);");
    EXPECT_TRUE(ExecJs(web_contents->GetPrimaryMainFrame(), js_str));
    ASSERT_TRUE(WaitForLoadStop(web_contents));
  }

  // Verify histograms are updated.
  int isolatable_sandboxed_iframes_value = 0;
  int unique_origins_value = 0;
  int unique_sites_value = 0;
  int process_overhead_value = 0;
  VerifyMetrics(isolatable_sandboxed_iframes_value, unique_origins_value,
                unique_sites_value, process_overhead_value);
}

// Test to verify that a sandboxed iframe with an empty url (nothing committed)
// doesn't get counted in the metrics.
IN_PROC_BROWSER_TEST_F(IsolatedSandboxedIframeBrowserTest,
                       NotIsolatedSandboxEmptyUrlSubframe) {
  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  VerifyStartupMetrics();

  // Create sandboxed child frame, with about:blank content.
  {
    GURL empty_url(embedded_test_server()->GetURL("a.com", "/page204.html"));
    std::string js_str = base::StringPrintf(
        "var frame = document.createElement('iframe'); "
        "frame.id = 'child_frame'; "
        "frame.sandbox = ''; "
        "frame.src = '%s'; "
        "document.body.appendChild(frame);",
        empty_url.spec().c_str());
    EXPECT_TRUE(ExecJs(web_contents->GetPrimaryMainFrame(), js_str));
    ASSERT_TRUE(WaitForLoadStop(web_contents));
  }

  // Verify histograms are updated.
  int isolatable_sandboxed_iframes_value = 0;
  int unique_origins_value = 0;
  int unique_sites_value = 0;
  int process_overhead_value = 0;
  VerifyMetrics(isolatable_sandboxed_iframes_value, unique_origins_value,
                unique_sites_value, process_overhead_value);
}

// Test to verify that a javascript: sandboxed iframe does not generate any
// metrics.
IN_PROC_BROWSER_TEST_F(IsolatedSandboxedIframeBrowserTest,
                       SandboxedIframeWithJSUrl) {
  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  VerifyStartupMetrics();

  // Create sandboxed child frame with a javascript: URL.
  std::string js_url_str("javascript:\"foo\"");
  {
    std::string js_str = base::StringPrintf(
        "var frame = document.createElement('iframe'); "
        "frame.id = 'test_frame'; "
        "frame.sandbox = 'allow-scripts'; "
        "frame.src = '%s'; "
        "document.body.appendChild(frame);",
        js_url_str.c_str());
    EXPECT_TRUE(ExecJs(web_contents->GetPrimaryMainFrame(), js_str));
    ASSERT_TRUE(WaitForLoadStop(web_contents));
  }

  // Verify histograms are updated.
  int isolatable_sandboxed_iframes_value = 0;
  int unique_origins_value = 0;
  int unique_sites_value = 0;
  int process_overhead_value = 0;
  VerifyMetrics(isolatable_sandboxed_iframes_value, unique_origins_value,
                unique_sites_value, process_overhead_value);
}

// Verify that when the flag for isolating sandboxed iframe is off, we collect
// metrics for isolatable iframe count and number of unique origins, but no
// metrics for actual process overhead.
IN_PROC_BROWSER_TEST_F(NotIsolatedSandboxedIframeBrowserTest, IsolatedSandbox) {
  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  // The child needs to have the same origin as the parent.
  GURL child_url(main_url);
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  VerifyStartupMetrics();

  // Create sandboxed child frame, same-origin.
  {
    std::string js_str = base::StringPrintf(
        "var frame = document.createElement('iframe'); "
        "frame.sandbox = ''; "
        "frame.src = '%s'; "
        "document.body.appendChild(frame);",
        child_url.spec().c_str());
    EXPECT_TRUE(ExecJs(web_contents->GetPrimaryMainFrame(), js_str));
    ASSERT_TRUE(WaitForLoadStop(web_contents));
  }

  // Verify histograms are updated.
  int isolatable_sandboxed_iframes_value = 1;
  int unique_origins_value = 1;
  int unique_sites_value = 1;
  int process_overhead_value = 0;
  VerifyMetrics(isolatable_sandboxed_iframes_value, unique_origins_value,
                unique_sites_value, process_overhead_value);
}

// Test to make sure that a sandboxed mainframe is considered isolatable if it
// has a same-site opener.
IN_PROC_BROWSER_TEST_F(IsolatedSandboxedIframeBrowserTest,
                       SandboxedMainframeWithSameSiteOpener) {
  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL child_url(main_url);
  GURL popup_url(main_url);
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  VerifyStartupMetrics();

  {
    std::string js_str = base::StringPrintf(
        "var frame = document.createElement('iframe'); "
        "frame.id = 'test_frame'; "
        "frame.sandbox = 'allow-popups allow-scripts'; "
        "frame.src = '%s'; "
        "document.body.appendChild(frame);",
        child_url.spec().c_str());
    EXPECT_TRUE(ExecJs(web_contents->GetPrimaryMainFrame(), js_str));
    ASSERT_TRUE(WaitForLoadStop(web_contents));
  }

  // Open popup from sandboxed iframe. This creates a sandboxed mainframe that
  // is isolatable. We make sure the popup url is same-site to the opener,
  // otherwise it would just create an OOPIF.
  content::RenderFrameHost* child_rfh =
      ChildFrameAt(web_contents->GetPrimaryMainFrame(), 0);
  {
    std::string js_str =
        base::StringPrintf("window.open('%s');", popup_url.spec().c_str());

    content::TestNavigationObserver popup_observer(nullptr);
    popup_observer.StartWatchingNewWebContents();
    EXPECT_TRUE(ExecJs(child_rfh, js_str));
    popup_observer.Wait();
  }

  // Verify histograms are updated.
  int isolatable_sandboxed_iframes_value = 2;
  int unique_origins_value = 1;
  int unique_sites_value = 1;
  int process_overhead_value = 1;
  VerifyMetrics(isolatable_sandboxed_iframes_value, unique_origins_value,
                unique_sites_value, process_overhead_value);
}

// Test to make sure that a CSP sandboxed mainframe is considered isolatable
// when opened by a non-sandboxed parent.
IN_PROC_BROWSER_TEST_F(IsolatedSandboxedIframeBrowserTest,
                       CspSandboxedMainframeWithSameSiteOpener) {
  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL popup_url(embedded_test_server()->GetURL("a.com", "/csp-sandbox.html"));
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  VerifyStartupMetrics();

  // Open popup from (non-sandboxed) main frame. The popup will arrive with a
  // CSP sandbox header, and so will be marked isolatable since it is same-site
  // to its opener.
  {
    std::string js_str =
        base::StringPrintf("window.open('%s');", popup_url.spec().c_str());

    content::TestNavigationObserver popup_observer(nullptr);
    popup_observer.StartWatchingNewWebContents();
    EXPECT_TRUE(ExecJs(web_contents->GetPrimaryMainFrame(), js_str));
    popup_observer.Wait();
  }

  // Verify histograms are updated.
  int isolatable_sandboxed_iframes_value = 1;
  int unique_origins_value = 1;
  int unique_sites_value = 1;
  int process_overhead_value = 1;
  VerifyMetrics(isolatable_sandboxed_iframes_value, unique_origins_value,
                unique_sites_value, process_overhead_value);
}

// Test to make sure that a CSP sandboxed mainframe is not considered isolatable
// when when visited directly.
IN_PROC_BROWSER_TEST_F(IsolatedSandboxedIframeBrowserTest,
                       CspSandboxedMainframeVisitedDirectly) {
  GURL main_url(embedded_test_server()->GetURL("a.com", "/csp-sandbox.html"));
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));

  // Verify histograms are updated.
  int isolatable_sandboxed_iframes_value = 0;
  int unique_origins_value = 0;
  int unique_sites_value = 0;
  int process_overhead_value = 0;
  VerifyMetrics(isolatable_sandboxed_iframes_value, unique_origins_value,
                unique_sites_value, process_overhead_value);
}

// Test to make sure that an iframe with a data:url is appropriately counted by
// the sandbox isolation metrics.
IN_PROC_BROWSER_TEST_F(IsolatedSandboxedIframeBrowserTest,
                       SandboxedIframeWithDataURL) {
  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  VerifyStartupMetrics();

  // Create sandboxed child frame with a data URL.
  std::string data_url_str("data:text/html,dataurl");
  {
    std::string js_str = base::StringPrintf(
        "var frame = document.createElement('iframe'); "
        "frame.id = 'test_frame'; "
        "frame.sandbox = ''; "
        "frame.src = '%s'; "
        "document.body.appendChild(frame);",
        data_url_str.c_str());
    EXPECT_TRUE(ExecJs(web_contents->GetPrimaryMainFrame(), js_str));
    ASSERT_TRUE(WaitForLoadStop(web_contents));
  }

  // Verify histograms are updated.
  int isolatable_sandboxed_iframes_value = 1;
  int unique_origins_value = 1;
  int unique_sites_value = 1;
  int process_overhead_value = 1;
  VerifyMetrics(isolatable_sandboxed_iframes_value, unique_origins_value,
                unique_sites_value, process_overhead_value);
}
