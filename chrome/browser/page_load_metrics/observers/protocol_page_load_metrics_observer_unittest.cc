// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/protocol_page_load_metrics_observer.h"

#include <memory>

#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "components/page_load_metrics/browser/protocol_util.h"
#include "components/page_load_metrics/common/test/page_load_metrics_test_util.h"

class ProtocolPageLoadMetricsObserverTest
    : public page_load_metrics::PageLoadMetricsObserverTestHarness {
 protected:
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    std::unique_ptr<ProtocolPageLoadMetricsObserver> observer =
        std::make_unique<ProtocolPageLoadMetricsObserver>();
    observer_ = observer.get();
    tracker->AddObserver(std::move(observer));
  }

  void InitializeTestPageLoadTiming(
      page_load_metrics::mojom::PageLoadTiming* timing) {
    page_load_metrics::InitPageLoadTimingForTest(timing);
    timing->navigation_start = base::Time::FromDoubleT(1);
    timing->parse_timing->parse_start = base::TimeDelta::FromMilliseconds(100);
    timing->paint_timing->first_paint = base::TimeDelta::FromMilliseconds(200);
    timing->paint_timing->first_contentful_paint =
        base::TimeDelta::FromMilliseconds(300);
    timing->paint_timing->first_meaningful_paint =
        base::TimeDelta::FromMilliseconds(400);
    timing->document_timing->dom_content_loaded_event_start =
        base::TimeDelta::FromMilliseconds(600);
    timing->document_timing->load_event_start =
        base::TimeDelta::FromMilliseconds(1000);
    PopulateRequiredTimingFields(timing);
  }

  void SimulateNavigation(
      net::HttpResponseInfo::ConnectionInfo connection_info) {
    NavigateAndCommit(GURL("http://google.com"));

    // Force the ConnectionInfo that the observer received from the
    // NavigationHandle.
    observer_->protocol_ =
        page_load_metrics::GetNetworkProtocol(connection_info);

    page_load_metrics::mojom::PageLoadTiming timing;
    InitializeTestPageLoadTiming(&timing);
    tester()->SimulateTimingUpdate(timing);

    // Navigate again to force OnComplete, which happens when a new navigation
    // occurs.
    NavigateAndCommit(GURL("http://example.com"));
  }

  int CountTotalProtocolMetricsRecorded() {
    int count = 0;

    base::HistogramTester::CountsMap counts_map =
        tester()->histogram_tester().GetTotalCountsForPrefix(
            "PageLoad.Clients.Protocol.");
    for (const auto& entry : counts_map)
      count += entry.second;
    return count;
  }

  void CheckHistograms(int expected_count, const std::string& protocol) {
    EXPECT_EQ(expected_count, CountTotalProtocolMetricsRecorded());
    if (expected_count == 0)
      return;

    std::string prefix = "PageLoad.Clients.Protocol.";
    prefix += protocol;

    tester()->histogram_tester().ExpectTotalCount(
        prefix + ".ParseTiming.NavigationToParseStart", 1);
    tester()->histogram_tester().ExpectTotalCount(
        prefix + ".PaintTiming.ParseStartToFirstContentfulPaint", 1);
    tester()->histogram_tester().ExpectTotalCount(
        prefix + ".PaintTiming.NavigationToFirstContentfulPaint", 1);
    tester()->histogram_tester().ExpectTotalCount(
        prefix + ".Experimental.PaintTiming.ParseStartToFirstMeaningfulPaint",
        1);
    tester()->histogram_tester().ExpectTotalCount(
        prefix + ".Experimental.PaintTiming.NavigationToFirstMeaningfulPaint",
        1);
    tester()->histogram_tester().ExpectTotalCount(
        prefix + ".DocumentTiming.NavigationToDOMContentLoadedEventFired", 1);
    tester()->histogram_tester().ExpectTotalCount(
        prefix + ".DocumentTiming.NavigationToLoadEventFired", 1);
  }

  ProtocolPageLoadMetricsObserver* observer_;
};

TEST_F(ProtocolPageLoadMetricsObserverTest, H11Navigation) {
  SimulateNavigation(net::HttpResponseInfo::CONNECTION_INFO_HTTP1_1);
  CheckHistograms(7, "H11");
}

TEST_F(ProtocolPageLoadMetricsObserverTest, H10Navigation) {
  SimulateNavigation(net::HttpResponseInfo::CONNECTION_INFO_HTTP1_0);
  CheckHistograms(0, "");
}

TEST_F(ProtocolPageLoadMetricsObserverTest, H09Navigation) {
  SimulateNavigation(net::HttpResponseInfo::CONNECTION_INFO_HTTP0_9);
  CheckHistograms(0, "");
}

TEST_F(ProtocolPageLoadMetricsObserverTest, H2Navigation) {
  SimulateNavigation(net::HttpResponseInfo::CONNECTION_INFO_HTTP2);
  CheckHistograms(7, "H2");
}

TEST_F(ProtocolPageLoadMetricsObserverTest, QuicNavigation) {
  SimulateNavigation(net::HttpResponseInfo::CONNECTION_INFO_QUIC_35);
  CheckHistograms(7, "QUIC");
}

TEST_F(ProtocolPageLoadMetricsObserverTest, UnknownNavigation) {
  SimulateNavigation(net::HttpResponseInfo::CONNECTION_INFO_UNKNOWN);
  CheckHistograms(0, "");
}
