// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <tuple>

#include "base/run_loop.h"
#include "base/sequence_checker.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/navigation_predictor/navigation_predictor.h"
#include "chrome/browser/navigation_predictor/navigation_predictor_keyed_service.h"
#include "chrome/browser/navigation_predictor/navigation_predictor_keyed_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/subresource_filter/subresource_filter_browser_test_harness.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/search_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_manager.h"
#include "components/search_engines/template_url_service.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/base/features.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/embedded_test_server_connection_listener.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace {

class NavigationPredictorBrowserTest
    : public subresource_filter::SubresourceFilterBrowserTest {
 public:
  NavigationPredictorBrowserTest() {
    // Report all anchors to avoid non-deterministic behavior.
    std::map<std::string, std::string> params;
    params["random_anchor_sampling_period"] = "1";
    params["traffic_client_enabled_percent"] = "100";

    feature_list_.InitAndEnableFeatureWithParameters(
        blink::features::kNavigationPredictor, params);

    NavigationPredictor::DisableRendererMetricSendingDelayForTesting();
  }

  NavigationPredictorBrowserTest(const NavigationPredictorBrowserTest&) =
      delete;
  NavigationPredictorBrowserTest& operator=(
      const NavigationPredictorBrowserTest&) = delete;

  void SetUp() override {
    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_server_->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_->ServeFilesFromSourceDirectory(
        "chrome/test/data/navigation_predictor");
    ASSERT_TRUE(https_server_->Start());

    http_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTP);
    http_server_->ServeFilesFromSourceDirectory(
        "chrome/test/data/navigation_predictor");
    ASSERT_TRUE(http_server_->Start());

    subresource_filter::SubresourceFilterBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    subresource_filter::SubresourceFilterBrowserTest::SetUpOnMainThread();
    host_resolver()->ClearRules();
    host_resolver()->AddRule("a.test", "127.0.0.1");
    host_resolver()->AddRule("b.test", "127.0.0.1");
    ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }

  const GURL GetTestURL(const char* file) const {
    return https_server_->GetURL("a.test", file);
  }

  const GURL GetTestURL(const char* hostname, const char* file) const {
    return https_server_->GetURL(hostname, file);
  }

  const GURL GetHttpTestURL(const char* hostname, const char* file) const {
    return http_server_->GetURL(hostname, file);
  }

  // Wait until at least |num_links| are reported as having entered the viewport
  // in UKM.
  void WaitLinkEnteredViewport(size_t num_links,
                               bool requires_polling = false) {
    EnsureLayout();

    const char* entry_name =
        ukm::builders::NavigationPredictorAnchorElementMetrics::kEntryName;

    while (ukm_recorder_->GetEntriesByName(entry_name).size() < num_links) {
      if (requires_polling) {
        // We need to poll for the condition to become true instead of using
        // `TestUkmRecorder::SetOnAddEntryCallback` if multiple lifecycle
        // updates are needed to get the next report.
        EnsureLayout();
      } else {
        base::RunLoop run_loop;
        ukm_recorder_->SetOnAddEntryCallback(entry_name,
                                             run_loop.QuitClosure());
        EnsureLayout();
        run_loop.Run();
      }
    }
  }

  void ResetUKM() {
    ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }

 private:
  void EnsureLayout() {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    content::RenderFrameHost* primary_rfh = web_contents->GetPrimaryMainFrame();
    if (primary_rfh->IsRenderFrameLive()) {
      EXPECT_EQ(true, EvalJsAfterLifecycleUpdate(primary_rfh, "", "true"));
      EXPECT_EQ(true, EvalJsAfterLifecycleUpdate(primary_rfh, "", "true"));
    }
  }

  std::unique_ptr<net::EmbeddedTestServer> http_server_;
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder_;
  base::test::ScopedFeatureList feature_list_;
};

class TestObserver : public NavigationPredictorKeyedService::Observer {
 public:
  TestObserver() = default;

  TestObserver(const TestObserver&) = delete;
  TestObserver& operator=(const TestObserver&) = delete;

  ~TestObserver() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

  std::optional<NavigationPredictorKeyedService::Prediction> last_prediction()
      const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return last_prediction_;
  }

  size_t count_predictions() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return count_predictions_;
  }

  // Waits until the count if received notifications is at least
  // |expected_notifications_count|.
  void WaitUntilNotificationsCountReached(size_t expected_notifications_count) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    // Ensure that |wait_loop_| is null implying there is no ongoing wait.
    ASSERT_FALSE(wait_loop_);

    while (count_predictions_ < expected_notifications_count) {
      expected_notifications_count_ = expected_notifications_count;
      wait_loop_ = std::make_unique<base::RunLoop>();
      wait_loop_->Run();
      wait_loop_.reset();
    }
  }

 private:
  void OnPredictionUpdated(
      const NavigationPredictorKeyedService::Prediction& prediction) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    ++count_predictions_;
    last_prediction_ = prediction;
    if (wait_loop_ && count_predictions_ >= expected_notifications_count_) {
      wait_loop_->Quit();
    }
  }

  // Count of prediction notifications received so far.
  size_t count_predictions_ = 0u;

  // last prediction received.
  std::optional<NavigationPredictorKeyedService::Prediction> last_prediction_;

  // If |wait_loop_| is non-null, then it quits as soon as count of received
  // notifications are at least |expected_notifications_count_|.
  std::unique_ptr<base::RunLoop> wait_loop_;
  std::optional<size_t> expected_notifications_count_;

  SEQUENCE_CHECKER(sequence_checker_);
};

IN_PROC_BROWSER_TEST_F(NavigationPredictorBrowserTest, Pipeline) {
  auto test_ukm_recorder = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  ResetUKM();

  const GURL& url = GetTestURL("/simple_page_with_anchors.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  WaitLinkEnteredViewport(1);

  // Force recording NavigationPredictorPageLinkMetrics UKM.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  using PageLinkEntry = ukm::builders::NavigationPredictorPageLinkMetrics;
  auto entries = test_ukm_recorder->GetEntriesByName(PageLinkEntry::kEntryName);
  EXPECT_EQ(1u, entries.size());
  auto* entry = entries[0].get();
  auto get_metric = [&](auto name) {
    return *test_ukm_recorder->GetEntryMetric(entry, name);
  };
  EXPECT_EQ(5, get_metric(PageLinkEntry::kNumberOfAnchors_TotalName));
  EXPECT_EQ(0, get_metric(PageLinkEntry::kNumberOfAnchors_ContainsImageName));
  EXPECT_EQ(0, get_metric(PageLinkEntry::kNumberOfAnchors_InIframeName));
  EXPECT_EQ(3, get_metric(PageLinkEntry::kNumberOfAnchors_SameHostName));
  EXPECT_EQ(0, get_metric(PageLinkEntry::kNumberOfAnchors_URLIncrementedName));

  // Same document anchor element should be removed.
  using AnchorEntry = ukm::builders::NavigationPredictorAnchorElementMetrics;
  entries = test_ukm_recorder->GetEntriesByName(AnchorEntry::kEntryName);
  EXPECT_EQ(2u, entries.size());
}

// Test that no metrics are recorded in off-the-record profiles.
IN_PROC_BROWSER_TEST_F(NavigationPredictorBrowserTest, PipelineOffTheRecord) {
  auto test_ukm_recorder = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  ResetUKM();

  const GURL& url = GetTestURL("/simple_page_with_anchors.html");
  Browser* incognito = CreateIncognitoBrowser();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(incognito, url));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(
      content::ExecJs(incognito->tab_strip_model()->GetActiveWebContents(),
                      "document.getElementById('google').click();"));
  base::RunLoop().RunUntilIdle();

  auto entries = test_ukm_recorder->GetMergedEntriesByName(
      ukm::builders::NavigationPredictorAnchorElementMetrics::kEntryName);
  EXPECT_EQ(0u, entries.size());
  entries = test_ukm_recorder->GetMergedEntriesByName(
      ukm::builders::NavigationPredictorPageLinkMetrics::kEntryName);
  EXPECT_EQ(0u, entries.size());
  entries = test_ukm_recorder->GetMergedEntriesByName(
      ukm::builders::NavigationPredictorPageLinkClick::kEntryName);
  EXPECT_EQ(0u, entries.size());
}

// Test that the browser does not process anchor element metrics from an http
// web page on page load.
IN_PROC_BROWSER_TEST_F(NavigationPredictorBrowserTest, PipelineHttp) {
  auto test_ukm_recorder = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  ResetUKM();

  // We don't use localhost for this test, as http localhost is trusted.
  const GURL& url = GetHttpTestURL("a.test", "/simple_page_with_anchors.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  base::RunLoop().RunUntilIdle();

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver click_nav_observer(web_contents);
  EXPECT_TRUE(content::ExecJs(web_contents,
                              "document.getElementById('google').click();"));
  click_nav_observer.Wait();
  base::RunLoop().RunUntilIdle();

  auto entries = test_ukm_recorder->GetMergedEntriesByName(
      ukm::builders::NavigationPredictorAnchorElementMetrics::kEntryName);
  EXPECT_EQ(0u, entries.size());
  entries = test_ukm_recorder->GetMergedEntriesByName(
      ukm::builders::NavigationPredictorPageLinkMetrics::kEntryName);
  EXPECT_EQ(0u, entries.size());
  entries = test_ukm_recorder->GetMergedEntriesByName(
      ukm::builders::NavigationPredictorPageLinkClick::kEntryName);
  EXPECT_EQ(0u, entries.size());
}

// Make sure AnchorsData gets cleared between navigations.
IN_PROC_BROWSER_TEST_F(NavigationPredictorBrowserTest, MultipleNavigations) {
  auto test_ukm_recorder = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  ResetUKM();

  const GURL& url = GetTestURL("/simple_page_with_anchors.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  WaitLinkEnteredViewport(1);
  using AnchorEntry = ukm::builders::NavigationPredictorAnchorElementMetrics;
  size_t num_links_in_viewport =
      test_ukm_recorder->GetEntriesByName(AnchorEntry::kEntryName).size();

  // Load the same URL again. The UKM record from the previous load should get
  // flushed.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  // Wait until layout has happened: at least one new link entered viewport
  // since the last page load.
  WaitLinkEnteredViewport(num_links_in_viewport + 1);

  using PageLinkEntry = ukm::builders::NavigationPredictorPageLinkMetrics;
  auto entries = test_ukm_recorder->GetEntriesByName(PageLinkEntry::kEntryName);
  EXPECT_EQ(1u, entries.size());
  auto* entry = entries[0].get();
  auto get_metric = [&](auto name) {
    return *test_ukm_recorder->GetEntryMetric(entry, name);
  };
  EXPECT_EQ(5, get_metric(PageLinkEntry::kNumberOfAnchors_TotalName));

  // Force recording NavigationPredictorPageLinkMetrics UKM.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  // If we correctly reset AnchorsData, the number of anchors should still be 5
  // (and not 10).
  entries = test_ukm_recorder->GetEntriesByName(PageLinkEntry::kEntryName);
  EXPECT_EQ(2u, entries.size());
  entry = entries[1];
  EXPECT_EQ(5, get_metric(PageLinkEntry::kNumberOfAnchors_TotalName));
}

// Tests that anchors from iframes are reported.
IN_PROC_BROWSER_TEST_F(NavigationPredictorBrowserTest, PageWithIframe) {
  auto test_ukm_recorder = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  ResetUKM();

  const GURL& url = GetTestURL("/page_with_anchors_and_iframe.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  // Wait until all links have entered the viewport. In particular this forces
  // the iframe to load.
  WaitLinkEnteredViewport(7);

  // Force recording NavigationPredictorPageLinkMetrics UKM.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  using PageLinkEntry = ukm::builders::NavigationPredictorPageLinkMetrics;
  auto entries = test_ukm_recorder->GetEntriesByName(PageLinkEntry::kEntryName);
  EXPECT_EQ(1u, entries.size());
  auto* entry = entries[0].get();
  auto get_metric = [&](auto name) {
    return *test_ukm_recorder->GetEntryMetric(entry, name);
  };
  EXPECT_EQ(7, get_metric(PageLinkEntry::kNumberOfAnchors_TotalName));
  EXPECT_EQ(1, get_metric(PageLinkEntry::kNumberOfAnchors_ContainsImageName));
  EXPECT_EQ(3, get_metric(PageLinkEntry::kNumberOfAnchors_InIframeName));
  EXPECT_EQ(3, get_metric(PageLinkEntry::kNumberOfAnchors_SameHostName));
  EXPECT_EQ(0, get_metric(PageLinkEntry::kNumberOfAnchors_URLIncrementedName));

  // Anchors in same-origin iframes should be reported as entering the viewport.
  using AnchorEntry = ukm::builders::NavigationPredictorAnchorElementMetrics;
  entries = test_ukm_recorder->GetEntriesByName(AnchorEntry::kEntryName);
  EXPECT_EQ(7u, entries.size());
}

// Tests parameterized on whether site isolation is enabled, to ensure that the
// metrics calculations in the renderer don't change based on the process model.
class NavigationPredictorSiteIsolationBrowserTest
    : public NavigationPredictorBrowserTest,
      public ::testing::WithParamInterface<bool> {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    NavigationPredictorBrowserTest::SetUpCommandLine(command_line);
    if (SiteIsolationEnabled()) {
      content::IsolateAllSitesForTesting(command_line);
    } else {
      command_line->RemoveSwitch(switches::kSitePerProcess);
      command_line->AppendSwitch(switches::kDisableSiteIsolation);
    }
  }

  bool SiteIsolationEnabled() const { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(All,
                         NavigationPredictorSiteIsolationBrowserTest,
                         testing::Bool());

// Tests cross-origin iframe. For now we don't log cross-origin links, so this
// test just makes sure the iframe is ignored and the browser doesn't crash.
IN_PROC_BROWSER_TEST_P(NavigationPredictorSiteIsolationBrowserTest,
                       PageWithCrossOriginIframe) {
  auto test_ukm_recorder = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  ResetUKM();

  const GURL& url =
      GetTestURL("/page_with_anchors_and_cross_origin_iframe.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  const GURL& iframe_url =
      GetTestURL("b.test", "/iframe_simple_page_with_anchors.html");
  EXPECT_TRUE(content::NavigateIframeToURL(
      browser()->tab_strip_model()->GetActiveWebContents(), "crossFrame",
      iframe_url));
  WaitLinkEnteredViewport(1);

  // Force recording NavigationPredictorPageLinkMetrics UKM.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  using PageLinkEntry = ukm::builders::NavigationPredictorPageLinkMetrics;
  auto entries = test_ukm_recorder->GetEntriesByName(PageLinkEntry::kEntryName);
  EXPECT_EQ(1u, entries.size());
  auto* entry = entries[0].get();
  auto get_metric = [&](auto name) {
    return *test_ukm_recorder->GetEntryMetric(entry, name);
  };
  EXPECT_EQ(4, get_metric(PageLinkEntry::kNumberOfAnchors_TotalName));
  EXPECT_EQ(0, get_metric(PageLinkEntry::kNumberOfAnchors_ContainsImageName));
  EXPECT_EQ(0, get_metric(PageLinkEntry::kNumberOfAnchors_InIframeName));
  EXPECT_EQ(3, get_metric(PageLinkEntry::kNumberOfAnchors_SameHostName));
  EXPECT_EQ(0, get_metric(PageLinkEntry::kNumberOfAnchors_URLIncrementedName));

  // Same anchors in iframes should be reported as entering the viewport.
  using AnchorEntry = ukm::builders::NavigationPredictorAnchorElementMetrics;
  entries = test_ukm_recorder->GetEntriesByName(AnchorEntry::kEntryName);
  EXPECT_EQ(4u, entries.size());
}

// Tests a frame hierarchy of A(B(A)). The cross-origin iframe B should be
// ignored, but the same-origin iframe A should be included even though its
// parent is cross-origin.
IN_PROC_BROWSER_TEST_P(NavigationPredictorSiteIsolationBrowserTest,
                       PageWithSameOriginIframeInCrossOriginIframe) {
  // TODO(crbug.com/41492823): Flaky timeouts on mac, linux rel, and cros rel.
#if BUILDFLAG(IS_MAC) || \
    ((BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)) && defined(NDEBUG))
  if (SiteIsolationEnabled()) {
    GTEST_SKIP() << "Flaky. https://crbug.com/41492823";
  }
#endif

  auto test_ukm_recorder = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  ResetUKM();

  const GURL url =
      GetTestURL("a.test", "/page_with_anchor_and_cross_origin_iframe_b.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // The links in the same-origin iframe won't be reported until the next
  // lifecycle update of the main frame, which WaitLinkEnteredViewport triggers.
  // Given that this could race with the processing of the links in the iframe
  // document, we may need to trigger updates multiple times.
  const bool requires_polling = true;

  WaitLinkEnteredViewport(4, requires_polling);

  // Force recording NavigationPredictorPageLinkMetrics UKM.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  using PageLinkEntry = ukm::builders::NavigationPredictorPageLinkMetrics;
  auto entries = test_ukm_recorder->GetEntriesByName(PageLinkEntry::kEntryName);
  EXPECT_EQ(1u, entries.size());
  auto* entry = entries[0].get();
  auto get_metric = [&](auto name) {
    return *test_ukm_recorder->GetEntryMetric(entry, name);
  };
  EXPECT_EQ(4, get_metric(PageLinkEntry::kNumberOfAnchors_TotalName));
  EXPECT_EQ(1, get_metric(PageLinkEntry::kNumberOfAnchors_ContainsImageName));
  EXPECT_EQ(3, get_metric(PageLinkEntry::kNumberOfAnchors_InIframeName));
  EXPECT_EQ(1, get_metric(PageLinkEntry::kNumberOfAnchors_SameHostName));
  EXPECT_EQ(0, get_metric(PageLinkEntry::kNumberOfAnchors_URLIncrementedName));

  using AnchorEntry = ukm::builders::NavigationPredictorAnchorElementMetrics;
  entries = test_ukm_recorder->GetEntriesByName(AnchorEntry::kEntryName);
  EXPECT_EQ(4u, entries.size());
}

// Inject link into the viewport after some time test reporting of
// NavigationStartToEnteredViewportMs.
IN_PROC_BROWSER_TEST_F(NavigationPredictorBrowserTest,
                       NavigationStartToEnteredViewportMs) {
  auto test_ukm_recorder = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  ResetUKM();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GetTestURL("/dynamically_inserted_anchor.html")));
  WaitLinkEnteredViewport(1);

  using AnchorEntry = ukm::builders::NavigationPredictorAnchorElementMetrics;
  auto entries = test_ukm_recorder->GetEntriesByName(AnchorEntry::kEntryName);
  EXPECT_EQ(1u, entries.size());
  uint64_t time_ms = *test_ukm_recorder->GetEntryMetric(
      entries[0], AnchorEntry::kNavigationStartToLinkLoggedMsName);
  EXPECT_LT(0u, time_ms);
  // To avoid making the test flaky we allow this value to be up to 100s.
  // This still tests for cases where the unsigned integer overflows or if we
  // fail to subtract navigation start from the timestamp when the link entered
  // the viewport.
  EXPECT_GT(100000u, time_ms);
}

// Simulate a click at the anchor element.
IN_PROC_BROWSER_TEST_F(NavigationPredictorBrowserTest, ClickAnchorElement) {
  auto test_ukm_recorder = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  ResetUKM();

  const GURL& url = GetTestURL("/simple_page_with_anchors.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  WaitLinkEnteredViewport(1);

  EXPECT_TRUE(
      content::ExecJs(browser()->tab_strip_model()->GetActiveWebContents(),
                      "document.getElementById('google').click();"));
  base::RunLoop().RunUntilIdle();

  // Navigate to another page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetTestURL("/1.html")));
  base::RunLoop().RunUntilIdle();

  // Make sure the click has been logged.
  auto entries = test_ukm_recorder->GetMergedEntriesByName(
      ukm::builders::NavigationPredictorPageLinkClick::kEntryName);
  EXPECT_EQ(1u, entries.size());
}

class NavigationPredictorBrowserTestWithDefaultPredictorEnabled
    : public NavigationPredictorBrowserTest {
 public:
  NavigationPredictorBrowserTestWithDefaultPredictorEnabled() {
    feature_list_.InitAndEnableFeatureWithParameters(
        blink::features::kNavigationPredictor, {});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Simulate a click at the anchor element in off-the-record profile.
IN_PROC_BROWSER_TEST_F(NavigationPredictorBrowserTest,
                       ClickAnchorElementOffTheRecord) {
  auto test_ukm_recorder = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  ResetUKM();

  const GURL& url = GetTestURL("/simple_page_with_anchors.html");

  Browser* incognito = CreateIncognitoBrowser();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(incognito, url));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(
      content::ExecJs(incognito->tab_strip_model()->GetActiveWebContents(),
                      "document.getElementById('google').click();"));
  content::WaitForLoadStop(
      incognito->tab_strip_model()->GetActiveWebContents());

  auto entries = test_ukm_recorder->GetMergedEntriesByName(
      ukm::builders::PageLoad::kEntryName);
  EXPECT_EQ(1u, entries.size());

  // Make sure no click has been logged.
  entries = test_ukm_recorder->GetMergedEntriesByName(
      ukm::builders::NavigationPredictorPageLinkClick::kEntryName);
  EXPECT_EQ(0u, entries.size());
}

// Tests that the browser counts anchors from anywhere on the page.
IN_PROC_BROWSER_TEST_F(NavigationPredictorBrowserTest,
                       ViewportOnlyAndUrlIncrementByOne) {
  auto test_ukm_recorder = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  ResetUKM();

  const GURL& url = GetTestURL("/long_page_with_anchors-1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  WaitLinkEnteredViewport(1);

  // Force recording NavigationPredictorPageLinkMetrics UKM.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  // Make sure no click has been logged.
  using UkmEntry = ukm::builders::NavigationPredictorPageLinkMetrics;
  auto entries = test_ukm_recorder->GetEntriesByName(UkmEntry::kEntryName);
  EXPECT_EQ(1u, entries.size());
  auto* entry = entries[0].get();
  auto get_metric = [&](auto name) {
    return *test_ukm_recorder->GetEntryMetric(entry, name);
  };
  EXPECT_EQ(3, get_metric(UkmEntry::kNumberOfAnchors_TotalName));
  EXPECT_EQ(0, get_metric(UkmEntry::kNumberOfAnchors_ContainsImageName));
  EXPECT_EQ(0, get_metric(UkmEntry::kNumberOfAnchors_InIframeName));
  EXPECT_EQ(1, get_metric(UkmEntry::kNumberOfAnchors_SameHostName));
  EXPECT_EQ(1, get_metric(UkmEntry::kNumberOfAnchors_URLIncrementedName));
}

// Test that anchors are dispated to the single observer, except for anchors
// linking to the same page (e.g. fragment links).
IN_PROC_BROWSER_TEST_F(NavigationPredictorBrowserTest, SingleObserver) {
  TestObserver observer;

  NavigationPredictorKeyedService* service =
      NavigationPredictorKeyedServiceFactory::GetForProfile(
          browser()->profile());
  EXPECT_NE(nullptr, service);
  service->AddObserver(&observer);

  const GURL& url = GetTestURL("/simple_page_with_anchors.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  WaitLinkEnteredViewport(1);
  observer.WaitUntilNotificationsCountReached(1);

  service->RemoveObserver(&observer);

  EXPECT_EQ(1u, observer.count_predictions());
  EXPECT_EQ(url, observer.last_prediction()->source_document_url());
  EXPECT_THAT(observer.last_prediction()->sorted_predicted_urls(),
              ::testing::UnorderedElementsAre("https://google.com/",
                                              "https://example.com/"));

  // Doing another navigation after removing the observer should not cause a
  // crash.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  WaitLinkEnteredViewport(1);
  EXPECT_EQ(1u, observer.count_predictions());
}

// Test that anchors are dispatched to the single observer, including
// anchors outside the viewport. Reactive prefetch relies on anchors from
// outside the viewport to be included since hints are only requested at onload
// predictions after that point are ignored.
IN_PROC_BROWSER_TEST_F(NavigationPredictorBrowserTest,
                       SingleObserverPastViewport) {
  TestObserver observer;

  NavigationPredictorKeyedService* service =
      NavigationPredictorKeyedServiceFactory::GetForProfile(
          browser()->profile());
  EXPECT_NE(nullptr, service);
  service->AddObserver(&observer);

  const GURL& url = GetTestURL("/long_page_with_anchors-1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  WaitLinkEnteredViewport(1);
  observer.WaitUntilNotificationsCountReached(1);

  service->RemoveObserver(&observer);

  EXPECT_EQ(1u, observer.count_predictions());
  EXPECT_EQ(url, observer.last_prediction()->source_document_url());
  EXPECT_THAT(observer.last_prediction()->sorted_predicted_urls(),
              ::testing::UnorderedElementsAre(
                  "https://google.com/", "https://example2.com/",
                  GetTestURL("/long_page_with_anchors-2.html")));

  // Doing another navigation after removing the observer should not cause a
  // crash.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  WaitLinkEnteredViewport(1);
  EXPECT_EQ(1u, observer.count_predictions());
}

// Same as NavigationScoreSingleObserver test but with more than one observer.
IN_PROC_BROWSER_TEST_F(NavigationPredictorBrowserTest, TwoObservers) {
  TestObserver observer_1;
  TestObserver observer_2;

  NavigationPredictorKeyedService* service =
      NavigationPredictorKeyedServiceFactory::GetForProfile(
          browser()->profile());
  service->AddObserver(&observer_1);
  service->AddObserver(&observer_2);

  const GURL& url = GetTestURL("/simple_page_with_anchors.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  WaitLinkEnteredViewport(1);
  observer_1.WaitUntilNotificationsCountReached(1);
  observer_2.WaitUntilNotificationsCountReached(1);

  service->RemoveObserver(&observer_1);

  EXPECT_EQ(1u, observer_1.count_predictions());
  EXPECT_EQ(url, observer_1.last_prediction()->source_document_url());
  EXPECT_EQ(2u, observer_1.last_prediction()->sorted_predicted_urls().size());
  EXPECT_THAT(observer_1.last_prediction()->sorted_predicted_urls(),
              ::testing::UnorderedElementsAre("https://google.com/",
                                              "https://example.com/"));
  EXPECT_EQ(1u, observer_2.count_predictions());
  EXPECT_EQ(url, observer_2.last_prediction()->source_document_url());

  // Only |observer_2| should get the notification since |observer_1| has
  // been removed from receiving the notifications.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  WaitLinkEnteredViewport(1);
  observer_2.WaitUntilNotificationsCountReached(2);
  EXPECT_EQ(1u, observer_1.count_predictions());
  EXPECT_EQ(2u, observer_2.count_predictions());
  EXPECT_THAT(observer_2.last_prediction()->sorted_predicted_urls(),
              ::testing::UnorderedElementsAre("https://google.com/",
                                              "https://example.com/"));
}

// Test that the navigation predictor keyed service is null for incognito
// profiles.
IN_PROC_BROWSER_TEST_F(NavigationPredictorBrowserTest, Incognito) {
  Browser* incognito = CreateIncognitoBrowser();
  NavigationPredictorKeyedService* incognito_service =
      NavigationPredictorKeyedServiceFactory::GetForProfile(
          incognito->profile());
  EXPECT_EQ(nullptr, incognito_service);
}

class NavigationPredictorMPArchBrowserTest
    : public NavigationPredictorBrowserTest {
 public:
  NavigationPredictorMPArchBrowserTest() = default;
  ~NavigationPredictorMPArchBrowserTest() override = default;
  NavigationPredictorMPArchBrowserTest(
      const NavigationPredictorMPArchBrowserTest&) = delete;

  NavigationPredictorMPArchBrowserTest& operator=(
      const NavigationPredictorMPArchBrowserTest&) = delete;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    test_server_.AddDefaultHandlers(GetChromeTestDataDir());
    test_server_.ServeFilesFromSourceDirectory(
        "chrome/test/data/navigation_predictor");
    test_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
    ASSERT_TRUE(test_server_.Start());
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  net::EmbeddedTestServer* test_server() { return &test_server_; }

 private:
  net::EmbeddedTestServer test_server_{net::EmbeddedTestServer::TYPE_HTTPS};
};

class NavigationPredictorPrerenderBrowserTest
    : public NavigationPredictorMPArchBrowserTest {
 public:
  NavigationPredictorPrerenderBrowserTest()
      : prerender_test_helper_(base::BindRepeating(
            &NavigationPredictorPrerenderBrowserTest::GetWebContents,
            base::Unretained(this))) {}
  ~NavigationPredictorPrerenderBrowserTest() override = default;
  NavigationPredictorPrerenderBrowserTest(
      const NavigationPredictorPrerenderBrowserTest&) = delete;

  NavigationPredictorPrerenderBrowserTest& operator=(
      const NavigationPredictorPrerenderBrowserTest&) = delete;

  void SetUp() override {
    prerender_test_helper_.RegisterServerRequestMonitor(test_server());
    NavigationPredictorMPArchBrowserTest::SetUp();
  }

  content::test::PrerenderTestHelper& prerender_test_helper() {
    return prerender_test_helper_;
  }

 private:
  content::test::PrerenderTestHelper prerender_test_helper_;
};

// Test that prerendering doesn't create a predictor object and doesn't affect
// the primary page's behavior.
IN_PROC_BROWSER_TEST_F(NavigationPredictorPrerenderBrowserTest,
                       PrerenderingDontCreatePredictor) {
  auto test_ukm_recorder = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  ResetUKM();

  // Navigate to an initial page.
  const GURL& url = test_server()->GetURL("/simple_page_with_anchors.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  WaitLinkEnteredViewport(1);

  using AnchorEntry = ukm::builders::NavigationPredictorAnchorElementMetrics;
  auto anchor_entries =
      test_ukm_recorder->GetEntriesByName(AnchorEntry::kEntryName);
  EXPECT_EQ(2u, anchor_entries.size());

  // Start prerendering. This shouldn't create a NavigationPredictor instance.
  // If it happens, the constructor of NavigationPredictor is called for the
  // non-primary page and the DCHECK there should fail.
  content::FrameTreeNodeId host_id = prerender_test_helper().AddPrerender(url);
  content::test::PrerenderHostObserver host_observer(*GetWebContents(),
                                                     host_id);
  EXPECT_FALSE(host_observer.was_activated());

  // Make sure the prerendering doesn't log any anchors.
  anchor_entries = test_ukm_recorder->GetEntriesByName(AnchorEntry::kEntryName);
  EXPECT_EQ(2u, anchor_entries.size());

  ResetUKM();

  // Activate the prerendered frame.
  prerender_test_helper().NavigatePrimaryPage(url);
  EXPECT_TRUE(host_observer.was_activated());
  WaitLinkEnteredViewport(1);

  // Make sure the activating logs anchors correctly.
  anchor_entries = test_ukm_recorder->GetEntriesByName(AnchorEntry::kEntryName);
  EXPECT_EQ(4u, anchor_entries.size());
}

class NavigationPredictorFencedFrameBrowserTest
    : public NavigationPredictorMPArchBrowserTest {
 public:
  NavigationPredictorFencedFrameBrowserTest() = default;
  ~NavigationPredictorFencedFrameBrowserTest() override = default;
  NavigationPredictorFencedFrameBrowserTest(
      const NavigationPredictorFencedFrameBrowserTest&) = delete;

  NavigationPredictorFencedFrameBrowserTest& operator=(
      const NavigationPredictorFencedFrameBrowserTest&) = delete;

  content::test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return fenced_frame_helper_;
  }

 private:
  content::test::FencedFrameTestHelper fenced_frame_helper_;
};

IN_PROC_BROWSER_TEST_F(NavigationPredictorFencedFrameBrowserTest,
                       EnsureFencedFrameDoesNotCreateNavigationPredictor) {
  auto test_ukm_recorder = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  ResetUKM();

  // Navigate to an initial page.
  const GURL& url = test_server()->GetURL("/simple_page_with_anchors.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  WaitLinkEnteredViewport(1);

  using AnchorEntry = ukm::builders::NavigationPredictorAnchorElementMetrics;
  auto anchor_entries =
      test_ukm_recorder->GetEntriesByName(AnchorEntry::kEntryName);
  EXPECT_EQ(2u, anchor_entries.size());

  // Create a fenced frame.
  const GURL& fenced_frame_url =
      test_server()->GetURL("/fenced_frames/simple_page_with_anchors.html");
  std::ignore = fenced_frame_test_helper().CreateFencedFrame(
      web_contents()->GetPrimaryMainFrame(), fenced_frame_url);

  // Make sure the fenced frame doesn't log any anchors.
  anchor_entries = test_ukm_recorder->GetEntriesByName(AnchorEntry::kEntryName);
  EXPECT_EQ(2u, anchor_entries.size());
}

}  // namespace
