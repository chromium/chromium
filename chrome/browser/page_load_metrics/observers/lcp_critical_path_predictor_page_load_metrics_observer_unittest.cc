// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/lcp_critical_path_predictor_page_load_metrics_observer.h"

#include <map>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"
#include "chrome/browser/predictors/loading_data_collector.h"
#include "chrome/browser/predictors/loading_predictor.h"
#include "chrome/browser/predictors/loading_predictor_factory.h"
#include "chrome/browser/predictors/predictors_features.h"
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
    timing_.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
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

    max_lcpp_histogram_buckets_ =
        blink::features::kLCPCriticalPathPredictorMaxHistogramBuckets.Get();
  }

  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    auto lcpp_observer =
        std::make_unique<LcpCriticalPathPredictorPageLoadMetricsObserver>();
    lcpp_observers_[tracker->GetUrl()] = lcpp_observer.get();
    tracker->AddObserver(std::move(lcpp_observer));
  }

  void ProvideLCPPHint(content::NavigationSimulator* navigation) {
    blink::mojom::LCPCriticalPathPredictorNavigationTimeHint hint;
    hint.lcp_element_locators = {"foo"};
    navigation->GetNavigationHandle()->SetLCPPNavigationHint(hint);
  }

  void SetMockLcpElementLocator(
      GURL url,
      const std::string& mock_element_locator = "foo",
      std::optional<uint32_t> mock_predicted_index = std::nullopt) {
    lcpp_observers_[url]->SetLcpElementLocator(mock_element_locator,
                                               mock_predicted_index);
  }

  void ExpectNoHistogram(const char* name,
                         const base::Location& location = FROM_HERE) {
    EXPECT_THAT(tester()->histogram_tester().GetAllSamples(name),
                base::BucketsAre())
        << location.ToString();
  }

  void ConfirmResult(GURL url,
                     bool learn_lcpp,
                     bool record_uma,
                     const base::Location& location = FROM_HERE) {
    // Navigate to about:blank to force hint/histogram recording.
    tester()->NavigateToUntrackedUrl();

    auto* loading_predictor =
        predictors::LoadingPredictorFactory::GetForProfile(
            Profile::FromBrowserContext(browser_context()));
    std::optional<predictors::LcppStat> lcpp_stat =
        loading_predictor->resource_prefetch_predictor()->GetLcppStat(
            /*initiator_origin=*/std::nullopt, url);
    EXPECT_EQ(learn_lcpp, lcpp_stat.has_value()) << location.ToString();

    base::Histogram::Count expected_count = record_uma ? 1 : 0;
    tester()->histogram_tester().ExpectTotalCount(
        internal::kHistogramLCPPFirstContentfulPaint, expected_count, location);
    tester()->histogram_tester().ExpectTotalCount(
        internal::kHistogramLCPPLargestContentfulPaint, expected_count,
        location);

    ExpectNoHistogram(internal::kHistogramLCPPPredictResult, location);
    ExpectNoHistogram(internal::kHistogramLCPPPredictHitIndex, location);
    ExpectNoHistogram(internal::kHistogramLCPPActualLCPIndex, location);
  }

  void NavigationWithLCPPHint(GURL url, bool provide_lcpp_hint) {
    std::unique_ptr<content::NavigationSimulator> navigation =
        content::NavigationSimulator::CreateBrowserInitiated(url,
                                                             web_contents());
    navigation->Start();
    if (provide_lcpp_hint) {
      ProvideLCPPHint(navigation.get());
    }
    navigation->Commit();
    tester()->SimulateTimingUpdate(timing_);
  }

  void TestSimpleNavigation(bool provide_lcpp_hint) {
    const GURL main_frame_url("https://test.example");
    NavigationWithLCPPHint(main_frame_url, provide_lcpp_hint);
    SetMockLcpElementLocator(main_frame_url);

    ConfirmResult(main_frame_url, /*learn_lcpp=*/true,
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
      SetMockLcpElementLocator(kPrerenderingUrl);
    }

    ConfirmResult(kPrerenderingUrl,
                  /*learn_lcpp=*/false, /*record_uma=*/activate);
  }

  static const uint32_t kNotFound = static_cast<uint32_t>(-1);

  void TestLCPPrediction(std::vector<uint32_t> predicted_lcp_indexes,
                         internal::LCPPPredictResult expect) {
    const GURL main_frame_url("https://test.example");
    // Let predictor learn pseudo("lcp_previous") LCP locator
    predictors::ResourcePrefetchPredictor* predictor =
        predictors::LoadingPredictorFactory::GetForProfile(
            Profile::FromBrowserContext(web_contents()->GetBrowserContext()))
            ->resource_prefetch_predictor();
    CHECK(predictor);
    predictors::LcppDataInputs lcpp_data_inputs;
    lcpp_data_inputs.lcp_element_locator = "lcp_previous";
    predictor->LearnLcpp(/*initiator_origin=*/std::nullopt, main_frame_url,
                         lcpp_data_inputs);

    // Predict LCP with the learned result.
    NavigationWithLCPPHint(main_frame_url, /*provide_lcpp_hint=*/true);
    for (auto index : predicted_lcp_indexes) {
      SetMockLcpElementLocator(
          main_frame_url, "lcp_actual",
          index == kNotFound ? std::nullopt : std::optional<uint32_t>(index));
    }
    tester()->NavigateToUntrackedUrl();
    EXPECT_THAT(tester()->histogram_tester().GetAllSamples(
                    internal::kHistogramLCPPPredictResult),
                base::BucketsAre(base::Bucket(expect, 1)));
  }

  void ExpectLCPHistogram(const char* name,
                          uint32_t value,
                          const base::Location& location = FROM_HERE) {
    EXPECT_THAT(tester()->histogram_tester().GetAllSamples(name),
                base::BucketsAre(base::Bucket(
                    value + internal::kLCPIndexHistogramOffset, 1)))
        << location.ToString();
  }

  int NotFound() { return max_lcpp_histogram_buckets_; }

 private:
  page_load_metrics::mojom::PageLoadTiming timing_;
  std::map<
      GURL,
      raw_ptr<LcpCriticalPathPredictorPageLoadMetricsObserver, CtnExperimental>>
      lcpp_observers_;
  int max_lcpp_histogram_buckets_;
};

TEST_F(LcpCriticalPathPredictorPageLoadMetricsObserverTest,
       MetricsRecordedWhenHintProvided) {
  TestSimpleNavigation(/*provide_lcpp_hint=*/true);
}

TEST_F(LcpCriticalPathPredictorPageLoadMetricsObserverTest,
       MetricsNotRecordedWithoutHint) {
  TestSimpleNavigation(/*provide_lcpp_hint=*/false);
}

TEST_F(LcpCriticalPathPredictorPageLoadMetricsObserverTest,
       PrerenderAndActivate) {
  TestPrerender(/*activate=*/true);
}

TEST_F(LcpCriticalPathPredictorPageLoadMetricsObserverTest,
       PrerenderButNotActivate) {
  TestPrerender(/*activate=*/false);
}

TEST_F(LcpCriticalPathPredictorPageLoadMetricsObserverTest, PredictLCPSuccess) {
  TestLCPPrediction({0u}, internal::LCPPPredictResult::kSuccess);
  ExpectLCPHistogram(internal::kHistogramLCPPPredictHitIndex, 0u);
  ExpectLCPHistogram(internal::kHistogramLCPPActualLCPIndex, 0u);
}

TEST_F(LcpCriticalPathPredictorPageLoadMetricsObserverTest,
       PredictLCPSuccess2) {
  TestLCPPrediction({kNotFound, 0u}, internal::LCPPPredictResult::kSuccess);
  ExpectLCPHistogram(internal::kHistogramLCPPPredictHitIndex, 0u);
  ExpectLCPHistogram(internal::kHistogramLCPPActualLCPIndex, 0u);
}

TEST_F(LcpCriticalPathPredictorPageLoadMetricsObserverTest, PredictLCPFailed) {
  TestLCPPrediction({kNotFound}, internal::LCPPPredictResult::kFailureNoHit);
  ExpectNoHistogram(internal::kHistogramLCPPPredictHitIndex);
  ExpectLCPHistogram(internal::kHistogramLCPPActualLCPIndex, NotFound());
}

TEST_F(LcpCriticalPathPredictorPageLoadMetricsObserverTest, PredictLCPFailed2) {
  TestLCPPrediction({0u, kNotFound},
                    internal::LCPPPredictResult::kFailureActuallyUnrecordedLCP);
  ExpectNoHistogram(internal::kHistogramLCPPPredictHitIndex);
  ExpectLCPHistogram(internal::kHistogramLCPPActualLCPIndex, NotFound());
}

TEST_F(LcpCriticalPathPredictorPageLoadMetricsObserverTest, PredictLCPFailed3) {
  TestLCPPrediction(
      {0u, 0u}, internal::LCPPPredictResult::kFailureActuallySameButLaterLCP);
  ExpectNoHistogram(internal::kHistogramLCPPPredictHitIndex);
  ExpectLCPHistogram(internal::kHistogramLCPPActualLCPIndex, 0u);
}

TEST_F(LcpCriticalPathPredictorPageLoadMetricsObserverTest, PredictLCPFailed4) {
  TestLCPPrediction({0u, 1u},
                    internal::LCPPPredictResult::kFailureActuallySecondaryLCP);
  ExpectNoHistogram(internal::kHistogramLCPPPredictHitIndex);
  ExpectLCPHistogram(internal::kHistogramLCPPActualLCPIndex, 1u);
}
