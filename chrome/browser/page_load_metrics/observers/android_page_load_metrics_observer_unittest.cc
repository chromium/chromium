// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/android_page_load_metrics_observer.h"

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"
#include "chrome/browser/profiles/profile.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "components/page_load_metrics/common/test/page_load_metrics_test_util.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "net/base/ip_endpoint.h"
#include "services/network/public/cpp/network_quality_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::AnyNumber;
using testing::Return;

class MockNetworkQualityTracker : public network::NetworkQualityTracker {
 public:
  MOCK_CONST_METHOD0(GetEffectiveConnectionType,
                     net::EffectiveConnectionType());
  MOCK_CONST_METHOD0(GetHttpRTT, base::TimeDelta());
  MOCK_CONST_METHOD0(GetTransportRTT, base::TimeDelta());
  MOCK_CONST_METHOD0(GetDownstreamThroughputKbps, int32_t());
};

class TestAndroidPageLoadMetricsObserver
    : public AndroidPageLoadMetricsObserver {
 public:
  TestAndroidPageLoadMetricsObserver(
      network::NetworkQualityTracker* network_quality_tracker)
      : AndroidPageLoadMetricsObserver(network_quality_tracker) {}

  net::EffectiveConnectionType reported_connection_type() const {
    return reported_connection_type_;
  }

  int64_t reported_http_rtt_ms() const { return reported_http_rtt_ms_; }

  int64_t reported_transport_rtt_ms() const {
    return reported_transport_rtt_ms_;
  }

  int64_t reported_first_contentful_paint_ms() const {
    return reported_first_contentful_paint_ms_;
  }

  base::TimeTicks reported_navigation_start_tick_fcp() const {
    return reported_navigation_start_tick_fcp_;
  }

  base::TimeTicks reported_navigation_start_tick_load() const {
    return reported_navigation_start_tick_load_;
  }

  int64_t reported_load_event_start_ms() const {
    return reported_load_event_start_ms_;
  }

  int64_t reported_dns_start_ms() const { return reported_dns_start_ms_; }

 protected:
  void ReportNetworkQualityEstimate(
      net::EffectiveConnectionType connection_type,
      int64_t http_rtt_ms,
      int64_t transport_rtt_ms) override {
    reported_connection_type_ = connection_type;
    reported_http_rtt_ms_ = http_rtt_ms;
    reported_transport_rtt_ms_ = transport_rtt_ms;
  }

  void ReportFirstContentfulPaint(
      base::TimeTicks navigation_start_tick,
      base::TimeDelta first_contentful_paint) override {
    reported_navigation_start_tick_fcp_ = navigation_start_tick;
    reported_first_contentful_paint_ms_ =
        first_contentful_paint.InMilliseconds();
  }

  void ReportLoadEventStart(base::TimeTicks navigation_start_tick,
                            base::TimeDelta load_event_start) override {
    reported_navigation_start_tick_load_ = navigation_start_tick;
    reported_load_event_start_ms_ = load_event_start.InMilliseconds();
  }

  void ReportLoadedMainResource(int64_t dns_start_ms,
                                int64_t dns_end_ms,
                                int64_t connect_start_ms,
                                int64_t connect_end_ms,
                                int64_t request_start_ms,
                                int64_t send_start_ms,
                                int64_t send_end_ms) override {
    reported_dns_start_ms_ = dns_start_ms;
  }

 private:
  net::EffectiveConnectionType reported_connection_type_ =
      net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN;
  int64_t reported_http_rtt_ms_ = 0;
  int64_t reported_transport_rtt_ms_ = 0;
  int64_t reported_first_contentful_paint_ms_ = 0;
  base::TimeTicks reported_navigation_start_tick_fcp_;
  base::TimeTicks reported_navigation_start_tick_load_;
  int64_t reported_load_event_start_ms_ = 0;
  int64_t reported_dns_start_ms_ = 0;
};

class AndroidPageLoadMetricsObserverTest
    : public page_load_metrics::PageLoadMetricsObserverTestHarness {
 public:
  AndroidPageLoadMetricsObserverTest() {}

  void SetUp() override {
    PageLoadMetricsObserverTestHarness::SetUp();
    // Save observer_ptr_ so we can query for test results, while the
    // PageLoadTracker owns it.
    observer_ptr_ =
        new TestAndroidPageLoadMetricsObserver(&mock_network_quality_tracker_);
    observer_ = base::WrapUnique<page_load_metrics::PageLoadMetricsObserver>(
        observer_ptr_.get());
  }

  TestAndroidPageLoadMetricsObserver* observer() const { return observer_ptr_; }

  base::TimeTicks GetNavigationStart() const { return navigation_start_; }

  void SetNetworkQualityMock() {
    EXPECT_CALL(mock_network_quality_tracker(), GetEffectiveConnectionType())
        .Times(AnyNumber())
        .WillRepeatedly(Return(net::EFFECTIVE_CONNECTION_TYPE_3G));
    EXPECT_CALL(mock_network_quality_tracker(), GetHttpRTT())
        .Times(AnyNumber())
        .WillRepeatedly(Return(base::Milliseconds(3)));
    EXPECT_CALL(mock_network_quality_tracker(), GetTransportRTT())
        .Times(AnyNumber())
        .WillRepeatedly(Return(base::Milliseconds(4)));
  }

  MockNetworkQualityTracker& mock_network_quality_tracker() {
    return mock_network_quality_tracker_;
  }

  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    navigation_start_ = tracker->navigation_start();
    tracker->AddObserver(std::move(observer_));
  }

 private:
  std::unique_ptr<page_load_metrics::PageLoadMetricsObserver> observer_;
  raw_ptr<TestAndroidPageLoadMetricsObserver> observer_ptr_;
  MockNetworkQualityTracker mock_network_quality_tracker_;
  base::TimeTicks navigation_start_;
};

TEST_F(AndroidPageLoadMetricsObserverTest, NetworkQualityEstimate) {
  SetNetworkQualityMock();
  NavigateAndCommit(GURL("https://www.example.com"));
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_3G,
            observer()->reported_connection_type());
  EXPECT_EQ(3L, observer()->reported_http_rtt_ms());
  EXPECT_EQ(4L, observer()->reported_transport_rtt_ms());
}

TEST_F(AndroidPageLoadMetricsObserverTest, MissingNetworkQualityEstimate) {
  EXPECT_CALL(mock_network_quality_tracker(), GetEffectiveConnectionType())
      .Times(AnyNumber())
      .WillRepeatedly(Return(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN));
  EXPECT_CALL(mock_network_quality_tracker(), GetHttpRTT())
      .Times(AnyNumber())
      .WillRepeatedly(Return(base::TimeDelta()));
  EXPECT_CALL(mock_network_quality_tracker(), GetTransportRTT())
      .Times(AnyNumber())
      .WillRepeatedly(Return(base::TimeDelta()));
  NavigateAndCommit(GURL("https://www.example.com"));
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN,
            observer()->reported_connection_type());
  EXPECT_EQ(0L, observer()->reported_http_rtt_ms());
  EXPECT_EQ(0L, observer()->reported_transport_rtt_ms());
}

TEST_F(AndroidPageLoadMetricsObserverTest, LoadTimingInfo) {
  SetNetworkQualityMock();
  auto navigation_simulator =
      content::NavigationSimulator::CreateRendererInitiated(
          GURL("https://www.example.com"),
          web_contents()->GetPrimaryMainFrame());
  navigation_simulator->Start();
  content::FrameTreeNodeId frame_tree_node_id =
      navigation_simulator->GetNavigationHandle()->GetFrameTreeNodeId();
  navigation_simulator->Commit();

  auto load_timing_info = std::make_unique<net::LoadTimingInfo>();
  const base::TimeTicks kNow = base::TimeTicks::Now();
  load_timing_info->connect_timing.domain_lookup_start = kNow;
  page_load_metrics::ExtraRequestCompleteInfo info(
      url::SchemeHostPort(GURL("https://ignored.com")), net::IPEndPoint(),
      frame_tree_node_id, false, /* cached */
      10 * 1024 /* size */, 0 /* original_network_content_length */,
      network::mojom::RequestDestination::kDocument, 0,
      std::move(load_timing_info));
  tester()->SimulateLoadedResource(info,
                                   navigation_simulator->GetGlobalRequestID());
  EXPECT_EQ(kNow.since_origin().InMilliseconds(),
            observer()->reported_dns_start_ms());
}

TEST_F(AndroidPageLoadMetricsObserverTest, LoadEvents) {
  SetNetworkQualityMock();
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  // Note this navigation start does not effect the start that is reported to
  // us.
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.document_timing->load_event_start = base::Milliseconds(30);
  timing.parse_timing->parse_start = base::Milliseconds(20);
  timing.paint_timing->first_contentful_paint = base::Milliseconds(20);
  PopulateRequiredTimingFields(&timing);
  NavigateAndCommit(GURL("https://www.example.com"));
  tester()->SimulateTimingUpdate(timing);
  EXPECT_EQ(30, observer()->reported_load_event_start_ms());
  EXPECT_EQ(GetNavigationStart(),
            observer()->reported_navigation_start_tick_load());
  EXPECT_EQ(20, observer()->reported_first_contentful_paint_ms());
  EXPECT_EQ(GetNavigationStart(),
            observer()->reported_navigation_start_tick_fcp());
}
