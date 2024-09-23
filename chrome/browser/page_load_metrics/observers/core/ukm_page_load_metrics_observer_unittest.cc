// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/core/ukm_page_load_metrics_observer.h"

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/metrics/metrics_hashes.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/test_trace_processor.h"
#include "base/test/trace_event_analyzer.h"
#include "base/time/time.h"
#include "base/trace_event/traced_value.h"
#include "build/build_config.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/history_tab_helper.h"
#include "chrome/browser/history_clusters/history_clusters_service_factory.h"
#include "chrome/browser/history_clusters/history_clusters_tab_helper.h"
#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"
#include "chrome/browser/performance_manager/test_support/test_user_performance_tuning_manager_environment.h"
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
#include "components/history_clusters/core/history_clusters_service.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/ntp_tiles/custom_links_store.h"
#include "components/page_load_metrics/browser/observers/core/largest_contentful_paint_handler.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "components/page_load_metrics/common/page_load_metrics.mojom.h"
#include "components/page_load_metrics/common/page_visit_final_status.h"
#include "components/page_load_metrics/common/test/page_load_metrics_test_util.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/navigation_simulator.h"
#include "net/base/ip_endpoint.h"
#include "net/nqe/effective_connection_type.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "services/network/public/cpp/network_quality_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/performance/largest_contentful_paint_type.h"
#include "third_party/metrics_proto/system_profile.pb.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"

using content::NavigationSimulator;
using content::RenderFrameHost;
using content::RenderFrameHostTester;
using page_load_metrics::PageVisitFinalStatus;
using testing::AnyNumber;
using testing::Mock;
using testing::Return;
using UserInteractionLatenciesPtr =
    page_load_metrics::mojom::UserInteractionLatenciesPtr;
using UserInteractionLatencies =
    page_load_metrics::mojom::UserInteractionLatencies;
using UserInteractionLatency = page_load_metrics::mojom::UserInteractionLatency;
using UserInteractionType = page_load_metrics::mojom::UserInteractionType;

namespace {

using GeneratedNavigation = ukm::builders::GeneratedNavigation;
using LargestContentState =
    page_load_metrics::PageLoadMetricsObserver::LargestContentState;
using LargestContentTextOrImage =
    page_load_metrics::ContentfulPaintTimingInfo::LargestContentTextOrImage;
using PageLoad = ukm::builders::PageLoad;
using PageLoad_Internal = ukm::builders::PageLoad_Internal;
using UserPerceivedPageVisit = ukm::builders::UserPerceivedPageVisit;

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
  template <typename... TaskEnvironmentTraits>
  explicit UkmPageLoadMetricsObserverTest(TaskEnvironmentTraits&&... traits)
      : page_load_metrics::PageLoadMetricsObserverTestHarness(
            std::forward<TaskEnvironmentTraits>(traits)...) {}

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

    bookmarks::BookmarkModel* bookmark_model =
        BookmarkModelFactory::GetForBrowserContext(profile());
    bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model);

    HistoryTabHelper::CreateForWebContents(web_contents());
    HistoryTabHelper::FromWebContents(web_contents())
        ->SetForceEligibleTabForTesting(true);

    HistoryClustersTabHelper::CreateForWebContents(web_contents());
  }

  TestingProfile::TestingFactories GetTestingFactories() const override {
    return {
        TestingProfile::TestingFactory{
            BookmarkModelFactory::GetInstance(),
            BookmarkModelFactory::GetDefaultFactory()},
        TestingProfile::TestingFactory{
            HistoryServiceFactory::GetInstance(),
            HistoryServiceFactory::GetDefaultFactory()},
        TestingProfile::TestingFactory{
            TemplateURLServiceFactory::GetInstance(),
            base::BindRepeating(&TemplateURLServiceFactory::BuildInstanceFor)}};
  }

  MockNetworkQualityProvider& mock_network_quality_provider() {
    return mock_network_quality_provider_;
  }

  // Tests that LCP reports the given |value|,
  // and tests that the LCP content type reported is |text_or_image|. If
  // |test_main_frame| is set, also tests that the main frame LCP histograms
  // also report |value|. If |text_or_image| is kText, then tests that image
  // BPP is not reported, and otherwise tests that it matches |bpp_bucket|.
  void TestLCP(
      int value,
      LargestContentTextOrImage text_or_image,
      bool test_main_frame,
      uint32_t bpp_bucket = 0,
      std::optional<net::RequestPriority> request_priority = std::nullopt,
      blink::LargestContentfulPaintType type =
          blink::LargestContentfulPaintType::kNone) {
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
    if (test_main_frame) {
      tester()->test_ukm_recorder().ExpectEntryMetric(
          entry,
          PageLoad::
              kPaintTiming_NavigationToLargestContentfulPaint2_MainFrameName,
          value);
    }
    EXPECT_TRUE(tester()->test_ukm_recorder().EntryHasMetric(
        entry, PageLoad::kPageTiming_ForegroundDurationName));

    if (text_or_image == LargestContentTextOrImage::kText) {
      EXPECT_FALSE(tester()->test_ukm_recorder().EntryHasMetric(
          entry, PageLoad::kPaintTiming_LargestContentfulPaintBPPName));
      EXPECT_FALSE(tester()->test_ukm_recorder().EntryHasMetric(
          entry,
          PageLoad::kPaintTiming_LargestContentfulPaintRequestPriorityName));
    } else {
      tester()->test_ukm_recorder().ExpectEntryMetric(
          entry, PageLoad::kPaintTiming_LargestContentfulPaintBPPName,
          bpp_bucket);

      if (request_priority) {
        tester()->test_ukm_recorder().ExpectEntryMetric(
            entry,
            PageLoad::kPaintTiming_LargestContentfulPaintRequestPriorityName,
            *request_priority);
      } else {
        EXPECT_FALSE(tester()->test_ukm_recorder().EntryHasMetric(
            entry,
            PageLoad::kPaintTiming_LargestContentfulPaintRequestPriorityName));
      }
    }

    tester()->test_ukm_recorder().ExpectEntryMetric(
        entry, PageLoad::kPaintTiming_LargestContentfulPaintTypeName,
        LargestContentfulPaintTypeToUKMFlags(type));

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
        static_cast<int>(text_or_image));
    tester()->test_ukm_recorder().ExpectEntryMetric(
        internal_entry,
        PageLoad_Internal::
            kPaintTiming_LargestContentfulPaint_TerminationStateName,
        static_cast<int>(LargestContentState::kReported));
  }

  // Tests that the main frame LCP reports the given |value| as its timestamp.
  void TestMainFrameLCPTimestamp(int value) {
    std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
        tester()->test_ukm_recorder().GetMergedEntriesByName(
            PageLoad::kEntryName);
    EXPECT_EQ(1ul, merged_entries.size());

    const ukm::mojom::UkmEntry* entry = merged_entries.begin()->second.get();
    tester()->test_ukm_recorder().ExpectEntryMetric(
        entry,
        PageLoad::
            kPaintTiming_NavigationToLargestContentfulPaint2_MainFrameName,
        value);
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
        entry,
        PageLoad::
            kPaintTiming_NavigationToLargestContentfulPaint2_MainFrameName));

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
    tester()->test_ukm_recorder().ExpectEntryMetric(
        internal_entry,
        PageLoad_Internal::
            kPaintTiming_LargestContentfulPaint_TerminationStateName,
        static_cast<int>(state));
  }

  UkmPageLoadMetricsObserver* observer() const { return observer_; }

 private:
  raw_ptr<UkmPageLoadMetricsObserver, DanglingUntriaged>
      observer_;  // Non-owning raw pointer.

  MockNetworkQualityProvider mock_network_quality_provider_;
};

class UkmPageLoadMetricsObserverWithMockTimeTest
    : public UkmPageLoadMetricsObserverTest {
 protected:
  UkmPageLoadMetricsObserverWithMockTimeTest()
      : UkmPageLoadMetricsObserverTest(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
};

TEST_F(UkmPageLoadMetricsObserverTest, NoMetrics) {
  EXPECT_EQ(0ul, tester()->test_ukm_recorder().sources_count());
  EXPECT_EQ(0ul, tester()->test_ukm_recorder().entries_count());
}

TEST_F(UkmPageLoadMetricsObserverTest, Basic) {
  // PageLoadTiming with all recorded metrics other than FMP. This allows us to
  // verify both that all metrics are logged, and that we don't log metrics that
  // aren't present in the PageLoadTiming struct.
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.parse_timing->parse_start = base::Milliseconds(100);
  timing.document_timing->dom_content_loaded_event_start =
      base::Milliseconds(200);
  timing.paint_timing->first_paint = base::Milliseconds(250);
  timing.paint_timing->first_contentful_paint = base::Milliseconds(300);
  timing.document_timing->load_event_start = base::Milliseconds(500);
  timing.input_to_navigation_start = base::Milliseconds(50);
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
    EXPECT_TRUE(tester()->test_ukm_recorder().EntryHasMetric(
        kv.second.get(), PageLoad::kPageTiming_ForegroundDurationName));
    EXPECT_FALSE(tester()->test_ukm_recorder().EntryHasMetric(
        kv.second.get(), PageLoad::kWasDiscardedName));
    EXPECT_FALSE(tester()->test_ukm_recorder().EntryHasMetric(
        kv.second.get(), PageLoad::kRefreshRateThrottledName));
  }
  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> pagevisit_entries =
      tester()->test_ukm_recorder().GetMergedEntriesByName(
          ukm::builders::UserPerceivedPageVisit::kEntryName);
  EXPECT_EQ(1ul, pagevisit_entries.size());
  for (const auto& kv : pagevisit_entries) {
    tester()->test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(),
        ukm::builders::UserPerceivedPageVisit::kUserInitiatedName, true);
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

TEST_F(UkmPageLoadMetricsObserverTest, LargestImagePaint) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.paint_timing->largest_contentful_paint->largest_image_paint =
      base::Milliseconds(600);
  timing.paint_timing->largest_contentful_paint->largest_image_paint_size = 50u;
  timing.paint_timing->largest_contentful_paint->image_bpp = 8.5;
  timing.paint_timing->largest_contentful_paint->image_request_priority_valid =
      true;
  timing.paint_timing->largest_contentful_paint->image_request_priority_value =
      net::RequestPriority::MEDIUM;
  PopulateExperimentalLCP(timing.paint_timing);
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kTestUrl1));
  tester()->SimulateTimingUpdate(timing);

  // Simulate closing the tab.
  DeleteContents();

  TestLCP(600, LargestContentTextOrImage::kImage, true /* test_main_frame */,
          30 /* image_bpp = "8.0 - 9.0" */, net::RequestPriority::MEDIUM);
}

TEST_F(UkmPageLoadMetricsObserverTest, LargestImagePaintCrossOrigin) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.paint_timing->largest_contentful_paint->largest_image_paint =
      base::Milliseconds(600);
  timing.paint_timing->largest_contentful_paint->largest_image_paint_size = 50u;
  timing.paint_timing->largest_contentful_paint->image_bpp = 8.5;
  timing.paint_timing->largest_contentful_paint->type =
      blink::LargestContentfulPaintTypeToUKMFlags(
          blink::LargestContentfulPaintType::kImage |
          blink::LargestContentfulPaintType::kCrossOrigin);
  timing.paint_timing->largest_contentful_paint->image_request_priority_valid =
      true;
  timing.paint_timing->largest_contentful_paint->image_request_priority_value =
      net::RequestPriority::MEDIUM;
  PopulateExperimentalLCP(timing.paint_timing);
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kTestUrl1));
  tester()->SimulateTimingUpdate(timing);

  // Simulate closing the tab.
  DeleteContents();

  TestLCP(600, LargestContentTextOrImage::kImage, true /* test_main_frame */,
          30 /* image_bpp = "8.0 - 9.0" */, net::RequestPriority::MEDIUM,
          blink::LargestContentfulPaintType::kImage |
              blink::LargestContentfulPaintType::kCrossOrigin);
}

TEST_F(UkmPageLoadMetricsObserverTest, LargestImagePaintVideo) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.paint_timing->largest_contentful_paint->largest_image_paint =
      base::Milliseconds(600);
  timing.paint_timing->largest_contentful_paint->largest_image_paint_size = 50u;
  timing.paint_timing->largest_contentful_paint->type =
      blink::LargestContentfulPaintTypeToUKMFlags(
          blink::LargestContentfulPaintType::kVideo);
  timing.paint_timing->largest_contentful_paint->image_bpp = 8.5;
  timing.paint_timing->largest_contentful_paint->image_request_priority_valid =
      false;
  PopulateExperimentalLCP(timing.paint_timing);
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kTestUrl1));
  tester()->SimulateTimingUpdate(timing);

  // Simulate closing the tab.
  DeleteContents();

  // `image_request_priority_valid` is unset above for video, so no priorities
  // are reported.
  TestLCP(600, LargestContentTextOrImage::kImage, true /* test_main_frame */,
          30 /* image_bpp = "8.0 - 9.0" */, std::nullopt /* request_priority */,
          blink::LargestContentfulPaintType::kVideo);
}

TEST_F(UkmPageLoadMetricsObserverTest, LargestImagePaintAnimated) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.paint_timing->largest_contentful_paint->largest_image_paint =
      base::Milliseconds(600);
  timing.paint_timing->largest_contentful_paint->largest_image_paint_size = 50u;
  timing.paint_timing->largest_contentful_paint->type =
      blink::LargestContentfulPaintTypeToUKMFlags(
          blink::LargestContentfulPaintType::kAnimatedImage);
  timing.paint_timing->largest_contentful_paint->image_bpp = 8.5;
  timing.paint_timing->largest_contentful_paint->image_request_priority_valid =
      true;
  timing.paint_timing->largest_contentful_paint->image_request_priority_value =
      net::RequestPriority::MEDIUM;
  PopulateExperimentalLCP(timing.paint_timing);
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kTestUrl1));
  tester()->SimulateTimingUpdate(timing);

  // Simulate closing the tab.
  DeleteContents();

  TestLCP(600, LargestContentTextOrImage::kImage, true /* test_main_frame */,
          30 /* image_bpp = "8.0 - 9.0" */, net::RequestPriority::MEDIUM,
          blink::LargestContentfulPaintType::kAnimatedImage);
}

// Test that when the main frame and a subframe both have LCP candidates, and
// the subframe's image is larger, that all of the values in the merged LCP are
// taken from the subframe. The main frame LCP entry should still reflect the
// timing of the main frame's image.
TEST_F(UkmPageLoadMetricsObserverTest, LargestImagePaintFromSubframeMerged) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.paint_timing->largest_contentful_paint->largest_image_paint =
      base::Milliseconds(600);
  timing.paint_timing->largest_contentful_paint->largest_image_paint_size = 50u;
  timing.paint_timing->largest_contentful_paint->type =
      blink::LargestContentfulPaintTypeToUKMFlags(
          blink::LargestContentfulPaintType::kNone);
  timing.paint_timing->largest_contentful_paint->image_bpp = 8.5;
  timing.paint_timing->largest_contentful_paint->image_request_priority_valid =
      true;
  timing.paint_timing->largest_contentful_paint->image_request_priority_value =
      net::RequestPriority::MEDIUM;
  PopulateExperimentalLCP(timing.paint_timing);
  PopulateRequiredTimingFields(&timing);

  page_load_metrics::mojom::PageLoadTiming subframe_timing;
  page_load_metrics::InitPageLoadTimingForTest(&subframe_timing);
  subframe_timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(2);
  subframe_timing.paint_timing->largest_contentful_paint->largest_image_paint =
      base::Milliseconds(4780);
  subframe_timing.paint_timing->largest_contentful_paint
      ->largest_image_paint_size = 100u;
  subframe_timing.paint_timing->largest_contentful_paint->type =
      blink::LargestContentfulPaintTypeToUKMFlags(
          blink::LargestContentfulPaintType::kAnimatedImage);
  subframe_timing.paint_timing->largest_contentful_paint->image_bpp = 1.5;
  subframe_timing.paint_timing->largest_contentful_paint
      ->image_request_priority_valid = true;
  subframe_timing.paint_timing->largest_contentful_paint
      ->image_request_priority_value = net::RequestPriority::LOW;
  PopulateExperimentalLCP(subframe_timing.paint_timing);
  PopulateRequiredTimingFields(&subframe_timing);

  // Commit the main frame and a subframe.
  NavigateAndCommit(GURL(kTestUrl1));
  RenderFrameHost* subframe =
      NavigationSimulator::NavigateAndCommitFromDocument(
          GURL(kSubframeTestUrl),
          RenderFrameHostTester::For(web_contents()->GetPrimaryMainFrame())
              ->AppendChild("subframe"));

  // Simulate timing updates in the main frame and the subframe.
  tester()->SimulateTimingUpdate(timing);
  tester()->SimulateTimingUpdate(subframe_timing, subframe);

  // Simulate closing the tab.
  DeleteContents();

  // Test that the largest paint came from the subframe. Type and bpp fields
  // should be set correctly. Don't check main frame here, as the timing will be
  // different for that paint.
  TestLCP(4780, LargestContentTextOrImage::kImage, false /* test_main_frame */,
          23 /* image_bpp = "1.0 - 2.0" */, net::RequestPriority::LOW,
          blink::LargestContentfulPaintType::kAnimatedImage);

  // Test that the main frame largest paint is also correct.
  TestMainFrameLCPTimestamp(600);
}

// Test that when the main frame and a subframe both have LCP candidates, and
// the main frame's image is larger, that all of the values in the merged LCP
// are taken from the main frame.
TEST_F(UkmPageLoadMetricsObserverTest, LargestImagePaintFromMainFrameMerged) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.paint_timing->largest_contentful_paint->largest_image_paint =
      base::Milliseconds(600);
  timing.paint_timing->largest_contentful_paint->largest_image_paint_size =
      100u;
  timing.paint_timing->largest_contentful_paint->type =
      blink::LargestContentfulPaintTypeToUKMFlags(
          blink::LargestContentfulPaintType::kNone);
  timing.paint_timing->largest_contentful_paint->image_bpp = 8.5;
  timing.paint_timing->largest_contentful_paint->image_request_priority_valid =
      true;
  timing.paint_timing->largest_contentful_paint->image_request_priority_value =
      net::RequestPriority::MEDIUM;
  PopulateExperimentalLCP(timing.paint_timing);
  PopulateRequiredTimingFields(&timing);

  page_load_metrics::mojom::PageLoadTiming subframe_timing;
  page_load_metrics::InitPageLoadTimingForTest(&subframe_timing);
  subframe_timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(2);
  subframe_timing.paint_timing->largest_contentful_paint->largest_image_paint =
      base::Milliseconds(4780);
  subframe_timing.paint_timing->largest_contentful_paint
      ->largest_image_paint_size = 50u;
  subframe_timing.paint_timing->largest_contentful_paint->type =
      blink::LargestContentfulPaintTypeToUKMFlags(
          blink::LargestContentfulPaintType::kAnimatedImage);
  subframe_timing.paint_timing->largest_contentful_paint->image_bpp = 1.5;
  subframe_timing.paint_timing->largest_contentful_paint
      ->image_request_priority_valid = true;
  subframe_timing.paint_timing->largest_contentful_paint
      ->image_request_priority_value = net::RequestPriority::LOW;
  PopulateExperimentalLCP(subframe_timing.paint_timing);
  PopulateRequiredTimingFields(&subframe_timing);

  // Commit the main frame and a subframe.
  NavigateAndCommit(GURL(kTestUrl1));
  RenderFrameHost* subframe =
      NavigationSimulator::NavigateAndCommitFromDocument(
          GURL(kSubframeTestUrl),
          RenderFrameHostTester::For(web_contents()->GetPrimaryMainFrame())
              ->AppendChild("subframe"));

  // Simulate timing updates in the main frame and the subframe.
  tester()->SimulateTimingUpdate(timing);
  tester()->SimulateTimingUpdate(subframe_timing, subframe);

  // Simulate closing the tab.
  DeleteContents();

  // Test that the largest paint came from the main frame. Type and bpp fields
  // should be set correctly. Verify that the main frame LCP timing is also set
  // correctly.
  TestLCP(600, LargestContentTextOrImage::kImage, true /* test_main_frame */,
          30 /* image_bpp = "8.0 - 9.0" */, net::RequestPriority::MEDIUM,
          blink::LargestContentfulPaintType::kNone);
}

TEST_F(UkmPageLoadMetricsObserverTest, LargestImageLoading) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  // The largest image is loading so its paint time is set to TimeDelta().
  timing.paint_timing->largest_contentful_paint->largest_image_paint =
      base::TimeDelta();
  timing.paint_timing->largest_contentful_paint->largest_image_paint_size = 50u;
  // There is a text paint, but it must be ignored because it is smaller than
  // the image paint.
  timing.paint_timing->largest_contentful_paint->largest_text_paint =
      base::Milliseconds(600);
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
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  // Largest image is loading.
  timing.paint_timing->largest_contentful_paint->largest_image_paint =
      base::TimeDelta();
  timing.paint_timing->largest_contentful_paint->largest_image_paint_size = 50u;
  // Largest text is larger than image.
  timing.paint_timing->largest_contentful_paint->largest_text_paint =
      base::Milliseconds(600);
  timing.paint_timing->largest_contentful_paint->largest_text_paint_size = 80u;
  PopulateExperimentalLCP(timing.paint_timing);
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kTestUrl1));
  tester()->SimulateTimingUpdate(timing);

  // Simulate closing the tab.
  DeleteContents();

  TestLCP(600, LargestContentTextOrImage::kText, true /* test_main_frame */);
}

TEST_F(UkmPageLoadMetricsObserverTest,
       LargestImagePaint_DiscardBackgroundResult) {
  std::unique_ptr<base::SimpleTestClock> mock_clock(
      new base::SimpleTestClock());
  mock_clock->SetNow(base::Time::NowFromSystemTime());

  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.paint_timing->largest_contentful_paint->largest_image_paint =
      base::Milliseconds(600);
  PopulateRequiredTimingFields(&timing);
  // The duration between nav start and first background set to 1ms.
  mock_clock->Advance(base::Milliseconds(1));
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
  mock_clock->Advance(base::Milliseconds(1));
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
      entry, PageLoad::kPageVisitFinalStatusName,
      static_cast<int64_t>(PageVisitFinalStatus::kNeverForegrounded));
  tester()->test_ukm_recorder().ExpectEntryMetric(
      entry, PageLoad::kNavigation_PageTransitionName,
      ui::PAGE_TRANSITION_LINK);
  tester()->test_ukm_recorder().ExpectEntryMetric(
      entry, PageLoad::kPageTiming_TotalForegroundDurationName, 0);
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
      base::Seconds(10);
  timing.paint_timing->largest_contentful_paint->largest_text_paint =
      base::Seconds(10);
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
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);

  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kTestUrl1));
  timing.paint_timing->largest_contentful_paint->largest_image_paint =
      base::Milliseconds(60);
  timing.paint_timing->largest_contentful_paint->largest_image_paint_size = 10u;
  timing.paint_timing->largest_contentful_paint->largest_text_paint =
      base::Milliseconds(60);
  timing.paint_timing->largest_contentful_paint->largest_text_paint_size = 10u;
  PopulateExperimentalLCP(timing.paint_timing);
  tester()->SimulateTimingUpdate(timing);

  timing.paint_timing->largest_contentful_paint->largest_image_paint =
      base::Milliseconds(600);
  timing.paint_timing->largest_contentful_paint->largest_image_paint_size = 10u;
  timing.paint_timing->largest_contentful_paint->largest_text_paint =
      base::Milliseconds(600);
  timing.paint_timing->largest_contentful_paint->largest_text_paint_size = 10u;
  PopulateExperimentalLCP(timing.paint_timing);
  tester()->SimulateTimingUpdate(timing);

  // Simulate closing the tab.
  DeleteContents();

  // There's a tie here and in this case the LCP handler returns image as the
  // type.
  TestLCP(600, LargestContentTextOrImage::kImage, true /* test_main_frame */);
}

TEST_F(UkmPageLoadMetricsObserverTest, LargestTextPaint) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.paint_timing->largest_contentful_paint->largest_text_paint =
      base::Milliseconds(600);
  timing.paint_timing->largest_contentful_paint->largest_text_paint_size = 50u;
  PopulateExperimentalLCP(timing.paint_timing);
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kTestUrl1));
  tester()->SimulateTimingUpdate(timing);

  // Simulate closing the tab.
  DeleteContents();

  TestLCP(600, LargestContentTextOrImage::kText, true /* test_main_frame */);
}

TEST_F(UkmPageLoadMetricsObserverTest, LargestContentfulPaint_Trace) {
  // TODO(crbug.com/40801822): Improve unit tests support for tracing.
  // In particular, the initialization call below is most likely too narrow /
  // doesn't take care of everything that is needed.  In the future we might
  // need to 1) initialize tracing from a better place (maybe
  // RenderViewHostTestEnabler) and 2) initialize more broadly (maybe via
  // tracing::PerfettoTracedProcess::SetupForTesting method once it is
  // reintroduced).
  perfetto::internal::TrackRegistry::InitializeInstance();

  using trace_analyzer::Query;
  trace_analyzer::Start("*");
  {
    page_load_metrics::mojom::PageLoadTiming timing;
    page_load_metrics::InitPageLoadTimingForTest(&timing);
    timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
    timing.paint_timing->largest_contentful_paint->largest_text_paint =
        base::Milliseconds(600);
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
  ASSERT_TRUE(events[0]->HasDictArg("data"));
  base::Value::Dict arg = events[0]->GetKnownArgAsDict("data");
  int time = arg.FindInt("durationInMilliseconds").value_or(0);
  EXPECT_EQ(600, time);
  int size = arg.FindInt("size").value_or(0);
  EXPECT_EQ(1000, size);
  const std::string* type = arg.FindString("type");
  ASSERT_TRUE(type);
  EXPECT_EQ("text", *type);
}

TEST_F(UkmPageLoadMetricsObserverTest,
       LargestContentfulPaint_Trace_InvalidateCandidate) {
  using trace_analyzer::Query;
  trace_analyzer::Start("loading");
  {
    page_load_metrics::mojom::PageLoadTiming timing;
    page_load_metrics::InitPageLoadTimingForTest(&timing);
    timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
    timing.paint_timing->largest_contentful_paint->largest_text_paint =
        base::Milliseconds(600);
    timing.paint_timing->largest_contentful_paint->largest_text_paint_size =
        1000;
    PopulateRequiredTimingFields(&timing);

    NavigateAndCommit(GURL(kTestUrl1));
    tester()->SimulateTimingUpdate(timing);

    timing.paint_timing->largest_contentful_paint->largest_text_paint =
        std::optional<base::TimeDelta>();
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
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.paint_timing->largest_contentful_paint->largest_text_paint =
      base::Milliseconds(600);
  timing.paint_timing->largest_contentful_paint->largest_text_paint_size = 1000;
  PopulateExperimentalLCP(timing.paint_timing);
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kTestUrl1));
  tester()->SimulateTimingUpdate(timing);

  // Simulate closing the tab.
  DeleteContents();

  TestLCP(600, LargestContentTextOrImage::kText, true /* test_main_frame */);
}

TEST_F(UkmPageLoadMetricsObserverTest, LargestContentfulPaint_OnlyImage) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.paint_timing->largest_contentful_paint->largest_image_paint =
      base::Milliseconds(600);
  timing.paint_timing->largest_contentful_paint->largest_image_paint_size =
      1000;
  PopulateExperimentalLCP(timing.paint_timing);
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kTestUrl1));
  tester()->SimulateTimingUpdate(timing);

  // Simulate closing the tab.
  DeleteContents();

  TestLCP(600, LargestContentTextOrImage::kImage, true /* test_main_frame */);
}

TEST_F(UkmPageLoadMetricsObserverTest,
       LargestContentfulPaint_ImageLargerThanText) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.paint_timing->largest_contentful_paint->largest_image_paint =
      base::Milliseconds(600);
  timing.paint_timing->largest_contentful_paint->largest_image_paint_size =
      1000;
  timing.paint_timing->largest_contentful_paint->largest_text_paint =
      base::Milliseconds(1000);
  timing.paint_timing->largest_contentful_paint->largest_text_paint_size = 500;
  PopulateExperimentalLCP(timing.paint_timing);
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kTestUrl1));
  tester()->SimulateTimingUpdate(timing);

  // Simulate closing the tab.
  DeleteContents();

  TestLCP(600, LargestContentTextOrImage::kImage, true /* test_main_frame */);
}

TEST_F(UkmPageLoadMetricsObserverTest,
       LargestContentfulPaintAllFrames_OnlySubframe) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  PopulateRequiredTimingFields(&timing);

  page_load_metrics::mojom::PageLoadTiming subframe_timing;
  page_load_metrics::InitPageLoadTimingForTest(&subframe_timing);
  subframe_timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(2);
  subframe_timing.paint_timing->largest_contentful_paint->largest_image_paint =
      base::Milliseconds(4780);
  subframe_timing.paint_timing->largest_contentful_paint
      ->largest_image_paint_size = 100u;
  PopulateExperimentalLCP(subframe_timing.paint_timing);
  PopulateRequiredTimingFields(&subframe_timing);

  // Commit the main frame and a subframe.
  NavigateAndCommit(GURL(kTestUrl1));
  RenderFrameHost* subframe =
      NavigationSimulator::NavigateAndCommitFromDocument(
          GURL(kSubframeTestUrl),
          RenderFrameHostTester::For(web_contents()->GetPrimaryMainFrame())
              ->AppendChild("subframe"));

  // Simulate timing updates in the main frame and the subframe.
  tester()->SimulateTimingUpdate(timing);
  tester()->SimulateTimingUpdate(subframe_timing, subframe);

  // Simulate closing the tab.
  DeleteContents();

  TestLCP(4780, LargestContentTextOrImage::kImage, false /* test_main_frame */);

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
}

TEST_F(UkmPageLoadMetricsObserverTest,
       LargestContentfulPaintAllFrames_SubframeImageLoading) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  PopulateRequiredTimingFields(&timing);

  page_load_metrics::mojom::PageLoadTiming subframe_timing;
  page_load_metrics::InitPageLoadTimingForTest(&subframe_timing);
  subframe_timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(2);
  // Subframe's largest image is still loading.
  subframe_timing.paint_timing->largest_contentful_paint->largest_image_paint =
      base::TimeDelta();
  subframe_timing.paint_timing->largest_contentful_paint
      ->largest_image_paint_size = 100u;
  // Subframe has text but it should be ignored as it's smaller than image.
  subframe_timing.paint_timing->largest_contentful_paint->largest_text_paint =
      base::Milliseconds(3000);
  subframe_timing.paint_timing->largest_contentful_paint
      ->largest_text_paint_size = 80u;
  PopulateExperimentalLCP(subframe_timing.paint_timing);
  PopulateRequiredTimingFields(&subframe_timing);

  // Commit the main frame and a subframe.
  NavigateAndCommit(GURL(kTestUrl1));
  RenderFrameHost* subframe =
      NavigationSimulator::NavigateAndCommitFromDocument(
          GURL(kSubframeTestUrl),
          RenderFrameHostTester::For(web_contents()->GetPrimaryMainFrame())
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
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.paint_timing->largest_contentful_paint->largest_image_paint =
      base::Milliseconds(4780);
  timing.paint_timing->largest_contentful_paint->largest_image_paint_size =
      100u;
  PopulateExperimentalLCP(timing.paint_timing);
  PopulateRequiredTimingFields(&timing);

  page_load_metrics::mojom::PageLoadTiming subframe_timing;
  page_load_metrics::InitPageLoadTimingForTest(&subframe_timing);
  subframe_timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(2);
  PopulateRequiredTimingFields(&subframe_timing);

  // Commit the main frame and a subframe.
  NavigateAndCommit(GURL(kTestUrl1));
  RenderFrameHost* subframe =
      NavigationSimulator::NavigateAndCommitFromDocument(
          GURL(kSubframeTestUrl),
          RenderFrameHostTester::For(web_contents()->GetPrimaryMainFrame())
              ->AppendChild("subframe"));

  // Simulate timing updates in the main frame and the subframe.
  tester()->SimulateTimingUpdate(timing);
  tester()->SimulateTimingUpdate(subframe_timing, subframe);

  // Simulate closing the tab.
  DeleteContents();

  TestLCP(4780, LargestContentTextOrImage::kImage, true /* test_main_frame */);
}

TEST_F(UkmPageLoadMetricsObserverTest,
       LargestContentfulPaintAllFrames_CrossSiteSubFrame) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.paint_timing->largest_contentful_paint->largest_image_paint =
      base::Milliseconds(4780);
  timing.paint_timing->largest_contentful_paint->largest_image_paint_size =
      100u;
  PopulateExperimentalLCP(timing.paint_timing);
  PopulateRequiredTimingFields(&timing);

  // Subframe timing for same-site
  page_load_metrics::mojom::PageLoadTiming subframe_timing;
  page_load_metrics::InitPageLoadTimingForTest(&subframe_timing);
  subframe_timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(2);
  subframe_timing.paint_timing->largest_contentful_paint->largest_image_paint =
      base::Milliseconds(4790);
  subframe_timing.paint_timing->largest_contentful_paint
      ->largest_image_paint_size = 110u;
  PopulateExperimentalLCP(subframe_timing.paint_timing);
  PopulateRequiredTimingFields(&subframe_timing);

  // Subframe timing for cross-site
  page_load_metrics::mojom::PageLoadTiming subframe_timing2;
  page_load_metrics::InitPageLoadTimingForTest(&subframe_timing2);
  subframe_timing2.navigation_start = base::Time::FromSecondsSinceUnixEpoch(2);
  subframe_timing2.paint_timing->largest_contentful_paint->largest_image_paint =
      base::Milliseconds(4800);
  subframe_timing2.paint_timing->largest_contentful_paint
      ->largest_image_paint_size = 50u;
  PopulateExperimentalLCP(subframe_timing2.paint_timing);
  PopulateRequiredTimingFields(&subframe_timing2);

  // Commit the main frame and a subframe.
  const char kSameSiteSubframeTestUrl[] =
      "https://sub.google.com/subframe.html";
  const char kCrossSiteSubframeTestUrl[] = "https://example.com/subframe.html";
  NavigateAndCommit(GURL(kTestUrl1));
  RenderFrameHost* same_site_subframe =
      NavigationSimulator::NavigateAndCommitFromDocument(
          GURL(kSameSiteSubframeTestUrl),
          RenderFrameHostTester::For(web_contents()->GetPrimaryMainFrame())
              ->AppendChild("same_site_subframe"));
  RenderFrameHost* cross_site_subframe =
      NavigationSimulator::NavigateAndCommitFromDocument(
          GURL(kCrossSiteSubframeTestUrl),
          RenderFrameHostTester::For(web_contents()->GetPrimaryMainFrame())
              ->AppendChild("cross_site_subframe"));

  // Simulate timing updates in the main frame and the subframe.
  tester()->SimulateTimingUpdate(timing);
  tester()->SimulateTimingUpdate(subframe_timing, same_site_subframe);
  tester()->SimulateTimingUpdate(subframe_timing2, cross_site_subframe);

  // Simulate closing the tab.
  DeleteContents();

  // Now test that there is a cross site subframe lcp.
  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      tester()->test_ukm_recorder().GetMergedEntriesByName(
          PageLoad::kEntryName);
  EXPECT_EQ(1ul, merged_entries.size());
  const ukm::mojom::UkmEntry* entry = merged_entries.begin()->second.get();

  tester()->test_ukm_recorder().ExpectEntryMetric(
      entry,
      PageLoad::
          kPaintTiming_NavigationToLargestContentfulPaint2_CrossSiteSubFrameName,
      4800);
}

// This is to test whether LargestContentfulPaintAllFrames can merge the
// candidates from different frames correctly. The metric should pick the larger
// candidate during merging.
TEST_F(UkmPageLoadMetricsObserverTest,
       LargestContentfulPaintAllFrames_MergeFrameCandidateBySize) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.paint_timing->largest_contentful_paint->largest_image_paint =
      base::Milliseconds(4780);
  timing.paint_timing->largest_contentful_paint->largest_image_paint_size = 50u;
  timing.paint_timing->largest_contentful_paint->image_bpp = 8.5;
  timing.paint_timing->largest_contentful_paint->image_request_priority_valid =
      true;
  timing.paint_timing->largest_contentful_paint->image_request_priority_value =
      net::RequestPriority::MEDIUM;
  PopulateExperimentalLCP(timing.paint_timing);
  PopulateRequiredTimingFields(&timing);

  page_load_metrics::mojom::PageLoadTiming subframe_timing;
  page_load_metrics::InitPageLoadTimingForTest(&subframe_timing);
  subframe_timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(2);
  subframe_timing.paint_timing->largest_contentful_paint->largest_image_paint =
      base::Milliseconds(990);
  subframe_timing.paint_timing->largest_contentful_paint
      ->largest_image_paint_size = 100u;
  subframe_timing.paint_timing->largest_contentful_paint->image_bpp = 1.5;
  subframe_timing.paint_timing->largest_contentful_paint
      ->image_request_priority_valid = true;
  subframe_timing.paint_timing->largest_contentful_paint
      ->image_request_priority_value = net::RequestPriority::LOW;
  PopulateExperimentalLCP(subframe_timing.paint_timing);
  PopulateRequiredTimingFields(&subframe_timing);

  // Commit the main frame and a subframe.
  NavigateAndCommit(GURL(kTestUrl1));
  RenderFrameHost* subframe =
      NavigationSimulator::NavigateAndCommitFromDocument(
          GURL(kSubframeTestUrl),
          RenderFrameHostTester::For(web_contents()->GetPrimaryMainFrame())
              ->AppendChild("subframe"));

  // Simulate timing updates in the main frame and the subframe.
  tester()->SimulateTimingUpdate(timing);
  tester()->SimulateTimingUpdate(subframe_timing, subframe);

  // Simulate closing the tab.
  DeleteContents();

  TestLCP(990, LargestContentTextOrImage::kImage, false /* test_main_frame */,
          23 /* image_bpp = "1.0 - 2.0" */, net::RequestPriority::LOW);
}

TEST_F(UkmPageLoadMetricsObserverTest, NormalizedUserInteractionLatencies) {
  NavigateAndCommit(GURL(kTestUrl1));

  page_load_metrics::mojom::InputTiming input_timing;
  input_timing.num_interactions = 3;
  input_timing.max_event_durations =
      UserInteractionLatencies::NewUserInteractionLatencies({});
  auto& max_event_durations =
      input_timing.max_event_durations->get_user_interaction_latencies();

  base::TimeTicks current_time = base::TimeTicks::Now();
  max_event_durations.emplace_back(UserInteractionLatency::New(
      base::Milliseconds(50), UserInteractionType::kKeyboard, 1,
      current_time + base::Milliseconds(1000)));
  max_event_durations.emplace_back(UserInteractionLatency::New(
      base::Milliseconds(100), UserInteractionType::kTapOrClick, 2,
      current_time + base::Milliseconds(2000)));
  max_event_durations.emplace_back(UserInteractionLatency::New(
      base::Milliseconds(150), UserInteractionType::kDrag, 3,
      current_time + base::Milliseconds(3000)));

  tester()->SimulateInputTimingUpdate(input_timing);

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
            kInteractiveTiming_WorstUserInteractionLatency_MaxEventDurationName,
        150);
    tester()->test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(),
        PageLoad::
            kInteractiveTiming_UserInteractionLatency_HighPercentile2_MaxEventDurationName,
        150);
    tester()->test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(), PageLoad::kInteractiveTiming_INPOffsetName, 3);
    tester()->test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(), PageLoad::kInteractiveTiming_NumInteractionsName, 3);
  }
}

TEST_F(UkmPageLoadMetricsObserverTest,
       NormalizedUserInteractionLatenciesRecordOnHidden) {
  NavigateAndCommit(GURL(kTestUrl1));

  page_load_metrics::mojom::InputTiming input_timing;
  input_timing.num_interactions = 3;
  input_timing.max_event_durations =
      UserInteractionLatencies::NewUserInteractionLatencies({});
  auto& max_event_durations =
      input_timing.max_event_durations->get_user_interaction_latencies();

  max_event_durations.emplace_back(UserInteractionLatency::New(
      base::Milliseconds(50), UserInteractionType::kKeyboard, 0,
      base::TimeTicks::Now()));

  tester()->SimulateInputTimingUpdate(input_timing);

  // Simulate hiding the tab (the new INP metrics should be recorded at the
  // first hide).
  web_contents()->WasHidden();

  const auto& ukm_recorder = tester()->test_ukm_recorder();
  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      ukm_recorder.GetMergedEntriesByName(PageLoad::kEntryName);
  EXPECT_EQ(1ul, merged_entries.size());

  const ukm::mojom::UkmEntry* entry = merged_entries.begin()->second.get();
  ukm_recorder.ExpectEntrySourceHasUrl(entry, GURL(kTestUrl1));
  ukm_recorder.ExpectEntryMetric(
      entry,
      PageLoad::
          kInteractiveTiming_UserInteractionLatencyAtFirstOnHidden_HighPercentile2_MaxEventDurationName,
      50);
  EXPECT_THAT(tester()->histogram_tester().GetAllSamples(
                  "PageLoad.InteractiveTiming."
                  "UserInteractionLatencyAtFirstOnHidden.HighPercentile2."
                  "MaxEventDuration"),
              testing::ElementsAre(base::Bucket(50, 1)));
}

TEST_F(UkmPageLoadMetricsObserverTest, FirstInputDelayAndTimestamp) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.interactive_timing->first_input_delay = base::Milliseconds(50);
  timing.interactive_timing->first_input_timestamp = base::Milliseconds(712);
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

TEST_F(UkmPageLoadMetricsObserverTest, FirstScrollDelayAndTimestamp) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.interactive_timing->first_scroll_delay = base::Milliseconds(50);
  timing.interactive_timing->first_scroll_timestamp = base::Milliseconds(70);
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
    tester()->test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(), PageLoad::kInteractiveTiming_FirstScrollTimestampName,
        64);
  }
}

TEST_F(UkmPageLoadMetricsObserverTest, MultiplePageLoads) {
  page_load_metrics::mojom::PageLoadTiming timing1;
  page_load_metrics::InitPageLoadTimingForTest(&timing1);
  timing1.parse_timing->parse_start = base::Milliseconds(10);
  timing1.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing1.paint_timing->first_contentful_paint = base::Milliseconds(200);
  PopulateRequiredTimingFields(&timing1);

  // Second navigation reports no timing metrics.
  page_load_metrics::mojom::PageLoadTiming timing2;
  page_load_metrics::InitPageLoadTimingForTest(&timing2);
  timing2.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
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
  EXPECT_TRUE(tester()->test_ukm_recorder().EntryHasMetric(
      entry2, PageLoad::kPageTiming_ForegroundDurationName));
}

TEST_F(UkmPageLoadMetricsObserverTest, NetworkQualityEstimates) {
  EXPECT_CALL(mock_network_quality_provider(), GetEffectiveConnectionType())
      .WillRepeatedly(Return(net::EFFECTIVE_CONNECTION_TYPE_3G));
  EXPECT_CALL(mock_network_quality_provider(), GetHttpRTT())
      .WillRepeatedly(Return(base::Milliseconds(100)));
  EXPECT_CALL(mock_network_quality_provider(), GetTransportRTT())
      .WillRepeatedly(Return(base::Milliseconds(200)));
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
      ukm::GetExponentialBucketMinForBytes(80 * 1024);

  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      tester()->test_ukm_recorder().GetMergedEntriesByName(
          PageLoad::kEntryName);
  EXPECT_EQ(1ul, merged_entries.size());

  for (const auto& kv : merged_entries) {
    tester()->test_ukm_recorder().ExpectEntrySourceHasUrl(kv.second.get(),
                                                          GURL(kTestUrl1));
    tester()->test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(), "Net.JavaScriptBytes2", bucketed_network_js_bytes);
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
      ukm::GetExponentialBucketMinForBytes(500 * 1024);

  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      tester()->test_ukm_recorder().GetMergedEntriesByName(
          PageLoad::kEntryName);
  EXPECT_EQ(1ul, merged_entries.size());

  for (const auto& kv : merged_entries) {
    tester()->test_ukm_recorder().ExpectEntrySourceHasUrl(kv.second.get(),
                                                          GURL(kTestUrl1));
    tester()->test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(), "Net.JavaScriptMaxBytes2",
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
        kv.second.get(), "Net.ImageBytes2",
        ukm::GetExponentialBucketMinForBytes(30 * 1024));
    tester()->test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(), "Net.ImageSubframeBytes2",
        ukm::GetExponentialBucketMinForBytes(20 * 1024));
    tester()->test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(), "Net.MediaBytes2",
        ukm::GetExponentialBucketMinForBytes(50 * 1024));
  }
}

TEST_F(UkmPageLoadMetricsObserverTest, CpuTimeMetrics) {
  NavigateAndCommit(GURL(kTestUrl1));

  // Simulate some CPU usage.
  page_load_metrics::mojom::CpuTiming cpu_timing(base::Milliseconds(500));
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
  base::TimeTicks current_time = base::TimeTicks::Now();
  page_load_metrics::mojom::FrameRenderDataUpdate render_data(1.0, 1.0, {});
  render_data.new_layout_shifts.emplace_back(
      page_load_metrics::mojom::LayoutShift::New(
          current_time - base::Milliseconds(4000), 0.5));
  render_data.new_layout_shifts.emplace_back(
      page_load_metrics::mojom::LayoutShift::New(
          current_time - base::Milliseconds(3500), 0.5));

  tester()->SimulateRenderDataUpdate(render_data);

  // Simulate hiding the tab (the report should include shifts after hide).
  web_contents()->WasHidden();

  page_load_metrics::mojom::FrameRenderDataUpdate render_data_2(1.5, 0.0, {});
  render_data_2.new_layout_shifts.emplace_back(
      page_load_metrics::mojom::LayoutShift::New(
          current_time - base::Milliseconds(2500), 1.5));
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
    ukm_recorder.ExpectEntryMetric(kv.second.get(),
                                   PageLoad::kNavigation_PageEndReason3Name,
                                   page_load_metrics::END_CLOSE);
  }
  EXPECT_THAT(tester()->histogram_tester().GetAllSamples(
                  "PageLoad.LayoutInstability.CumulativeShiftScore"),
              testing::ElementsAre(base::Bucket(25, 1)));
  EXPECT_THAT(tester()->histogram_tester().GetAllSamples(
                  "PageLoad.LayoutInstability.MaxCumulativeShiftScore."
                  "SessionWindow.Gap1000ms.Max5000ms2"),
              testing::ElementsAre(base::Bucket(24000, 1)));
}

TEST_F(UkmPageLoadMetricsObserverTest, SoftNavigationCount) {
  auto url = GURL(kTestUrl1);
  NavigateAndCommit(url);

  auto soft_navigation_metrics =
      page_load_metrics::mojom::SoftNavigationMetrics(
          1, base::Milliseconds(12), "00000-00000-00000-00000",
          page_load_metrics::mojom::LargestContentfulPaintTiming::New());

  content::MockNavigationHandle navigation_handle;
  navigation_handle.set_has_committed(true);
  navigation_handle.set_is_in_primary_main_frame(true);
  navigation_handle.set_is_same_document(true);

  // Simulate the detection of soft navigation so that the ukm source id for
  // soft navigation is initialized.
  tester()->SimulateSoftNavigation(&navigation_handle);

  tester()->SimulateSoftNavigationCountUpdate(soft_navigation_metrics);

  // Simulate closing the tab.
  DeleteContents();

  const auto& ukm_recorder = tester()->test_ukm_recorder();
  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      ukm_recorder.GetMergedEntriesByName(PageLoad::kEntryName);
  EXPECT_EQ(1ul, merged_entries.size());

  for (const auto& kv : merged_entries) {
    const ukm::mojom::UkmEntry* ukm_entry = kv.second.get();
    ukm_recorder.ExpectEntrySourceHasUrl(ukm_entry, GURL(kTestUrl1));
    ukm_recorder.ExpectEntryMetric(ukm_entry,
                                   PageLoad::kSoftNavigationCountName, 1);
    ukm_recorder.ExpectEntryMetric(kv.second.get(),
                                   PageLoad::kNavigation_PageEndReason3Name,
                                   page_load_metrics::END_CLOSE);
  }
}

TEST_F(UkmPageLoadMetricsObserverTest,
       ExperimentalLayoutInstabilityRecordOnHidden) {
  NavigateAndCommit(GURL(kTestUrl1));
  base::TimeTicks current_time = base::TimeTicks::Now();
  page_load_metrics::mojom::FrameRenderDataUpdate render_data(1.0, 1.0, {});
  render_data.new_layout_shifts.emplace_back(
      page_load_metrics::mojom::LayoutShift::New(
          current_time - base::Milliseconds(4000), 0.5));
  render_data.new_layout_shifts.emplace_back(
      page_load_metrics::mojom::LayoutShift::New(
          current_time - base::Milliseconds(3500), 0.5));

  tester()->SimulateRenderDataUpdate(render_data);

  // Simulate hiding the tab (the experimental CLS metrics should include
  // shifts before the first hide).
  web_contents()->WasHidden();

  const auto& ukm_recorder = tester()->test_ukm_recorder();
  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      ukm_recorder.GetMergedEntriesByName(PageLoad::kEntryName);
  EXPECT_EQ(1ul, merged_entries.size());

  for (const auto& kv : merged_entries) {
    const ukm::mojom::UkmEntry* ukm_entry = kv.second.get();
    ukm_recorder.ExpectEntrySourceHasUrl(ukm_entry, GURL(kTestUrl1));
    ukm_recorder.ExpectEntryMetric(
        ukm_entry,
        PageLoad::
            kExperimental_LayoutInstability_CumulativeShiftScoreAtFirstOnHiddenName,
        100);
    ukm_recorder.ExpectEntryMetric(
        ukm_entry,
        PageLoad::
            kExperimental_LayoutInstability_MaxCumulativeShiftScoreAtFirstOnHidden_SessionWindow_Gap1000ms_Max5000msName,
        100);
  }

  page_load_metrics::mojom::FrameRenderDataUpdate render_data_2(1.5, 0.0, {});
  render_data_2.new_layout_shifts.emplace_back(
      page_load_metrics::mojom::LayoutShift::New(
          current_time - base::Milliseconds(2500), 1.5));
  tester()->SimulateRenderDataUpdate(render_data_2);

  // Simulate closing the tab (the CLS metrics should include all the shifts
  // before the tab closes).
  DeleteContents();

  const auto& ukm_recorder_2 = tester()->test_ukm_recorder();
  merged_entries = ukm_recorder_2.GetMergedEntriesByName(PageLoad::kEntryName);
  EXPECT_EQ(1ul, merged_entries.size());

  for (const auto& kv : merged_entries) {
    const ukm::mojom::UkmEntry* ukm_entry = kv.second.get();
    ukm_recorder_2.ExpectEntrySourceHasUrl(ukm_entry, GURL(kTestUrl1));
    ukm_recorder.ExpectEntryMetric(
        ukm_entry,
        PageLoad::
            kExperimental_LayoutInstability_CumulativeShiftScoreAtFirstOnHiddenName,
        100);
    ukm_recorder.ExpectEntryMetric(
        ukm_entry,
        PageLoad::
            kExperimental_LayoutInstability_MaxCumulativeShiftScoreAtFirstOnHidden_SessionWindow_Gap1000ms_Max5000msName,
        100);
    ukm_recorder_2.ExpectEntryMetric(
        ukm_entry, PageLoad::kLayoutInstability_CumulativeShiftScoreName, 250);
    ukm_recorder_2.ExpectEntryMetric(
        ukm_entry,
        PageLoad::
            kLayoutInstability_CumulativeShiftScore_MainFrame_BeforeInputOrScrollName,
        100);
    ukm_recorder_2.ExpectEntryMetric(
        ukm_entry,
        PageLoad::
            kLayoutInstability_MaxCumulativeShiftScore_SessionWindow_Gap1000ms_Max5000msName,
        250);
    ukm_recorder_2.ExpectEntryMetric(kv.second.get(),
                                     PageLoad::kNavigation_PageEndReason3Name,
                                     page_load_metrics::END_CLOSE);
  }
  EXPECT_THAT(tester()->histogram_tester().GetAllSamples(
                  "PageLoad.LayoutInstability.CumulativeShiftScore"),
              testing::ElementsAre(base::Bucket(25, 1)));
  EXPECT_THAT(tester()->histogram_tester().GetAllSamples(
                  "PageLoad.LayoutInstability.MaxCumulativeShiftScore."
                  "SessionWindow.Gap1000ms.Max5000ms2"),
              testing::ElementsAre(base::Bucket(24000, 1)));
  EXPECT_THAT(tester()->histogram_tester().GetAllSamples(
                  "PageLoad.LayoutInstability."
                  "CumulativeShiftScoreAtFirstOnHidden"),
              testing::ElementsAre(base::Bucket(10, 1)));
  // The layout shift score was originally 1, after multiplying 10000, it
  // should fit into the bucket of value 9130, with a histogram of maximum
  // value of 24000.
  const base::HistogramBase::Sample max_cls = 9130;
  EXPECT_THAT(tester()->histogram_tester().GetAllSamples(
                  "PageLoad.LayoutInstability."
                  "MaxCumulativeShiftScoreAtFirstOnHidden.SessionWindow."
                  "Gap1000ms.Max5000ms"),
              testing::ElementsAre(base::Bucket(max_cls, 1)));
}

TEST_F(UkmPageLoadMetricsObserverTest,
       ExperimentalLayoutInstabilityRecordOnPageOpenBackground) {
  // Open the page at the background.
  web_contents()->WasHidden();
  NavigateAndCommit(GURL(kTestUrl1));
  base::TimeTicks current_time = base::TimeTicks::Now();

  // Bring the tab to the foreground and simulate a layout shift.
  web_contents()->WasShown();
  page_load_metrics::mojom::FrameRenderDataUpdate render_data(1.0, 1.0, {});
  render_data.new_layout_shifts.emplace_back(
      page_load_metrics::mojom::LayoutShift::New(
          current_time - base::Milliseconds(4000), 0.5));
  render_data.new_layout_shifts.emplace_back(
      page_load_metrics::mojom::LayoutShift::New(
          current_time - base::Milliseconds(3500), 0.5));

  tester()->SimulateRenderDataUpdate(render_data);

  // Simulate hiding the tab (the report should include shifts before hide).
  web_contents()->WasHidden();

  const auto& ukm_recorder = tester()->test_ukm_recorder();
  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      ukm_recorder.GetMergedEntriesByName(PageLoad::kEntryName);
  EXPECT_EQ(1ul, merged_entries.size());

  for (const auto& kv : merged_entries) {
    const ukm::mojom::UkmEntry* ukm_entry = kv.second.get();
    ukm_recorder.ExpectEntrySourceHasUrl(ukm_entry, GURL(kTestUrl1));
    ukm_recorder.ExpectEntryMetric(
        ukm_entry,
        PageLoad::
            kExperimental_LayoutInstability_CumulativeShiftScoreAtFirstOnHiddenName,
        100);
    ukm_recorder.ExpectEntryMetric(
        ukm_entry,
        PageLoad::
            kExperimental_LayoutInstability_MaxCumulativeShiftScoreAtFirstOnHidden_SessionWindow_Gap1000ms_Max5000msName,
        100);
  }
  EXPECT_THAT(tester()->histogram_tester().GetAllSamples(
                  "PageLoad.LayoutInstability."
                  "CumulativeShiftScoreAtFirstOnHidden"),
              testing::ElementsAre(base::Bucket(10, 1)));
  // The layout shift score was originally 1, after multiplying 10000, it
  // should fit into the bucket of value 9130, with a histogram of maximum
  // value of 24000.
  const base::HistogramBase::Sample max_cls = 9130;
  EXPECT_THAT(tester()->histogram_tester().GetAllSamples(
                  "PageLoad.LayoutInstability."
                  "MaxCumulativeShiftScoreAtFirstOnHidden.SessionWindow."
                  "Gap1000ms.Max5000ms"),
              testing::ElementsAre(base::Bucket(max_cls, 1)));
}

TEST_F(UkmPageLoadMetricsObserverWithMockTimeTest,
       LargestContentfulPaintRecordOnHidden) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.paint_timing->largest_contentful_paint->largest_image_paint =
      base::Milliseconds(60);
  timing.paint_timing->largest_contentful_paint->largest_image_paint_size = 50u;

  PopulateExperimentalLCP(timing.paint_timing);
  PopulateRequiredTimingFields(&timing);
  NavigateAndCommit(GURL(kTestUrl1));
  tester()->SimulateTimingUpdate(timing);

  task_environment()->FastForwardBy(base::Milliseconds(1000));
  web_contents()->WasHidden();

  const auto& ukm_recorder = tester()->test_ukm_recorder();
  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      ukm_recorder.GetMergedEntriesByName(PageLoad::kEntryName);
  EXPECT_EQ(1ul, merged_entries.size());

  const ukm::mojom::UkmEntry* entry = merged_entries.begin()->second.get();
  ukm_recorder.ExpectEntrySourceHasUrl(entry, GURL(kTestUrl1));
  ukm_recorder.ExpectEntryMetric(
      entry,
      PageLoad::
          kPaintTiming_NavigationToLargestContentfulPaint2AtFirstOnHiddenName,
      60);
  EXPECT_THAT(tester()->histogram_tester().GetAllSamples(
                  "PageLoad.PaintTiming."
                  "NavigationToLargestContentfulPaint2AtFirstOnHidden"),
              testing::ElementsAre(base::Bucket(60, 1)));
}

TEST_F(UkmPageLoadMetricsObserverWithMockTimeTest,
       LargestContentfulPaintRecordOnPageOpenBackground) {
  // Open the page at the background.
  web_contents()->WasHidden();
  NavigateAndCommit(GURL(kTestUrl1));

  // Bring the tab to the foreground and simulate an image paint.
  web_contents()->WasShown();
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  PopulateRequiredTimingFields(&timing);

  timing.paint_timing->largest_contentful_paint->largest_image_paint =
      base::Milliseconds(60);
  timing.paint_timing->largest_contentful_paint->largest_image_paint_size = 50u;
  PopulateExperimentalLCP(timing.paint_timing);
  tester()->SimulateTimingUpdate(timing);

  // Simulate hiding the tab and check we should not record LCP for pages that
  // are started at the background.
  task_environment()->FastForwardBy(base::Milliseconds(1000));
  web_contents()->WasHidden();

  const auto& ukm_recorder = tester()->test_ukm_recorder();
  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      ukm_recorder.GetMergedEntriesByName(PageLoad::kEntryName);
  EXPECT_EQ(1ul, merged_entries.size());
  const ukm::mojom::UkmEntry* entry = merged_entries.begin()->second.get();
  ukm_recorder.ExpectEntrySourceHasUrl(entry, GURL(kTestUrl1));
  EXPECT_FALSE(tester()->test_ukm_recorder().EntryHasMetric(
      entry, PageLoad::kPaintTiming_NavigationToLargestContentfulPaint2Name));
  EXPECT_FALSE(tester()->test_ukm_recorder().EntryHasMetric(
      entry,
      PageLoad::
          kPaintTiming_NavigationToLargestContentfulPaint2AtFirstOnHiddenName));
  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.PaintTiming."
      "NavigationToLargestContentfulPaint2AtFirstOnHidden",
      0);
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
  page_load_metrics::mojom::FrameRenderDataUpdate render_data(1.0, 1.0, {});
  tester()->SimulateRenderDataUpdate(render_data);

  RenderFrameHost* subframe =
      NavigationSimulator::NavigateAndCommitFromDocument(
          GURL(kSubframeTestUrl),
          RenderFrameHostTester::For(web_contents()->GetPrimaryMainFrame())
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
  static const char16_t kShortName[] = u"test";
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
  data.SetShortName(kShortName);
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

TEST_F(UkmPageLoadMetricsObserverTest, NavigationIsScopedSearchLikeNavigation) {
  static const char16_t kShortName[] = u"test";
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
  data.SetShortName(kShortName);
  data.SetKeyword(data.short_name());
  data.SetURL(kSearchURL);

  // Set the DSE to the test URL.
  TemplateURL* template_url = model->Add(std::make_unique<TemplateURL>(data));
  ASSERT_TRUE(template_url);

  NavigateAndCommit(GURL(kSearchURLWithQuery));

  // Simulate closing the tab.
  DeleteContents();

  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      tester()->test_ukm_recorder().GetMergedEntriesByName(
          PageLoad::kEntryName);
  EXPECT_EQ(1ul, merged_entries.size());

  for (const auto& kv : merged_entries) {
    tester()->test_ukm_recorder().ExpectEntrySourceHasUrl(
        kv.second.get(), GURL(kSearchURLWithQuery));
    tester()->test_ukm_recorder().ExpectEntryMetric(
        kv.second.get(), PageLoad::kIsScopedSearchLikeNavigationName, true);
  }
}

TEST_F(UkmPageLoadMetricsObserverTest, NoLargestContentfulPaint) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
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
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.paint_timing->first_contentful_paint =
      tester()->GetDelegateForCommittedLoad().GetTimeToFirstBackground();
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
  base::TimeDelta time_to_first_background =
      *tester()->GetDelegateForCommittedLoad().GetTimeToFirstBackground();

  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.paint_timing->largest_contentful_paint->largest_image_paint =
      time_to_first_background;
  timing.paint_timing->largest_contentful_paint->largest_image_paint_size = 50u;
  PopulateExperimentalLCP(timing.paint_timing);
  PopulateRequiredTimingFields(&timing);

  // Simulate LCP at the same time as the hide (but reported after).
  tester()->SimulateTimingUpdate(timing);

  // Simulate closing the tab.
  DeleteContents();

  // Check that we reported the LCP UKM.
  TestLCP(time_to_first_background.InMilliseconds(),
          LargestContentTextOrImage::kImage, true /* test_main_frame */);
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
#if !BUILDFLAG(IS_ANDROID)
TEST_F(UkmPageLoadMetricsObserverTest, IsNTPCustomLink) {
  GURL url(kTestUrl1);

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
#endif  // !BUILDFLAG(IS_ANDROID)

TEST_F(UkmPageLoadMetricsObserverTest, DurationSinceLastVisitSeconds) {
  // TODO(tommycli): Should we move this test to either HistoryClustersService
  // or HistoryClustersTabHelper? On the one hand, the logic resides there. On
  // the other hand this serves as a good integration test with UKM.
  GURL url(kTestUrl1);

  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(profile(),
                                           ServiceAccessType::IMPLICIT_ACCESS);
  ASSERT_TRUE(history_service);
  // Fake that we visited this site 45 days ago.
  history_service->AddPage(url, base::Time::Now() - base::Days(45),
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
      base::Days(30).InSeconds());
}

TEST_F(UkmPageLoadMetricsObserverTest, NavigationTimestamp) {
  base::Time::Exploded exploded;
  base::Time::Now().LocalExplode(&exploded);
  NavigateAndCommit(GURL(kTestUrl1));

  // Simulate closing the tab.
  DeleteContents();

  // Verify UKM records for page load timestamp.
  const auto& ukm_recorder = tester()->test_ukm_recorder();
  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      ukm_recorder.GetMergedEntriesByName(PageLoad::kEntryName);
  EXPECT_EQ(1ul, merged_entries.size());
  const ukm::mojom::UkmEntry* entry = merged_entries.begin()->second.get();
  tester()->test_ukm_recorder().ExpectEntryMetric(
      entry, PageLoad::kHourOfDayName, exploded.hour);
  tester()->test_ukm_recorder().ExpectEntryMetric(
      entry, PageLoad::kDayOfWeekName, exploded.day_of_week);
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

  page_load_metrics::mojom::FrameRenderDataUpdate render_data(1.0, 1.0, {});
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
  page_load_metrics::mojom::FrameRenderDataUpdate render_data(delta, delta, {});
  tester()->SimulateRenderDataUpdate(render_data, frame);
}

RenderFrameHost* CLSUkmPageLoadMetricsObserverTest::NavigateSubframe() {
  return NavigationSimulator::NavigateAndCommitFromDocument(
      GURL(kSubframeTestUrl),
      RenderFrameHostTester::For(web_contents()->GetPrimaryMainFrame())
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

  RenderFrameHost* main_frame = web_contents()->GetPrimaryMainFrame();
  RenderFrameHost* subframe = NavigateSubframe();

  SimulateShiftDelta(1.0, main_frame);
  SimulateShiftDelta(1.5, subframe);

  // Simulate input.
  page_load_metrics::mojom::PageLoadTiming timing;
  InitPageLoadTimingWithInputOrScroll(timing, base::Seconds(1));
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

TEST_F(UkmPageLoadMetricsObserverTest,
       TestLogsBrowserInitiatedNavigationAsUserInitiated) {
  // Simulate a browser initiated navigation, which is always considered
  // user initiated.
  auto& test_ukm_recorder = tester()->test_ukm_recorder();
  tester()->NavigateWithPageTransitionAndCommit(
      GURL(kTestUrl1), ui::PageTransition::PAGE_TRANSITION_TYPED);
  tester()->NavigateToUntrackedUrl();

  auto result_metrics = test_ukm_recorder.FilteredHumanReadableMetricForEntry(
      PageLoad::kEntryName,
      PageLoad::kExperimental_Navigation_UserInitiatedName);
  EXPECT_EQ(1U, result_metrics.size());
  EXPECT_EQ(PageLoad::kExperimental_Navigation_UserInitiatedName,
            result_metrics[0].begin()->first);
  EXPECT_TRUE(result_metrics[0].begin()->second);
  // Check the UserPerceivedPageVisit version of the metrics as well.
  result_metrics = test_ukm_recorder.FilteredHumanReadableMetricForEntry(
      UserPerceivedPageVisit::kEntryName,
      UserPerceivedPageVisit::kUserInitiatedName);
  EXPECT_EQ(1U, result_metrics.size());
  EXPECT_EQ(UserPerceivedPageVisit::kUserInitiatedName,
            result_metrics[0].begin()->first);
  EXPECT_TRUE(result_metrics[0].begin()->second);
}

TEST_F(UkmPageLoadMetricsObserverTest,
       TestLogsUserInitiatedRendererNavigationAsUserInitiated) {
  auto& test_ukm_recorder = tester()->test_ukm_recorder();
  // Simulate a renderer initiated navigation. The associated navigation input
  // start time means this will also be considered user initiated.
  std::unique_ptr<content::NavigationSimulator> navigation =
      content::NavigationSimulator::CreateRendererInitiated(GURL(kTestUrl1),
                                                            main_rfh());
  navigation->SetNavigationInputStart(base::TimeTicks::Now());
  navigation->Commit();
  tester()->NavigateToUntrackedUrl();
  auto result_metrics = test_ukm_recorder.FilteredHumanReadableMetricForEntry(
      PageLoad::kEntryName,
      PageLoad::kExperimental_Navigation_UserInitiatedName);
  EXPECT_EQ(1U, result_metrics.size());
  EXPECT_EQ(PageLoad::kExperimental_Navigation_UserInitiatedName,
            result_metrics[0].begin()->first);
  EXPECT_TRUE(result_metrics[0].begin()->second);

  result_metrics = test_ukm_recorder.FilteredHumanReadableMetricForEntry(
      UserPerceivedPageVisit::kEntryName,
      UserPerceivedPageVisit::kUserInitiatedName);
  EXPECT_EQ(1U, result_metrics.size());
  EXPECT_EQ(UserPerceivedPageVisit::kUserInitiatedName,
            result_metrics[0].begin()->first);
  EXPECT_TRUE(result_metrics[0].begin()->second);
}

TEST_F(UkmPageLoadMetricsObserverTest,
       TestLogsRendererInitiatedRendererNavigationAsUserInitiated) {
  auto& test_ukm_recorder = tester()->test_ukm_recorder();
  // Simulate a renderer initiated navigation without an associated
  // navigation input start time. This will be considered not user
  // initiated.
  std::unique_ptr<content::NavigationSimulator> navigation =
      content::NavigationSimulator::CreateRendererInitiated(GURL(kTestUrl1),
                                                            main_rfh());
  navigation->Commit();
  tester()->NavigateToUntrackedUrl();
  auto result_metrics = test_ukm_recorder.FilteredHumanReadableMetricForEntry(
      PageLoad::kEntryName,
      PageLoad::kExperimental_Navigation_UserInitiatedName);
  EXPECT_EQ(1U, result_metrics.size());
  EXPECT_EQ(PageLoad::kExperimental_Navigation_UserInitiatedName,
            result_metrics[0].begin()->first);
  EXPECT_FALSE(result_metrics[0].begin()->second);

  result_metrics = test_ukm_recorder.FilteredHumanReadableMetricForEntry(
      UserPerceivedPageVisit::kEntryName,
      UserPerceivedPageVisit::kUserInitiatedName);
  EXPECT_EQ(1U, result_metrics.size());
  EXPECT_EQ(UserPerceivedPageVisit::kUserInitiatedName,
            result_metrics[0].begin()->first);
  EXPECT_FALSE(result_metrics[0].begin()->second);
}

TEST_F(UkmPageLoadMetricsObserverTest, TestWasDiscarded) {
  web_contents()->SetWasDiscarded(true);
  NavigateAndCommit(GURL(kTestUrl1));

  // Simulate closing the tab.
  DeleteContents();

  const auto& ukm_recorder = tester()->test_ukm_recorder();
  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      ukm_recorder.GetMergedEntriesByName(PageLoad::kEntryName);
  EXPECT_EQ(1ul, merged_entries.size());
  const ukm::mojom::UkmEntry* entry = merged_entries.begin()->second.get();
  tester()->test_ukm_recorder().ExpectEntryMetric(
      entry, PageLoad::kWasDiscardedName, 1);
}

#if !BUILDFLAG(IS_ANDROID)
// Power saver mode only exists on desktop.
TEST_F(UkmPageLoadMetricsObserverTest, TestRefreshRateThrottled) {
  TestingPrefServiceSimple local_state;
  performance_manager::user_tuning::prefs::RegisterLocalStatePrefs(
      local_state.registry());
  performance_manager::user_tuning::TestUserPerformanceTuningManagerEnvironment
      uptm_environment;
  uptm_environment.SetUp(&local_state);
  performance_manager::user_tuning::
      TestUserPerformanceTuningManagerEnvironment::SetBatterySaverMode(
          &local_state, true);

  NavigateAndCommit(GURL(kTestUrl1));

  // Simulate closing the tab.
  DeleteContents();

  const auto& ukm_recorder = tester()->test_ukm_recorder();
  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      ukm_recorder.GetMergedEntriesByName(PageLoad::kEntryName);
  EXPECT_EQ(1ul, merged_entries.size());
  const ukm::mojom::UkmEntry* entry = merged_entries.begin()->second.get();
  tester()->test_ukm_recorder().ExpectEntryMetric(
      entry, PageLoad::kRefreshRateThrottledName, 1);

  uptm_environment.TearDown();
}
#endif

// The following tests are ensure that Page Load metrics are recorded in a
// trace. Currently enabled only for platforms where USE_PERFETTO_CLIENT_LIBRARY
// is true (Android, Linux) as test infra (TestTraceProcessor) requires it.
class TracingWebContentsObserver : public content::WebContentsObserver {
 public:
  explicit TracingWebContentsObserver(content::WebContents* contents)
      : content::WebContentsObserver(contents) {
    WebContentsObserver::Observe(contents);
  }

  TracingWebContentsObserver(const TracingWebContentsObserver&) = delete;
  TracingWebContentsObserver& operator=(const TracingWebContentsObserver&) =
      delete;

  ~TracingWebContentsObserver() override = default;

  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override {
    navigation_id_ = navigation_handle->GetNavigationId();
  }

  int64_t NavigationId() { return navigation_id_; }

 private:
  int64_t navigation_id_ = -1;
};

TEST_F(UkmPageLoadMetricsObserverTest, TestTracingUserTimingMetrics) {
  ::base::test::TracingEnvironment tracing_environment_;
  TracingWebContentsObserver observer(web_contents());

  base::test::TestTraceProcessor ttp;
  ttp.StartTrace("interactions");

  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.user_timing_mark_fully_loaded = base::Milliseconds(200);
  timing.user_timing_mark_fully_visible = base::Milliseconds(250);
  timing.user_timing_mark_interactive = base::Milliseconds(300);
  PopulateRequiredTimingFields(&timing);

  GURL url(kTestUrl1);
  NavigateAndCommit(url);
  tester()->SimulateTimingUpdate(timing);
  int64_t navigation_id = observer.NavigationId();

  // Simulate closing the tab.
  DeleteContents();

  absl::Status status = ttp.StopAndParseTrace();
  ASSERT_TRUE(status.ok()) << status.message();

  std::string query = R"(
    SELECT
      EXTRACT_ARG(arg_set_id, 'page_load.navigation_id')
        AS navigation_id
    FROM slice
    WHERE name = 'PageLoadMetrics.UserTimingMarkFullyLoaded'
  )";
  auto result = ttp.RunQuery(query);
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_THAT(result.value(),
              ::testing::ElementsAre(std::vector<std::string>{"navigation_id"},
                                     std::vector<std::string>{
                                         base::NumberToString(navigation_id)}));

  std::string query_2 = R"(
    SELECT
      EXTRACT_ARG(arg_set_id, 'page_load.navigation_id')
        AS navigation_id
    FROM slice
    WHERE name = 'PageLoadMetrics.UserTimingMarkFullyVisible'
  )";
  auto result_2 = ttp.RunQuery(query_2);
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_THAT(result_2.value(),
              ::testing::ElementsAre(std::vector<std::string>{"navigation_id"},
                                     std::vector<std::string>{
                                         base::NumberToString(navigation_id)}));

  std::string query_3 = R"(
    SELECT
      EXTRACT_ARG(arg_set_id, 'page_load.navigation_id')
        AS navigation_id
    FROM slice
    WHERE name = 'PageLoadMetrics.UserTimingMarkInteractive'
  )";
  auto result_3 = ttp.RunQuery(query_3);
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_THAT(result_3.value(),
              ::testing::ElementsAre(std::vector<std::string>{"navigation_id"},
                                     std::vector<std::string>{
                                         base::NumberToString(navigation_id)}));
}
