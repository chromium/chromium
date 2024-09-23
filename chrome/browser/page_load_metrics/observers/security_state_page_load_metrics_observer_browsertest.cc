// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/security_state_page_load_metrics_observer.h"

#include <memory>

#include "base/feature_list.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/lookalikes/lookalike_test_helper.h"
#include "chrome/browser/lookalikes/safety_tip_web_contents_observer.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/lookalikes/core/safety_tip_test_utils.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

using UkmEntry = ukm::builders::Security_SiteEngagement;

// A WebContentsObserver to allow waiting on a change in visible security state.
class SecurityStyleTestObserver : public content::WebContentsObserver {
 public:
  explicit SecurityStyleTestObserver(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents) {}

  SecurityStyleTestObserver(const SecurityStyleTestObserver&) = delete;
  SecurityStyleTestObserver& operator=(const SecurityStyleTestObserver&) =
      delete;

  ~SecurityStyleTestObserver() override {}

  void DidChangeVisibleSecurityState() override { run_loop_.Quit(); }

  void WaitForDidChangeVisibleSecurityState() { run_loop_.Run(); }

 private:
  base::RunLoop run_loop_;
};

class SecurityStatePageLoadMetricsBrowserTest : public InProcessBrowserTest {
 public:
  SecurityStatePageLoadMetricsBrowserTest() {}

  SecurityStatePageLoadMetricsBrowserTest(
      const SecurityStatePageLoadMetricsBrowserTest&) = delete;
  SecurityStatePageLoadMetricsBrowserTest& operator=(
      const SecurityStatePageLoadMetricsBrowserTest&) = delete;

  ~SecurityStatePageLoadMetricsBrowserTest() override {}

  void PreRunTestOnMainThread() override {
    InProcessBrowserTest::PreRunTestOnMainThread();
    test_ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("accounts-google.com", "127.0.0.1");

    LookalikeTestHelper::SetUpLookalikeTestParams();
  }

  void TearDownOnMainThread() override {
    LookalikeTestHelper::TearDownLookalikeTestParams();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    mock_cert_verifier_.SetUpCommandLine(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    mock_cert_verifier_.SetUpInProcessBrowserTestFixture();
  }

  void TearDownInProcessBrowserTestFixture() override {
    mock_cert_verifier_.TearDownInProcessBrowserTestFixture();
  }

 protected:
  void StartHttpsServer(net::EmbeddedTestServer::ServerCertificate cert) {
    if (cert == net::EmbeddedTestServer::CERT_OK) {
      mock_cert_verifier_.mock_cert_verifier()->set_default_result(net::OK);
    }

    https_test_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_test_server_->SetSSLConfig(cert);
    https_test_server_->ServeFilesFromSourceDirectory(GetChromeTestDataDir());
    ASSERT_TRUE(https_test_server_->Start());
  }

  void StartHttpServer() {
    http_test_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTP);
    http_test_server_->ServeFilesFromSourceDirectory(GetChromeTestDataDir());
    ASSERT_TRUE(http_test_server_->Start());
  }

  void CloseAllTabs() {
    TabStripModel* tab_strip_model = browser()->tab_strip_model();
    content::WebContentsDestroyedWatcher destroyed_watcher(
        tab_strip_model->GetActiveWebContents());
    tab_strip_model->CloseAllTabs();
    destroyed_watcher.Wait();
  }

  // Checks UKM results for a specific URL and metric.
  void ExpectMetricForUrl(const GURL& url,
                          const char* metric_name,
                          int64_t expected_value) {
    // Find last entry matching |url|.
    const ukm::mojom::UkmEntry* last = nullptr;
    for (const ukm::mojom::UkmEntry* entry :
         test_ukm_recorder_->GetEntriesByName(UkmEntry::kEntryName)) {
      auto* source = test_ukm_recorder_->GetSourceForSourceId(entry->source_id);
      if (source && source->url() == url)
        last = entry;
    }
    ASSERT_TRUE(last);
    if (last) {
      test_ukm_recorder_->ExpectEntrySourceHasUrl(last, url);
      test_ukm_recorder_->ExpectEntryMetric(last, metric_name, expected_value);
    }
  }

  size_t CountUkmEntries() {
    return test_ukm_recorder_->GetEntriesByName(UkmEntry::kEntryName).size();
  }

  base::HistogramTester* histogram_tester() const {
    return histogram_tester_.get();
  }
  net::EmbeddedTestServer* https_test_server() {
    return https_test_server_.get();
  }
  net::EmbeddedTestServer* http_test_server() {
    return http_test_server_.get();
  }

  void ClearUkmRecorder() {
    test_ukm_recorder_.reset();
    test_ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }

 private:
  std::unique_ptr<base::HistogramTester> histogram_tester_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> test_ukm_recorder_;
  std::unique_ptr<net::EmbeddedTestServer> https_test_server_;
  std::unique_ptr<net::EmbeddedTestServer> http_test_server_;
  content::ContentMockCertVerifier mock_cert_verifier_;
};

IN_PROC_BROWSER_TEST_F(SecurityStatePageLoadMetricsBrowserTest, Simple_Https) {
  StartHttpsServer(net::EmbeddedTestServer::CERT_OK);
  GURL url = https_test_server()->GetURL("/simple.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  CloseAllTabs();

  // Site Engagement metrics.
  histogram_tester()->ExpectTotalCount(
      SecurityStatePageLoadMetricsObserver::
          GetEngagementFinalHistogramNameForTesting(security_state::NONE),
      0);
  histogram_tester()->ExpectTotalCount(
      SecurityStatePageLoadMetricsObserver::
          GetEngagementFinalHistogramNameForTesting(security_state::SECURE),
      1);
  EXPECT_EQ(1u, CountUkmEntries());
  ExpectMetricForUrl(url, UkmEntry::kInitialSecurityLevelName,
                     security_state::SECURE);
  ExpectMetricForUrl(url, UkmEntry::kFinalSecurityLevelName,
                     security_state::SECURE);

  // Navigation metrics.
  histogram_tester()->ExpectUniqueSample(
      SecurityStatePageLoadMetricsObserver::
          GetSecurityLevelPageEndReasonHistogramNameForTesting(
              security_state::SECURE),
      page_load_metrics::END_CLOSE, 1);
}

IN_PROC_BROWSER_TEST_F(SecurityStatePageLoadMetricsBrowserTest, Simple_Http) {
  StartHttpServer();
  GURL url = http_test_server()->GetURL("/simple.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  CloseAllTabs();

  histogram_tester()->ExpectTotalCount(
      SecurityStatePageLoadMetricsObserver::
          GetEngagementFinalHistogramNameForTesting(security_state::NONE),
      1);
  histogram_tester()->ExpectTotalCount(
      SecurityStatePageLoadMetricsObserver::
          GetEngagementFinalHistogramNameForTesting(security_state::SECURE),
      0);
  EXPECT_EQ(1u, CountUkmEntries());
  ExpectMetricForUrl(url, UkmEntry::kInitialSecurityLevelName,
                     security_state::NONE);
  ExpectMetricForUrl(url, UkmEntry::kFinalSecurityLevelName,
                     security_state::NONE);
}

IN_PROC_BROWSER_TEST_F(SecurityStatePageLoadMetricsBrowserTest, ReloadPage) {
  StartHttpsServer(net::EmbeddedTestServer::CERT_OK);
  GURL url = https_test_server()->GetURL("/simple.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  EXPECT_TRUE(content::WaitForLoadStop(contents));

  histogram_tester()->ExpectTotalCount(
      SecurityStatePageLoadMetricsObserver::
          GetEngagementFinalHistogramNameForTesting(security_state::NONE),
      0);
  histogram_tester()->ExpectTotalCount(
      SecurityStatePageLoadMetricsObserver::
          GetEngagementFinalHistogramNameForTesting(security_state::SECURE),
      1);
  EXPECT_EQ(1u, CountUkmEntries());
  ExpectMetricForUrl(url, UkmEntry::kInitialSecurityLevelName,
                     security_state::SECURE);
  ExpectMetricForUrl(url, UkmEntry::kFinalSecurityLevelName,
                     security_state::SECURE);

  histogram_tester()->ExpectUniqueSample(
      SecurityStatePageLoadMetricsObserver::
          GetSecurityLevelPageEndReasonHistogramNameForTesting(
              security_state::SECURE),
      page_load_metrics::END_RELOAD, 1);
}

// Tests that Site Engagement histograms are recorded correctly on a page with a
// Safety Tip.
IN_PROC_BROWSER_TEST_F(SecurityStatePageLoadMetricsBrowserTest,
                       SafetyTipSiteEngagement) {
  const std::string kSiteEngagementHistogramPrefix = "Security.SiteEngagement.";
  StartHttpsServer(net::EmbeddedTestServer::CERT_OK);

  struct TestCase {
    // The URL to navigate to.
    GURL url;
    // If true, url is expected to show a safety tip.
    bool expect_safety_tip;
  } kTestCases[] = {
      {https_test_server()->GetURL("/simple.html"), false},
      {https_test_server()->GetURL("accounts-google.com", "/simple.html"),
       true},
  };

  // The histogram should be recorded regardless of whether the page is flagged
  // with a Safety Tip or not.
  for (const TestCase& test_case : kTestCases) {
    ClearUkmRecorder();
    base::HistogramTester histogram_tester;

    chrome::AddTabAt(browser(), GURL(), -1, true);
    content::WebContents* contents =
        browser()->tab_strip_model()->GetActiveWebContents();

    SafetyTipWebContentsObserver* safety_tip_observer =
        SafetyTipWebContentsObserver::FromWebContents(contents);
    ASSERT_TRUE(safety_tip_observer);

    // Navigate to |url| and wait for the safety tip check to complete before
    // checking the histograms.
    base::RunLoop run_loop;
    safety_tip_observer->RegisterSafetyTipCheckCallbackForTesting(
        run_loop.QuitClosure());
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_case.url));
    run_loop.Run();

    // The UKM isn't recorded until the page is destroyed.
    int previous_tab_count = browser()->tab_strip_model()->count();
    browser()->tab_strip_model()->CloseWebContentsAt(1,
                                                     TabCloseTypes::CLOSE_NONE);
    ASSERT_EQ(previous_tab_count - 1, browser()->tab_strip_model()->count());

    histogram_tester.ExpectTotalCount(
        kSiteEngagementHistogramPrefix + (test_case.expect_safety_tip
                                              ? "SafetyTip_Lookalike"
                                              : "SafetyTip_None"),
        1);
    EXPECT_EQ(1u, CountUkmEntries());
    ExpectMetricForUrl(
        test_case.url, UkmEntry::kSafetyTipStatusName,
        static_cast<int64_t>(test_case.expect_safety_tip
                                 ? security_state::SafetyTipStatus::kLookalike
                                 : security_state::SafetyTipStatus::kNone));
  }
}

// Tests that the Safety Tip page end reason histogram is recorded properly, by
// reloading the page and checking that the reload-page reason is recorded in
// the histogram.
IN_PROC_BROWSER_TEST_F(SecurityStatePageLoadMetricsBrowserTest,
                       ReloadPageWithSafetyTip) {
  StartHttpServer();

  struct TestCase {
    // The URL to navigate to.
    GURL url;
    // If true, url is expected to show a safety tip.
    bool expect_safety_tip;
  } kTestCases[] = {
      {http_test_server()->GetURL("/simple.html"), false},
      {http_test_server()->GetURL("accounts-google.com", "/simple.html"), true},
  };

  // The histogram should be recorded regardless of whether the page is flagged
  // with a Safety Tip or not.
  for (const TestCase& test_case : kTestCases) {
    base::HistogramTester histogram_tester;

    content::WebContents* contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    SafetyTipWebContentsObserver* safety_tip_observer =
        SafetyTipWebContentsObserver::FromWebContents(contents);
    ASSERT_TRUE(safety_tip_observer);

    // Navigate to the lookalike and wait for the safety tip check to complete
    // before checking the histograms.
    base::RunLoop run_loop;
    safety_tip_observer->RegisterSafetyTipCheckCallbackForTesting(
        run_loop.QuitClosure());
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_case.url));
    run_loop.Run();

    chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
    EXPECT_TRUE(content::WaitForLoadStop(contents));

    histogram_tester.ExpectUniqueSample(
        SecurityStatePageLoadMetricsObserver::
            GetSafetyTipPageEndReasonHistogramNameForTesting(
                test_case.expect_safety_tip
                    ? security_state::SafetyTipStatus::kLookalike
                    : security_state::SafetyTipStatus::kNone),
        page_load_metrics::END_RELOAD, 1);
  }
}

IN_PROC_BROWSER_TEST_F(SecurityStatePageLoadMetricsBrowserTest, OtherScheme) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(chrome::kChromeUIVersionURL)));
  CloseAllTabs();
  histogram_tester()->ExpectTotalCount(
      SecurityStatePageLoadMetricsObserver::
          GetEngagementFinalHistogramNameForTesting(security_state::NONE),
      0);
  histogram_tester()->ExpectTotalCount(
      SecurityStatePageLoadMetricsObserver::
          GetEngagementFinalHistogramNameForTesting(security_state::SECURE),
      0);
  EXPECT_EQ(0u, CountUkmEntries());

  histogram_tester()->ExpectTotalCount(
      SecurityStatePageLoadMetricsObserver::
          GetSecurityLevelPageEndReasonHistogramNameForTesting(
              security_state::NONE),
      0);
}

class SecurityStatePageLoadMetricsBrowserTestWithAutoupgradesDisabled
    : public SecurityStatePageLoadMetricsBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    SecurityStatePageLoadMetricsBrowserTest::SetUpCommandLine(command_line);
    feature_list.InitAndDisableFeature(
        blink::features::kMixedContentAutoupgrade);
  }

 private:
  base::test::ScopedFeatureList feature_list;
};

IN_PROC_BROWSER_TEST_F(
    SecurityStatePageLoadMetricsBrowserTestWithAutoupgradesDisabled,
    MixedContent) {
  StartHttpsServer(net::EmbeddedTestServer::CERT_OK);
  GURL url = https_test_server()->GetURL("/simple.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  SecurityStyleTestObserver observer(tab);
  ASSERT_TRUE(
      content::ExecJs(browser()->tab_strip_model()->GetActiveWebContents(),
                      "var img = document.createElement('img'); "
                      "img.src = 'http://example.com/image.png'; "
                      "document.body.appendChild(img);"));
  observer.WaitForDidChangeVisibleSecurityState();
  CloseAllTabs();

  security_state::SecurityLevel mixed_content_security_level =
      security_state::WARNING;

  histogram_tester()->ExpectTotalCount(
      SecurityStatePageLoadMetricsObserver::
          GetEngagementFinalHistogramNameForTesting(
              mixed_content_security_level),
      1);
  histogram_tester()->ExpectTotalCount(
      SecurityStatePageLoadMetricsObserver::
          GetEngagementFinalHistogramNameForTesting(security_state::SECURE),
      0);
  EXPECT_EQ(1u, CountUkmEntries());
  ExpectMetricForUrl(url, UkmEntry::kInitialSecurityLevelName,
                     security_state::SECURE);
  ExpectMetricForUrl(url, UkmEntry::kFinalSecurityLevelName,
                     mixed_content_security_level);
}

IN_PROC_BROWSER_TEST_F(SecurityStatePageLoadMetricsBrowserTest,
                       UncommittedLoadWithError) {
  // Make HTTPS server cause an interstitial.
  StartHttpsServer(net::EmbeddedTestServer::CERT_EXPIRED);
  GURL url = https_test_server()->GetURL("/simple.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  CloseAllTabs();

  histogram_tester()->ExpectTotalCount(
      SecurityStatePageLoadMetricsObserver::
          GetEngagementFinalHistogramNameForTesting(security_state::NONE),
      0);
  histogram_tester()->ExpectTotalCount(
      SecurityStatePageLoadMetricsObserver::
          GetEngagementFinalHistogramNameForTesting(security_state::SECURE),
      0);
  EXPECT_EQ(0u, CountUkmEntries());
}

IN_PROC_BROWSER_TEST_F(SecurityStatePageLoadMetricsBrowserTest,
                       HostDoesNotExist) {
  GURL url("http://nonexistent.test/page.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  histogram_tester()->ExpectTotalCount(
      SecurityStatePageLoadMetricsObserver::
          GetEngagementFinalHistogramNameForTesting(security_state::NONE),
      0);
  histogram_tester()->ExpectTotalCount(
      SecurityStatePageLoadMetricsObserver::
          GetEngagementFinalHistogramNameForTesting(security_state::SECURE),
      0);
  EXPECT_EQ(0u, CountUkmEntries());

  histogram_tester()->ExpectTotalCount(
      SecurityStatePageLoadMetricsObserver::
          GetSecurityLevelPageEndReasonHistogramNameForTesting(
              security_state::SECURE),
      0);
}

// Tests that the Safety Tip page end reason histogram is recorded properly on a
// net error page. No Safety Tip should appear on net errors.
IN_PROC_BROWSER_TEST_F(SecurityStatePageLoadMetricsBrowserTest,
                       SafetyTipHostDoesNotExist) {
  GURL url("http://nonexistent-google.com/page.html");

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  SafetyTipWebContentsObserver* safety_tip_observer =
      SafetyTipWebContentsObserver::FromWebContents(contents);
  ASSERT_TRUE(safety_tip_observer);

  // Navigate to |url| and wait for the safety tip check to complete before
  // checking the histograms.
  base::RunLoop run_loop;
  safety_tip_observer->RegisterSafetyTipCheckCallbackForTesting(
      run_loop.QuitClosure());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  run_loop.Run();

  histogram_tester()->ExpectTotalCount(
      SecurityStatePageLoadMetricsObserver::
          GetSafetyTipPageEndReasonHistogramNameForTesting(
              security_state::SafetyTipStatus::kNone),
      0);
}

IN_PROC_BROWSER_TEST_F(SecurityStatePageLoadMetricsBrowserTest,
                       Navigate_Both_NonHtmlMainResource) {
  StartHttpServer();
  StartHttpsServer(net::EmbeddedTestServer::CERT_OK);

  GURL http_url = http_test_server()->GetURL("/circle.svg");
  GURL https_url = https_test_server()->GetURL("/circle.svg");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), http_url));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), https_url));
  CloseAllTabs();

  histogram_tester()->ExpectTotalCount(
      SecurityStatePageLoadMetricsObserver::
          GetEngagementFinalHistogramNameForTesting(security_state::NONE),
      0);
  histogram_tester()->ExpectTotalCount(
      SecurityStatePageLoadMetricsObserver::
          GetEngagementFinalHistogramNameForTesting(security_state::SECURE),
      0);
  EXPECT_EQ(0u, CountUkmEntries());

  histogram_tester()->ExpectTotalCount(
      SecurityStatePageLoadMetricsObserver::
          GetSecurityLevelPageEndReasonHistogramNameForTesting(
              security_state::SECURE),
      0);
}

// Regression test for crbug.com/942326, where foreground duration was not being
// updated unless the tab was hidden.
IN_PROC_BROWSER_TEST_F(SecurityStatePageLoadMetricsBrowserTest,
                       NonZeroForegroundTime) {
  StartHttpServer();
  StartHttpsServer(net::EmbeddedTestServer::CERT_OK);

  GURL https_url = https_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), https_url));

  const base::TimeDelta kMinForegroundTime = base::Milliseconds(10);

  // Ensure that the tab is open for more than 0 ms, even in the face of bots
  // with bad clocks.
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), kMinForegroundTime);
  run_loop.Run();
  CloseAllTabs();

  auto security_level_samples =
      histogram_tester()->GetAllSamples("Security.TimeOnPage2.SECURE");
  EXPECT_EQ(1u, security_level_samples.size());
  EXPECT_LE(kMinForegroundTime.InMilliseconds(),
            security_level_samples.front().min);
  auto safety_tip_samples =
      histogram_tester()->GetAllSamples("Security.TimeOnPage2.SafetyTip_None");
  EXPECT_EQ(1u, safety_tip_samples.size());
  EXPECT_LE(kMinForegroundTime.InMilliseconds(),
            safety_tip_samples.front().min);
}

class SecurityStatePageLoadMetricsMPArchBrowserTest
    : public SecurityStatePageLoadMetricsBrowserTest {
 public:
  SecurityStatePageLoadMetricsMPArchBrowserTest() = default;
  ~SecurityStatePageLoadMetricsMPArchBrowserTest() override = default;

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }
};

class SecurityStatePageLoadMetricsPrerenderBrowserTest
    : public SecurityStatePageLoadMetricsMPArchBrowserTest {
 public:
  SecurityStatePageLoadMetricsPrerenderBrowserTest()
      : prerender_helper_(base::BindRepeating(
            &SecurityStatePageLoadMetricsPrerenderBrowserTest::GetWebContents,
            base::Unretained(this))) {}
  ~SecurityStatePageLoadMetricsPrerenderBrowserTest() override = default;

  void SetUp() override {
    prerender_helper_.RegisterServerRequestMonitor(embedded_test_server());
    SecurityStatePageLoadMetricsBrowserTest::SetUp();
  }

  content::test::PrerenderTestHelper* prerender_helper() {
    return &prerender_helper_;
  }

 private:
  content::test::PrerenderTestHelper prerender_helper_;
};

IN_PROC_BROWSER_TEST_F(
    SecurityStatePageLoadMetricsPrerenderBrowserTest,
    EnsurePrerenderingDoesNotRecordOnCommitSecurityLevelHistogram) {
  StartHttpsServer(net::EmbeddedTestServer::CERT_OK);

  GURL https_url = https_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), https_url));

  histogram_tester()->ExpectTotalCount("Security.SecurityLevel.OnCommit", 1);

  // Loads a page in the prerender.
  GURL prerender_url = https_test_server()->GetURL("/title2.html");
  const content::FrameTreeNodeId host_id =
      prerender_helper()->AddPrerender(prerender_url);
  content::test::PrerenderHostObserver host_observer(*GetWebContents(),
                                                     host_id);
  EXPECT_FALSE(host_observer.was_activated());

  histogram_tester()->ExpectTotalCount("Security.SecurityLevel.OnCommit", 1);

  // Activate the page from the prerendering.
  prerender_helper()->NavigatePrimaryPage(prerender_url);
  EXPECT_TRUE(host_observer.was_activated());

  // Prerendering records it on DidActivatePrerenderedPage.
  histogram_tester()->ExpectTotalCount("Security.SecurityLevel.OnCommit", 2);
}

class SecurityStatePageLoadMetricsFencedFrameBrowserTest
    : public SecurityStatePageLoadMetricsMPArchBrowserTest {
 public:
  SecurityStatePageLoadMetricsFencedFrameBrowserTest() = default;
  ~SecurityStatePageLoadMetricsFencedFrameBrowserTest() override = default;

  content::test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return fenced_frame_helper_;
  }

 private:
  content::test::FencedFrameTestHelper fenced_frame_helper_;
};

IN_PROC_BROWSER_TEST_F(SecurityStatePageLoadMetricsFencedFrameBrowserTest,
                       DoNotRecordOnCommitSecurityLevelHistogram) {
  StartHttpsServer(net::EmbeddedTestServer::CERT_OK);

  GURL https_url = https_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), https_url));

  histogram_tester()->ExpectTotalCount("Security.SecurityLevel.OnCommit", 1);

  // Create a fenced frame.
  GURL fenced_frame_url(
      https_test_server()->GetURL("/fenced_frames/title1.html"));
  content::RenderFrameHost* fenced_frame_host =
      fenced_frame_test_helper().CreateFencedFrame(
          GetWebContents()->GetPrimaryMainFrame(), fenced_frame_url);
  ASSERT_TRUE(fenced_frame_host);

  histogram_tester()->ExpectTotalCount("Security.SecurityLevel.OnCommit", 1);
}
