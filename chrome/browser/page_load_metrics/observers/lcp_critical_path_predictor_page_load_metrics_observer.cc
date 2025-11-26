// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/lcp_critical_path_predictor_page_load_metrics_observer.h"

#include <algorithm>

#include "base/check_is_test.h"
#include "base/notreached.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/predictors/lcp_critical_path_predictor/lcp_critical_path_predictor_util.h"
#include "chrome/browser/predictors/loading_predictor.h"
#include "chrome/browser/predictors/loading_predictor_factory.h"
#include "chrome/browser/predictors/predictors_features.h"
#include "chrome/browser/profiles/profile.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/url_util.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"

namespace internal {

#define HISTOGRAM_PREFIX "PageLoad.Clients.LCPP."
const char kHistogramLCPPFirstContentfulPaint[] =
    HISTOGRAM_PREFIX "PaintTiming.NavigationToFirstContentfulPaint";
const char kHistogramLCPPLargestContentfulPaint[] =
    HISTOGRAM_PREFIX "PaintTiming.NavigationToLargestContentfulPaint";
const char kHistogramLCPPPredictResult[] =
    HISTOGRAM_PREFIX "PaintTiming.PredictLCPResult2";
const char kHistogramLCPPPredictSuccessLCPTiming[] = HISTOGRAM_PREFIX
    "PaintTiming.PredictSuccess.NavigationToLargestContentfulPaint";
const char kHistogramLCPPPredictHitIndex[] =
    HISTOGRAM_PREFIX "PaintTiming.PredictHitIndex2";
const char kHistogramLCPPActualLCPIndex[] =
    HISTOGRAM_PREFIX "PaintTiming.ActualLCPIndex2";
const char kHistogramLCPPActualLCPIsImage[] =
    HISTOGRAM_PREFIX "PaintTiming.ActualLCPIsImage";
const char kHistogramLCPPSubresourceCountPrecision[] =
    HISTOGRAM_PREFIX "Subresource.Count.Precision";
const char kHistogramLCPPSubresourceCountRecall[] =
    HISTOGRAM_PREFIX "Subresource.Count.Recall";
const char kHistogramLCPPSubresourceCountSameSiteRatio[] =
    HISTOGRAM_PREFIX "Subresource.Count.SameSiteRatio";
const char kHistogramLCPPSubresourceCountType[] =
    HISTOGRAM_PREFIX "Subresource.Count.Type";

const char kHistogramLCPPImageLoadingPriorityFrequencyOfActualPositive[] =
    HISTOGRAM_PREFIX "ImageLoadingPriority.FrequencyOfActualPositive.2";
const char kHistogramLCPPImageLoadingPriorityFrequencyOfActualNegative[] =
    HISTOGRAM_PREFIX "ImageLoadingPriority.FrequencyOfActualNegative.2";
const char kHistogramLCPPImageLoadingPriorityConfidenceOfActualPositive[] =
    HISTOGRAM_PREFIX "ImageLoadingPriority.ConfidenceOfActualPositive.2";
const char kHistogramLCPPImageLoadingPriorityConfidenceOfActualNegative[] =
    HISTOGRAM_PREFIX "ImageLoadingPriority.ConfidenceOfActualNegative.2";

const char kHistogramLCPPSubresourceFrequencyOfActualPositive[] =
    HISTOGRAM_PREFIX "Subresource.FrequencyOfActualPositive.2";
const char kHistogramLCPPSubresourceFrequencyOfActualNegative[] =
    HISTOGRAM_PREFIX "Subresource.FrequencyOfActualNegative.2";
const char kHistogramLCPPSubresourceConfidenceOfActualPositive[] =
    HISTOGRAM_PREFIX "Subresource.ConfidenceOfActualPositive.2";
const char kHistogramLCPPSubresourceConfidenceOfActualNegative[] =
    HISTOGRAM_PREFIX "Subresource.ConfidenceOfActualNegative.2";
const char kHistogramLCPPSubresourceFrequencyOfActualPositiveSameSite[] =
    HISTOGRAM_PREFIX "Subresource.FrequencyOfActualPositive.SameSite.2";
const char kHistogramLCPPSubresourceFrequencyOfActualNegativeSameSite[] =
    HISTOGRAM_PREFIX "Subresource.FrequencyOfActualNegative.SameSite.2";
const char kHistogramLCPPSubresourceConfidenceOfActualPositiveSameSite[] =
    HISTOGRAM_PREFIX "Subresource.ConfidenceOfActualPositive.SameSite.2";
const char kHistogramLCPPSubresourceConfidenceOfActualNegativeSameSite[] =
    HISTOGRAM_PREFIX "Subresource.ConfidenceOfActualNegative.SameSite.2";
const char kHistogramLCPPSubresourceFrequencyOfActualPositiveCrossSite[] =
    HISTOGRAM_PREFIX "Subresource.FrequencyOfActualPositive.CrossSite.2";
const char kHistogramLCPPSubresourceFrequencyOfActualNegativeCrossSite[] =
    HISTOGRAM_PREFIX "Subresource.FrequencyOfActualNegative.CrossSite.2";
const char kHistogramLCPPSubresourceConfidenceOfActualPositiveCrossSite[] =
    HISTOGRAM_PREFIX "Subresource.ConfidenceOfActualPositive.CrossSite.2";
const char kHistogramLCPPSubresourceConfidenceOfActualNegativeCrossSite[] =
    HISTOGRAM_PREFIX "Subresource.ConfidenceOfActualNegative.CrossSite.2";

}  // namespace internal

namespace {

size_t GetLCPPFontURLPredictorMaxUrlCountPerOrigin() {
  return blink::features::kLCPPFontURLPredictorMaxUrlCountPerOrigin.Get();
}

void RemoveFetchedSubresourceUrlsAfterLCP(
    std::map<GURL,
             std::pair<base::TimeDelta, network::mojom::RequestDestination>>&
        fetched_subresource_urls,
    const base::TimeDelta& lcp) {
  // Remove subresource that came after LCP because such subresource
  // wouldn't affect LCP.
  std::erase_if(fetched_subresource_urls, [&](const auto& url_and_time_type) {
    return url_and_time_type.second.first > lcp;
  });
}

bool IsSameSite(const GURL& url1, const GURL& url2) {
  return url1.SchemeIs(url2.GetScheme()) &&
         net::registry_controlled_domains::SameDomainOrHost(
             url1, url2,
             net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
}

void ReportSubresourceUMA(
    const GURL& commit_url,
    const std::optional<predictors::LcppStat>& lcpp_stat_prelearn,
    const predictors::LcppDataInputs& lcpp_data_inputs) {
  std::set<GURL> predicted_subresource_urls;
  if (lcpp_stat_prelearn) {
    for (const GURL& url : PredictFetchedSubresourceUrls(*lcpp_stat_prelearn)) {
      predicted_subresource_urls.insert(url);
    }
  }

  int predict_hit_count = 0;
  int same_site_count = 0;
  for (const auto& it : lcpp_data_inputs.subresource_urls) {
    const GURL& actual_subresource_url = it.first;
    if (predicted_subresource_urls.contains(actual_subresource_url)) {
      predict_hit_count++;
    }
    if (IsSameSite(commit_url, actual_subresource_url)) {
      same_site_count++;
    }
    base::UmaHistogramEnumeration(internal::kHistogramLCPPSubresourceCountType,
                                  it.second.second);
  }

  if (!predicted_subresource_urls.empty()) {
    const int precision = base::saturated_cast<int>(
        predict_hit_count * 100 / predicted_subresource_urls.size());
    base::UmaHistogramPercentage(
        internal::kHistogramLCPPSubresourceCountPrecision, precision);
  }

  if (!lcpp_data_inputs.subresource_urls.empty()) {
    const size_t actual_url_count = lcpp_data_inputs.subresource_urls.size();
    const int recall =
        base::saturated_cast<int>(predict_hit_count * 100 / actual_url_count);
    base::UmaHistogramPercentage(internal::kHistogramLCPPSubresourceCountRecall,
                                 recall);

    const int same_site_ratio =
        base::saturated_cast<int>(same_site_count * 100 / actual_url_count);
    base::UmaHistogramPercentage(
        internal::kHistogramLCPPSubresourceCountSameSiteRatio, same_site_ratio);
  }
}

const char* ConvertConfidenceToSuffix(double confidence) {
  switch (static_cast<int>(10 * confidence)) {
    case 0:
      return "0To10";
    case 1:
      return "10To20";
    case 2:
      return "20To30";
    case 3:
      return "30To40";
    case 4:
      return "40To50";
    case 5:
      return "50To60";
    case 6:
      return "60To70";
    case 7:
      return "70To80";
    case 8:
      return "80To90";
    case 9:
    case 10:
      return "90To100";
    default:
      NOTREACHED();
  }
}

int NormalizeConfidence(double confidence) {
  return static_cast<int>(std::floor(std::clamp(10.0 * confidence, 0.0, 9.9)));
}

int NormalizeTotalFrequency(double total_frequency) {
  return static_cast<int>(
      std::floor(std::clamp(total_frequency / 10.0, 0.0, 9.9)));
}

int CalculateScoreFromConfidenceAndTotalFrequency(double confidence,
                                                  double total_frequency) {
  int normalized_confidence_0_to_9 = NormalizeConfidence(confidence);
  int normalized_total_frequency_0_to_9 =
      NormalizeTotalFrequency(total_frequency);
  return 10 * normalized_confidence_0_to_9 + normalized_total_frequency_0_to_9;
}

int CalculateScoreFromTotalFrequencyAndConfidence(double confidence,
                                                  double total_frequency) {
  int normalized_confidence_0_to_9 = NormalizeConfidence(confidence);
  int normalized_total_frequency_0_to_9 =
      NormalizeTotalFrequency(total_frequency);
  return 10 * normalized_total_frequency_0_to_9 + normalized_confidence_0_to_9;
}

void MaybeReportConfidenceUMAs(
    const GURL& commit_url,
    const std::optional<predictors::LcppStat>& lcpp_stat_prelearn,
    const predictors::LcppDataInputs& lcpp_data_inputs) {
  // Even when the LCPP database doesn't provide any data, we want to record
  // metrics.
  const predictors::LcppStat& prelearn =
      lcpp_stat_prelearn ? *lcpp_stat_prelearn : predictors::LcppStat();

  const auto& locator =
      GetLcpElementLocatorForCriticalPathPredictor(lcpp_data_inputs);
  const std::string& actual_lcp_element_locator =
      locator ? *locator : std::string();
  if (!actual_lcp_element_locator.empty()) {
    const auto record_frequency_of_actual_positives = [](double frequency) {
      // The maximum count is defined by
      // `kLCPCriticalPathPredictorHistogramSlidingWindowSize`. The default
      // value is 1000.
      base::UmaHistogramCounts1000(
          internal::kHistogramLCPPImageLoadingPriorityFrequencyOfActualPositive,
          frequency);
    };

    const auto record_frequency_of_actual_negatives = [](double frequency) {
      base::UmaHistogramCounts1000(
          internal::kHistogramLCPPImageLoadingPriorityFrequencyOfActualNegative,
          frequency);
    };

    const auto record_confidence_of_actual_positives = [](double confidence) {
      base::UmaHistogramPercentage(
          internal::
              kHistogramLCPPImageLoadingPriorityConfidenceOfActualPositive,
          100 * confidence);
    };

    const auto record_confidence_of_actual_negatives = [](double confidence) {
      base::UmaHistogramPercentage(
          internal::
              kHistogramLCPPImageLoadingPriorityConfidenceOfActualNegative,
          100 * confidence);
    };

    const auto record_total_frequency_of_actual_positives =
        [](double confidence, double total_frequency) {
          base::UmaHistogramCounts1000(
              base::StrCat({HISTOGRAM_PREFIX "ImageLoadingPriority"
                                             ".TotalFrequencyOfActualPositive"
                                             ".WithConfidence",
                            ConvertConfidenceToSuffix(confidence), ".2"}),
              total_frequency);
          base::UmaHistogramPercentage(
              HISTOGRAM_PREFIX
              "ImageLoadingPriority"
              ".TotalFrequencyOfActualPositive"
              ".PerConfidence"
              ".3",
              CalculateScoreFromConfidenceAndTotalFrequency(confidence,
                                                            total_frequency));
          base::UmaHistogramPercentage(
              HISTOGRAM_PREFIX
              "ImageLoadingPriority"
              ".ConfidenceOfActualPositive"
              ".PerTotalFrequency"
              ".2",
              CalculateScoreFromTotalFrequencyAndConfidence(confidence,
                                                            total_frequency));
        };

    const auto record_total_frequency_of_actual_negatives =
        [](double confidence, double total_frequency) {
          base::UmaHistogramCounts1000(
              base::StrCat({HISTOGRAM_PREFIX "ImageLoadingPriority"
                                             ".TotalFrequencyOfActualNegative"
                                             ".WithConfidence",
                            ConvertConfidenceToSuffix(confidence), ".2"}),
              total_frequency);
          base::UmaHistogramPercentage(
              HISTOGRAM_PREFIX
              "ImageLoadingPriority"
              ".TotalFrequencyOfActualNegative"
              ".PerConfidence"
              ".3",
              CalculateScoreFromConfidenceAndTotalFrequency(confidence,
                                                            total_frequency));
          base::UmaHistogramPercentage(
              HISTOGRAM_PREFIX
              "ImageLoadingPriority"
              ".ConfidenceOfActualNegative"
              ".PerTotalFrequency"
              ".2",
              CalculateScoreFromTotalFrequencyAndConfidence(confidence,
                                                            total_frequency));
        };

    double total_frequency =
        prelearn.lcp_element_locator_stat().other_bucket_frequency();
    bool actual_positive_was_predicted = false;
    for (const auto& bucket :
         prelearn.lcp_element_locator_stat().lcp_element_locator_buckets()) {
      total_frequency += bucket.frequency();
      if (bucket.lcp_element_locator() == actual_lcp_element_locator) {
        actual_positive_was_predicted = true;
        record_frequency_of_actual_positives(bucket.frequency());
      } else {
        record_frequency_of_actual_negatives(bucket.frequency());
      }
    }

    for (auto& [confidence, lcp_element_locator] :
         predictors::ConvertLcpElementLocatorStatToConfidenceStringPairs(
             prelearn.lcp_element_locator_stat())) {
      if (lcp_element_locator == actual_lcp_element_locator) {
        record_confidence_of_actual_positives(confidence);
        record_total_frequency_of_actual_positives(confidence, total_frequency);
      } else {
        record_confidence_of_actual_negatives(confidence);
        record_total_frequency_of_actual_negatives(confidence, total_frequency);
      }
    }

    if (!actual_positive_was_predicted) {
      // If the actual-positive sample was not recorded, it means that the model
      // didn't have any knowledge about it, hence we record it as 0 frequency
      // and 0 confidence.
      record_frequency_of_actual_positives(/*frequency=*/0.0);
      record_confidence_of_actual_positives(/*confidence=*/0.0);
      record_total_frequency_of_actual_positives(/*confidence=*/0.0,
                                                 total_frequency);
    }
  }

  // At this moment, we know the ground truth data `lcpp_data_inputs` that came
  // from the recent actual navigation result. We also know the
  // {confidence, frequency} values from the intermediate result of prediction
  // that comes from `prelearn`. From these information, we can obtain
  // the following information.
  //
  // - Actual positive samples. These are the same as `actual_urls`. Actual
  //   positive samples contain true positives and false negatives.
  // - Actual negative samples. These are the urls that were predicted by our
  //   model, but were not loaded. Actual negative samples contain false
  //   positives.
  //
  // Unfortunately, in our case, we don't have true negatives.
  {
    const auto record_frequency_of_actual_positives = [](double frequency,
                                                         bool is_same_site) {
      // The maximum count is defined by
      // `kLCPCriticalPathPredictorHistogramSlidingWindowSize`. The default
      // value is 1000.
      base::UmaHistogramCounts1000(
          internal::kHistogramLCPPSubresourceFrequencyOfActualPositive,
          frequency);
      if (is_same_site) {
        base::UmaHistogramCounts1000(
            internal::
                kHistogramLCPPSubresourceFrequencyOfActualPositiveSameSite,
            frequency);
      } else {
        base::UmaHistogramCounts1000(
            internal::
                kHistogramLCPPSubresourceFrequencyOfActualPositiveCrossSite,
            frequency);
      }
    };

    const auto record_frequency_of_actual_negatives = [](double frequency,
                                                         bool is_same_site) {
      base::UmaHistogramCounts1000(
          internal::kHistogramLCPPSubresourceFrequencyOfActualNegative,
          frequency);
      if (is_same_site) {
        base::UmaHistogramCounts1000(
            internal::
                kHistogramLCPPSubresourceFrequencyOfActualNegativeSameSite,
            frequency);
      } else {
        base::UmaHistogramCounts1000(
            internal::
                kHistogramLCPPSubresourceFrequencyOfActualNegativeCrossSite,
            frequency);
      }
    };

    const auto record_confidence_of_actual_positives = [](double confidence,
                                                          bool is_same_site) {
      base::UmaHistogramPercentage(
          internal::kHistogramLCPPSubresourceConfidenceOfActualPositive,
          100.0 * confidence);
      if (is_same_site) {
        base::UmaHistogramPercentage(
            internal::
                kHistogramLCPPSubresourceConfidenceOfActualPositiveSameSite,
            100.0 * confidence);
      } else {
        base::UmaHistogramPercentage(
            internal::
                kHistogramLCPPSubresourceConfidenceOfActualPositiveCrossSite,
            100.0 * confidence);
      }
    };

    const auto record_confidence_of_actual_negatives = [](double confidence,
                                                          bool is_same_site) {
      base::UmaHistogramPercentage(
          internal::kHistogramLCPPSubresourceConfidenceOfActualNegative,
          100.0 * confidence);
      if (is_same_site) {
        base::UmaHistogramPercentage(
            internal::
                kHistogramLCPPSubresourceConfidenceOfActualNegativeSameSite,
            100.0 * confidence);
      } else {
        base::UmaHistogramPercentage(
            internal::
                kHistogramLCPPSubresourceConfidenceOfActualNegativeCrossSite,
            100.0 * confidence);
      }
    };

    const auto record_total_frequency_of_actual_positives =
        [](double confidence, double total_frequency, bool is_same_site) {
          const char* same_site_or_cross_site =
              is_same_site ? ".SameSite" : ".CrossSite";
          base::UmaHistogramCounts1000(
              base::StrCat({HISTOGRAM_PREFIX "Subresource"
                                             ".TotalFrequencyOfActualPositive"
                                             ".WithConfidence",
                            ConvertConfidenceToSuffix(confidence), ".2"}),
              total_frequency);
          base::UmaHistogramCounts1000(
              base::StrCat({HISTOGRAM_PREFIX "Subresource"
                                             ".TotalFrequencyOfActualPositive"
                                             ".WithConfidence",
                            ConvertConfidenceToSuffix(confidence),
                            same_site_or_cross_site, ".2"}),
              total_frequency);
          base::UmaHistogramPercentage(
              HISTOGRAM_PREFIX
              "Subresource"
              ".TotalFrequencyOfActualPositive"
              ".PerConfidence"
              ".3",
              CalculateScoreFromConfidenceAndTotalFrequency(confidence,
                                                            total_frequency));
          base::UmaHistogramPercentage(
              base::StrCat({HISTOGRAM_PREFIX "Subresource"
                                             ".TotalFrequencyOfActualPositive"
                                             ".PerConfidence",
                            same_site_or_cross_site, ".3"}),
              CalculateScoreFromConfidenceAndTotalFrequency(confidence,
                                                            total_frequency));
          base::UmaHistogramPercentage(
              HISTOGRAM_PREFIX
              "Subresource"
              ".ConfidenceOfActualPositive"
              ".PerTotalFrequency"
              ".3",
              CalculateScoreFromTotalFrequencyAndConfidence(confidence,
                                                            total_frequency));
          base::UmaHistogramPercentage(
              base::StrCat({HISTOGRAM_PREFIX "Subresource"
                                             ".ConfidenceOfActualPositive"
                                             ".PerTotalFrequency",
                            same_site_or_cross_site, ".3"}),
              CalculateScoreFromTotalFrequencyAndConfidence(confidence,
                                                            total_frequency));
        };

    const auto record_total_frequency_of_actual_negatives =
        [](double confidence, double total_frequency, bool is_same_site) {
          const char* same_site_or_cross_site =
              is_same_site ? ".SameSite" : ".CrossSite";
          base::UmaHistogramCounts1000(
              base::StrCat({HISTOGRAM_PREFIX "Subresource"
                                             ".TotalFrequencyOfActualNegative"
                                             ".WithConfidence",
                            ConvertConfidenceToSuffix(confidence), ".2"}),
              total_frequency);
          base::UmaHistogramCounts1000(
              base::StrCat({
                  HISTOGRAM_PREFIX "Subresource"
                                   ".TotalFrequencyOfActualNegative"
                                   ".WithConfidence",
                  ConvertConfidenceToSuffix(confidence),
                  same_site_or_cross_site,
                  ".2",
              }),
              total_frequency);
          base::UmaHistogramPercentage(
              HISTOGRAM_PREFIX
              "Subresource"
              ".TotalFrequencyOfActualNegative"
              ".PerConfidence"
              ".3",
              CalculateScoreFromConfidenceAndTotalFrequency(confidence,
                                                            total_frequency));
          base::UmaHistogramPercentage(
              base::StrCat({HISTOGRAM_PREFIX "Subresource"
                                             ".TotalFrequencyOfActualNegative"
                                             ".PerConfidence",
                            same_site_or_cross_site, ".3"}),
              CalculateScoreFromConfidenceAndTotalFrequency(confidence,
                                                            total_frequency));
          base::UmaHistogramPercentage(
              HISTOGRAM_PREFIX
              "Subresource"
              ".ConfidenceOfActualNegative"
              ".PerTotalFrequency"
              ".3",
              CalculateScoreFromTotalFrequencyAndConfidence(confidence,
                                                            total_frequency));
          base::UmaHistogramPercentage(
              base::StrCat({HISTOGRAM_PREFIX "Subresource"
                                             ".ConfidenceOfActualNegative"
                                             ".PerTotalFrequency",
                            same_site_or_cross_site, ".3"}),
              CalculateScoreFromTotalFrequencyAndConfidence(confidence,
                                                            total_frequency));
        };

    const auto& actual_urls = lcpp_data_inputs.subresource_urls;
    double total_frequency =
        prelearn.fetched_subresource_url_stat().other_bucket_frequency();
    std::set<GURL> predicted_urls;
    for (const auto& [predicted_url_string, frequency] :
         prelearn.fetched_subresource_url_stat().main_buckets()) {
      total_frequency += frequency;
      const GURL predicted_url(predicted_url_string);
      predicted_urls.insert(predicted_url);
      bool is_same_site = IsSameSite(commit_url, predicted_url);
      if (actual_urls.contains(predicted_url)) {
        record_frequency_of_actual_positives(frequency, is_same_site);
      } else {
        record_frequency_of_actual_negatives(frequency, is_same_site);
      }
    }

    for (auto& [confidence, predicted_url_string] :
         predictors::ConvertLcppStringFrequencyStatDataToConfidenceStringPairs(
             prelearn.fetched_subresource_url_stat())) {
      GURL predicted_url(predicted_url_string);
      bool is_same_site = IsSameSite(commit_url, predicted_url);
      if (actual_urls.contains(GURL(predicted_url))) {
        record_confidence_of_actual_positives(confidence, is_same_site);
        record_total_frequency_of_actual_positives(confidence, total_frequency,
                                                   is_same_site);
      } else {
        record_confidence_of_actual_negatives(confidence, is_same_site);
        record_total_frequency_of_actual_negatives(confidence, total_frequency,
                                                   is_same_site);
      }
    }

    // The following code records the URLs that were not predicted but actually
    // loaded. These samples must be included as part of the actual positives.
    for (const auto& it : actual_urls) {
      const GURL& actual_url = it.first;
      if (predicted_urls.contains(actual_url)) {
        continue;
      }
      bool is_same_site = IsSameSite(commit_url, actual_url);
      // There was no data for this actually loaded URL in the LCPP database,
      // hence the frequency and confidence is 0.
      record_frequency_of_actual_positives(/*frequency=*/0.0, is_same_site);
      record_confidence_of_actual_positives(/*confidence=*/0.0, is_same_site);
      record_total_frequency_of_actual_positives(/*confidence=*/0.0,
                                                 total_frequency, is_same_site);
    }
  }
}

}  // namespace

namespace internal {

void MaybeReportConfidenceUMAsForTesting(  // IN-TEST
    const GURL& commit_url,
    const std::optional<predictors::LcppStat>& lcpp_stat_prelearn,
    const predictors::LcppDataInputs& lcpp_data_inputs) {
  MaybeReportConfidenceUMAs(commit_url, lcpp_stat_prelearn, lcpp_data_inputs);
}

}  // namespace internal

PAGE_USER_DATA_KEY_IMPL(
    LcpCriticalPathPredictorPageLoadMetricsObserver::PageData);

LcpCriticalPathPredictorPageLoadMetricsObserver::PageData::PageData(
    content::Page& page)
    : content::PageUserData<PageData>(page) {}

LcpCriticalPathPredictorPageLoadMetricsObserver::PageData::~PageData() =
    default;

LcpCriticalPathPredictorPageLoadMetricsObserver::
    LcpCriticalPathPredictorPageLoadMetricsObserver() = default;

LcpCriticalPathPredictorPageLoadMetricsObserver::
    ~LcpCriticalPathPredictorPageLoadMetricsObserver() = default;

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
LcpCriticalPathPredictorPageLoadMetricsObserver::OnStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url,
    bool started_in_foreground) {
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
LcpCriticalPathPredictorPageLoadMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle) {
  const blink::mojom::LCPCriticalPathPredictorNavigationTimeHintPtr& hint =
      navigation_handle->GetLCPPNavigationHint();
  if (hint) {
    if (!hint->lcp_element_locators.empty() ||
        !hint->lcp_element_locators_all.empty() ||
        !hint->lcp_influencer_scripts.empty() ||
        !hint->preconnect_origins.empty()) {
      is_lcpp_hinted_navigation_ = true;
    }
    if (hint->for_testing) {
      CHECK_IS_TEST();
      is_testing_ = true;
    }
  }

  initiator_origin_ = navigation_handle->GetInitiatorOrigin();
  commit_url_ = navigation_handle->GetURL();
  if (!predictors::IsURLValidForLcpp(*commit_url_)) {
    return STOP_OBSERVING;
  }
  LcpCriticalPathPredictorPageLoadMetricsObserver::PageData::GetOrCreateForPage(
      GetDelegate().GetWebContents()->GetPrimaryPage())
      ->SetLcpCriticalPathPredictorPageLoadMetricsObserver(
          weak_factory_.GetWeakPtr());
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
LcpCriticalPathPredictorPageLoadMetricsObserver::OnFencedFramesStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
LcpCriticalPathPredictorPageLoadMetricsObserver::OnPrerenderStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  is_prerender_ = true;
  return CONTINUE_OBSERVING;
}

void LcpCriticalPathPredictorPageLoadMetricsObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  FinalizeLCP();
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
LcpCriticalPathPredictorPageLoadMetricsObserver::
    FlushMetricsOnAppEnterBackground(
        const page_load_metrics::mojom::PageLoadTiming& timing) {
  // This follows UmaPageLoadMetricsObserver.
  if (GetDelegate().DidCommit()) {
    FinalizeLCP();
  }
  return STOP_OBSERVING;
}

predictors::ResourcePrefetchPredictor*
LcpCriticalPathPredictorPageLoadMetricsObserver::GetPredictor() {
  // `loading_predictor` is nullptr in
  // `LcpCriticalPathPredictorPageLoadMetricsObserverTest`, or if the profile
  // `IsOffTheRecord`.
  if (auto* loading_predictor =
          predictors::LoadingPredictorFactory::GetForProfile(
              Profile::FromBrowserContext(
                  GetDelegate().GetWebContents()->GetBrowserContext()))) {
    return loading_predictor->resource_prefetch_predictor();
  }
  return nullptr;
}

void LcpCriticalPathPredictorPageLoadMetricsObserver::FinalizeLCP() {
  if (!commit_url_) {
    return;
  }

  const page_load_metrics::ContentfulPaintTimingInfo& largest_contentful_paint =
      GetDelegate()
          .GetLargestContentfulPaintHandler()
          .MergeMainFrameAndSubframes();

  if (!largest_contentful_paint.ContainsValidTime() ||
      (!is_prerender_ && !WasStartedInForegroundOptionalEventInForeground(
                             largest_contentful_paint.Time(), GetDelegate()))) {
    return;
  }

  // * Finalize the staged LCPP signals to the database.
  predictors::ResourcePrefetchPredictor* predictor = GetPredictor();
  // Take the learned LCPP here so that we can report it after overwriting it
  // with the new data below.
  std::optional<predictors::LcppStat> lcpp_stat_prelearn =
      predictor ? predictor->GetLcppStat(initiator_origin_, *commit_url_)
                : std::nullopt;

  if (lcpp_data_inputs_.has_value()
      // Don't learn LCPP when prerender to avoid data skew. Activation LCP
      // should be much shorter than regular LCP.
      && !is_prerender_ && predictor) {
    RemoveFetchedSubresourceUrlsAfterLCP(
        lcpp_data_inputs_->subresource_urls,
        largest_contentful_paint.Time().value());
    ReportSubresourceUMA(*commit_url_, lcpp_stat_prelearn, *lcpp_data_inputs_);
    MaybeReportConfidenceUMAs(*commit_url_, lcpp_stat_prelearn,
                              *lcpp_data_inputs_);
    base::UmaHistogramCounts10000("Blink.LCPP.PreconnectCount",
                                  lcpp_data_inputs_->preconnect_origins.size());
    predictor->LearnLcpp(initiator_origin_, *commit_url_, *lcpp_data_inputs_);
  }

  // * Emit LCPP breakdown PageLoad UMAs.
  // The UMAs are recorded iff the navigation was made with a non-empty LCPP
  // hint
  if (is_lcpp_hinted_navigation_ &&
      (!is_prerender_ ||
       GetDelegate().WasPrerenderedThenActivatedInForeground())) {
    base::TimeDelta corrected =
        page_load_metrics::CorrectEventAsNavigationOrActivationOrigined(
            GetDelegate(), largest_contentful_paint.Time().value());
    PAGE_LOAD_HISTOGRAM(internal::kHistogramLCPPLargestContentfulPaint,
                        corrected);
    ReportUMAForTimingPredictor(std::move(lcpp_stat_prelearn), corrected);
    if (is_lcp_element_image_) {
      base::UmaHistogramBoolean(internal::kHistogramLCPPActualLCPIsImage,
                                *is_lcp_element_image_);
    }
  }
}

void LcpCriticalPathPredictorPageLoadMetricsObserver::
    OnFirstContentfulPaintInPage(
        const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (!is_lcpp_hinted_navigation_) {
    return;
  }

  base::TimeDelta corrected =
      page_load_metrics::CorrectEventAsNavigationOrActivationOrigined(
          GetDelegate(), timing.paint_timing->first_contentful_paint.value());
  PAGE_LOAD_HISTOGRAM(internal::kHistogramLCPPFirstContentfulPaint, corrected);
}

void LcpCriticalPathPredictorPageLoadMetricsObserver::OnLcpUpdated(
    blink::mojom::LcpElementPtr lcp_element) {
  if (lcp_element->locator) {
    if (!lcpp_data_inputs_) {
      lcpp_data_inputs_.emplace();
    }
    lcpp_data_inputs_->lcp_element_locator = *lcp_element->locator;
    if (lcp_element->is_image) {
      lcpp_data_inputs_->lcp_element_locator_image = *lcp_element->locator;
    }
  }
  is_lcp_element_image_ = lcp_element->is_image;
  predicted_lcp_indexes_.push_back(lcp_element->predicted_index);

  if (is_testing_) {
    CHECK_IS_TEST();
    GetPredictor()->OnLcpUpdatedForTesting(lcp_element->locator);
  }
}

void LcpCriticalPathPredictorPageLoadMetricsObserver::
    OnLcpTimingPredictedForTesting(
        const std::optional<std::string>& element_locator) {
  CHECK_IS_TEST();
  GetPredictor()->OnLcpTimingPredictedForTesting(element_locator);
}

void LcpCriticalPathPredictorPageLoadMetricsObserver::AppendFetchedFontUrl(
    const GURL& font_url,
    bool hit) {
  if (!lcpp_data_inputs_) {
    lcpp_data_inputs_.emplace();
  }
  ++lcpp_data_inputs_->font_url_count;
  if (hit) {
    ++lcpp_data_inputs_->font_url_hit_count;
  }

  if (commit_url_ && IsSameSite(font_url, *commit_url_)) {
    ++lcpp_data_inputs_->same_site_font_url_count;
  } else {
    ++lcpp_data_inputs_->cross_site_font_url_count;
    if (!blink::features::kLCPPCrossSiteFontPredictionAllowed.Get()) {
      return;
    }
  }
  if (lcpp_data_inputs_->font_urls.size() >=
      GetLCPPFontURLPredictorMaxUrlCountPerOrigin()) {
    return;
  }
  if (hit) {
    ++lcpp_data_inputs_->font_url_reenter_count;
  }
  lcpp_data_inputs_->font_urls.push_back(font_url);
}

void LcpCriticalPathPredictorPageLoadMetricsObserver::
    AppendFetchedSubresourceUrl(
        const GURL& subresource_url,
        const base::TimeDelta& subresource_load_start,
        network::mojom::RequestDestination request_destination) {
  if (!lcpp_data_inputs_) {
    lcpp_data_inputs_.emplace();
  }
  if (lcpp_data_inputs_->subresource_urls.empty()) {
    base::UmaHistogramMediumTimes(
        "Blink.LCPP.NavigationToStartPreload.MainFrame.FirstSubresource.Time",
        subresource_load_start);
    const base::TimeTicks navigation_start = GetDelegate().GetNavigationStart();
    TRACE_EVENT_BEGIN("loading", "NavigationToStartFirstPreload",
                      perfetto::Track::FromPointer(this), navigation_start,
                      "url", subresource_url);
    TRACE_EVENT_END("loading", perfetto::Track::FromPointer(this),
                    navigation_start + subresource_load_start);
  }
  base::UmaHistogramMediumTimes(
      "Blink.LCPP.NavigationToStartPreload.MainFrame.EachSubresource.Time",
      subresource_load_start);
  if (!lcpp_data_inputs_->subresource_urls.contains(subresource_url)) {
    lcpp_data_inputs_->subresource_urls.emplace(
        subresource_url,
        std::make_pair(subresource_load_start, request_destination));
  }
}

void LcpCriticalPathPredictorPageLoadMetricsObserver::
    SetLcpInfluencerScriptUrls(
        const std::vector<GURL>& lcp_influencer_scripts) {
  if (!lcpp_data_inputs_) {
    lcpp_data_inputs_.emplace();
  }
  lcpp_data_inputs_->lcp_influencer_scripts = lcp_influencer_scripts;
}

void LcpCriticalPathPredictorPageLoadMetricsObserver::AddPreconnectOrigin(
    const url::Origin& origin) {
  if (!lcpp_data_inputs_) {
    lcpp_data_inputs_.emplace();
  }

  std::set<url::Origin>& preconnect_origins =
      lcpp_data_inputs_->preconnect_origins;
  if (blink::features::kLCPPAutoPreconnectRecordAllOrigins.Get()) {
    preconnect_origins.insert(origin);
  } else {
    preconnect_origins.clear();
    preconnect_origins.insert(origin);
  }
}

void LcpCriticalPathPredictorPageLoadMetricsObserver::SetUnusedPreloads(
    const std::vector<GURL>& unused_preloads) {
  if (!lcpp_data_inputs_) {
    lcpp_data_inputs_.emplace();
  }
  lcpp_data_inputs_->unused_preload_resources = unused_preloads;
}

void LcpCriticalPathPredictorPageLoadMetricsObserver::
    ReportUMAForTimingPredictor(
        std::optional<predictors::LcppStat> lcpp_stat_prelearn,
        base::TimeDelta lcp_timing) {
  if (!lcpp_data_inputs_.has_value() || !commit_url_ || !lcpp_stat_prelearn ||
      !IsValidLcppStat(*lcpp_stat_prelearn)) {
    return;
  }
  std::optional<blink::mojom::LCPCriticalPathPredictorNavigationTimeHint> hint =
      ConvertLcppStatToLCPCriticalPathPredictorNavigationTimeHint(
          *lcpp_stat_prelearn);
  if (!hint || !hint->lcp_element_locators.size()) {
    return;
  }

  if (predicted_lcp_indexes_.empty()) {
    return;
  }
  // Then, We have a prelearn data and at least one LCP locator in current
  // load. Let's stat it.

  // This value existence indicates failure because predicted LCP should be the
  // last.
  std::optional<uint32_t> first_valid_index_except_last = std::nullopt;
  for (size_t i = 0; i < predicted_lcp_indexes_.size() - 1; i++) {
    const std::optional<uint32_t>& maybe_index = predicted_lcp_indexes_[i];
    if (maybe_index) {
      first_valid_index_except_last = *maybe_index;
      break;
    }
  }
  const std::optional<uint32_t>& last_lcp_index = predicted_lcp_indexes_.back();

  internal::LCPPPredictResult result;
  const int max_lcpp_histogram_buckets =
      blink::features::kLCPCriticalPathPredictorMaxHistogramBuckets.Get() +
      internal::kLCPIndexHistogramOffset;
  if (first_valid_index_except_last) {
    if (last_lcp_index) {
      if (*first_valid_index_except_last == *last_lcp_index) {
        // `predicted_lcp_indexes_` is like {1, 1}.
        result = internal::LCPPPredictResult::kFailureActuallySameButLaterLCP;
      } else {
        //  `predicted_lcp_indexes_` is like {1,2} or {1,1,2}.
        result = internal::LCPPPredictResult::kFailureActuallySecondaryLCP;
      }
    } else {
      // `predicted_lcp_indexes_` is like {1, null}.
      result = internal::LCPPPredictResult::kFailureActuallyUnrecordedLCP;
    }
  } else {
    if (last_lcp_index) {
      //  `predicted_lcp_indexes_` is like {null*, 1}.
      result = internal::LCPPPredictResult::kSuccess;
      base::UmaHistogramExactLinear(
          internal::kHistogramLCPPPredictHitIndex,
          *last_lcp_index + internal::kLCPIndexHistogramOffset,
          max_lcpp_histogram_buckets);
      PAGE_LOAD_HISTOGRAM(internal::kHistogramLCPPPredictSuccessLCPTiming,
                          lcp_timing);
    } else {
      // `predicted_lcp_indexes_` is like {null*}.
      result = internal::LCPPPredictResult::kFailureNoHit;
    }
  }

  base::UmaHistogramEnumeration(internal::kHistogramLCPPPredictResult, result);
  base::UmaHistogramExactLinear(
      internal::kHistogramLCPPActualLCPIndex,
      last_lcp_index ? *last_lcp_index + internal::kLCPIndexHistogramOffset
                     : max_lcpp_histogram_buckets,
      max_lcpp_histogram_buckets);
}
