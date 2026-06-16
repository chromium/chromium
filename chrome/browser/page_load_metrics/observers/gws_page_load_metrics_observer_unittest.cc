// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/after_startup_task_utils.h"
#include "chrome/browser/page_load_metrics/observers/chrome_gws_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/histogram_suffixes.h"
#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"
#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "components/page_load_metrics/common/test/page_load_metrics_test_util.h"
#include "components/page_load_metrics/google/browser/gws_abandoned_page_load_metrics_observer.h"
#include "components/page_load_metrics/google/browser/histogram_suffixes.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_navigation_handle.h"
#include "net/dns/public/resolution_details.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"

namespace {

constexpr char kGoogleSearchResultsUrl[] = "https://www.google.com/search?q=d";

}  // namespace

class GWSPageLoadMetricsObserverTest
    : public page_load_metrics::PageLoadMetricsObserverTestHarness {
 public:
  GWSPageLoadMetricsObserverTest()
      // Tests in this suite need a mock clock, because they care about which
      // histogram buckets the times of various events land inside. Using the
      // real clock would introduce flakes depending on how long the test takes
      // to execute. See https://issues.chromium.org/issues/327150423
      : page_load_metrics::PageLoadMetricsObserverTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  // page_load_metrics::PageLoadMetricsObserverTestHarness:
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    auto observer = std::make_unique<ChromeGWSPageLoadMetricsObserver>();
    // Set the PLMO navigation to the first navigation to ensure that we get
    // constant UMA names.
    observer->SetIsFirstNavigationForTesting(true);
    observer->SetNewTabPageForTesting(true);
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

  std::string AddHistogramSuffix(const std::string& metric_name) {
    return metric_name + internal::kSuffixFirstNavigation +
           internal::kSuffixFromNewTabPage;
  }

  void InitializeTestPageLoadTiming(
      page_load_metrics::mojom::PageLoadTiming* timing) {
    page_load_metrics::InitPageLoadTimingForTest(timing);
    timing->navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
    timing->interactive_timing->first_input_delay = base::Milliseconds(50);
    timing->interactive_timing->first_input_timestamp = base::Milliseconds(712);
    timing->parse_timing->parse_start = base::Milliseconds(100);
    timing->paint_timing->first_paint = base::Milliseconds(200);
    timing->paint_timing->first_contentful_paint = base::Milliseconds(300);
    timing->document_timing->dom_content_loaded_event_start =
        base::Milliseconds(600);
    timing->document_timing->load_event_start = base::Milliseconds(1000);

    timing->paint_timing->largest_contentful_paint->largest_image_paint =
        base::Milliseconds(4780);
    timing->paint_timing->largest_contentful_paint->largest_image_paint_size =
        100u;

    PopulateRequiredTimingFields(timing);
  }

 protected:
  raw_ptr<GWSPageLoadMetricsObserver, DanglingUntriaged> observer_ = nullptr;
};

TEST_F(GWSPageLoadMetricsObserverTest, Search) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);

  // TODO(crbug.com/393980912): Add a test for the histogram related to
  // LoadTimingInternalInfo. To do this, we need to add it to PageLoadTiming for
  // testing purposes. However, we shouldn't expose LoadTimingInternalInfo to
  // untrustworthy processes.

  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.parse_timing->parse_start = base::Milliseconds(1);
  timing.connect_start = base::Milliseconds(1);
  timing.domain_lookup_timing->domain_lookup_start = base::Milliseconds(1);
  timing.domain_lookup_timing->domain_lookup_end = base::Milliseconds(1);
  timing.paint_timing->first_contentful_paint = base::Milliseconds(10);
  timing.paint_timing->largest_contentful_paint->largest_text_paint =
      base::Milliseconds(100);
  timing.paint_timing->largest_contentful_paint->largest_text_paint_size = 20u;
  PopulateRequiredTimingFields(&timing);

  // Wait until the browser init is complete.
  AfterStartupTaskUtils::SetBrowserStartupIsCompleteForTesting();

  NavigateAndCommit(GURL(kGoogleSearchResultsUrl));

  tester()->SimulateTimingUpdate(timing);

  // Navigate again to force logging.
  tester()->NavigateToUntrackedUrl();
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSNavigationStartToFirstRequestStart, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramGWSNavigationStartToFirstRequestStart, 1, 1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSNavigationStartToFirstResponseStart, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramGWSNavigationStartToFirstResponseStart, 1, 1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSFirstRequestStartToFirstResponseStart, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramGWSFirstRequestStartToFirstResponseStart, 0, 1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSFirstRequestStartToFinalResponseStart, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramGWSFirstRequestStartToFinalResponseStart, 0, 1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSNavigationStartToFirstLoaderCallback, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramGWSNavigationStartToFirstLoaderCallback, 1, 1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSNavigationStartToFinalRequestStart, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramGWSNavigationStartToFinalRequestStart, 1, 1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSNavigationStartToFinalResponseStart, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramGWSNavigationStartToFinalResponseStart, 1, 1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSFinalRequestStartToFinalResponseStart, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramGWSFinalRequestStartToFinalResponseStart, 0, 1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSNavigationStartToFinalLoaderCallback, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramGWSNavigationStartToFinalLoaderCallback, 1, 1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSNavigationStartToOnComplete, 1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSParseStart, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramGWSParseStart, 1, 1);
  tester()->histogram_tester().ExpectTotalCount(
      AddHistogramSuffix(internal::kHistogramGWSConnectStart), 1);
  tester()->histogram_tester().ExpectBucketCount(
      AddHistogramSuffix(internal::kHistogramGWSConnectStart), 1, 1);
  tester()->histogram_tester().ExpectTotalCount(
      AddHistogramSuffix(internal::kHistogramGWSDomainLookupStart), 1);
  tester()->histogram_tester().ExpectBucketCount(
      AddHistogramSuffix(internal::kHistogramGWSDomainLookupStart), 1, 1);
  tester()->histogram_tester().ExpectTotalCount(
      AddHistogramSuffix(internal::kHistogramGWSDomainLookupEnd), 1);
  tester()->histogram_tester().ExpectBucketCount(
      AddHistogramSuffix(internal::kHistogramGWSDomainLookupEnd), 1, 1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSFirstContentfulPaint, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramGWSFirstContentfulPaint, 10, 1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSLargestContentfulPaint, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramGWSLargestContentfulPaint, 100, 1);
}

TEST_F(GWSPageLoadMetricsObserverTest, ConnectionEvents) {
  content::NavigationHandleTiming timing;
  timing.connected_callback_delay = base::Milliseconds(1);
  timing.accept_ch_frame_received = true;

  content::MockNavigationHandle handle(GURL(kGoogleSearchResultsUrl),
                                       main_rfh());
  EXPECT_CALL(handle, GetNavigationHandleTiming())
      .WillRepeatedly(testing::ReturnRef(timing));
  // Explicitly ensure the mock represents a non-cached response.
  handle.set_was_response_cached(false);

  tester()->StartNavigation(GURL(kGoogleSearchResultsUrl));
  observer_->OnCommit(&handle);

  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramGWSOnConnectedCalled, true, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramGWSAcceptCHFrameReceived, true, 1);
}

TEST_F(GWSPageLoadMetricsObserverTest, DNSResolutionSegmentation) {
  content::NavigationHandleTiming timing;
  timing.session_details = {
      .session_source = net::SessionSource::kNew,
      .resolution_details =
          net::ResolutionDetails{
              .source = net::ResolutionSource::kSecure,
              .task_completion_delay = base::Milliseconds(5),
              .doh_details =
                  net::DohResolutionDetails{
                      .session_source = net::SessionSource::kNew,
                      .connection_info = net::HttpConnectionInfoCoarse::kHTTP2,
                  },
          },
  };
  timing.first_request_domain_lookup_delay = base::Milliseconds(10);

  base::TimeTicks now = base::TimeTicks::Now();
  timing.first_request_start_time = now;
  timing.first_response_start_time = now + base::Milliseconds(10);
  timing.first_loader_callback_time = now + base::Milliseconds(20);
  timing.final_request_start_time = now + base::Milliseconds(30);
  timing.final_response_start_time = now + base::Milliseconds(40);
  timing.final_loader_callback_time = now + base::Milliseconds(50);
  timing.navigation_commit_sent_time = now + base::Milliseconds(60);

  content::MockNavigationHandle handle(GURL(kGoogleSearchResultsUrl),
                                       main_rfh());
  EXPECT_CALL(handle, GetNavigationHandleTiming())
      .WillRepeatedly(testing::ReturnRef(timing));
  handle.set_was_response_cached(false);

  NavigateAndCommit(GURL(kGoogleSearchResultsUrl));

  page_load_metrics::mojom::PageLoadTiming page_load_timing;
  page_load_metrics::InitPageLoadTimingForTest(&page_load_timing);
  page_load_timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  page_load_timing.parse_timing->parse_start = base::Milliseconds(1);
  page_load_timing.paint_timing->first_contentful_paint =
      base::Milliseconds(10);
  page_load_timing.paint_timing->largest_contentful_paint->largest_text_paint =
      base::Milliseconds(100);
  page_load_timing.paint_timing->largest_contentful_paint
      ->largest_text_paint_size = 20u;
  PopulateRequiredTimingFields(&page_load_timing);

  tester()->SimulateTimingUpdate(page_load_timing);

  observer_->OnCommit(&handle);

  tester()->NavigateToUntrackedUrl();
  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.GoogleSearch.WarmUpType", 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramGWSConnectTimingFirstRequestDomainLookupDelay, 10, 1);

  tester()->histogram_tester().ExpectBucketCount(
      internal::
          kHistogramGWSConnectTimingFirstRequestDomainLookupDelaySecureDns,
      10, 1);
  tester()->histogram_tester().ExpectBucketCount(
      base::StrCat(
          {internal::
               kHistogramGWSConnectTimingFirstRequestResolutionDetailsTaskCompletionDelay,
           ".SecureDns"}),
      5, 1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::
          kHistogramGWSConnectTimingFirstRequestDomainLookupDelayInsecureDns,
      0);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramGWSConnectTimingFirstRequestDohDetailsSessionSource,
      static_cast<int>(net::SessionSource::kNew), 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramGWSConnectTimingFirstRequestDohDetailsConnectionInfo,
      static_cast<int>(net::HttpConnectionInfoCoarse::kHTTP2), 1);
}

TEST_F(GWSPageLoadMetricsObserverTest, DNSResolutionSegmentationFallback) {
  content::NavigationHandleTiming timing;
  timing.session_details = {
      .session_source = net::SessionSource::kNew,
      .resolution_details =
          net::ResolutionDetails{
              .source = net::ResolutionSource::kInsecure,
              .task_completion_delay = base::Milliseconds(8),
              .secure_dns_attempted = true,
          },
  };
  timing.first_request_domain_lookup_delay = base::Milliseconds(15);

  base::TimeTicks now = base::TimeTicks::Now();
  timing.first_request_start_time = now;
  timing.first_response_start_time = now + base::Milliseconds(10);
  timing.first_loader_callback_time = now + base::Milliseconds(20);
  timing.final_request_start_time = now + base::Milliseconds(30);
  timing.final_response_start_time = now + base::Milliseconds(40);
  timing.final_loader_callback_time = now + base::Milliseconds(50);
  timing.navigation_commit_sent_time = now + base::Milliseconds(60);

  content::MockNavigationHandle handle(GURL(kGoogleSearchResultsUrl),
                                       main_rfh());
  EXPECT_CALL(handle, GetNavigationHandleTiming())
      .WillRepeatedly(testing::ReturnRef(timing));
  handle.set_was_response_cached(false);

  NavigateAndCommit(GURL(kGoogleSearchResultsUrl));

  page_load_metrics::mojom::PageLoadTiming page_load_timing;
  page_load_metrics::InitPageLoadTimingForTest(&page_load_timing);
  page_load_timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  page_load_timing.parse_timing->parse_start = base::Milliseconds(1);
  page_load_timing.paint_timing->first_contentful_paint =
      base::Milliseconds(10);
  page_load_timing.paint_timing->largest_contentful_paint->largest_text_paint =
      base::Milliseconds(100);
  page_load_timing.paint_timing->largest_contentful_paint
      ->largest_text_paint_size = 20u;
  PopulateRequiredTimingFields(&page_load_timing);

  tester()->SimulateTimingUpdate(page_load_timing);

  observer_->OnCommit(&handle);

  tester()->NavigateToUntrackedUrl();
  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.GoogleSearch.WarmUpType", 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramGWSConnectTimingFirstRequestDomainLookupDelay, 15, 1);

  tester()->histogram_tester().ExpectTotalCount(
      internal::
          kHistogramGWSConnectTimingFirstRequestDomainLookupDelaySecureDns,
      0);
  tester()->histogram_tester().ExpectBucketCount(
      internal::
          kHistogramGWSConnectTimingFirstRequestDomainLookupDelayInsecureDns,
      15, 1);
  tester()->histogram_tester().ExpectBucketCount(
      base::StrCat(
          {internal::
               kHistogramGWSConnectTimingFirstRequestResolutionDetailsTaskCompletionDelay,
           ".InsecureDns"}),
      8, 1);
}

TEST_F(GWSPageLoadMetricsObserverTest, NonSearch) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.parse_timing->parse_start = base::Milliseconds(1);
  timing.connect_start = base::Milliseconds(1);
  timing.domain_lookup_timing->domain_lookup_start = base::Milliseconds(1);
  timing.domain_lookup_timing->domain_lookup_end = base::Milliseconds(1);
  timing.paint_timing->first_contentful_paint = base::Milliseconds(10);
  timing.paint_timing->largest_contentful_paint->largest_text_paint =
      base::Milliseconds(100);
  timing.paint_timing->largest_contentful_paint->largest_text_paint_size = 20u;
  PopulateRequiredTimingFields(&timing);

  // Wait until the browser init is complete.
  AfterStartupTaskUtils::SetBrowserStartupIsCompleteForTesting();

  NavigateAndCommit(GURL("https://www.google.com/foo&q=test"));

  tester()->SimulateTimingUpdate(timing);
  // Navigate again to force logging.
  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSNavigationStartToFirstRequestStart, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSNavigationStartToFirstResponseStart, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSNavigationStartToFirstLoaderCallback, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSNavigationStartToFinalRequestStart, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSNavigationStartToFinalResponseStart, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSNavigationStartToFinalLoaderCallback, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSNavigationStartToOnComplete, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSParseStart, 0);
  tester()->histogram_tester().ExpectTotalCount(
      AddHistogramSuffix(internal::kHistogramGWSConnectStart), 0);
  tester()->histogram_tester().ExpectTotalCount(
      AddHistogramSuffix(internal::kHistogramGWSDomainLookupStart), 0);
  tester()->histogram_tester().ExpectTotalCount(
      AddHistogramSuffix(internal::kHistogramGWSDomainLookupEnd), 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSFirstContentfulPaint, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSLargestContentfulPaint, 0);
}

TEST_F(GWSPageLoadMetricsObserverTest, SearchBackground) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.parse_timing->parse_start = base::Seconds(60);
  timing.connect_start = base::Milliseconds(1);
  timing.domain_lookup_timing->domain_lookup_start = base::Milliseconds(1);
  timing.domain_lookup_timing->domain_lookup_end = base::Milliseconds(1);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.paint_timing->first_contentful_paint = base::Seconds(60);
  timing.paint_timing->largest_contentful_paint->largest_text_paint =
      base::Seconds(60);
  timing.paint_timing->largest_contentful_paint->largest_text_paint_size = 20u;
  PopulateRequiredTimingFields(&timing);

  // Wait until the browser init is complete.
  AfterStartupTaskUtils::SetBrowserStartupIsCompleteForTesting();

  NavigateAndCommit(GURL(kGoogleSearchResultsUrl));
  web_contents()->WasHidden();
  tester()->SimulateTimingUpdate(timing);
  // Navigate again to force logging.
  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSNavigationStartToFirstRequestStart, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSNavigationStartToFirstResponseStart, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSNavigationStartToFirstLoaderCallback, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSNavigationStartToFinalRequestStart, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSNavigationStartToFinalResponseStart, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSNavigationStartToFinalLoaderCallback, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSNavigationStartToOnComplete, 1);
  tester()->histogram_tester().ExpectTotalCount(
      AddHistogramSuffix(internal::kHistogramGWSConnectStart), 0);
  tester()->histogram_tester().ExpectTotalCount(
      AddHistogramSuffix(internal::kHistogramGWSDomainLookupStart), 0);
  tester()->histogram_tester().ExpectTotalCount(
      AddHistogramSuffix(internal::kHistogramGWSDomainLookupEnd), 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSParseStart, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSFirstContentfulPaint, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSLargestContentfulPaint, 0);
}

TEST_F(GWSPageLoadMetricsObserverTest, SearchBackgroundLater) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.parse_timing->parse_start = base::Microseconds(1);
  timing.connect_start = base::Milliseconds(1);
  timing.domain_lookup_timing->domain_lookup_start = base::Milliseconds(1);
  timing.domain_lookup_timing->domain_lookup_end = base::Milliseconds(1);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.paint_timing->first_contentful_paint = base::Microseconds(1);
  timing.paint_timing->largest_contentful_paint->largest_text_paint =
      base::Microseconds(1);
  timing.paint_timing->largest_contentful_paint->largest_text_paint_size = 20u;
  PopulateRequiredTimingFields(&timing);

  // Wait until the browser init is complete.
  AfterStartupTaskUtils::SetBrowserStartupIsCompleteForTesting();

  NavigateAndCommit(GURL(kGoogleSearchResultsUrl));
  // Sleep to make sure the backgrounded time is > than the paint time, even
  // for low resolution timers.
  task_environment()->FastForwardBy(base::Milliseconds(50));
  web_contents()->WasHidden();
  tester()->SimulateTimingUpdate(timing);
  // Navigate again to force logging.
  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSNavigationStartToFirstRequestStart, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramGWSNavigationStartToFirstRequestStart, 0, 1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSNavigationStartToFirstResponseStart, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramGWSNavigationStartToFirstResponseStart, 0, 1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSFirstRequestStartToFirstResponseStart, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramGWSFirstRequestStartToFirstResponseStart, 0, 1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSFirstRequestStartToFinalResponseStart, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramGWSFirstRequestStartToFinalResponseStart, 0, 1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSNavigationStartToFirstLoaderCallback, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramGWSNavigationStartToFirstLoaderCallback, 0, 1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSNavigationStartToFinalRequestStart, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramGWSNavigationStartToFinalRequestStart, 0, 1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSNavigationStartToFinalResponseStart, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramGWSNavigationStartToFinalResponseStart, 0, 1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSFinalRequestStartToFinalResponseStart, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramGWSFinalRequestStartToFinalResponseStart, 0, 1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSNavigationStartToFinalLoaderCallback, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramGWSNavigationStartToFinalLoaderCallback, 0, 1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSNavigationStartToOnComplete, 1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSParseStart, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramGWSParseStart, 0, 1);
  tester()->histogram_tester().ExpectTotalCount(
      AddHistogramSuffix(internal::kHistogramGWSConnectStart), 1);
  tester()->histogram_tester().ExpectBucketCount(
      AddHistogramSuffix(internal::kHistogramGWSConnectStart), 0, 1);
  tester()->histogram_tester().ExpectTotalCount(
      AddHistogramSuffix(internal::kHistogramGWSDomainLookupStart), 1);
  tester()->histogram_tester().ExpectBucketCount(
      AddHistogramSuffix(internal::kHistogramGWSDomainLookupStart), 0, 1);
  tester()->histogram_tester().ExpectTotalCount(
      AddHistogramSuffix(internal::kHistogramGWSDomainLookupEnd), 1);
  tester()->histogram_tester().ExpectBucketCount(
      AddHistogramSuffix(internal::kHistogramGWSDomainLookupEnd), 0, 1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSFirstContentfulPaint, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramGWSFirstContentfulPaint, 0, 1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramGWSLargestContentfulPaint, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramGWSLargestContentfulPaint, 0, 1);
}

TEST_F(GWSPageLoadMetricsObserverTest, CustomUserTimingMark) {
  // No user timing mark. Expecting AFT events are not recorded.
  page_load_metrics::mojom::CustomUserTimingMark timing;

  // Wait until the browser init is complete.
  AfterStartupTaskUtils::SetBrowserStartupIsCompleteForTesting();

  NavigateAndCommit(GURL(kGoogleSearchResultsUrl));
  tester()->SimulateCustomUserTimingUpdate(timing.Clone());
  tester()->histogram_tester().ExpectTotalCount(internal::kHistogramGWSAFTStart,
                                                0);
  tester()->histogram_tester().ExpectTotalCount(internal::kHistogramGWSAFTEnd,
                                                0);

  // Simulate AFT events. This is recorded with expected event name.
  auto timing2 = timing.Clone();
  timing2->mark_name = internal::kGwsAFTStartMarkName;
  timing2->start_time = base::Milliseconds(100);

  auto timing3 = timing.Clone();
  timing3->mark_name = internal::kGwsAFTEndMarkName;
  timing3->start_time = base::Milliseconds(500);

  tester()->SimulateCustomUserTimingUpdate(timing2.Clone());
  tester()->histogram_tester().ExpectTotalCount(internal::kHistogramGWSAFTStart,
                                                1);

  tester()->SimulateCustomUserTimingUpdate(timing2.Clone());
  tester()->SimulateCustomUserTimingUpdate(timing3.Clone());
  tester()->histogram_tester().ExpectTotalCount(internal::kHistogramGWSAFTStart,
                                                2);
  tester()->histogram_tester().ExpectTotalCount(internal::kHistogramGWSAFTEnd,
                                                1);
}

TEST_F(GWSPageLoadMetricsObserverTest, ServiceWorker) {
  page_load_metrics::mojom::PageLoadTiming timing;
  InitializeTestPageLoadTiming(&timing);

  NavigateAndCommit(GURL(kGoogleSearchResultsUrl));
  page_load_metrics::mojom::FrameMetadata metadata;
  metadata.behavior_flags |=
      blink::LoadingBehaviorFlag::kLoadingBehaviorServiceWorkerControlled;
  tester()->SimulateTimingAndMetadataUpdate(timing, metadata);

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramServiceWorkerParseStartSearch, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramServiceWorkerParseStartSearch,
      timing.parse_timing->parse_start.value().InMilliseconds(), 1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramServiceWorkerFirstContentfulPaintSearch, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramServiceWorkerFirstContentfulPaintSearch,
      timing.paint_timing->first_contentful_paint.value().InMilliseconds(), 1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramServiceWorkerParseStartToFirstContentfulPaintSearch,
      1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramServiceWorkerParseStartToFirstContentfulPaintSearch,
      (timing.paint_timing->first_contentful_paint.value() -
       timing.parse_timing->parse_start.value())
          .InMilliseconds(),
      1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramServiceWorkerDomContentLoadedSearch, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramServiceWorkerDomContentLoadedSearch,
      timing.document_timing->dom_content_loaded_event_start.value()
          .InMilliseconds(),
      1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramServiceWorkerLoadSearch, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramServiceWorkerLoadSearch,
      timing.document_timing->load_event_start.value().InMilliseconds(), 1);

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramNoServiceWorkerFirstContentfulPaintSearch, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramNoServiceWorkerParseStartToFirstContentfulPaintSearch,
      0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramNoServiceWorkerDomContentLoadedSearch, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramNoServiceWorkerLoadSearch, 0);
}

TEST_F(GWSPageLoadMetricsObserverTest, NoServiceWorker) {
  page_load_metrics::mojom::PageLoadTiming timing;
  InitializeTestPageLoadTiming(&timing);

  NavigateAndCommit(GURL(kGoogleSearchResultsUrl));
  page_load_metrics::mojom::FrameMetadata metadata;
  tester()->SimulateTimingAndMetadataUpdate(timing, metadata);

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramNoServiceWorkerFirstContentfulPaintSearch, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramNoServiceWorkerFirstContentfulPaintSearch,
      timing.paint_timing->first_contentful_paint.value().InMilliseconds(), 1);

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramNoServiceWorkerParseStartToFirstContentfulPaintSearch,
      1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramNoServiceWorkerParseStartToFirstContentfulPaintSearch,
      (timing.paint_timing->first_contentful_paint.value() -
       timing.parse_timing->parse_start.value())
          .InMilliseconds(),
      1);

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramNoServiceWorkerDomContentLoadedSearch, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramNoServiceWorkerDomContentLoadedSearch,
      timing.document_timing->dom_content_loaded_event_start.value()
          .InMilliseconds(),
      1);

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramNoServiceWorkerLoadSearch, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kHistogramNoServiceWorkerLoadSearch,
      timing.document_timing->load_event_start.value().InMilliseconds(), 1);

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramServiceWorkerParseStartSearch, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramServiceWorkerFirstContentfulPaintSearch, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramServiceWorkerParseStartToFirstContentfulPaintSearch,
      0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramServiceWorkerDomContentLoadedSearch, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramServiceWorkerLoadSearch, 0);
}

TEST_F(GWSPageLoadMetricsObserverTest, FontLoadingMetrics) {
  page_load_metrics::mojom::PageLoadTiming timing;
  InitializeTestPageLoadTiming(&timing);

  auto font_loading_metrics =
      page_load_metrics::mojom::FontLoadingMetrics::New();
  font_loading_metrics->fallback_duration = base::Milliseconds(150);
  font_loading_metrics->fallback_count = 5;
  font_loading_metrics->fallback_initial_duration = base::Milliseconds(42);
  font_loading_metrics->shape_cache_hit_count = 80;
  font_loading_metrics->shape_cache_miss_count = 20;

  // Wait until the browser init is complete.
  AfterStartupTaskUtils::SetBrowserStartupIsCompleteForTesting();

  NavigateAndCommit(GURL(kGoogleSearchResultsUrl));
  tester()->SimulateTimingAndFontLoadingMetricsUpdate(
      timing, std::move(font_loading_metrics));

  // Verify FCP metrics are logged
  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.GoogleSearch.FontLoading.FallbackDuration.FCP", 1);
  tester()->histogram_tester().ExpectBucketCount(
      "PageLoad.Clients.GoogleSearch.FontLoading.FallbackDuration.FCP", 150, 1);

  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.GoogleSearch.FontLoading.FallbackCount.FCP", 1);
  tester()->histogram_tester().ExpectBucketCount(
      "PageLoad.Clients.GoogleSearch.FontLoading.FallbackCount.FCP", 5, 1);

  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.GoogleSearch.FontLoading.ShapeCacheHitRate.FCP", 1);
  tester()->histogram_tester().ExpectBucketCount(
      "PageLoad.Clients.GoogleSearch.FontLoading.ShapeCacheHitRate.FCP", 80, 1);

  // Simulate AFTEnd mark.
  page_load_metrics::mojom::CustomUserTimingMark timing_mark;
  timing_mark.mark_name = internal::kGwsAFTEndMarkName;
  timing_mark.start_time = base::Milliseconds(500);
  tester()->SimulateCustomUserTimingUpdate(timing_mark.Clone());

  // Verify AFTEnd metrics are logged.
  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.GoogleSearch.FontLoading.FallbackDuration.AFTEnd", 1);
  tester()->histogram_tester().ExpectBucketCount(
      "PageLoad.Clients.GoogleSearch.FontLoading.FallbackDuration.AFTEnd", 150,
      1);

  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.GoogleSearch.FontLoading.FallbackCount.AFTEnd", 1);
  tester()->histogram_tester().ExpectBucketCount(
      "PageLoad.Clients.GoogleSearch.FontLoading.FallbackCount.AFTEnd", 5, 1);

  // Navigate again to force Complete logging.
  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.GoogleSearch.FontLoading.FallbackCount.Complete", 1);
  tester()->histogram_tester().ExpectBucketCount(
      "PageLoad.Clients.GoogleSearch.FontLoading.FallbackCount.Complete", 5, 1);

  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.GoogleSearch.FontLoading.FallbackDuration.Complete", 1);
  tester()->histogram_tester().ExpectBucketCount(
      "PageLoad.Clients.GoogleSearch.FontLoading.FallbackDuration.Complete",
      150, 1);

  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.GoogleSearch.FontLoading.InitialFallbackDuration."
      "Complete",
      1);
  tester()->histogram_tester().ExpectBucketCount(
      "PageLoad.Clients.GoogleSearch.FontLoading.InitialFallbackDuration."
      "Complete",
      42, 1);
}
