// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/amp_page_load_metrics_observer.h"

#include <string>
#include <utility>

#include "base/macros.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "components/page_load_metrics/common/page_load_timing.h"
#include "components/page_load_metrics/common/test/page_load_metrics_test_util.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_utils.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "url/gurl.h"

using content::NavigationSimulator;

class AMPPageLoadMetricsObserverTest
    : public page_load_metrics::PageLoadMetricsObserverTestHarness {
 public:
  AMPPageLoadMetricsObserverTest() {}

  void SetUp() override {
    PageLoadMetricsObserverTestHarness::SetUp();
    ResetTest();
  }

  void ResetTest() {
    page_load_metrics::InitPageLoadTimingForTest(&timing_);
    // Reset to the default testing state. Does not reset histogram state.
    timing_.navigation_start = base::Time::FromDoubleT(1);
    timing_.response_start = base::TimeDelta::FromSeconds(2);
    timing_.parse_timing->parse_start = base::TimeDelta::FromSeconds(3);
    timing_.paint_timing->first_contentful_paint =
        base::TimeDelta::FromSeconds(4);
    timing_.paint_timing->first_image_paint = base::TimeDelta::FromSeconds(5);
    timing_.document_timing->load_event_start = base::TimeDelta::FromSeconds(7);
    PopulateRequiredTimingFields(&timing_);
  }

  void RunTest(const GURL& url) {
    NavigateAndCommit(url);
    tester()->SimulateTimingUpdate(timing_);

    // Navigate again to force OnComplete, which happens when a new navigation
    // occurs.
    NavigateAndCommit(GURL("http://otherurl.com"));
  }

  void ValidateHistogramsFor(const std::string& histogram,
                             const char* view_type,
                             const base::Optional<base::TimeDelta>& event,
                             bool expect_histograms) {
    const size_t kTypeOffset = strlen("PageLoad.Clients.AMP.");
    std::string view_type_histogram = histogram;
    view_type_histogram.insert(kTypeOffset, view_type);
    tester()->histogram_tester().ExpectTotalCount(histogram,
                                                  expect_histograms ? 1 : 0);
    tester()->histogram_tester().ExpectTotalCount(view_type_histogram,
                                                  expect_histograms ? 1 : 0);
    if (!expect_histograms)
      return;
    tester()->histogram_tester().ExpectUniqueSample(
        histogram,
        static_cast<base::HistogramBase::Sample>(
            event.value().InMilliseconds()),
        1);
    tester()->histogram_tester().ExpectUniqueSample(
        view_type_histogram,
        static_cast<base::HistogramBase::Sample>(
            event.value().InMilliseconds()),
        1);
  }

  ukm::mojom::UkmEntryPtr GetAmpPageLoadUkmEntry(const GURL& url) {
    ukm::mojom::UkmEntryPtr entry;
    for (auto& it : tester()->test_ukm_recorder().GetMergedEntriesByName(
             ukm::builders::AmpPageLoad::kEntryName)) {
      const ukm::UkmSource* source =
          tester()->test_ukm_recorder().GetSourceForSourceId(it.first);
      if (source->url() == url) {
        entry = std::move(it.second);
      }
    }
    return entry;
  }

 protected:
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    tracker->AddObserver(base::WrapUnique(new AMPPageLoadMetricsObserver()));
  }

  page_load_metrics::mojom::PageLoadTiming timing_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AMPPageLoadMetricsObserverTest);
};

TEST_F(AMPPageLoadMetricsObserverTest, AMPCachePage) {
  RunTest(GURL("https://cdn.ampproject.org/page"));
  EXPECT_TRUE(tester()
                  ->test_ukm_recorder()
                  .GetEntriesByName(ukm::builders::AmpPageLoad::kEntryName)
                  .empty());
}

TEST_F(AMPPageLoadMetricsObserverTest, GoogleSearchAMPCachePage) {
  RunTest(GURL("https://www.google.com/amp/page"));
  EXPECT_TRUE(tester()
                  ->test_ukm_recorder()
                  .GetEntriesByName(ukm::builders::AmpPageLoad::kEntryName)
                  .empty());
}

TEST_F(AMPPageLoadMetricsObserverTest, GoogleSearchAMPCachePageBaseURL) {
  RunTest(GURL("https://www.google.com/amp/"));
  EXPECT_TRUE(tester()
                  ->test_ukm_recorder()
                  .GetEntriesByName(ukm::builders::AmpPageLoad::kEntryName)
                  .empty());
}

TEST_F(AMPPageLoadMetricsObserverTest, GoogleNewsAMPCachePage) {
  RunTest(GURL("https://news.google.com/news/amp?page"));
  EXPECT_TRUE(tester()
                  ->test_ukm_recorder()
                  .GetEntriesByName(ukm::builders::AmpPageLoad::kEntryName)
                  .empty());
}

TEST_F(AMPPageLoadMetricsObserverTest, GoogleNewsAMPCachePageBaseURL) {
  RunTest(GURL("https://news.google.com/news/amp"));
  EXPECT_TRUE(tester()
                  ->test_ukm_recorder()
                  .GetEntriesByName(ukm::builders::AmpPageLoad::kEntryName)
                  .empty());
}

TEST_F(AMPPageLoadMetricsObserverTest, NonAMPPage) {
  RunTest(GURL("https://www.google.com/not-amp/page"));
  EXPECT_TRUE(tester()
                  ->test_ukm_recorder()
                  .GetEntriesByName(ukm::builders::AmpPageLoad::kEntryName)
                  .empty());
}

TEST_F(AMPPageLoadMetricsObserverTest, GoogleSearchAMPViewerSameDocument) {
  NavigationSimulator::CreateRendererInitiated(
      GURL("https://www.google.com/search"), main_rfh())
      ->Commit();

  NavigationSimulator::CreateRendererInitiated(
      GURL("https://www.google.com/amp/page"), main_rfh())
      ->CommitSameDocument();

  // Verify that subframe metrics aren't recorded without an AMP subframe.
  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.AMP.Experimental.PageTiming.NavigationToInput.Subframe",
      0);
  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.AMP.Experimental.PageTiming.InputToNavigation.Subframe",
      0);
  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.AMP.Experimental.PageTiming."
      "MainFrameToSubFrameNavigationDelta.Subframe",
      0);

  EXPECT_TRUE(tester()
                  ->test_ukm_recorder()
                  .GetEntriesByName(ukm::builders::AmpPageLoad::kEntryName)
                  .empty());
}

TEST_F(AMPPageLoadMetricsObserverTest, SubFrameInputBeforeNavigation) {
  GURL main_frame_url("https://ampviewer.com/");
  GURL amp_url("https://ampviewer.com/page");

  // This emulates the AMP subframe non-prerender flow: first we perform a
  // same-document navigation in the main frame to the AMP viewer URL, then we
  // create and navigate the subframe to an AMP cache URL.
  NavigationSimulator::CreateRendererInitiated(main_frame_url, main_rfh())
      ->Commit();

  NavigationSimulator::CreateRendererInitiated(amp_url, main_rfh())
      ->CommitSameDocument();

  content::RenderFrameHost* subframe =
      NavigationSimulator::NavigateAndCommitFromDocument(
          GURL("https://ampsubframe.com/page"
               "?amp_js_v=0.1#viewerUrl=https%3A%2F%2Fampviewer.com%2Fpage"),
          content::RenderFrameHostTester::For(web_contents()->GetMainFrame())
              ->AppendChild("subframe"));

  page_load_metrics::mojom::PageLoadMetadata metadata;
  metadata.behavior_flags =
      blink::LoadingBehaviorFlag::kLoadingBehaviorAmpDocumentLoaded;
  tester()->SimulateMetadataUpdate(metadata, subframe);

  // Navigate the main frame to trigger metrics recording.
  NavigationSimulator::CreateRendererInitiated(
      GURL("https://ampviewer.com/other"), main_rfh())
      ->CommitSameDocument();

  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.AMP.Experimental.PageTiming.NavigationToInput.Subframe",
      0);
  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.AMP.Experimental.PageTiming.InputToNavigation.Subframe",
      1);
  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.AMP.Experimental.PageTiming."
      "MainFrameToSubFrameNavigationDelta.Subframe",
      0);

  ukm::mojom::UkmEntryPtr main_frame_entry =
      GetAmpPageLoadUkmEntry(main_frame_url);
  ukm::mojom::UkmEntryPtr sub_frame_entry = GetAmpPageLoadUkmEntry(amp_url);
  ASSERT_NE(nullptr, main_frame_entry.get());
  ASSERT_NE(nullptr, sub_frame_entry.get());

  // We expect a source with a negative NavigationDelta metric, since the main
  // frame navigation occurred before the AMP subframe navigation.
  const int64_t* nav_delta_metric =
      tester()->test_ukm_recorder().GetEntryMetric(
          sub_frame_entry.get(), "SubFrame.MainFrameToSubFrameNavigationDelta");
  EXPECT_NE(nullptr, nav_delta_metric);
  EXPECT_GE(*nav_delta_metric, 0ll);

  const int64_t* amp_subframe_metric =
      tester()->test_ukm_recorder().GetEntryMetric(main_frame_entry.get(),
                                                   "SubFrameAmpPageLoad");
  EXPECT_NE(nullptr, amp_subframe_metric);
  EXPECT_GE(*amp_subframe_metric, 1ll);
  EXPECT_EQ(nullptr, tester()->test_ukm_recorder().GetEntryMetric(
                         main_frame_entry.get(), "MainFrameAmpPageLoad"));
}

TEST_F(AMPPageLoadMetricsObserverTest, SubFrameNavigationBeforeInput) {
  GURL amp_url("https://ampviewer.com/page");

  // This emulates the AMP subframe prerender flow: first we create and navigate
  // the subframe to an AMP cache URL, then we perform a same-document
  // navigation in the main frame to the AMP viewer URL.
  NavigationSimulator::CreateRendererInitiated(GURL("https://ampviewer.com/"),
                                               main_rfh())
      ->Commit();

  content::RenderFrameHost* subframe =
      NavigationSimulator::NavigateAndCommitFromDocument(
          GURL("https://ampsubframe.com/page"
               "?amp_js_v=0.1#viewerUrl=https%3A%2F%2Fampviewer.com%2Fpage"),
          content::RenderFrameHostTester::For(web_contents()->GetMainFrame())
              ->AppendChild("subframe"));

  NavigationSimulator::CreateRendererInitiated(amp_url, main_rfh())
      ->CommitSameDocument();

  page_load_metrics::mojom::PageLoadMetadata metadata;
  metadata.behavior_flags =
      blink::LoadingBehaviorFlag::kLoadingBehaviorAmpDocumentLoaded;
  tester()->SimulateMetadataUpdate(metadata, subframe);

  // Navigate the main frame to trigger metrics recording.
  NavigationSimulator::CreateRendererInitiated(
      GURL("https://ampviewer.com/other"), main_rfh())
      ->CommitSameDocument();

  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.AMP.Experimental.PageTiming.NavigationToInput.Subframe",
      1);
  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.AMP.Experimental.PageTiming.InputToNavigation.Subframe",
      0);
  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.AMP.Experimental.PageTiming."
      "MainFrameToSubFrameNavigationDelta.Subframe",
      0);

  ukm::mojom::UkmEntryPtr entry = GetAmpPageLoadUkmEntry(amp_url);
  ASSERT_NE(nullptr, entry.get());

  // We expect a source with a positive NavigationDelta metric, since the main
  // frame navigation occurred after the AMP subframe navigation.
  tester()->test_ukm_recorder().ExpectEntrySourceHasUrl(entry.get(), amp_url);
  const int64_t* nav_delta_metric =
      tester()->test_ukm_recorder().GetEntryMetric(
          entry.get(), "SubFrame.MainFrameToSubFrameNavigationDelta");
  EXPECT_NE(nullptr, nav_delta_metric);
  EXPECT_LE(*nav_delta_metric, 0ll);
}

TEST_F(AMPPageLoadMetricsObserverTest, SubFrameMetrics) {
  GURL amp_url("https://ampviewer.com/page");

  NavigationSimulator::CreateRendererInitiated(GURL("https://ampviewer.com/"),
                                               main_rfh())
      ->Commit();

  NavigationSimulator::CreateRendererInitiated(amp_url, main_rfh())
      ->CommitSameDocument();

  content::RenderFrameHost* subframe =
      NavigationSimulator::NavigateAndCommitFromDocument(
          GURL("https://ampsubframe.com/page"
               "?amp_js_v=0.1#viewerUrl=https%3A%2F%2Fampviewer.com%2Fpage"),
          content::RenderFrameHostTester::For(web_contents()->GetMainFrame())
              ->AppendChild("subframe"));

  page_load_metrics::mojom::PageLoadMetadata metadata;
  metadata.behavior_flags =
      blink::LoadingBehaviorFlag::kLoadingBehaviorAmpDocumentLoaded;
  tester()->SimulateMetadataUpdate(metadata, subframe);

  page_load_metrics::mojom::PageLoadTiming subframe_timing;
  page_load_metrics::InitPageLoadTimingForTest(&subframe_timing);
  subframe_timing.navigation_start = base::Time::FromDoubleT(2);
  subframe_timing.paint_timing->first_contentful_paint =
      base::TimeDelta::FromMilliseconds(5);
  subframe_timing.paint_timing->largest_image_paint_size = 1;
  subframe_timing.paint_timing->largest_image_paint =
      base::TimeDelta::FromMilliseconds(10);
  subframe_timing.interactive_timing->first_input_timestamp =
      base::TimeDelta::FromMilliseconds(20);
  subframe_timing.interactive_timing->first_input_delay =
      base::TimeDelta::FromMilliseconds(3);
  PopulateRequiredTimingFields(&subframe_timing);

  tester()->SimulateTimingUpdate(subframe_timing, subframe);

  // Navigate the main frame to trigger metrics recording.
  NavigationSimulator::CreateRendererInitiated(
      GURL("https://ampviewer.com/other"), main_rfh())
      ->CommitSameDocument();

  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.AMP.PaintTiming.InputToFirstContentfulPaint.Subframe",
      1);
  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.AMP.PaintTiming.InputToLargestContentfulPaint.Subframe",
      1);
  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.AMP.InteractiveTiming.FirstInputDelay4.Subframe", 1);

  ukm::mojom::UkmEntryPtr entry = GetAmpPageLoadUkmEntry(amp_url);
  ASSERT_NE(nullptr, entry.get());
  tester()->test_ukm_recorder().ExpectEntrySourceHasUrl(entry.get(), amp_url);
  tester()->test_ukm_recorder().ExpectEntryMetric(
      entry.get(), "SubFrame.InteractiveTiming.FirstInputDelay4", 3);
  tester()->test_ukm_recorder().ExpectEntryMetric(
      entry.get(), "SubFrame.PaintTiming.NavigationToFirstContentfulPaint", 5);
  tester()->test_ukm_recorder().ExpectEntryMetric(
      entry.get(), "SubFrame.PaintTiming.NavigationToLargestContentfulPaint",
      10);
}

TEST_F(AMPPageLoadMetricsObserverTest, SubFrameMetrics_LayoutInstability) {
  GURL amp_url("https://ampviewer.com/page");

  NavigationSimulator::CreateRendererInitiated(GURL("https://ampviewer.com/"),
                                               main_rfh())
      ->Commit();

  NavigationSimulator::CreateRendererInitiated(amp_url, main_rfh())
      ->CommitSameDocument();

  content::RenderFrameHost* subframe =
      NavigationSimulator::NavigateAndCommitFromDocument(
          GURL("https://ampsubframe.com/page"
               "?amp_js_v=0.1#viewerUrl=https%3A%2F%2Fampviewer.com%2Fpage"),
          content::RenderFrameHostTester::For(web_contents()->GetMainFrame())
              ->AppendChild("subframe"));

  page_load_metrics::mojom::PageLoadMetadata metadata;
  metadata.behavior_flags =
      blink::LoadingBehaviorFlag::kLoadingBehaviorAmpDocumentLoaded;
  tester()->SimulateMetadataUpdate(metadata, subframe);

  page_load_metrics::mojom::FrameRenderDataUpdate render_data(1.0, 0.5);
  tester()->SimulateRenderDataUpdate(render_data, subframe);

  // Navigate the main frame to trigger metrics recording.
  NavigationSimulator::CreateRendererInitiated(
      GURL("https://ampviewer.com/other"), main_rfh())
      ->CommitSameDocument();

  tester()->histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.AMP.LayoutInstability.CumulativeShiftScore.Subframe",
      10, 1);

  ukm::mojom::UkmEntryPtr entry = GetAmpPageLoadUkmEntry(amp_url);
  ASSERT_NE(nullptr, entry.get());
  tester()->test_ukm_recorder().ExpectEntrySourceHasUrl(entry.get(), amp_url);
  tester()->test_ukm_recorder().ExpectEntryMetric(
      entry.get(), "SubFrame.LayoutInstability.CumulativeShiftScore", 100);
  tester()->test_ukm_recorder().ExpectEntryMetric(
      entry.get(),
      "SubFrame.LayoutInstability.CumulativeShiftScore.BeforeInputOrScroll",
      50);
}

TEST_F(AMPPageLoadMetricsObserverTest, SubFrameMetricsFullNavigation) {
  GURL amp_url("https://ampviewer.com/page");

  NavigationSimulator::CreateRendererInitiated(amp_url, main_rfh())->Commit();

  content::RenderFrameHost* subframe =
      NavigationSimulator::NavigateAndCommitFromDocument(
          GURL("https://ampsubframe.com/page"
               "?amp_js_v=0.1#viewerUrl=https%3A%2F%2Fampviewer.com%2Fpage"),
          content::RenderFrameHostTester::For(web_contents()->GetMainFrame())
              ->AppendChild("subframe"));

  page_load_metrics::mojom::PageLoadMetadata metadata;
  metadata.behavior_flags =
      blink::LoadingBehaviorFlag::kLoadingBehaviorAmpDocumentLoaded;
  tester()->SimulateMetadataUpdate(metadata, subframe);

  page_load_metrics::mojom::PageLoadTiming subframe_timing;
  page_load_metrics::InitPageLoadTimingForTest(&subframe_timing);
  subframe_timing.navigation_start = base::Time::FromDoubleT(2);
  subframe_timing.paint_timing->first_contentful_paint =
      base::TimeDelta::FromMilliseconds(5);
  subframe_timing.paint_timing->largest_image_paint_size = 1;
  subframe_timing.paint_timing->largest_image_paint =
      base::TimeDelta::FromMilliseconds(10);
  subframe_timing.interactive_timing->first_input_timestamp =
      base::TimeDelta::FromMilliseconds(20);
  subframe_timing.interactive_timing->first_input_delay =
      base::TimeDelta::FromMilliseconds(3);
  PopulateRequiredTimingFields(&subframe_timing);

  tester()->SimulateTimingUpdate(subframe_timing, subframe);

  // Navigate the main frame to trigger metrics recording.
  NavigationSimulator::CreateRendererInitiated(
      GURL("https://ampviewer.com/other"), main_rfh())
      ->CommitSameDocument();

  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.AMP.PaintTiming.InputToFirstContentfulPaint.Subframe."
      "FullNavigation",
      1);
  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.AMP.PaintTiming.InputToLargestContentfulPaint.Subframe."
      "FullNavigation",
      1);
  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.AMP.InteractiveTiming.FirstInputDelay4.Subframe."
      "FullNavigation",
      1);

  ukm::mojom::UkmEntryPtr entry = GetAmpPageLoadUkmEntry(amp_url);
  tester()->test_ukm_recorder().ExpectEntrySourceHasUrl(entry.get(), amp_url);
  tester()->test_ukm_recorder().ExpectEntryMetric(
      entry.get(), "SubFrame.InteractiveTiming.FirstInputDelay4", 3);
  tester()->test_ukm_recorder().ExpectEntryMetric(
      entry.get(), "SubFrame.PaintTiming.NavigationToFirstContentfulPaint", 5);
  tester()->test_ukm_recorder().ExpectEntryMetric(
      entry.get(), "SubFrame.PaintTiming.NavigationToLargestContentfulPaint",
      10);
}

TEST_F(AMPPageLoadMetricsObserverTest, SubFrameRecordOnFullNavigation) {
  GURL amp_url("https://ampviewer.com/page");

  NavigationSimulator::CreateRendererInitiated(GURL("https://ampviewer.com/"),
                                               main_rfh())
      ->Commit();

  NavigationSimulator::CreateRendererInitiated(amp_url, main_rfh())
      ->CommitSameDocument();

  content::RenderFrameHost* subframe =
      NavigationSimulator::NavigateAndCommitFromDocument(
          GURL("https://ampsubframe.com/page"
               "?amp_js_v=0.1#viewerUrl=https%3A%2F%2Fampviewer.com%2Fpage"),
          content::RenderFrameHostTester::For(web_contents()->GetMainFrame())
              ->AppendChild("subframe"));

  page_load_metrics::mojom::PageLoadMetadata metadata;
  metadata.behavior_flags =
      blink::LoadingBehaviorFlag::kLoadingBehaviorAmpDocumentLoaded;
  tester()->SimulateMetadataUpdate(metadata, subframe);

  // Navigate the main frame to trigger metrics recording.
  NavigationSimulator::CreateRendererInitiated(GURL("https://www.example.com/"),
                                               main_rfh())
      ->Commit();

  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.AMP.Experimental.PageTiming.InputToNavigation.Subframe",
      1);

  // We expect a source with a negative NavigationDelta metric, since the main
  // frame navigation occurred before the AMP subframe navigation.
  ukm::mojom::UkmEntryPtr entry = GetAmpPageLoadUkmEntry(amp_url);
  tester()->test_ukm_recorder().ExpectEntrySourceHasUrl(entry.get(), amp_url);
  const int64_t* nav_delta_metric =
      tester()->test_ukm_recorder().GetEntryMetric(
          entry.get(), "SubFrame.MainFrameToSubFrameNavigationDelta");
  EXPECT_NE(nullptr, nav_delta_metric);
  EXPECT_GE(*nav_delta_metric, 0ll);
}

TEST_F(AMPPageLoadMetricsObserverTest, SubFrameRecordOnFrameDeleted) {
  GURL amp_url("https://ampviewer.com/page");

  NavigationSimulator::CreateRendererInitiated(GURL("https://ampviewer.com/"),
                                               main_rfh())
      ->Commit();

  NavigationSimulator::CreateRendererInitiated(amp_url, main_rfh())
      ->CommitSameDocument();

  content::RenderFrameHost* subframe =
      NavigationSimulator::NavigateAndCommitFromDocument(
          GURL("https://ampsubframe.com/page"
               "?amp_js_v=0.1#viewerUrl=https%3A%2F%2Fampviewer.com%2Fpage"),
          content::RenderFrameHostTester::For(web_contents()->GetMainFrame())
              ->AppendChild("subframe"));

  page_load_metrics::mojom::PageLoadMetadata metadata;
  metadata.behavior_flags =
      blink::LoadingBehaviorFlag::kLoadingBehaviorAmpDocumentLoaded;
  tester()->SimulateMetadataUpdate(metadata, subframe);

  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.AMP.Experimental.PageTiming.InputToNavigation.Subframe",
      0);

  // Delete the subframe, which should trigger metrics recording.
  content::RenderFrameHostTester::For(subframe)->Detach();

  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.AMP.Experimental.PageTiming.InputToNavigation.Subframe",
      1);

  // We expect a source with a negative NavigationDelta metric, since the main
  // frame navigation occurred before the AMP subframe navigation.
  ukm::mojom::UkmEntryPtr entry = GetAmpPageLoadUkmEntry(amp_url);
  tester()->test_ukm_recorder().ExpectEntrySourceHasUrl(entry.get(), amp_url);
  const int64_t* nav_delta_metric =
      tester()->test_ukm_recorder().GetEntryMetric(
          entry.get(), "SubFrame.MainFrameToSubFrameNavigationDelta");
  EXPECT_NE(nullptr, nav_delta_metric);
  EXPECT_GE(*nav_delta_metric, 0ll);
}

TEST_F(AMPPageLoadMetricsObserverTest, SubFrameMultipleFrames) {
  GURL main_frame_url("https://ampviewer.com/");
  GURL amp_url1("https://ampviewer.com/page");
  GURL amp_url2("https://ampviewer.com/page2");

  NavigationSimulator::CreateRendererInitiated(main_frame_url, main_rfh())
      ->Commit();

  // Simulate a prerender.
  content::RenderFrameHost* subframe2 =
      NavigationSimulator::NavigateAndCommitFromDocument(
          GURL("https://ampsubframe.com/page2"
               "?amp_js_v=0.1#viewerUrl=https%3A%2F%2Fampviewer.com%2Fpage2"),
          content::RenderFrameHostTester::For(web_contents()->GetMainFrame())
              ->AppendChild("subframe2"));

  // Perform a main-frame navigation to a different AMP document (not the
  // prerender).
  NavigationSimulator::CreateRendererInitiated(amp_url1, main_rfh())
      ->CommitSameDocument();

  // Load the associated AMP document in an iframe.
  content::RenderFrameHost* subframe1 =
      NavigationSimulator::NavigateAndCommitFromDocument(
          GURL("https://ampsubframe.com/page"
               "?amp_js_v=0.1#viewerUrl=https%3A%2F%2Fampviewer.com%2Fpage"),
          content::RenderFrameHostTester::For(web_contents()->GetMainFrame())
              ->AppendChild("subframe1"));

  page_load_metrics::mojom::PageLoadMetadata metadata;
  metadata.behavior_flags =
      blink::LoadingBehaviorFlag::kLoadingBehaviorAmpDocumentLoaded;
  tester()->SimulateMetadataUpdate(metadata, subframe1);
  tester()->SimulateMetadataUpdate(metadata, subframe2);

  // Navigate the main frame to trigger metrics recording - we expect metrics to
  // have been recorded for 1 AMP page (the non-prerendered page).
  NavigationSimulator::CreateRendererInitiated(
      GURL("https://ampviewer.com/other"), main_rfh())
      ->CommitSameDocument();

  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.AMP.Experimental.PageTiming.NavigationToInput.Subframe",
      0);
  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.AMP.Experimental.PageTiming.InputToNavigation.Subframe",
      1);
  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.AMP.Experimental.PageTiming."
      "MainFrameToSubFrameNavigationDelta.Subframe",
      0);

  // Now navigate to the main-frame URL for the prerendered AMP document. This
  // should trigger metrics recording for that document.
  NavigationSimulator::CreateRendererInitiated(amp_url2, main_rfh())
      ->CommitSameDocument();

  // Navigate the main frame to trigger metrics recording.
  NavigationSimulator::CreateRendererInitiated(
      GURL("https://ampviewer.com/other"), main_rfh())
      ->CommitSameDocument();

  // We now expect one NavigationToInput (for the prerender) and one
  // InputToNavigation (for the non-prerender).
  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.AMP.Experimental.PageTiming.NavigationToInput.Subframe",
      1);
  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.AMP.Experimental.PageTiming.InputToNavigation.Subframe",
      1);
  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.AMP.Experimental.PageTiming."
      "MainFrameToSubFrameNavigationDelta.Subframe",
      0);

  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> entries =
      tester()->test_ukm_recorder().GetMergedEntriesByName(
          ukm::builders::AmpPageLoad::kEntryName);
  EXPECT_EQ(3ull, entries.size());

  const ukm::UkmSource* source1 = nullptr;
  const ukm::UkmSource* source2 = nullptr;
  for (const auto& kv : entries) {
    const ukm::UkmSource* candidate =
        tester()->test_ukm_recorder().GetSourceForSourceId(kv.first);
    ASSERT_NE(nullptr, candidate);
    if (candidate->url() == amp_url1) {
      source1 = candidate;
    } else if (candidate->url() == amp_url2) {
      source2 = candidate;
    } else if (candidate->url() == main_frame_url) {
      // Ignore.
    } else {
      FAIL() << "Encountered unexpected source for: " << candidate->url();
    }
  }
  EXPECT_NE(source1, source2);

  const ukm::mojom::UkmEntry* entry1 = entries.at(source1->id()).get();
  EXPECT_NE(nullptr, entry1);
  const ukm::mojom::UkmEntry* entry2 = entries.at(source2->id()).get();
  EXPECT_NE(nullptr, entry2);

  // The entry for amp_url1 should have a negative NavigationDelta metric, since
  // the main frame navigation occurred before the AMP subframe navigation.
  const int64_t* entry1_nav_delta_metric =
      tester()->test_ukm_recorder().GetEntryMetric(
          entry1, "SubFrame.MainFrameToSubFrameNavigationDelta");
  EXPECT_NE(nullptr, entry1_nav_delta_metric);
  EXPECT_GE(*entry1_nav_delta_metric, 0ll);

  // The entry for amp_url2 should have a positive NavigationDelta metric, since
  // the main frame navigation occurred after the AMP subframe navigation.
  const int64_t* entry2_nav_delta_metric =
      tester()->test_ukm_recorder().GetEntryMetric(
          entry2, "SubFrame.MainFrameToSubFrameNavigationDelta");
  EXPECT_NE(nullptr, entry2_nav_delta_metric);
  EXPECT_LE(*entry2_nav_delta_metric, 0ll);
}

TEST_F(AMPPageLoadMetricsObserverTest,
       SubFrameWithNonSameDocumentMainFrameNavigation) {
  GURL amp_url("https://ampviewer.com/page");

  NavigationSimulator::CreateRendererInitiated(amp_url, main_rfh())->Commit();

  // Load the associated AMP document in an iframe.
  content::RenderFrameHost* subframe =
      NavigationSimulator::NavigateAndCommitFromDocument(
          GURL("https://ampsubframe.com/page"
               "?amp_js_v=0.1#viewerUrl=https%3A%2F%2Fampviewer.com%2Fpage"),
          content::RenderFrameHostTester::For(web_contents()->GetMainFrame())
              ->AppendChild("subframe"));

  page_load_metrics::mojom::PageLoadMetadata metadata;
  metadata.behavior_flags =
      blink::LoadingBehaviorFlag::kLoadingBehaviorAmpDocumentLoaded;
  tester()->SimulateMetadataUpdate(metadata, subframe);

  // Navigate the main frame to trigger metrics recording.
  NavigationSimulator::CreateRendererInitiated(
      GURL("https://ampviewer.com/other"), main_rfh())
      ->CommitSameDocument();

  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.AMP.Experimental.PageTiming.NavigationToInput.Subframe",
      0);
  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.AMP.Experimental.PageTiming.InputToNavigation.Subframe",
      0);
  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.AMP.Experimental.PageTiming."
      "MainFrameToSubFrameNavigationDelta.Subframe",
      1);

  // We expect a source with a negative NavigationDelta metric, since the main
  // frame navigation occurred before the AMP subframe navigation.
  ukm::mojom::UkmEntryPtr entry = GetAmpPageLoadUkmEntry(amp_url);
  tester()->test_ukm_recorder().ExpectEntrySourceHasUrl(entry.get(), amp_url);
  const int64_t* nav_delta_metric =
      tester()->test_ukm_recorder().GetEntryMetric(
          entry.get(), "SubFrame.MainFrameToSubFrameNavigationDelta");
  EXPECT_NE(nullptr, nav_delta_metric);
  EXPECT_GE(*nav_delta_metric, 0ll);
}

TEST_F(AMPPageLoadMetricsObserverTest, NoSubFrameMetricsForNonAmpSubFrame) {
  NavigationSimulator::CreateRendererInitiated(GURL("https://ampviewer.com/"),
                                               main_rfh())
      ->Commit();

  NavigationSimulator::CreateRendererInitiated(
      GURL("https://ampviewer.com/page"), main_rfh())
      ->CommitSameDocument();

  // Create a non-AMP subframe document.
  NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("https://example.com/"),
      content::RenderFrameHostTester::For(web_contents()->GetMainFrame())
          ->AppendChild("subframe"));

  // Navigate the main frame to trigger metrics recording.
  NavigationSimulator::CreateRendererInitiated(
      GURL("https://ampviewer.com/other"), main_rfh())
      ->CommitSameDocument();

  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.AMP.Experimental.PageTiming.NavigationToInput.Subframe",
      0);
  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.AMP.Experimental.PageTiming.InputToNavigation.Subframe",
      0);
  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.AMP.Experimental.PageTiming."
      "MainFrameToSubFrameNavigationDelta.Subframe",
      0);

  EXPECT_TRUE(tester()
                  ->test_ukm_recorder()
                  .GetEntriesByName(ukm::builders::AmpPageLoad::kEntryName)
                  .empty());
}

TEST_F(AMPPageLoadMetricsObserverTest,
       NoSubFrameMetricsForSubFrameWithoutViewerUrl) {
  GURL subframe_url("https://ampviewer.com/page");
  NavigationSimulator::CreateRendererInitiated(GURL("https://ampviewer.com/"),
                                               main_rfh())
      ->Commit();

  NavigationSimulator::CreateRendererInitiated(GURL(subframe_url), main_rfh())
      ->CommitSameDocument();

  content::RenderFrameHost* subframe =
      NavigationSimulator::NavigateAndCommitFromDocument(
          GURL(subframe_url),
          content::RenderFrameHostTester::For(web_contents()->GetMainFrame())
              ->AppendChild("subframe"));

  page_load_metrics::mojom::PageLoadMetadata metadata;
  metadata.behavior_flags =
      blink::LoadingBehaviorFlag::kLoadingBehaviorAmpDocumentLoaded;
  tester()->SimulateMetadataUpdate(metadata, subframe);

  // Navigate the main frame to trigger metrics recording.
  NavigationSimulator::CreateRendererInitiated(
      GURL("https://ampviewer.com/other"), main_rfh())
      ->CommitSameDocument();

  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.AMP.Experimental.PageTiming.NavigationToInput.Subframe",
      0);
  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.AMP.Experimental.PageTiming.InputToNavigation.Subframe",
      0);
  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.AMP.Experimental.PageTiming."
      "MainFrameToSubFrameNavigationDelta.Subframe",
      0);

  ukm::mojom::UkmEntryPtr entry = GetAmpPageLoadUkmEntry(subframe_url);
  EXPECT_EQ(nullptr, entry.get());
}
