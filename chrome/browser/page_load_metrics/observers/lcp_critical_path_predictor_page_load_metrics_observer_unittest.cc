// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/lcp_critical_path_predictor_page_load_metrics_observer.h"

#include <map>
#include <memory>

#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"
#include "chrome/browser/predictors/loading_data_collector.h"
#include "chrome/browser/predictors/loading_predictor.h"
#include "chrome/browser/predictors/loading_predictor_factory.h"
#include "components/history/core/test/history_service_test_util.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "components/page_load_metrics/common/page_load_metrics.mojom.h"
#include "components/page_load_metrics/common/test/page_load_metrics_test_util.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_contents_tester.h"

#include "third_party/blink/public/mojom/lcp_critical_path_predictor/lcp_critical_path_predictor.mojom.h"

namespace predictors {
class PredictorInitializer : public TestObserver {
 public:
  explicit PredictorInitializer(ResourcePrefetchPredictor* predictor)
      : TestObserver(predictor), predictor_(predictor) {}

  PredictorInitializer(const PredictorInitializer&) = delete;
  PredictorInitializer& operator=(const PredictorInitializer&) = delete;

  void WaitUntilInitialized() {
    ASSERT_EQ(predictor_->initialization_state_,
              ResourcePrefetchPredictor::NOT_INITIALIZED);
    predictor_->StartInitialization();
    run_loop_.Run();
  }

  void OnPredictorInitialized() override { run_loop_.Quit(); }

 private:
  raw_ptr<ResourcePrefetchPredictor> predictor_;
  base::RunLoop run_loop_;
};
}  // namespace predictors

class LcpCriticalPathPredictorPageLoadMetricsObserverTest
    : public page_load_metrics::PageLoadMetricsObserverTestHarness {
 protected:
  void SetUp() final {
    page_load_metrics::PageLoadMetricsObserverTestHarness::SetUp();

    page_load_metrics::InitPageLoadTimingForTest(&timing_);
    timing_.navigation_start = base::Time::FromDoubleT(1);
    timing_.parse_timing->parse_start = base::Milliseconds(10);
    timing_.paint_timing->first_paint = base::Seconds(2);
    timing_.paint_timing->first_contentful_paint = base::Seconds(3);
    timing_.paint_timing->first_meaningful_paint = base::Seconds(4);

    timing_.paint_timing->largest_contentful_paint->largest_image_paint =
        base::Seconds(5);
    timing_.paint_timing->largest_contentful_paint->largest_image_paint_size =
        100u;

    PopulateRequiredTimingFields(&timing_);

    Profile* profile = Profile::FromBrowserContext(browser_context());
    HistoryServiceFactory::GetInstance()->SetTestingFactory(
        browser_context(), HistoryServiceFactory::GetDefaultFactory());
    history::HistoryService* history_service =
        HistoryServiceFactory::GetForProfile(
            profile, ServiceAccessType::EXPLICIT_ACCESS);
    ASSERT_TRUE(history_service);
    auto* loading_predictor =
        predictors::LoadingPredictorFactory::GetForProfile(profile);
    predictors::PredictorInitializer initializer(
        loading_predictor->resource_prefetch_predictor());
    initializer.WaitUntilInitialized();
  }

  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    auto lcpp_observer =
        std::make_unique<LcpCriticalPathPredictorPageLoadMetricsObserver>();
    lcpp_observers_[tracker->GetUrl()] = lcpp_observer.get();
    tracker->AddObserver(std::move(lcpp_observer));
  }

  void ProvideLCPPHint(content::NavigationSimulator* navigation) {
    blink::mojom::LCPCriticalPathPredictorNavigationTimeHint hint;
    hint.lcp_element_locators = {"dummy"};
    navigation->GetNavigationHandle()->SetLCPPNavigationHint(hint);
  }
  void SetDummyLcpElementLocator(GURL url) {
    lcpp_observers_[url]->SetLcpElementLocator("dummy");
  }

  void TestUMA(GURL url, bool learn_lcpp, bool record_uma) {
    // Navigate to about:blank to force hint/histogram recording.
    tester()->NavigateToUntrackedUrl();

    auto* loading_predictor =
        predictors::LoadingPredictorFactory::GetForProfile(
            Profile::FromBrowserContext(browser_context()));
    absl::optional<predictors::LcppData> lcpp_data =
        loading_predictor->resource_prefetch_predictor()->GetLcppData(url);
    EXPECT_EQ(learn_lcpp, lcpp_data.has_value());

    base::Histogram::Count expected_count = record_uma ? 1 : 0;
    tester()->histogram_tester().ExpectTotalCount(
        internal::kHistogramLCPPFirstContentfulPaint, expected_count);
    tester()->histogram_tester().ExpectTotalCount(
        internal::kHistogramLCPPLargestContentfulPaint, expected_count);
  }

  void TestHistogramsRecorded(bool provide_lcpp_hint) {
    const GURL main_frame_url("https://test.example");

    std::unique_ptr<content::NavigationSimulator> navigation =
        content::NavigationSimulator::CreateBrowserInitiated(main_frame_url,
                                                             web_contents());

    navigation->Start();
    if (provide_lcpp_hint) {
      ProvideLCPPHint(navigation.get());
    }
    navigation->Commit();
    tester()->SimulateTimingUpdate(timing_);
    SetDummyLcpElementLocator(main_frame_url);

    TestUMA(main_frame_url, /*learn_lcpp=*/true,
            /*record_uma=*/provide_lcpp_hint);
  }

  void TestPrerender(bool activate) {
    content::test::ScopedPrerenderWebContentsDelegate web_contents_delegate(
        *web_contents());

    // Navigate to the initial page to set the initiator page's origin
    // explicitly.
    NavigateAndCommit(GURL("https://www.example.com"));

    const GURL kPrerenderingUrl("https://www.example.com/prerender");
    auto* web_contents_tester = content::WebContentsTester::For(web_contents());
    std::unique_ptr<content::NavigationSimulator> navigation =
        web_contents_tester->AddPrerenderAndStartNavigation(kPrerenderingUrl);
    ProvideLCPPHint(navigation.get());
    navigation->Commit();

    if (activate) {
      web_contents_tester->ActivatePrerenderedPage(kPrerenderingUrl);
      timing_.activation_start = base::Seconds(1);
      tester()->SimulateTimingUpdate(timing_,
                                     navigation->GetFinalRenderFrameHost());
      SetDummyLcpElementLocator(kPrerenderingUrl);
    }

    TestUMA(kPrerenderingUrl,
            /*learn_lcpp=*/false, /*record_uma=*/activate);
  }

  page_load_metrics::mojom::PageLoadTiming timing_;
  std::map<GURL, LcpCriticalPathPredictorPageLoadMetricsObserver*>
      lcpp_observers_;
};

TEST_F(LcpCriticalPathPredictorPageLoadMetricsObserverTest,
       MetricsRecordedWhenHintProvided) {
  TestHistogramsRecorded(true);
}

TEST_F(LcpCriticalPathPredictorPageLoadMetricsObserverTest,
       MetricsNotRecordedWithoutHint) {
  TestHistogramsRecorded(false);
}

TEST_F(LcpCriticalPathPredictorPageLoadMetricsObserverTest,
       PrerenderAndActivate) {
  TestPrerender(/*activate=*/true);
}
TEST_F(LcpCriticalPathPredictorPageLoadMetricsObserverTest,
       PrerenderButNotActivate) {
  TestPrerender(/*activate=*/false);
}
