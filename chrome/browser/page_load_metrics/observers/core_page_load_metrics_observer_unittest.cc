// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/core_page_load_metrics_observer.h"

#include <memory>

#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_util.h"
#include "chrome/browser/page_load_metrics/page_load_tracker.h"
#include "chrome/common/page_load_metrics/test/page_load_metrics_test_util.h"
#include "chrome/test/base/testing_browser_process.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/platform/web_mouse_event.h"

namespace {

const char kDefaultTestUrl[] = "https://google.com";
const char kDefaultTestUrlAnchor[] = "https://google.com#samepage";
const char kDefaultTestUrl2[] = "https://whatever.com";

}  // namespace

class CorePageLoadMetricsObserverTest
    : public page_load_metrics::PageLoadMetricsObserverTestHarness {
 protected:
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    tracker->AddObserver(std::make_unique<CorePageLoadMetricsObserver>());
  }

  void SetUp() override {
    page_load_metrics::PageLoadMetricsObserverTestHarness::SetUp();
  }
};

TEST_F(CorePageLoadMetricsObserverTest, NoMetrics) {
  histogram_tester().ExpectTotalCount(internal::kHistogramDomContentLoaded, 0);
  histogram_tester().ExpectTotalCount(internal::kHistogramLoad, 0);
  histogram_tester().ExpectTotalCount(internal::kHistogramFirstLayout, 0);
  histogram_tester().ExpectTotalCount(internal::kHistogramFirstTextPaint, 0);
}

TEST_F(CorePageLoadMetricsObserverTest,
       SameDocumentNoTriggerUntilTrueNavCommit) {
  base::TimeDelta first_layout = base::TimeDelta::FromMilliseconds(1);

  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.document_timing->first_layout = first_layout;
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kDefaultTestUrl));
  SimulateTimingUpdate(timing);

  NavigateAndCommit(GURL(kDefaultTestUrlAnchor));

  NavigateAndCommit(GURL(kDefaultTestUrl2));
  histogram_tester().ExpectTotalCount(internal::kHistogramDomContentLoaded, 0);
  histogram_tester().ExpectTotalCount(internal::kHistogramLoad, 0);
  histogram_tester().ExpectTotalCount(internal::kHistogramFirstLayout, 1);
  histogram_tester().ExpectBucketCount(internal::kHistogramFirstLayout,
                                       first_layout.InMilliseconds(), 1);
  histogram_tester().ExpectTotalCount(internal::kHistogramFirstTextPaint, 0);
}

TEST_F(CorePageLoadMetricsObserverTest, SingleMetricAfterCommit) {
  base::TimeDelta first_layout = base::TimeDelta::FromMilliseconds(1);
  base::TimeDelta parse_start = base::TimeDelta::FromMilliseconds(1);
  base::TimeDelta parse_stop = base::TimeDelta::FromMilliseconds(5);
  base::TimeDelta parse_script_load_duration =
      base::TimeDelta::FromMilliseconds(3);
  base::TimeDelta parse_script_exec_duration =
      base::TimeDelta::FromMilliseconds(1);

  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.document_timing->first_layout = first_layout;
  timing.parse_timing->parse_start = parse_start;
  timing.parse_timing->parse_stop = parse_stop;
  timing.parse_timing->parse_blocked_on_script_load_duration =
      parse_script_load_duration;
  timing.parse_timing->parse_blocked_on_script_execution_duration =
      parse_script_exec_duration;
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kDefaultTestUrl));
  SimulateTimingUpdate(timing);

  // Navigate again to force histogram recording.
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  histogram_tester().ExpectTotalCount(internal::kHistogramDomContentLoaded, 0);
  histogram_tester().ExpectTotalCount(internal::kHistogramLoad, 0);
  histogram_tester().ExpectTotalCount(internal::kHistogramFirstLayout, 1);
  histogram_tester().ExpectBucketCount(internal::kHistogramFirstLayout,
                                       first_layout.InMilliseconds(), 1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramParseDuration,
      (parse_stop - parse_start).InMilliseconds(), 1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramParseBlockedOnScriptLoad,
      parse_script_load_duration.InMilliseconds(), 1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramParseBlockedOnScriptExecution,
      parse_script_exec_duration.InMilliseconds(), 1);
  histogram_tester().ExpectTotalCount(internal::kHistogramFirstTextPaint, 0);

  histogram_tester().ExpectTotalCount(
      internal::kHistogramPageTimingForegroundDuration, 1);
}

TEST_F(CorePageLoadMetricsObserverTest, MultipleMetricsAfterCommits) {
  base::TimeDelta response = base::TimeDelta::FromMilliseconds(1);
  base::TimeDelta first_layout_1 = base::TimeDelta::FromMilliseconds(10);
  base::TimeDelta first_layout_2 = base::TimeDelta::FromMilliseconds(20);
  base::TimeDelta first_text_paint = base::TimeDelta::FromMilliseconds(30);
  base::TimeDelta first_contentful_paint = first_text_paint;
  base::TimeDelta dom_content = base::TimeDelta::FromMilliseconds(40);
  base::TimeDelta load = base::TimeDelta::FromMilliseconds(100);

  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.response_start = response;
  timing.document_timing->first_layout = first_layout_1;
  timing.paint_timing->first_text_paint = first_text_paint;
  timing.paint_timing->first_contentful_paint = first_contentful_paint;
  timing.document_timing->dom_content_loaded_event_start = dom_content;
  timing.document_timing->load_event_start = load;
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kDefaultTestUrl));
  SimulateTimingUpdate(timing);

  histogram_tester().ExpectTotalCount(internal::kHistogramFirstContentfulPaint,
                                      1);
  histogram_tester().ExpectBucketCount(internal::kHistogramFirstContentfulPaint,
                                       first_contentful_paint.InMilliseconds(),
                                       1);

  NavigateAndCommit(GURL(kDefaultTestUrl2));

  page_load_metrics::mojom::PageLoadTiming timing2;
  page_load_metrics::InitPageLoadTimingForTest(&timing2);
  timing2.navigation_start = base::Time::FromDoubleT(200);
  timing2.document_timing->first_layout = first_layout_2;
  PopulateRequiredTimingFields(&timing2);

  SimulateTimingUpdate(timing2);

  NavigateAndCommit(GURL(kDefaultTestUrl));

  histogram_tester().ExpectTotalCount(internal::kHistogramFirstLayout, 2);
  histogram_tester().ExpectBucketCount(internal::kHistogramFirstLayout,
                                       first_layout_1.InMilliseconds(), 1);
  histogram_tester().ExpectBucketCount(internal::kHistogramFirstLayout,
                                       first_layout_2.InMilliseconds(), 1);

  histogram_tester().ExpectTotalCount(internal::kHistogramFirstContentfulPaint,
                                      1);
  histogram_tester().ExpectBucketCount(internal::kHistogramFirstContentfulPaint,
                                       first_contentful_paint.InMilliseconds(),
                                       1);
  histogram_tester().ExpectTotalCount(internal::kHistogramFirstTextPaint, 1);
  histogram_tester().ExpectBucketCount(internal::kHistogramFirstTextPaint,
                                       first_text_paint.InMilliseconds(), 1);

  histogram_tester().ExpectTotalCount(internal::kHistogramDomContentLoaded, 1);
  histogram_tester().ExpectBucketCount(internal::kHistogramDomContentLoaded,
                                       dom_content.InMilliseconds(), 1);

  histogram_tester().ExpectTotalCount(internal::kHistogramLoad, 1);
  histogram_tester().ExpectBucketCount(internal::kHistogramLoad,
                                       load.InMilliseconds(), 1);
}

TEST_F(CorePageLoadMetricsObserverTest, BackgroundDifferentHistogram) {
  base::TimeDelta first_layout = base::TimeDelta::FromSeconds(2);

  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.document_timing->first_layout = first_layout;
  PopulateRequiredTimingFields(&timing);

  // Simulate "Open link in new tab."
  web_contents()->WasHidden();
  NavigateAndCommit(GURL(kDefaultTestUrl));
  SimulateTimingUpdate(timing);

  // Simulate switching to the tab and making another navigation.
  web_contents()->WasShown();

  // Navigate again to force histogram recording.
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  histogram_tester().ExpectTotalCount(
      internal::kBackgroundHistogramDomContentLoaded, 0);
  histogram_tester().ExpectTotalCount(internal::kBackgroundHistogramLoad, 0);
  histogram_tester().ExpectTotalCount(internal::kBackgroundHistogramFirstLayout,
                                      1);
  histogram_tester().ExpectBucketCount(
      internal::kBackgroundHistogramFirstLayout, first_layout.InMilliseconds(),
      1);
  histogram_tester().ExpectTotalCount(
      internal::kBackgroundHistogramFirstTextPaint, 0);

  histogram_tester().ExpectTotalCount(internal::kHistogramDomContentLoaded, 0);
  histogram_tester().ExpectTotalCount(internal::kHistogramLoad, 0);
  histogram_tester().ExpectTotalCount(internal::kHistogramFirstLayout, 0);
  histogram_tester().ExpectTotalCount(internal::kHistogramFirstTextPaint, 0);
}

TEST_F(CorePageLoadMetricsObserverTest, OnlyBackgroundLaterEvents) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.document_timing->dom_content_loaded_event_start =
      base::TimeDelta::FromMicroseconds(1);
  PopulateRequiredTimingFields(&timing);

  // Make sure first_text_paint hasn't been set (wasn't set by
  // PopulateRequiredTimingFields), since we want to defer setting it until
  // after backgrounding.
  ASSERT_FALSE(timing.paint_timing->first_text_paint);

  NavigateAndCommit(GURL(kDefaultTestUrl));
  SimulateTimingUpdate(timing);

  // Background the tab, then foreground it.
  web_contents()->WasHidden();
  web_contents()->WasShown();
  timing.paint_timing->first_text_paint = base::TimeDelta::FromSeconds(4);
  PopulateRequiredTimingFields(&timing);
  SimulateTimingUpdate(timing);

  // If the system clock is low resolution, PageLoadTracker's
  // first_background_time_ may be same as other times such as
  // dom_content_loaded_event_start.
  page_load_metrics::PageLoadExtraInfo info =
      GetPageLoadExtraInfoForCommittedLoad();

  // Navigate again to force histogram recording.
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  if (page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.document_timing->dom_content_loaded_event_start, info)) {
    histogram_tester().ExpectTotalCount(internal::kHistogramDomContentLoaded,
                                        1);
    histogram_tester().ExpectBucketCount(
        internal::kHistogramDomContentLoaded,
        timing.document_timing->dom_content_loaded_event_start.value()
            .InMilliseconds(),
        1);
    histogram_tester().ExpectTotalCount(
        internal::kBackgroundHistogramDomContentLoaded, 0);
  } else {
    histogram_tester().ExpectTotalCount(
        internal::kBackgroundHistogramDomContentLoaded, 1);
    histogram_tester().ExpectTotalCount(internal::kHistogramDomContentLoaded,
                                        0);
  }

  histogram_tester().ExpectTotalCount(internal::kBackgroundHistogramLoad, 0);
  histogram_tester().ExpectTotalCount(
      internal::kBackgroundHistogramFirstTextPaint, 1);
  histogram_tester().ExpectBucketCount(
      internal::kBackgroundHistogramFirstTextPaint,
      timing.paint_timing->first_text_paint.value().InMilliseconds(), 1);

  histogram_tester().ExpectTotalCount(internal::kHistogramLoad, 0);
  histogram_tester().ExpectTotalCount(internal::kHistogramFirstTextPaint, 0);

  histogram_tester().ExpectTotalCount(
      internal::kHistogramPageTimingForegroundDuration, 1);
}

TEST_F(CorePageLoadMetricsObserverTest, DontBackgroundQuickerLoad) {
  // Set this event at 1 microsecond so it occurs before we foreground later in
  // the test.
  base::TimeDelta first_layout = base::TimeDelta::FromMicroseconds(1);

  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.document_timing->first_layout = first_layout;
  PopulateRequiredTimingFields(&timing);

  web_contents()->WasHidden();

  // Open in new tab
  StartNavigation(GURL(kDefaultTestUrl));

  // Switch to the tab
  web_contents()->WasShown();

  // Start another provisional load
  NavigateAndCommit(GURL(kDefaultTestUrl2));
  SimulateTimingUpdate(timing);

  // Navigate again to see if the timing updated for the foregrounded load.
  NavigateAndCommit(GURL(kDefaultTestUrl));

  histogram_tester().ExpectTotalCount(internal::kHistogramDomContentLoaded, 0);
  histogram_tester().ExpectTotalCount(internal::kHistogramLoad, 0);
  histogram_tester().ExpectTotalCount(internal::kHistogramFirstLayout, 1);
  histogram_tester().ExpectBucketCount(internal::kHistogramFirstLayout,
                                       first_layout.InMilliseconds(), 1);
  histogram_tester().ExpectTotalCount(internal::kHistogramFirstTextPaint, 0);
}

TEST_F(CorePageLoadMetricsObserverTest, FailedProvisionalLoad) {
  GURL url(kDefaultTestUrl);
  // The following tests a navigation that fails and should commit an error
  // page, but finishes before the error page commit.
  std::unique_ptr<content::NavigationSimulator> navigation =
      content::NavigationSimulator::CreateRendererInitiated(url, main_rfh());
  navigation->Fail(net::ERR_TIMED_OUT);
  navigation->AbortCommit();
  content::RenderFrameHostTester::For(navigation->GetFinalRenderFrameHost())
      ->SimulateNavigationStop();

  histogram_tester().ExpectTotalCount(internal::kHistogramDomContentLoaded, 0);
  histogram_tester().ExpectTotalCount(internal::kHistogramLoad, 0);
  histogram_tester().ExpectTotalCount(internal::kHistogramFirstLayout, 0);
  histogram_tester().ExpectTotalCount(internal::kHistogramFirstTextPaint, 0);
  histogram_tester().ExpectTotalCount(internal::kHistogramFailedProvisionalLoad,
                                      1);

  histogram_tester().ExpectTotalCount(
      internal::kHistogramPageTimingForegroundDuration, 0);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramPageTimingForegroundDurationNoCommit, 1);
}

TEST_F(CorePageLoadMetricsObserverTest, FailedBackgroundProvisionalLoad) {
  // Test that failed provisional event does not get logged in the
  // histogram if it happened in the background
  GURL url(kDefaultTestUrl);
  web_contents()->WasHidden();
  content::NavigationSimulator::NavigateAndFailFromDocument(
      url, net::ERR_TIMED_OUT, main_rfh());

  histogram_tester().ExpectTotalCount(internal::kHistogramFailedProvisionalLoad,
                                      0);
}

TEST_F(CorePageLoadMetricsObserverTest, Reload) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.parse_timing->parse_start = base::TimeDelta::FromMilliseconds(5);
  timing.paint_timing->first_contentful_paint =
      base::TimeDelta::FromMilliseconds(10);
  PopulateRequiredTimingFields(&timing);

  GURL url(kDefaultTestUrl);
  NavigateWithPageTransitionAndCommit(url, ui::PAGE_TRANSITION_RELOAD);
  SimulateTimingUpdate(timing);

  page_load_metrics::ExtraRequestCompleteInfo resources[] = {
      // Cached request.

      {GURL(kResourceUrl),
       net::HostPortPair(),
       -1 /* frame_tree_node_id */,
       true /*was_cached*/,
       1024 * 20 /* raw_body_bytes */,
       0 /* original_network_content_length */,
       nullptr /* data_reduction_proxy_data */,
       content::ResourceType::RESOURCE_TYPE_SCRIPT,
       0,
       {} /* load_timing_info */},
      // Uncached non-proxied request.
      {GURL(kResourceUrl),
       net::HostPortPair(),
       -1 /* frame_tree_node_id */,
       false /*was_cached*/,
       1024 * 40 /* raw_body_bytes */,
       1024 * 40 /* original_network_content_length */,
       nullptr /* data_reduction_proxy_data */,
       content::ResourceType::RESOURCE_TYPE_SCRIPT,
       0,
       {} /* load_timing_info */},
  };

  int64_t network_bytes = 0;
  int64_t cache_bytes = 0;
  for (const auto& request : resources) {
    SimulateLoadedResource(request);
    if (!request.was_cached) {
      network_bytes += request.raw_body_bytes;
    } else {
      cache_bytes += request.raw_body_bytes;
    }
  }

  NavigateToUntrackedUrl();

  histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeFirstContentfulPaintReload, 1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramLoadTypeFirstContentfulPaintReload,
      timing.paint_timing->first_contentful_paint.value().InMilliseconds(), 1);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeFirstContentfulPaintForwardBack, 0);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeFirstContentfulPaintNewNavigation, 0);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeParseStartReload, 1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramLoadTypeParseStartReload,
      timing.parse_timing->parse_start.value().InMilliseconds(), 1);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeParseStartForwardBack, 0);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeParseStartNewNavigation, 0);

  histogram_tester().ExpectUniqueSample(
      internal::kHistogramLoadTypeNetworkBytesReload,
      static_cast<int>((network_bytes) / 1024), 1);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeNetworkBytesForwardBack, 0);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeNetworkBytesNewNavigation, 0);

  histogram_tester().ExpectUniqueSample(
      internal::kHistogramLoadTypeCacheBytesReload,
      static_cast<int>((cache_bytes) / 1024), 1);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeCacheBytesForwardBack, 0);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeCacheBytesNewNavigation, 0);

  histogram_tester().ExpectUniqueSample(
      internal::kHistogramLoadTypeTotalBytesReload,
      static_cast<int>((network_bytes + cache_bytes) / 1024), 1);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeTotalBytesForwardBack, 0);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeTotalBytesNewNavigation, 0);
}

TEST_F(CorePageLoadMetricsObserverTest, ForwardBack) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.parse_timing->parse_start = base::TimeDelta::FromMilliseconds(5);
  timing.paint_timing->first_contentful_paint =
      base::TimeDelta::FromMilliseconds(10);
  PopulateRequiredTimingFields(&timing);

  GURL url(kDefaultTestUrl);
  // Back navigations to a page that was reloaded report a main transition type
  // of PAGE_TRANSITION_RELOAD with a PAGE_TRANSITION_FORWARD_BACK
  // modifier. This test verifies that when we encounter such a page, we log it
  // as a forward/back navigation.
  NavigateWithPageTransitionAndCommit(
      url, ui::PageTransitionFromInt(ui::PAGE_TRANSITION_RELOAD |
                                     ui::PAGE_TRANSITION_FORWARD_BACK));
  SimulateTimingUpdate(timing);

  page_load_metrics::ExtraRequestCompleteInfo resources[] = {
      // Cached request.
      {GURL(kResourceUrl),
       net::HostPortPair(),
       -1 /* frame_tree_node_id */,
       true /*was_cached*/,
       1024 * 20 /* raw_body_bytes */,
       0 /* original_network_content_length */,
       nullptr /* data_reduction_proxy_data */,
       content::ResourceType::RESOURCE_TYPE_SCRIPT,
       0,
       {} /* load_timing_info */},
      // Uncached non-proxied request.
      {GURL(kResourceUrl),
       net::HostPortPair(),
       -1 /* frame_tree_node_id */,
       false /*was_cached*/,
       1024 * 40 /* raw_body_bytes */,
       1024 * 40 /* original_network_content_length */,
       nullptr /* data_reduction_proxy_data */,
       content::ResourceType::RESOURCE_TYPE_SCRIPT,
       0,
       {} /* load_timing_info */},
  };

  int64_t network_bytes = 0;
  int64_t cache_bytes = 0;
  for (const auto& request : resources) {
    SimulateLoadedResource(request);
    if (!request.was_cached) {
      network_bytes += request.raw_body_bytes;
    } else {
      cache_bytes += request.raw_body_bytes;
    }
  }

  NavigateToUntrackedUrl();

  histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeFirstContentfulPaintReload, 0);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeFirstContentfulPaintForwardBack, 1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramLoadTypeFirstContentfulPaintForwardBack,
      timing.paint_timing->first_contentful_paint.value().InMilliseconds(), 1);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeFirstContentfulPaintNewNavigation, 0);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeParseStartReload, 0);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeParseStartForwardBack, 1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramLoadTypeParseStartForwardBack,
      timing.parse_timing->parse_start.value().InMilliseconds(), 1);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeParseStartNewNavigation, 0);

  histogram_tester().ExpectUniqueSample(
      internal::kHistogramLoadTypeNetworkBytesForwardBack,
      static_cast<int>((network_bytes) / 1024), 1);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeNetworkBytesNewNavigation, 0);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeNetworkBytesReload, 0);

  histogram_tester().ExpectUniqueSample(
      internal::kHistogramLoadTypeCacheBytesForwardBack,
      static_cast<int>((cache_bytes) / 1024), 1);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeCacheBytesNewNavigation, 0);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeCacheBytesReload, 0);

  histogram_tester().ExpectUniqueSample(
      internal::kHistogramLoadTypeTotalBytesForwardBack,
      static_cast<int>((network_bytes + cache_bytes) / 1024), 1);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeTotalBytesNewNavigation, 0);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeTotalBytesReload, 0);
}

TEST_F(CorePageLoadMetricsObserverTest, NewNavigation) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.parse_timing->parse_start = base::TimeDelta::FromMilliseconds(5);
  timing.paint_timing->first_contentful_paint =
      base::TimeDelta::FromMilliseconds(10);
  PopulateRequiredTimingFields(&timing);

  GURL url(kDefaultTestUrl);
  NavigateWithPageTransitionAndCommit(url, ui::PAGE_TRANSITION_LINK);
  SimulateTimingUpdate(timing);

  page_load_metrics::ExtraRequestCompleteInfo resources[] = {
      // Cached request.
      {GURL(kResourceUrl),
       net::HostPortPair(),
       -1 /* frame_tree_node_id */,
       true /*was_cached*/,
       1024 * 20 /* raw_body_bytes */,
       0 /* original_network_content_length */,
       nullptr /* data_reduction_proxy_data */,
       content::ResourceType::RESOURCE_TYPE_SCRIPT,
       0,
       {} /* load_timing_info */},
      // Uncached non-proxied request.
      {GURL(kResourceUrl),
       net::HostPortPair(),
       -1 /* frame_tree_node_id */,
       false /*was_cached*/,
       1024 * 40 /* raw_body_bytes */,
       1024 * 40 /* original_network_content_length */,
       nullptr /* data_reduction_proxy_data */,
       content::ResourceType::RESOURCE_TYPE_SCRIPT,
       0,
       {} /* load_timing_info */},
  };

  int64_t network_bytes = 0;
  int64_t cache_bytes = 0;
  for (const auto& request : resources) {
    SimulateLoadedResource(request);
    if (!request.was_cached) {
      network_bytes += request.raw_body_bytes;
    } else {
      cache_bytes += request.raw_body_bytes;
    }
  }

  NavigateToUntrackedUrl();

  histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeFirstContentfulPaintReload, 0);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeFirstContentfulPaintForwardBack, 0);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeFirstContentfulPaintNewNavigation, 1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramLoadTypeFirstContentfulPaintNewNavigation,
      timing.paint_timing->first_contentful_paint.value().InMilliseconds(), 1);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeParseStartReload, 0);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeParseStartForwardBack, 0);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeParseStartNewNavigation, 1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramLoadTypeParseStartNewNavigation,
      timing.parse_timing->parse_start.value().InMilliseconds(), 1);

  histogram_tester().ExpectUniqueSample(
      internal::kHistogramLoadTypeNetworkBytesNewNavigation,
      static_cast<int>((network_bytes) / 1024), 1);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeNetworkBytesForwardBack, 0);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeNetworkBytesReload, 0);

  histogram_tester().ExpectUniqueSample(
      internal::kHistogramLoadTypeCacheBytesNewNavigation,
      static_cast<int>((cache_bytes) / 1024), 1);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeCacheBytesForwardBack, 0);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeCacheBytesReload, 0);

  histogram_tester().ExpectUniqueSample(
      internal::kHistogramLoadTypeTotalBytesNewNavigation,
      static_cast<int>((network_bytes + cache_bytes) / 1024), 1);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeTotalBytesForwardBack, 0);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeTotalBytesReload, 0);
}

TEST_F(CorePageLoadMetricsObserverTest, BytesAndResourcesCounted) {
  NavigateAndCommit(GURL(kDefaultTestUrl));
  NavigateAndCommit(GURL(kDefaultTestUrl2));
  histogram_tester().ExpectTotalCount(internal::kHistogramPageLoadTotalBytes,
                                      1);
  histogram_tester().ExpectTotalCount(internal::kHistogramPageLoadNetworkBytes,
                                      1);
  histogram_tester().ExpectTotalCount(internal::kHistogramPageLoadCacheBytes,
                                      1);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramTotalCompletedResources, 1);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramNetworkCompletedResources, 1);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramCacheCompletedResources, 1);
}

TEST_F(CorePageLoadMetricsObserverTest, FirstMeaningfulPaint) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.parse_timing->parse_start = base::TimeDelta::FromMilliseconds(5);
  timing.paint_timing->first_meaningful_paint =
      base::TimeDelta::FromMilliseconds(10);
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kDefaultTestUrl));
  SimulateTimingUpdate(timing);
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  histogram_tester().ExpectTotalCount(
      internal::kHistogramFirstMeaningfulPaint, 1);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramParseStartToFirstMeaningfulPaint, 1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramFirstMeaningfulPaintStatus,
      internal::FIRST_MEANINGFUL_PAINT_RECORDED, 1);
}

TEST_F(CorePageLoadMetricsObserverTest, LargestImagePaint) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  // Pick a value that lines up with a histogram bucket.
  timing.paint_timing->largest_image_paint =
      base::TimeDelta::FromMilliseconds(4780);
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kDefaultTestUrl));
  SimulateTimingUpdate(timing);
  // Navigate again to force histogram recording.
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  EXPECT_THAT(
      histogram_tester().GetAllSamples(internal::kHistogramLargestImagePaint),
      testing::ElementsAre(base::Bucket(4780, 1)));
}

TEST_F(CorePageLoadMetricsObserverTest,
       LargestImagePaint_DiscardBackgroundResult) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kDefaultTestUrl));
  web_contents()->WasHidden();
  // This event happens after first background, so it will be discarded.
  timing.paint_timing->largest_image_paint = base::Time::Now() - base::Time();
  SimulateTimingUpdate(timing);
  // Navigate again to force histogram recording.
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  histogram_tester().ExpectTotalCount(internal::kHistogramLargestImagePaint, 0);
}

TEST_F(CorePageLoadMetricsObserverTest, LargestImagePaint_ReportLastCandidate) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);

  NavigateAndCommit(GURL(kDefaultTestUrl));
  timing.navigation_start = base::Time::FromDoubleT(1);

  timing.paint_timing->largest_image_paint =
      base::TimeDelta::FromMilliseconds(1000);
  PopulateRequiredTimingFields(&timing);
  SimulateTimingUpdate(timing);

  timing.paint_timing->largest_image_paint =
      base::TimeDelta::FromMilliseconds(4780);
  PopulateRequiredTimingFields(&timing);
  SimulateTimingUpdate(timing);
  // Navigate again to force histogram recording.
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  EXPECT_THAT(
      histogram_tester().GetAllSamples(internal::kHistogramLargestImagePaint),
      testing::ElementsAre(base::Bucket(4780, 1)));
}

TEST_F(CorePageLoadMetricsObserverTest, LastImagePaint) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  // Pick a value that lines up with a histogram bucket.
  timing.paint_timing->last_image_paint =
      base::TimeDelta::FromMilliseconds(4780);
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kDefaultTestUrl));
  SimulateTimingUpdate(timing);
  // Navigate again to force histogram recording.
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  EXPECT_THAT(
      histogram_tester().GetAllSamples(internal::kHistogramLastImagePaint),
      testing::ElementsAre(base::Bucket(4780, 1)));
}

TEST_F(CorePageLoadMetricsObserverTest,
       LastImagePaint_DiscardBackgroundResult) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kDefaultTestUrl));
  timing.navigation_start = base::Time::FromDoubleT(1);
  web_contents()->WasHidden();
  // Set a large enough value to make sure it will be larger than background
  // time, so that the result will be discarded.
  timing.paint_timing->last_image_paint = base::TimeDelta::FromSeconds(10);
  SimulateTimingUpdate(timing);
  // Navigate again to force histogram recording.
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  histogram_tester().ExpectTotalCount(internal::kHistogramLastImagePaint, 0);
}

TEST_F(CorePageLoadMetricsObserverTest, LastImagePaint_ReportLastCandidate) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);

  NavigateAndCommit(GURL(kDefaultTestUrl));
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.paint_timing->last_image_paint =
      base::TimeDelta::FromMilliseconds(1000);
  PopulateRequiredTimingFields(&timing);
  SimulateTimingUpdate(timing);

  timing.paint_timing->last_image_paint =
      base::TimeDelta::FromMilliseconds(4780);
  PopulateRequiredTimingFields(&timing);
  SimulateTimingUpdate(timing);
  // Navigate again to force histogram recording.
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  EXPECT_THAT(
      histogram_tester().GetAllSamples(internal::kHistogramLastImagePaint),
      testing::ElementsAre(base::Bucket(4780, 1)));
}

TEST_F(CorePageLoadMetricsObserverTest,
       LastImagePaint_ReportLastNullCandidate) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);

  NavigateAndCommit(GURL(kDefaultTestUrl));
  timing.navigation_start = base::Time::FromDoubleT(1);

  timing.paint_timing->last_image_paint =
      base::TimeDelta::FromMilliseconds(1000);
  PopulateRequiredTimingFields(&timing);
  SimulateTimingUpdate(timing);

  timing.paint_timing->last_image_paint = base::Optional<base::TimeDelta>();
  PopulateRequiredTimingFields(&timing);
  SimulateTimingUpdate(timing);
  // Navigate again to force histogram recording.
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  histogram_tester().ExpectTotalCount(internal::kHistogramLastImagePaint, 0);
}

TEST_F(CorePageLoadMetricsObserverTest, LargestTextPaint) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  // Pick a value that lines up with a histogram bucket.
  timing.paint_timing->largest_text_paint =
      base::TimeDelta::FromMilliseconds(4780);
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kDefaultTestUrl));
  SimulateTimingUpdate(timing);
  // Navigate again to force histogram recording.
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  EXPECT_THAT(
      histogram_tester().GetAllSamples(internal::kHistogramLargestTextPaint),
      testing::ElementsAre(base::Bucket(4780, 1)));
}

TEST_F(CorePageLoadMetricsObserverTest, LastTextPaint) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  // Pick a value that lines up with a histogram bucket.
  timing.paint_timing->last_text_paint =
      base::TimeDelta::FromMilliseconds(4780);
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kDefaultTestUrl));
  SimulateTimingUpdate(timing);
  // Navigate again to force histogram recording.
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  EXPECT_THAT(
      histogram_tester().GetAllSamples(internal::kHistogramLastTextPaint),
      testing::ElementsAre(base::Bucket(4780, 1)));
}

TEST_F(CorePageLoadMetricsObserverTest, ForegroundToFirstMeaningfulPaint) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.paint_timing->first_meaningful_paint = base::TimeDelta::FromSeconds(2);
  PopulateRequiredTimingFields(&timing);

  // Simulate "Open link in new tab."
  web_contents()->WasHidden();
  NavigateAndCommit(GURL(kDefaultTestUrl));

  // First Meaningful Paint happens after tab is foregrounded.
  web_contents()->WasShown();
  SimulateTimingUpdate(timing);

  // Navigate again to force histogram recording.
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  histogram_tester().ExpectTotalCount(
      internal::kHistogramForegroundToFirstMeaningfulPaint, 1);
}

TEST_F(CorePageLoadMetricsObserverTest, TimeToInteractiveAlwaysForeground) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.interactive_timing->interactive =
      base::TimeDelta::FromMilliseconds(100);
  timing.interactive_timing->interactive_detection =
      base::TimeDelta::FromMilliseconds(5200);
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kDefaultTestUrl));
  SimulateTimingUpdate(timing);
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  histogram_tester().ExpectTotalCount(internal::kHistogramTimeToInteractive, 1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramTimeToInteractiveStatus,
      internal::TIME_TO_INTERACTIVE_RECORDED, 1);
}

TEST_F(CorePageLoadMetricsObserverTest, TimeToInteractiveStatusBackgrounded) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.interactive_timing->interactive =
      base::TimeDelta::FromMilliseconds(100);
  timing.interactive_timing->interactive_detection =
      base::TimeDelta::FromMilliseconds(5200);
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kDefaultTestUrl));

  // Background the tab, then foreground it.
  web_contents()->WasHidden();
  web_contents()->WasShown();

  SimulateTimingUpdate(timing);

  // Navigate again to force histogram recording.
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  histogram_tester().ExpectTotalCount(internal::kHistogramTimeToInteractive, 0);
  histogram_tester().ExpectUniqueSample(
      internal::kHistogramTimeToInteractiveStatus,
      internal::TIME_TO_INTERACTIVE_BACKGROUNDED, 1);
}

TEST_F(CorePageLoadMetricsObserverTest,
       TimeToInteractiveStatusUserInteractionBeforeInteractive) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.paint_timing->first_meaningful_paint =
      base::TimeDelta::FromMilliseconds(200);
  timing.interactive_timing->first_invalidating_input =
      base::TimeDelta::FromMilliseconds(1000);
  timing.interactive_timing->interactive =
      base::TimeDelta::FromMilliseconds(2000);
  timing.interactive_timing->interactive_detection =
      base::TimeDelta::FromMilliseconds(7100);
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kDefaultTestUrl));
  SimulateTimingUpdate(timing);
  // Navigate again to force histogram recording.
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  histogram_tester().ExpectTotalCount(internal::kHistogramTimeToInteractive, 0);
  histogram_tester().ExpectUniqueSample(
      internal::kHistogramTimeToInteractiveStatus,
      internal::TIME_TO_INTERACTIVE_USER_INTERACTION_BEFORE_INTERACTIVE, 1);
}

TEST_F(CorePageLoadMetricsObserverTest,
       TimeToInteractiveStatusDidNotReachQuiescence) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.paint_timing->first_meaningful_paint =
      base::TimeDelta::FromMilliseconds(200);
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kDefaultTestUrl));
  SimulateTimingUpdate(timing);
  // Navigate again to force histogram recording.
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  histogram_tester().ExpectTotalCount(internal::kHistogramTimeToInteractive, 0);
  histogram_tester().ExpectUniqueSample(
      internal::kHistogramTimeToInteractiveStatus,
      internal::TIME_TO_INTERACTIVE_DID_NOT_REACH_QUIESCENCE, 1);
}

TEST_F(CorePageLoadMetricsObserverTest, TimeToInteractiveStatusDidNotReachFMP) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.paint_timing->first_paint = base::TimeDelta::FromMilliseconds(200);
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kDefaultTestUrl));
  SimulateTimingUpdate(timing);
  // Navigate again to force histogram recording.
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  histogram_tester().ExpectTotalCount(internal::kHistogramTimeToInteractive, 0);
  histogram_tester().ExpectUniqueSample(
      internal::kHistogramTimeToInteractiveStatus,
      internal::TIME_TO_INTERACTIVE_DID_NOT_REACH_FIRST_MEANINGFUL_PAINT, 1);
}

TEST_F(CorePageLoadMetricsObserverTest, FirstInputDelayAndTimestamp) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.interactive_timing->first_input_delay =
      base::TimeDelta::FromMilliseconds(5);
  // Pick a value that lines up with a histogram bucket.
  timing.interactive_timing->first_input_timestamp =
      base::TimeDelta::FromMilliseconds(4780);
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kDefaultTestUrl));
  SimulateTimingUpdate(timing);
  // Navigate again to force histogram recording.
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  EXPECT_THAT(
      histogram_tester().GetAllSamples(internal::kHistogramFirstInputDelay),
      testing::ElementsAre(base::Bucket(5, 1)));
  EXPECT_THAT(
      histogram_tester().GetAllSamples(internal::kHistogramFirstInputTimestamp),
      testing::ElementsAre(base::Bucket(4780, 1)));
}

TEST_F(CorePageLoadMetricsObserverTest, LongestInputDelayAndTimestamp) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.interactive_timing->longest_input_delay =
      base::TimeDelta::FromMilliseconds(5);
  // Pick a value that lines up with a histogram bucket.
  timing.interactive_timing->longest_input_timestamp =
      base::TimeDelta::FromMilliseconds(4780);
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kDefaultTestUrl));
  SimulateTimingUpdate(timing);
  // Navigate again to force histogram recording.
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  EXPECT_THAT(
      histogram_tester().GetAllSamples(internal::kHistogramLongestInputDelay),
      testing::ElementsAre(base::Bucket(5, 1)));
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  internal::kHistogramLongestInputTimestamp),
              testing::ElementsAre(base::Bucket(4780, 1)));
}

TEST_F(CorePageLoadMetricsObserverTest,
       FirstInputDelayAndTimestampBackgrounded) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.interactive_timing->first_input_delay =
      base::TimeDelta::FromMilliseconds(5);
  timing.interactive_timing->first_input_timestamp =
      base::TimeDelta::FromMilliseconds(5000);
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kDefaultTestUrl));

  // Background the tab, then foreground it.
  web_contents()->WasHidden();
  web_contents()->WasShown();

  SimulateTimingUpdate(timing);
  // Navigate again to force histogram recording.
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  histogram_tester().ExpectTotalCount(internal::kHistogramFirstInputDelay, 0);
  histogram_tester().ExpectTotalCount(internal::kHistogramFirstInputTimestamp,
                                      0);
}

TEST_F(CorePageLoadMetricsObserverTest, NavigationToBackNavigationWithGesture) {
  GURL url(kDefaultTestUrl);

  // Navigate once to the page with a user gesture.
  auto simulator =
      content::NavigationSimulator::CreateRendererInitiated(url, main_rfh());
  simulator->SetHasUserGesture(true);
  simulator->Commit();

  // Now the user presses the back button.
  NavigateWithPageTransitionAndCommit(
      url, ui::PageTransitionFromInt(ui::PAGE_TRANSITION_FORWARD_BACK));

  histogram_tester().ExpectTotalCount(
      internal::kHistogramUserGestureNavigationToForwardBack, 1);
}

TEST_F(CorePageLoadMetricsObserverTest,
       BrowserNavigationToBackNavigationWithGesture) {
  GURL url(kDefaultTestUrl);

  // Navigate once to the page with a user gesture.
  auto simulator =
      content::NavigationSimulator::CreateBrowserInitiated(url, web_contents());
  simulator->SetHasUserGesture(true);
  simulator->Commit();

  // Now the user presses the back button.
  NavigateWithPageTransitionAndCommit(
      url, ui::PageTransitionFromInt(ui::PAGE_TRANSITION_FORWARD_BACK));

  histogram_tester().ExpectTotalCount(
      internal::kHistogramUserGestureNavigationToForwardBack, 0);
}

TEST_F(CorePageLoadMetricsObserverTest,
       NavigationToBackNavigationWithoutGesture) {
  GURL url(kDefaultTestUrl);

  // Navigate once to the page with a user gesture.
  auto simulator =
      content::NavigationSimulator::CreateRendererInitiated(url, main_rfh());
  simulator->SetHasUserGesture(false);
  simulator->Commit();

  // Now the user presses the back button.
  NavigateWithPageTransitionAndCommit(
      url, ui::PageTransitionFromInt(ui::PAGE_TRANSITION_FORWARD_BACK));

  histogram_tester().ExpectTotalCount(
      internal::kHistogramUserGestureNavigationToForwardBack, 0);
}

TEST_F(CorePageLoadMetricsObserverTest,
       AbortedNavigationToBackNavigationWithGesture) {
  GURL url(kDefaultTestUrl);

  // Navigate once to the page with a user gesture.
  auto simulator =
      content::NavigationSimulator::CreateRendererInitiated(url, main_rfh());
  simulator->SetHasUserGesture(true);
  simulator->Start();

  // Now the user presses the back button before the first navigation committed.
  NavigateWithPageTransitionAndCommit(
      url, ui::PageTransitionFromInt(ui::PAGE_TRANSITION_FORWARD_BACK));

  histogram_tester().ExpectTotalCount(
      internal::kHistogramUserGestureNavigationToForwardBack, 1);
}
