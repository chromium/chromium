// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/from_gws_page_load_metrics_observer.h"

#include <vector>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"
#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "components/page_load_metrics/common/test/page_load_metrics_test_util.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "url/gurl.h"

namespace {

constexpr char kExampleUrl[] = "http://www.example.com/";
constexpr char kExampleUrl2[] = "http://www.example2.com/";
constexpr char kGoogleSearchResultsUrl[] = "https://www.google.com/webhp?q=d";
constexpr char kGoogleSearchResultsVideoUrl[] =
    "https://www.google.co.uk/search?q=test&tbm=vid";

}  // namespace

class FromGWSPageLoadMetricsObserverTest
    : public page_load_metrics::PageLoadMetricsObserverTestHarness {
 public:
  // page_load_metrics::PageLoadMetricsObserverTestHarness:
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    auto observer = std::make_unique<FromGWSPageLoadMetricsObserver>();
    observer_ = observer.get();
    tracker->AddObserver(std::move(observer));
  }

  void SimulateTimingWithoutPaint() {
    page_load_metrics::mojom::PageLoadTiming timing;
    page_load_metrics::InitPageLoadTimingForTest(&timing);
    timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
    tester()->SimulateTimingUpdate(timing);
  }

  void SimulateTimingWithFirstPaint() {
    page_load_metrics::mojom::PageLoadTiming timing;
    page_load_metrics::InitPageLoadTimingForTest(&timing);
    timing.parse_timing->parse_start = base::Milliseconds(0);
    timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
    timing.paint_timing->first_paint = base::Milliseconds(0);
    PopulateRequiredTimingFields(&timing);
    tester()->SimulateTimingUpdate(timing);
  }

  void SimulateMouseEvent() {
    blink::WebMouseEvent mouse_event(
        blink::WebInputEvent::Type::kMouseDown,
        blink::WebInputEvent::kNoModifiers,
        blink::WebInputEvent::GetStaticTimeStampForTests());
    mouse_event.button = blink::WebMouseEvent::Button::kLeft;
    mouse_event.SetPositionInWidget(7, 7);
    mouse_event.click_count = 1;
    tester()->SimulateInputEvent(mouse_event);
  }

 protected:
  raw_ptr<FromGWSPageLoadMetricsObserver, DanglingUntriaged> observer_ =
      nullptr;
};

class FromGWSPageLoadMetricsLoggerTest : public testing::Test {};

TEST_F(FromGWSPageLoadMetricsObserverTest, NoMetrics) {
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramFromGWSFirstImagePaint, 0);
  EXPECT_EQ(0ul, tester()->test_ukm_recorder().entries_count());
}

TEST_F(FromGWSPageLoadMetricsObserverTest, NoPreviousCommittedUrl) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.paint_timing->first_image_paint = base::Milliseconds(1);
  PopulateRequiredTimingFields(&timing);
  NavigateAndCommit(GURL(kExampleUrl));

  tester()->SimulateTimingUpdate(timing);

  // Navigate again to force logging.
  tester()->NavigateToUntrackedUrl();
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramFromGWSFirstImagePaint, 0);
  auto entries = tester()->test_ukm_recorder().GetEntriesByName(
      ukm::builders::PageLoad_FromGoogleSearch::kEntryName);
  EXPECT_EQ(0u, entries.size());
  size_t document_created_entries_count =
      tester()->test_ukm_recorder().GetEntriesByName("DocumentCreated").size();
  size_t unload_entries_count =
      tester()->test_ukm_recorder().GetEntriesByName("Unload").size();
  EXPECT_EQ(document_created_entries_count, unload_entries_count);
}

TEST_F(FromGWSPageLoadMetricsObserverTest, NonSearchPreviousCommittedUrl) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.paint_timing->first_image_paint = base::Milliseconds(1);
  PopulateRequiredTimingFields(&timing);
  NavigateAndCommit(GURL("http://www.other.com"));
  NavigateAndCommit(GURL(kExampleUrl));

  tester()->SimulateTimingUpdate(timing);

  // Navigate again to force logging.
  tester()->NavigateToUntrackedUrl();
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramFromGWSFirstImagePaint, 0);
  auto entries = tester()->test_ukm_recorder().GetEntriesByName(
      ukm::builders::PageLoad_FromGoogleSearch::kEntryName);
  EXPECT_EQ(0u, entries.size());
  size_t document_created_entries_count =
      tester()->test_ukm_recorder().GetEntriesByName("DocumentCreated").size();
  size_t unload_entries_count =
      tester()->test_ukm_recorder().GetEntriesByName("Unload").size();
  EXPECT_EQ(document_created_entries_count, unload_entries_count);
}

TEST_F(FromGWSPageLoadMetricsObserverTest,
       GoogleNonSearchPreviousCommittedUrl1) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.paint_timing->first_image_paint = base::Milliseconds(1);
  PopulateRequiredTimingFields(&timing);
  NavigateAndCommit(GURL("https://www.google.com/"));
  NavigateAndCommit(GURL(kExampleUrl));

  tester()->SimulateTimingUpdate(timing);

  // Navigate again to force logging.
  tester()->NavigateToUntrackedUrl();
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramFromGWSFirstImagePaint, 0);
  auto entries = tester()->test_ukm_recorder().GetEntriesByName(
      ukm::builders::PageLoad_FromGoogleSearch::kEntryName);
  EXPECT_EQ(0u, entries.size());
  size_t document_created_entries_count =
      tester()->test_ukm_recorder().GetEntriesByName("DocumentCreated").size();
  size_t unload_entries_count =
      tester()->test_ukm_recorder().GetEntriesByName("Unload").size();
  EXPECT_EQ(document_created_entries_count, unload_entries_count);
}

TEST_F(FromGWSPageLoadMetricsObserverTest,
       GoogleNonSearchPreviousCommittedUrl2) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.paint_timing->first_image_paint = base::Milliseconds(1);
  PopulateRequiredTimingFields(&timing);
  // Navigation from /search, but missing a query string, so can't have been a
  // search results page.
  NavigateAndCommit(GURL("https://www.google.com/search?a=b&c=d"));
  NavigateAndCommit(GURL(kExampleUrl));

  tester()->SimulateTimingUpdate(timing);

  // Navigate again to force logging.
  tester()->NavigateToUntrackedUrl();
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramFromGWSFirstImagePaint, 0);
  auto entries = tester()->test_ukm_recorder().GetEntriesByName(
      ukm::builders::PageLoad_FromGoogleSearch::kEntryName);
  EXPECT_EQ(0u, entries.size());
  size_t document_created_entries_count =
      tester()->test_ukm_recorder().GetEntriesByName("DocumentCreated").size();
  size_t unload_entries_count =
      tester()->test_ukm_recorder().GetEntriesByName("Unload").size();
  EXPECT_EQ(document_created_entries_count, unload_entries_count);
}

TEST_F(FromGWSPageLoadMetricsObserverTest, SearchPreviousCommittedUrl1) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.parse_timing->parse_start = base::Milliseconds(10);
  timing.paint_timing->first_paint = base::Milliseconds(20);
  timing.paint_timing->first_contentful_paint = base::Milliseconds(40);
  timing.paint_timing->largest_contentful_paint->largest_text_paint =
      base::Milliseconds(50);
  timing.paint_timing->largest_contentful_paint->largest_text_paint_size = 20u;
  timing.paint_timing->first_image_paint = base::Milliseconds(160);
  timing.parse_timing->parse_stop = base::Milliseconds(320);
  timing.document_timing->dom_content_loaded_event_start =
      base::Milliseconds(640);
  timing.document_timing->load_event_start = base::Milliseconds(1280);
  timing.interactive_timing->first_input_delay = base::Milliseconds(50);
  timing.interactive_timing->first_input_timestamp = base::Milliseconds(1400);
  PopulateRequiredTimingFields(&timing);
  NavigateAndCommit(GURL("https://www.google.com/webhp?q=test"));
  NavigateAndCommit(GURL(kExampleUrl));

  tester()->SimulateTimingUpdate(timing);
  page_load_metrics::mojom::FrameRenderDataUpdate render_data(1.0, 1.0, {});
  tester()->SimulateRenderDataUpdate(render_data);
  render_data.layout_shift_delta = 1.5;
  render_data.layout_shift_delta_before_input_or_scroll = 0.0;
  render_data.new_layout_shifts.emplace_back(
      page_load_metrics::mojom::LayoutShift::New(base::TimeTicks::Now(), 0.5));
  tester()->SimulateRenderDataUpdate(render_data);

  // Navigate again to force logging.
  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramFromGWSParseStart, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramFromGWSParseStart,
      timing.parse_timing->parse_start.value().InMilliseconds(), 1);

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramFromGWSFirstPaint, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramFromGWSFirstPaint,
      timing.paint_timing->first_paint.value().InMilliseconds(), 1);

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramFromGWSFirstContentfulPaint, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramFromGWSFirstContentfulPaint,
      timing.paint_timing->first_contentful_paint.value().InMilliseconds(), 1);

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramFromGWSLargestContentfulPaint, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramFromGWSLargestContentfulPaint,
      timing.paint_timing->largest_contentful_paint->largest_text_paint.value()
          .InMilliseconds(),
      1);

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramFromGWSParseStartToFirstContentfulPaint, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramFromGWSParseStartToFirstContentfulPaint,
      (timing.paint_timing->first_contentful_paint.value() -
       timing.parse_timing->parse_start.value())
          .InMilliseconds(),
      1);

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramFromGWSFirstImagePaint, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramFromGWSFirstImagePaint,
      timing.paint_timing->first_image_paint.value().InMilliseconds(), 1);

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramFromGWSDomContentLoaded, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramFromGWSDomContentLoaded,
      timing.document_timing->dom_content_loaded_event_start.value()
          .InMilliseconds(),
      1);

  tester()->histogram_tester().ExpectTotalCount(internal::kHistogramFromGWSLoad,
                                                1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramFromGWSLoad,
      timing.document_timing->load_event_start.value().InMilliseconds(), 1);

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramFromGWSFirstInputDelay, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramFromGWSFirstInputDelay,
      timing.interactive_timing->first_input_delay.value().InMilliseconds(), 1);

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramFromGWSCumulativeLayoutShiftMainFrame, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramFromGWSCumulativeLayoutShiftMainFrame, 25, 1);

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramFromGWSMaxCumulativeShiftScoreSessionWindow, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramFromGWSMaxCumulativeShiftScoreSessionWindow, 5000, 1);

  auto entries = tester()->test_ukm_recorder().GetEntriesByName(
      ukm::builders::PageLoad_FromGoogleSearch::kEntryName);
  EXPECT_EQ(1u, entries.size());
  for (const ukm::mojom::UkmEntry* const entry : entries) {
    tester()->test_ukm_recorder().ExpectEntrySourceHasUrl(entry,
                                                          GURL(kExampleUrl));
  }
}

TEST_F(FromGWSPageLoadMetricsObserverTest, SearchPreviousCommittedUrl2) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.parse_timing->parse_start = base::Milliseconds(1);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.paint_timing->first_image_paint = base::Milliseconds(1);
  PopulateRequiredTimingFields(&timing);
  NavigateAndCommit(GURL("https://www.google.com/#q=test"));
  NavigateAndCommit(GURL(kExampleUrl));

  tester()->SimulateTimingUpdate(timing);

  // Navigate again to force logging.
  tester()->NavigateToUntrackedUrl();
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramFromGWSFirstImagePaint, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramFromGWSFirstImagePaint,
      timing.paint_timing->first_image_paint.value().InMilliseconds(), 1);

  auto entries = tester()->test_ukm_recorder().GetEntriesByName(
      ukm::builders::PageLoad_FromGoogleSearch::kEntryName);
  EXPECT_EQ(1u, entries.size());
  for (const ukm::mojom::UkmEntry* const entry : entries) {
    tester()->test_ukm_recorder().ExpectEntrySourceHasUrl(entry,
                                                          GURL(kExampleUrl));
  }
}

TEST_F(FromGWSPageLoadMetricsObserverTest, SearchPreviousCommittedUrl3) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.parse_timing->parse_start = base::Milliseconds(1);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.paint_timing->first_image_paint = base::Milliseconds(1);
  PopulateRequiredTimingFields(&timing);
  NavigateAndCommit(GURL("https://www.google.com/webhp#q=test"));
  NavigateAndCommit(GURL(kExampleUrl));

  tester()->SimulateTimingUpdate(timing);

  // Navigate again to force logging.
  tester()->NavigateToUntrackedUrl();
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramFromGWSFirstImagePaint, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramFromGWSFirstImagePaint,
      timing.paint_timing->first_image_paint.value().InMilliseconds(), 1);

  auto entries = tester()->test_ukm_recorder().GetEntriesByName(
      ukm::builders::PageLoad_FromGoogleSearch::kEntryName);
  EXPECT_EQ(1u, entries.size());
  for (const ukm::mojom::UkmEntry* const entry : entries) {
    tester()->test_ukm_recorder().ExpectEntrySourceHasUrl(entry,
                                                          GURL(kExampleUrl));
  }
}

TEST_F(FromGWSPageLoadMetricsObserverTest, SearchPreviousCommittedUrl4) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.parse_timing->parse_start = base::Milliseconds(1);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.paint_timing->first_image_paint = base::Milliseconds(1);
  PopulateRequiredTimingFields(&timing);
  NavigateAndCommit(GURL("https://www.google.co.uk/search#q=test"));
  NavigateAndCommit(GURL(kExampleUrl));

  tester()->SimulateTimingUpdate(timing);

  // Navigate again to force logging.
  tester()->NavigateToUntrackedUrl();
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramFromGWSFirstImagePaint, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramFromGWSFirstImagePaint,
      timing.paint_timing->first_image_paint.value().InMilliseconds(), 1);

  auto entries = tester()->test_ukm_recorder().GetEntriesByName(
      ukm::builders::PageLoad_FromGoogleSearch::kEntryName);
  EXPECT_EQ(1u, entries.size());
  for (const ukm::mojom::UkmEntry* const entry : entries) {
    tester()->test_ukm_recorder().ExpectEntrySourceHasUrl(entry,
                                                          GURL(kExampleUrl));
  }
}

TEST_F(FromGWSPageLoadMetricsObserverTest, SearchPreviousCommittedUrlToMaps1) {
  const GURL kMapsUrl("https://www.google.com/maps");

  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.parse_timing->parse_start = base::Milliseconds(1);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.paint_timing->first_image_paint = base::Milliseconds(1);
  PopulateRequiredTimingFields(&timing);
  NavigateAndCommit(GURL("https://www.google.com/#q=test"));
  NavigateAndCommit(kMapsUrl);

  tester()->SimulateTimingUpdate(timing);

  // Navigate again to force logging.
  tester()->NavigateToUntrackedUrl();
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramFromGWSFirstImagePaint, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramFromGWSFirstImagePaint,
      timing.paint_timing->first_image_paint.value().InMilliseconds(), 1);

  auto entries = tester()->test_ukm_recorder().GetEntriesByName(
      ukm::builders::PageLoad_FromGoogleSearch::kEntryName);
  EXPECT_EQ(1u, entries.size());
  for (const ukm::mojom::UkmEntry* const entry : entries) {
    tester()->test_ukm_recorder().ExpectEntrySourceHasUrl(entry,
                                                          GURL(kMapsUrl));
  }
}

TEST_F(FromGWSPageLoadMetricsObserverTest, SearchPreviousCommittedUrlToMaps2) {
  const GURL kMapsUrl(
      "https://www.google.com/maps/@35.6511182,139.7030912,14z?entry=ttu");

  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.parse_timing->parse_start = base::Milliseconds(1);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.paint_timing->first_image_paint = base::Milliseconds(1);
  PopulateRequiredTimingFields(&timing);
  NavigateAndCommit(GURL("https://www.google.com/#q=test"));
  NavigateAndCommit(kMapsUrl);

  tester()->SimulateTimingUpdate(timing);

  // Navigate again to force logging.
  tester()->NavigateToUntrackedUrl();
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramFromGWSFirstImagePaint, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramFromGWSFirstImagePaint,
      timing.paint_timing->first_image_paint.value().InMilliseconds(), 1);

  auto entries = tester()->test_ukm_recorder().GetEntriesByName(
      ukm::builders::PageLoad_FromGoogleSearch::kEntryName);
  EXPECT_EQ(1u, entries.size());
  for (const ukm::mojom::UkmEntry* const entry : entries) {
    tester()->test_ukm_recorder().ExpectEntrySourceHasUrl(entry,
                                                          GURL(kMapsUrl));
  }
}

TEST_F(FromGWSPageLoadMetricsObserverTest, SearchPreviousCommittedUrlToMaps3) {
  const GURL kMapsUrl(
      "https://www.google.co.jp/maps/@35.6511182,139.7030912,14z?entry=ttu");

  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.parse_timing->parse_start = base::Milliseconds(1);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.paint_timing->first_image_paint = base::Milliseconds(1);
  PopulateRequiredTimingFields(&timing);
  NavigateAndCommit(GURL("https://www.google.co.jp/#q=test"));
  NavigateAndCommit(kMapsUrl);

  tester()->SimulateTimingUpdate(timing);

  // Navigate again to force logging.
  tester()->NavigateToUntrackedUrl();
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramFromGWSFirstImagePaint, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramFromGWSFirstImagePaint,
      timing.paint_timing->first_image_paint.value().InMilliseconds(), 1);

  auto entries = tester()->test_ukm_recorder().GetEntriesByName(
      ukm::builders::PageLoad_FromGoogleSearch::kEntryName);
  EXPECT_EQ(1u, entries.size());
  for (const ukm::mojom::UkmEntry* const entry : entries) {
    tester()->test_ukm_recorder().ExpectEntrySourceHasUrl(entry,
                                                          GURL(kMapsUrl));
  }
}

TEST_F(FromGWSPageLoadMetricsObserverTest, GoogleSearchModeLogged) {
  // TODO: rm timing?
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.parse_timing->parse_start = base::Milliseconds(1);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.paint_timing->first_image_paint = base::Milliseconds(1);
  PopulateRequiredTimingFields(&timing);
  NavigateAndCommit(GURL(kGoogleSearchResultsVideoUrl));
  NavigateAndCommit(GURL(kExampleUrl));

  tester()->SimulateTimingUpdate(timing);

  // Navigate again to force logging.
  tester()->NavigateToUntrackedUrl();
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramFromGWSFirstImagePaint, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramFromGWSFirstImagePaint,
      timing.paint_timing->first_image_paint.value().InMilliseconds(), 1);

  auto entries = tester()->test_ukm_recorder().GetEntriesByName(
      ukm::builders::PageLoad_FromGoogleSearch::kEntryName);
  EXPECT_EQ(1u, entries.size());
  for (const ukm::mojom::UkmEntry* const entry : entries) {
    tester()->test_ukm_recorder().ExpectEntryMetric(
        entry, ukm::builders::PageLoad_FromGoogleSearch::kGoogleSearchModeName,
        static_cast<int64_t>(google_util::GoogleSearchMode::kVideos));
    tester()->test_ukm_recorder().ExpectEntrySourceHasUrl(entry,
                                                          GURL(kExampleUrl));
  }
}

TEST_F(FromGWSPageLoadMetricsObserverTest, SearchToNonSearchToOtherPage) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.parse_timing->parse_start = base::Milliseconds(1);
  timing.paint_timing->first_image_paint = base::Milliseconds(1);
  page_load_metrics::mojom::PageLoadTiming timing2;
  page_load_metrics::InitPageLoadTimingForTest(&timing2);
  timing2.navigation_start = base::Time::FromSecondsSinceUnixEpoch(2);
  timing2.paint_timing->first_image_paint = base::Milliseconds(100);
  PopulateRequiredTimingFields(&timing);
  PopulateRequiredTimingFields(&timing2);
  NavigateAndCommit(GURL("https://www.google.co.uk/search#q=test"));
  NavigateAndCommit(GURL(kExampleUrl));
  tester()->SimulateTimingUpdate(timing);
  NavigateAndCommit(GURL("http://www.example.com/other"));
  tester()->SimulateTimingUpdate(timing2);

  // Navigate again to force logging. We expect to log timing for the page
  // navigated from search, but not for the page navigated from that page.
  tester()->NavigateToUntrackedUrl();
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramFromGWSFirstImagePaint, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramFromGWSFirstImagePaint,
      timing.paint_timing->first_image_paint.value().InMilliseconds(), 1);

  auto entries = tester()->test_ukm_recorder().GetEntriesByName(
      ukm::builders::PageLoad_FromGoogleSearch::kEntryName);
  EXPECT_EQ(1u, entries.size());
  for (const ukm::mojom::UkmEntry* const entry : entries) {
    tester()->test_ukm_recorder().ExpectEntrySourceHasUrl(entry,
                                                          GURL(kExampleUrl));
  }
}

TEST_F(FromGWSPageLoadMetricsObserverTest, SearchToNonSearchToSearch) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.parse_timing->parse_start = base::Milliseconds(1);
  timing.paint_timing->first_image_paint = base::Milliseconds(1);
  page_load_metrics::mojom::PageLoadTiming timing2;
  page_load_metrics::InitPageLoadTimingForTest(&timing2);
  timing2.navigation_start = base::Time::FromSecondsSinceUnixEpoch(2);
  timing2.paint_timing->first_image_paint = base::Milliseconds(100);
  PopulateRequiredTimingFields(&timing);
  PopulateRequiredTimingFields(&timing2);
  NavigateAndCommit(GURL("https://www.google.co.uk/search#q=test"));
  NavigateAndCommit(GURL(kExampleUrl));
  tester()->SimulateTimingUpdate(timing);
  NavigateAndCommit(GURL("https://www.google.co.uk/search#q=test"));
  tester()->SimulateTimingUpdate(timing2);

  // Navigate again to force logging. We expect to log timing for the page
  // navigated from search, but not for the search page we navigated to.
  tester()->NavigateToUntrackedUrl();
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramFromGWSFirstImagePaint, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramFromGWSFirstImagePaint,
      timing.paint_timing->first_image_paint.value().InMilliseconds(), 1);

  auto entries = tester()->test_ukm_recorder().GetEntriesByName(
      ukm::builders::PageLoad_FromGoogleSearch::kEntryName);
  EXPECT_EQ(1u, entries.size());
  for (const ukm::mojom::UkmEntry* const entry : entries) {
    tester()->test_ukm_recorder().ExpectEntrySourceHasUrl(entry,
                                                          GURL(kExampleUrl));
  }
}

TEST_F(FromGWSPageLoadMetricsObserverTest,
       SearchToNonSearchToSearchToNonSearch) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.parse_timing->parse_start = base::Milliseconds(1);
  timing.paint_timing->first_image_paint = base::Milliseconds(1);
  page_load_metrics::mojom::PageLoadTiming timing2;
  page_load_metrics::InitPageLoadTimingForTest(&timing2);
  timing2.navigation_start = base::Time::FromSecondsSinceUnixEpoch(2);
  timing2.parse_timing->parse_start = base::Milliseconds(100);
  timing2.paint_timing->first_image_paint = base::Milliseconds(100);
  page_load_metrics::mojom::PageLoadTiming timing3;
  page_load_metrics::InitPageLoadTimingForTest(&timing3);
  timing3.navigation_start = base::Time::FromSecondsSinceUnixEpoch(3);
  timing3.parse_timing->parse_start = base::Milliseconds(1000);
  timing3.paint_timing->first_image_paint = base::Milliseconds(1000);
  PopulateRequiredTimingFields(&timing);
  PopulateRequiredTimingFields(&timing2);
  PopulateRequiredTimingFields(&timing3);
  NavigateAndCommit(GURL("https://www.google.co.uk/search#q=test"));
  NavigateAndCommit(GURL(kExampleUrl));
  tester()->SimulateTimingUpdate(timing);
  NavigateAndCommit(GURL("https://www.google.co.uk/search#q=test"));
  tester()->SimulateTimingUpdate(timing2);
  NavigateAndCommit(GURL(kExampleUrl));
  tester()->SimulateTimingUpdate(timing3);

  // Navigate again to force logging. We expect to log timing for both pages
  // navigated from search, but not for the search pages we navigated to.
  tester()->NavigateToUntrackedUrl();
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramFromGWSFirstImagePaint, 2);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramFromGWSFirstImagePaint,
      timing.paint_timing->first_image_paint.value().InMilliseconds(), 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramFromGWSFirstImagePaint,
      timing3.paint_timing->first_image_paint.value().InMilliseconds(), 1);

  auto entries = tester()->test_ukm_recorder().GetEntriesByName(
      ukm::builders::PageLoad_FromGoogleSearch::kEntryName);
  EXPECT_EQ(2u, entries.size());
  for (const ukm::mojom::UkmEntry* const entry : entries) {
    tester()->test_ukm_recorder().ExpectEntrySourceHasUrl(entry,
                                                          GURL(kExampleUrl));
  }
}

TEST_F(FromGWSPageLoadMetricsObserverTest,
       SearchToNonSearchToSearchToNonSearchBackgrounded) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.parse_timing->parse_start = base::Milliseconds(1);
  timing.paint_timing->first_image_paint = base::Milliseconds(1);
  page_load_metrics::mojom::PageLoadTiming timing2;
  page_load_metrics::InitPageLoadTimingForTest(&timing2);
  timing2.navigation_start = base::Time::FromSecondsSinceUnixEpoch(2);
  timing2.paint_timing->first_image_paint = base::Milliseconds(100);
  page_load_metrics::mojom::PageLoadTiming timing3;
  page_load_metrics::InitPageLoadTimingForTest(&timing3);
  timing3.navigation_start = base::Time::FromSecondsSinceUnixEpoch(3);
  timing3.paint_timing->first_image_paint = base::Milliseconds(1000);
  PopulateRequiredTimingFields(&timing);
  PopulateRequiredTimingFields(&timing2);
  PopulateRequiredTimingFields(&timing3);
  NavigateAndCommit(GURL("https://www.google.co.uk/search#q=test"));
  NavigateAndCommit(GURL(kExampleUrl));
  tester()->SimulateTimingUpdate(timing);
  NavigateAndCommit(GURL("https://www.google.co.uk/search#q=test"));
  web_contents()->WasHidden();
  tester()->SimulateTimingUpdate(timing2);
  NavigateAndCommit(GURL(kExampleUrl));
  tester()->SimulateTimingUpdate(timing3);

  // Navigate again to force logging. We expect to log timing for the first page
  // navigated from search, but not the second since it was backgrounded.
  tester()->NavigateToUntrackedUrl();
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramFromGWSFirstImagePaint, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramFromGWSFirstImagePaint,
      timing.paint_timing->first_image_paint.value().InMilliseconds(), 1);

  auto entries = tester()->test_ukm_recorder().GetEntriesByName(
      ukm::builders::PageLoad_FromGoogleSearch::kEntryName);
  EXPECT_EQ(2u, entries.size());
  for (const ukm::mojom::UkmEntry* const entry : entries) {
    tester()->test_ukm_recorder().ExpectEntrySourceHasUrl(entry,
                                                          GURL(kExampleUrl));
  }
}

TEST_F(FromGWSPageLoadMetricsObserverTest,
       SearchRedirectorPreviousCommittedUrl) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.parse_timing->parse_start = base::Milliseconds(1);
  timing.paint_timing->first_image_paint = base::Milliseconds(1);
  PopulateRequiredTimingFields(&timing);
  NavigateAndCommit(GURL("https://www.google.com/search#q=test"));
  NavigateAndCommit(GURL("https://www.google.com/url?source=web"));
  NavigateAndCommit(GURL(kExampleUrl));

  tester()->SimulateTimingUpdate(timing);

  // Navigate again to force logging.
  tester()->NavigateToUntrackedUrl();
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramFromGWSFirstImagePaint, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramFromGWSFirstImagePaint,
      timing.paint_timing->first_image_paint.value().InMilliseconds(), 1);

  auto entries = tester()->test_ukm_recorder().GetEntriesByName(
      ukm::builders::PageLoad_FromGoogleSearch::kEntryName);
  EXPECT_EQ(1u, entries.size());
  for (const ukm::mojom::UkmEntry* const entry : entries) {
    tester()->test_ukm_recorder().ExpectEntrySourceHasUrl(entry,
                                                          GURL(kExampleUrl));
  }
}

TEST_F(FromGWSPageLoadMetricsObserverTest,
       NonSearchRedirectorPreviousCommittedUrl) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.paint_timing->first_image_paint = base::Milliseconds(1);
  PopulateRequiredTimingFields(&timing);
  NavigateAndCommit(GURL("https://www.google.com/webhp?q=test"));
  NavigateAndCommit(GURL("https://www.google.com/url?a=b&c=d"));
  NavigateAndCommit(GURL(kExampleUrl));

  tester()->SimulateTimingUpdate(timing);

  // Navigate again to force logging.
  tester()->NavigateToUntrackedUrl();
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramFromGWSFirstImagePaint, 0);

  auto entries = tester()->test_ukm_recorder().GetEntriesByName(
      ukm::builders::PageLoad_FromGoogleSearch::kEntryName);
  EXPECT_EQ(0u, entries.size());

  size_t document_created_entries_count =
      tester()->test_ukm_recorder().GetEntriesByName("DocumentCreated").size();
  size_t unload_entries_count =
      tester()->test_ukm_recorder().GetEntriesByName("Unload").size();
  EXPECT_EQ(document_created_entries_count, unload_entries_count);
}

TEST_F(FromGWSPageLoadMetricsObserverTest,
       SearchPreviousCommittedUrlBackgroundLater) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.parse_timing->parse_start = base::Microseconds(1);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.paint_timing->first_image_paint = base::Microseconds(1);
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL("https://www.google.com/search#q=test"));
  NavigateAndCommit(GURL(kExampleUrl));
  page_load_metrics::mojom::FrameRenderDataUpdate render_data(1.0, 1.0, {});
  tester()->SimulateRenderDataUpdate(render_data);

  web_contents()->WasHidden();
  tester()->SimulateTimingUpdate(timing);
  render_data.layout_shift_delta = 1.5;
  render_data.layout_shift_delta_before_input_or_scroll = 0.0;
  tester()->SimulateRenderDataUpdate(render_data);

  // If the system clock is low resolution PageLoadTracker's background_time_
  // may be < timing.first_image_paint.
  if (page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.paint_timing->first_image_paint,
          tester()->GetDelegateForCommittedLoad())) {
    tester()->histogram_tester().ExpectTotalCount(
        internal::kHistogramFromGWSFirstImagePaint, 1);
    tester()->histogram_tester().ExpectBucketCount(
        internal::kHistogramFromGWSFirstImagePaint,
        timing.paint_timing->first_image_paint.value().InMilliseconds(), 1);
  } else {
    tester()->histogram_tester().ExpectTotalCount(
        internal::kHistogramFromGWSFirstImagePaint, 0);
  }

  // Navigate again to force logging layout shift score.
  tester()->NavigateToUntrackedUrl();

  // Layout shift score should still be updated after tab was hidden.
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramFromGWSCumulativeLayoutShiftMainFrame, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramFromGWSCumulativeLayoutShiftMainFrame, 25, 1);
}

// Disabled due to flakiness: https://crbug.com/1092018
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_NewNavigationBeforeCommit DISABLED_NewNavigationBeforeCommit
#else
#define MAYBE_NewNavigationBeforeCommit NewNavigationBeforeCommit
#endif
TEST_F(FromGWSPageLoadMetricsObserverTest, MAYBE_NewNavigationBeforeCommit) {
  NavigateAndCommit(GURL(kGoogleSearchResultsUrl));
  tester()->StartNavigation(GURL("http://example.test"));

  // Simulate the user performing another navigation before commit.
  NavigateAndCommit(GURL("https://www.example.com"));
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramFromGWSAbortNewNavigationBeforeCommit, 1);
}

TEST_F(FromGWSPageLoadMetricsObserverTest, NewNavigationBeforePaint) {
  NavigateAndCommit(GURL(kGoogleSearchResultsUrl));
  NavigateAndCommit(GURL("http://example.test"));
  SimulateTimingWithoutPaint();
  // Simulate the user performing another navigation before paint.
  NavigateAndCommit(GURL("https://www.example.com"));
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramFromGWSAbortNewNavigationBeforePaint, 1);
}

TEST_F(FromGWSPageLoadMetricsObserverTest, StopBeforeCommit) {
  NavigateAndCommit(GURL(kGoogleSearchResultsUrl));
  tester()->StartNavigation(GURL("http://example.test"));
  // Simulate the user pressing the stop button.
  web_contents()->Stop();
  // Now close the tab. This will trigger logging for the prior navigation which
  // was stopped above.
  DeleteContents();
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramFromGWSAbortStopBeforeCommit, 1);
}

TEST_F(FromGWSPageLoadMetricsObserverTest, StopBeforeCommitNonSearch) {
  NavigateAndCommit(GURL("http://google.com"));
  tester()->StartNavigation(GURL("http://example.test"));
  // Simulate the user pressing the stop button.
  web_contents()->Stop();
  // Now close the tab. This will trigger logging for the prior navigation which
  // was stopped above.
  DeleteContents();
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramFromGWSAbortStopBeforeCommit, 0);
}

TEST_F(FromGWSPageLoadMetricsObserverTest, StopBeforeCommitSearchToSearch) {
  NavigateAndCommit(GURL(kGoogleSearchResultsUrl));
  tester()->StartNavigation(GURL("http://www.google.com/webhp?q=5"));
  // Simulate the user pressing the stop button.
  web_contents()->Stop();
  // Now close the tab. This will trigger logging for the prior navigation which
  // was stopped above.
  DeleteContents();
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramFromGWSAbortStopBeforeCommit, 0);
}

TEST_F(FromGWSPageLoadMetricsObserverTest, StopBeforePaint) {
  NavigateAndCommit(GURL(kGoogleSearchResultsUrl));
  NavigateAndCommit(GURL("http://example.test"));
  SimulateTimingWithoutPaint();
  // Simulate the user pressing the stop button.
  web_contents()->Stop();
  // Now close the tab. This will trigger logging for the prior navigation which
  // was stopped above.
  DeleteContents();
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramFromGWSAbortStopBeforePaint, 1);
}

TEST_F(FromGWSPageLoadMetricsObserverTest, StopBeforeCommitAndBeforePaint) {
  // Commit the first navigation.
  NavigateAndCommit(GURL(kGoogleSearchResultsUrl));
  NavigateAndCommit(GURL("https://example.test"));
  SimulateTimingWithoutPaint();
  // Now start a second navigation, but don't commit it.
  tester()->StartNavigation(GURL("https://www.google.com"));
  // Simulate the user pressing the stop button. This should cause us to record
  // stop metrics for the FromGWS committed load, too.
  web_contents()->Stop();
  // Simulate closing the tab.
  DeleteContents();
  // The second navigation was not from GWS.
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramFromGWSAbortStopBeforeCommit, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramFromGWSAbortStopBeforePaint, 1);
}

TEST_F(FromGWSPageLoadMetricsObserverTest, CloseBeforeCommit) {
  NavigateAndCommit(GURL(kGoogleSearchResultsUrl));
  tester()->StartNavigation(GURL("https://example.test"));
  // Simulate closing the tab.
  DeleteContents();
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramFromGWSAbortCloseBeforeCommit, 1);
}

TEST_F(FromGWSPageLoadMetricsObserverTest, CloseBeforePaint) {
  NavigateAndCommit(GURL(kGoogleSearchResultsUrl));
  NavigateAndCommit(GURL("https://example.test"));
  SimulateTimingWithoutPaint();
  // Simulate closing the tab.
  DeleteContents();
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramFromGWSAbortCloseBeforePaint, 1);
}

TEST_F(FromGWSPageLoadMetricsObserverTest,
       AbortCloseBeforeCommitAndBeforePaint) {
  // Commit the first navigation.
  NavigateAndCommit(GURL(kGoogleSearchResultsUrl));
  NavigateAndCommit(GURL("https://example.test"));
  SimulateTimingWithoutPaint();
  // Now start a second navigation, but don't commit it.
  tester()->StartNavigation(GURL("https://example.test2"));
  // Simulate closing the tab.
  DeleteContents();
  // The second navigation was not from GWS.
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramFromGWSAbortCloseBeforeCommit, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramFromGWSAbortCloseBeforePaint, 1);
}

TEST_F(FromGWSPageLoadMetricsObserverTest,
       AbortStopBeforeCommitAndCloseBeforePaint) {
  NavigateAndCommit(GURL(kGoogleSearchResultsUrl));
  tester()->StartNavigation(GURL("https://example.test"));
  // Simulate the user pressing the stop button.
  web_contents()->Stop();
  NavigateAndCommit(GURL("https://example.test2"));
  SimulateTimingWithoutPaint();
  // Simulate closing the tab.
  DeleteContents();
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramFromGWSAbortStopBeforeCommit, 1);
  // The second navigation was from GWS, as GWS was the last committed URL.
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramFromGWSAbortCloseBeforePaint, 1);
}

// TODO(bmcquade, csharrison): add tests for reload, back/forward, and other
// aborts.

TEST_F(FromGWSPageLoadMetricsObserverTest, NoAbortNewNavigationFromAboutURL) {
  NavigateAndCommit(GURL(kGoogleSearchResultsUrl));
  NavigateAndCommit(GURL("about:blank"));
  NavigateAndCommit(GURL("https://www.example.com"));
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramFromGWSAbortNewNavigationBeforePaint, 0);
}

TEST_F(FromGWSPageLoadMetricsObserverTest, NoAbortNewNavigationAfterPaint) {
  NavigateAndCommit(GURL(kGoogleSearchResultsUrl));
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.paint_timing->first_paint = base::Microseconds(1);
  PopulateRequiredTimingFields(&timing);
  NavigateAndCommit(GURL("https://example.test"));
  tester()->SimulateTimingUpdate(timing);

  // The test cannot assume that abort time will be > first_paint
  // (1 micro-sec). If the system clock is low resolution, PageLoadTracker's
  // abort time may be <= first_paint. In that case the histogram will be
  // logged. Thus both 0 and 1 counts of histograms are considered good.

  NavigateAndCommit(GURL("https://example.test2"));

  base::HistogramTester::CountsMap counts_map =
      tester()->histogram_tester().GetTotalCountsForPrefix(
          internal::kHistogramFromGWSAbortNewNavigationBeforePaint);

  EXPECT_TRUE(counts_map.empty() ||
              (counts_map.size() == 1 && counts_map.begin()->second == 1));
}

TEST_F(FromGWSPageLoadMetricsObserverTest, NewNavigationBeforeInteraction) {
  NavigateAndCommit(GURL(kGoogleSearchResultsUrl));
  NavigateAndCommit(GURL("http://example.test"));
  SimulateTimingWithFirstPaint();
  // Simulate the user performing another navigation before paint.
  NavigateAndCommit(GURL("https://www.example.com"));
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramFromGWSAbortNewNavigationBeforeInteraction, 1);
}

TEST_F(FromGWSPageLoadMetricsObserverTest, StopBeforeInteraction) {
  NavigateAndCommit(GURL(kGoogleSearchResultsUrl));
  NavigateAndCommit(GURL("http://example.test"));
  SimulateTimingWithFirstPaint();
  // Simulate the user pressing the stop button.
  web_contents()->Stop();
  // Now close the tab. This will trigger logging for the prior navigation which
  // was stopped above.
  DeleteContents();
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramFromGWSAbortStopBeforeInteraction, 1);
}

TEST_F(FromGWSPageLoadMetricsObserverTest, CloseBeforeInteraction) {
  NavigateAndCommit(GURL(kGoogleSearchResultsUrl));
  NavigateAndCommit(GURL("https://example.test"));
  SimulateTimingWithFirstPaint();
  // Simulate closing the tab.
  DeleteContents();
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramFromGWSAbortCloseBeforeInteraction, 1);
}

TEST_F(FromGWSPageLoadMetricsObserverTest, CloseBeforePaintAndInteraction) {
  NavigateAndCommit(GURL(kGoogleSearchResultsUrl));
  NavigateAndCommit(GURL("https://example.test"));
  SimulateTimingWithoutPaint();
  // Simulate closing the tab.
  DeleteContents();
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramFromGWSAbortCloseBeforeInteraction, 0);
}

TEST_F(FromGWSPageLoadMetricsObserverTest, CloseAfterInteraction) {
  NavigateAndCommit(GURL(kGoogleSearchResultsUrl));
  NavigateAndCommit(GURL("https://example.test"));
  SimulateTimingWithFirstPaint();
  // Simulate user interaction.
  SimulateMouseEvent();
  // Simulate closing the tab.
  DeleteContents();
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramFromGWSAbortCloseBeforeInteraction, 0);
}

TEST_F(FromGWSPageLoadMetricsObserverTest, CommittedIntent) {
  NavigateAndCommit(GURL(kGoogleSearchResultsUrl));
  NavigateAndCommit(GURL("intent://en.m.wikipedia.org/wiki/Test"));
  SimulateTimingWithFirstPaint();
  // Simulate closing the tab.
  DeleteContents();
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramFromGWSAbortCloseBeforeInteraction, 0);
}

TEST_F(FromGWSPageLoadMetricsObserverTest, ProvisionalIntent) {
  NavigateAndCommit(GURL(kGoogleSearchResultsUrl));
  tester()->StartNavigation(GURL("intent://en.m.wikipedia.org/wiki/Test"));
  // Simulate closing the tab.
  DeleteContents();
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramFromGWSAbortCloseBeforeCommit, 0);
}

// Fixture for testing side panel interactions with FromGWS metrics.
class FromGWSPageLoadMetricsObserverWithSidePanelTest
    : public FromGWSPageLoadMetricsObserverTest {
 public:
  // FromGWSPageLoadMetricsObserverTest:
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    FromGWSPageLoadMetricsObserverTest::RegisterObservers(tracker);

    if (simulate_gws_side_panel_navigation_) {
      observer_->SetNavigationStateForSidePanelForTesting(
          initiating_side_panel_url_, navigation_initiated_via_link_);
      simulate_gws_side_panel_navigation_ = false;
    }
  }

  // Call before navigating the web contents to simulate a GWS side panel
  // initiated navigation.
  void SetNextNavigationInitiatedFromSidePanel(
      const GURL& initiating_side_panel_url,
      bool navigation_initiated_via_link) {
    simulate_gws_side_panel_navigation_ = true;
    initiating_side_panel_url_ = initiating_side_panel_url;
    navigation_initiated_via_link_ = navigation_initiated_via_link;
  }

 private:
  bool simulate_gws_side_panel_navigation_ = true;
  GURL initiating_side_panel_url_;
  bool navigation_initiated_via_link_ = false;
};

// Ensures link initiated side panel navigations from Google SRPs report GWS
// metrics, even if the initially committed page in the tab frame is a
// non-Google search page.
TEST_F(FromGWSPageLoadMetricsObserverWithSidePanelTest,
       NonSearchPreviousCommittedUrl) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.parse_timing->parse_start = base::Milliseconds(1);
  timing.paint_timing->first_image_paint = base::Milliseconds(1);
  PopulateRequiredTimingFields(&timing);

  // Navigate and commit to a non-Google page.
  NavigateAndCommit(GURL(kExampleUrl));

  // Simulate a side panel initiated navigation from a Google SRP to a
  // non-Google page.
  SetNextNavigationInitiatedFromSidePanel(GURL(kGoogleSearchResultsUrl), true);
  NavigateAndCommit(GURL(kExampleUrl2));
  tester()->SimulateTimingUpdate(timing);

  // Navigate again to force logging.
  tester()->NavigateToUntrackedUrl();

  // Test UMA metrics recorded successfully.
  const auto first_image_paint =
      timing.paint_timing->first_image_paint.value().InMilliseconds();
  tester()->histogram_tester().ExpectUniqueSample(
      internal::kHistogramFromGWSFirstImagePaint, first_image_paint, 1);
  tester()->histogram_tester().ExpectUniqueSample(
      internal::kHistogramFromGWSFromSidePanelFirstImagePaint,
      first_image_paint, 1);

  // Test UKM metrics recorded successfully.
  auto entries = tester()->test_ukm_recorder().GetEntriesByName(
      ukm::builders::PageLoad_FromGoogleSearch::kEntryName);
  EXPECT_EQ(1u, entries.size());
  size_t document_created_entries_count =
      tester()->test_ukm_recorder().GetEntriesByName("DocumentCreated").size();
  size_t unload_entries_count =
      tester()->test_ukm_recorder().GetEntriesByName("Unload").size();
  EXPECT_EQ(document_created_entries_count, unload_entries_count);
}

// Ensures link initiated side panel navigations from Google SRPs log expected
// regular and side panel specific GWS metrics.
TEST_F(FromGWSPageLoadMetricsObserverWithSidePanelTest,
       RecordsRegularAndSidePanelSpecificMetrics) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.parse_timing->parse_start = base::Milliseconds(10);
  timing.paint_timing->first_paint = base::Milliseconds(20);
  timing.paint_timing->first_contentful_paint = base::Milliseconds(40);
  timing.paint_timing->largest_contentful_paint->largest_text_paint =
      base::Milliseconds(50);
  timing.paint_timing->largest_contentful_paint->largest_text_paint_size = 20u;
  timing.paint_timing->first_image_paint = base::Milliseconds(160);
  timing.parse_timing->parse_stop = base::Milliseconds(320);
  timing.document_timing->dom_content_loaded_event_start =
      base::Milliseconds(640);
  timing.document_timing->load_event_start = base::Milliseconds(1280);
  timing.interactive_timing->first_input_delay = base::Milliseconds(50);
  timing.interactive_timing->first_input_timestamp = base::Milliseconds(1400);
  PopulateRequiredTimingFields(&timing);

  // Navigate and commit to a non-Google page.
  NavigateAndCommit(GURL(kExampleUrl));

  // Simulate a side panel initiated navigation to another non-Google page.
  SetNextNavigationInitiatedFromSidePanel(GURL(kGoogleSearchResultsUrl), true);
  NavigateAndCommit(GURL(kExampleUrl2));
  tester()->SimulateTimingUpdate(timing);

  page_load_metrics::mojom::FrameRenderDataUpdate render_data(1.0, 1.0, {});
  tester()->SimulateRenderDataUpdate(render_data);
  render_data.layout_shift_delta = 1.5;
  render_data.layout_shift_delta_before_input_or_scroll = 0.0;
  render_data.new_layout_shifts.emplace_back(
      page_load_metrics::mojom::LayoutShift::New(base::TimeTicks::Now(), 0.5));
  tester()->SimulateRenderDataUpdate(render_data);

  // Navigate again to force logging.
  tester()->NavigateToUntrackedUrl();

  // NavigationToFirstContentfulPaint.
  const auto first_contentful_paint =
      timing.paint_timing->first_contentful_paint.value().InMilliseconds();
  tester()->histogram_tester().ExpectUniqueSample(
      internal::kHistogramFromGWSFirstContentfulPaint, first_contentful_paint,
      1);
  tester()->histogram_tester().ExpectUniqueSample(
      internal::kHistogramFromGWSFromSidePanelFirstContentfulPaint,
      first_contentful_paint, 1);

  // NavigationToLargestContentfulPaint2.
  const auto largest_contentful_paint =
      timing.paint_timing->largest_contentful_paint->largest_text_paint.value()
          .InMilliseconds();
  tester()->histogram_tester().ExpectUniqueSample(
      internal::kHistogramFromGWSLargestContentfulPaint,
      largest_contentful_paint, 1);
  tester()->histogram_tester().ExpectUniqueSample(
      internal::kHistogramFromGWSFromSidePanelLargestContentfulPaint,
      largest_contentful_paint, 1);

  // NavigationToFirstImagePaint.
  const auto first_image_paint =
      timing.paint_timing->first_image_paint.value().InMilliseconds();
  tester()->histogram_tester().ExpectUniqueSample(
      internal::kHistogramFromGWSFirstImagePaint, first_image_paint, 1);
  tester()->histogram_tester().ExpectUniqueSample(
      internal::kHistogramFromGWSFromSidePanelFirstImagePaint,
      first_image_paint, 1);

  // FirstInputDelay4.
  const auto first_input_delay =
      timing.interactive_timing->first_input_delay.value().InMilliseconds();
  tester()->histogram_tester().ExpectUniqueSample(
      internal::kHistogramFromGWSFirstInputDelay, first_input_delay, 1);
  tester()->histogram_tester().ExpectUniqueSample(
      internal::kHistogramFromGWSFromSidePanelFirstInputDelay,
      first_input_delay, 1);

  // MaxCumulativeShiftScore.SessionWindow.Gap1000ms.Max5000ms2.
  tester()->histogram_tester().ExpectUniqueSample(
      internal::kHistogramFromGWSMaxCumulativeShiftScoreSessionWindow, 5000, 1);
  tester()->histogram_tester().ExpectUniqueSample(
      internal::
          kHistogramFromGWSFromSidePanelMaxCumulativeShiftScoreSessionWindow,
      5000, 1);

  // Test UKM metrics recorded successfully.
  auto entries = tester()->test_ukm_recorder().GetEntriesByName(
      ukm::builders::PageLoad_FromGoogleSearch::kEntryName);
  EXPECT_EQ(1u, entries.size());
  const auto* entry = entries[0].get();
  tester()->test_ukm_recorder().ExpectEntrySourceHasUrl(entry,
                                                        GURL(kExampleUrl2));
}

// If initiated from the side panel hosting a Google SRP ensure we capture the
// search mode of the side panel's Google SRP.
TEST_F(FromGWSPageLoadMetricsObserverWithSidePanelTest,
       GoogleSearchModeLogged) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.parse_timing->parse_start = base::Milliseconds(1);
  timing.paint_timing->first_image_paint = base::Milliseconds(1);
  PopulateRequiredTimingFields(&timing);

  // Navigate and commit to a non-Google page.
  NavigateAndCommit(GURL(kExampleUrl));

  // Simulate a side panel initiated navigation from a Google videos SRP to
  // another non-Google page.
  SetNextNavigationInitiatedFromSidePanel(GURL(kGoogleSearchResultsVideoUrl),
                                          true);
  NavigateAndCommit(GURL(kExampleUrl2));
  tester()->SimulateTimingUpdate(timing);

  // Navigate again to force logging.
  tester()->NavigateToUntrackedUrl();

  // Test UMA metrics recorded successfully.
  const auto first_image_paint =
      timing.paint_timing->first_image_paint.value().InMilliseconds();
  tester()->histogram_tester().ExpectUniqueSample(
      internal::kHistogramFromGWSFirstImagePaint, first_image_paint, 1);
  tester()->histogram_tester().ExpectUniqueSample(
      internal::kHistogramFromGWSFromSidePanelFirstImagePaint,
      first_image_paint, 1);

  // Test that search mode was recorded successfully.
  auto entries = tester()->test_ukm_recorder().GetEntriesByName(
      ukm::builders::PageLoad_FromGoogleSearch::kEntryName);
  EXPECT_EQ(1u, entries.size());
  auto* const entry = entries[0].get();
  tester()->test_ukm_recorder().ExpectEntryMetric(
      entry, ukm::builders::PageLoad_FromGoogleSearch::kGoogleSearchModeName,
      static_cast<int64_t>(google_util::GoogleSearchMode::kVideos));
  tester()->test_ukm_recorder().ExpectEntrySourceHasUrl(entry,
                                                        GURL(kExampleUrl2));
}

// Test that we do not report visible metrics if the tab has been backgrounded.
TEST_F(FromGWSPageLoadMetricsObserverWithSidePanelTest,
       SidePanelNavigationBackgrounded) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.parse_timing->parse_start = base::Milliseconds(1);
  timing.paint_timing->first_image_paint = base::Milliseconds(1);
  PopulateRequiredTimingFields(&timing);

  page_load_metrics::mojom::PageLoadTiming timing2;
  page_load_metrics::InitPageLoadTimingForTest(&timing2);
  timing2.navigation_start = base::Time::FromSecondsSinceUnixEpoch(2);
  timing2.parse_timing->parse_start = base::Milliseconds(100);
  timing2.paint_timing->first_image_paint = base::Milliseconds(100);
  PopulateRequiredTimingFields(&timing2);

  // Navigate and commit to a non-Google page.
  NavigateAndCommit(GURL(kExampleUrl));

  // Simulate a side panel initiated navigation from a Google SRP to a visible
  // non-Google page.
  SetNextNavigationInitiatedFromSidePanel(GURL(kGoogleSearchResultsUrl), true);
  NavigateAndCommit(GURL(kExampleUrl2));
  tester()->SimulateTimingUpdate(timing);

  // Simulate a side panel initiated navigation from a Google SRP to a hidden
  // non-Google page.
  web_contents()->WasHidden();
  SetNextNavigationInitiatedFromSidePanel(GURL(kGoogleSearchResultsVideoUrl),
                                          true);
  NavigateAndCommit(GURL(kExampleUrl));
  tester()->SimulateTimingUpdate(timing);

  // Navigate again to force logging.
  tester()->NavigateToUntrackedUrl();

  // Test to ensure that foreground timings were only logged for the first
  // navigation.
  const auto first_image_paint =
      timing.paint_timing->first_image_paint.value().InMilliseconds();
  tester()->histogram_tester().ExpectUniqueSample(
      internal::kHistogramFromGWSFirstImagePaint, first_image_paint, 1);
  tester()->histogram_tester().ExpectUniqueSample(
      internal::kHistogramFromGWSFromSidePanelFirstImagePaint,
      first_image_paint, 1);

  // UKM metrics should have been logged for both navigations.
  auto entries = tester()->test_ukm_recorder().GetEntriesByName(
      ukm::builders::PageLoad_FromGoogleSearch::kEntryName);
  EXPECT_EQ(2u, entries.size());
  tester()->test_ukm_recorder().ExpectEntrySourceHasUrl(entries[0],
                                                        GURL(kExampleUrl2));
  tester()->test_ukm_recorder().ExpectEntrySourceHasUrl(entries[1],
                                                        GURL(kExampleUrl));
}

// If a side panel initiated navigation is not a Google SRP or is not link
// initiated it should not report GWS metrics.
TEST_F(FromGWSPageLoadMetricsObserverWithSidePanelTest,
       DoesNotRecordNonGWSAndNonLinkInitiatedNavigations) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.parse_timing->parse_start = base::Milliseconds(1);
  timing.paint_timing->first_image_paint = base::Milliseconds(1);
  PopulateRequiredTimingFields(&timing);

  page_load_metrics::mojom::PageLoadTiming timing2;
  page_load_metrics::InitPageLoadTimingForTest(&timing2);
  timing2.navigation_start = base::Time::FromSecondsSinceUnixEpoch(2);
  timing2.parse_timing->parse_start = base::Milliseconds(100);
  timing2.paint_timing->first_image_paint = base::Milliseconds(100);
  PopulateRequiredTimingFields(&timing2);

  // Navigate and commit to a non-Google page.
  NavigateAndCommit(GURL(kExampleUrl));

  // Simulate a side panel initiated navigation from a non-Google page to a
  // non-Google page. Test non-Google side panel pages do not log GWS metrics.
  SetNextNavigationInitiatedFromSidePanel(GURL(kExampleUrl), true);
  NavigateAndCommit(GURL(kExampleUrl2));
  tester()->SimulateTimingUpdate(timing);

  // Navigate again to force logging.
  tester()->NavigateToUntrackedUrl();

  // Test side panel initiated navigations from Google SRPs which are not link
  // initiated do not log GWS metrics.
  SetNextNavigationInitiatedFromSidePanel(GURL(kGoogleSearchResultsUrl), false);
  NavigateAndCommit(GURL(kExampleUrl2));
  tester()->SimulateTimingUpdate(timing2);

  // Navigate again to force logging.
  tester()->NavigateToUntrackedUrl();

  // Test UMA metrics were not recorded.
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramFromGWSFirstImagePaint, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramFromGWSFromSidePanelFirstImagePaint, 0);

  // Test UKM metrics were not recorded.
  auto entries = tester()->test_ukm_recorder().GetEntriesByName(
      ukm::builders::PageLoad_FromGoogleSearch::kEntryName);
  EXPECT_EQ(0u, entries.size());
}

TEST_F(FromGWSPageLoadMetricsLoggerTest, Basic) {
  FromGWSPageLoadMetricsLogger logger;
  ASSERT_FALSE(logger.ShouldLogPostCommitMetrics(GURL(kExampleUrl)));
}

TEST_F(FromGWSPageLoadMetricsLoggerTest, NoPreviousPage) {
  FromGWSPageLoadMetricsLogger logger;
  logger.SetPreviouslyCommittedUrl(GURL());
  logger.set_navigation_initiated_via_link(true);
  ASSERT_FALSE(logger.ShouldLogPostCommitMetrics(GURL(kExampleUrl)));
}

TEST_F(FromGWSPageLoadMetricsLoggerTest, NavigationNotInitiatedViaLink) {
  FromGWSPageLoadMetricsLogger logger;
  logger.SetPreviouslyCommittedUrl(GURL(kGoogleSearchResultsUrl));
  logger.set_navigation_initiated_via_link(false);
  ASSERT_FALSE(logger.ShouldLogPostCommitMetrics(GURL(kExampleUrl)));
}

TEST_F(FromGWSPageLoadMetricsLoggerTest, ProvisionalFromGWS) {
  FromGWSPageLoadMetricsLogger logger;
  logger.SetPreviouslyCommittedUrl(GURL(kGoogleSearchResultsUrl));
  logger.SetProvisionalUrl(GURL(kGoogleSearchResultsUrl));
  ASSERT_FALSE(logger.ShouldLogFailedProvisionalLoadMetrics());
}

TEST_F(FromGWSPageLoadMetricsLoggerTest, ProvisionalNotFromGWS) {
  FromGWSPageLoadMetricsLogger logger;
  logger.SetPreviouslyCommittedUrl(GURL(kGoogleSearchResultsUrl));
  logger.SetProvisionalUrl(GURL(kExampleUrl));
  ASSERT_TRUE(logger.ShouldLogFailedProvisionalLoadMetrics());
}

TEST_F(FromGWSPageLoadMetricsLoggerTest, ProvisionalIgnoredAfterCommit1) {
  FromGWSPageLoadMetricsLogger logger;
  logger.SetPreviouslyCommittedUrl(GURL(kGoogleSearchResultsUrl));
  logger.SetProvisionalUrl(GURL(kExampleUrl));
  logger.set_navigation_initiated_via_link(true);
  ASSERT_FALSE(
      logger.ShouldLogPostCommitMetrics(GURL(kGoogleSearchResultsUrl)));
}

TEST_F(FromGWSPageLoadMetricsLoggerTest, ProvisionalIgnoredAfterCommit2) {
  FromGWSPageLoadMetricsLogger logger;
  logger.SetPreviouslyCommittedUrl(GURL(kGoogleSearchResultsUrl));
  logger.SetProvisionalUrl(GURL(kGoogleSearchResultsUrl));
  logger.set_navigation_initiated_via_link(true);
  ASSERT_TRUE(logger.ShouldLogPostCommitMetrics(GURL(kExampleUrl)));
}

TEST_F(FromGWSPageLoadMetricsLoggerTest, NavigationFromSearch) {
  FromGWSPageLoadMetricsLogger logger;
  logger.SetPreviouslyCommittedUrl(GURL(kGoogleSearchResultsUrl));
  logger.set_navigation_initiated_via_link(true);
  ASSERT_TRUE(logger.ShouldLogPostCommitMetrics(GURL(kExampleUrl)));
}

TEST_F(FromGWSPageLoadMetricsLoggerTest, NavigationToSearchHostname) {
  FromGWSPageLoadMetricsLogger logger;
  logger.SetPreviouslyCommittedUrl(GURL(kGoogleSearchResultsUrl));
  logger.set_navigation_initiated_via_link(true);
  ASSERT_FALSE(
      logger.ShouldLogPostCommitMetrics(GURL("https://www.google.com/about/")));
}

TEST_F(FromGWSPageLoadMetricsLoggerTest, NavigationFromSearchRedirector) {
  FromGWSPageLoadMetricsLogger logger;
  logger.SetPreviouslyCommittedUrl(
      GURL("https://www.google.com/url?source=web"));
  logger.set_navigation_initiated_via_link(true);
  ASSERT_TRUE(logger.ShouldLogPostCommitMetrics(GURL(kExampleUrl)));
}

using FromGWSPageLoadMetricsLoggerWithSidePanelTest = testing::Test;

TEST_F(FromGWSPageLoadMetricsLoggerWithSidePanelTest,
       InitiatedFromSidePanelWithGoogleSRP) {
  FromGWSPageLoadMetricsLogger logger;
  logger.SetNavigationStateForSidePanel(GURL(kGoogleSearchResultsUrl), true);
  EXPECT_TRUE(logger.ShouldLogPostCommitMetrics(GURL(kExampleUrl)));
  EXPECT_TRUE(logger.ShouldLogFailedProvisionalLoadMetrics());
}

TEST_F(FromGWSPageLoadMetricsLoggerWithSidePanelTest,
       InitiatedFromSidePanelWithNonGooglePage) {
  FromGWSPageLoadMetricsLogger logger;
  logger.SetNavigationStateForSidePanel(GURL(kExampleUrl), false);
  EXPECT_FALSE(logger.ShouldLogPostCommitMetrics(GURL(kExampleUrl2)));
  EXPECT_FALSE(logger.ShouldLogFailedProvisionalLoadMetrics());
}

TEST_F(FromGWSPageLoadMetricsLoggerWithSidePanelTest,
       InitiatedFromSidePanelWithGoogleSRPNotLinkInitiated) {
  FromGWSPageLoadMetricsLogger logger;
  logger.SetNavigationStateForSidePanel(GURL(kGoogleSearchResultsUrl), false);
  EXPECT_FALSE(logger.ShouldLogPostCommitMetrics(GURL(kExampleUrl)));
  EXPECT_FALSE(logger.ShouldLogFailedProvisionalLoadMetrics());
}
