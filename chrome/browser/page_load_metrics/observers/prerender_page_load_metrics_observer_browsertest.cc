// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/contains.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/page_load_metrics/integration_tests/metric_integration_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/page_load_metrics/browser/observers/prerender_page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/prerender_test_util.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/features.h"

using PrerenderPageLoad = ukm::builders::PrerenderPageLoad;

class PrerenderPageLoadMetricsObserverBrowserTest
    : public MetricIntegrationTest {
 public:
  PrerenderPageLoadMetricsObserverBrowserTest()
      : prerender_helper_(base::BindRepeating(
            &PrerenderPageLoadMetricsObserverBrowserTest::web_contents,
            base::Unretained(this))) {
    feature_list_.InitAndEnableFeature(blink::features::kPrerender2);
  }
  ~PrerenderPageLoadMetricsObserverBrowserTest() override = default;

  void SetUpOnMainThread() override {
    prerender_helper_.SetUpOnMainThread(embedded_test_server());
    MetricIntegrationTest::SetUpOnMainThread();
  }

  int GetUkmMetricEntryCount(const std::string& entry_name,
                             const std::string& metric_name) {
    const std::vector<ukm::TestUkmRecorder::HumanReadableUkmMetrics>
        metric_entries = ukm_recorder().GetMetrics(entry_name, {metric_name});
    int count = 0;
    for (const auto& entry : metric_entries) {
      if (base::Contains(entry, metric_name))
        count++;
    }
    return count;
  }

  std::vector<int64_t> GetUkmMetricEntryValues(const std::string& entry_name,
                                               const std::string& metric_name) {
    const std::vector<ukm::TestUkmRecorder::HumanReadableUkmMetrics>
        metric_entries = ukm_recorder().GetMetrics(entry_name, {metric_name});
    std::vector<int64_t> metrics;
    for (const auto& entry : metric_entries) {
      auto it = entry.find(metric_name);
      if (it != entry.end())
        metrics.push_back(it->second);
    }
    return metrics;
  }

 protected:
  content::test::PrerenderTestHelper prerender_helper_;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(PrerenderPageLoadMetricsObserverBrowserTest,
                       ActivateInForeground) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Navigate to an initial page.
  auto initial_url = embedded_test_server()->GetURL("/empty.html");
  ui_test_utils::NavigateToURL(browser(), initial_url);

  // Start a prerender.
  GURL prerender_url = embedded_test_server()->GetURL("/title2.html");
  prerender_helper_.AddPrerender(prerender_url);

  // Activate and wait for FCP.
  auto waiter = std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
      web_contents());
  waiter->AddPageExpectation(page_load_metrics::PageLoadMetricsTestWaiter::
                                 TimingField::kFirstContentfulPaint);
  prerender_helper_.NavigatePrimaryPage(prerender_url);
  waiter->Wait();

  histogram_tester().ExpectTotalCount(
      internal::kHistogramPrerenderNavigationToActivation, 1);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramPrerenderActivationToFirstPaint, 1);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramPrerenderActivationToFirstContentfulPaint, 1);

  // Simulate mouse click and wait for FirstInputDelay.
  content::SimulateMouseClick(web_contents(), 0,
                              blink::WebPointerProperties::Button::kLeft);
  waiter->AddPageExpectation(page_load_metrics::PageLoadMetricsTestWaiter::
                                 TimingField::kFirstInputDelay);
  waiter->Wait();

  histogram_tester().ExpectTotalCount(
      internal::kHistogramPrerenderFirstInputDelay4, 1);

  // Force navigation to another page, which should force logging of histograms
  // persisted at the end of the page load lifetime.
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));

  histogram_tester().ExpectTotalCount(
      internal::kHistogramPrerenderActivationToLargestContentfulPaint2, 1);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramPrerenderCumulativeShiftScore, 1);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramPrerenderCumulativeShiftScoreMainFrame, 1);

  ASSERT_THAT(GetUkmMetricEntryValues(PrerenderPageLoad::kEntryName,
                                      PrerenderPageLoad::kWasPrerenderedName),
              testing::ElementsAre(1));
  EXPECT_EQ(GetUkmMetricEntryCount(
                PrerenderPageLoad::kEntryName,
                PrerenderPageLoad::kTiming_NavigationToActivationName),
            1);
  EXPECT_EQ(
      GetUkmMetricEntryCount(
          PrerenderPageLoad::kEntryName,
          PrerenderPageLoad::kTiming_ActivationToFirstContentfulPaintName),
      1);
  EXPECT_EQ(
      GetUkmMetricEntryCount(
          PrerenderPageLoad::kEntryName,
          PrerenderPageLoad::kTiming_ActivationToLargestContentfulPaintName),
      1);
}

IN_PROC_BROWSER_TEST_F(PrerenderPageLoadMetricsObserverBrowserTest,
                       ActivateInBackground) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Navigate to an initial page.
  auto initial_url = embedded_test_server()->GetURL("/empty.html");
  ui_test_utils::NavigateToURL(browser(), initial_url);

  // Start a prerender.
  GURL prerender_url = embedded_test_server()->GetURL("/title2.html");
  prerender_helper_.AddPrerender(prerender_url);

  // Make the tab backgrounded before the prerendered page is activated.
  web_contents()->WasHidden();

  // Activate the page, foreground the tab and wait for FCP.
  auto waiter = std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
      web_contents());
  waiter->AddPageExpectation(page_load_metrics::PageLoadMetricsTestWaiter::
                                 TimingField::kFirstContentfulPaint);
  prerender_helper_.NavigatePrimaryPage(prerender_url);
  EXPECT_TRUE(WaitForLoadStop(web_contents()));
  web_contents()->WasShown();
  waiter->Wait();

  histogram_tester().ExpectTotalCount(
      internal::kHistogramPrerenderNavigationToActivation, 1);

  // Simulate mouse click and wait for FirstInputDelay.
  content::SimulateMouseClick(web_contents(), 0,
                              blink::WebPointerProperties::Button::kLeft);
  waiter->AddPageExpectation(page_load_metrics::PageLoadMetricsTestWaiter::
                                 TimingField::kFirstInputDelay);
  waiter->Wait();

  // Force navigation to another page, which should force logging of histograms
  // persisted at the end of the page load lifetime.
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));

  // As the tab was in the background when activated, no CWV metrics are
  // recorded.
  histogram_tester().ExpectTotalCount(
      internal::kHistogramPrerenderActivationToFirstPaint, 0);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramPrerenderActivationToFirstContentfulPaint, 0);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramPrerenderActivationToLargestContentfulPaint2, 0);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramPrerenderFirstInputDelay4, 0);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramPrerenderCumulativeShiftScore, 0);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramPrerenderCumulativeShiftScoreMainFrame, 0);

  ASSERT_THAT(GetUkmMetricEntryValues(PrerenderPageLoad::kEntryName,
                                      PrerenderPageLoad::kWasPrerenderedName),
              testing::ElementsAre(1));
  EXPECT_EQ(GetUkmMetricEntryCount(
                PrerenderPageLoad::kEntryName,
                PrerenderPageLoad::kTiming_NavigationToActivationName),
            1);
  EXPECT_EQ(
      GetUkmMetricEntryCount(
          PrerenderPageLoad::kEntryName,
          PrerenderPageLoad::kTiming_ActivationToFirstContentfulPaintName),
      0);
  EXPECT_EQ(
      GetUkmMetricEntryCount(
          PrerenderPageLoad::kEntryName,
          PrerenderPageLoad::kTiming_ActivationToLargestContentfulPaintName),
      0);
}

IN_PROC_BROWSER_TEST_F(PrerenderPageLoadMetricsObserverBrowserTest,
                       CancelledPrerender) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Navigate to an initial page.
  auto initial_url = embedded_test_server()->GetURL("/empty.html");
  ui_test_utils::NavigateToURL(browser(), initial_url);

  // Start a prerender.
  GURL prerender_url = embedded_test_server()->GetURL("/title2.html");
  const int host_id = prerender_helper_.AddPrerender(prerender_url);

  // Start a navigation in the prerender frame tree that will cancel the
  // initiator's prerendering.
  content::test::PrerenderHostObserver observer(*web_contents(), host_id);

  GURL hung_url = embedded_test_server()->GetURL("/hung");
  prerender_helper_.NavigatePrerenderedPage(host_id, hung_url);

  observer.WaitForDestroyed();

  // Force navigation to another page, which should force logging of histograms
  // persisted at the end of the page load lifetime.
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));

  // As the prerender was cancelled, no prerendering metrics are recorded.
  EXPECT_EQ(0u, histogram_tester()
                    .GetTotalCountsForPrefix("PageLoad.Clients.Prerender.")
                    .size());

  EXPECT_EQ(GetUkmMetricEntryCount(PrerenderPageLoad::kEntryName,
                                   PrerenderPageLoad::kWasPrerenderedName),
            0);
  EXPECT_EQ(GetUkmMetricEntryCount(
                PrerenderPageLoad::kEntryName,
                PrerenderPageLoad::kTiming_NavigationToActivationName),
            0);
  EXPECT_EQ(
      GetUkmMetricEntryCount(
          PrerenderPageLoad::kEntryName,
          PrerenderPageLoad::kTiming_ActivationToFirstContentfulPaintName),
      0);
  EXPECT_EQ(
      GetUkmMetricEntryCount(
          PrerenderPageLoad::kEntryName,
          PrerenderPageLoad::kTiming_ActivationToLargestContentfulPaintName),
      0);
}
