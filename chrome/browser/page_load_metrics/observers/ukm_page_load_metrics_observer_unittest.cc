// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/ukm_page_load_metrics_observer.h"

#include <memory>

#include "base/metrics/metrics_hashes.h"
#include "base/optional.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/trace_event_analyzer.h"
#include "base/time/time.h"
#include "base/trace_event/traced_value.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/features.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "components/page_load_metrics/common/test/page_load_metrics_test_util.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "net/base/ip_endpoint.h"
#include "net/nqe/effective_connection_type.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "services/network/public/cpp/network_quality_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/metrics_proto/system_profile.pb.h"

using content::NavigationSimulator;
using content::RenderFrameHost;
using content::RenderFrameHostTester;
using testing::AnyNumber;
using testing::Mock;
using testing::Return;

namespace {

using PageLoad = ukm::builders::PageLoad;

const char kTestUrl1[] = "https://www.google.com/";
const char kTestUrl2[] = "https://www.example.com/";
const char kSubframeTestUrl[] = "https://www.google.com/subframe.html";

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
    page_load_metrics::LargestContentfulPaintHandler::SetTestMode(true);

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
  EXPECT_EQ(0ul, tester()->test_ukm_recorder().sources_count());
  EXPECT_EQ(0ul, tester()->test_ukm_recorder().entries_count());
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
  tester()->SimulateTimingUpdate(timing);

  // Simulate closing the tab.
  DeleteContents();

  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      tester()->test_ukm_recorder().GetMergedEntriesByName(
          PageLoad::kEntryName);
  EXPECT_EQ(1ul, merged_entries.size());

  for (const auto& kv : merged_entries) {
    tester()->test_ukm_recorder().ExpectEntrySourceHasUrl(kv.second.get(),
                                                          GURL(kTestUrl1));
    tester()->test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(), PageLoad::kExperimental_InputToNavigationStartName,
        50);
    tester()->test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(), PageLoad::kExperimental_Navigation_UserInitiatedName,
        true);
    tester()->test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(), PageLoad::kNavigation_PageTransitionName,
        ui::PAGE_TRANSITION_LINK);
    tester()->test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(), PageLoad::kNavigation_PageEndReasonName,
        page_load_metrics::END_CLOSE);
    tester()->test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(), PageLoad::kParseTiming_NavigationToParseStartName,
        100);
    tester()->test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(),
        PageLoad::kDocumentTiming_NavigationToDOMContentLoadedEventFiredName,
        200);
    tester()->test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(), PageLoad::kPaintTiming_NavigationToFirstPaintName,
        250);
    tester()->test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(),
        PageLoad::kPaintTiming_NavigationToFirstContentfulPaintName, 300);
    tester()->test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(),
        PageLoad::kDocumentTiming_NavigationToLoadEventFiredName, 500);
    tester()->test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(), PageLoad::kNet_HttpResponseCodeName, 200);
    EXPECT_FALSE(tester()->test_ukm_recorder().EntryHasMetric(
        kv.second.get(),
        PageLoad::
            kExperimental_PaintTiming_NavigationToFirstMeaningfulPaintName));
    EXPECT_TRUE(tester()->test_ukm_recorder().EntryHasMetric(
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
  navigation->AbortCommit();

  // Simulate closing the tab.
  DeleteContents();

  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      tester()->test_ukm_recorder().GetMergedEntriesByName(
          PageLoad::kEntryName);
  EXPECT_EQ(1ul, merged_entries.size());

  // Make sure that only the following metrics are logged. In particular, no
  // paint/document/etc timing metrics should be logged for failed provisional
  // loads.
  for (const auto& kv : merged_entries) {
    tester()->test_ukm_recorder().ExpectEntrySourceHasUrl(kv.second.get(),
                                                          GURL(kTestUrl1));
    tester()->test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(), PageLoad::kNavigation_PageTransitionName,
        ui::PAGE_TRANSITION_LINK);
    tester()->test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(), PageLoad::kNavigation_PageEndReasonName,
        page_load_metrics::END_PROVISIONAL_LOAD_FAILED);
    tester()->test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(),
        PageLoad::kNet_EffectiveConnectionType2_OnNavigationStartName,
        metrics::SystemProfileProto::Network::EFFECTIVE_CONNECTION_TYPE_2G);
    tester()->test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(), PageLoad::kNet_ErrorCode_OnFailedProvisionalLoadName,
        static_cast<int64_t>(net::ERR_TIMED_OUT) * -1);
    EXPECT_TRUE(tester()->test_ukm_recorder().EntryHasMetric(
        kv.second.get(), PageLoad::kPageTiming_ForegroundDurationName));
    EXPECT_TRUE(tester()->test_ukm_recorder().EntryHasMetric(
        kv.second.get(),
        PageLoad::kPageTiming_NavigationToFailedProvisionalLoadName));
    tester()->test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(), PageLoad::kExperimental_Navigation_UserInitiatedName,
        false);
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
  tester()->SimulateTimingUpdate(timing);

  // Simulate closing the tab.
  DeleteContents();

  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      tester()->test_ukm_recorder().GetMergedEntriesByName(
          PageLoad::kEntryName);
  EXPECT_EQ(1ul, merged_entries.size());

  for (const auto& kv : merged_entries) {
    tester()->test_ukm_recorder().ExpectEntrySourceHasUrl(kv.second.get(),
                                                          GURL(kTestUrl1));
    tester()->test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(),
        PageLoad::
            kExperimental_PaintTiming_NavigationToFirstMeaningfulPaintName,
        600);
    EXPECT_TRUE(tester()->test_ukm_recorder().EntryHasMetric(
        kv.second.get(), PageLoad::kPageTiming_ForegroundDurationName));
  }
}

TEST_F(UkmPageLoadMetricsObserverTest, LargestImagePaint) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.paint_timing->largest_image_paint =
      base::TimeDelta::FromMilliseconds(600);
  timing.paint_timing->largest_image_paint_size = 50u;
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kTestUrl1));
  tester()->SimulateTimingUpdate(timing);

  // Simulate closing the tab.
  DeleteContents();

  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      tester()->test_ukm_recorder().GetMergedEntriesByName(
          PageLoad::kEntryName);
  EXPECT_EQ(1ul, merged_entries.size());

  for (const auto& kv : merged_entries) {
    tester()->test_ukm_recorder().ExpectEntrySourceHasUrl(kv.second.get(),
                                                          GURL(kTestUrl1));
    tester()->test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(),
        PageLoad::kExperimental_PaintTiming_NavigationToLargestImagePaintName,
        600);
    EXPECT_TRUE(tester()->test_ukm_recorder().EntryHasMetric(
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
  tester()->SimulateTimingUpdate(timing);

  // Simulate closing the tab.
  DeleteContents();

  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      tester()->test_ukm_recorder().GetMergedEntriesByName(
          PageLoad::kEntryName);
  for (const auto& kv : merged_entries) {
    EXPECT_FALSE(tester()->test_ukm_recorder().EntryHasMetric(
        kv.second.get(),
        PageLoad::kExperimental_PaintTiming_NavigationToLargestImagePaintName));
  }
}

TEST_F(UkmPageLoadMetricsObserverTest, FCPPlusPlus_DiscardBackgroundResult) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);

  PopulateRequiredTimingFields(&timing);

  // Backgrounding. The backgrounding time will be recorded. This time is very
  // short but indefinite, so we should make sure we set a large enough time for
  // the timings so that they are larger than the backgrounding time.
  web_contents()->WasHidden();
  // Set a large enough value to make sure it will be larger than backgrounding
  // time, so that the result will be discarded.
  timing.paint_timing->largest_image_paint = base::TimeDelta::FromSeconds(10);
  timing.paint_timing->largest_text_paint = base::TimeDelta::FromSeconds(10);
  NavigateAndCommit(GURL(kTestUrl1));
  tester()->SimulateTimingUpdate(timing);

  // Simulate closing the tab.
  DeleteContents();

  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      tester()->test_ukm_recorder().GetMergedEntriesByName(
          PageLoad::kEntryName);
  for (const auto& kv : merged_entries) {
    EXPECT_FALSE(tester()->test_ukm_recorder().EntryHasMetric(
        kv.second.get(),
        PageLoad::kExperimental_PaintTiming_NavigationToLargestImagePaintName));
    EXPECT_FALSE(tester()->test_ukm_recorder().EntryHasMetric(
        kv.second.get(),
        PageLoad::kExperimental_PaintTiming_NavigationToLargestTextPaintName));
  }
}

TEST_F(UkmPageLoadMetricsObserverTest, FCPPlusPlus_ReportLastCandidate) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);

  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kTestUrl1));
  timing.paint_timing->largest_image_paint =
      base::TimeDelta::FromMilliseconds(60);
  timing.paint_timing->largest_image_paint_size = 10u;
  timing.paint_timing->largest_text_paint =
      base::TimeDelta::FromMilliseconds(60);
  timing.paint_timing->largest_text_paint_size = 10u;
  tester()->SimulateTimingUpdate(timing);

  timing.paint_timing->largest_image_paint =
      base::TimeDelta::FromMilliseconds(600);
  timing.paint_timing->largest_image_paint_size = 10u;
  timing.paint_timing->largest_text_paint =
      base::TimeDelta::FromMilliseconds(600);
  timing.paint_timing->largest_text_paint_size = 10u;
  tester()->SimulateTimingUpdate(timing);

  // Simulate closing the tab.
  DeleteContents();

  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      tester()->test_ukm_recorder().GetMergedEntriesByName(
          PageLoad::kEntryName);
  EXPECT_EQ(1ul, merged_entries.size());

  for (const auto& kv : merged_entries) {
    tester()->test_ukm_recorder().ExpectEntrySourceHasUrl(kv.second.get(),
                                                          GURL(kTestUrl1));
    tester()->test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(),
        PageLoad::kExperimental_PaintTiming_NavigationToLargestImagePaintName,
        600);
    tester()->test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(),
        PageLoad::kExperimental_PaintTiming_NavigationToLargestTextPaintName,
        600);
    EXPECT_TRUE(tester()->test_ukm_recorder().EntryHasMetric(
        kv.second.get(), PageLoad::kPageTiming_ForegroundDurationName));
  }
}

TEST_F(UkmPageLoadMetricsObserverTest, LargestTextPaint) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.paint_timing->largest_text_paint =
      base::TimeDelta::FromMilliseconds(600);
  timing.paint_timing->largest_text_paint_size = 50u;
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kTestUrl1));
  tester()->SimulateTimingUpdate(timing);

  // Simulate closing the tab.
  DeleteContents();

  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      tester()->test_ukm_recorder().GetMergedEntriesByName(
          PageLoad::kEntryName);
  EXPECT_EQ(1ul, merged_entries.size());

  for (const auto& kv : merged_entries) {
    tester()->test_ukm_recorder().ExpectEntrySourceHasUrl(kv.second.get(),
                                                          GURL(kTestUrl1));
    tester()->test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(),
        PageLoad::kExperimental_PaintTiming_NavigationToLargestTextPaintName,
        600);
    EXPECT_TRUE(tester()->test_ukm_recorder().EntryHasMetric(
        kv.second.get(), PageLoad::kPageTiming_ForegroundDurationName));
  }
}

TEST_F(UkmPageLoadMetricsObserverTest, LargestContentfulPaint_Trace) {
  using trace_analyzer::Query;
  trace_analyzer::Start("*");
  {
    page_load_metrics::mojom::PageLoadTiming timing;
    page_load_metrics::InitPageLoadTimingForTest(&timing);
    timing.navigation_start = base::Time::FromDoubleT(1);
    timing.paint_timing->largest_text_paint =
        base::TimeDelta::FromMilliseconds(600);
    timing.paint_timing->largest_text_paint_size = 1000;
    PopulateRequiredTimingFields(&timing);

    NavigateAndCommit(GURL(kTestUrl1));
    tester()->SimulateTimingUpdate(timing);

    // Simulate closing the tab.
    DeleteContents();
  }
  auto analyzer = trace_analyzer::Stop();
  trace_analyzer::TraceEventVector events;
  Query q = Query::EventNameIs(
      "NavStartToLargestContentfulPaint::Candidate::AllFrames::UKM");
  analyzer->FindEvents(q, &events);
  EXPECT_EQ(1u, events.size());
  EXPECT_EQ("loading", events[0]->category);
  EXPECT_TRUE(events[0]->HasArg("data"));
  std::unique_ptr<base::Value> arg;
  EXPECT_TRUE(events[0]->GetArgAsValue("data", &arg));
  base::DictionaryValue* arg_dict;
  EXPECT_TRUE(arg->GetAsDictionary(&arg_dict));
  int time;
  EXPECT_TRUE(arg_dict->GetInteger("durationInMilliseconds", &time));
  EXPECT_EQ(600, time);
  int size;
  EXPECT_TRUE(arg_dict->GetInteger("size", &size));
  EXPECT_EQ(1000, size);
  std::string type;
  EXPECT_TRUE(arg_dict->GetString("type", &type));
  EXPECT_EQ("text", type);
}

TEST_F(UkmPageLoadMetricsObserverTest,
       LargestContentfulPaint_Trace_InvalidateCandidate) {
  using trace_analyzer::Query;
  trace_analyzer::Start("loading");
  {
    page_load_metrics::mojom::PageLoadTiming timing;
    page_load_metrics::InitPageLoadTimingForTest(&timing);
    timing.navigation_start = base::Time::FromDoubleT(1);
    timing.paint_timing->largest_text_paint =
        base::TimeDelta::FromMilliseconds(600);
    timing.paint_timing->largest_text_paint_size = 1000;
    PopulateRequiredTimingFields(&timing);

    NavigateAndCommit(GURL(kTestUrl1));
    tester()->SimulateTimingUpdate(timing);

    timing.paint_timing->largest_text_paint = base::Optional<base::TimeDelta>();
    timing.paint_timing->largest_text_paint_size = 0;
    PopulateRequiredTimingFields(&timing);

    tester()->SimulateTimingUpdate(timing);

    // Simulate closing the tab.
    DeleteContents();
  }
  auto analyzer = trace_analyzer::Stop();

  trace_analyzer::TraceEventVector candidate_events;
  Query candidate_query = Query::EventNameIs(
      "NavStartToLargestContentfulPaint::Candidate::AllFrames::UKM");
  analyzer->FindEvents(candidate_query, &candidate_events);
  EXPECT_EQ(1u, candidate_events.size());

  trace_analyzer::TraceEventVector invalidate_events;
  Query invalidate_query = Query::EventNameIs(
      "NavStartToLargestContentfulPaint::"
      "Invalidate::AllFrames::UKM");
  analyzer->FindEvents(invalidate_query, &invalidate_events);
  EXPECT_EQ(1u, invalidate_events.size());
  EXPECT_EQ("loading", invalidate_events[0]->category);
}

TEST_F(UkmPageLoadMetricsObserverTest, LargestContentfulPaint_OnlyText) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.paint_timing->largest_text_paint =
      base::TimeDelta::FromMilliseconds(600);
  timing.paint_timing->largest_text_paint_size = 1000;
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kTestUrl1));
  tester()->SimulateTimingUpdate(timing);

  // Simulate closing the tab.
  DeleteContents();

  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      tester()->test_ukm_recorder().GetMergedEntriesByName(
          PageLoad::kEntryName);
  EXPECT_EQ(1ul, merged_entries.size());

  for (const auto& kv : merged_entries) {
    tester()->test_ukm_recorder().ExpectEntrySourceHasUrl(kv.second.get(),
                                                          GURL(kTestUrl1));
    tester()->test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(),
        PageLoad::kPaintTiming_NavigationToLargestContentfulPaintName, 600);
    EXPECT_TRUE(tester()->test_ukm_recorder().EntryHasMetric(
        kv.second.get(), PageLoad::kPageTiming_ForegroundDurationName));
  }
}

TEST_F(UkmPageLoadMetricsObserverTest, LargestContentfulPaint_OnlyImage) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.paint_timing->largest_image_paint =
      base::TimeDelta::FromMilliseconds(600);
  timing.paint_timing->largest_image_paint_size = 1000;
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kTestUrl1));
  tester()->SimulateTimingUpdate(timing);

  // Simulate closing the tab.
  DeleteContents();

  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      tester()->test_ukm_recorder().GetMergedEntriesByName(
          PageLoad::kEntryName);
  EXPECT_EQ(1ul, merged_entries.size());

  for (const auto& kv : merged_entries) {
    tester()->test_ukm_recorder().ExpectEntrySourceHasUrl(kv.second.get(),
                                                          GURL(kTestUrl1));
    tester()->test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(),
        PageLoad::kPaintTiming_NavigationToLargestContentfulPaintName, 600);
    EXPECT_TRUE(tester()->test_ukm_recorder().EntryHasMetric(
        kv.second.get(), PageLoad::kPageTiming_ForegroundDurationName));
  }
}

TEST_F(UkmPageLoadMetricsObserverTest,
       LargestContentfulPaint_ImageLargerThanText) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.paint_timing->largest_image_paint =
      base::TimeDelta::FromMilliseconds(600);
  timing.paint_timing->largest_image_paint_size = 1000;
  timing.paint_timing->largest_text_paint =
      base::TimeDelta::FromMilliseconds(1000);
  timing.paint_timing->largest_text_paint_size = 500;
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kTestUrl1));
  tester()->SimulateTimingUpdate(timing);

  // Simulate closing the tab.
  DeleteContents();

  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      tester()->test_ukm_recorder().GetMergedEntriesByName(
          PageLoad::kEntryName);
  EXPECT_EQ(1ul, merged_entries.size());

  for (const auto& kv : merged_entries) {
    tester()->test_ukm_recorder().ExpectEntrySourceHasUrl(kv.second.get(),
                                                          GURL(kTestUrl1));
    tester()->test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(),
        PageLoad::kPaintTiming_NavigationToLargestContentfulPaintName, 600);
    EXPECT_TRUE(tester()->test_ukm_recorder().EntryHasMetric(
        kv.second.get(), PageLoad::kPageTiming_ForegroundDurationName));
  }
}

TEST_F(UkmPageLoadMetricsObserverTest,
       LargestContentfulPaintAllFrames_OnlySubframe) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  PopulateRequiredTimingFields(&timing);

  page_load_metrics::mojom::PageLoadTiming subframe_timing;
  page_load_metrics::InitPageLoadTimingForTest(&subframe_timing);
  subframe_timing.navigation_start = base::Time::FromDoubleT(2);
  subframe_timing.paint_timing->largest_image_paint =
      base::TimeDelta::FromMilliseconds(4780);
  subframe_timing.paint_timing->largest_image_paint_size = 100u;
  PopulateRequiredTimingFields(&subframe_timing);

  // Commit the main frame and a subframe.
  NavigateAndCommit(GURL(kTestUrl1));
  RenderFrameHost* subframe =
      NavigationSimulator::NavigateAndCommitFromDocument(
          GURL(kSubframeTestUrl),
          RenderFrameHostTester::For(web_contents()->GetMainFrame())
              ->AppendChild("subframe"));

  // Simulate timing updates in the main frame and the subframe.
  tester()->SimulateTimingUpdate(timing);
  tester()->SimulateTimingUpdate(subframe_timing, subframe);

  // Simulate closing the tab.
  DeleteContents();

  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      tester()->test_ukm_recorder().GetMergedEntriesByName(
          PageLoad::kEntryName);
  EXPECT_EQ(1ul, merged_entries.size());

  for (const auto& kv : merged_entries) {
    tester()->test_ukm_recorder().ExpectEntrySourceHasUrl(kv.second.get(),
                                                          GURL(kTestUrl1));
    tester()->test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(),
        PageLoad::kPaintTiming_NavigationToLargestContentfulPaintName, 4780);
    EXPECT_FALSE(tester()->test_ukm_recorder().EntryHasMetric(
        kv.second.get(),
        PageLoad::
            kPaintTiming_NavigationToLargestContentfulPaint_MainFrameName));
    EXPECT_TRUE(tester()->test_ukm_recorder().EntryHasMetric(
        kv.second.get(), PageLoad::kPageTiming_ForegroundDurationName));
  }
}

TEST_F(UkmPageLoadMetricsObserverTest,
       LargestContentfulPaintAllFrames_OnlyMainFrame) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.paint_timing->largest_image_paint =
      base::TimeDelta::FromMilliseconds(4780);
  timing.paint_timing->largest_image_paint_size = 100u;
  PopulateRequiredTimingFields(&timing);

  page_load_metrics::mojom::PageLoadTiming subframe_timing;
  page_load_metrics::InitPageLoadTimingForTest(&subframe_timing);
  subframe_timing.navigation_start = base::Time::FromDoubleT(2);
  PopulateRequiredTimingFields(&subframe_timing);

  // Commit the main frame and a subframe.
  NavigateAndCommit(GURL(kTestUrl1));
  RenderFrameHost* subframe =
      NavigationSimulator::NavigateAndCommitFromDocument(
          GURL(kSubframeTestUrl),
          RenderFrameHostTester::For(web_contents()->GetMainFrame())
              ->AppendChild("subframe"));

  // Simulate timing updates in the main frame and the subframe.
  tester()->SimulateTimingUpdate(timing);
  tester()->SimulateTimingUpdate(subframe_timing, subframe);

  // Simulate closing the tab.
  DeleteContents();

  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      tester()->test_ukm_recorder().GetMergedEntriesByName(
          PageLoad::kEntryName);
  EXPECT_EQ(1ul, merged_entries.size());

  for (const auto& kv : merged_entries) {
    tester()->test_ukm_recorder().ExpectEntrySourceHasUrl(kv.second.get(),
                                                          GURL(kTestUrl1));
    tester()->test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(),
        PageLoad::kPaintTiming_NavigationToLargestContentfulPaintName, 4780);
    tester()->test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(),
        PageLoad::kPaintTiming_NavigationToLargestContentfulPaint_MainFrameName,
        4780);
    EXPECT_TRUE(tester()->test_ukm_recorder().EntryHasMetric(
        kv.second.get(), PageLoad::kPageTiming_ForegroundDurationName));
  }
}

// This is to test whether LargestContentfulPaintAllFrames can merge the
// candidates from different frames correctly. The metric should pick the larger
// candidate during merging.
TEST_F(UkmPageLoadMetricsObserverTest,
       LargestContentfulPaintAllFrames_MergeFrameCandidateBySize) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.paint_timing->largest_image_paint =
      base::TimeDelta::FromMilliseconds(4780);
  timing.paint_timing->largest_image_paint_size = 50u;
  PopulateRequiredTimingFields(&timing);

  page_load_metrics::mojom::PageLoadTiming subframe_timing;
  page_load_metrics::InitPageLoadTimingForTest(&subframe_timing);
  subframe_timing.navigation_start = base::Time::FromDoubleT(2);
  timing.paint_timing->largest_image_paint =
      base::TimeDelta::FromMilliseconds(990);
  timing.paint_timing->largest_image_paint_size = 100u;
  PopulateRequiredTimingFields(&subframe_timing);

  // Commit the main frame and a subframe.
  NavigateAndCommit(GURL(kTestUrl1));
  RenderFrameHost* subframe =
      NavigationSimulator::NavigateAndCommitFromDocument(
          GURL(kSubframeTestUrl),
          RenderFrameHostTester::For(web_contents()->GetMainFrame())
              ->AppendChild("subframe"));

  // Simulate timing updates in the main frame and the subframe.
  tester()->SimulateTimingUpdate(timing);
  tester()->SimulateTimingUpdate(subframe_timing, subframe);

  // Simulate closing the tab.
  DeleteContents();

  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      tester()->test_ukm_recorder().GetMergedEntriesByName(
          PageLoad::kEntryName);
  EXPECT_EQ(1ul, merged_entries.size());

  for (const auto& kv : merged_entries) {
    tester()->test_ukm_recorder().ExpectEntrySourceHasUrl(kv.second.get(),
                                                          GURL(kTestUrl1));
    tester()->test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(),
        PageLoad::kPaintTiming_NavigationToLargestContentfulPaintName, 990);
    EXPECT_TRUE(tester()->test_ukm_recorder().EntryHasMetric(
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
  tester()->SimulateTimingUpdate(timing);

  // Simulate closing the tab.
  DeleteContents();

  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      tester()->test_ukm_recorder().GetMergedEntriesByName(
          PageLoad::kEntryName);
  EXPECT_EQ(1ul, merged_entries.size());

  for (const auto& kv : merged_entries) {
    tester()->test_ukm_recorder().ExpectEntrySourceHasUrl(kv.second.get(),
                                                          GURL(kTestUrl1));
    tester()->test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(), PageLoad::kExperimental_NavigationToInteractiveName,
        600);
    EXPECT_TRUE(tester()->test_ukm_recorder().EntryHasMetric(
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
  tester()->SimulateTimingUpdate(timing);

  // Simulate closing the tab.
  DeleteContents();

  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      tester()->test_ukm_recorder().GetMergedEntriesByName(
          PageLoad::kEntryName);
  EXPECT_EQ(1ul, merged_entries.size());

  for (const auto& kv : merged_entries) {
    tester()->test_ukm_recorder().ExpectEntrySourceHasUrl(kv.second.get(),
                                                          GURL(kTestUrl1));
    EXPECT_FALSE(tester()->test_ukm_recorder().EntryHasMetric(
        kv.second.get(), PageLoad::kExperimental_NavigationToInteractiveName));
    EXPECT_TRUE(tester()->test_ukm_recorder().EntryHasMetric(
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
  tester()->SimulateTimingUpdate(timing);

  // Simulate closing the tab.
  DeleteContents();

  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      tester()->test_ukm_recorder().GetMergedEntriesByName(
          PageLoad::kEntryName);
  EXPECT_EQ(1ul, merged_entries.size());

  for (const auto& kv : merged_entries) {
    tester()->test_ukm_recorder().ExpectEntrySourceHasUrl(kv.second.get(),
                                                          GURL(kTestUrl1));
    tester()->test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(), PageLoad::kInteractiveTiming_FirstInputDelay4Name, 50);
    tester()->test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(), PageLoad::kInteractiveTiming_FirstInputTimestamp4Name,
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
  tester()->SimulateTimingUpdate(timing);

  // Simulate closing the tab.
  DeleteContents();

  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      tester()->test_ukm_recorder().GetMergedEntriesByName(
          PageLoad::kEntryName);
  EXPECT_EQ(1ul, merged_entries.size());

  for (const auto& kv : merged_entries) {
    tester()->test_ukm_recorder().ExpectEntrySourceHasUrl(kv.second.get(),
                                                          GURL(kTestUrl1));
    tester()->test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(), PageLoad::kInteractiveTiming_LongestInputDelay4Name,
        50);
    tester()->test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(),
        PageLoad::kInteractiveTiming_LongestInputTimestamp4Name, 712);
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
  tester()->SimulateTimingUpdate(timing1);

  NavigateAndCommit(GURL(kTestUrl2));
  tester()->SimulateTimingUpdate(timing2);

  // Simulate closing the tab.
  DeleteContents();

  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      tester()->test_ukm_recorder().GetMergedEntriesByName(
          PageLoad::kEntryName);
  EXPECT_EQ(2ul, merged_entries.size());
  const ukm::mojom::UkmEntry* entry1 = nullptr;
  const ukm::mojom::UkmEntry* entry2 = nullptr;
  for (const auto& kv : merged_entries) {
    if (tester()->test_ukm_recorder().EntryHasMetric(
            kv.second.get(),
            PageLoad::kPaintTiming_NavigationToFirstContentfulPaintName)) {
      entry1 = kv.second.get();
    } else {
      entry2 = kv.second.get();
    }
  }
  ASSERT_NE(entry1, nullptr);
  ASSERT_NE(entry2, nullptr);

  tester()->test_ukm_recorder().ExpectEntrySourceHasUrl(entry1,
                                                        GURL(kTestUrl1));
  tester()->test_ukm_recorder().ExpectEntryMetric(
      entry1, PageLoad::kNavigation_PageEndReasonName,
      page_load_metrics::END_NEW_NAVIGATION);
  tester()->test_ukm_recorder().ExpectEntryMetric(
      entry1, PageLoad::kPaintTiming_NavigationToFirstContentfulPaintName, 200);
  EXPECT_FALSE(tester()->test_ukm_recorder().EntryHasMetric(
      entry1,
      PageLoad::
          kExperimental_PaintTiming_NavigationToFirstMeaningfulPaintName));
  EXPECT_TRUE(tester()->test_ukm_recorder().EntryHasMetric(
      entry1, PageLoad::kPageTiming_ForegroundDurationName));

  tester()->test_ukm_recorder().ExpectEntrySourceHasUrl(entry2,
                                                        GURL(kTestUrl2));
  tester()->test_ukm_recorder().ExpectEntryMetric(
      entry2, PageLoad::kNavigation_PageEndReasonName,
      page_load_metrics::END_CLOSE);
  EXPECT_FALSE(tester()->test_ukm_recorder().EntryHasMetric(
      entry2, PageLoad::kParseTiming_NavigationToParseStartName));
  EXPECT_FALSE(tester()->test_ukm_recorder().EntryHasMetric(
      entry2, PageLoad::kPaintTiming_NavigationToFirstContentfulPaintName));
  EXPECT_FALSE(tester()->test_ukm_recorder().EntryHasMetric(
      entry2,
      PageLoad::
          kExperimental_PaintTiming_NavigationToFirstMeaningfulPaintName));
  EXPECT_TRUE(tester()->test_ukm_recorder().EntryHasMetric(
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
      tester()->test_ukm_recorder().GetMergedEntriesByName(
          PageLoad::kEntryName);
  EXPECT_EQ(1ul, merged_entries.size());

  for (const auto& kv : merged_entries) {
    tester()->test_ukm_recorder().ExpectEntrySourceHasUrl(kv.second.get(),
                                                          GURL(kTestUrl1));
    tester()->test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(),
        PageLoad::kNet_EffectiveConnectionType2_OnNavigationStartName,
        metrics::SystemProfileProto::Network::EFFECTIVE_CONNECTION_TYPE_3G);
    tester()->test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(), PageLoad::kNet_HttpRttEstimate_OnNavigationStartName,
        100);
    tester()->test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(),
        PageLoad::kNet_TransportRttEstimate_OnNavigationStartName, 200);
    tester()->test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(),
        PageLoad::kNet_DownstreamKbpsEstimate_OnNavigationStartName, 300);
  }
}

TEST_F(UkmPageLoadMetricsObserverTest, PageTransitionReload) {
  GURL url(kTestUrl1);
  tester()->NavigateWithPageTransitionAndCommit(GURL(kTestUrl1),
                                                ui::PAGE_TRANSITION_RELOAD);

  // Simulate closing the tab.
  DeleteContents();

  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      tester()->test_ukm_recorder().GetMergedEntriesByName(
          PageLoad::kEntryName);
  EXPECT_EQ(1ul, merged_entries.size());

  for (const auto& kv : merged_entries) {
    tester()->test_ukm_recorder().ExpectEntrySourceHasUrl(kv.second.get(),
                                                          GURL(kTestUrl1));
    tester()->test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(), PageLoad::kNavigation_PageTransitionName,
        ui::PAGE_TRANSITION_RELOAD);
  }
}

TEST_F(UkmPageLoadMetricsObserverTest, PageSizeMetrics) {
  NavigateAndCommit(GURL(kTestUrl1));

  std::vector<page_load_metrics::mojom::ResourceDataUpdatePtr> resources;
  // Cached resource.
  resources.push_back(CreateResource(true /* was_cached */, 0 /* delta_bytes */,
                                     20 * 1024 /* encoded_body_length */,
                                     true /* is_complete */));
  // Uncached resource.
  resources.push_back(CreateResource(
      false /* was_cached */, 40 * 1024 /* delta_bytes */,
      40 * 1024 /* encoded_body_length */, true /* is_complete */));
  tester()->SimulateResourceDataUseUpdate(resources);

  int64_t network_bytes = 0;
  int64_t cache_bytes = 0;
  for (const auto& request : resources) {
    if (request->cache_type ==
        page_load_metrics::mojom::CacheType::kNotCached) {
      network_bytes += request->delta_bytes;
    } else {
      cache_bytes += request->encoded_body_length;
    }
  }

  // Simulate closing the tab.
  DeleteContents();

  int64_t bucketed_network_bytes =
      ukm::GetExponentialBucketMin(network_bytes, 1.3);
  int64_t bucketed_cache_bytes = ukm::GetExponentialBucketMin(cache_bytes, 1.3);

  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      tester()->test_ukm_recorder().GetMergedEntriesByName(
          PageLoad::kEntryName);
  EXPECT_EQ(1ul, merged_entries.size());

  for (const auto& kv : merged_entries) {
    tester()->test_ukm_recorder().ExpectEntrySourceHasUrl(kv.second.get(),
                                                          GURL(kTestUrl1));
    tester()->test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(), "Net.NetworkBytes2", bucketed_network_bytes);
    tester()->test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(), "Net.CacheBytes2", bucketed_cache_bytes);
  }
}

TEST_F(UkmPageLoadMetricsObserverTest, CpuTimeMetrics) {
  NavigateAndCommit(GURL(kTestUrl1));

  // Simulate some CPU usage.
  page_load_metrics::mojom::CpuTiming cpu_timing(
      base::TimeDelta::FromMilliseconds(500));
  tester()->SimulateCpuTimingUpdate(cpu_timing);

  // Simulate closing the tab.
  DeleteContents();

  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      tester()->test_ukm_recorder().GetMergedEntriesByName(
          PageLoad::kEntryName);
  EXPECT_EQ(1ul, merged_entries.size());

  for (const auto& kv : merged_entries) {
    tester()->test_ukm_recorder().ExpectEntrySourceHasUrl(kv.second.get(),
                                                          GURL(kTestUrl1));
    tester()->test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(), PageLoad::kCpuTimeName, 500);
  }
}

TEST_F(UkmPageLoadMetricsObserverTest, LayoutInstability) {
  NavigateAndCommit(GURL(kTestUrl1));

  page_load_metrics::mojom::FrameRenderDataUpdate render_data(1.0, 1.0);
  tester()->SimulateRenderDataUpdate(render_data);

  // Simulate hiding the tab (the report should include shifts after hide).
  web_contents()->WasHidden();

  render_data.layout_shift_delta = 1.5;
  render_data.layout_shift_delta_before_input_or_scroll = 0.0;
  tester()->SimulateRenderDataUpdate(render_data);

  // Simulate closing the tab.
  DeleteContents();

  const auto& ukm_recorder = tester()->test_ukm_recorder();
  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      ukm_recorder.GetMergedEntriesByName(PageLoad::kEntryName);
  EXPECT_EQ(1ul, merged_entries.size());

  for (const auto& kv : merged_entries) {
    const ukm::mojom::UkmEntry* ukm_entry = kv.second.get();
    ukm_recorder.ExpectEntrySourceHasUrl(ukm_entry, GURL(kTestUrl1));
    ukm_recorder.ExpectEntryMetric(
        ukm_entry, PageLoad::kLayoutInstability_CumulativeShiftScoreName, 250);
    ukm_recorder.ExpectEntryMetric(
        ukm_entry,
        PageLoad::
            kLayoutInstability_CumulativeShiftScore_MainFrame_BeforeInputOrScrollName,
        100);
  }

  EXPECT_THAT(tester()->histogram_tester().GetAllSamples(
                  "PageLoad.LayoutInstability.CumulativeShiftScore"),
              testing::ElementsAre(base::Bucket(25, 1)));
}

TEST_F(UkmPageLoadMetricsObserverTest, MHTMLNotTracked) {
  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      GURL(kTestUrl1), web_contents());
  navigation->SetContentsMimeType("multipart/related");
  navigation->Commit();

  // Simulate closing the tab.
  DeleteContents();

  EXPECT_EQ(0ul, tester()->test_ukm_recorder().entries_count());
}

TEST_F(UkmPageLoadMetricsObserverTest, LayoutInstabilitySubframeAggregation) {
  NavigateAndCommit(GURL(kTestUrl1));

  // Simulate layout instability in the main frame.
  page_load_metrics::mojom::FrameRenderDataUpdate render_data(1.0, 1.0);
  tester()->SimulateRenderDataUpdate(render_data);

  RenderFrameHost* subframe =
      NavigationSimulator::NavigateAndCommitFromDocument(
          GURL(kSubframeTestUrl),
          RenderFrameHostTester::For(web_contents()->GetMainFrame())
              ->AppendChild("subframe"));

  // Simulate layout instability in the subframe.
  render_data.layout_shift_delta = 1.5;
  tester()->SimulateRenderDataUpdate(render_data, subframe);

  // Simulate closing the tab.
  DeleteContents();

  // CLS score should be the sum of LS scores from all frames.
  EXPECT_THAT(tester()->histogram_tester().GetAllSamples(
                  "PageLoad.LayoutInstability.CumulativeShiftScore"),
              testing::ElementsAre(base::Bucket(25, 1)));

  // Main-frame (DCLS) score includes only the LS scores in the main frame.
  EXPECT_THAT(tester()->histogram_tester().GetAllSamples(
                  "PageLoad.LayoutInstability.CumulativeShiftScore.MainFrame"),
              testing::ElementsAre(base::Bucket(10, 1)));

  const auto& ukm_recorder = tester()->test_ukm_recorder();
  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      ukm_recorder.GetMergedEntriesByName(PageLoad::kEntryName);
  EXPECT_EQ(1ul, merged_entries.size());

  for (const auto& kv : merged_entries) {
    const ukm::mojom::UkmEntry* ukm_entry = kv.second.get();
    ukm_recorder.ExpectEntrySourceHasUrl(ukm_entry, GURL(kTestUrl1));

    // Check CLS score in UKM.
    ukm_recorder.ExpectEntryMetric(
        ukm_entry, PageLoad::kLayoutInstability_CumulativeShiftScoreName, 250);

    // Check DCLS score in UKM.
    ukm_recorder.ExpectEntryMetric(
        ukm_entry,
        PageLoad::kLayoutInstability_CumulativeShiftScore_MainFrameName, 100);
  }
}

TEST_F(UkmPageLoadMetricsObserverTest, ThirdPartyCookieBlockingDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(content_settings::kImprovedCookieControls);
  profile()->GetPrefs()->SetInteger(
      prefs::kCookieControlsMode,
      static_cast<int>(content_settings::CookieControlsMode::kOff));

  NavigateAndCommit(GURL(kTestUrl1));

  // Simulate closing the tab.
  DeleteContents();

  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      tester()->test_ukm_recorder().GetMergedEntriesByName(
          PageLoad::kEntryName);
  EXPECT_EQ(1ul, merged_entries.size());

  for (const auto& kv : merged_entries) {
    tester()->test_ukm_recorder().ExpectEntrySourceHasUrl(kv.second.get(),
                                                          GURL(kTestUrl1));
    EXPECT_FALSE(tester()->test_ukm_recorder().EntryHasMetric(
        kv.second.get(),
        PageLoad::kThirdPartyCookieBlockingEnabledForSiteName));
  }
}

TEST_F(UkmPageLoadMetricsObserverTest,
       ThirdPartyCookieBlockingFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(content_settings::kImprovedCookieControls);
  profile()->GetPrefs()->SetInteger(
      prefs::kCookieControlsMode,
      static_cast<int>(content_settings::CookieControlsMode::kOn));

  NavigateAndCommit(GURL(kTestUrl1));

  // Simulate closing the tab.
  DeleteContents();

  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      tester()->test_ukm_recorder().GetMergedEntriesByName(
          PageLoad::kEntryName);
  EXPECT_EQ(1ul, merged_entries.size());

  for (const auto& kv : merged_entries) {
    tester()->test_ukm_recorder().ExpectEntrySourceHasUrl(kv.second.get(),
                                                          GURL(kTestUrl1));
    EXPECT_FALSE(tester()->test_ukm_recorder().EntryHasMetric(
        kv.second.get(),
        PageLoad::kThirdPartyCookieBlockingEnabledForSiteName));
  }
}

TEST_F(UkmPageLoadMetricsObserverTest, ThirdPartyCookieBlockingEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(content_settings::kImprovedCookieControls);
  profile()->GetPrefs()->SetInteger(
      prefs::kCookieControlsMode,
      static_cast<int>(content_settings::CookieControlsMode::kOn));

  NavigateAndCommit(GURL(kTestUrl1));

  // Simulate closing the tab.
  DeleteContents();

  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      tester()->test_ukm_recorder().GetMergedEntriesByName(
          PageLoad::kEntryName);
  EXPECT_EQ(1ul, merged_entries.size());

  for (const auto& kv : merged_entries) {
    tester()->test_ukm_recorder().ExpectEntrySourceHasUrl(kv.second.get(),
                                                          GURL(kTestUrl1));
    tester()->test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(), PageLoad::kThirdPartyCookieBlockingEnabledForSiteName,
        true);
  }
}

TEST_F(UkmPageLoadMetricsObserverTest,
       ThirdPartyCookieBlockingDisabledForSite) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(content_settings::kImprovedCookieControls);
  profile()->GetPrefs()->SetInteger(
      prefs::kCookieControlsMode,
      static_cast<int>(content_settings::CookieControlsMode::kOn));
  auto cookie_settings = CookieSettingsFactory::GetForProfile(profile());
  cookie_settings->SetThirdPartyCookieSetting(GURL(kTestUrl1),
                                              CONTENT_SETTING_ALLOW);

  NavigateAndCommit(GURL(kTestUrl1));

  // Simulate closing the tab.
  DeleteContents();

  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      tester()->test_ukm_recorder().GetMergedEntriesByName(
          PageLoad::kEntryName);
  EXPECT_EQ(1ul, merged_entries.size());

  for (const auto& kv : merged_entries) {
    tester()->test_ukm_recorder().ExpectEntrySourceHasUrl(kv.second.get(),
                                                          GURL(kTestUrl1));
    tester()->test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(), PageLoad::kThirdPartyCookieBlockingEnabledForSiteName,
        false);
  }
}

class TestOfflinePreviewsUkmPageLoadMetricsObserver
    : public UkmPageLoadMetricsObserver {
 public:
  TestOfflinePreviewsUkmPageLoadMetricsObserver(
      MockNetworkQualityProvider* network_quality_provider)
      : UkmPageLoadMetricsObserver(network_quality_provider) {}

  ~TestOfflinePreviewsUkmPageLoadMetricsObserver() override {}

  bool IsOfflinePreview(content::WebContents* web_contents) const override {
    return true;
  }
};

class OfflinePreviewsUKMPageLoadMetricsObserverTest
    : public UkmPageLoadMetricsObserverTest {
 public:
  OfflinePreviewsUKMPageLoadMetricsObserverTest() = default;
  ~OfflinePreviewsUKMPageLoadMetricsObserverTest() override = default;

  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    tracker->AddObserver(
        std::make_unique<TestOfflinePreviewsUkmPageLoadMetricsObserver>(
            &mock_network_quality_provider()));
  }
};

TEST_F(OfflinePreviewsUKMPageLoadMetricsObserverTest, OfflinePreviewReported) {
  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      GURL(kTestUrl1), web_contents());
  navigation->SetContentsMimeType("multipart/related");
  navigation->Commit();

  // Simulate closing the tab.
  DeleteContents();

  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      tester()->test_ukm_recorder().GetMergedEntriesByName(
          PageLoad::kEntryName);
  EXPECT_EQ(1ul, merged_entries.size());

  for (const auto& kv : merged_entries) {
    tester()->test_ukm_recorder().ExpectEntrySourceHasUrl(kv.second.get(),
                                                          GURL(kTestUrl1));
    tester()->test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(), PageLoad::kNavigation_PageEndReasonName,
        page_load_metrics::END_CLOSE);
  }
}
