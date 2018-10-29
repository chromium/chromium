// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/ukm_page_load_metrics_observer.h"

#include <memory>

#include "base/metrics/metrics_hashes.h"
#include "base/optional.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"
#include "chrome/browser/page_load_metrics/page_load_tracker.h"
#include "chrome/common/page_load_metrics/test/page_load_metrics_test_util.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "net/nqe/effective_connection_type.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "services/network/public/cpp/network_quality_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/metrics_proto/system_profile.pb.h"

using testing::AnyNumber;
using testing::Mock;
using testing::Return;

namespace {

using PageLoad = ukm::builders::PageLoad;

const char kTestUrl1[] = "https://www.google.com/";
const char kTestUrl2[] = "https://www.example.com/";

class MockNetworkQualityProvider : public network::NetworkQualityTracker {
 public:
  MOCK_CONST_METHOD0(GetEffectiveConnectionType,
                     net::EffectiveConnectionType());
  MOCK_CONST_METHOD0(GetHttpRTT, base::TimeDelta());
  MOCK_CONST_METHOD0(GetTransportRTT, base::TimeDelta());
  MOCK_CONST_METHOD0(GetDownstreamThroughputKbps, int32_t());
};

}  // namespace

class UkmPageLoadMetricsObserverTest
    : public page_load_metrics::PageLoadMetricsObserverTestHarness {
 protected:
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    tracker->AddObserver(std::make_unique<UkmPageLoadMetricsObserver>(
        &mock_network_quality_provider_));
  }

  void SetUp() override {
    page_load_metrics::PageLoadMetricsObserverTestHarness::SetUp();

    EXPECT_CALL(mock_network_quality_provider_, GetEffectiveConnectionType())
        .Times(AnyNumber())
        .WillRepeatedly(Return(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN));

    EXPECT_CALL(mock_network_quality_provider_, GetHttpRTT())
        .Times(AnyNumber())
        .WillRepeatedly(Return(base::TimeDelta()));

    EXPECT_CALL(mock_network_quality_provider_, GetTransportRTT())
        .Times(AnyNumber())
        .WillRepeatedly(Return(base::TimeDelta()));

    EXPECT_CALL(mock_network_quality_provider_, GetDownstreamThroughputKbps())
        .Times(AnyNumber())
        .WillRepeatedly(Return(int32_t()));
  }

  MockNetworkQualityProvider& mock_network_quality_provider() {
    return mock_network_quality_provider_;
  }

 private:
  MockNetworkQualityProvider mock_network_quality_provider_;
};

TEST_F(UkmPageLoadMetricsObserverTest, NoMetrics) {
  EXPECT_EQ(0ul, test_ukm_recorder().sources_count());
  EXPECT_EQ(0ul, test_ukm_recorder().entries_count());
}

TEST_F(UkmPageLoadMetricsObserverTest, Basic) {
  // PageLoadTiming with all recorded metrics other than FMP. This allows us to
  // verify both that all metrics are logged, and that we don't log metrics that
  // aren't present in the PageLoadTiming struct. Logging of FMP is verified in
  // the FirstMeaningfulPaint test below.
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.parse_timing->parse_start = base::TimeDelta::FromMilliseconds(100);
  timing.document_timing->dom_content_loaded_event_start =
      base::TimeDelta::FromMilliseconds(200);
  timing.paint_timing->first_paint = base::TimeDelta::FromMilliseconds(250);
  timing.paint_timing->first_contentful_paint =
      base::TimeDelta::FromMilliseconds(300);
  timing.document_timing->load_event_start =
      base::TimeDelta::FromMilliseconds(500);
  timing.input_to_navigation_start = base::TimeDelta::FromMilliseconds(50);
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kTestUrl1));
  SimulateTimingUpdate(timing);

  // Simulate closing the tab.
  DeleteContents();

  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      test_ukm_recorder().GetMergedEntriesByName(PageLoad::kEntryName);
  EXPECT_EQ(1ul, merged_entries.size());

  for (const auto& kv : merged_entries) {
    test_ukm_recorder().ExpectEntrySourceHasUrl(kv.second.get(),
                                                GURL(kTestUrl1));
    test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(), PageLoad::kExperimental_InputToNavigationStartName,
        50);
    test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(), PageLoad::kNavigation_PageTransitionName,
        ui::PAGE_TRANSITION_LINK);
    test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(), PageLoad::kNavigation_PageEndReasonName,
        page_load_metrics::END_CLOSE);
    test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(), PageLoad::kParseTiming_NavigationToParseStartName,
        100);
    test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(),
        PageLoad::kDocumentTiming_NavigationToDOMContentLoadedEventFiredName,
        200);
    test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(), PageLoad::kPaintTiming_NavigationToFirstPaintName,
        250);
    test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(),
        PageLoad::kPaintTiming_NavigationToFirstContentfulPaintName, 300);
    test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(),
        PageLoad::kDocumentTiming_NavigationToLoadEventFiredName, 500);
    test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(), PageLoad::kNet_HttpResponseCodeName, 200);
    EXPECT_FALSE(test_ukm_recorder().EntryHasMetric(
        kv.second.get(),
        PageLoad::
            kExperimental_PaintTiming_NavigationToFirstMeaningfulPaintName));
    EXPECT_TRUE(test_ukm_recorder().EntryHasMetric(
        kv.second.get(), PageLoad::kPageTiming_ForegroundDurationName));
  }
}

TEST_F(UkmPageLoadMetricsObserverTest, FailedProvisionalLoad) {
  EXPECT_CALL(mock_network_quality_provider(), GetEffectiveConnectionType())
      .WillRepeatedly(Return(net::EFFECTIVE_CONNECTION_TYPE_2G));

  // The following simulates a navigation that fails and should commit an error
  // page, but finishes before the error page actually commits.
  GURL url(kTestUrl1);
  std::unique_ptr<content::NavigationSimulator> navigation =
      content::NavigationSimulator::CreateRendererInitiated(url, main_rfh());
  navigation->Fail(net::ERR_TIMED_OUT);
  content::RenderFrameHostTester::For(main_rfh())->SimulateNavigationStop();

  // Simulate closing the tab.
  DeleteContents();

  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      test_ukm_recorder().GetMergedEntriesByName(PageLoad::kEntryName);
  EXPECT_EQ(1ul, merged_entries.size());

  // Make sure that only the following metrics are logged. In particular, no
  // paint/document/etc timing metrics should be logged for failed provisional
  // loads.
  for (const auto& kv : merged_entries) {
    test_ukm_recorder().ExpectEntrySourceHasUrl(kv.second.get(),
                                                GURL(kTestUrl1));
    test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(), PageLoad::kNavigation_PageTransitionName,
        ui::PAGE_TRANSITION_LINK);
    test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(), PageLoad::kNavigation_PageEndReasonName,
        page_load_metrics::END_PROVISIONAL_LOAD_FAILED);
    test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(),
        PageLoad::kNet_EffectiveConnectionType2_OnNavigationStartName,
        metrics::SystemProfileProto::Network::EFFECTIVE_CONNECTION_TYPE_2G);
    test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(), PageLoad::kNet_ErrorCode_OnFailedProvisionalLoadName,
        static_cast<int64_t>(net::ERR_TIMED_OUT) * -1);
    EXPECT_TRUE(test_ukm_recorder().EntryHasMetric(
        kv.second.get(), PageLoad::kPageTiming_ForegroundDurationName));
    EXPECT_TRUE(test_ukm_recorder().EntryHasMetric(
        kv.second.get(),
        PageLoad::kPageTiming_NavigationToFailedProvisionalLoadName));
  }
}

TEST_F(UkmPageLoadMetricsObserverTest, FirstMeaningfulPaint) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.paint_timing->first_meaningful_paint =
      base::TimeDelta::FromMilliseconds(600);
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kTestUrl1));
  SimulateTimingUpdate(timing);

  // Simulate closing the tab.
  DeleteContents();

  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      test_ukm_recorder().GetMergedEntriesByName(PageLoad::kEntryName);
  EXPECT_EQ(1ul, merged_entries.size());

  for (const auto& kv : merged_entries) {
    test_ukm_recorder().ExpectEntrySourceHasUrl(kv.second.get(),
                                                GURL(kTestUrl1));
    test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(),
        PageLoad::
            kExperimental_PaintTiming_NavigationToFirstMeaningfulPaintName,
        600);
    EXPECT_TRUE(test_ukm_recorder().EntryHasMetric(
        kv.second.get(), PageLoad::kPageTiming_ForegroundDurationName));
  }
}

TEST_F(UkmPageLoadMetricsObserverTest, LargestImagePaint) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.paint_timing->largest_image_paint =
      base::TimeDelta::FromMilliseconds(600);
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kTestUrl1));
  SimulateTimingUpdate(timing);

  // Simulate closing the tab.
  DeleteContents();

  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      test_ukm_recorder().GetMergedEntriesByName(PageLoad::kEntryName);
  EXPECT_EQ(1ul, merged_entries.size());

  for (const auto& kv : merged_entries) {
    test_ukm_recorder().ExpectEntrySourceHasUrl(kv.second.get(),
                                                GURL(kTestUrl1));
    test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(),
        PageLoad::kExperimental_PaintTiming_NavigationToLargestImagePaintName,
        600);
    EXPECT_TRUE(test_ukm_recorder().EntryHasMetric(
        kv.second.get(), PageLoad::kPageTiming_ForegroundDurationName));
  }
}

TEST_F(UkmPageLoadMetricsObserverTest,
       LargestImagePaint_DiscardBackgroundResult) {
  std::unique_ptr<base::SimpleTestClock> mock_clock(
      new base::SimpleTestClock());
  mock_clock->SetNow(base::Time::NowFromSystemTime());

  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.paint_timing->largest_image_paint =
      base::TimeDelta::FromMilliseconds(600);
  PopulateRequiredTimingFields(&timing);
  // The duration between nav start and first background set to 1ms.
  mock_clock->Advance(base::TimeDelta::FromMilliseconds(1));
  web_contents()->WasHidden();
  NavigateAndCommit(GURL(kTestUrl1));
  SimulateTimingUpdate(timing);

  // Simulate closing the tab.
  DeleteContents();

  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      test_ukm_recorder().GetMergedEntriesByName(PageLoad::kEntryName);
  EXPECT_EQ(0ul, merged_entries.size());
}

TEST_F(UkmPageLoadMetricsObserverTest, LastImagePaint) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.paint_timing->last_image_paint =
      base::TimeDelta::FromMilliseconds(600);
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kTestUrl1));
  SimulateTimingUpdate(timing);

  // Simulate closing the tab.
  DeleteContents();

  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      test_ukm_recorder().GetMergedEntriesByName(PageLoad::kEntryName);
  EXPECT_EQ(1ul, merged_entries.size());

  for (const auto& kv : merged_entries) {
    test_ukm_recorder().ExpectEntrySourceHasUrl(kv.second.get(),
                                                GURL(kTestUrl1));
    test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(),
        PageLoad::kExperimental_PaintTiming_NavigationToLastImagePaintName,
        600);
    EXPECT_TRUE(test_ukm_recorder().EntryHasMetric(
        kv.second.get(), PageLoad::kPageTiming_ForegroundDurationName));
  }
}

TEST_F(UkmPageLoadMetricsObserverTest, FCPPlusPlus_DiscardBackgroundResult) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);

  PopulateRequiredTimingFields(&timing);

  web_contents()->WasHidden();
  // Set a large enough value to make sure it will be larger than background
  // time, so that the result will be discarded.
  timing.paint_timing->largest_image_paint = base::TimeDelta::FromSeconds(10);
  timing.paint_timing->last_image_paint = base::TimeDelta::FromSeconds(10);
  timing.paint_timing->last_text_paint = base::TimeDelta::FromSeconds(10);
  timing.paint_timing->largest_text_paint = base::TimeDelta::FromSeconds(10);
  NavigateAndCommit(GURL(kTestUrl1));
  SimulateTimingUpdate(timing);

  // Simulate closing the tab.
  DeleteContents();

  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      test_ukm_recorder().GetMergedEntriesByName(PageLoad::kEntryName);
  EXPECT_EQ(0ul, merged_entries.size());
}

TEST_F(UkmPageLoadMetricsObserverTest, FCPPlusPlus_ReportLastCandidate) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);

  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kTestUrl1));
  timing.paint_timing->last_image_paint = base::TimeDelta::FromMilliseconds(60);
  timing.paint_timing->largest_image_paint =
      base::TimeDelta::FromMilliseconds(60);
  timing.paint_timing->last_text_paint = base::TimeDelta::FromMilliseconds(60);
  timing.paint_timing->largest_text_paint =
      base::TimeDelta::FromMilliseconds(60);
  SimulateTimingUpdate(timing);

  timing.paint_timing->last_image_paint =
      base::TimeDelta::FromMilliseconds(600);
  timing.paint_timing->largest_image_paint =
      base::TimeDelta::FromMilliseconds(600);
  timing.paint_timing->last_text_paint = base::TimeDelta::FromMilliseconds(600);
  timing.paint_timing->largest_text_paint =
      base::TimeDelta::FromMilliseconds(600);
  SimulateTimingUpdate(timing);

  // Simulate closing the tab.
  DeleteContents();

  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      test_ukm_recorder().GetMergedEntriesByName(PageLoad::kEntryName);
  EXPECT_EQ(1ul, merged_entries.size());

  for (const auto& kv : merged_entries) {
    test_ukm_recorder().ExpectEntrySourceHasUrl(kv.second.get(),
                                                GURL(kTestUrl1));
    test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(),
        PageLoad::kExperimental_PaintTiming_NavigationToLastImagePaintName,
        600);
    test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(),
        PageLoad::kExperimental_PaintTiming_NavigationToLargestImagePaintName,
        600);
    test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(),
        PageLoad::kExperimental_PaintTiming_NavigationToLastTextPaintName, 600);
    test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(),
        PageLoad::kExperimental_PaintTiming_NavigationToLargestTextPaintName,
        600);
    EXPECT_TRUE(test_ukm_recorder().EntryHasMetric(
        kv.second.get(), PageLoad::kPageTiming_ForegroundDurationName));
  }
}

TEST_F(UkmPageLoadMetricsObserverTest, LargestTextPaint) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.paint_timing->largest_text_paint =
      base::TimeDelta::FromMilliseconds(600);
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kTestUrl1));
  SimulateTimingUpdate(timing);

  // Simulate closing the tab.
  DeleteContents();

  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      test_ukm_recorder().GetMergedEntriesByName(PageLoad::kEntryName);
  EXPECT_EQ(1ul, merged_entries.size());

  for (const auto& kv : merged_entries) {
    test_ukm_recorder().ExpectEntrySourceHasUrl(kv.second.get(),
                                                GURL(kTestUrl1));
    test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(),
        PageLoad::kExperimental_PaintTiming_NavigationToLargestTextPaintName,
        600);
    EXPECT_TRUE(test_ukm_recorder().EntryHasMetric(
        kv.second.get(), PageLoad::kPageTiming_ForegroundDurationName));
  }
}

TEST_F(UkmPageLoadMetricsObserverTest, LastTextPaint) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.paint_timing->last_text_paint = base::TimeDelta::FromMilliseconds(600);
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kTestUrl1));
  SimulateTimingUpdate(timing);

  // Simulate closing the tab.
  DeleteContents();

  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      test_ukm_recorder().GetMergedEntriesByName(PageLoad::kEntryName);
  EXPECT_EQ(1ul, merged_entries.size());

  for (const auto& kv : merged_entries) {
    test_ukm_recorder().ExpectEntrySourceHasUrl(kv.second.get(),
                                                GURL(kTestUrl1));
    test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(),
        PageLoad::kExperimental_PaintTiming_NavigationToLastTextPaintName, 600);
    EXPECT_TRUE(test_ukm_recorder().EntryHasMetric(
        kv.second.get(), PageLoad::kPageTiming_ForegroundDurationName));
  }
}

TEST_F(UkmPageLoadMetricsObserverTest, PageInteractive) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.interactive_timing->interactive =
      base::TimeDelta::FromMilliseconds(600);
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kTestUrl1));
  SimulateTimingUpdate(timing);

  // Simulate closing the tab.
  DeleteContents();

  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      test_ukm_recorder().GetMergedEntriesByName(PageLoad::kEntryName);
  EXPECT_EQ(1ul, merged_entries.size());

  for (const auto& kv : merged_entries) {
    test_ukm_recorder().ExpectEntrySourceHasUrl(kv.second.get(),
                                                GURL(kTestUrl1));
    test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(), PageLoad::kExperimental_NavigationToInteractiveName,
        600);
    EXPECT_TRUE(test_ukm_recorder().EntryHasMetric(
        kv.second.get(), PageLoad::kPageTiming_ForegroundDurationName));
  }
}

TEST_F(UkmPageLoadMetricsObserverTest, PageInteractiveInputInvalidated) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.interactive_timing->interactive =
      base::TimeDelta::FromMilliseconds(1000);
  timing.interactive_timing->first_invalidating_input =
      base::TimeDelta::FromMilliseconds(600);
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kTestUrl1));
  SimulateTimingUpdate(timing);

  // Simulate closing the tab.
  DeleteContents();

  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      test_ukm_recorder().GetMergedEntriesByName(PageLoad::kEntryName);
  EXPECT_EQ(1ul, merged_entries.size());

  for (const auto& kv : merged_entries) {
    test_ukm_recorder().ExpectEntrySourceHasUrl(kv.second.get(),
                                                GURL(kTestUrl1));
    EXPECT_FALSE(test_ukm_recorder().EntryHasMetric(
        kv.second.get(), PageLoad::kExperimental_NavigationToInteractiveName));
    EXPECT_TRUE(test_ukm_recorder().EntryHasMetric(
        kv.second.get(), PageLoad::kPageTiming_ForegroundDurationName));
  }
}

TEST_F(UkmPageLoadMetricsObserverTest, FirstInputDelayAndTimestamp) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.interactive_timing->first_input_delay =
      base::TimeDelta::FromMilliseconds(50);
  timing.interactive_timing->first_input_timestamp =
      base::TimeDelta::FromMilliseconds(712);
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kTestUrl1));
  SimulateTimingUpdate(timing);

  // Simulate closing the tab.
  DeleteContents();

  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      test_ukm_recorder().GetMergedEntriesByName(PageLoad::kEntryName);
  EXPECT_EQ(1ul, merged_entries.size());

  for (const auto& kv : merged_entries) {
    test_ukm_recorder().ExpectEntrySourceHasUrl(kv.second.get(),
                                                GURL(kTestUrl1));
    test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(), PageLoad::kInteractiveTiming_FirstInputDelayName, 50);
    test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(), PageLoad::kInteractiveTiming_FirstInputTimestampName,
        712);
  }
}

TEST_F(UkmPageLoadMetricsObserverTest, LongestInputDelayAndTimestamp) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.interactive_timing->longest_input_delay =
      base::TimeDelta::FromMilliseconds(50);
  timing.interactive_timing->longest_input_timestamp =
      base::TimeDelta::FromMilliseconds(712);
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kTestUrl1));
  SimulateTimingUpdate(timing);

  // Simulate closing the tab.
  DeleteContents();

  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      test_ukm_recorder().GetMergedEntriesByName(PageLoad::kEntryName);
  EXPECT_EQ(1ul, merged_entries.size());

  for (const auto& kv : merged_entries) {
    test_ukm_recorder().ExpectEntrySourceHasUrl(kv.second.get(),
                                                GURL(kTestUrl1));
    test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(), PageLoad::kInteractiveTiming_LongestInputDelayName,
        50);
    test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(), PageLoad::kInteractiveTiming_LongestInputTimestampName,
        712);
  }
}

TEST_F(UkmPageLoadMetricsObserverTest, MultiplePageLoads) {
  page_load_metrics::mojom::PageLoadTiming timing1;
  page_load_metrics::InitPageLoadTimingForTest(&timing1);
  timing1.navigation_start = base::Time::FromDoubleT(1);
  timing1.paint_timing->first_contentful_paint =
      base::TimeDelta::FromMilliseconds(200);
  PopulateRequiredTimingFields(&timing1);

  // Second navigation reports no timing metrics.
  page_load_metrics::mojom::PageLoadTiming timing2;
  page_load_metrics::InitPageLoadTimingForTest(&timing2);
  timing2.navigation_start = base::Time::FromDoubleT(1);
  PopulateRequiredTimingFields(&timing2);

  NavigateAndCommit(GURL(kTestUrl1));
  SimulateTimingUpdate(timing1);

  NavigateAndCommit(GURL(kTestUrl2));
  SimulateTimingUpdate(timing2);

  // Simulate closing the tab.
  DeleteContents();

  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      test_ukm_recorder().GetMergedEntriesByName(PageLoad::kEntryName);
  EXPECT_EQ(2ul, merged_entries.size());
  const ukm::mojom::UkmEntry* entry1 = nullptr;
  const ukm::mojom::UkmEntry* entry2 = nullptr;
  for (const auto& kv : merged_entries) {
    if (test_ukm_recorder().EntryHasMetric(
            kv.second.get(),
            PageLoad::kPaintTiming_NavigationToFirstContentfulPaintName)) {
      entry1 = kv.second.get();
    } else {
      entry2 = kv.second.get();
    }
  }
  ASSERT_NE(entry1, nullptr);
  ASSERT_NE(entry2, nullptr);

  test_ukm_recorder().ExpectEntrySourceHasUrl(entry1, GURL(kTestUrl1));
  test_ukm_recorder().ExpectEntryMetric(entry1,
                                        PageLoad::kNavigation_PageEndReasonName,
                                        page_load_metrics::END_NEW_NAVIGATION);
  test_ukm_recorder().ExpectEntryMetric(
      entry1, PageLoad::kPaintTiming_NavigationToFirstContentfulPaintName, 200);
  EXPECT_FALSE(test_ukm_recorder().EntryHasMetric(
      entry1,
      PageLoad::
          kExperimental_PaintTiming_NavigationToFirstMeaningfulPaintName));
  EXPECT_TRUE(test_ukm_recorder().EntryHasMetric(
      entry1, PageLoad::kPageTiming_ForegroundDurationName));

  test_ukm_recorder().ExpectEntrySourceHasUrl(entry2, GURL(kTestUrl2));
  test_ukm_recorder().ExpectEntryMetric(entry2,
                                        PageLoad::kNavigation_PageEndReasonName,
                                        page_load_metrics::END_CLOSE);
  EXPECT_FALSE(test_ukm_recorder().EntryHasMetric(
      entry2, PageLoad::kParseTiming_NavigationToParseStartName));
  EXPECT_FALSE(test_ukm_recorder().EntryHasMetric(
      entry2, PageLoad::kPaintTiming_NavigationToFirstContentfulPaintName));
  EXPECT_FALSE(test_ukm_recorder().EntryHasMetric(
      entry2,
      PageLoad::
          kExperimental_PaintTiming_NavigationToFirstMeaningfulPaintName));
  EXPECT_TRUE(test_ukm_recorder().EntryHasMetric(
      entry2, PageLoad::kPageTiming_ForegroundDurationName));
}

TEST_F(UkmPageLoadMetricsObserverTest, NetworkQualityEstimates) {
  EXPECT_CALL(mock_network_quality_provider(), GetEffectiveConnectionType())
      .WillRepeatedly(Return(net::EFFECTIVE_CONNECTION_TYPE_3G));
  EXPECT_CALL(mock_network_quality_provider(), GetHttpRTT())
      .WillRepeatedly(Return(base::TimeDelta::FromMilliseconds(100)));
  EXPECT_CALL(mock_network_quality_provider(), GetTransportRTT())
      .WillRepeatedly(Return(base::TimeDelta::FromMilliseconds(200)));
  EXPECT_CALL(mock_network_quality_provider(), GetDownstreamThroughputKbps())
      .WillRepeatedly(Return(300));

  NavigateAndCommit(GURL(kTestUrl1));

  // Simulate closing the tab.
  DeleteContents();

  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      test_ukm_recorder().GetMergedEntriesByName(PageLoad::kEntryName);
  EXPECT_EQ(1ul, merged_entries.size());

  for (const auto& kv : merged_entries) {
    test_ukm_recorder().ExpectEntrySourceHasUrl(kv.second.get(),
                                                GURL(kTestUrl1));
    test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(),
        PageLoad::kNet_EffectiveConnectionType2_OnNavigationStartName,
        metrics::SystemProfileProto::Network::EFFECTIVE_CONNECTION_TYPE_3G);
    test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(), PageLoad::kNet_HttpRttEstimate_OnNavigationStartName,
        100);
    test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(),
        PageLoad::kNet_TransportRttEstimate_OnNavigationStartName, 200);
    test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(),
        PageLoad::kNet_DownstreamKbpsEstimate_OnNavigationStartName, 300);
  }
}

TEST_F(UkmPageLoadMetricsObserverTest, PageTransitionReload) {
  GURL url(kTestUrl1);
  NavigateWithPageTransitionAndCommit(GURL(kTestUrl1),
                                      ui::PAGE_TRANSITION_RELOAD);

  // Simulate closing the tab.
  DeleteContents();

  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      test_ukm_recorder().GetMergedEntriesByName(PageLoad::kEntryName);
  EXPECT_EQ(1ul, merged_entries.size());

  for (const auto& kv : merged_entries) {
    test_ukm_recorder().ExpectEntrySourceHasUrl(kv.second.get(),
                                                GURL(kTestUrl1));
    test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(), PageLoad::kNavigation_PageTransitionName,
        ui::PAGE_TRANSITION_RELOAD);
  }
}

TEST_F(UkmPageLoadMetricsObserverTest, BodySizeMetrics) {
  NavigateAndCommit(GURL(kTestUrl1));

  page_load_metrics::ExtraRequestCompleteInfo resources[] = {
      // Cached request.
      {GURL(kResourceUrl),
       net::HostPortPair(),
       -1 /* frame_tree_node_id */,
       true /* was_cached */,
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
       false /* was_cached */,
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

  // Simulate closing the tab.
  DeleteContents();

  int64_t bucketed_network_bytes =
      ukm::GetExponentialBucketMin(network_bytes, 1.3);
  int64_t bucketed_cache_bytes = ukm::GetExponentialBucketMin(cache_bytes, 1.3);

  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      test_ukm_recorder().GetMergedEntriesByName(PageLoad::kEntryName);
  EXPECT_EQ(1ul, merged_entries.size());

  for (const auto& kv : merged_entries) {
    test_ukm_recorder().ExpectEntrySourceHasUrl(kv.second.get(),
                                                GURL(kTestUrl1));
    test_ukm_recorder().ExpectEntryMetric(kv.second.get(), "Net.NetworkBytes",
                                          bucketed_network_bytes);
    test_ukm_recorder().ExpectEntryMetric(kv.second.get(), "Net.CacheBytes",
                                          bucketed_cache_bytes);
  }
}
