// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/google/browser/gws_prewarm_page_load_metrics_observer.h"

#include <memory>

#include "base/strings/strcat.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "components/page_load_metrics/browser/fake_page_load_metrics_observer_delegate.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "components/page_load_metrics/common/page_load_metrics.mojom.h"
#include "content/public/test/mock_navigation_handle.h"
#include "net/base/ip_endpoint.h"
#include "net/base/load_timing_info.h"
#include "net/base/net_errors.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"

namespace {

constexpr char kGoogleSearchPrewarmUrl[] =
    "https://www.google.com/search/warmup.html";
constexpr char kGoogleSearchUrl[] = "https://www.google.com/search?q=test";
constexpr char kNonGoogleUrl[] = "https://www.example.com/";

class TestPrewarmObserverDelegate
    : public page_load_metrics::FakePageLoadMetricsObserverDelegate {
 public:
  TestPrewarmObserverDelegate() = default;
  ~TestPrewarmObserverDelegate() override = default;

  std::optional<base::TimeDelta> GetTimeToPageEnd() const override {
    return time_to_page_end_;
  }

  void set_time_to_page_end(std::optional<base::TimeDelta> time_to_page_end) {
    time_to_page_end_ = time_to_page_end;
  }

 private:
  std::optional<base::TimeDelta> time_to_page_end_;
};

page_load_metrics::ExtraRequestCompleteInfo CreateExtraRequestCompleteInfo(
    network::mojom::RequestDestination destination,
    bool was_cached,
    base::TimeTicks request_start,
    base::TimeTicks receive_headers_end,
    int net_error = net::OK) {
  auto load_timing_info = std::make_unique<net::LoadTimingInfo>();
  load_timing_info->request_start = request_start;
  load_timing_info->receive_headers_end = receive_headers_end;
  return page_load_metrics::ExtraRequestCompleteInfo(
      url::SchemeHostPort(GURL("https://www.google.com/test")),
      net::IPEndPoint(), content::FrameTreeNodeId(), was_cached,
      base::ByteSize(100), base::ByteSize(100), destination, net_error,
      std::move(load_timing_info));
}

class GWSPrewarmPageLoadMetricsObserverTest : public testing::Test {
 public:
  GWSPrewarmPageLoadMetricsObserverTest() = default;
};

TEST_F(GWSPrewarmPageLoadMetricsObserverTest, NonPrerenderNavigationIgnored) {
  content::MockNavigationHandle handle(GURL(kGoogleSearchPrewarmUrl),
                                       /*render_frame_host=*/nullptr);
  GWSPrewarmPageLoadMetricsObserver observer;
  EXPECT_EQ(page_load_metrics::PageLoadMetricsObserver::STOP_OBSERVING,
            observer.OnStart(&handle, GURL(), /*started_in_foreground=*/true));
}

TEST_F(GWSPrewarmPageLoadMetricsObserverTest, FencedFramesIgnored) {
  content::MockNavigationHandle handle(GURL(kGoogleSearchPrewarmUrl),
                                       /*render_frame_host=*/nullptr);
  GWSPrewarmPageLoadMetricsObserver observer;
  EXPECT_EQ(page_load_metrics::PageLoadMetricsObserver::STOP_OBSERVING,
            observer.OnFencedFramesStart(&handle, GURL()));
}

TEST_F(GWSPrewarmPageLoadMetricsObserverTest, NonPrewarmPrerenderIgnored) {
  content::MockNavigationHandle handle(GURL(kGoogleSearchUrl),
                                       /*render_frame_host=*/nullptr);
  GWSPrewarmPageLoadMetricsObserver observer;
  EXPECT_EQ(page_load_metrics::PageLoadMetricsObserver::STOP_OBSERVING,
            observer.OnPrerenderStart(&handle, GURL()));
}

TEST_F(GWSPrewarmPageLoadMetricsObserverTest, NonGooglePrerenderIgnored) {
  content::MockNavigationHandle handle(GURL(kNonGoogleUrl),
                                       /*render_frame_host=*/nullptr);
  GWSPrewarmPageLoadMetricsObserver observer;
  EXPECT_EQ(page_load_metrics::PageLoadMetricsObserver::STOP_OBSERVING,
            observer.OnPrerenderStart(&handle, GURL()));
}

TEST_F(GWSPrewarmPageLoadMetricsObserverTest, PrewarmPrerenderObservedCached) {
  base::HistogramTester histogram_tester;
  content::MockNavigationHandle handle(GURL(kGoogleSearchPrewarmUrl),
                                       /*render_frame_host=*/nullptr);
  handle.set_was_response_cached(true);

  GWSPrewarmPageLoadMetricsObserver observer;
  EXPECT_EQ(page_load_metrics::PageLoadMetricsObserver::CONTINUE_OBSERVING,
            observer.OnPrerenderStart(&handle, GURL()));
  EXPECT_TRUE(observer.is_prerendered());

  EXPECT_EQ(page_load_metrics::PageLoadMetricsObserver::CONTINUE_OBSERVING,
            observer.OnCommit(&handle));
  EXPECT_TRUE(observer.was_cached());
  histogram_tester.ExpectUniqueSample(
      internal::kHistogramGWSPrewarmWasResponseCached, true, 1);
}

TEST_F(GWSPrewarmPageLoadMetricsObserverTest,
       PrewarmPrerenderObservedNotCached) {
  base::HistogramTester histogram_tester;
  content::MockNavigationHandle handle(GURL(kGoogleSearchPrewarmUrl),
                                       /*render_frame_host=*/nullptr);
  handle.set_was_response_cached(false);

  GWSPrewarmPageLoadMetricsObserver observer;
  EXPECT_EQ(page_load_metrics::PageLoadMetricsObserver::CONTINUE_OBSERVING,
            observer.OnPrerenderStart(&handle, GURL()));
  EXPECT_TRUE(observer.is_prerendered());

  EXPECT_EQ(page_load_metrics::PageLoadMetricsObserver::CONTINUE_OBSERVING,
            observer.OnCommit(&handle));
  EXPECT_FALSE(observer.was_cached());
  histogram_tester.ExpectUniqueSample(
      internal::kHistogramGWSPrewarmWasResponseCached, false, 1);
}

TEST_F(GWSPrewarmPageLoadMetricsObserverTest,
       RedirectToNonPrewarmUrlStopsObserving) {
  content::MockNavigationHandle start_handle(GURL(kGoogleSearchPrewarmUrl),
                                             /*render_frame_host=*/nullptr);
  GWSPrewarmPageLoadMetricsObserver observer;
  EXPECT_EQ(page_load_metrics::PageLoadMetricsObserver::CONTINUE_OBSERVING,
            observer.OnPrerenderStart(&start_handle, GURL()));
  EXPECT_TRUE(observer.is_prerendered());

  content::MockNavigationHandle redirect_handle(GURL(kNonGoogleUrl),
                                                /*render_frame_host=*/nullptr);
  EXPECT_EQ(page_load_metrics::PageLoadMetricsObserver::STOP_OBSERVING,
            observer.OnRedirect(&redirect_handle));
}

TEST_F(GWSPrewarmPageLoadMetricsObserverTest,
       CommitRedirectToNonPrewarmUrlStopsObserving) {
  content::MockNavigationHandle start_handle(GURL(kGoogleSearchPrewarmUrl),
                                             /*render_frame_host=*/nullptr);
  GWSPrewarmPageLoadMetricsObserver observer;
  EXPECT_EQ(page_load_metrics::PageLoadMetricsObserver::CONTINUE_OBSERVING,
            observer.OnPrerenderStart(&start_handle, GURL()));
  EXPECT_TRUE(observer.is_prerendered());

  content::MockNavigationHandle commit_handle(GURL(kGoogleSearchUrl),
                                              /*render_frame_host=*/nullptr);
  EXPECT_EQ(page_load_metrics::PageLoadMetricsObserver::STOP_OBSERVING,
            observer.OnCommit(&commit_handle));
}

TEST_F(GWSPrewarmPageLoadMetricsObserverTest, DOMAndLoadEventsRecorded) {
  base::HistogramTester histogram_tester;
  GWSPrewarmPageLoadMetricsObserver observer;

  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::mojom::DocumentTiming doc_timing;
  doc_timing.dom_content_loaded_event_start = base::Milliseconds(250);
  doc_timing.load_event_start = base::Milliseconds(550);
  timing.document_timing = doc_timing.Clone();

  observer.OnDomContentLoadedEventStart(timing);
  observer.OnLoadEventStart(timing);

  histogram_tester.ExpectUniqueTimeSample(
      internal::kHistogramGWSPrewarmNavigationToDomContentLoaded,
      base::Milliseconds(250), 1);
  histogram_tester.ExpectUniqueTimeSample(
      internal::kHistogramGWSPrewarmNavigationToLoadEvent,
      base::Milliseconds(550), 1);
}

TEST_F(GWSPrewarmPageLoadMetricsObserverTest, SubresourceLoadsObserved) {
  base::HistogramTester histogram_tester;
  TestPrewarmObserverDelegate delegate;
  GWSPrewarmPageLoadMetricsObserver observer;
  observer.SetDelegate(&delegate);

  base::TimeTicks nav_start = delegate.GetNavigationStart();

  // Script loaded from cache.
  auto script_info = CreateExtraRequestCompleteInfo(
      network::mojom::RequestDestination::kScript, /*was_cached=*/true,
      nav_start + base::Milliseconds(50), nav_start + base::Milliseconds(120));
  observer.OnLoadedResource(script_info);

  // Style loaded from network.
  auto style_info = CreateExtraRequestCompleteInfo(
      network::mojom::RequestDestination::kStyle, /*was_cached=*/false,
      nav_start + base::Milliseconds(60), nav_start + base::Milliseconds(200));
  observer.OnLoadedResource(style_info);

  // Image loaded from cache.
  auto image_info = CreateExtraRequestCompleteInfo(
      network::mojom::RequestDestination::kImage, /*was_cached=*/true,
      nav_start + base::Milliseconds(100), nav_start + base::Milliseconds(300));
  observer.OnLoadedResource(image_info);

  // Document load is ignored for subresource metrics.
  auto doc_info = CreateExtraRequestCompleteInfo(
      network::mojom::RequestDestination::kDocument, /*was_cached=*/false,
      nav_start, nav_start + base::Milliseconds(50));
  observer.OnLoadedResource(doc_info);

  histogram_tester.ExpectBucketCount(
      internal::kHistogramGWSPrewarmSubresourceDestination,
      network::mojom::RequestDestination::kScript, 1);
  histogram_tester.ExpectBucketCount(
      internal::kHistogramGWSPrewarmSubresourceDestination,
      network::mojom::RequestDestination::kStyle, 1);
  histogram_tester.ExpectBucketCount(
      internal::kHistogramGWSPrewarmSubresourceDestination,
      network::mojom::RequestDestination::kImage, 1);
  histogram_tester.ExpectBucketCount(
      internal::kHistogramGWSPrewarmSubresourceDestination,
      network::mojom::RequestDestination::kDocument, 0);

  histogram_tester.ExpectBucketCount(
      base::StrCat({internal::kHistogramGWSPrewarmSubresourceDestination,
                    internal::kSubresourceSuffixCached}),
      network::mojom::RequestDestination::kScript, 1);
  histogram_tester.ExpectBucketCount(
      base::StrCat({internal::kHistogramGWSPrewarmSubresourceDestination,
                    internal::kSubresourceSuffixCached}),
      network::mojom::RequestDestination::kImage, 1);
  histogram_tester.ExpectBucketCount(
      base::StrCat({internal::kHistogramGWSPrewarmSubresourceDestination,
                    internal::kSubresourceSuffixNotCached}),
      network::mojom::RequestDestination::kStyle, 1);

  histogram_tester.ExpectTotalCount(
      internal::kHistogramGWSPrewarmNavigationToLastSubresourceLoad, 0);
}

TEST_F(GWSPrewarmPageLoadMetricsObserverTest,
       CancellationAfterCommitWithLoadEvent) {
  base::HistogramTester histogram_tester;
  TestPrewarmObserverDelegate delegate;
  delegate.set_time_to_page_end(base::Milliseconds(1500));
  GWSPrewarmPageLoadMetricsObserver observer;
  observer.SetDelegate(&delegate);

  base::TimeTicks nav_start = delegate.GetNavigationStart();

  content::MockNavigationHandle start_handle(GURL(kGoogleSearchPrewarmUrl),
                                             /*render_frame_host=*/nullptr);
  EXPECT_EQ(page_load_metrics::PageLoadMetricsObserver::CONTINUE_OBSERVING,
            observer.OnPrerenderStart(&start_handle, GURL()));
  EXPECT_EQ(page_load_metrics::PageLoadMetricsObserver::CONTINUE_OBSERVING,
            observer.OnCommit(&start_handle));

  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::mojom::DocumentTiming doc_timing;
  doc_timing.load_event_start = base::Milliseconds(600);
  timing.document_timing = doc_timing.Clone();
  observer.OnLoadEventStart(timing);

  auto script_info = CreateExtraRequestCompleteInfo(
      network::mojom::RequestDestination::kScript, /*was_cached=*/true,
      nav_start + base::Milliseconds(50), nav_start + base::Milliseconds(500));
  observer.OnLoadedResource(script_info);

  auto style_info = CreateExtraRequestCompleteInfo(
      network::mojom::RequestDestination::kStyle, /*was_cached=*/false,
      nav_start + base::Milliseconds(60), nav_start + base::Milliseconds(580));
  observer.OnLoadedResource(style_info);

  observer.OnComplete(timing);

  histogram_tester.ExpectUniqueTimeSample(
      internal::kHistogramGWSPrewarmNavigationToCancellation,
      base::Milliseconds(1500), 1);
  histogram_tester.ExpectUniqueSample(
      internal::kHistogramGWSPrewarmLoadEventOccurredBeforeCancellation, true,
      1);
  histogram_tester.ExpectUniqueTimeSample(
      internal::kHistogramGWSPrewarmLoadEventToCancellation,
      base::Milliseconds(900), 1);
  histogram_tester.ExpectUniqueTimeSample(
      internal::kHistogramGWSPrewarmNavigationToLastSubresourceLoad,
      base::Milliseconds(580), 1);
  histogram_tester.ExpectUniqueTimeSample(
      internal::kHistogramGWSPrewarmLastSubresourceLoadToCancellation,
      base::Milliseconds(920), 1);

  histogram_tester.ExpectUniqueSample(
      internal::kHistogramGWSPrewarmSubresourceCount, 2, 1);
  histogram_tester.ExpectUniqueSample(
      base::StrCat({internal::kHistogramGWSPrewarmSubresourceCount,
                    internal::kSubresourceSuffixCached}),
      1, 1);
  histogram_tester.ExpectUniqueSample(
      base::StrCat({internal::kHistogramGWSPrewarmSubresourceCount,
                    internal::kSubresourceSuffixHttpCached}),
      1, 1);
  histogram_tester.ExpectUniqueSample(
      base::StrCat({internal::kHistogramGWSPrewarmSubresourceCount,
                    internal::kSubresourceSuffixMemoryCached}),
      0, 1);
  histogram_tester.ExpectUniqueSample(
      base::StrCat({internal::kHistogramGWSPrewarmSubresourceCount,
                    internal::kSubresourceSuffixNotCached}),
      1, 1);
}

TEST_F(GWSPrewarmPageLoadMetricsObserverTest,
       CancellationAfterCommitWithoutLoadEvent) {
  base::HistogramTester histogram_tester;
  TestPrewarmObserverDelegate delegate;
  delegate.set_time_to_page_end(base::Milliseconds(400));
  GWSPrewarmPageLoadMetricsObserver observer;
  observer.SetDelegate(&delegate);

  content::MockNavigationHandle start_handle(GURL(kGoogleSearchPrewarmUrl),
                                             /*render_frame_host=*/nullptr);
  EXPECT_EQ(page_load_metrics::PageLoadMetricsObserver::CONTINUE_OBSERVING,
            observer.OnPrerenderStart(&start_handle, GURL()));
  EXPECT_EQ(page_load_metrics::PageLoadMetricsObserver::CONTINUE_OBSERVING,
            observer.OnCommit(&start_handle));

  page_load_metrics::mojom::PageLoadTiming timing;
  observer.OnComplete(timing);

  histogram_tester.ExpectUniqueTimeSample(
      internal::kHistogramGWSPrewarmNavigationToCancellation,
      base::Milliseconds(400), 1);
  histogram_tester.ExpectUniqueSample(
      internal::kHistogramGWSPrewarmLoadEventOccurredBeforeCancellation, false,
      1);
  histogram_tester.ExpectTotalCount(
      internal::kHistogramGWSPrewarmLoadEventToCancellation, 0);
  histogram_tester.ExpectUniqueSample(
      internal::kHistogramGWSPrewarmSubresourceCount, 0, 1);
}

TEST_F(GWSPrewarmPageLoadMetricsObserverTest,
       CancellationBeforeCommitFailedProvisionalLoad) {
  base::HistogramTester histogram_tester;
  TestPrewarmObserverDelegate delegate;
  GWSPrewarmPageLoadMetricsObserver observer;
  observer.SetDelegate(&delegate);

  page_load_metrics::FailedProvisionalLoadInfo failed_info(
      base::Milliseconds(350), net::ERR_ABORTED, /*net_extended_error_code=*/0,
      /*error_navigation_trigger=*/std::nullopt,
      content::NavigationDiscardReason::kExplicitCancellation);

  observer.OnFailedProvisionalLoad(failed_info);

  histogram_tester.ExpectUniqueTimeSample(
      internal::kHistogramGWSPrewarmNavigationToCancellation,
      base::Milliseconds(350), 1);
  histogram_tester.ExpectUniqueSample(
      internal::kHistogramGWSPrewarmLoadEventOccurredBeforeCancellation, false,
      1);
}

TEST_F(GWSPrewarmPageLoadMetricsObserverTest, DidLoadResourceFromMemoryCache) {
  base::HistogramTester histogram_tester;
  TestPrewarmObserverDelegate delegate;
  delegate.navigation_start_ = base::TimeTicks::Now();
  delegate.set_time_to_page_end(base::Milliseconds(1200));
  GWSPrewarmPageLoadMetricsObserver observer;
  observer.SetDelegate(&delegate);

  content::MockNavigationHandle start_handle(GURL(kGoogleSearchPrewarmUrl),
                                             /*render_frame_host=*/nullptr);
  EXPECT_EQ(page_load_metrics::PageLoadMetricsObserver::CONTINUE_OBSERVING,
            observer.OnPrerenderStart(&start_handle, GURL()));
  EXPECT_EQ(page_load_metrics::PageLoadMetricsObserver::CONTINUE_OBSERVING,
            observer.OnCommit(&start_handle));

  page_load_metrics::MemoryResourceLoadInfo memory_resource(
      url::SchemeHostPort(GURL("https://www.google.com/test.js")),
      network::mojom::RequestDestination::kScript, content::FrameTreeNodeId(1));
  observer.DidLoadResourceFromMemoryCache(memory_resource);

  histogram_tester.ExpectUniqueSample(
      internal::kHistogramGWSPrewarmSubresourceDestination,
      network::mojom::RequestDestination::kScript, 1);
  histogram_tester.ExpectUniqueSample(
      base::StrCat({internal::kHistogramGWSPrewarmSubresourceDestination,
                    internal::kSubresourceSuffixCached}),
      network::mojom::RequestDestination::kScript, 1);
  histogram_tester.ExpectUniqueSample(
      base::StrCat({internal::kHistogramGWSPrewarmSubresourceDestination,
                    internal::kSubresourceSuffixMemoryCached}),
      network::mojom::RequestDestination::kScript, 1);
  histogram_tester.ExpectTotalCount(
      base::StrCat({internal::kHistogramGWSPrewarmSubresourceDestination,
                    internal::kSubresourceSuffixHttpCached}),
      0);
  histogram_tester.ExpectTotalCount(
      base::StrCat({internal::kHistogramGWSPrewarmSubresourceDestination,
                    internal::kSubresourceSuffixNotCached}),
      0);

  page_load_metrics::mojom::PageLoadTiming timing;
  observer.OnComplete(timing);

  histogram_tester.ExpectUniqueSample(
      internal::kHistogramGWSPrewarmSubresourceCount, 1, 1);
  histogram_tester.ExpectUniqueSample(
      base::StrCat({internal::kHistogramGWSPrewarmSubresourceCount,
                    internal::kSubresourceSuffixCached}),
      1, 1);
  histogram_tester.ExpectUniqueSample(
      base::StrCat({internal::kHistogramGWSPrewarmSubresourceCount,
                    internal::kSubresourceSuffixHttpCached}),
      0, 1);
  histogram_tester.ExpectUniqueSample(
      base::StrCat({internal::kHistogramGWSPrewarmSubresourceCount,
                    internal::kSubresourceSuffixMemoryCached}),
      1, 1);
  histogram_tester.ExpectUniqueSample(
      base::StrCat({internal::kHistogramGWSPrewarmSubresourceCount,
                    internal::kSubresourceSuffixNotCached}),
      0, 1);
  histogram_tester.ExpectTotalCount(
      internal::kHistogramGWSPrewarmNavigationToLastSubresourceLoad, 0);
  histogram_tester.ExpectTotalCount(
      internal::kHistogramGWSPrewarmLastSubresourceLoadToCancellation, 0);
}

TEST_F(GWSPrewarmPageLoadMetricsObserverTest, SubresourceLoadOutOfOrder) {
  base::HistogramTester histogram_tester;
  TestPrewarmObserverDelegate delegate;
  base::TimeTicks now = base::TimeTicks::Now();
  delegate.navigation_start_ = now;
  delegate.set_time_to_page_end(base::Milliseconds(1200));
  GWSPrewarmPageLoadMetricsObserver observer;
  observer.SetDelegate(&delegate);

  content::MockNavigationHandle start_handle(GURL(kGoogleSearchPrewarmUrl),
                                             /*render_frame_host=*/nullptr);
  EXPECT_EQ(page_load_metrics::PageLoadMetricsObserver::CONTINUE_OBSERVING,
            observer.OnPrerenderStart(&start_handle, GURL()));
  EXPECT_EQ(page_load_metrics::PageLoadMetricsObserver::CONTINUE_OBSERVING,
            observer.OnCommit(&start_handle));

  // Resource 1 finishes at 800ms.
  observer.OnLoadedResource(CreateExtraRequestCompleteInfo(
      network::mojom::RequestDestination::kScript, /*was_cached=*/false, now,
      now + base::Milliseconds(800)));

  // Resource 2 finishes at 400ms (completes after Resource 1 in arrival order,
  // but earlier in time).
  observer.OnLoadedResource(CreateExtraRequestCompleteInfo(
      network::mojom::RequestDestination::kScript, /*was_cached=*/false, now,
      now + base::Milliseconds(400)));

  page_load_metrics::mojom::PageLoadTiming timing;
  observer.OnComplete(timing);

  // NavigationToLastSubresourceLoad should be max(800ms, 400ms) = 800ms.
  histogram_tester.ExpectUniqueSample(
      internal::kHistogramGWSPrewarmNavigationToLastSubresourceLoad, 800, 1);
  // LastSubresourceLoadToCancellation should be 1200ms - 800ms = 400ms.
  histogram_tester.ExpectUniqueSample(
      internal::kHistogramGWSPrewarmLastSubresourceLoadToCancellation, 400, 1);
}

TEST_F(GWSPrewarmPageLoadMetricsObserverTest, FailedSubresourceLoadIgnored) {
  base::HistogramTester histogram_tester;
  TestPrewarmObserverDelegate delegate;
  delegate.set_time_to_page_end(base::Milliseconds(1000));
  GWSPrewarmPageLoadMetricsObserver observer;
  observer.SetDelegate(&delegate);

  base::TimeTicks now = delegate.GetNavigationStart();

  content::MockNavigationHandle start_handle(GURL(kGoogleSearchPrewarmUrl),
                                             /*render_frame_host=*/nullptr);
  EXPECT_EQ(page_load_metrics::PageLoadMetricsObserver::CONTINUE_OBSERVING,
            observer.OnPrerenderStart(&start_handle, GURL()));
  EXPECT_EQ(page_load_metrics::PageLoadMetricsObserver::CONTINUE_OBSERVING,
            observer.OnCommit(&start_handle));

  // Failed resource with net::ERR_ABORTED.
  observer.OnLoadedResource(CreateExtraRequestCompleteInfo(
      network::mojom::RequestDestination::kScript, /*was_cached=*/false, now,
      now + base::Milliseconds(300), net::ERR_ABORTED));

  // Succeeded resource with net::OK.
  observer.OnLoadedResource(CreateExtraRequestCompleteInfo(
      network::mojom::RequestDestination::kStyle, /*was_cached=*/true, now,
      now + base::Milliseconds(500), net::OK));

  page_load_metrics::mojom::PageLoadTiming timing;
  observer.OnComplete(timing);

  // Only the successful style resource should be counted.
  histogram_tester.ExpectUniqueSample(
      internal::kHistogramGWSPrewarmSubresourceCount, 1, 1);
  histogram_tester.ExpectUniqueSample(
      base::StrCat({internal::kHistogramGWSPrewarmSubresourceCount,
                    internal::kSubresourceSuffixCached}),
      1, 1);
  histogram_tester.ExpectUniqueSample(
      base::StrCat({internal::kHistogramGWSPrewarmSubresourceCount,
                    internal::kSubresourceSuffixNotCached}),
      0, 1);

  // SubresourceDestination is recorded per completed subresource.
  histogram_tester.ExpectUniqueSample(
      internal::kHistogramGWSPrewarmSubresourceDestination,
      network::mojom::RequestDestination::kStyle, 1);

  // NavigationToLastSubresourceLoad should be 500ms (from style), not 300ms.
  histogram_tester.ExpectUniqueSample(
      internal::kHistogramGWSPrewarmNavigationToLastSubresourceLoad, 500, 1);
  histogram_tester.ExpectUniqueSample(
      internal::kHistogramGWSPrewarmLastSubresourceLoadToCancellation, 500, 1);
}

TEST_F(GWSPrewarmPageLoadMetricsObserverTest, DidActivatePrerenderedPage) {
  base::HistogramTester histogram_tester;
  TestPrewarmObserverDelegate delegate;
  GWSPrewarmPageLoadMetricsObserver observer;
  observer.SetDelegate(&delegate);
  content::MockNavigationHandle handle;
  observer.DidActivatePrerenderedPage(&handle);
  EXPECT_TRUE(observer.was_activated());

  // Session end records WasActivated = true and skips cancellation histograms
  // and subresource counts.
  page_load_metrics::mojom::PageLoadTiming timing;
  observer.OnComplete(timing);
  histogram_tester.ExpectUniqueSample(
      internal::kHistogramGWSPrewarmWasActivated, true, 1);
  histogram_tester.ExpectTotalCount(
      internal::kHistogramGWSPrewarmNavigationToCancellation, 0);
  histogram_tester.ExpectTotalCount(
      internal::kHistogramGWSPrewarmSubresourceCount, 0);
}

TEST_F(GWSPrewarmPageLoadMetricsObserverTest,
       WasActivatedFalseOnNormalCancellation) {
  base::HistogramTester histogram_tester;
  TestPrewarmObserverDelegate delegate;
  delegate.set_time_to_page_end(base::Milliseconds(500));
  GWSPrewarmPageLoadMetricsObserver observer;
  observer.SetDelegate(&delegate);

  page_load_metrics::mojom::PageLoadTiming timing;
  observer.OnComplete(timing);

  histogram_tester.ExpectUniqueSample(
      internal::kHistogramGWSPrewarmWasActivated, false, 1);
}

TEST_F(GWSPrewarmPageLoadMetricsObserverTest,
       DOMAndLoadEventsNullDocumentTiming) {
  base::HistogramTester histogram_tester;
  GWSPrewarmPageLoadMetricsObserver observer;

  page_load_metrics::mojom::PageLoadTiming timing;
  // timing.document_timing is null by default.

  observer.OnDomContentLoadedEventStart(timing);
  observer.OnLoadEventStart(timing);

  histogram_tester.ExpectTotalCount(
      internal::kHistogramGWSPrewarmNavigationToDomContentLoaded, 0);
  histogram_tester.ExpectTotalCount(
      internal::kHistogramGWSPrewarmNavigationToLoadEvent, 0);
}

TEST_F(GWSPrewarmPageLoadMetricsObserverTest,
       DOMAndLoadEventsNegativeDocumentTimingIgnored) {
  base::HistogramTester histogram_tester;
  GWSPrewarmPageLoadMetricsObserver observer;

  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::mojom::DocumentTiming doc_timing;
  doc_timing.dom_content_loaded_event_start = base::Milliseconds(-100);
  doc_timing.load_event_start = base::Milliseconds(-50);
  timing.document_timing = doc_timing.Clone();

  observer.OnDomContentLoadedEventStart(timing);
  observer.OnLoadEventStart(timing);

  histogram_tester.ExpectTotalCount(
      internal::kHistogramGWSPrewarmNavigationToDomContentLoaded, 0);
  histogram_tester.ExpectTotalCount(
      internal::kHistogramGWSPrewarmNavigationToLoadEvent, 0);
}

TEST_F(GWSPrewarmPageLoadMetricsObserverTest,
       EventsAndSubresourcesIgnoredAfterActivation) {
  base::HistogramTester histogram_tester;
  TestPrewarmObserverDelegate delegate;
  GWSPrewarmPageLoadMetricsObserver observer;
  observer.SetDelegate(&delegate);

  content::MockNavigationHandle handle;
  observer.DidActivatePrerenderedPage(&handle);
  EXPECT_TRUE(observer.was_activated());

  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::mojom::DocumentTiming doc_timing;
  doc_timing.dom_content_loaded_event_start = base::Milliseconds(250);
  doc_timing.load_event_start = base::Milliseconds(550);
  timing.document_timing = doc_timing.Clone();

  observer.OnDomContentLoadedEventStart(timing);
  observer.OnLoadEventStart(timing);

  auto script_info = CreateExtraRequestCompleteInfo(
      network::mojom::RequestDestination::kScript, /*was_cached=*/true,
      delegate.GetNavigationStart() + base::Milliseconds(50),
      delegate.GetNavigationStart() + base::Milliseconds(120));
  observer.OnLoadedResource(script_info);

  page_load_metrics::MemoryResourceLoadInfo memory_resource(
      url::SchemeHostPort(GURL("https://www.google.com/test.js")),
      network::mojom::RequestDestination::kScript, content::FrameTreeNodeId(1));
  observer.DidLoadResourceFromMemoryCache(memory_resource);

  histogram_tester.ExpectTotalCount(
      internal::kHistogramGWSPrewarmNavigationToDomContentLoaded, 0);
  histogram_tester.ExpectTotalCount(
      internal::kHistogramGWSPrewarmNavigationToLoadEvent, 0);
  histogram_tester.ExpectTotalCount(
      internal::kHistogramGWSPrewarmSubresourceDestination, 0);
}

TEST_F(GWSPrewarmPageLoadMetricsObserverTest, EventsAfterCancellationDropped) {
  base::HistogramTester histogram_tester;
  TestPrewarmObserverDelegate delegate;
  // Cancellation happened at 500ms.
  delegate.set_time_to_page_end(base::Milliseconds(500));
  GWSPrewarmPageLoadMetricsObserver observer;
  observer.SetDelegate(&delegate);

  base::TimeTicks nav_start = delegate.GetNavigationStart();

  content::MockNavigationHandle start_handle(GURL(kGoogleSearchPrewarmUrl),
                                             /*render_frame_host=*/nullptr);
  EXPECT_EQ(page_load_metrics::PageLoadMetricsObserver::CONTINUE_OBSERVING,
            observer.OnPrerenderStart(&start_handle, GURL()));
  EXPECT_EQ(page_load_metrics::PageLoadMetricsObserver::CONTINUE_OBSERVING,
            observer.OnCommit(&start_handle));

  // Load event occurred at 600ms (after cancellation at 500ms due to IPC race).
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::mojom::DocumentTiming doc_timing;
  doc_timing.load_event_start = base::Milliseconds(600);
  timing.document_timing = doc_timing.Clone();
  observer.OnLoadEventStart(timing);

  // Subresource completed at 700ms (after cancellation at 500ms).
  auto script_info = CreateExtraRequestCompleteInfo(
      network::mojom::RequestDestination::kScript, /*was_cached=*/false,
      nav_start + base::Milliseconds(50), nav_start + base::Milliseconds(700));
  observer.OnLoadedResource(script_info);

  observer.OnComplete(timing);

  histogram_tester.ExpectUniqueTimeSample(
      internal::kHistogramGWSPrewarmNavigationToCancellation,
      base::Milliseconds(500), 1);
  // LoadEventOccurredBeforeCancellation must record false because load event
  // was after cancellation.
  histogram_tester.ExpectUniqueSample(
      internal::kHistogramGWSPrewarmLoadEventOccurredBeforeCancellation, false,
      1);
  // Headroom histograms must not be recorded.
  histogram_tester.ExpectTotalCount(
      internal::kHistogramGWSPrewarmLoadEventToCancellation, 0);
  histogram_tester.ExpectTotalCount(
      internal::kHistogramGWSPrewarmNavigationToLastSubresourceLoad, 0);
  histogram_tester.ExpectTotalCount(
      internal::kHistogramGWSPrewarmLastSubresourceLoadToCancellation, 0);
}

}  // namespace
