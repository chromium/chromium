// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/lcp_critical_path_predictor_page_load_metrics_observer.h"

#include "chrome/browser/predictors/loading_predictor.h"
#include "chrome/browser/predictors/loading_predictor_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/features.h"

namespace internal {

#define HISTOGRAM_PREFIX "PageLoad.Clients.LCPP."
const char kHistogramLCPPFirstContentfulPaint[] =
    HISTOGRAM_PREFIX "PaintTiming.NavigationToFirstContentfulPaint";
const char kHistogramLCPPLargestContentfulPaint[] =
    HISTOGRAM_PREFIX "PaintTiming.NavigationToLargestContentfulPaint";
const char kHistogramLCPPPredictSuccess[] =
    HISTOGRAM_PREFIX "PaintTiming.PredictLCPSuccess";

}  // namespace internal

namespace {

size_t GetLCPPFontURLPredictorMaxUrlCountPerOrigin() {
  static size_t max_allowed_url_count = base::checked_cast<size_t>(
      blink::features::kLCPPFontURLPredictorMaxUrlCountPerOrigin.Get());
  return max_allowed_url_count;
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
               !hint->lcp_influencer_scripts.empty())) {
    is_lcpp_hinted_navigation_ = true;
  }

  commit_url_ = navigation_handle->GetURL();
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
  absl::optional<predictors::LcppData> lcpp_data_prelearn =
      predictor ? predictor->GetLcppData(*commit_url_) : absl::nullopt;

  // TODO(crbug.com/715525): kSpeculativePreconnectFeature flag can also affect
  // this. Unflag the feature.
  if (lcpp_data_inputs_.has_value()
      // Don't learn LCPP when prerender to avoid data skew. Activation LCP
      // should be much shorter than regular LCP.
      && !is_prerender_ && predictor) {
    predictor->LearnLcpp(commit_url_->host(), *lcpp_data_inputs_);
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
    ReportUMAForTimingPredictor(std::move(lcpp_data_prelearn));
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
    const std::string& lcp_element_locator) {
  if (!lcpp_data_inputs_) {
    lcpp_data_inputs_.emplace();
  }
  lcpp_data_inputs_->lcp_element_locator = lcp_element_locator;
}

void LcpCriticalPathPredictorPageLoadMetricsObserver::AppendFetchedFontUrl(
    const GURL& font_url) {
  if (!lcpp_data_inputs_) {
    lcpp_data_inputs_.emplace();
  }
  ++lcpp_data_inputs_->font_url_count;
  if (lcpp_data_inputs_->font_urls.size() >=
      GetLCPPFontURLPredictorMaxUrlCountPerOrigin()) {
    return;
  }
  lcpp_data_inputs_->font_urls.push_back(font_url);
}

void LcpCriticalPathPredictorPageLoadMetricsObserver::
    SetLcpInfluencerScriptUrls(
        const std::vector<GURL>& lcp_influencer_scripts) {
  if (!lcpp_data_inputs_) {
    lcpp_data_inputs_.emplace();
  }
  lcpp_data_inputs_->lcp_influencer_scripts = lcp_influencer_scripts;
}

void LcpCriticalPathPredictorPageLoadMetricsObserver::
    ReportUMAForTimingPredictor(
        absl::optional<predictors::LcppData> lcpp_data_prelearn) {
  if (!lcpp_data_inputs_.has_value() || !commit_url_ || !lcpp_data_prelearn ||
      !IsValidLcppStat(lcpp_data_prelearn->lcpp_stat())) {
    return;
  }
  absl::optional<blink::mojom::LCPCriticalPathPredictorNavigationTimeHint>
      hint = ConvertLcppDataToLCPCriticalPathPredictorNavigationTimeHint(
          *lcpp_data_prelearn);
  if (!hint || !hint->lcp_element_locators.size()) {
    return;
  }
  // Predicted the most frequent LCP would be next LCP and record the
  // actual result. see PredictLcpElementLocators() for the `hint` contents.
  const bool predicted =
      (hint->lcp_element_locators[0] == lcpp_data_inputs_->lcp_element_locator);
  base::UmaHistogramBoolean(internal::kHistogramLCPPPredictSuccess, predicted);
}
