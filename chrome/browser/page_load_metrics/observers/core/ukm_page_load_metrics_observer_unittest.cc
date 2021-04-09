// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/core/ukm_page_load_metrics_observer.h"

#include <memory>

#include "base/metrics/metrics_hashes.h"
#include "base/optional.h"
#include "base/test/simple_test_clock.h"
#include "base/test/trace_event_analyzer.h"
#include "base/time/time.h"
#include "base/trace_event/traced_value.h"
#include "build/build_config.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/history_tab_helper.h"
#include "chrome/browser/history_clusters/history_clusters_tab_helper.h"
#include "chrome/browser/history_clusters/memories_service_factory.h"
#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/search_test_utils.h"
#include "chrome/test/base/testing_profile.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/test/history_service_test_util.h"
#include "components/history_clusters/core/memories_service.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/ntp_tiles/custom_links_store.h"
#include "components/page_load_metrics/browser/observers/core/largest_contentful_paint_handler.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "components/page_load_metrics/common/test/page_load_metrics_test_util.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url_service.h"
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

using GeneratedNavigation = ukm::builders::GeneratedNavigation;
using LargestContentState =
    page_load_metrics::PageLoadMetricsObserver::LargestContentState;
using LargestContentType =
    page_load_metrics::ContentfulPaintTimingInfo::LargestContentType;
using PageLoad = ukm::builders::PageLoad;
using MobileFriendliness = ukm::builders::MobileFriendliness;
using PageLoad_Internal = ukm::builders::PageLoad_Internal;

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
    std::unique_ptr<UkmPageLoadMetricsObserver> observer =
        std::make_unique<UkmPageLoadMetricsObserver>(
            &mock_network_quality_provider_);
    observer_ = observer.get();
    tracker->AddObserver(std::move(observer));
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
    TemplateURLServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        profile(),
        base::BindRepeating(&TemplateURLServiceFactory::BuildInstanceFor));

    BookmarkModelFactory::GetInstance()->SetTestingFactory(
        profile(), BookmarkModelFactory::GetDefaultFactory());
    bookmarks::BookmarkModel* bookmark_model =
        BookmarkModelFactory::GetForBrowserContext(profile());
    bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model);

    HistoryTabHelper::CreateForWebContents(web_contents());
    HistoryTabHelper::FromWebContents(web_contents())
        ->SetForceEligibleTabForTesting(true);

    HistoryClustersTabHelper::CreateForWebContents(web_contents());
  }

  MockNetworkQualityProvider& mock_network_quality_provider() {
    return mock_network_quality_provider_;
  }

  // Tests that LCP and its experimental histograms report the given |value|,
  // and tests that the LCP content type reported is |type|. If
  // |test_main_frame| is set, also tests that the main frame LCP histograms
  // also report |value|.
  void TestLCP(int value, LargestContentType type, bool test_main_frame) {
    std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
        tester()->test_ukm_recorder().GetMergedEntriesByName(
            PageLoad::kEntryName);
    EXPECT_EQ(1ul, merged_entries.size());

    const ukm::mojom::UkmEntry* entry = merged_entries.begin()->second.get();
    tester()->test_ukm_recorder().ExpectEntrySourceHasUrl(entry,
                                                          GURL(kTestUrl1));
    tester()->test_ukm_recorder().ExpectEntryMetric(
        entry, PageLoad::kPaintTiming_NavigationToLargestContentfulPaint2Name,
        value);
    tester()->test_ukm_recorder().ExpectEntryMetric(
        entry, PageLoad::kPaintTiming_NavigationToLargestContentfulPaintName,
        value);
    if (test_main_frame) {
      tester()->test_ukm_recorder().ExpectEntryMetric(
          entry,
          PageLoad::
              kPaintTiming_NavigationToLargestContentfulPaint2_MainFrameName,
          value);
      tester()->test_ukm_recorder().ExpectEntryMetric(
          entry,
          PageLoad::
              kPaintTiming_NavigationToLargestContentfulPaint_MainFrameName,
          value);
    }
    EXPECT_TRUE(tester()->test_ukm_recorder().EntryHasMetric(
        entry, PageLoad::kPageTiming_ForegroundDurationName));

    std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> internal_merged_entries =
        tester()->test_ukm_recorder().GetMergedEntriesByName(
            PageLoad_Internal::kEntryName);
    EXPECT_EQ(1ul, internal_merged_entries.size());
    const ukm::mojom::UkmEntry* internal_entry =
        internal_merged_entries.begin()->second.get();
    tester()->test_ukm_recorder().ExpectEntrySourceHasUrl(internal_entry,
                                                          GURL(kTestUrl1));
    tester()->test_ukm_recorder().ExpectEntryMetric(
        internal_entry,
        PageLoad_Internal::kPaintTiming_LargestContentfulPaint_ContentTypeName,
        static_cast<int>(type));
    tester()->test_ukm_recorder().ExpectEntryMetric(
        internal_entry,
        PageLoad_Internal::
            kPaintTiming_ExperimentalLargestContentfulPaint_ContentTypeName,
        static_cast<int>(type));
    tester()->test_ukm_recorder().ExpectEntryMetric(
        internal_entry,
        PageLoad_Internal::
            kPaintTiming_LargestContentfulPaint_TerminationStateName,
        static_cast<int>(LargestContentState::kReported));
    tester()->test_ukm_recorder().ExpectEntryMetric(
        internal_entry,
        PageLoad_Internal::
            kPaintTiming_ExperimentalLargestContentfulPaint_TerminationStateName,
        static_cast<int>(LargestContentState::kReported));
  }

  void TestNoLCP(LargestContentState state) {
    std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
        tester()->test_ukm_recorder().GetMergedEntriesByName(
            PageLoad::kEntryName);
    EXPECT_EQ(1ul, merged_entries.size());
    const ukm::mojom::UkmEntry* entry = merged_entries.begin()->second.get();
    EXPECT_FALSE(tester()->test_ukm_recorder().EntryHasMetric(
        entry, PageLoad::kPaintTiming_NavigationToLargestContentfulPaint2Name));
    EXPECT_FALSE(tester()->test_ukm_recorder().EntryHasMetric(
        entry, PageLoad::kPaintTiming_NavigationToLargestContentfulPaintName));
    EXPECT_FALSE(tester()->test_ukm_recorder().EntryHasMetric(
        entry,
        PageLoad::
            kPaintTiming_NavigationToLargestContentfulPaint2_MainFrameName));
    EXPECT_FALSE(tester()->test_ukm_recorder().EntryHasMetric(
        entry,
        PageLoad::
            kPaintTiming_NavigationToLargestContentfulPaint_MainFrameName));

    std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> internal_merged_entries =
        tester()->test_ukm_recorder().GetMergedEntriesByName(
            PageLoad_Internal::kEntryName);
    EXPECT_EQ(1ul, internal_merged_entries.size());
    const ukm::mojom::UkmEntry* internal_entry =
        internal_merged_entries.begin()->second.get();
    tester()->test_ukm_recorder().ExpectEntrySourceHasUrl(internal_entry,
                                                          GURL(kTestUrl1));
    EXPECT_FALSE(tester()->test_ukm_recorder().EntryHasMetric(
        internal_entry,
        PageLoad_Internal::
            kPaintTiming_LargestContentfulPaint_ContentTypeName));
    EXPECT_FALSE(tester()->test_ukm_recorder().EntryHasMetric(
        internal_entry,
        PageLoad_Internal::
            kPaintTiming_ExperimentalLargestContentfulPaint_ContentTypeName));
    tester()->test_ukm_recorder().ExpectEntryMetric(
        internal_entry,
        PageLoad_Internal::
            kPaintTiming_LargestContentfulPaint_TerminationStateName,
        static_cast<int>(state));
    tester()->test_ukm_recorder().ExpectEntryMetric(
        internal_entry,
        PageLoad_Internal::
            kPaintTiming_ExperimentalLargestContentfulPaint_TerminationStateName,
        static_cast<int>(state));
  }

  UkmPageLoadMetricsObserver* observer() const { return observer_; }

 private:
  UkmPageLoadMetricsObserver* observer_;  // Non-owning raw pointer.

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
        kv.second.get(), PageLoad::kNavigation_PageEndReason3Name,
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
        kv.second.get(), PageLoad::kNavigation_PageEndReason3Name,
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
  timing.parse_timing->parse_start = base::TimeDelta::FromMilliseconds(10);
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
  timing.paint_timing->largest_contentful_paint->largest_image_paint =
      base::TimeDelta::FromMilliseconds(600);
  timing.paint_timing->largest_contentful_paint->largest_image_paint_size = 50u;
  PopulateExperimentalLCP(timing.paint_timing);
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kTestUrl1));
  tester()->SimulateTimingUpdate(timing);

  // Simulate closing the tab.
  DeleteContents();

  TestLCP(600, LargestContentType::kImage, true /* test_main_frame */);
}

TEST_F(UkmPageLoadMetricsObserverTest, LargestImageLoading) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  // The largest image is loading so its paint time is set to TimeDelta().
  timing.paint_timing->largest_contentful_paint->largest_image_paint =
      base::TimeDelta();
  timing.paint_timing->largest_contentful_paint->largest_image_paint_size = 50u;
  // There is a text paint, but it must be ignored because it is smaller than
  // the image paint.
  timing.paint_timing->largest_contentful_paint->largest_text_paint =
      base::TimeDelta::FromMilliseconds(600);
  timing.paint_timing->largest_contentful_paint->largest_text_paint_size = 25u;
  PopulateExperimentalLCP(timing.paint_timing);
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kTestUrl1));
  tester()->SimulateTimingUpdate(timing);

  // Simulate closing the tab.
  DeleteContents();

  TestNoLCP(LargestContentState::kLargestImageLoading);
}

TEST_F(UkmPageLoadMetricsObserverTest, LargestImageLoadingSmallerThanText) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  // Largest image is loading.
  timing.paint_timing->largest_contentful_paint->largest_image_paint =
      base::TimeDelta();
  timing.paint_timing->largest_contentful_paint->largest_image_paint_size = 50u;
  // Largest text is larger than image.
  timing.paint_timing->largest_contentful_paint->largest_text_paint =
      base::TimeDelta::FromMilliseconds(600);
  timing.paint_timing->largest_contentful_paint->largest_text_paint_size = 80u;
  PopulateExperimentalLCP(timing.paint_timing);
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kTestUrl1));
  tester()->SimulateTimingUpdate(timing);

  // Simulate closing the tab.
  DeleteContents();

  TestLCP(600, LargestContentType::kText, true /* test_main_frame */);
}

TEST_F(UkmPageLoadMetricsObserverTest,
       LargestImagePaint_DiscardBackgroundResult) {
  std::unique_ptr<base::SimpleTestClock> mock_clock(
      new base::SimpleTestClock());
  mock_clock->SetNow(base::Time::NowFromSystemTime());

  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.paint_timing->largest_contentful_paint->largest_image_paint =
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
  EXPECT_EQ(1ul, merged_entries.size());
  const ukm::mojom::UkmEntry* entry = merged_entries.begin()->second.get();
  EXPECT_FALSE(tester()->test_ukm_recorder().EntryHasMetric(
      entry, PageLoad::kPaintTiming_NavigationToLargestContentfulPaint2Name));
  tester()->test_ukm_recorder().ExpectEntryMetric(
      entry, PageLoad::kNavigation_PageEndReason3Name,
      page_load_metrics::END_CLOSE);

  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> internal_merged_entries =
      tester()->test_ukm_recorder().GetMergedEntriesByName(
          PageLoad_Internal::kEntryName);
  // RecordTimingMetrics() is not called in this test.
  EXPECT_EQ(0ul, internal_merged_entries.size());
}

TEST_F(UkmPageLoadMetricsObserverTest, AbortNeverForegrounded) {
  std::unique_ptr<base::SimpleTestClock> mock_clock(
      new base::SimpleTestClock());
  mock_clock->SetNow(base::Time::NowFromSystemTime());

  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
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
  EXPECT_EQ(1ul, merged_entries.size());
  const ukm::mojom::UkmEntry* entry = merged_entries.begin()->second.get();
  tester()->test_ukm_recorder().ExpectEntryMetric(
      entry, PageLoad::kNavigation_PageEndReason3Name,
      page_load_metrics::END_CLOSE);
  tester()->test_ukm_recorder().ExpectEntryMetric(
      entry, PageLoad::kExperimental_PageLoadTypeName,
      static_cast<int64_t>(PageLoadType::kNeverForegrounded));
  tester()->test_ukm_recorder().ExpectEntryMetric(
      entry, PageLoad::kNavigation_PageTransitionName,
      ui::PAGE_TRANSITION_LINK);
  tester()->test_ukm_recorder().ExpectEntryMetric(
      entry, PageLoad::kExperimental_TotalForegroundDurationName, 0);
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
  timing.paint_timing->largest_contentful_paint->largest_image_paint =
      base::TimeDelta::FromSeconds(10);
  timing.paint_timing->largest_contentful_paint->largest_text_paint =
      base::TimeDelta::FromSeconds(10);
  NavigateAndCommit(GURL(kTestUrl1));
  tester()->SimulateTimingUpdate(timing);

  // Simulate closing the tab.
  DeleteContents();

  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      tester()->test_ukm_recorder().GetMergedEntriesByName(
          PageLoad::kEntryName);
  EXPECT_EQ(1ul, merged_entries.size());
  const ukm::mojom::UkmEntry* entry = merged_entries.begin()->second.get();
  EXPECT_FALSE(tester()->test_ukm_recorder().EntryHasMetric(
      entry, PageLoad::kPaintTiming_NavigationToLargestContentfulPaint2Name));
  tester()->test_ukm_recorder().ExpectEntryMetric(
      entry, PageLoad::kNavigation_PageEndReason3Name,
      page_load_metrics::END_CLOSE);

  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> internal_merged_entries =
      tester()->test_ukm_recorder().GetMergedEntriesByName(
          PageLoad_Internal::kEntryName);
  // RecordTimingMetrics() is not called in this test.
  EXPECT_EQ(0ul, internal_merged_entries.size());
}

TEST_F(UkmPageLoadMetricsObserverTest, FCPPlusPlus_ReportLastCandidate) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);

  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kTestUrl1));
  timing.paint_timing->largest_contentful_paint->largest_image_paint =
      base::TimeDelta::FromMilliseconds(60);
  timing.paint_timing->largest_contentful_paint->largest_image_paint_size = 10u;
  timing.paint_timing->largest_contentful_paint->largest_text_paint =
      base::TimeDelta::FromMilliseconds(60);
  timing.paint_timing->largest_contentful_paint->largest_text_paint_size = 10u;
  PopulateExperimentalLCP(timing.paint_timing);
  tester()->SimulateTimingUpdate(timing);

  timing.paint_timing->largest_contentful_paint->largest_image_paint =
      base::TimeDelta::FromMilliseconds(600);
  timing.paint_timing->largest_contentful_paint->largest_image_paint_size = 10u;
  timing.paint_timing->largest_contentful_paint->largest_text_paint =
      base::TimeDelta::FromMilliseconds(600);
  timing.paint_timing->largest_contentful_paint->largest_text_paint_size = 10u;
  PopulateExperimentalLCP(timing.paint_timing);
  tester()->SimulateTimingUpdate(timing);

  // Simulate closing the tab.
  DeleteContents();

  // There's a tie here and in this case the LCP handler returns image as the
  // type.
  TestLCP(600, LargestContentType::kImage, true /* test_main_frame */);
}

TEST_F(UkmPageLoadMetricsObserverTest, LargestTextPaint) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.paint_timing->largest_contentful_paint->largest_text_paint =
      base::TimeDelta::FromMilliseconds(600);
  timing.paint_timing->largest_contentful_paint->largest_text_paint_size = 50u;
  PopulateExperimentalLCP(timing.paint_timing);
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kTestUrl1));
  tester()->SimulateTimingUpdate(timing);

  // Simulate closing the tab.
  DeleteContents();

  TestLCP(600, LargestContentType::kText, true /* test_main_frame */);
}

TEST_F(UkmPageLoadMetricsObserverTest, LargestContentfulPaint_Trace) {
  using trace_analyzer::Query;
  trace_analyzer::Start("*");
  {
    page_load_metrics::mojom::PageLoadTiming timing;
    page_load_metrics::InitPageLoadTimingForTest(&timing);
    timing.navigation_start = base::Time::FromDoubleT(1);
    timing.paint_timing->largest_contentful_paint->largest_text_paint =
        base::TimeDelta::FromMilliseconds(600);
    timing.paint_timing->largest_contentful_paint->largest_text_paint_size =
        1000;
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
    timing.paint_timing->largest_contentful_paint->largest_text_paint =
        base::TimeDelta::FromMilliseconds(600);
    timing.paint_timing->largest_contentful_paint->largest_text_paint_size =
        1000;
    PopulateRequiredTimingFields(&timing);

    NavigateAndCommit(GURL(kTestUrl1));
    tester()->SimulateTimingUpdate(timing);

    timing.paint_timing->largest_contentful_paint->largest_text_paint =
        base::Optional<base::TimeDelta>();
    timing.paint_timing->largest_contentful_paint->largest_text_paint_size = 0;
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
  timing.paint_timing->largest_contentful_paint->largest_text_paint =
      base::TimeDelta::FromMilliseconds(600);
  timing.paint_timing->largest_contentful_paint->largest_text_paint_size = 1000;
  PopulateExperimentalLCP(timing.paint_timing);
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kTestUrl1));
  tester()->SimulateTimingUpdate(timing);

  // Simulate closing the tab.
  DeleteContents();

  TestLCP(600, LargestContentType::kText, true /* test_main_frame */);
}

TEST_F(UkmPageLoadMetricsObserverTest, LargestContentfulPaint_OnlyImage) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.paint_timing->largest_contentful_paint->largest_image_paint =
      base::TimeDelta::FromMilliseconds(600);
  timing.paint_timing->largest_contentful_paint->largest_image_paint_size =
      1000;
  PopulateExperimentalLCP(timing.paint_timing);
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kTestUrl1));
  tester()->SimulateTimingUpdate(timing);

  // Simulate closing the tab.
  DeleteContents();

  TestLCP(600, LargestContentType::kImage, true /* test_main_frame */);
}

TEST_F(UkmPageLoadMetricsObserverTest,
       LargestContentfulPaint_ImageLargerThanText) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.paint_timing->largest_contentful_paint->largest_image_paint =
      base::TimeDelta::FromMilliseconds(600);
  timing.paint_timing->largest_contentful_paint->largest_image_paint_size =
      1000;
  timing.paint_timing->largest_contentful_paint->largest_text_paint =
      base::TimeDelta::FromMilliseconds(1000);
  timing.paint_timing->largest_contentful_paint->largest_text_paint_size = 500;
  PopulateExperimentalLCP(timing.paint_timing);
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kTestUrl1));
  tester()->SimulateTimingUpdate(timing);

  // Simulate closing the tab.
  DeleteContents();

  TestLCP(600, LargestContentType::kImage, true /* test_main_frame */);
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
  subframe_timing.paint_timing->largest_contentful_paint->largest_image_paint =
      base::TimeDelta::FromMilliseconds(4780);
  subframe_timing.paint_timing->largest_contentful_paint
      ->largest_image_paint_size = 100u;
  PopulateExperimentalLCP(subframe_timing.paint_timing);
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

  TestLCP(4780, LargestContentType::kImage, false /* test_main_frame */);

  // Now test that there is no main-frame LCP.
  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      tester()->test_ukm_recorder().GetMergedEntriesByName(
          PageLoad::kEntryName);
  EXPECT_EQ(1ul, merged_entries.size());
  const ukm::mojom::UkmEntry* entry = merged_entries.begin()->second.get();
  EXPECT_FALSE(tester()->test_ukm_recorder().EntryHasMetric(
      entry,
      PageLoad::
          kPaintTiming_NavigationToLargestContentfulPaint2_MainFrameName));
  EXPECT_FALSE(tester()->test_ukm_recorder().EntryHasMetric(
      entry,
      PageLoad::kPaintTiming_NavigationToLargestContentfulPaint_MainFrameName));
}

TEST_F(UkmPageLoadMetricsObserverTest,
       LargestContentfulPaintAllFrames_SubframeImageLoading) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  PopulateRequiredTimingFields(&timing);

  page_load_metrics::mojom::PageLoadTiming subframe_timing;
  page_load_metrics::InitPageLoadTimingForTest(&subframe_timing);
  subframe_timing.navigation_start = base::Time::FromDoubleT(2);
  // Subframe's largest image is still loading.
  subframe_timing.paint_timing->largest_contentful_paint->largest_image_paint =
      base::TimeDelta();
  subframe_timing.paint_timing->largest_contentful_paint
      ->largest_image_paint_size = 100u;
  // Subframe has text but it should be ignored as it's smaller than image.
  subframe_timing.paint_timing->largest_contentful_paint->largest_text_paint =
      base::TimeDelta::FromMilliseconds(3000);
  subframe_timing.paint_timing->largest_contentful_paint
      ->largest_text_paint_size = 80u;
  PopulateExperimentalLCP(subframe_timing.paint_timing);
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

  TestNoLCP(LargestContentState::kLargestImageLoading);
}

TEST_F(UkmPageLoadMetricsObserverTest,
       LargestContentfulPaintAllFrames_OnlyMainFrame) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.paint_timing->largest_contentful_paint->largest_image_paint =
      base::TimeDelta::FromMilliseconds(4780);
  timing.paint_timing->largest_contentful_paint->largest_image_paint_size =
      100u;
  PopulateExperimentalLCP(timing.paint_timing);
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

  TestLCP(4780, LargestContentType::kImage, true /* test_main_frame */);
}

// This is to test whether LargestContentfulPaintAllFrames can merge the
// candidates from different frames correctly. The metric should pick the larger
// candidate during merging.
TEST_F(UkmPageLoadMetricsObserverTest,
       LargestContentfulPaintAllFrames_MergeFrameCandidateBySize) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.paint_timing->largest_contentful_paint->largest_image_paint =
      base::TimeDelta::FromMilliseconds(4780);
  timing.paint_timing->largest_contentful_paint->largest_image_paint_size = 50u;
  PopulateExperimentalLCP(timing.paint_timing);
  PopulateRequiredTimingFields(&timing);

  page_load_metrics::mojom::PageLoadTiming subframe_timing;
  page_load_metrics::InitPageLoadTimingForTest(&subframe_timing);
  subframe_timing.navigation_start = base::Time::FromDoubleT(2);
  subframe_timing.paint_timing->largest_contentful_paint->largest_image_paint =
      base::TimeDelta::FromMilliseconds(990);
  subframe_timing.paint_timing->largest_contentful_paint
      ->largest_image_paint_size = 100u;
  PopulateExperimentalLCP(subframe_timing.paint_timing);
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

  TestLCP(990, LargestContentType::kImage, false /* test_main_frame */);
}

TEST_F(UkmPageLoadMetricsObserverTest,
       FirstInputDelayAndTimestampAndProcessingTime) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.interactive_timing->first_input_delay =
      base::TimeDelta::FromMilliseconds(50);
  timing.interactive_timing->first_input_timestamp =
      base::TimeDelta::FromMilliseconds(712);
  timing.interactive_timing->first_input_processing_time =
      base::TimeDelta::FromMilliseconds(25);
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
    tester()->test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(),
        PageLoad::kInteractiveTiming_FirstInputProcessingTimesName, 25);
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

TEST_F(UkmPageLoadMetricsObserverTest, InputTiming) {
  NavigateAndCommit(GURL(kTestUrl1));

  page_load_metrics::mojom::InputTiming input_timing;
  input_timing.num_input_events = 2;
  input_timing.total_input_delay = base::TimeDelta::FromMilliseconds(100);
  input_timing.total_adjusted_input_delay =
      base::TimeDelta::FromMilliseconds(10);
  tester()->SimulateInputTimingUpdate(input_timing);

  DeleteContents();

  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      tester()->test_ukm_recorder().GetMergedEntriesByName(
          PageLoad::kEntryName);
  EXPECT_EQ(1ul, merged_entries.size());

  for (const auto& kv : merged_entries) {
    tester()->test_ukm_recorder().ExpectEntrySourceHasUrl(kv.second.get(),
                                                          GURL(kTestUrl1));
    tester()->test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(), PageLoad::kInteractiveTiming_NumInputEventsName, 2);
    tester()->test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(), PageLoad::kInteractiveTiming_TotalInputDelayName, 100);
    tester()->test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(),
        PageLoad::kInteractiveTiming_TotalAdjustedInputDelayName, 10);
  }
}

TEST_F(UkmPageLoadMetricsObserverTest, MobileFriendliness) {
  NavigateAndCommit(GURL(kTestUrl1));
  blink::MobileFriendliness mobile_friendliness;
  mobile_friendliness.viewport_device_width = blink::mojom::ViewportStatus::kNo;
  mobile_friendliness.viewport_hardcoded_width = 533;
  mobile_friendliness.viewport_initial_scale_x10 = 10;
  mobile_friendliness.allow_user_zoom = blink::mojom::ViewportStatus::kYes;
  const int expected_viewport_hardcoded_width = 520;
  const int expected_viewport_initial_scale = 10;

  tester()->SimulateMobileFriendlinessUpdate(mobile_friendliness);

  // Simulate closing the tab.
  DeleteContents();

  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      tester()->test_ukm_recorder().GetMergedEntriesByName(
          MobileFriendliness::kEntryName);
  EXPECT_EQ(1ul, merged_entries.size());
  for (const auto& kv : merged_entries) {
    tester()->test_ukm_recorder().ExpectEntrySourceHasUrl(kv.second.get(),
                                                          GURL(kTestUrl1));
    tester()->test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(), MobileFriendliness::kViewportDeviceWidthName, false);
    tester()->test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(), MobileFriendliness::kViewportHardcodedWidthName,
        expected_viewport_hardcoded_width);
    tester()->test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(), MobileFriendliness::kViewportInitialScaleX10Name,
        expected_viewport_initial_scale);
    tester()->test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(), MobileFriendliness::kAllowUserZoomName, true);
  }
}

TEST_F(UkmPageLoadMetricsObserverTest, FirstScrollDelay) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.interactive_timing->first_scroll_delay =
      base::TimeDelta::FromMilliseconds(50);
  timing.interactive_timing->first_scroll_timestamp =
      base::TimeDelta::FromMilliseconds(70);
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
        kv.second.get(), PageLoad::kInteractiveTiming_FirstScrollDelayName, 50);
  }
}

TEST_F(UkmPageLoadMetricsObserverTest, MultiplePageLoads) {
  page_load_metrics::mojom::PageLoadTiming timing1;
  page_load_metrics::InitPageLoadTimingForTest(&timing1);
  timing1.parse_timing->parse_start = base::TimeDelta::FromMilliseconds(10);
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
      entry1, PageLoad::kNavigation_PageEndReason3Name,
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
      entry2, PageLoad::kNavigation_PageEndReason3Name,
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
                                     30 * 1024 /* decoded_body_length */,
                                     true /* is_complete */));
  // Uncached resource.
  resources.push_back(CreateResource(
      false /* was_cached */, 40 * 1024 /* delta_bytes */,
      40 * 1024 /* encoded_body_length */, 50 * 1024 /* decoded_body_length */,
      true /* is_complete */));
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

TEST_F(UkmPageLoadMetricsObserverTest, JSSizeMetrics) {
  NavigateAndCommit(GURL(kTestUrl1));

  std::vector<page_load_metrics::mojom::ResourceDataUpdatePtr> resources;
  // 30 kilobytes after decoding.
  resources.push_back(CreateResource(true /* was_cached */, 0 /* delta_bytes */,
                                     20 * 1024 /* encoded_body_length */,
                                     30 * 1024 /* decoded_body_length */,
                                     true /* is_complete */));

  // 50 kilobytes after decoding.
  resources.push_back(CreateResource(
      false /* was_cached */, 40 * 1024 /* delta_bytes */,
      40 * 1024 /* encoded_body_length */, 50 * 1024 /* decoded_body_length */,
      true /* is_complete */));

  // 120 kilobytes after decoding, not JS.
  resources.push_back(CreateResource(
      false /* was_cached */, 40 * 1024 /* delta_bytes */,
      100 * 1024 /* encoded_body_length */,
      120 * 1024 /* decoded_body_length */, true /* is_complete */));

  resources[0]->mime_type = "application/javascript";
  resources[1]->mime_type = "application/javascript";
  resources[2]->mime_type = "test";

  tester()->SimulateResourceDataUseUpdate(resources);

  // Simulate closing the tab.
  DeleteContents();

  // Metrics look at decoded body length.
  // 30 + 50 = 80 kilobytes.
  int64_t bucketed_network_js_bytes =
      ukm::GetExponentialBucketMin(80 * 1024, 10);

  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      tester()->test_ukm_recorder().GetMergedEntriesByName(
          PageLoad::kEntryName);
  EXPECT_EQ(1ul, merged_entries.size());

  for (const auto& kv : merged_entries) {
    tester()->test_ukm_recorder().ExpectEntrySourceHasUrl(kv.second.get(),
                                                          GURL(kTestUrl1));
    tester()->test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(), "Net.JavaScriptBytes", bucketed_network_js_bytes);
  }
}

TEST_F(UkmPageLoadMetricsObserverTest, JSMaxSizeMetrics) {
  NavigateAndCommit(GURL(kTestUrl1));

  std::vector<page_load_metrics::mojom::ResourceDataUpdatePtr> resources;

  // 30 kilobytes after decoding.
  resources.push_back(CreateResource(true /* was_cached */, 0 /* delta_bytes */,
                                     20 * 1024 /* encoded_body_length */,
                                     30 * 1024 /* decoded_body_length */,
                                     true /* is_complete */));

  // 500 kilobytes after decoding.
  resources.push_back(CreateResource(
      false /* was_cached */, 400 * 1024 /* delta_bytes */,
      400 * 1024 /* encoded_body_length */,
      500 * 1024 /* decoded_body_length */, true /* is_complete */));

  // 120 kilobytes after decoding, not JS.
  resources.push_back(CreateResource(
      false /* was_cached */, 40 * 1024 /* delta_bytes */,
      100 * 1024 /* encoded_body_length */,
      120 * 1024 /* decoded_body_length */, true /* is_complete */));

  resources[0]->mime_type = "application/javascript";
  resources[1]->mime_type = "application/javascript";
  resources[2]->mime_type = "test";

  tester()->SimulateResourceDataUseUpdate(resources);

  // Simulate closing the tab.
  DeleteContents();

  // Metrics look at max decoded body length.
  // max(30,500) = 500 kilobytes.
  int64_t bucketed_network_js_max_bytes =
      ukm::GetExponentialBucketMin(500 * 1024, 10);

  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      tester()->test_ukm_recorder().GetMergedEntriesByName(
          PageLoad::kEntryName);
  EXPECT_EQ(1ul, merged_entries.size());

  for (const auto& kv : merged_entries) {
    tester()->test_ukm_recorder().ExpectEntrySourceHasUrl(kv.second.get(),
                                                          GURL(kTestUrl1));
    tester()->test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(), "Net.JavaScriptMaxBytes",
        bucketed_network_js_max_bytes);
  }
}

TEST_F(UkmPageLoadMetricsObserverTest, ImageMediaSizeMetrics) {
  NavigateAndCommit(GURL(kTestUrl1));

  std::vector<page_load_metrics::mojom::ResourceDataUpdatePtr> resources;
  resources.push_back(CreateResource(
      false /* was_cached */, 10 * 1024 /* delta_bytes */,
      10 * 1024 /* encoded_body_length */, 10 * 1024 /* decoded_body_length */,
      true /* is_complete */));
  resources.push_back(CreateResource(
      false /* was_cached */, 20 * 1024 /* delta_bytes */,
      20 * 1024 /* encoded_body_length */, 20 * 1024 /* decoded_body_length */,
      true /* is_complete */));
  resources.push_back(CreateResource(
      false /* was_cached */, 50 * 1024 /* delta_bytes */,
      50 * 1024 /* encoded_body_length */, 50 * 1024 /* decoded_body_length */,
      true /* is_complete */));

  resources[0]->mime_type = "image/png";
  resources[0]->is_main_frame_resource = true;
  resources[1]->mime_type = "image/jpg";
  resources[1]->is_main_frame_resource = false;
  resources[2]->mime_type = "video/mp4";

  tester()->SimulateResourceDataUseUpdate(resources);

  // Simulate closing the tab.
  DeleteContents();

  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      tester()->test_ukm_recorder().GetMergedEntriesByName(
          PageLoad::kEntryName);
  EXPECT_EQ(1ul, merged_entries.size());

  for (const auto& kv : merged_entries) {
    tester()->test_ukm_recorder().ExpectEntrySourceHasUrl(kv.second.get(),
                                                          GURL(kTestUrl1));
    // 30 KB for all images, 20 KB for subframe images, and 50 KB for media.
    tester()->test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(), "Net.ImageBytes",
        ukm::GetExponentialBucketMin(30 * 1024, 1.15));
    tester()->test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(), "Net.ImageSubframeBytes",
        ukm::GetExponentialBucketMin(20 * 1024, 1.15));
    tester()->test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(), "Net.MediaBytes",
        ukm::GetExponentialBucketMin(50 * 1024, 1.15));
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
  base::TimeTicks time_origin = base::TimeTicks::Now();
  page_load_metrics::mojom::FrameRenderDataUpdate render_data(
      1.0, 1.0, 0, 0, 0, 0, {},
      {time_origin - base::TimeDelta::FromMilliseconds(3000)});
  render_data.new_layout_shifts.emplace_back(
      page_load_metrics::mojom::LayoutShift::New(
          time_origin - base::TimeDelta::FromMilliseconds(4000), 0.5));
  render_data.new_layout_shifts.emplace_back(
      page_load_metrics::mojom::LayoutShift::New(
          time_origin - base::TimeDelta::FromMilliseconds(3500), 0.5));

  tester()->SimulateRenderDataUpdate(render_data);

  // Simulate hiding the tab (the report should include shifts after hide).
  web_contents()->WasHidden();

  page_load_metrics::mojom::FrameRenderDataUpdate render_data_2(1.5, 0.0, 0, 0,
                                                                0, 0, {}, {});
  render_data_2.new_layout_shifts.emplace_back(
      page_load_metrics::mojom::LayoutShift::New(
          time_origin - base::TimeDelta::FromMilliseconds(2500), 1.5));
  tester()->SimulateRenderDataUpdate(render_data_2);

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
    ukm_recorder.ExpectEntryMetric(
        ukm_entry,
        PageLoad::
            kLayoutInstability_MaxCumulativeShiftScore_SessionWindow_Gap1000ms_Max5000msName,
        250);
    ukm_recorder.ExpectEntryMetric(
        ukm_entry,
        PageLoad::
            kLayoutInstability_MaxCumulativeShiftScore_SessionWindow_Gap1000msName,
        250);
    ukm_recorder.ExpectEntryMetric(
        ukm_entry,
        PageLoad::
            kLayoutInstability_MaxCumulativeShiftScore_SlidingWindow_Duration1000msName,
        200);
    ukm_recorder.ExpectEntryMetric(
        ukm_entry,
        PageLoad::
            kLayoutInstability_MaxCumulativeShiftScore_SlidingWindow_Duration300msName,
        150);
    ukm_recorder.ExpectEntryMetric(
        ukm_entry,
        PageLoad::
            kLayoutInstability_AverageCumulativeShiftScore_SessionWindow_Gap5000msName,
        250);
    ukm_recorder.ExpectEntryMetric(
        ukm_entry,
        PageLoad::
            kLayoutInstability_MaxCumulativeShiftScore_SessionWindowByInputs_Gap1000ms_Max5000msName,
        150);
    ukm_recorder.ExpectEntryMetric(kv.second.get(),
                                   PageLoad::kNavigation_PageEndReason3Name,
                                   page_load_metrics::END_CLOSE);
  }

  EXPECT_THAT(tester()->histogram_tester().GetAllSamples(
                  "PageLoad.LayoutInstability.CumulativeShiftScore"),
              testing::ElementsAre(base::Bucket(25, 1)));
  EXPECT_THAT(tester()->histogram_tester().GetAllSamples(
                  "PageLoad.LayoutInstability.MaxCumulativeShiftScore."
                  "SessionWindow.Gap1000ms.Max5000ms"),
              testing::ElementsAre(base::Bucket(25, 1)));
  EXPECT_THAT(tester()->histogram_tester().GetAllSamples(
                  "PageLoad.LayoutInstability.MaxCumulativeShiftScore."
                  "SessionWindowByInputs.Gap1000ms.Max5000ms"),
              testing::ElementsAre(base::Bucket(15, 1)));
}

TEST_F(UkmPageLoadMetricsObserverTest, SiteInstanceRenderProcessAssignment) {
  NavigateAndCommit(GURL(kTestUrl1));

  // Simulate closing the tab.
  DeleteContents();

  const auto& ukm_recorder = tester()->test_ukm_recorder();
  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      ukm_recorder.GetMergedEntriesByName(PageLoad::kEntryName);
  EXPECT_EQ(1ul, merged_entries.size());

  for (const auto& kv : merged_entries) {
    const int64_t* metric = ukm_recorder.GetEntryMetric(
        kv.second.get(),
        ukm::builders::PageLoad::kSiteInstanceRenderProcessAssignmentName);
    EXPECT_TRUE(metric);
    EXPECT_NE(0u, *metric);
  }
}

TEST_F(UkmPageLoadMetricsObserverTest,
       PerfectHeuristicsDelayaAsyncScriptExecution) {
  NavigateAndCommit(GURL(kTestUrl1));

  page_load_metrics::mojom::FrameMetadata metadata;
  metadata.behavior_flags |= blink::LoadingBehaviorFlag::
      kLoadingBehaviorAsyncScriptReadyBeforeDocumentFinishedParsing;
  tester()->SimulateMetadataUpdate(metadata, web_contents()->GetMainFrame());

  // Simulate closing the tab.
  DeleteContents();

  auto entries = tester()->test_ukm_recorder().GetEntriesByName(
      ukm::builders::PerfectHeuristics::kEntryName);
  EXPECT_EQ(1ul, entries.size());
  tester()->test_ukm_recorder().ExpectEntryMetric(
      entries.front(),
      ukm::builders::PerfectHeuristics::
          kdelay_async_script_execution_before_finished_parsingName,
      1);
}

TEST_F(UkmPageLoadMetricsObserverTest,
       PerfectHeuristicsDelayaCompetingLowPriorityRequests) {
  NavigateAndCommit(GURL(kTestUrl1));

  page_load_metrics::mojom::FrameMetadata metadata;
  metadata.behavior_flags |= blink::LoadingBehaviorFlag::
      kLoadingBehaviorCompetingLowPriorityRequestsDelayed;
  tester()->SimulateMetadataUpdate(metadata, web_contents()->GetMainFrame());

  // Simulate closing the tab.
  DeleteContents();

  auto entries = tester()->test_ukm_recorder().GetEntriesByName(
      ukm::builders::PerfectHeuristics::kEntryName);
  EXPECT_EQ(1ul, entries.size());
  tester()->test_ukm_recorder().ExpectEntryMetric(
      entries.front(),
      ukm::builders::PerfectHeuristics::kDelayCompetingLowPriorityRequestsName,
      1);
}

TEST_F(UkmPageLoadMetricsObserverTest, MHTMLNotTrackedOfflinePreview) {
  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      GURL(kTestUrl1), web_contents());
  navigation->SetContentsMimeType("multipart/related");
  navigation->Commit();

  // Simulate closing the tab.
  DeleteContents();

  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      tester()->test_ukm_recorder().GetMergedEntriesByName(
          PageLoad::kEntryName);
  EXPECT_EQ(0ul, merged_entries.size());
}

TEST_F(UkmPageLoadMetricsObserverTest, LayoutInstabilitySubframeAggregation) {
  NavigateAndCommit(GURL(kTestUrl1));

  // Simulate layout instability in the main frame.
  page_load_metrics::mojom::FrameRenderDataUpdate render_data(1.0, 1.0, 0, 0, 0,
                                                              0, {}, {});
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

TEST_F(UkmPageLoadMetricsObserverTest, ThirdPartyCookieBlockingEnabled) {
  profile()->GetPrefs()->SetInteger(
      prefs::kCookieControlsMode,
      static_cast<int>(content_settings::CookieControlsMode::kBlockThirdParty));

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
  profile()->GetPrefs()->SetInteger(
      prefs::kCookieControlsMode,
      static_cast<int>(content_settings::CookieControlsMode::kBlockThirdParty));
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

TEST_F(UkmPageLoadMetricsObserverTest, NotSearchOrHomePage) {
  static const char kOtherURL[] = "https://www.other.com";

  NavigateAndCommit(GURL(kOtherURL));

  // Simulate closing the tab.
  DeleteContents();

  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      tester()->test_ukm_recorder().GetMergedEntriesByName(
          GeneratedNavigation::kEntryName);
  EXPECT_EQ(0ul, merged_entries.size());
}

TEST_F(UkmPageLoadMetricsObserverTest, HomePageReported) {
  static const char kOtherURL[] = "https://www.homepage.com/";

  Profile::FromBrowserContext(browser_context())
      ->GetPrefs()
      ->SetString(prefs::kHomePage, kOtherURL);

  NavigateAndCommit(GURL(kOtherURL));

  // Simulate closing the tab.
  DeleteContents();

  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      tester()->test_ukm_recorder().GetMergedEntriesByName(
          GeneratedNavigation::kEntryName);
  EXPECT_EQ(1ul, merged_entries.size());

  for (const auto& kv : merged_entries) {
    tester()->test_ukm_recorder().ExpectEntrySourceHasUrl(kv.second.get(),
                                                          GURL(kOtherURL));
    tester()->test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(),
        GeneratedNavigation::kFirstURLIsDefaultSearchEngineName, false);
    tester()->test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(),
        GeneratedNavigation::kFinalURLIsDefaultSearchEngineName, false);
    tester()->test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(), GeneratedNavigation::kFirstURLIsHomePageName, true);
    tester()->test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(), GeneratedNavigation::kFinalURLIsHomePageName, true);
  }
}

TEST_F(UkmPageLoadMetricsObserverTest, DefaultSearchReported) {
  static const char kShortName[] = "test";
  static const char kSearchURL[] =
      "https://www.searchurl.com/search?q={searchTerms}";
  static const char kSearchURLWithQuery[] =
      "https://www.searchurl.com/search?q=somequery";

  TemplateURLService* model = TemplateURLServiceFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context()));
  ASSERT_TRUE(model);
  search_test_utils::WaitForTemplateURLServiceToLoad(model);
  ASSERT_TRUE(model->loaded());

  TemplateURLData data;
  data.SetShortName(base::ASCIIToUTF16(kShortName));
  data.SetKeyword(data.short_name());
  data.SetURL(kSearchURL);

  // Set the DSE to the test URL.
  TemplateURL* template_url = model->Add(std::make_unique<TemplateURL>(data));
  ASSERT_TRUE(template_url);
  model->SetUserSelectedDefaultSearchProvider(template_url);

  NavigateAndCommit(GURL(kSearchURLWithQuery));

  // Simulate closing the tab.
  DeleteContents();

  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      tester()->test_ukm_recorder().GetMergedEntriesByName(
          GeneratedNavigation::kEntryName);
  EXPECT_EQ(1ul, merged_entries.size());

  for (const auto& kv : merged_entries) {
    tester()->test_ukm_recorder().ExpectEntrySourceHasUrl(
        kv.second.get(), GURL(kSearchURLWithQuery));
    tester()->test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(),
        GeneratedNavigation::kFirstURLIsDefaultSearchEngineName, true);
    tester()->test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(),
        GeneratedNavigation::kFinalURLIsDefaultSearchEngineName, true);
    tester()->test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(), GeneratedNavigation::kFirstURLIsHomePageName, false);
    tester()->test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(), GeneratedNavigation::kFinalURLIsHomePageName, false);
  }
}

TEST_F(UkmPageLoadMetricsObserverTest, NoLargestContentfulPaint) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kTestUrl1));
  tester()->SimulateTimingUpdate(timing);

  // Simulate closing the tab.
  DeleteContents();

  TestNoLCP(LargestContentState::kNotFound);
}

TEST_F(UkmPageLoadMetricsObserverTest, FCPHiddenWhileFlushing) {
  NavigateAndCommit(GURL(kTestUrl1));

  // Simulate hiding the tab.
  web_contents()->WasHidden();

  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.parse_timing->parse_start = base::TimeDelta();
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.paint_timing->first_contentful_paint =
      tester()->GetDelegateForCommittedLoad().GetFirstBackgroundTime();
  PopulateRequiredTimingFields(&timing);

  // Simulate FCP at the same time as the hide (but reported after).
  tester()->SimulateTimingUpdate(timing);

  const auto& ukm_recorder = tester()->test_ukm_recorder();
  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      ukm_recorder.GetMergedEntriesByName(PageLoad::kEntryName);
  EXPECT_EQ(1ul, merged_entries.size());

  // Check that we reported the FCP UKM.
  for (const auto& kv : merged_entries) {
    EXPECT_TRUE(tester()->test_ukm_recorder().EntryHasMetric(
        kv.second.get(),
        PageLoad::kPaintTiming_NavigationToFirstContentfulPaintName));
  }
}

TEST_F(UkmPageLoadMetricsObserverTest, LCPHiddenWhileFlushing) {
  NavigateAndCommit(GURL(kTestUrl1));

  // Simulate hiding the tab.
  web_contents()->WasHidden();
  base::TimeDelta background_time =
      *tester()->GetDelegateForCommittedLoad().GetFirstBackgroundTime();

  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.paint_timing->largest_contentful_paint->largest_image_paint =
      background_time;
  timing.paint_timing->largest_contentful_paint->largest_image_paint_size = 50u;
  PopulateExperimentalLCP(timing.paint_timing);
  PopulateRequiredTimingFields(&timing);

  // Simulate LCP at the same time as the hide (but reported after).
  tester()->SimulateTimingUpdate(timing);

  // Simulate closing the tab.
  DeleteContents();

  // Check that we reported the LCP UKM.
  TestLCP(background_time.InMilliseconds(), LargestContentType::kImage,
          true /* test_main_frame */);
}

TEST_F(UkmPageLoadMetricsObserverTest, AppEnterBackground) {
  NavigateAndCommit(GURL(kTestUrl1));
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);

  tester()->SimulateAppEnterBackground();

  const auto& ukm_recorder = tester()->test_ukm_recorder();
  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      ukm_recorder.GetMergedEntriesByName(PageLoad::kEntryName);
  EXPECT_EQ(1ul, merged_entries.size());
  const ukm::mojom::UkmEntry* entry = merged_entries.begin()->second.get();
  tester()->test_ukm_recorder().ExpectEntryMetric(
      entry, PageLoad::kNavigation_PageEndReason3Name,
      page_load_metrics::END_APP_ENTER_BACKGROUND);
}

TEST_F(UkmPageLoadMetricsObserverTest, IsExistingBookmark) {
  GURL url(kTestUrl1);

  bookmarks::BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(browser_context());
  ASSERT_TRUE(model);
  ASSERT_TRUE(
      model->AddURL(model->bookmark_bar_node(), 0, std::u16string(), url));

  NavigateAndCommit(url);

  // Simulate closing the tab.
  DeleteContents();

  const auto& ukm_recorder = tester()->test_ukm_recorder();
  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      ukm_recorder.GetMergedEntriesByName(PageLoad::kEntryName);
  EXPECT_EQ(1ul, merged_entries.size());
  const ukm::mojom::UkmEntry* entry = merged_entries.begin()->second.get();
  tester()->test_ukm_recorder().ExpectEntryMetric(
      entry, PageLoad::kIsExistingBookmarkName, 1);
  tester()->test_ukm_recorder().ExpectEntryMetric(
      entry, PageLoad::kIsNewBookmarkName, 0);
}

TEST_F(UkmPageLoadMetricsObserverTest, IsNewBookmark) {
  GURL url(kTestUrl1);

  ASSERT_TRUE(profile()->CreateHistoryService());
  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(profile(),
                                           ServiceAccessType::IMPLICIT_ACCESS);
  ASSERT_TRUE(history_service);
  history_service->AddPage(url, base::Time::Now(),
                           history::VisitSource::SOURCE_BROWSED);

  NavigateAndCommit(url);
  history::BlockUntilHistoryProcessesPendingRequests(history_service);

  bookmarks::BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(browser_context());
  ASSERT_TRUE(model);
  ASSERT_TRUE(
      model->AddURL(model->bookmark_bar_node(), 0, std::u16string(), url));

  // Simulate closing the tab.
  DeleteContents();

  const auto& ukm_recorder = tester()->test_ukm_recorder();
  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      ukm_recorder.GetMergedEntriesByName(PageLoad::kEntryName);
  EXPECT_EQ(1ul, merged_entries.size());
  const ukm::mojom::UkmEntry* entry = merged_entries.begin()->second.get();
  tester()->test_ukm_recorder().ExpectEntryMetric(
      entry, PageLoad::kIsExistingBookmarkName, 0);
  tester()->test_ukm_recorder().ExpectEntryMetric(
      entry, PageLoad::kIsNewBookmarkName, 1);
}

// Android does not have NTP Custom Links.
#if !defined(OS_ANDROID)
TEST_F(UkmPageLoadMetricsObserverTest, IsNTPCustomLink) {
  GURL url(kTestUrl1);

  ASSERT_TRUE(profile()->CreateHistoryService());
  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(profile(),
                                           ServiceAccessType::IMPLICIT_ACCESS);
  ASSERT_TRUE(history_service);
  history_service->AddPage(url, base::Time::Now(),
                           history::VisitSource::SOURCE_BROWSED);

  NavigateAndCommit(url);
  history::BlockUntilHistoryProcessesPendingRequests(history_service);

  ntp_tiles::CustomLinksStore custom_link_store(profile()->GetPrefs());
  custom_link_store.StoreLinks({
      {url, u"Test Title"},
  });

  // Simulate closing the tab.
  DeleteContents();

  const auto& ukm_recorder = tester()->test_ukm_recorder();
  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      ukm_recorder.GetMergedEntriesByName(PageLoad::kEntryName);
  EXPECT_EQ(1ul, merged_entries.size());
  const ukm::mojom::UkmEntry* entry = merged_entries.begin()->second.get();
  tester()->test_ukm_recorder().ExpectEntryMetric(
      entry, PageLoad::kIsNTPCustomLinkName, 1);
}
#endif  // !defined(OS_ANDROID)

TEST_F(UkmPageLoadMetricsObserverTest, DurationSinceLastVisitSeconds) {
  // TODO(tommycli): Should we move this test to either MemoriesService or
  // HistoryClustersTabHelper? On the one hand, the logic resides there.
  // On the other hand this serves as a good integration test with UKM.
  GURL url(kTestUrl1);

  ASSERT_TRUE(profile()->CreateHistoryService());
  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(profile(),
                                           ServiceAccessType::IMPLICIT_ACCESS);
  ASSERT_TRUE(history_service);
  // Fake that we visited this site 45 days ago.
  history_service->AddPage(url,
                           base::Time::Now() - base::TimeDelta::FromDays(45),
                           history::VisitSource::SOURCE_BROWSED);
  NavigateAndCommit(url);
  history::BlockUntilHistoryProcessesPendingRequests(history_service);

  // Simulate closing the tab.
  DeleteContents();

  // Verify UKM records that we visited the page clamped to 30 days ago to
  // respect the UKM retention period.
  const auto& ukm_recorder = tester()->test_ukm_recorder();
  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      ukm_recorder.GetMergedEntriesByName(PageLoad::kEntryName);
  EXPECT_EQ(1ul, merged_entries.size());
  const ukm::mojom::UkmEntry* entry = merged_entries.begin()->second.get();
  tester()->test_ukm_recorder().ExpectEntryMetric(
      entry, PageLoad::kDurationSinceLastVisitSecondsName,
      base::TimeDelta::FromDays(30).InSeconds());
}

TEST_F(UkmPageLoadMetricsObserverTest,
       DurationSinceLastVisitSecondsHistoryServiceLosesRace) {
  GURL url(kTestUrl1);

  // Simulate that we navigated, but HistoryService doesn't respond back to the
  // UKM observer before it's destroyed.
  NavigateAndCommit(url);
  DeleteContents();

  // Verify UKM records -1 in this case.
  const auto& ukm_recorder = tester()->test_ukm_recorder();
  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      ukm_recorder.GetMergedEntriesByName(PageLoad::kEntryName);
  EXPECT_EQ(1ul, merged_entries.size());
  const ukm::mojom::UkmEntry* entry = merged_entries.begin()->second.get();
  tester()->test_ukm_recorder().ExpectEntryMetric(
      entry, PageLoad::kDurationSinceLastVisitSecondsName, -1);
}

class TestOfflinePreviewsUkmPageLoadMetricsObserver
    : public UkmPageLoadMetricsObserver {
 public:
  explicit TestOfflinePreviewsUkmPageLoadMetricsObserver(
      MockNetworkQualityProvider* network_quality_provider)
      : UkmPageLoadMetricsObserver(network_quality_provider) {}

  ~TestOfflinePreviewsUkmPageLoadMetricsObserver() override = default;

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
        kv.second.get(), PageLoad::kNavigation_PageEndReason3Name,
        page_load_metrics::END_CLOSE);
  }
}

TEST_F(UkmPageLoadMetricsObserverTest, NavigationTiming) {
  GURL url(kTestUrl1);
  NavigateAndCommit(url);

  // Simulate closing the tab.
  DeleteContents();

  using ukm::builders::NavigationTiming;
  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      tester()->test_ukm_recorder().GetMergedEntriesByName(
          NavigationTiming::kEntryName);
  EXPECT_EQ(1ul, merged_entries.size());

  const std::vector<const char*> metrics = {
      NavigationTiming::kFirstRequestStartName,
      NavigationTiming::kFirstResponseStartName,
      NavigationTiming::kFirstLoaderCallbackName,
      NavigationTiming::kFinalRequestStartName,
      NavigationTiming::kFinalResponseStartName,
      NavigationTiming::kFinalLoaderCallbackName,
      NavigationTiming::kNavigationCommitSentName};

  for (const auto& kv : merged_entries) {
    tester()->test_ukm_recorder().ExpectEntrySourceHasUrl(kv.second.get(), url);

    // Verify if the elapsed times from the navigation start are recorded.
    for (const char* metric : metrics) {
      EXPECT_TRUE(tester()->test_ukm_recorder().EntryHasMetric(kv.second.get(),
                                                               metric));
    }
  }
}

TEST_F(UkmPageLoadMetricsObserverTest, CLSNeverForegroundedNoReport) {
  web_contents()->WasHidden();
  NavigateAndCommit(GURL(kTestUrl1));

  page_load_metrics::mojom::FrameRenderDataUpdate render_data(1.0, 1.0, 0, 0, 0,
                                                              0, {}, {});
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
    EXPECT_FALSE(ukm_recorder.EntryHasMetric(
        ukm_entry, PageLoad::kLayoutInstability_CumulativeShiftScoreName));
  }
}

class CLSUkmPageLoadMetricsObserverTest
    : public UkmPageLoadMetricsObserverTest {
 protected:
  void RunBeforeInputOrScrollCase(bool input_in_subframe);
  void SimulateShiftDelta(float delta, content::RenderFrameHost* frame);
  RenderFrameHost* NavigateSubframe();
  void VerifyUKMBuckets(int total, int before_input_or_scroll);
  void InitPageLoadTimingWithInputOrScroll(
      page_load_metrics::mojom::PageLoadTiming& timing,
      base::TimeDelta timestamp);
};

void CLSUkmPageLoadMetricsObserverTest::SimulateShiftDelta(
    float delta,
    content::RenderFrameHost* frame) {
  page_load_metrics::mojom::FrameRenderDataUpdate render_data(delta, delta, 0,
                                                              0, 0, 0, {}, {});
  tester()->SimulateRenderDataUpdate(render_data, frame);
}

RenderFrameHost* CLSUkmPageLoadMetricsObserverTest::NavigateSubframe() {
  return NavigationSimulator::NavigateAndCommitFromDocument(
      GURL(kSubframeTestUrl),
      RenderFrameHostTester::For(web_contents()->GetMainFrame())
          ->AppendChild("subframe"));
}

void CLSUkmPageLoadMetricsObserverTest::VerifyUKMBuckets(
    int total,
    int before_input_or_scroll) {
  const char* total_name =
      PageLoad::kLayoutInstability_CumulativeShiftScoreName;
  const char* before_input_or_scroll_name =
      PageLoad::kLayoutInstability_CumulativeShiftScore_BeforeInputOrScrollName;

  const auto& ukm_recorder = tester()->test_ukm_recorder();
  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      ukm_recorder.GetMergedEntriesByName(PageLoad::kEntryName);
  EXPECT_EQ(1ul, merged_entries.size());

  for (const auto& kv : merged_entries) {
    const ukm::mojom::UkmEntry* ukm_entry = kv.second.get();
    ukm_recorder.ExpectEntrySourceHasUrl(ukm_entry, GURL(kTestUrl1));

    ukm_recorder.ExpectEntryMetric(ukm_entry, total_name, total);
    ukm_recorder.ExpectEntryMetric(ukm_entry, before_input_or_scroll_name,
                                   before_input_or_scroll);
  }
}

void CLSUkmPageLoadMetricsObserverTest::InitPageLoadTimingWithInputOrScroll(
    page_load_metrics::mojom::PageLoadTiming& timing,
    base::TimeDelta timestamp) {
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  PopulateRequiredTimingFields(&timing);
  timing.paint_timing->first_input_or_scroll_notified_timestamp = timestamp;
}

void CLSUkmPageLoadMetricsObserverTest::RunBeforeInputOrScrollCase(
    bool input_in_subframe) {
  NavigateAndCommit(GURL(kTestUrl1));

  RenderFrameHost* main_frame = web_contents()->GetMainFrame();
  RenderFrameHost* subframe = NavigateSubframe();

  SimulateShiftDelta(1.0, main_frame);
  SimulateShiftDelta(1.5, subframe);

  // Simulate input.
  page_load_metrics::mojom::PageLoadTiming timing;
  InitPageLoadTimingWithInputOrScroll(timing, base::TimeDelta::FromSeconds(1));
  tester()->SimulateTimingUpdate(timing,
                                 input_in_subframe ? subframe : main_frame);

  SimulateShiftDelta(1.2, main_frame);
  SimulateShiftDelta(0.8, subframe);

  DeleteContents();

  // Total CLS: 1.0 + 1.5 + 1.2 + 0.8 = 4.5 (bucket 450).
  // Before input: 1.0 + 1.5 = 2.5 (bucket 250).
  VerifyUKMBuckets(450, 250);
}

TEST_F(CLSUkmPageLoadMetricsObserverTest, BeforeInputOrScroll_Main) {
  RunBeforeInputOrScrollCase(false);
}

TEST_F(CLSUkmPageLoadMetricsObserverTest, BeforeInputOrScroll_Sub) {
  RunBeforeInputOrScrollCase(true);
}

TEST_F(UkmPageLoadMetricsObserverTest, BucketWithOffsetAndUnit) {
  EXPECT_EQ(500, internal::BucketWithOffsetAndUnit(500, 500, 10));

  // large num
  EXPECT_EQ(500, internal::BucketWithOffsetAndUnit(501, 500, 10));
  EXPECT_EQ(510, internal::BucketWithOffsetAndUnit(510, 500, 10));
  EXPECT_EQ(520, internal::BucketWithOffsetAndUnit(525, 500, 10));
  EXPECT_EQ(540, internal::BucketWithOffsetAndUnit(550, 500, 10));
  EXPECT_EQ(820, internal::BucketWithOffsetAndUnit(1000, 500, 10));
  EXPECT_EQ(1780, internal::BucketWithOffsetAndUnit(2000, 500, 10));

  // small num
  EXPECT_EQ(500, internal::BucketWithOffsetAndUnit(499, 500, 10));
  EXPECT_EQ(490, internal::BucketWithOffsetAndUnit(490, 500, 10));
  EXPECT_EQ(480, internal::BucketWithOffsetAndUnit(475, 500, 10));
  EXPECT_EQ(460, internal::BucketWithOffsetAndUnit(450, 500, 10));
  EXPECT_EQ(180, internal::BucketWithOffsetAndUnit(100, 500, 10));
  EXPECT_EQ(180, internal::BucketWithOffsetAndUnit(0, 500, 10));

  // different offset
  EXPECT_EQ(1000, internal::BucketWithOffsetAndUnit(1000, 1000, 10));
  EXPECT_EQ(1010, internal::BucketWithOffsetAndUnit(1010, 1000, 10));
  EXPECT_EQ(1080, internal::BucketWithOffsetAndUnit(1100, 1000, 10));

  // different unit
  EXPECT_EQ(1000, internal::BucketWithOffsetAndUnit(1000, 1000, 100));
  EXPECT_EQ(1000, internal::BucketWithOffsetAndUnit(1010, 1000, 100));
  EXPECT_EQ(1100, internal::BucketWithOffsetAndUnit(1100, 1000, 100));
}
