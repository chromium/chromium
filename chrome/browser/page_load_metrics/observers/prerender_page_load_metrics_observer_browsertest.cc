// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/contains.h"
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
            base::Unretained(this))) {}
  ~PrerenderPageLoadMetricsObserverBrowserTest() override = default;

  void SetUp() override {
    prerender_helper_.SetUp(embedded_test_server());
    MetricIntegrationTest::SetUp();
  }

  // Similar to UkmRecorder::GetMergedEntriesByName(), but returned map is keyed
  // by source URL.
  std::map<GURL, ukm::mojom::UkmEntryPtr> GetMergedUkmEntries(
      const std::string& entry_name) {
    auto entries =
        ukm_recorder().GetMergedEntriesByName(PrerenderPageLoad::kEntryName);
    std::map<GURL, ukm::mojom::UkmEntryPtr> result;
    for (auto& kv : entries) {
      const ukm::mojom::UkmEntry* entry = kv.second.get();
      const ukm::UkmSource* source =
          ukm_recorder().GetSourceForSourceId(entry->source_id);
      EXPECT_TRUE(source);
      EXPECT_TRUE(source->url().is_valid());
      result.emplace(source->url(), std::move(kv.second));
    }
    return result;
  }

 protected:
  content::test::PrerenderTestHelper prerender_helper_;
};

IN_PROC_BROWSER_TEST_F(PrerenderPageLoadMetricsObserverBrowserTest,
                       ActivateInForeground) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Navigate to an initial page.
  auto initial_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));

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
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  histogram_tester().ExpectTotalCount(
      internal::kHistogramPrerenderActivationToLargestContentfulPaint2, 1);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramPrerenderCumulativeShiftScore, 1);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramPrerenderCumulativeShiftScoreMainFrame, 1);

  auto entries = GetMergedUkmEntries(PrerenderPageLoad::kEntryName);
  EXPECT_EQ(2u, entries.size());

  const ukm::mojom::UkmEntry* prerendered_page_entry =
      entries[prerender_url].get();
  ASSERT_TRUE(prerendered_page_entry);
  EXPECT_FALSE(ukm_recorder().EntryHasMetric(
      prerendered_page_entry, PrerenderPageLoad::kTriggeredPrerenderName));
  ukm_recorder().ExpectEntryMetric(prerendered_page_entry,
                                   PrerenderPageLoad::kWasPrerenderedName, 1);
  EXPECT_TRUE(ukm_recorder().EntryHasMetric(
      prerendered_page_entry,
      PrerenderPageLoad::kTiming_NavigationToActivationName));
  EXPECT_TRUE(ukm_recorder().EntryHasMetric(
      prerendered_page_entry,
      PrerenderPageLoad::kTiming_ActivationToFirstContentfulPaintName));
  EXPECT_TRUE(ukm_recorder().EntryHasMetric(
      prerendered_page_entry,
      PrerenderPageLoad::kTiming_ActivationToLargestContentfulPaintName));

  const ukm::mojom::UkmEntry* initiator_page_entry = entries[initial_url].get();
  ASSERT_TRUE(initiator_page_entry);
  ukm_recorder().ExpectEntryMetric(
      initiator_page_entry, PrerenderPageLoad::kTriggeredPrerenderName, 1);
  EXPECT_FALSE(ukm_recorder().EntryHasMetric(
      initiator_page_entry, PrerenderPageLoad::kWasPrerenderedName));
  EXPECT_FALSE(ukm_recorder().EntryHasMetric(
      initiator_page_entry,
      PrerenderPageLoad::kTiming_NavigationToActivationName));
  EXPECT_FALSE(ukm_recorder().EntryHasMetric(
      initiator_page_entry,
      PrerenderPageLoad::kTiming_ActivationToFirstContentfulPaintName));
  EXPECT_FALSE(ukm_recorder().EntryHasMetric(
      initiator_page_entry,
      PrerenderPageLoad::kTiming_ActivationToLargestContentfulPaintName));
}

IN_PROC_BROWSER_TEST_F(PrerenderPageLoadMetricsObserverBrowserTest,
                       CancelledPrerender) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Navigate to an initial page.
  auto initial_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));

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
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  // As the prerender was cancelled, no prerendering metrics are recorded.
  EXPECT_EQ(0u, histogram_tester()
                    .GetTotalCountsForPrefix("PageLoad.Clients.Prerender.")
                    .size());

  auto entries = GetMergedUkmEntries(PrerenderPageLoad::kEntryName);
  EXPECT_FALSE(base::Contains(entries, prerender_url));
}

IN_PROC_BROWSER_TEST_F(PrerenderPageLoadMetricsObserverBrowserTest,
                       Redirection) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Navigate to an initial page.
  auto initial_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));

  // Start a prerender.
  GURL redirected_url = embedded_test_server()->GetURL("/title2.html");
  GURL prerender_url = embedded_test_server()->GetURL("/server-redirect?" +
                                                      redirected_url.spec());
  prerender_helper_.AddPrerender(prerender_url);

  // Activate and wait for FCP.
  auto waiter = std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
      web_contents());
  waiter->AddPageExpectation(page_load_metrics::PageLoadMetricsTestWaiter::
                                 TimingField::kFirstContentfulPaint);
  prerender_helper_.NavigatePrimaryPage(prerender_url);
  waiter->Wait();

  // Force navigation to another page, which should force logging of histograms
  // persisted at the end of the page load lifetime.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  // Verify that UKM records the URL after the redirection.
  auto entries = GetMergedUkmEntries(PrerenderPageLoad::kEntryName);
  ASSERT_FALSE(base::Contains(entries, prerender_url));
  const ukm::mojom::UkmEntry* prerendered_page_entry =
      entries[redirected_url].get();
  ASSERT_TRUE(prerendered_page_entry);

  EXPECT_FALSE(ukm_recorder().EntryHasMetric(
      prerendered_page_entry, PrerenderPageLoad::kTriggeredPrerenderName));
  ukm_recorder().ExpectEntryMetric(prerendered_page_entry,
                                   PrerenderPageLoad::kWasPrerenderedName, 1);
  EXPECT_TRUE(ukm_recorder().EntryHasMetric(
      prerendered_page_entry,
      PrerenderPageLoad::kTiming_NavigationToActivationName));
  EXPECT_TRUE(ukm_recorder().EntryHasMetric(
      prerendered_page_entry,
      PrerenderPageLoad::kTiming_ActivationToFirstContentfulPaintName));
  EXPECT_TRUE(ukm_recorder().EntryHasMetric(
      prerendered_page_entry,
      PrerenderPageLoad::kTiming_ActivationToLargestContentfulPaintName));
}
