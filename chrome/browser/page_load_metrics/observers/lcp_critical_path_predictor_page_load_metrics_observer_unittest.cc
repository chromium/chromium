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

std::optional<std::string>& GetLcpElementLocatorForCriticalPathPredictor(
    LcppDataInputs& inputs) {
  static const bool kCriticalPathPredictorImageOnly =
      (blink::features::kLCPCriticalPathPredictorRecordedLcpElementTypes
           .Get() == blink::features::LcppRecordedLcpElementTypes::kImageOnly);
  return kCriticalPathPredictorImageOnly ? inputs.lcp_element_locator_image
                                         : inputs.lcp_element_locator;
}

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
      bool is_image_element = true,
      std::optional<uint32_t> mock_predicted_index = std::nullopt) {
    lcpp_observers_[url]->OnLcpUpdated(blink::mojom::LcpElement::New(
        mock_element_locator, is_image_element, mock_predicted_index));
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

    base::Histogram::Count32 expected_count = record_uma ? 1 : 0;
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

  void TestSimpleNavigation(bool provide_lcpp_hint,
                            bool is_image_element = true) {
    const GURL main_frame_url("https://test.example");
    NavigationWithLCPPHint(main_frame_url, provide_lcpp_hint);
    SetMockLcpElementLocator(main_frame_url,
                             /*mock_element_locator=*/"foo", is_image_element);

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
                         internal::LCPPPredictResult expect,
                         const base::Location& location = FROM_HERE) {
    const GURL main_frame_url("https://test.example");
    // Let predictor learn pseudo("lcp_previous") LCP locator
    predictors::ResourcePrefetchPredictor* predictor =
        predictors::LoadingPredictorFactory::GetForProfile(
            Profile::FromBrowserContext(web_contents()->GetBrowserContext()))
            ->resource_prefetch_predictor();
    CHECK(predictor);
    predictors::LcppDataInputs lcpp_data_inputs;
    lcpp_data_inputs.lcp_element_locator = "lcp_previous";
    lcpp_data_inputs.lcp_element_locator_image = "lcp_previous";
    predictor->LearnLcpp(/*initiator_origin=*/std::nullopt, main_frame_url,
                         lcpp_data_inputs);

    // Predict LCP with the learned result.
    NavigationWithLCPPHint(main_frame_url, /*provide_lcpp_hint=*/true);
    for (auto index : predicted_lcp_indexes) {
      SetMockLcpElementLocator(
          main_frame_url, "lcp_actual",
          /*is_image_element=*/true,
          index == kNotFound ? std::nullopt : std::optional<uint32_t>(index));
    }
    tester()->NavigateToUntrackedUrl();
    tester()->histogram_tester().ExpectUniqueSample(
        internal::kHistogramLCPPPredictResult, expect, 1, location);
  }

  template <class T>
  void ExpectUniqueSample(const char* name,
                          const T& value,
                          const base::Location& location = FROM_HERE) {
    tester()->histogram_tester().ExpectUniqueSample(name, value, 1, location);
  }

  void ExpectLCPHistogram(const char* name,
                          uint32_t value,
                          const base::Location& location = FROM_HERE) {
    ExpectUniqueSample(name, value + internal::kLCPIndexHistogramOffset,
                       location);
  }

  int NotFound() { return max_lcpp_histogram_buckets_; }

  const page_load_metrics::mojom::PageLoadTiming& Timing() const {
    return timing_;
  }

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
  tester()->histogram_tester().ExpectUniqueTimeSample(
      internal::kHistogramLCPPPredictSuccessLCPTiming,
      *Timing().paint_timing->largest_contentful_paint->largest_image_paint, 1);
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

TEST_F(LcpCriticalPathPredictorPageLoadMetricsObserverTest, UMAIsImageTrue) {
  TestSimpleNavigation(/*provide_lcpp_hint=*/true,
                       /*is_image_element=*/true);
  ExpectUniqueSample(internal::kHistogramLCPPActualLCPIsImage, true);
}

TEST_F(LcpCriticalPathPredictorPageLoadMetricsObserverTest, UMAIsImageFalse) {
  TestSimpleNavigation(/*provide_lcpp_hint=*/true,
                       /*is_image_element=*/false);
  ExpectUniqueSample(internal::kHistogramLCPPActualLCPIsImage, false);
}

TEST(MaybeReportConfidenceUMAsTest, Empty) {
  const auto kEmptyHistograms = {
      internal::kHistogramLCPPImageLoadingPriorityFrequencyOfActualPositive,
      internal::kHistogramLCPPImageLoadingPriorityFrequencyOfActualNegative,
      internal::kHistogramLCPPImageLoadingPriorityConfidenceOfActualPositive,
      internal::kHistogramLCPPImageLoadingPriorityConfidenceOfActualNegative,
      internal::kHistogramLCPPSubresourceFrequencyOfActualPositive,
      internal::kHistogramLCPPSubresourceFrequencyOfActualNegative,
      internal::kHistogramLCPPSubresourceConfidenceOfActualPositive,
      internal::kHistogramLCPPSubresourceConfidenceOfActualNegative,
      internal::kHistogramLCPPSubresourceFrequencyOfActualPositiveSameSite,
      internal::kHistogramLCPPSubresourceFrequencyOfActualNegativeSameSite,
      internal::kHistogramLCPPSubresourceConfidenceOfActualPositiveSameSite,
      internal::kHistogramLCPPSubresourceConfidenceOfActualNegativeSameSite,
      internal::kHistogramLCPPSubresourceFrequencyOfActualPositiveCrossSite,
      internal::kHistogramLCPPSubresourceFrequencyOfActualNegativeCrossSite,
      internal::kHistogramLCPPSubresourceConfidenceOfActualPositiveCrossSite,
      internal::kHistogramLCPPSubresourceConfidenceOfActualNegativeCrossSite,
  };
  const auto expect_empty = [&kEmptyHistograms](
                                const base::HistogramTester& histogram_tester,
                                const base::Location& location = FROM_HERE) {
    for (const auto& name : kEmptyHistograms) {
      histogram_tester.ExpectTotalCount(name, 0, location);
    }
  };
  {
    base::HistogramTester histogram_tester;
    predictors::LcppDataInputs lcpp_data_inputs;
    internal::MaybeReportConfidenceUMAsForTesting(GURL(), std::nullopt,
                                                  lcpp_data_inputs);
    expect_empty(histogram_tester);
  }
  {
    base::HistogramTester histogram_tester;
    predictors::LcppStat lcpp_stat_prelearn;
    predictors::LcppDataInputs lcpp_data_inputs;
    internal::MaybeReportConfidenceUMAsForTesting(GURL(), lcpp_stat_prelearn,
                                                  lcpp_data_inputs);
    expect_empty(histogram_tester);
  }
}

TEST(MaybeReportConfidenceUMAsTest, ImageLoadingPriority) {
  const auto kEmptyHistograms = {
      internal::kHistogramLCPPSubresourceFrequencyOfActualPositive,
      internal::kHistogramLCPPSubresourceFrequencyOfActualNegative,
      internal::kHistogramLCPPSubresourceConfidenceOfActualPositive,
      internal::kHistogramLCPPSubresourceConfidenceOfActualNegative,
      internal::kHistogramLCPPSubresourceFrequencyOfActualPositiveSameSite,
      internal::kHistogramLCPPSubresourceFrequencyOfActualNegativeSameSite,
      internal::kHistogramLCPPSubresourceConfidenceOfActualPositiveSameSite,
      internal::kHistogramLCPPSubresourceConfidenceOfActualNegativeSameSite,
      internal::kHistogramLCPPSubresourceFrequencyOfActualPositiveCrossSite,
      internal::kHistogramLCPPSubresourceFrequencyOfActualNegativeCrossSite,
      internal::kHistogramLCPPSubresourceConfidenceOfActualPositiveCrossSite,
      internal::kHistogramLCPPSubresourceConfidenceOfActualNegativeCrossSite,
  };
  const auto expect_empty = [&kEmptyHistograms](
                                const base::HistogramTester& histogram_tester,
                                const base::Location& location = FROM_HERE) {
    for (const auto& name : kEmptyHistograms) {
      histogram_tester.ExpectTotalCount(name, 0, location);
    }
  };

  {
    base::HistogramTester histogram_tester;
    predictors::LcppStat lcpp_stat_prelearn;
    {
      auto& stat = *lcpp_stat_prelearn.mutable_lcp_element_locator_stat();
      {
        auto& bucket = *stat.add_lcp_element_locator_buckets();
        bucket.set_lcp_element_locator("#a");
        bucket.set_frequency(3);  // 30%
      }
      {
        auto& bucket = *stat.add_lcp_element_locator_buckets();
        bucket.set_lcp_element_locator("#b");
        bucket.set_frequency(2);  // 20%
      }
      stat.set_other_bucket_frequency(5);  // 50%
    }
    predictors::LcppDataInputs lcpp_data_inputs;
    GetLcpElementLocatorForCriticalPathPredictor(lcpp_data_inputs) = "#a";
    internal::MaybeReportConfidenceUMAsForTesting(
        GURL("https://a.com"), lcpp_stat_prelearn, lcpp_data_inputs);
    // "#a" will an actual positive sample.
    int frequency_of_a = 3;
    histogram_tester.ExpectUniqueSample(
        internal::kHistogramLCPPImageLoadingPriorityFrequencyOfActualPositive,
        frequency_of_a, /*expected_bucket_count=*/1);
    int confidence_of_a = 100 * 3 / (3 + 2 + 5);
    EXPECT_EQ(confidence_of_a, 30);
    histogram_tester.ExpectUniqueSample(
        internal::kHistogramLCPPImageLoadingPriorityConfidenceOfActualPositive,
        confidence_of_a, /*expected_bucket_count=*/1);
    // "#b" will be an actual negative sample.
    int frequency_of_b = 2;
    histogram_tester.ExpectUniqueSample(
        internal::kHistogramLCPPImageLoadingPriorityFrequencyOfActualNegative,
        frequency_of_b, /*expected_bucket_count=*/1);
    int confidence_of_b = 100 * 2 / (3 + 2 + 5);
    EXPECT_EQ(confidence_of_b, 20);
    histogram_tester.ExpectUniqueSample(
        internal::kHistogramLCPPImageLoadingPriorityConfidenceOfActualNegative,
        confidence_of_b, /*expected_bucket_count=*/1);
    expect_empty(histogram_tester);
  }

  {
    base::HistogramTester histogram_tester;
    predictors::LcppStat lcpp_stat_prelearn;
    {
      auto& stat = *lcpp_stat_prelearn.mutable_lcp_element_locator_stat();
      {
        auto& bucket = *stat.add_lcp_element_locator_buckets();
        bucket.set_lcp_element_locator("#a");
        bucket.set_frequency(3);
      }
      {
        auto& bucket = *stat.add_lcp_element_locator_buckets();
        bucket.set_lcp_element_locator("#b");
        bucket.set_frequency(2);
      }
      stat.set_other_bucket_frequency(5);
    }
    predictors::LcppDataInputs lcpp_data_inputs;
    GetLcpElementLocatorForCriticalPathPredictor(lcpp_data_inputs) = "#c";
    internal::MaybeReportConfidenceUMAsForTesting(
        GURL("https://a.com"), lcpp_stat_prelearn, lcpp_data_inputs);
    // "#c" is an actual positive sample.
    int frequency_of_c = 0;
    histogram_tester.ExpectUniqueSample(
        internal::kHistogramLCPPImageLoadingPriorityFrequencyOfActualPositive,
        frequency_of_c, /*expected_bucket_count=*/1);
    int confidence_of_c = 0;
    histogram_tester.ExpectUniqueSample(
        internal::kHistogramLCPPImageLoadingPriorityConfidenceOfActualPositive,
        confidence_of_c, /*expected_bucket_count=*/1);
    // "#a" and "#b" are the actual negative samples.
    histogram_tester.ExpectTotalCount(
        internal::kHistogramLCPPImageLoadingPriorityFrequencyOfActualNegative,
        /*expected_count=*/2);
    int frequency_of_a = 3;
    histogram_tester.ExpectBucketCount(
        internal::kHistogramLCPPImageLoadingPriorityFrequencyOfActualNegative,
        frequency_of_a, /*expected_count=*/1);
    int frequency_of_b = 2;
    histogram_tester.ExpectBucketCount(
        internal::kHistogramLCPPImageLoadingPriorityFrequencyOfActualNegative,
        frequency_of_b, /*expected_count=*/1);
    histogram_tester.ExpectTotalCount(
        internal::kHistogramLCPPImageLoadingPriorityConfidenceOfActualNegative,
        /*expected_count=*/2);
    int confidence_of_a = 100 * 3 / (3 + 2 + 5);
    EXPECT_EQ(confidence_of_a, 30);
    histogram_tester.ExpectBucketCount(
        internal::kHistogramLCPPImageLoadingPriorityConfidenceOfActualNegative,
        confidence_of_a, /*expected_count=*/1);
    int confidence_of_b = 100 * 2 / (3 + 2 + 5);
    EXPECT_EQ(confidence_of_b, 20);
    histogram_tester.ExpectBucketCount(
        internal::kHistogramLCPPImageLoadingPriorityConfidenceOfActualNegative,
        confidence_of_b, /*expected_count=*/1);
    expect_empty(histogram_tester);
  }

  {
    base::HistogramTester histogram_tester;
    predictors::LcppStat lcpp_stat_prelearn;
    {
      auto& stat = *lcpp_stat_prelearn.mutable_lcp_element_locator_stat();
      {
        auto& bucket = *stat.add_lcp_element_locator_buckets();
        bucket.set_lcp_element_locator("#a");
        bucket.set_frequency(1000);  // confidence = 100%
      }
      stat.set_other_bucket_frequency(0);
    }
    predictors::LcppDataInputs lcpp_data_inputs;
    GetLcpElementLocatorForCriticalPathPredictor(lcpp_data_inputs) = "#a";
    internal::MaybeReportConfidenceUMAsForTesting(
        GURL("https://a.com"), lcpp_stat_prelearn, lcpp_data_inputs);
    int total_frequency = 1000;
    int frequency_of_a = 1000;
    int confidence_of_a = 100;
    histogram_tester.ExpectUniqueSample(
        internal::kHistogramLCPPImageLoadingPriorityFrequencyOfActualPositive,
        frequency_of_a, /*expected_bucket_count=*/1);
    histogram_tester.ExpectUniqueSample(
        internal::kHistogramLCPPImageLoadingPriorityConfidenceOfActualPositive,
        confidence_of_a, /*expected_bucket_count=*/1);
    histogram_tester.ExpectTotalCount(
        internal::kHistogramLCPPImageLoadingPriorityFrequencyOfActualNegative,
        /*expected_count=*/0);
    histogram_tester.ExpectTotalCount(
        internal::kHistogramLCPPImageLoadingPriorityConfidenceOfActualNegative,
        /*expected_count=*/0);
    histogram_tester.ExpectUniqueSample(
        "PageLoad.Clients.LCPP."
        "ImageLoadingPriority"
        ".TotalFrequencyOfActualPositive"
        ".PerConfidence.3",
        99, /*expected_bucket_count=*/1);
    histogram_tester.ExpectUniqueSample(
        "PageLoad.Clients.LCPP."
        "ImageLoadingPriority"
        ".TotalFrequencyOfActualPositive"
        ".WithConfidence"
        "90To100.2",
        total_frequency, /*expected_bucket_count=*/1);
    histogram_tester.ExpectTotalCount(
        "PageLoad.Clients.LCPP."
        "ImageLoadingPriority"
        ".TotalFrequencyOfActualNegative"
        ".PerConfidence.3",
        /*expected_count=*/0);
    histogram_tester.ExpectTotalCount(
        "PageLoad.Clients.LCPP."
        "ImageLoadingPriority"
        ".TotalFrequencyOfActualNegative"
        ".WithConfidence"
        "90To100.2",
        /*expected_count=*/0);
    expect_empty(histogram_tester);
  }

  {
    base::HistogramTester histogram_tester;
    predictors::LcppStat lcpp_stat_prelearn;
    {
      auto& stat = *lcpp_stat_prelearn.mutable_lcp_element_locator_stat();
      {
        auto& bucket = *stat.add_lcp_element_locator_buckets();
        bucket.set_lcp_element_locator("#a");
        bucket.set_frequency(1000);  // confidence = 100%
      }
      stat.set_other_bucket_frequency(0);
    }
    predictors::LcppDataInputs lcpp_data_inputs;
    GetLcpElementLocatorForCriticalPathPredictor(lcpp_data_inputs) = "#b";
    internal::MaybeReportConfidenceUMAsForTesting(
        GURL("https://a.com"), lcpp_stat_prelearn, lcpp_data_inputs);
    // "#a" is an actual positive sample.
    int total_frequency = 1000;
    int frequency_of_a = 1000;
    int confidence_of_a = 100;
    int frequency_of_b = 0;
    int confidence_of_b = 0;
    histogram_tester.ExpectUniqueSample(
        internal::kHistogramLCPPImageLoadingPriorityFrequencyOfActualPositive,
        frequency_of_b, /*expected_bucket_count=*/1);
    histogram_tester.ExpectUniqueSample(
        internal::kHistogramLCPPImageLoadingPriorityConfidenceOfActualPositive,
        confidence_of_b, /*expected_bucket_count=*/1);
    histogram_tester.ExpectUniqueSample(
        internal::kHistogramLCPPImageLoadingPriorityFrequencyOfActualNegative,
        frequency_of_a, /*expected_bucket_count=*/1);
    histogram_tester.ExpectUniqueSample(
        internal::kHistogramLCPPImageLoadingPriorityConfidenceOfActualNegative,
        confidence_of_a, /*expected_bucket_count=*/1);
    histogram_tester.ExpectUniqueSample(
        "PageLoad.Clients.LCPP"
        ".ImageLoadingPriority"
        ".TotalFrequencyOfActualPositive"
        ".PerConfidence.3",
        9, /*expected_bucket_count=*/1);
    histogram_tester.ExpectUniqueSample(
        "PageLoad.Clients.LCPP"
        ".ImageLoadingPriority"
        ".TotalFrequencyOfActualPositive"
        ".WithConfidence"
        "0To10.2",
        total_frequency, /*expected_bucket_count=*/1);
    histogram_tester.ExpectUniqueSample(
        "PageLoad.Clients.LCPP"
        ".ImageLoadingPriority"
        ".TotalFrequencyOfActualNegative"
        ".PerConfidence.3",
        99, /*expected_bucket_count=*/1);
    histogram_tester.ExpectUniqueSample(
        "PageLoad.Clients.LCPP"
        ".ImageLoadingPriority"
        ".TotalFrequencyOfActualNegative"
        ".WithConfidence"
        "90To100.2",
        total_frequency, /*expected_bucket_count=*/1);
    expect_empty(histogram_tester);
  }
}

TEST(MaybeReportConfidenceUMAsTest, Subresource) {
  const auto kEmptyHistograms = {
      internal::kHistogramLCPPImageLoadingPriorityFrequencyOfActualPositive,
      internal::kHistogramLCPPImageLoadingPriorityFrequencyOfActualNegative,
      internal::kHistogramLCPPImageLoadingPriorityConfidenceOfActualPositive,
      internal::kHistogramLCPPImageLoadingPriorityConfidenceOfActualNegative,
  };
  const auto expect_empty = [&kEmptyHistograms](
                                const base::HistogramTester& histogram_tester,
                                const base::Location& location = FROM_HERE) {
    for (const auto& name : kEmptyHistograms) {
      histogram_tester.ExpectTotalCount(name, 0, location);
    }
  };

  {
    base::HistogramTester histogram_tester;
    predictors::LcppStat lcpp_stat_prelearn;
    {
      auto& stat = *lcpp_stat_prelearn.mutable_fetched_subresource_url_stat();
      auto& buckets = *stat.mutable_main_buckets();
      buckets.insert({"https://a.com", 5});  // 25%
      buckets.insert({"https://b.com", 4});  // 20%
      buckets.insert({"https://c.com", 3});  // 15%
      buckets.insert({"https://d.com", 2});  // 10%
      stat.set_other_bucket_frequency(6);    // 30%
    }
    predictors::LcppDataInputs lcpp_data_inputs;
    lcpp_data_inputs.subresource_urls.emplace(
        GURL("https://a.com"),
        std::make_pair(base::Seconds(0),
                       network::mojom::RequestDestination::kEmpty));
    lcpp_data_inputs.subresource_urls.emplace(
        GURL("https://e.com"),
        std::make_pair(base::Seconds(0),
                       network::mojom::RequestDestination::kEmpty));
    internal::MaybeReportConfidenceUMAsForTesting(
        GURL("https://a.com"), lcpp_stat_prelearn, lcpp_data_inputs);

    // {a, e}.com are the actual positive samples.
    histogram_tester.ExpectTotalCount(
        internal::kHistogramLCPPSubresourceFrequencyOfActualPositive,
        /*expected_count=*/2);
    int frequency_of_a = 5;
    histogram_tester.ExpectBucketCount(
        internal::kHistogramLCPPSubresourceFrequencyOfActualPositive,
        frequency_of_a, /*expected_count=*/1);
    int frequency_of_e = 0;
    histogram_tester.ExpectBucketCount(
        internal::kHistogramLCPPSubresourceFrequencyOfActualPositive,
        frequency_of_e, /*expected_count=*/1);
    histogram_tester.ExpectTotalCount(
        internal::kHistogramLCPPSubresourceConfidenceOfActualPositive,
        /*expected_count=*/2);
    int confidence_of_a = 100 * 5 / (5 + 4 + 3 + 2 + 6);
    EXPECT_EQ(confidence_of_a, 25);
    histogram_tester.ExpectBucketCount(
        internal::kHistogramLCPPSubresourceConfidenceOfActualPositive,
        confidence_of_a, /*expected_count=*/1);
    int confidence_of_e = 0;
    histogram_tester.ExpectBucketCount(
        internal::kHistogramLCPPSubresourceConfidenceOfActualPositive,
        confidence_of_e, /*expected_count=*/1);
    histogram_tester.ExpectUniqueSample(
        internal::kHistogramLCPPSubresourceFrequencyOfActualPositiveSameSite,
        frequency_of_a, /*expected_bucket_count=*/1);
    histogram_tester.ExpectUniqueSample(
        internal::kHistogramLCPPSubresourceConfidenceOfActualPositiveSameSite,
        confidence_of_a, /*expected_bucket_count=*/1);
    int frequency_of_g = 0;
    histogram_tester.ExpectUniqueSample(
        internal::kHistogramLCPPSubresourceFrequencyOfActualPositiveCrossSite,
        frequency_of_g, /*expected_bucket_count=*/1);
    int confidence_of_g = 0;
    histogram_tester.ExpectUniqueSample(
        internal::kHistogramLCPPSubresourceConfidenceOfActualPositiveCrossSite,
        confidence_of_g, /*expected_bucket_count=*/1);

    // {b, c, d}.com  are the actual negative samples.
    histogram_tester.ExpectTotalCount(
        internal::kHistogramLCPPSubresourceFrequencyOfActualNegative,
        /*expected_count=*/3);
    int frequency_of_b = 4;
    histogram_tester.ExpectBucketCount(
        internal::kHistogramLCPPSubresourceFrequencyOfActualNegative,
        frequency_of_b, /*expected_count=*/1);
    int frequency_of_c = 3;
    histogram_tester.ExpectBucketCount(
        internal::kHistogramLCPPSubresourceFrequencyOfActualNegative,
        frequency_of_c, /*expected_count=*/1);
    int frequency_of_d = 2;
    histogram_tester.ExpectBucketCount(
        internal::kHistogramLCPPSubresourceFrequencyOfActualNegative,
        frequency_of_d, /*expected_count=*/1);
    histogram_tester.ExpectTotalCount(
        internal::kHistogramLCPPSubresourceConfidenceOfActualNegative,
        /*expected_count=*/3);
    int confidence_of_b = 100 * 4 / (5 + 4 + 3 + 2 + 6);
    EXPECT_EQ(confidence_of_b, 20);
    histogram_tester.ExpectBucketCount(
        internal::kHistogramLCPPSubresourceConfidenceOfActualNegative,
        confidence_of_b, /*expected_count=*/1);
    int confidence_of_c = 100 * 3 / (5 + 4 + 3 + 2 + 6);
    EXPECT_EQ(confidence_of_c, 15);
    histogram_tester.ExpectBucketCount(
        internal::kHistogramLCPPSubresourceConfidenceOfActualNegative,
        confidence_of_c, /*expected_count=*/1);
    int confidence_of_d = 100 * 2 / (5 + 4 + 3 + 2 + 6);
    EXPECT_EQ(confidence_of_d, 10);
    histogram_tester.ExpectBucketCount(
        internal::kHistogramLCPPSubresourceConfidenceOfActualNegative,
        confidence_of_d, /*expected_count=*/1);
    histogram_tester.ExpectTotalCount(
        internal::kHistogramLCPPSubresourceFrequencyOfActualNegativeSameSite,
        0);
    histogram_tester.ExpectTotalCount(
        internal::kHistogramLCPPSubresourceConfidenceOfActualNegativeSameSite,
        0);
    histogram_tester.ExpectTotalCount(
        internal::kHistogramLCPPSubresourceFrequencyOfActualNegativeCrossSite,
        /*expected_count=*/3);
    histogram_tester.ExpectBucketCount(
        internal::kHistogramLCPPSubresourceFrequencyOfActualNegativeCrossSite,
        frequency_of_b, /*expected_count=*/1);
    histogram_tester.ExpectBucketCount(
        internal::kHistogramLCPPSubresourceFrequencyOfActualNegativeCrossSite,
        frequency_of_c, /*expected_count=*/1);
    histogram_tester.ExpectBucketCount(
        internal::kHistogramLCPPSubresourceFrequencyOfActualNegativeCrossSite,
        frequency_of_d, /*expected_count=*/1);
    histogram_tester.ExpectTotalCount(
        internal::kHistogramLCPPSubresourceConfidenceOfActualNegativeCrossSite,
        /*expected_count=*/3);
    histogram_tester.ExpectBucketCount(
        internal::kHistogramLCPPSubresourceConfidenceOfActualNegativeCrossSite,
        confidence_of_b, /*expected_count=*/1);
    histogram_tester.ExpectBucketCount(
        internal::kHistogramLCPPSubresourceConfidenceOfActualNegativeCrossSite,
        confidence_of_c, /*expected_count=*/1);
    histogram_tester.ExpectBucketCount(
        internal::kHistogramLCPPSubresourceConfidenceOfActualNegativeCrossSite,
        confidence_of_d, /*expected_count=*/1);
    expect_empty(histogram_tester);
  }

  {
    base::HistogramTester histogram_tester;
    predictors::LcppStat lcpp_stat_prelearn;
    {
      auto& stat = *lcpp_stat_prelearn.mutable_fetched_subresource_url_stat();
      auto& buckets = *stat.mutable_main_buckets();
      buckets.insert({"https://a.com", 1000});  // 100%
      stat.set_other_bucket_frequency(0);
    }
    predictors::LcppDataInputs lcpp_data_inputs;
    lcpp_data_inputs.subresource_urls.emplace(
        GURL("https://a.com"),
        std::make_pair(base::Seconds(0),
                       network::mojom::RequestDestination::kEmpty));
    internal::MaybeReportConfidenceUMAsForTesting(
        GURL("https://a.com"), lcpp_stat_prelearn, lcpp_data_inputs);
    int total_frequency = 1000;
    histogram_tester.ExpectUniqueSample(
        "PageLoad.Clients.LCPP"
        ".Subresource"
        ".TotalFrequencyOfActualPositive"
        ".PerConfidence"
        ".3",
        99, /*expected_bucket_count=*/1);
    histogram_tester.ExpectUniqueSample(
        "PageLoad.Clients.LCPP"
        ".Subresource"
        ".TotalFrequencyOfActualPositive"
        ".WithConfidence90To100.2",
        total_frequency, /*expected_bucket_count=*/1);
    histogram_tester.ExpectUniqueSample(
        "PageLoad.Clients.LCPP"
        ".Subresource"
        ".TotalFrequencyOfActualPositive"
        ".PerConfidence"
        ".SameSite"
        ".3",
        99, /*expected_bucket_count=*/1);
    histogram_tester.ExpectUniqueSample(
        "PageLoad.Clients.LCPP"
        ".Subresource"
        ".TotalFrequencyOfActualPositive"
        ".WithConfidence90To100"
        ".SameSite.2",
        total_frequency, /*expected_bucket_count=*/1);
    expect_empty(histogram_tester);
  }
}
