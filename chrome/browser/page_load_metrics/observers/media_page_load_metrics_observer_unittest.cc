// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/media_page_load_metrics_observer.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "components/page_load_metrics/common/page_load_metrics.mojom.h"
#include "components/page_load_metrics/common/page_load_timing.h"
#include "components/page_load_metrics/common/test/page_load_metrics_test_util.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "third_party/blink/public/common/loader/loading_behavior_flag.h"
#include "url/gurl.h"

namespace {

const char kDefaultTestUrl[] = "https://google.com";

}  // namespace

class MediaPageLoadMetricsObserverTest
    : public page_load_metrics::PageLoadMetricsObserverTestHarness {
 public:
  MediaPageLoadMetricsObserverTest() {}

  MediaPageLoadMetricsObserverTest(const MediaPageLoadMetricsObserverTest&) =
      delete;
  MediaPageLoadMetricsObserverTest& operator=(
      const MediaPageLoadMetricsObserverTest&) = delete;

  ~MediaPageLoadMetricsObserverTest() override = default;

  void ResetTest() {
    page_load_metrics::InitPageLoadTimingForTest(&timing_);
    // Reset to the default testing state. Does not reset histogram state.
    timing_.navigation_start = base::Time::FromDoubleT(1);
    timing_.response_start = base::Seconds(2);
    timing_.parse_timing->parse_start = base::Seconds(3);
    timing_.paint_timing->first_contentful_paint = base::Seconds(4);
    timing_.paint_timing->first_image_paint = base::Seconds(5);
    timing_.document_timing->load_event_start = base::Seconds(7);
    PopulateRequiredTimingFields(&timing_);

    network_bytes_ = 0;
    cache_bytes_ = 0;
  }

  void SimulateEvents(content::RenderFrameHost* rfh,
                      bool simulate_play_media,
                      bool simulate_app_background) {
    if (simulate_play_media)
      tester()->SimulateMediaPlayed(rfh);

    tester()->SimulateTimingUpdate(timing_);

    auto resources =
        GetSampleResourceDataUpdateForTesting(10 * 1024 /* resource_size */);
    tester()->SimulateResourceDataUseUpdate(resources);
    for (const auto& resource : resources) {
      if (resource->is_complete) {
        if (resource->cache_type ==
            page_load_metrics::mojom::CacheType::kNotCached)
          network_bytes_ += resource->encoded_body_length;
        else
          cache_bytes_ += resource->encoded_body_length;
      }
    }

    if (simulate_app_background) {
      // The histograms should be logged when the app is backgrounded.
      tester()->SimulateAppEnterBackground();
    } else {
      tester()->NavigateToUntrackedUrl();
    }
  }

 protected:
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    tracker->AddObserver(std::make_unique<MediaPageLoadMetricsObserver>());
  }

  // Simulated byte usage since the last time the test was reset.
  int64_t network_bytes_;
  int64_t cache_bytes_;

 private:
  page_load_metrics::mojom::PageLoadTiming timing_;
};

TEST_F(MediaPageLoadMetricsObserverTest, MediaNotPlayed) {
  ResetTest();

  NavigateAndCommit(GURL(kDefaultTestUrl));
  content::RenderFrameHost* mainframe = web_contents()->GetPrimaryMainFrame();

  SimulateEvents(mainframe, false /* simulate_play_media */,
                 false /* simulate_app_background */);

  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.MediaPageLoad2.Experimental.Bytes.Network", 0);
  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.MediaPageLoad2.Experimental.Bytes.Cache2", 0);
  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.MediaPageLoad2.Experimental.Bytes.Total2", 0);
}

TEST_F(MediaPageLoadMetricsObserverTest, MediaPlayed) {
  ResetTest();

  NavigateAndCommit(GURL(kDefaultTestUrl));
  content::RenderFrameHost* mainframe = web_contents()->GetPrimaryMainFrame();

  SimulateEvents(mainframe, true /* simulate_play_media */,
                 false /* simulate_app_background */);

  tester()->histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.MediaPageLoad2.Experimental.Bytes.Network",
      static_cast<int>(network_bytes_ / 1024), 1);
  tester()->histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.MediaPageLoad2.Experimental.Bytes.Cache2",
      static_cast<int>(cache_bytes_ / 1024), 1);
  tester()->histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.MediaPageLoad2.Experimental.Bytes.Total2",
      static_cast<int>((network_bytes_ + cache_bytes_) / 1024), 1);
}

TEST_F(MediaPageLoadMetricsObserverTest, MediaPlayedAppBackground) {
  ResetTest();

  NavigateAndCommit(GURL(kDefaultTestUrl));
  content::RenderFrameHost* mainframe = web_contents()->GetPrimaryMainFrame();

  SimulateEvents(mainframe, true /* simulate_play_media */,
                 true /* simulate_app_background */);

  tester()->histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.MediaPageLoad2.Experimental.Bytes.Network",
      static_cast<int>(network_bytes_ / 1024), 1);
  tester()->histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.MediaPageLoad2.Experimental.Bytes.Cache2",
      static_cast<int>(cache_bytes_ / 1024), 1);
  tester()->histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.MediaPageLoad2.Experimental.Bytes.Total2",
      static_cast<int>((network_bytes_ + cache_bytes_) / 1024), 1);
}

TEST_F(MediaPageLoadMetricsObserverTest, MediaPlayedInSubframe) {
  ResetTest();

  NavigateAndCommit(GURL(kDefaultTestUrl));
  content::RenderFrameHost* mainframe = web_contents()->GetPrimaryMainFrame();
  content::RenderFrameHost* subframe =
      content::RenderFrameHostTester::For(mainframe)->AppendChild("subframe");
  std::unique_ptr<content::NavigationSimulator> simulator =
      content::NavigationSimulator::CreateRendererInitiated(
          GURL(kDefaultTestUrl), subframe);
  simulator->Commit();

  SimulateEvents(subframe, true /* simulate_play_media */,
                 false /* simulate_app_background */);

  tester()->histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.MediaPageLoad2.Experimental.Bytes.Network",
      static_cast<int>(network_bytes_ / 1024), 1);
  tester()->histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.MediaPageLoad2.Experimental.Bytes.Cache2",
      static_cast<int>(cache_bytes_ / 1024), 1);
  tester()->histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.MediaPageLoad2.Experimental.Bytes.Total2",
      static_cast<int>((network_bytes_ + cache_bytes_) / 1024), 1);
}

TEST_F(MediaPageLoadMetricsObserverTest, MediaPlayedInFencedFrame) {
  ResetTest();

  NavigateAndCommit(GURL(kDefaultTestUrl));
  content::RenderFrameHost* mainframe = web_contents()->GetPrimaryMainFrame();
  content::RenderFrameHost* subframe =
      content::RenderFrameHostTester::For(mainframe)->AppendFencedFrame();
  std::unique_ptr<content::NavigationSimulator> simulator =
      content::NavigationSimulator::CreateRendererInitiated(
          GURL(kDefaultTestUrl), subframe);
  simulator->Commit();

  SimulateEvents(subframe, true /* simulate_play_media */,
                 false /* simulate_app_background */);

  tester()->histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.MediaPageLoad2.Experimental.Bytes.Network",
      static_cast<int>(network_bytes_ / 1024), 1);
  tester()->histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.MediaPageLoad2.Experimental.Bytes.Cache2",
      static_cast<int>(cache_bytes_ / 1024), 1);
  tester()->histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.MediaPageLoad2.Experimental.Bytes.Total2",
      static_cast<int>((network_bytes_ + cache_bytes_) / 1024), 1);
}
