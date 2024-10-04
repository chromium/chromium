// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/lcp_critical_path_predictor_page_load_metrics_observer.h"

#include "base/trace_event/base_tracing.h"
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

namespace internal {

#define HISTOGRAM_PREFIX "PageLoad.Clients.LCPP."
const char kHistogramLCPPFirstContentfulPaint[] =
    HISTOGRAM_PREFIX "PaintTiming.NavigationToFirstContentfulPaint";
const char kHistogramLCPPLargestContentfulPaint[] =
    HISTOGRAM_PREFIX "PaintTiming.NavigationToLargestContentfulPaint";
const char kHistogramLCPPPredictResult[] =
    HISTOGRAM_PREFIX "PaintTiming.PredictLCPResult";
const char kHistogramLCPPPredictHitIndex[] =
    HISTOGRAM_PREFIX "PaintTiming.PredictHitIndex";
const char kHistogramLCPPActualLCPIndex[] =
    HISTOGRAM_PREFIX "PaintTiming.ActualLCPIndex";

}  // namespace internal

namespace {

size_t GetLCPPFontURLPredictorMaxUrlCountPerOrigin() {
  static size_t max_allowed_url_count = base::checked_cast<size_t>(
      blink::features::kLCPPFontURLPredictorMaxUrlCountPerOrigin.Get());
  return max_allowed_url_count;
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
  return url1.SchemeIs(url2.scheme()) &&
         net::registry_controlled_domains::SameDomainOrHost(
             url1, url2,
             net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
}

}  // namespace

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
  if (hint && (!hint->lcp_element_locators.empty() ||
               !hint->lcp_influencer_scripts.empty() ||
               !hint->preconnect_origins.empty())) {
    is_lcpp_hinted_navigation_ = true;
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
  predictors::ResourcePrefetchPredictor* predictor = nullptr;
  // `loading_predictor` is nullptr in
  // `LcpCriticalPathPredictorPageLoadMetricsObserverTest`, or if the profile
  // `IsOffTheRecord`.
  if (auto* loading_predictor =
          predictors::LoadingPredictorFactory::GetForProfile(
              Profile::FromBrowserContext(
                  GetDelegate().GetWebContents()->GetBrowserContext()))) {
    predictor = loading_predictor->resource_prefetch_predictor();
  }
  // Take the learned LCPP here so that we can report it after overwriting it
  // with the new data below.
  std::optional<predictors::LcppStat> lcpp_stat_prelearn =
      predictor ? predictor->GetLcppStat(initiator_origin_, *commit_url_)
                : std::nullopt;

  // TODO(crbug.com/40517495): kSpeculativePreconnectFeature flag can also
  // affect this. Unflag the feature.
  if (lcpp_data_inputs_.has_value()
      // Don't learn LCPP when prerender to avoid data skew. Activation LCP
      // should be much shorter than regular LCP.
      && !is_prerender_ && predictor) {
    RemoveFetchedSubresourceUrlsAfterLCP(
        lcpp_data_inputs_->subresource_urls,
        largest_contentful_paint.Time().value());
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
    ReportUMAForTimingPredictor(std::move(lcpp_stat_prelearn));
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

void LcpCriticalPathPredictorPageLoadMetricsObserver::SetLcpElementLocator(
    const std::string& lcp_element_locator,
    std::optional<uint32_t> predicted_lcp_index) {
  if (!lcpp_data_inputs_) {
    lcpp_data_inputs_.emplace();
  }
  lcpp_data_inputs_->lcp_element_locator = lcp_element_locator;
  predicted_lcp_indexes_.push_back(predicted_lcp_index);
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
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP1(
        "loading", "NavigationToStartFirstPreload", TRACE_ID_LOCAL(this),
        navigation_start, "url", subresource_url);
    TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP0(
        "loading", "NavigationToStartFirstPreload", TRACE_ID_LOCAL(this),
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

void LcpCriticalPathPredictorPageLoadMetricsObserver::SetPreconnectOrigins(
    const std::vector<GURL>& origins) {
  if (!lcpp_data_inputs_) {
    lcpp_data_inputs_.emplace();
  }
  lcpp_data_inputs_->preconnect_origins = origins;
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
        std::optional<predictors::LcppStat> lcpp_stat_prelearn) {
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
