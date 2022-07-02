// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#include "base/containers/contains.h"
#include "chrome/browser/page_load_metrics/integration_tests/metric_integration_test.h"
#include "chrome/browser/prerender/prerender_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/page_load_metrics/browser/observers/core/uma_page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/observers/prerender_page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
#include "content/public/browser/prerender_trigger_type.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/prerender_test_util.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/features.h"

using PrerenderPageLoad = ukm::builders::PrerenderPageLoad;
using PageLoad = ukm::builders::PageLoad;

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

// TODO(crbug.com/1329881): Re-enable this test
#if BUILDFLAG(IS_MAC)
#define MAYBE_Activate_SpeculationRule DISABLED_Activate_SpeculationRule
#else
#define MAYBE_Activate_SpeculationRule Activate_SpeculationRule
#endif
IN_PROC_BROWSER_TEST_F(PrerenderPageLoadMetricsObserverBrowserTest,
                       MAYBE_Activate_SpeculationRule) {
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

  // Expect only FP and FCP for prerender are recorded.
  histogram_tester().ExpectTotalCount(
      prerender_helper_.GenerateHistogramName(
          internal::kHistogramPrerenderNavigationToActivation,
          content::PrerenderTriggerType::kSpeculationRule, ""),
      1);
  histogram_tester().ExpectTotalCount(
      prerender_helper_.GenerateHistogramName(
          internal::kHistogramPrerenderActivationToFirstPaint,
          content::PrerenderTriggerType::kSpeculationRule, ""),
      1);
  histogram_tester().ExpectTotalCount(internal::kHistogramFirstPaint, 0);
  histogram_tester().ExpectTotalCount(
      prerender_helper_.GenerateHistogramName(
          internal::kHistogramPrerenderActivationToFirstContentfulPaint,
          content::PrerenderTriggerType::kSpeculationRule, ""),
      1);
  histogram_tester().ExpectTotalCount(internal::kHistogramFirstContentfulPaint,
                                      0);

  // Simulate mouse click and wait for FirstInputDelay.
  content::SimulateMouseClick(web_contents(), 0,
                              blink::WebPointerProperties::Button::kLeft);
  waiter->AddPageExpectation(page_load_metrics::PageLoadMetricsTestWaiter::
                                 TimingField::kFirstInputDelay);
  waiter->Wait();

  // Expect only FID for prerender is recorded.
  histogram_tester().ExpectTotalCount(
      prerender_helper_.GenerateHistogramName(
          internal::kHistogramPrerenderFirstInputDelay4,
          content::PrerenderTriggerType::kSpeculationRule, ""),
      1);
  histogram_tester().ExpectTotalCount(internal::kHistogramFirstInputDelay, 0);

  // Force navigation to another page, which should force logging of histograms
  // persisted at the end of the page load lifetime.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  // Expect only LCP for prerender is recorded.
  histogram_tester().ExpectTotalCount(
      prerender_helper_.GenerateHistogramName(
          internal::kHistogramPrerenderActivationToLargestContentfulPaint2,
          content::PrerenderTriggerType::kSpeculationRule, ""),
      1);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramLargestContentfulPaint, 0);

  histogram_tester().ExpectTotalCount(
      prerender_helper_.GenerateHistogramName(
          internal::kHistogramPrerenderCumulativeShiftScore,
          content::PrerenderTriggerType::kSpeculationRule, ""),
      1);
  histogram_tester().ExpectTotalCount(
      prerender_helper_.GenerateHistogramName(
          internal::kHistogramPrerenderCumulativeShiftScoreMainFrame,
          content::PrerenderTriggerType::kSpeculationRule, ""),
      1);
  histogram_tester().ExpectTotalCount(
      prerender_helper_.GenerateHistogramName(
          internal::
              kHistogramPrerenderMaxCumulativeShiftScoreSessionWindowGap1000msMax5000ms2,
          content::PrerenderTriggerType::kSpeculationRule, ""),
      1);

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
  EXPECT_TRUE(ukm_recorder().EntryHasMetric(
      prerendered_page_entry,
      PrerenderPageLoad::kInteractiveTiming_FirstInputDelay4Name));
  EXPECT_TRUE(ukm_recorder().EntryHasMetric(
      prerendered_page_entry,
      PrerenderPageLoad::
          kLayoutInstability_MaxCumulativeShiftScore_SessionWindow_Gap1000ms_Max5000msName));
  EXPECT_FALSE(ukm_recorder().EntryHasMetric(
      prerendered_page_entry,
      PageLoad::kPaintTiming_NavigationToFirstContentfulPaintName));
  EXPECT_FALSE(ukm_recorder().EntryHasMetric(
      prerendered_page_entry,
      PageLoad::kPaintTiming_NavigationToLargestContentfulPaint2Name));

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

// TODO(crbug.com/1329881): Re-enable this test
#if BUILDFLAG(IS_MAC)
#define MAYBE_Activate_Embedder_DirectURLInput \
  DISABLED_Activate_Embedder_DirectURLInput
#else
#define MAYBE_Activate_Embedder_DirectURLInput Activate_Embedder_DirectURLInput
#endif
IN_PROC_BROWSER_TEST_F(PrerenderPageLoadMetricsObserverBrowserTest,
                       MAYBE_Activate_Embedder_DirectURLInput) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL prerender_url = embedded_test_server()->GetURL("/title2.html");

  // Hold PrerenderHandle while the test is executed to avoid canceling a
  // prerender host in the destruction of PrerenderHandle.
  std::unique_ptr<content::PrerenderHandle> prerender_handle =
      prerender_helper_.AddEmbedderTriggeredPrerenderAsync(
          prerender_url, content::PrerenderTriggerType::kEmbedder,
          prerender_utils::kDirectUrlInputMetricSuffix,
          ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                    ui::PAGE_TRANSITION_FROM_ADDRESS_BAR));
  EXPECT_TRUE(prerender_handle);

  // Wait until the completion of prerendering navigation.
  prerender_helper_.WaitForPrerenderLoadCompletion(prerender_url);
  int host_id = prerender_helper_.GetHostForUrl(prerender_url);
  EXPECT_NE(host_id, content::RenderFrameHost::kNoFrameTreeNodeId);

  // Activate and wait for FCP.
  auto waiter = std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
      web_contents());
  waiter->AddPageExpectation(page_load_metrics::PageLoadMetricsTestWaiter::
                                 TimingField::kFirstContentfulPaint);
  // Simulate a browser-initiated navigation.
  web_contents()->OpenURL(content::OpenURLParams(
      prerender_url, content::Referrer(), WindowOpenDisposition::CURRENT_TAB,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
      /*is_renderer_initiated=*/false));
  waiter->Wait();

  // Expect only FP and FCP for prerender are recorded.
  histogram_tester().ExpectTotalCount(
      prerender_helper_.GenerateHistogramName(
          internal::kHistogramPrerenderNavigationToActivation,
          content::PrerenderTriggerType::kEmbedder,
          prerender_utils::kDirectUrlInputMetricSuffix),
      1);
  histogram_tester().ExpectTotalCount(
      prerender_helper_.GenerateHistogramName(
          internal::kHistogramPrerenderActivationToFirstPaint,
          content::PrerenderTriggerType::kEmbedder,
          prerender_utils::kDirectUrlInputMetricSuffix),
      1);
  histogram_tester().ExpectTotalCount(internal::kHistogramFirstPaint, 0);
  histogram_tester().ExpectTotalCount(
      prerender_helper_.GenerateHistogramName(
          internal::kHistogramPrerenderActivationToFirstContentfulPaint,
          content::PrerenderTriggerType::kEmbedder,
          prerender_utils::kDirectUrlInputMetricSuffix),
      1);
  histogram_tester().ExpectTotalCount(internal::kHistogramFirstContentfulPaint,
                                      0);

  // Simulate mouse click and wait for FirstInputDelay.
  content::SimulateMouseClick(web_contents(), 0,
                              blink::WebPointerProperties::Button::kLeft);
  waiter->AddPageExpectation(page_load_metrics::PageLoadMetricsTestWaiter::
                                 TimingField::kFirstInputDelay);
  waiter->Wait();

  // Expect only FID for prerender is recorded.
  histogram_tester().ExpectTotalCount(
      prerender_helper_.GenerateHistogramName(
          internal::kHistogramPrerenderFirstInputDelay4,
          content::PrerenderTriggerType::kEmbedder,
          prerender_utils::kDirectUrlInputMetricSuffix),
      1);
  histogram_tester().ExpectTotalCount(internal::kHistogramFirstInputDelay, 0);

  // Force navigation to another page, which should force logging of
  // histograms persisted at the end of the page load lifetime.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  // Expect only LCP for prerender is recorded.
  histogram_tester().ExpectTotalCount(
      prerender_helper_.GenerateHistogramName(
          internal::kHistogramPrerenderActivationToLargestContentfulPaint2,
          content::PrerenderTriggerType::kEmbedder,
          prerender_utils::kDirectUrlInputMetricSuffix),
      1);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramLargestContentfulPaint, 0);

  histogram_tester().ExpectTotalCount(
      prerender_helper_.GenerateHistogramName(
          internal::kHistogramPrerenderCumulativeShiftScore,
          content::PrerenderTriggerType::kEmbedder,
          prerender_utils::kDirectUrlInputMetricSuffix),
      1);
  histogram_tester().ExpectTotalCount(
      prerender_helper_.GenerateHistogramName(
          internal::kHistogramPrerenderCumulativeShiftScoreMainFrame,
          content::PrerenderTriggerType::kEmbedder,
          prerender_utils::kDirectUrlInputMetricSuffix),
      1);
  histogram_tester().ExpectTotalCount(
      prerender_helper_.GenerateHistogramName(
          internal::
              kHistogramPrerenderMaxCumulativeShiftScoreSessionWindowGap1000msMax5000ms2,
          content::PrerenderTriggerType::kEmbedder,
          prerender_utils::kDirectUrlInputMetricSuffix),
      1);
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

// TODO(crbug.com/1329881): Re-enable this test
#if BUILDFLAG(IS_MAC)
#define MAYBE_Redirection DISABLED_Redirection
#else
#define MAYBE_Redirection Redirection
#endif
IN_PROC_BROWSER_TEST_F(PrerenderPageLoadMetricsObserverBrowserTest,
                       MAYBE_Redirection) {
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
