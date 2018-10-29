// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/metrics_web_contents_observer.h"

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/process/kill.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "base/timer/mock_timer.h"
#include "chrome/browser/page_load_metrics/metrics_navigation_throttle.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_embedder_interface.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/page_load_tracker.h"
#include "chrome/common/page_load_metrics/test/weak_mock_timer.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/buildflags/buildflags.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/value_builder.h"
#endif

using content::NavigationSimulator;

namespace page_load_metrics {

namespace {

const char kDefaultTestUrl[] = "https://google.com/";
const char kDefaultTestUrlAnchor[] = "https://google.com/#samedocument";
const char kDefaultTestUrl2[] = "https://whatever.com/";
const char kFilteredStartUrl[] = "https://whatever.com/ignore-on-start";
const char kFilteredCommitUrl[] = "https://whatever.com/ignore-on-commit";

// Simple PageLoadMetricsObserver that copies observed PageLoadTimings into the
// provided std::vector, so they can be analyzed by unit tests.
class TestPageLoadMetricsObserver : public PageLoadMetricsObserver {
 public:
  TestPageLoadMetricsObserver(
      std::vector<mojom::PageLoadTimingPtr>* updated_timings,
      std::vector<mojom::PageLoadTimingPtr>* updated_subframe_timings,
      std::vector<mojom::PageLoadTimingPtr>* complete_timings,
      std::vector<ExtraRequestCompleteInfo>* loaded_resources,
      std::vector<GURL>* observed_committed_urls,
      std::vector<GURL>* observed_aborted_urls,
      std::vector<mojom::PageLoadFeatures>* observed_features)
      : updated_timings_(updated_timings),
        updated_subframe_timings_(updated_subframe_timings),
        complete_timings_(complete_timings),
        loaded_resources_(loaded_resources),
        observed_features_(observed_features),
        observed_committed_urls_(observed_committed_urls),
        observed_aborted_urls_(observed_aborted_urls) {}

  ObservePolicy OnStart(content::NavigationHandle* navigation_handle,
               const GURL& currently_committed_url,
               bool started_in_foreground) override {
    observed_committed_urls_->push_back(currently_committed_url);
    return CONTINUE_OBSERVING;
  }

  void OnTimingUpdate(content::RenderFrameHost* subframe_rfh,
                      const mojom::PageLoadTiming& timing,
                      const PageLoadExtraInfo& extra_info) override {
    if (subframe_rfh) {
      DCHECK(subframe_rfh->GetParent());
      updated_subframe_timings_->push_back(timing.Clone());
    } else {
      updated_timings_->push_back(timing.Clone());
    }
  }

  void OnComplete(const mojom::PageLoadTiming& timing,
                  const PageLoadExtraInfo& extra_info) override {
    complete_timings_->push_back(timing.Clone());
  }

  ObservePolicy FlushMetricsOnAppEnterBackground(
      const mojom::PageLoadTiming& timing,
      const PageLoadExtraInfo& extra_info) override {
    return STOP_OBSERVING;
  }

  void OnLoadedResource(
      const ExtraRequestCompleteInfo& extra_request_complete_info) override {
    loaded_resources_->emplace_back(extra_request_complete_info);
  }

  void OnFeaturesUsageObserved(const mojom::PageLoadFeatures& features,
                               const PageLoadExtraInfo& extra_info) override {
    observed_features_->push_back(features);
  }

  void OnDidInternalNavigationAbort(
      content::NavigationHandle* navigation_handle) override {
    observed_aborted_urls_->push_back(navigation_handle->GetURL());
  }

 private:
  std::vector<mojom::PageLoadTimingPtr>* const updated_timings_;
  std::vector<mojom::PageLoadTimingPtr>* const updated_subframe_timings_;
  std::vector<mojom::PageLoadTimingPtr>* const complete_timings_;
  std::vector<ExtraRequestCompleteInfo>* const loaded_resources_;
  std::vector<mojom::PageLoadFeatures>* const observed_features_;
  std::vector<GURL>* const observed_committed_urls_;
  std::vector<GURL>* const observed_aborted_urls_;
};

// Test PageLoadMetricsObserver that stops observing page loads with certain
// substrings in the URL.
class FilteringPageLoadMetricsObserver : public PageLoadMetricsObserver {
 public:
  explicit FilteringPageLoadMetricsObserver(
      std::vector<GURL>* completed_filtered_urls)
      : completed_filtered_urls_(completed_filtered_urls) {}

  ObservePolicy OnStart(content::NavigationHandle* handle,
                        const GURL& currently_committed_url,
                        bool started_in_foreground) override {
    const bool should_ignore =
        handle->GetURL().spec().find("ignore-on-start") != std::string::npos;
    return should_ignore ? STOP_OBSERVING : CONTINUE_OBSERVING;
  }

  ObservePolicy OnCommit(content::NavigationHandle* handle,
                         ukm::SourceId source_id) override {
    const bool should_ignore =
        handle->GetURL().spec().find("ignore-on-commit") != std::string::npos;
    return should_ignore ? STOP_OBSERVING : CONTINUE_OBSERVING;
  }

  void OnComplete(const mojom::PageLoadTiming& timing,
                  const PageLoadExtraInfo& extra_info) override {
    completed_filtered_urls_->push_back(extra_info.url);
  }

 private:
  std::vector<GURL>* const completed_filtered_urls_;
};

class TestPageLoadMetricsEmbedderInterface
    : public PageLoadMetricsEmbedderInterface,
      public test::WeakMockTimerProvider {
 public:
  TestPageLoadMetricsEmbedderInterface() : is_ntp_(false) {}

  bool IsNewTabPageUrl(const GURL& url) override { return is_ntp_; }
  void set_is_ntp(bool is_ntp) { is_ntp_ = is_ntp; }
  void RegisterObservers(PageLoadTracker* tracker) override {
    tracker->AddObserver(std::make_unique<TestPageLoadMetricsObserver>(
        &updated_timings_, &updated_subframe_timings_, &complete_timings_,
        &loaded_resources_, &observed_committed_urls_, &observed_aborted_urls_,
        &observed_features_));
    tracker->AddObserver(std::make_unique<FilteringPageLoadMetricsObserver>(
        &completed_filtered_urls_));
  }
  std::unique_ptr<base::OneShotTimer> CreateTimer() override {
    auto timer = std::make_unique<test::WeakMockTimer>();
    SetMockTimer(timer->AsWeakPtr());
    return std::move(timer);
  }
  const std::vector<mojom::PageLoadTimingPtr>& updated_timings() const {
    return updated_timings_;
  }
  const std::vector<mojom::PageLoadTimingPtr>& complete_timings() const {
    return complete_timings_;
  }
  const std::vector<mojom::PageLoadTimingPtr>& updated_subframe_timings()
      const {
    return updated_subframe_timings_;
  }

  // currently_committed_urls passed to OnStart().
  const std::vector<GURL>& observed_committed_urls_from_on_start() const {
    return observed_committed_urls_;
  }

  const std::vector<GURL>& observed_aborted_urls() const {
    return observed_aborted_urls_;
  }

  const std::vector<mojom::PageLoadFeatures>& observed_features() const {
    return observed_features_;
  }

  const std::vector<ExtraRequestCompleteInfo>& loaded_resources() const {
    return loaded_resources_;
  }

  // committed URLs passed to FilteringPageLoadMetricsObserver::OnComplete().
  const std::vector<GURL>& completed_filtered_urls() const {
    return completed_filtered_urls_;
  }

 private:
  std::vector<mojom::PageLoadTimingPtr> updated_timings_;
  std::vector<mojom::PageLoadTimingPtr> updated_subframe_timings_;
  std::vector<mojom::PageLoadTimingPtr> complete_timings_;
  std::vector<GURL> observed_committed_urls_;
  std::vector<GURL> observed_aborted_urls_;
  std::vector<ExtraRequestCompleteInfo> loaded_resources_;
  std::vector<GURL> completed_filtered_urls_;
  std::vector<mojom::PageLoadFeatures> observed_features_;
  bool is_ntp_;
};

void PopulatePageLoadTiming(mojom::PageLoadTiming* timing) {
  page_load_metrics::InitPageLoadTimingForTest(timing);
  timing->navigation_start = base::Time::FromDoubleT(1);
  timing->response_start = base::TimeDelta::FromMilliseconds(10);
  timing->parse_timing->parse_start = base::TimeDelta::FromMilliseconds(20);
  timing->document_timing->first_layout = base::TimeDelta::FromMilliseconds(30);
}

}  //  namespace

class MetricsWebContentsObserverTest : public ChromeRenderViewHostTestHarness {
 public:
  MetricsWebContentsObserverTest() : num_errors_(0) {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    AttachObserver();
  }

  void NavigateToUntrackedUrl() {
    content::WebContentsTester::For(web_contents())
        ->NavigateAndCommit(GURL(url::kAboutBlankURL));
  }

  // Returns the mock timer used for buffering updates in the
  // PageLoadMetricsUpdateDispatcher.
  base::MockOneShotTimer* GetMostRecentTimer() {
    return embedder_interface_->GetMockTimer();
  }

  void SimulateTimingUpdate(const mojom::PageLoadTiming& timing) {
    SimulateTimingUpdate(timing, web_contents()->GetMainFrame());
  }

  void SimulateTimingUpdate(const mojom::PageLoadTiming& timing,
                            content::RenderFrameHost* render_frame_host) {
    SimulateTimingUpdateWithoutFiringDispatchTimer(timing, render_frame_host);
    // If sending the timing update caused the PageLoadMetricsUpdateDispatcher
    // to schedule a buffering timer, then fire it now so metrics are dispatched
    // to observers.
    base::MockOneShotTimer* mock_timer = GetMostRecentTimer();
    if (mock_timer && mock_timer->IsRunning())
      mock_timer->Fire();
  }

  void SimulateTimingUpdateWithoutFiringDispatchTimer(
      const mojom::PageLoadTiming& timing,
      content::RenderFrameHost* render_frame_host) {
    observer()->OnTimingUpdated(
        render_frame_host, timing, mojom::PageLoadMetadata(),
        mojom::PageLoadFeatures(), std::vector<mojom::ResourceDataUpdatePtr>());
  }

  void AttachObserver() {
    auto embedder_interface =
        std::make_unique<TestPageLoadMetricsEmbedderInterface>();
    embedder_interface_ = embedder_interface.get();
    MetricsWebContentsObserver* observer =
        MetricsWebContentsObserver::CreateForWebContents(
            web_contents(), std::move(embedder_interface));
    observer->OnVisibilityChanged(content::Visibility::VISIBLE);
  }

  void CheckErrorEvent(InternalErrorLoadEvent error, int count) {
    histogram_tester_.ExpectBucketCount(internal::kErrorEvents, error, count);
    num_errors_ += count;
  }

  void CheckTotalErrorEvents() {
    histogram_tester_.ExpectTotalCount(internal::kErrorEvents, num_errors_);
  }

  void CheckNoErrorEvents() {
    histogram_tester_.ExpectTotalCount(internal::kErrorEvents, 0);
  }

  int CountEmptyCompleteTimingReported() {
    int empty = 0;
    for (const auto& timing : embedder_interface_->complete_timings()) {
      if (page_load_metrics::IsEmpty(*timing))
        ++empty;
    }
    return empty;
  }

  const std::vector<mojom::PageLoadTimingPtr>& updated_timings() const {
    return embedder_interface_->updated_timings();
  }
  const std::vector<mojom::PageLoadTimingPtr>& complete_timings() const {
    return embedder_interface_->complete_timings();
  }
  const std::vector<mojom::PageLoadTimingPtr>& updated_subframe_timings()
      const {
    return embedder_interface_->updated_subframe_timings();
  }
  int CountCompleteTimingReported() { return complete_timings().size(); }
  int CountUpdatedTimingReported() { return updated_timings().size(); }
  int CountUpdatedSubFrameTimingReported() {
    return updated_subframe_timings().size();
  }

  const std::vector<GURL>& observed_committed_urls_from_on_start() const {
    return embedder_interface_->observed_committed_urls_from_on_start();
  }

  const std::vector<GURL>& observed_aborted_urls() const {
    return embedder_interface_->observed_aborted_urls();
  }

  const std::vector<mojom::PageLoadFeatures>& observed_features() const {
    return embedder_interface_->observed_features();
  }

  const std::vector<GURL>& completed_filtered_urls() const {
    return embedder_interface_->completed_filtered_urls();
  }

  const std::vector<ExtraRequestCompleteInfo>& loaded_resources() const {
    return embedder_interface_->loaded_resources();
  }

 protected:
  MetricsWebContentsObserver* observer() {
    return MetricsWebContentsObserver::FromWebContents(web_contents());
  }

  base::HistogramTester histogram_tester_;
  TestPageLoadMetricsEmbedderInterface* embedder_interface_;

 private:
  int num_errors_;

  DISALLOW_COPY_AND_ASSIGN(MetricsWebContentsObserverTest);
};

TEST_F(MetricsWebContentsObserverTest, SuccessfulMainFrameNavigation) {
  mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);

  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents());

  ASSERT_TRUE(observed_committed_urls_from_on_start().empty());
  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl));
  ASSERT_EQ(1u, observed_committed_urls_from_on_start().size());
  ASSERT_TRUE(observed_committed_urls_from_on_start().at(0).is_empty());

  ASSERT_EQ(0, CountUpdatedTimingReported());
  SimulateTimingUpdate(timing);
  ASSERT_EQ(1, CountUpdatedTimingReported());
  ASSERT_EQ(0, CountCompleteTimingReported());

  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl2));
  ASSERT_EQ(1, CountCompleteTimingReported());
  ASSERT_EQ(0, CountEmptyCompleteTimingReported());
  ASSERT_EQ(2u, observed_committed_urls_from_on_start().size());
  ASSERT_EQ(kDefaultTestUrl,
            observed_committed_urls_from_on_start().at(1).spec());
  ASSERT_EQ(1, CountUpdatedTimingReported());
  ASSERT_EQ(0, CountUpdatedSubFrameTimingReported());

  CheckNoErrorEvents();
}

TEST_F(MetricsWebContentsObserverTest, MainFrameNavigationInternalAbort) {
  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents());
  web_contents_tester->NavigateAndFail(
      GURL(kDefaultTestUrl), net::ERR_ABORTED,
      base::MakeRefCounted<net::HttpResponseHeaders>("some_headers"));
  ASSERT_EQ(1u, observed_aborted_urls().size());
  ASSERT_EQ(kDefaultTestUrl, observed_aborted_urls().front().spec());
}

TEST_F(MetricsWebContentsObserverTest, SubFrame) {
  mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.response_start = base::TimeDelta::FromMilliseconds(10);
  timing.parse_timing->parse_start = base::TimeDelta::FromMilliseconds(20);
  timing.document_timing->first_layout = base::TimeDelta::FromMilliseconds(30);

  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents());
  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl));
  SimulateTimingUpdate(timing);

  ASSERT_EQ(1, CountUpdatedTimingReported());
  EXPECT_TRUE(timing.Equals(*updated_timings().back()));

  content::RenderFrameHostTester* rfh_tester =
      content::RenderFrameHostTester::For(main_rfh());
  content::RenderFrameHost* subframe = rfh_tester->AppendChild("subframe");

  // Dispatch a timing update for the child frame that includes a first paint.
  mojom::PageLoadTiming subframe_timing;
  page_load_metrics::InitPageLoadTimingForTest(&subframe_timing);
  subframe_timing.navigation_start = base::Time::FromDoubleT(2);
  subframe_timing.response_start = base::TimeDelta::FromMilliseconds(10);
  subframe_timing.parse_timing->parse_start =
      base::TimeDelta::FromMilliseconds(20);
  subframe_timing.document_timing->first_layout =
      base::TimeDelta::FromMilliseconds(30);
  subframe_timing.paint_timing->first_paint =
      base::TimeDelta::FromMilliseconds(40);
  subframe = content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL(kDefaultTestUrl2), subframe);
  content::RenderFrameHostTester* subframe_tester =
      content::RenderFrameHostTester::For(subframe);
  SimulateTimingUpdate(subframe_timing, subframe);
  subframe_tester->SimulateNavigationStop();

  ASSERT_EQ(1, CountUpdatedSubFrameTimingReported());
  EXPECT_TRUE(subframe_timing.Equals(*updated_subframe_timings().back()));

  // The subframe update which included a paint should have also triggered
  // a main frame update, which includes a first paint.
  ASSERT_EQ(2, CountUpdatedTimingReported());
  EXPECT_FALSE(timing.Equals(*updated_timings().back()));
  EXPECT_TRUE(updated_timings().back()->paint_timing->first_paint);

  // Navigate again to see if the timing updated for a subframe message.
  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl2));

  ASSERT_EQ(1, CountCompleteTimingReported());
  ASSERT_EQ(2, CountUpdatedTimingReported());
  ASSERT_EQ(0, CountEmptyCompleteTimingReported());

  ASSERT_EQ(1, CountUpdatedSubFrameTimingReported());
  EXPECT_TRUE(subframe_timing.Equals(*updated_subframe_timings().back()));

  CheckNoErrorEvents();
}

TEST_F(MetricsWebContentsObserverTest, SameDocumentNoTrigger) {
  mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);

  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents());
  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl));
  ASSERT_EQ(0, CountUpdatedTimingReported());
  SimulateTimingUpdate(timing);
  ASSERT_EQ(1, CountUpdatedTimingReported());
  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrlAnchor));
  // Send the same timing update. The original tracker for kDefaultTestUrl
  // should dedup the update, and the tracker for kDefaultTestUrlAnchor should
  // have been destroyed as a result of its being a same page navigation, so
  // CountUpdatedTimingReported() should continue to return 1.
  SimulateTimingUpdate(timing);

  ASSERT_EQ(1, CountUpdatedTimingReported());
  ASSERT_EQ(0, CountCompleteTimingReported());

  // Navigate again to force histogram logging.
  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl2));

  // A same page navigation shouldn't trigger logging UMA for the original.
  ASSERT_EQ(1, CountUpdatedTimingReported());
  ASSERT_EQ(1, CountCompleteTimingReported());
  ASSERT_EQ(0, CountEmptyCompleteTimingReported());
  CheckNoErrorEvents();
}

TEST_F(MetricsWebContentsObserverTest, DontLogNewTabPage) {
  mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);

  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents());
  embedder_interface_->set_is_ntp(true);

  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl));
  SimulateTimingUpdate(timing);
  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl2));
  ASSERT_EQ(0, CountUpdatedTimingReported());
  ASSERT_EQ(0, CountCompleteTimingReported());
  CheckErrorEvent(ERR_IPC_WITH_NO_RELEVANT_LOAD, 1);
  CheckTotalErrorEvents();
}

TEST_F(MetricsWebContentsObserverTest, DontLogIrrelevantNavigation) {
  mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(10);

  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents());

  GURL about_blank_url = GURL("about:blank");
  web_contents_tester->NavigateAndCommit(about_blank_url);
  SimulateTimingUpdate(timing);
  ASSERT_EQ(0, CountUpdatedTimingReported());
  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl));
  ASSERT_EQ(0, CountUpdatedTimingReported());
  ASSERT_EQ(0, CountCompleteTimingReported());

  CheckErrorEvent(ERR_IPC_FROM_BAD_URL_SCHEME, 1);
  CheckErrorEvent(ERR_IPC_WITH_NO_RELEVANT_LOAD, 1);
  CheckTotalErrorEvents();
}

TEST_F(MetricsWebContentsObserverTest, EmptyTimingError) {
  mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);

  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents());

  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl));
  SimulateTimingUpdate(timing);
  ASSERT_EQ(0, CountUpdatedTimingReported());
  NavigateToUntrackedUrl();
  ASSERT_EQ(0, CountUpdatedTimingReported());
  ASSERT_EQ(1, CountCompleteTimingReported());

  CheckErrorEvent(ERR_BAD_TIMING_IPC_INVALID_TIMING, 1);
  CheckErrorEvent(ERR_NO_IPCS_RECEIVED, 1);
  CheckTotalErrorEvents();

  histogram_tester_.ExpectTotalCount(
      page_load_metrics::internal::kPageLoadTimingStatus, 1);
  histogram_tester_.ExpectBucketCount(
      page_load_metrics::internal::kPageLoadTimingStatus,
      page_load_metrics::internal::INVALID_EMPTY_TIMING, 1);
}

TEST_F(MetricsWebContentsObserverTest, NullNavigationStartError) {
  mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.parse_timing->parse_start = base::TimeDelta::FromMilliseconds(1);

  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents());

  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl));
  SimulateTimingUpdate(timing);
  ASSERT_EQ(0, CountUpdatedTimingReported());
  NavigateToUntrackedUrl();
  ASSERT_EQ(0, CountUpdatedTimingReported());
  ASSERT_EQ(1, CountCompleteTimingReported());

  CheckErrorEvent(ERR_BAD_TIMING_IPC_INVALID_TIMING, 1);
  CheckErrorEvent(ERR_NO_IPCS_RECEIVED, 1);
  CheckTotalErrorEvents();

  histogram_tester_.ExpectTotalCount(
      page_load_metrics::internal::kPageLoadTimingStatus, 1);
  histogram_tester_.ExpectBucketCount(
      page_load_metrics::internal::kPageLoadTimingStatus,
      page_load_metrics::internal::INVALID_NULL_NAVIGATION_START, 1);
}

TEST_F(MetricsWebContentsObserverTest, TimingOrderError) {
  mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.parse_timing->parse_stop = base::TimeDelta::FromMilliseconds(1);

  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents());

  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl));
  SimulateTimingUpdate(timing);
  ASSERT_EQ(0, CountUpdatedTimingReported());
  NavigateToUntrackedUrl();
  ASSERT_EQ(0, CountUpdatedTimingReported());
  ASSERT_EQ(1, CountCompleteTimingReported());

  CheckErrorEvent(ERR_BAD_TIMING_IPC_INVALID_TIMING, 1);
  CheckErrorEvent(ERR_NO_IPCS_RECEIVED, 1);
  CheckTotalErrorEvents();

  histogram_tester_.ExpectTotalCount(
      page_load_metrics::internal::kPageLoadTimingStatus, 1);
  histogram_tester_.ExpectBucketCount(
      page_load_metrics::internal::kPageLoadTimingStatus,
      page_load_metrics::internal::INVALID_ORDER_PARSE_START_PARSE_STOP, 1);
}

TEST_F(MetricsWebContentsObserverTest, BadIPC) {
  mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(10);
  mojom::PageLoadTiming timing2;
  page_load_metrics::InitPageLoadTimingForTest(&timing2);
  timing2.navigation_start = base::Time::FromDoubleT(100);

  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents());
  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl));

  SimulateTimingUpdate(timing);
  ASSERT_EQ(1, CountUpdatedTimingReported());
  SimulateTimingUpdate(timing2);
  ASSERT_EQ(1, CountUpdatedTimingReported());

  CheckErrorEvent(ERR_BAD_TIMING_IPC_INVALID_TIMING_DESCENDENT, 1);
  CheckTotalErrorEvents();
}

TEST_F(MetricsWebContentsObserverTest, ObservePartialNavigation) {
  // Reset the state of the tests, and attach the MetricsWebContentsObserver in
  // the middle of a navigation. This tests that the class is robust to only
  // observing some of a navigation.
  DeleteContents();
  SetContents(CreateTestWebContents());

  mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(10);

  // Start the navigation, then start observing the web contents. This used to
  // crash us. Make sure we bail out and don't log histograms.
  std::unique_ptr<NavigationSimulator> navigation =
      NavigationSimulator::CreateBrowserInitiated(GURL(kDefaultTestUrl),
                                                  web_contents());
  navigation->Start();
  AttachObserver();
  navigation->Commit();

  SimulateTimingUpdate(timing);

  // Navigate again to force histogram logging.
  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents());
  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl2));
  ASSERT_EQ(0, CountCompleteTimingReported());
  ASSERT_EQ(0, CountUpdatedTimingReported());
  CheckErrorEvent(ERR_IPC_WITH_NO_RELEVANT_LOAD, 1);
  CheckTotalErrorEvents();
}

TEST_F(MetricsWebContentsObserverTest, DontLogAbortChains) {
  NavigateAndCommit(GURL(kDefaultTestUrl));
  NavigateAndCommit(GURL(kDefaultTestUrl2));
  NavigateAndCommit(GURL(kDefaultTestUrl));
  histogram_tester_.ExpectTotalCount(internal::kAbortChainSizeNewNavigation, 0);
  CheckErrorEvent(ERR_NO_IPCS_RECEIVED, 2);
  CheckTotalErrorEvents();
}

TEST_F(MetricsWebContentsObserverTest, LogAbortChains) {
  // Start and abort three loads before one finally commits.
  NavigationSimulator::NavigateAndFailFromBrowser(
      web_contents(), GURL(kDefaultTestUrl), net::ERR_ABORTED);

  NavigationSimulator::NavigateAndFailFromBrowser(
      web_contents(), GURL(kDefaultTestUrl2), net::ERR_ABORTED);

  NavigationSimulator::NavigateAndFailFromBrowser(
      web_contents(), GURL(kDefaultTestUrl), net::ERR_ABORTED);

  NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                    GURL(kDefaultTestUrl2));

  histogram_tester_.ExpectTotalCount(internal::kAbortChainSizeNewNavigation, 1);
  histogram_tester_.ExpectBucketCount(internal::kAbortChainSizeNewNavigation, 3,
                                      1);
  CheckNoErrorEvents();
}

TEST_F(MetricsWebContentsObserverTest, LogAbortChainsSameURL) {
  // Start and abort three loads before one finally commits.
  NavigationSimulator::NavigateAndFailFromBrowser(
      web_contents(), GURL(kDefaultTestUrl), net::ERR_ABORTED);

  NavigationSimulator::NavigateAndFailFromBrowser(
      web_contents(), GURL(kDefaultTestUrl), net::ERR_ABORTED);

  NavigationSimulator::NavigateAndFailFromBrowser(
      web_contents(), GURL(kDefaultTestUrl), net::ERR_ABORTED);

  NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                    GURL(kDefaultTestUrl));
  histogram_tester_.ExpectTotalCount(internal::kAbortChainSizeNewNavigation, 1);
  histogram_tester_.ExpectBucketCount(internal::kAbortChainSizeNewNavigation, 3,
                                      1);
  histogram_tester_.ExpectTotalCount(internal::kAbortChainSizeSameURL, 1);
  histogram_tester_.ExpectBucketCount(internal::kAbortChainSizeSameURL, 3, 1);
}

TEST_F(MetricsWebContentsObserverTest, LogAbortChainsNoCommit) {
  // Start and abort three loads before one finally commits.
  NavigationSimulator::NavigateAndFailFromBrowser(
      web_contents(), GURL(kDefaultTestUrl), net::ERR_ABORTED);

  NavigationSimulator::NavigateAndFailFromBrowser(
      web_contents(), GURL(kDefaultTestUrl2), net::ERR_ABORTED);

  NavigationSimulator::NavigateAndFailFromBrowser(
      web_contents(), GURL(kDefaultTestUrl), net::ERR_ABORTED);

  web_contents()->Stop();

  histogram_tester_.ExpectTotalCount(internal::kAbortChainSizeNoCommit, 1);
  histogram_tester_.ExpectBucketCount(internal::kAbortChainSizeNoCommit, 3, 1);
}

TEST_F(MetricsWebContentsObserverTest, FlushMetricsOnAppEnterBackground) {
  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents());
  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl));

  histogram_tester_.ExpectTotalCount(
      internal::kPageLoadCompletedAfterAppBackground, 0);

  observer()->FlushMetricsOnAppEnterBackground();

  histogram_tester_.ExpectTotalCount(
      internal::kPageLoadCompletedAfterAppBackground, 1);
  histogram_tester_.ExpectBucketCount(
      internal::kPageLoadCompletedAfterAppBackground, false, 1);
  histogram_tester_.ExpectBucketCount(
      internal::kPageLoadCompletedAfterAppBackground, true, 0);

  // Navigate again, which forces completion callbacks on the previous
  // navigation to be invoked.
  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl2));

  // Verify that, even though the page load completed, no complete timings were
  // reported, because the TestPageLoadMetricsObserver's
  // FlushMetricsOnAppEnterBackground implementation returned STOP_OBSERVING,
  // thus preventing OnComplete from being invoked.
  ASSERT_EQ(0, CountCompleteTimingReported());

  DeleteContents();

  histogram_tester_.ExpectTotalCount(
      internal::kPageLoadCompletedAfterAppBackground, 2);
  histogram_tester_.ExpectBucketCount(
      internal::kPageLoadCompletedAfterAppBackground, false, 1);
  histogram_tester_.ExpectBucketCount(
      internal::kPageLoadCompletedAfterAppBackground, true, 1);
}

TEST_F(MetricsWebContentsObserverTest, StopObservingOnCommit) {
  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents());
  ASSERT_TRUE(completed_filtered_urls().empty());

  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl));
  ASSERT_TRUE(completed_filtered_urls().empty());

  // kFilteredCommitUrl should stop observing in OnCommit, and thus should not
  // reach OnComplete().
  web_contents_tester->NavigateAndCommit(GURL(kFilteredCommitUrl));
  ASSERT_EQ(std::vector<GURL>({GURL(kDefaultTestUrl)}),
            completed_filtered_urls());

  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl2));
  ASSERT_EQ(std::vector<GURL>({GURL(kDefaultTestUrl)}),
            completed_filtered_urls());

  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl));
  ASSERT_EQ(std::vector<GURL>({GURL(kDefaultTestUrl), GURL(kDefaultTestUrl2)}),
            completed_filtered_urls());
}

TEST_F(MetricsWebContentsObserverTest, StopObservingOnStart) {
  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents());
  ASSERT_TRUE(completed_filtered_urls().empty());

  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl));
  ASSERT_TRUE(completed_filtered_urls().empty());

  // kFilteredCommitUrl should stop observing in OnStart, and thus should not
  // reach OnComplete().
  web_contents_tester->NavigateAndCommit(GURL(kFilteredStartUrl));
  ASSERT_EQ(std::vector<GURL>({GURL(kDefaultTestUrl)}),
            completed_filtered_urls());

  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl2));
  ASSERT_EQ(std::vector<GURL>({GURL(kDefaultTestUrl)}),
            completed_filtered_urls());

  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl));
  ASSERT_EQ(std::vector<GURL>({GURL(kDefaultTestUrl), GURL(kDefaultTestUrl2)}),
            completed_filtered_urls());
}

// We buffer cross frame timings in order to provide a consistent view of
// timing data to observers. See crbug.com/722860 for more.
TEST_F(MetricsWebContentsObserverTest, OutOfOrderCrossFrameTiming) {
  mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.response_start = base::TimeDelta::FromMilliseconds(10);

  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents());
  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl));
  SimulateTimingUpdate(timing);

  ASSERT_EQ(1, CountUpdatedTimingReported());
  EXPECT_TRUE(timing.Equals(*updated_timings().back()));

  content::RenderFrameHostTester* rfh_tester =
      content::RenderFrameHostTester::For(main_rfh());
  content::RenderFrameHost* subframe = rfh_tester->AppendChild("subframe");

  // Dispatch a timing update for the child frame that includes a first paint.
  mojom::PageLoadTiming subframe_timing;
  PopulatePageLoadTiming(&subframe_timing);
  subframe_timing.paint_timing->first_paint =
      base::TimeDelta::FromMilliseconds(40);
  subframe = content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL(kDefaultTestUrl2), subframe);
  SimulateTimingUpdate(subframe_timing, subframe);

  // Though a first paint was dispatched in the child, it should not yet be
  // reflected as an updated timing in the main frame, since the main frame
  // hasn't received updates for required earlier events such as parse_start and
  // first_layout.
  ASSERT_EQ(1, CountUpdatedSubFrameTimingReported());
  EXPECT_TRUE(subframe_timing.Equals(*updated_subframe_timings().back()));
  ASSERT_EQ(1, CountUpdatedTimingReported());
  EXPECT_TRUE(timing.Equals(*updated_timings().back()));

  // Dispatch the parse_start event in the parent. We should still not observe
  // a first paint main frame update, since we don't yet have a first_layout.
  timing.parse_timing->parse_start = base::TimeDelta::FromMilliseconds(20);
  SimulateTimingUpdate(timing);
  ASSERT_EQ(1, CountUpdatedTimingReported());
  EXPECT_FALSE(timing.Equals(*updated_timings().back()));
  EXPECT_FALSE(updated_timings().back()->parse_timing->parse_start);
  EXPECT_FALSE(updated_timings().back()->paint_timing->first_paint);

  // Dispatch a first_layout in the parent. We should now unbuffer the first
  // paint main frame update and receive a main frame update with a first paint
  // value.
  timing.document_timing->first_layout = base::TimeDelta::FromMilliseconds(30);
  SimulateTimingUpdate(timing);
  ASSERT_EQ(2, CountUpdatedTimingReported());
  EXPECT_FALSE(timing.Equals(*updated_timings().back()));
  EXPECT_TRUE(
      updated_timings().back()->parse_timing->Equals(*timing.parse_timing));
  EXPECT_TRUE(updated_timings().back()->document_timing->Equals(
      *timing.document_timing));
  EXPECT_FALSE(
      updated_timings().back()->paint_timing->Equals(*timing.paint_timing));
  EXPECT_TRUE(updated_timings().back()->paint_timing->first_paint);

  // Navigate again to see if the timing updated for a subframe message.
  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl2));

  ASSERT_EQ(1, CountCompleteTimingReported());
  ASSERT_EQ(2, CountUpdatedTimingReported());
  ASSERT_EQ(0, CountEmptyCompleteTimingReported());

  ASSERT_EQ(1, CountUpdatedSubFrameTimingReported());
  EXPECT_TRUE(subframe_timing.Equals(*updated_subframe_timings().back()));

  CheckNoErrorEvents();
}

// We buffer cross-frame paint updates to account for paint timings from
// different frames arriving out of order.
TEST_F(MetricsWebContentsObserverTest, OutOfOrderCrossFrameTiming2) {
  // Dispatch a timing update for the main frame that includes a first
  // paint. This should be buffered, with the dispatch timer running.
  mojom::PageLoadTiming timing;
  PopulatePageLoadTiming(&timing);
  // Ensure this is much bigger than the subframe first paint below. We
  // currently can't inject the navigation start offset, so we must ensure that
  // subframe first paint + navigation start offset < main frame first paint.
  timing.paint_timing->first_paint = base::TimeDelta::FromMilliseconds(100000);
  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents());
  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl));
  SimulateTimingUpdateWithoutFiringDispatchTimer(timing, main_rfh());

  EXPECT_TRUE(GetMostRecentTimer()->IsRunning());
  ASSERT_EQ(0, CountUpdatedTimingReported());

  content::RenderFrameHostTester* rfh_tester =
      content::RenderFrameHostTester::For(main_rfh());

  // Dispatch a timing update for a child frame that includes a first paint.
  mojom::PageLoadTiming subframe_timing;
  PopulatePageLoadTiming(&subframe_timing);
  subframe_timing.paint_timing->first_paint =
      base::TimeDelta::FromMilliseconds(500);
  content::RenderFrameHost* subframe = rfh_tester->AppendChild("subframe");
  subframe = content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL(kDefaultTestUrl2), subframe);
  content::RenderFrameHostTester* subframe_tester =
      content::RenderFrameHostTester::For(subframe);
  SimulateTimingUpdateWithoutFiringDispatchTimer(subframe_timing, subframe);
  subframe_tester->SimulateNavigationStop();

  histogram_tester_.ExpectTotalCount(
      page_load_metrics::internal::kHistogramOutOfOrderTiming, 1);

  EXPECT_TRUE(GetMostRecentTimer()->IsRunning());
  ASSERT_EQ(0, CountUpdatedTimingReported());

  // At this point, the timing update is buffered, waiting for the timer to
  // fire.
  GetMostRecentTimer()->Fire();

  // Firing the timer should produce a timing update. The update should be a
  // merged view of the main frame timing, with a first paint timestamp from the
  // subframe.
  ASSERT_EQ(1, CountUpdatedTimingReported());
  EXPECT_FALSE(timing.Equals(*updated_timings().back()));
  EXPECT_TRUE(
      updated_timings().back()->parse_timing->Equals(*timing.parse_timing));
  EXPECT_TRUE(updated_timings().back()->document_timing->Equals(
      *timing.document_timing));
  EXPECT_FALSE(
      updated_timings().back()->paint_timing->Equals(*timing.paint_timing));
  EXPECT_TRUE(updated_timings().back()->paint_timing->first_paint);

  // The first paint value should be the min of all received first paints, which
  // in this case is the first paint from the subframe. Since it is offset by
  // the subframe's navigation start, the received value should be >= the first
  // paint value specified in the subframe.
  EXPECT_GE(updated_timings().back()->paint_timing->first_paint,
            subframe_timing.paint_timing->first_paint);
  EXPECT_LT(updated_timings().back()->paint_timing->first_paint,
            timing.paint_timing->first_paint);

  base::TimeDelta initial_first_paint =
      updated_timings().back()->paint_timing->first_paint.value();

  // Dispatch a timing update for an additional child frame, with an earlier
  // first paint time. This should cause an immediate update, without a timer
  // delay.
  subframe_timing.paint_timing->first_paint =
      base::TimeDelta::FromMilliseconds(50);
  content::RenderFrameHost* subframe2 = rfh_tester->AppendChild("subframe");
  subframe2 = content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL(kDefaultTestUrl2), subframe2);
  content::RenderFrameHostTester* subframe2_tester =
      content::RenderFrameHostTester::For(subframe2);
  SimulateTimingUpdateWithoutFiringDispatchTimer(subframe_timing, subframe2);
  subframe2_tester->SimulateNavigationStop();

  base::TimeDelta updated_first_paint =
      updated_timings().back()->paint_timing->first_paint.value();

  EXPECT_FALSE(GetMostRecentTimer()->IsRunning());
  ASSERT_EQ(2, CountUpdatedTimingReported());
  EXPECT_LT(updated_first_paint, initial_first_paint);

  histogram_tester_.ExpectTotalCount(
      page_load_metrics::internal::kHistogramOutOfOrderTimingBuffered, 1);
  histogram_tester_.ExpectBucketCount(
      page_load_metrics::internal::kHistogramOutOfOrderTimingBuffered,
      (initial_first_paint - updated_first_paint).InMilliseconds(), 1);

  CheckNoErrorEvents();
}

TEST_F(MetricsWebContentsObserverTest,
       FirstInputDelayMissingFirstInputTimestamp) {
  mojom::PageLoadTiming timing;
  PopulatePageLoadTiming(&timing);
  timing.interactive_timing->first_input_delay =
      base::TimeDelta::FromMilliseconds(10);

  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents());

  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl));
  SimulateTimingUpdate(timing);
  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl2));

  const mojom::InteractiveTiming& interactive_timing =
      *complete_timings().back()->interactive_timing;

  // Won't have been set, as we're missing the first_input_timestamp.
  EXPECT_FALSE(interactive_timing.first_input_delay.has_value());

  histogram_tester_.ExpectTotalCount(
      page_load_metrics::internal::kPageLoadTimingStatus, 1);
  histogram_tester_.ExpectBucketCount(
      page_load_metrics::internal::kPageLoadTimingStatus,
      page_load_metrics::internal::INVALID_NULL_FIRST_INPUT_TIMESTAMP, 1);

  CheckErrorEvent(ERR_BAD_TIMING_IPC_INVALID_TIMING, 1);
  CheckErrorEvent(ERR_NO_IPCS_RECEIVED, 1);
  CheckTotalErrorEvents();
}

TEST_F(MetricsWebContentsObserverTest,
       FirstInputTimestampMissingFirstInputDelay) {
  mojom::PageLoadTiming timing;
  PopulatePageLoadTiming(&timing);
  timing.interactive_timing->first_input_timestamp =
      base::TimeDelta::FromMilliseconds(10);

  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents());

  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl));
  SimulateTimingUpdate(timing);
  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl2));

  const mojom::InteractiveTiming& interactive_timing =
      *complete_timings().back()->interactive_timing;

  // Won't have been set, as we're missing the first_input_delay.
  EXPECT_FALSE(interactive_timing.first_input_timestamp.has_value());

  histogram_tester_.ExpectTotalCount(
      page_load_metrics::internal::kPageLoadTimingStatus, 1);
  histogram_tester_.ExpectBucketCount(
      page_load_metrics::internal::kPageLoadTimingStatus,
      page_load_metrics::internal::INVALID_NULL_FIRST_INPUT_DELAY, 1);

  CheckErrorEvent(ERR_BAD_TIMING_IPC_INVALID_TIMING, 1);
  CheckErrorEvent(ERR_NO_IPCS_RECEIVED, 1);
  CheckTotalErrorEvents();
}

TEST_F(MetricsWebContentsObserverTest,
       LongestInputDelayMissingLongestInputTimestamp) {
  mojom::PageLoadTiming timing;
  PopulatePageLoadTiming(&timing);
  timing.interactive_timing->longest_input_delay =
      base::TimeDelta::FromMilliseconds(10);

  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents());

  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl));
  SimulateTimingUpdate(timing);
  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl2));

  const mojom::InteractiveTiming& interactive_timing =
      *complete_timings().back()->interactive_timing;

  // Won't have been set, as we're missing the longest_input_timestamp.
  EXPECT_FALSE(interactive_timing.longest_input_delay.has_value());

  histogram_tester_.ExpectTotalCount(
      page_load_metrics::internal::kPageLoadTimingStatus, 1);
  histogram_tester_.ExpectBucketCount(
      page_load_metrics::internal::kPageLoadTimingStatus,
      page_load_metrics::internal::INVALID_NULL_LONGEST_INPUT_TIMESTAMP, 1);

  CheckErrorEvent(ERR_BAD_TIMING_IPC_INVALID_TIMING, 1);
  CheckErrorEvent(ERR_NO_IPCS_RECEIVED, 1);
  CheckTotalErrorEvents();
}

TEST_F(MetricsWebContentsObserverTest,
       LongestInputTimestampMissingLongestInputDelay) {
  mojom::PageLoadTiming timing;
  PopulatePageLoadTiming(&timing);
  timing.interactive_timing->longest_input_timestamp =
      base::TimeDelta::FromMilliseconds(10);

  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents());

  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl));
  SimulateTimingUpdate(timing);
  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl2));

  const mojom::InteractiveTiming& interactive_timing =
      *complete_timings().back()->interactive_timing;

  // Won't have been set, as we're missing the longest_input_delay.
  EXPECT_FALSE(interactive_timing.longest_input_timestamp.has_value());

  histogram_tester_.ExpectTotalCount(
      page_load_metrics::internal::kPageLoadTimingStatus, 1);
  histogram_tester_.ExpectBucketCount(
      page_load_metrics::internal::kPageLoadTimingStatus,
      page_load_metrics::internal::INVALID_NULL_LONGEST_INPUT_DELAY, 1);

  CheckErrorEvent(ERR_BAD_TIMING_IPC_INVALID_TIMING, 1);
  CheckErrorEvent(ERR_NO_IPCS_RECEIVED, 1);
  CheckTotalErrorEvents();
}

TEST_F(MetricsWebContentsObserverTest,
       LongestInputDelaySmallerThanFirstInputDelay) {
  mojom::PageLoadTiming timing;
  PopulatePageLoadTiming(&timing);
  timing.interactive_timing->first_input_delay =
      base::TimeDelta::FromMilliseconds(50);
  timing.interactive_timing->first_input_timestamp =
      base::TimeDelta::FromMilliseconds(1000);

  timing.interactive_timing->longest_input_delay =
      base::TimeDelta::FromMilliseconds(10);
  timing.interactive_timing->longest_input_timestamp =
      base::TimeDelta::FromMilliseconds(2000);

  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents());

  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl));
  SimulateTimingUpdate(timing);
  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl2));

  const mojom::InteractiveTiming& interactive_timing =
      *complete_timings().back()->interactive_timing;

  // Won't have been set, as it's invalid.
  EXPECT_FALSE(interactive_timing.longest_input_delay.has_value());

  histogram_tester_.ExpectTotalCount(
      page_load_metrics::internal::kPageLoadTimingStatus, 1);
  histogram_tester_.ExpectBucketCount(
      page_load_metrics::internal::kPageLoadTimingStatus,
      page_load_metrics::internal::
          INVALID_LONGEST_INPUT_DELAY_LESS_THAN_FIRST_INPUT_DELAY,
      1);

  CheckErrorEvent(ERR_BAD_TIMING_IPC_INVALID_TIMING, 1);
  CheckErrorEvent(ERR_NO_IPCS_RECEIVED, 1);
  CheckTotalErrorEvents();
}

TEST_F(MetricsWebContentsObserverTest,
       LongestInputTimestampEarlierThanFirstInputTimestamp) {
  mojom::PageLoadTiming timing;
  PopulatePageLoadTiming(&timing);
  timing.interactive_timing->first_input_delay =
      base::TimeDelta::FromMilliseconds(50);
  timing.interactive_timing->first_input_timestamp =
      base::TimeDelta::FromMilliseconds(1000);

  timing.interactive_timing->longest_input_delay =
      base::TimeDelta::FromMilliseconds(60);
  timing.interactive_timing->longest_input_timestamp =
      base::TimeDelta::FromMilliseconds(500);

  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents());

  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl));
  SimulateTimingUpdate(timing);
  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl2));

  const mojom::InteractiveTiming& interactive_timing =
      *complete_timings().back()->interactive_timing;

  // Won't have been set, as it's invalid.
  EXPECT_FALSE(interactive_timing.longest_input_delay.has_value());

  histogram_tester_.ExpectTotalCount(
      page_load_metrics::internal::kPageLoadTimingStatus, 1);
  histogram_tester_.ExpectBucketCount(
      page_load_metrics::internal::kPageLoadTimingStatus,
      page_load_metrics::internal::
          INVALID_LONGEST_INPUT_TIMESTAMP_LESS_THAN_FIRST_INPUT_TIMESTAMP,
      1);

  CheckErrorEvent(ERR_BAD_TIMING_IPC_INVALID_TIMING, 1);
  CheckErrorEvent(ERR_NO_IPCS_RECEIVED, 1);
  CheckTotalErrorEvents();
}

// Main frame delivers an input notification. Subsequently, a subframe delivers
// an input notification, where the input occurred first. Verify that
// FirstInputDelay and FirstInputTimestamp come from the subframe.
TEST_F(MetricsWebContentsObserverTest,
       FirstInputDelayAndTimingSubframeFirstDeliveredSecond) {
  mojom::PageLoadTiming timing;
  PopulatePageLoadTiming(&timing);
  timing.interactive_timing->first_input_delay =
      base::TimeDelta::FromMilliseconds(10);
  // Set this far in the future. We currently can't control the navigation start
  // offset, so we ensure that the subframe timestamp + the unknown offset is
  // less than the main frame timestamp.
  timing.interactive_timing->first_input_timestamp =
      base::TimeDelta::FromMinutes(100);

  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents());
  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl));
  SimulateTimingUpdate(timing);

  content::RenderFrameHostTester* rfh_tester =
      content::RenderFrameHostTester::For(main_rfh());
  content::RenderFrameHost* subframe = rfh_tester->AppendChild("subframe");

  // Dispatch a timing update for the child frame that includes a first input
  // earlier than the one for the main frame.
  mojom::PageLoadTiming subframe_timing;
  PopulatePageLoadTiming(&subframe_timing);
  subframe_timing.interactive_timing->first_input_delay =
      base::TimeDelta::FromMilliseconds(15);
  subframe_timing.interactive_timing->first_input_timestamp =
      base::TimeDelta::FromMilliseconds(90);

  subframe = content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL(kDefaultTestUrl2), subframe);
  SimulateTimingUpdate(subframe_timing, subframe);

  // Navigate again to confirm the timing updated for a subframe message.
  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl2));

  const mojom::InteractiveTiming& interactive_timing =
      *complete_timings().back()->interactive_timing;

  EXPECT_EQ(base::TimeDelta::FromMilliseconds(15),
            interactive_timing.first_input_delay);
  // Ensure the timestamp is from the subframe. The main frame timestamp was 100
  // minutes.
  EXPECT_LT(interactive_timing.first_input_timestamp,
            base::TimeDelta::FromMinutes(10));

  CheckNoErrorEvents();
}

// A subframe delivers an input notification. Subsequently, the mainframe
// delivers an input notification, where the input occurred first. Verify that
// FirstInputDelay and FirstInputTimestamp come from the main frame.
TEST_F(MetricsWebContentsObserverTest,
       FirstInputDelayAndTimingMainframeFirstDeliveredSecond) {
  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents());

  // We need to navigate before we can navigate the subframe.
  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl));

  content::RenderFrameHostTester* rfh_tester =
      content::RenderFrameHostTester::For(main_rfh());
  content::RenderFrameHost* subframe = rfh_tester->AppendChild("subframe");

  mojom::PageLoadTiming subframe_timing;
  PopulatePageLoadTiming(&subframe_timing);
  subframe_timing.interactive_timing->first_input_delay =
      base::TimeDelta::FromMilliseconds(10);
  subframe_timing.interactive_timing->first_input_timestamp =
      base::TimeDelta::FromMinutes(100);

  subframe = content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL(kDefaultTestUrl2), subframe);
  SimulateTimingUpdate(subframe_timing, subframe);

  mojom::PageLoadTiming timing;
  PopulatePageLoadTiming(&timing);
  // Dispatch a timing update for the main frame that includes a first input
  // earlier than the one for the subframe.

  timing.interactive_timing->first_input_delay =
      base::TimeDelta::FromMilliseconds(15);
  // Set this far in the future. We currently can't control the navigation start
  // offset, so we ensure that the main frame timestamp + the unknown offset is
  // less than the subframe timestamp.
  timing.interactive_timing->first_input_timestamp =
      base::TimeDelta::FromMilliseconds(90);

  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl));
  SimulateTimingUpdate(timing);

  // Navigate again to confirm the timing updated for the mainframe message.
  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl2));

  const mojom::InteractiveTiming& interactive_timing =
      *complete_timings().back()->interactive_timing;

  EXPECT_EQ(base::TimeDelta::FromMilliseconds(15),
            interactive_timing.first_input_delay);
  // Ensure the timestamp is from the main frame. The subframe timestamp was 100
  // minutes.
  EXPECT_LT(interactive_timing.first_input_timestamp,
            base::TimeDelta::FromMinutes(10));

  CheckNoErrorEvents();
}

TEST_F(MetricsWebContentsObserverTest, LongestInputInMainFrame) {
  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents());

  // We need to navigate before we can navigate the subframe.
  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl));

  content::RenderFrameHostTester* rfh_tester =
      content::RenderFrameHostTester::For(main_rfh());
  content::RenderFrameHost* subframe = rfh_tester->AppendChild("subframe");

  mojom::PageLoadTiming subframe_timing;
  PopulatePageLoadTiming(&subframe_timing);
  subframe_timing.interactive_timing->longest_input_delay =
      base::TimeDelta::FromMilliseconds(70);
  subframe_timing.interactive_timing->longest_input_timestamp =
      base::TimeDelta::FromMilliseconds(1000);

  subframe = content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL(kDefaultTestUrl2), subframe);
  SimulateTimingUpdate(subframe_timing, subframe);

  mojom::PageLoadTiming main_frame_timing;
  PopulatePageLoadTiming(&main_frame_timing);

  // Dispatch a timing update for the main frame that includes a longest input
  // delay longer than the one for the subframe.
  main_frame_timing.interactive_timing->longest_input_delay =
      base::TimeDelta::FromMilliseconds(100);
  main_frame_timing.interactive_timing->longest_input_timestamp =
      base::TimeDelta::FromMilliseconds(2000);
  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl));
  SimulateTimingUpdate(main_frame_timing);

  // Second subframe.
  content::RenderFrameHost* subframe2 = rfh_tester->AppendChild("subframe2");
  mojom::PageLoadTiming subframe2_timing;
  PopulatePageLoadTiming(&subframe2_timing);
  subframe2_timing.interactive_timing->longest_input_delay =
      base::TimeDelta::FromMilliseconds(80);
  subframe2_timing.interactive_timing->longest_input_timestamp =
      base::TimeDelta::FromMilliseconds(3000);
  subframe2 = content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL(kDefaultTestUrl2), subframe2);
  SimulateTimingUpdate(subframe2_timing, subframe2);

  // Navigate again to confirm all timings are updated.
  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl2));

  const mojom::InteractiveTiming& interactive_timing =
      *complete_timings().back()->interactive_timing;

  EXPECT_EQ(base::TimeDelta::FromMilliseconds(100),
            interactive_timing.longest_input_delay);
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(2000),
            interactive_timing.longest_input_timestamp);

  CheckNoErrorEvents();
}

// -----------------------------------------------------------------------------
//     |                          |                          |
//     1s                         2s                         3s
//     Subframe1                  Main Frame                 Subframe2
//     LID (15ms)                 LID (100ms)                LID (200ms)
//
// Delivery order: Main Frame -> Subframe1 -> Subframe2.
TEST_F(MetricsWebContentsObserverTest, LongestInputInSubframe) {
  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents());

  mojom::PageLoadTiming main_frame_timing;
  PopulatePageLoadTiming(&main_frame_timing);
  main_frame_timing.interactive_timing->longest_input_delay =
      base::TimeDelta::FromMilliseconds(100);
  main_frame_timing.interactive_timing->longest_input_timestamp =
      base::TimeDelta::FromMilliseconds(2000);
  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl));
  SimulateTimingUpdate(main_frame_timing);

  content::RenderFrameHostTester* rfh_tester =
      content::RenderFrameHostTester::For(main_rfh());

  // First subframe.
  content::RenderFrameHost* subframe1 = rfh_tester->AppendChild("subframe1");
  mojom::PageLoadTiming subframe_timing;
  PopulatePageLoadTiming(&subframe_timing);
  subframe_timing.interactive_timing->longest_input_delay =
      base::TimeDelta::FromMilliseconds(15);
  subframe_timing.interactive_timing->longest_input_timestamp =
      base::TimeDelta::FromMilliseconds(1000);
  subframe1 = content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL(kDefaultTestUrl2), subframe1);
  SimulateTimingUpdate(subframe_timing, subframe1);

  // Second subframe.
  content::RenderFrameHost* subframe2 = rfh_tester->AppendChild("subframe2");
  mojom::PageLoadTiming subframe2_timing;
  PopulatePageLoadTiming(&subframe2_timing);
  subframe2_timing.interactive_timing->longest_input_delay =
      base::TimeDelta::FromMilliseconds(200);
  subframe2_timing.interactive_timing->longest_input_timestamp =
      base::TimeDelta::FromMilliseconds(3000);
  // TODO: Make this url3.
  subframe2 = content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL(kDefaultTestUrl2), subframe2);
  SimulateTimingUpdate(subframe2_timing, subframe2);

  // Navigate again to confirm all timings are updated.
  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl2));

  const mojom::InteractiveTiming& interactive_timing =
      *complete_timings().back()->interactive_timing;

  EXPECT_EQ(base::TimeDelta::FromMilliseconds(200),
            interactive_timing.longest_input_delay);

  // Actual LID timestamp includes the delta between navigation start in
  // subframe2 and navigation time in the main frame. That delta varies with
  // different runs, so we only check here that the timestamp is greater than
  // 3s.
  EXPECT_GT(interactive_timing.longest_input_timestamp.value(),
            base::TimeDelta::FromMilliseconds(3000));

  CheckNoErrorEvents();
}

TEST_F(MetricsWebContentsObserverTest, DispatchDelayedMetricsOnPageClose) {
  mojom::PageLoadTiming timing;
  PopulatePageLoadTiming(&timing);
  timing.paint_timing->first_paint = base::TimeDelta::FromMilliseconds(1000);
  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents());
  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl));
  SimulateTimingUpdateWithoutFiringDispatchTimer(timing, main_rfh());

  EXPECT_TRUE(GetMostRecentTimer()->IsRunning());
  ASSERT_EQ(0, CountUpdatedTimingReported());
  ASSERT_EQ(0, CountCompleteTimingReported());

  // Navigate to a new page. This should force dispatch of the buffered timing
  // update.
  NavigateToUntrackedUrl();

  ASSERT_EQ(1, CountUpdatedTimingReported());
  ASSERT_EQ(1, CountCompleteTimingReported());
  EXPECT_TRUE(timing.Equals(*updated_timings().back()));
  EXPECT_TRUE(timing.Equals(*complete_timings().back()));

  CheckNoErrorEvents();
}

TEST_F(MetricsWebContentsObserverTest, OnLoadedResource_MainFrame) {
  GURL main_resource_url(kDefaultTestUrl);
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(main_resource_url);

  auto navigation_simulator =
      content::NavigationSimulator::CreateRendererInitiated(
          main_resource_url, web_contents()->GetMainFrame());
  navigation_simulator->Start();
  int frame_tree_node_id =
      navigation_simulator->GetNavigationHandle()->GetFrameTreeNodeId();
  navigation_simulator->Commit();

  const auto request_id = navigation_simulator->GetGlobalRequestID();

  observer()->OnRequestComplete(
      main_resource_url, net::HostPortPair(), frame_tree_node_id, request_id,
      web_contents()->GetMainFrame(),
      content::ResourceType::RESOURCE_TYPE_MAIN_FRAME, false, nullptr, 0, 0,
      base::TimeTicks::Now(), net::OK, nullptr);
  EXPECT_EQ(1u, loaded_resources().size());
  EXPECT_EQ(main_resource_url, loaded_resources().back().url);

  NavigateToUntrackedUrl();

  // Deliver a second main frame resource. This one should be ignored, since the
  // specified |request_id| is no longer associated with any tracked page loads.
  observer()->OnRequestComplete(
      main_resource_url, net::HostPortPair(), frame_tree_node_id, request_id,
      web_contents()->GetMainFrame(),
      content::ResourceType::RESOURCE_TYPE_MAIN_FRAME, false, nullptr, 0, 0,
      base::TimeTicks::Now(), net::OK, nullptr);
  EXPECT_EQ(1u, loaded_resources().size());
  EXPECT_EQ(main_resource_url, loaded_resources().back().url);
}

TEST_F(MetricsWebContentsObserverTest, OnLoadedResource_Subresource) {
  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents());
  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl));
  GURL loaded_resource_url("http://www.other.com/");
  observer()->OnRequestComplete(
      loaded_resource_url, net::HostPortPair(),
      web_contents()->GetMainFrame()->GetFrameTreeNodeId(),
      content::GlobalRequestID(), web_contents()->GetMainFrame(),
      content::RESOURCE_TYPE_SCRIPT, false, nullptr, 0, 0,
      base::TimeTicks::Now(), net::OK, nullptr);

  EXPECT_EQ(1u, loaded_resources().size());
  EXPECT_EQ(loaded_resource_url, loaded_resources().back().url);
}

TEST_F(MetricsWebContentsObserverTest,
       OnLoadedResource_ResourceFromOtherRFHIgnored) {
  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents());
  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl));

  // This is a bit of a hack. We want to simulate giving the
  // MetricsWebContentsObserver a RenderFrameHost from a previously committed
  // page, to verify that resources for RFHs that don't match the currently
  // committed RFH are ignored. There isn't a way to hold on to an old RFH (it
  // gets cleaned up soon after being navigated away from) so instead we use an
  // RFH from another WebContents, as a way to simulate the desired behavior.
  std::unique_ptr<content::WebContents> other_web_contents(
      content::WebContentsTester::CreateTestWebContents(browser_context(),
                                                        nullptr));
  observer()->OnRequestComplete(
      GURL("http://www.other.com/"), net::HostPortPair(),
      other_web_contents->GetMainFrame()->GetFrameTreeNodeId(),
      content::GlobalRequestID(), other_web_contents->GetMainFrame(),
      content::RESOURCE_TYPE_SCRIPT, false, nullptr, 0, 0,
      base::TimeTicks::Now(), net::OK, nullptr);

  EXPECT_TRUE(loaded_resources().empty());
}

TEST_F(MetricsWebContentsObserverTest,
       OnLoadedResource_IgnoreNonHttpOrHttpsScheme) {
  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents());
  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl));
  GURL loaded_resource_url("data:text/html,Hello world");
  observer()->OnRequestComplete(
      loaded_resource_url, net::HostPortPair(),
      web_contents()->GetMainFrame()->GetFrameTreeNodeId(),
      content::GlobalRequestID(), web_contents()->GetMainFrame(),
      content::RESOURCE_TYPE_SCRIPT, false, nullptr, 0, 0,
      base::TimeTicks::Now(), net::OK, nullptr);

  EXPECT_TRUE(loaded_resources().empty());
}

TEST_F(MetricsWebContentsObserverTest, RecordFeatureUsage) {
  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents());
  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl));
  ASSERT_EQ(main_rfh()->GetLastCommittedURL().spec(), GURL(kDefaultTestUrl));

  std::vector<blink::mojom::WebFeature> web_features;
  web_features.push_back(blink::mojom::WebFeature::kHTMLMarqueeElement);
  web_features.push_back(blink::mojom::WebFeature::kFormAttribute);
  mojom::PageLoadFeatures features(web_features, {}, {});
  MetricsWebContentsObserver::RecordFeatureUsage(main_rfh(), features);

  ASSERT_EQ(observed_features().size(), 1ul);
  ASSERT_EQ(observed_features()[0].features.size(), 2ul);
  EXPECT_EQ(observed_features()[0].features[0],
            blink::mojom::WebFeature::kHTMLMarqueeElement);
  EXPECT_EQ(observed_features()[0].features[1],
            blink::mojom::WebFeature::kFormAttribute);
}

TEST_F(MetricsWebContentsObserverTest, RecordFeatureUsageNoObserver) {
  // Reset the state of the tests, and don't add an observer.
  DeleteContents();
  SetContents(CreateTestWebContents());

  // This call should just do nothing, and should not crash - if that happens,
  // we are good.
  std::vector<blink::mojom::WebFeature> web_features;
  web_features.push_back(blink::mojom::WebFeature::kHTMLMarqueeElement);
  web_features.push_back(blink::mojom::WebFeature::kFormAttribute);
  mojom::PageLoadFeatures features(web_features, {}, {});
  MetricsWebContentsObserver::RecordFeatureUsage(main_rfh(), features);
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
TEST_F(MetricsWebContentsObserverTest,
       RecordFeatureUsageIgnoresChromeExtensionUpdates) {
  // Register our fake extension. The URL we access must be part of the
  // 'web_accessible_resources' for the network commit to work.
  base::DictionaryValue manifest;
  manifest.SetString(extensions::manifest_keys::kVersion, "1.0.0.0");
  manifest.SetString(extensions::manifest_keys::kName, "TestExtension");
  manifest.SetInteger(extensions::manifest_keys::kManifestVersion, 2);
  manifest.Set("web_accessible_resources",
               extensions::ListBuilder().Append("main.html").Build());
  std::string error;
  scoped_refptr<extensions::Extension> extension =
      extensions::Extension::Create(
          base::FilePath(FILE_PATH_LITERAL("//no-such-file")),
          extensions::Manifest::INVALID_LOCATION, manifest,
          extensions::Extension::NO_FLAGS, "mbflcebpggnecokmikipoihdbecnjfoj",
          &error);
  ASSERT_TRUE(error.empty());
  extensions::ExtensionRegistry::Get(web_contents()->GetBrowserContext())
      ->AddEnabled(extension);

  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents());
  const GURL chrome_extension_url(
      "chrome-extension://mbflcebpggnecokmikipoihdbecnjfoj/main.html");
  web_contents_tester->NavigateAndCommit(GURL(chrome_extension_url));
  ASSERT_EQ(main_rfh()->GetLastCommittedURL().spec(),
            GURL(chrome_extension_url));

  std::vector<blink::mojom::WebFeature> web_features;
  web_features.push_back(blink::mojom::WebFeature::kHTMLMarqueeElement);
  web_features.push_back(blink::mojom::WebFeature::kFormAttribute);
  mojom::PageLoadFeatures features(web_features, {}, {});
  MetricsWebContentsObserver::RecordFeatureUsage(main_rfh(), features);

  // The features come from an extension source, so shouldn't be counted.
  EXPECT_EQ(observed_features().size(), 0ul);
}
#endif

}  // namespace page_load_metrics
