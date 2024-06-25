// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/metrics/metrics_memory_details.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
#include "components/variations/active_field_trials.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/public/cpp/features.h"
#include "url/gurl.h"

using base::Bucket;

namespace {

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

  // Returns a HistogramTester which observed the most recent call to
  // StartFetchAndWait().
  base::HistogramTester* uma() { return uma_.get(); }

  int GetTotalProcessCount() {
    std::vector<Bucket> buckets = uma_->GetAllSamples(
        "Memory.RenderProcessHost.Count.InitializedAndNotDead");
    DCHECK(buckets.size() == 1U);
    return buckets[0].min;
  }

  int GetOacProcessCount() {
    std::vector<Bucket> buckets = uma_->GetAllSamples(
        "Memory.RenderProcessHost.Count.OriginAgentClusterOverhead");
    // The bucket size will be zero when testing with OriginAgentCluster
    // disabled.
    CHECK(buckets.size() == 1U || buckets.size() == 0U);
    return buckets.size() == 1U ? buckets[0].min : 0;
  }

  int GetOacProcessCountPercent() {
    std::vector<Bucket> buckets = uma_->GetAllSamples(
        "Memory.RenderProcessHost.Percent.OriginAgentClusterOverhead");
    // The bucket size will be zero when testing with OriginAgentCluster
    // disabled.
    CHECK(buckets.size() == 1U || buckets.size() == 0U);
    return buckets.size() == 1U ? buckets[0].min : 0;
  }

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

// Matches a container of histogram samples, for the common case where the
// histogram received just one sample.
#define HasOneSample(x) ElementsAre(Sample(x, 1))

}  // namespace

// General browsertests for the Origin-Agent-Cluster header can be found in
// content/browser/isolated_origin_browsertest.cc. However testing metrics
// related behavior is best done from chrome/; thus, this file exists.

class OriginAgentClusterBrowserTest : public InProcessBrowserTest {
 public:
  OriginAgentClusterBrowserTest()
      : OriginAgentClusterBrowserTest(true /* enable_origin_agent_cluster_*/) {}

  OriginAgentClusterBrowserTest(const OriginAgentClusterBrowserTest&) = delete;
  OriginAgentClusterBrowserTest& operator=(
      const OriginAgentClusterBrowserTest&) = delete;

  ~OriginAgentClusterBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());

    // Start the HTTPS server here so we can properly get the URL for the
    // command-line isolated origin.
    https_server()->AddDefaultHandlers(GetChromeTestDataDir());
    https_server()->RegisterRequestHandler(
        base::BindRepeating(&OriginAgentClusterBrowserTest::HandleResponse,
                            base::Unretained(this)));
    ASSERT_TRUE(https_server()->Start());

    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
    std::string origin_list =
        https_server()->GetURL("isolated.foo.com", "/").spec();
    command_line->AppendSwitchASCII(switches::kIsolateOrigins, origin_list);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->StartAcceptingConnections();
  }

  void TearDownOnMainThread() override {
    ASSERT_TRUE(https_server()->ShutdownAndWaitUntilComplete());
  }

  net::EmbeddedTestServer* https_server() { return &https_server_; }

 protected:
  explicit OriginAgentClusterBrowserTest(bool enable_oac)
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS),
        enable_origin_agent_cluster_(enable_oac) {
    // To keep the tests easier to reason about, turn off both the spare
    // renderer process and process reuse for subframes in different
    // BrowsingInstances.
    if (enable_origin_agent_cluster_) {
      feature_list_.InitWithFeatures(
          /* enable_features */ {features::kOriginIsolationHeader,
                                 features::kDisableProcessReuse},
          /* disable_features */ {features::kSpareRendererForSitePerProcess});
    } else {
      feature_list_.InitWithFeatures(
          /* enable_features */ {features::kDisableProcessReuse},
          /* disable_features */ {features::kOriginIsolationHeader,
                                  features::kSpareRendererForSitePerProcess});
    }
  }

 private:
  std::unique_ptr<net::test_server::HttpResponse> HandleResponse(
      const net::test_server::HttpRequest& request) {
    if (request.relative_url == "/origin_key_me") {
      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_code(net::HTTP_OK);
      response->set_content_type("text/html");
      response->AddCustomHeader("Origin-Agent-Cluster", "?1");
      response->set_content("I like origin keys!");
      return std::move(response);
    } else if (request.relative_url == "/origin_key_me_iframe") {
      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_code(net::HTTP_OK);
      response->set_content_type("text/html");
      response->AddCustomHeader("Origin-Agent-Cluster", "?1");
      response->set_content("<body><iframe id='test'></iframe></body>");
      return std::move(response);
    }

    return nullptr;
  }

  net::EmbeddedTestServer https_server_;
  base::test::ScopedFeatureList feature_list_;
  bool enable_origin_agent_cluster_;
};

class OriginAgentClusterDisabledBrowserTest
    : public OriginAgentClusterBrowserTest {
 public:
  OriginAgentClusterDisabledBrowserTest()
      : OriginAgentClusterBrowserTest(false /* enable_origin_agent_cluster_*/) {
  }
  OriginAgentClusterDisabledBrowserTest(
      const OriginAgentClusterDisabledBrowserTest&) = delete;
  OriginAgentClusterDisabledBrowserTest& operator=(
      const OriginAgentClusterDisabledBrowserTest&) = delete;

  ~OriginAgentClusterDisabledBrowserTest() override = default;
};  // class OriginAgentClusterDisabledBrowserTest

IN_PROC_BROWSER_TEST_F(OriginAgentClusterBrowserTest, Navigations) {
  GURL start_url(https_server()->GetURL("foo.com", "/iframe.html"));
  GURL origin_keyed_url(
      https_server()->GetURL("origin-keyed.foo.com", "/origin_key_me"));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  auto web_feature_waiter =
      std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
          web_contents);
  web_feature_waiter->AddWebFeatureExpectation(
      blink::mojom::WebFeature::kOriginAgentClusterHeader);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), start_url));

  EXPECT_FALSE(web_feature_waiter->DidObserveWebFeature(
      blink::mojom::WebFeature::kOriginAgentClusterHeader));

  EXPECT_TRUE(NavigateIframeToURL(web_contents, "test", origin_keyed_url));

  web_feature_waiter->Wait();
}

IN_PROC_BROWSER_TEST_F(OriginAgentClusterBrowserTest,
                       ProcessCountMetricsSimple) {
  GURL start_url(https_server()->GetURL("foo.com", "/iframe.html"));
  GURL origin_keyed_url(
      https_server()->GetURL("origin-keyed.foo.com", "/origin_key_me"));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), start_url));
  EXPECT_TRUE(NavigateIframeToURL(web_contents, "test", origin_keyed_url));

  // Get the metrics.
  scoped_refptr<TestMemoryDetails> details = new TestMemoryDetails();
  details->StartFetchAndWait();

  EXPECT_EQ(2, details->GetTotalProcessCount());
  EXPECT_EQ(1, details->GetOacProcessCount());
  EXPECT_EQ(50, details->GetOacProcessCountPercent());
}

// Same as OriginAgentClusterBrowserTest.ProcessCountMetricsSimple, but with
// OriginAgentCluster disabled, so no metrics should be recorded.
IN_PROC_BROWSER_TEST_F(OriginAgentClusterDisabledBrowserTest,
                       ProcessCountMetricsSimple) {
  GURL start_url(https_server()->GetURL("foo.com", "/iframe.html"));
  GURL origin_keyed_url(
      https_server()->GetURL("origin-keyed.foo.com", "/origin_key_me"));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), start_url));
  EXPECT_TRUE(NavigateIframeToURL(web_contents, "test", origin_keyed_url));

  // Get the metrics.
  scoped_refptr<TestMemoryDetails> details = new TestMemoryDetails();
  details->StartFetchAndWait();

  EXPECT_EQ(1, details->GetTotalProcessCount());
  EXPECT_EQ(0, details->GetOacProcessCount());
  EXPECT_EQ(0, details->GetOacProcessCountPercent());
}

// In the case where we load an OAC origin with no base-origin, we expect zero
// overhead since the isolated origin only creates a single process, and no
// process is created for the base-origin.
IN_PROC_BROWSER_TEST_F(OriginAgentClusterBrowserTest,
                       ProcessCountMetricsNoBaseOrigin) {
  GURL origin_keyed_url(
      https_server()->GetURL("origin-keyed.foo.com", "/origin_key_me"));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), origin_keyed_url));

  // Get the metrics.
  scoped_refptr<TestMemoryDetails> details = new TestMemoryDetails();
  details->StartFetchAndWait();

  EXPECT_EQ(1, details->GetTotalProcessCount());
  EXPECT_EQ(0, details->GetOacProcessCount());
  EXPECT_EQ(0, details->GetOacProcessCountPercent());
}

// We expect the OAC overhead to be zero when no OAC origins are present.
IN_PROC_BROWSER_TEST_F(OriginAgentClusterBrowserTest,
                       ProcessCountMetricsNoOACs) {
  bool origin_keyed_processes_by_default =
      content::SiteIsolationPolicy::AreOriginKeyedProcessesEnabledByDefault();
  GURL start_url(https_server()->GetURL("foo.com", "/iframe.html"));
  GURL sub_origin_url(https_server()->GetURL("sub.foo.com", "/title1.html"));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), start_url));
  EXPECT_TRUE(NavigateIframeToURL(web_contents, "test", sub_origin_url));

  // Get the metrics.
  scoped_refptr<TestMemoryDetails> details = new TestMemoryDetails();
  details->StartFetchAndWait();

  if (origin_keyed_processes_by_default) {
    // Even though sub.foo.com doesn't have an OAC opt-in header, it will still
    // be isolated in this case due to the Origin Isolation mode, and thus it
    // should still count as overhead.
    EXPECT_EQ(2, details->GetTotalProcessCount());
    EXPECT_EQ(1, details->GetOacProcessCount());
    EXPECT_EQ(50, details->GetOacProcessCountPercent());
  } else {
    EXPECT_EQ(1, details->GetTotalProcessCount());
    EXPECT_EQ(0, details->GetOacProcessCount());
    EXPECT_EQ(0, details->GetOacProcessCountPercent());
  }
}

// Two distinct OAC sub-origins with a base-origin should have an overhead of
// two processes.
IN_PROC_BROWSER_TEST_F(OriginAgentClusterBrowserTest,
                       ProcessCountMetricsTwoSubOrigins) {
  GURL start_url(https_server()->GetURL("foo.com", "/two_iframes_blank.html"));
  GURL origin_keyed_url1(
      https_server()->GetURL("sub1.foo.com", "/origin_key_me"));
  GURL origin_keyed_url2(
      https_server()->GetURL("sub2.foo.com", "/origin_key_me"));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), start_url));
  EXPECT_TRUE(NavigateIframeToURL(web_contents, "iframe1", origin_keyed_url1));
  EXPECT_TRUE(NavigateIframeToURL(web_contents, "iframe2", origin_keyed_url2));

  // Get the metrics.
  scoped_refptr<TestMemoryDetails> details = new TestMemoryDetails();
  details->StartFetchAndWait();

  EXPECT_EQ(3, details->GetTotalProcessCount());
  EXPECT_EQ(2, details->GetOacProcessCount());
  EXPECT_EQ(66, details->GetOacProcessCountPercent());
}

// This test loads the same base-origin with an isolated sub-origin in each of
// two tabs. Each tab represents a separate BrowsingInstance, so we expect the
// OAC overhead of 1 to be counted twice.
IN_PROC_BROWSER_TEST_F(OriginAgentClusterBrowserTest,
                       ProcessCountMetricsTwoTabs) {
  GURL start_url(https_server()->GetURL("foo.com", "/iframe.html"));
  GURL origin_keyed_url(
      https_server()->GetURL("sub.foo.com", "/origin_key_me"));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), start_url));
  content::WebContents* tab1 =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  EXPECT_TRUE(NavigateIframeToURL(tab1, "test", origin_keyed_url));
  // Open two a.com tabs (with cross site http iframes). IsolateExtensions mode
  // should have no effect so far, since there are no frames straddling the
  // extension/web boundary.
  ASSERT_TRUE(AddTabAtIndex(1, start_url, ui::PAGE_TRANSITION_TYPED));
  content::WebContents* tab2 =
      browser()->tab_strip_model()->GetWebContentsAt(1);
  EXPECT_TRUE(NavigateIframeToURL(tab2, "test", origin_keyed_url));

  EXPECT_NE(tab1, tab2);

  // Get the metrics.
  scoped_refptr<TestMemoryDetails> details = new TestMemoryDetails();
  details->StartFetchAndWait();

  EXPECT_EQ(4, details->GetTotalProcessCount());
  EXPECT_EQ(2, details->GetOacProcessCount());
  EXPECT_EQ(50, details->GetOacProcessCountPercent());
}

// Make sure command-line isolated origins don't trigger the OAC metrics.
IN_PROC_BROWSER_TEST_F(OriginAgentClusterBrowserTest,
                       ProcessCountMetricsNoCmdLineIsolation) {
  GURL main_frame_url(https_server()->GetURL("foo.com", "/iframe.html"));
  GURL cmd_line_isolated_url(
      https_server()->GetURL("isolated.foo.com", "/title1.html"));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));
  EXPECT_TRUE(NavigateIframeToURL(web_contents, "test", cmd_line_isolated_url));

  // Make sure we got two SiteInstances.
  auto* main_frame = web_contents->GetPrimaryMainFrame();
  auto* child_frame = ChildFrameAt(main_frame, 0);
  EXPECT_NE(main_frame->GetSiteInstance(), child_frame->GetSiteInstance());

  // Get the metrics.
  scoped_refptr<TestMemoryDetails> details = new TestMemoryDetails();
  details->StartFetchAndWait();

  EXPECT_EQ(0, details->GetOacProcessCount());
  EXPECT_EQ(0, details->GetOacProcessCountPercent());
}

// Make sure command-line isolated origins don't trigger the OAC metrics.
// Same as ProcessCountMetricsNoCmdLineIsolation but isolated child has OAC.
// We don't consider this as overhead because the extra process would still
// exist for this user even without OAC.
IN_PROC_BROWSER_TEST_F(OriginAgentClusterBrowserTest,
                       ProcessCountMetricsNoCmdLineIsolationWithOAC1) {
  GURL main_frame_url(https_server()->GetURL("foo.com", "/iframe.html"));
  GURL cmd_line_isolated_url(
      https_server()->GetURL("isolated.foo.com", "/origin_key_me"));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));
  EXPECT_TRUE(NavigateIframeToURL(web_contents, "test", cmd_line_isolated_url));

  // Make sure we got two SiteInstances.
  auto* main_frame = web_contents->GetPrimaryMainFrame();
  auto* child_frame = ChildFrameAt(main_frame, 0);
  EXPECT_NE(main_frame->GetSiteInstance(), child_frame->GetSiteInstance());

  // Get the metrics.
  scoped_refptr<TestMemoryDetails> details = new TestMemoryDetails();
  details->StartFetchAndWait();

  EXPECT_EQ(0, details->GetOacProcessCount());
  EXPECT_EQ(0, details->GetOacProcessCountPercent());
}

// Make sure command-line isolated origins don't trigger the OAC metrics.
// Same as ProcessCountMetricsNoCmdLineIsolation but both mainframe and child
// have OAC.
IN_PROC_BROWSER_TEST_F(OriginAgentClusterBrowserTest,
                       ProcessCountMetricsNoCmdLineIsolationWithOAC2) {
  GURL main_frame_url(
      https_server()->GetURL("foo.com", "/origin_key_me_iframe"));
  GURL cmd_line_isolated_url(
      https_server()->GetURL("isolated.foo.com", "/origin_key_me"));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));
  EXPECT_TRUE(NavigateIframeToURL(web_contents, "test", cmd_line_isolated_url));

  // Make sure we got two SiteInstances.
  auto* main_frame = web_contents->GetPrimaryMainFrame();
  auto* child_frame = ChildFrameAt(main_frame, 0);
  EXPECT_NE(main_frame->GetSiteInstance(), child_frame->GetSiteInstance());

  // Get the metrics.
  scoped_refptr<TestMemoryDetails> details = new TestMemoryDetails();
  details->StartFetchAndWait();

  EXPECT_EQ(0, details->GetOacProcessCount());
  EXPECT_EQ(0, details->GetOacProcessCountPercent());
}

// Make sure command-line isolated origins don't trigger the OAC metrics.
// Same as ProcessCountMetricsNoCmdLineIsolation but mainframe has OAC.
IN_PROC_BROWSER_TEST_F(OriginAgentClusterBrowserTest,
                       ProcessCountMetricsNoCmdLineIsolationWithOAC3) {
  GURL main_frame_url(
      https_server()->GetURL("foo.com", "/origin_key_me_iframe"));
  GURL cmd_line_isolated_url(
      https_server()->GetURL("isolated.foo.com", "/title1.html"));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));
  EXPECT_TRUE(NavigateIframeToURL(web_contents, "test", cmd_line_isolated_url));

  // Make sure we got two SiteInstances.
  auto* main_frame = web_contents->GetPrimaryMainFrame();
  auto* child_frame = ChildFrameAt(main_frame, 0);
  EXPECT_NE(main_frame->GetSiteInstance(), child_frame->GetSiteInstance());

  // Get the metrics.
  scoped_refptr<TestMemoryDetails> details = new TestMemoryDetails();
  details->StartFetchAndWait();

  EXPECT_EQ(0, details->GetOacProcessCount());
  EXPECT_EQ(0, details->GetOacProcessCountPercent());
}
