// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/navigation_predictor/navigation_predictor.h"

#include <algorithm>
#include <memory>

#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/system/sys_info.h"
#include "chrome/browser/navigation_predictor/navigation_predictor_keyed_service.h"
#include "chrome/browser/navigation_predictor/navigation_predictor_keyed_service_factory.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"
#include "url/url_canon.h"

namespace {

// A feature to allow multiple prerenders. The feature itself is always enabled,
// but the params it exposes are variable.
const base::Feature kNavigationPredictorMultiplePrerenders{
    "NavigationPredictorMultiplePrerenders", base::FEATURE_ENABLED_BY_DEFAULT};

bool IsMainFrame(content::RenderFrameHost* rfh) {
  // Don't use rfh->GetRenderViewHost()->GetMainFrame() here because
  // RenderViewHost is being deprecated and because in OOPIF,
  // RenderViewHost::GetMainFrame() returns nullptr for child frames hosted in a
  // different process from the main frame.
  return rfh->GetParent() == nullptr;
}

std::string GetURLWithoutRefParams(const GURL& gurl) {
  url::Replacements<char> replacements;
  replacements.ClearRef();
  return gurl.ReplaceComponents(replacements).spec();
}

// Returns true if |a| and |b| are both valid HTTP/HTTPS URLs and have the
// same scheme, host, path and query params. This method does not take into
// account the ref params of the two URLs.
bool AreGURLsEqualExcludingRefParams(const GURL& a, const GURL& b) {
  return GetURLWithoutRefParams(a) == GetURLWithoutRefParams(b);
}
}  // namespace

struct NavigationPredictor::NavigationScore {
  NavigationScore(const GURL& url,
                  double ratio_area,
                  bool is_url_incremented_by_one,
                  size_t area_rank,
                  double score,
                  double ratio_distance_root_top,
                  bool contains_image,
                  bool is_in_iframe,
                  size_t index)
      : url(url),
        ratio_area(ratio_area),
        is_url_incremented_by_one(is_url_incremented_by_one),
        area_rank(area_rank),
        score(score),
        ratio_distance_root_top(ratio_distance_root_top),
        contains_image(contains_image),
        is_in_iframe(is_in_iframe),
        index(index) {}
  // URL of the target link.
  const GURL url;

  // The ratio between the absolute clickable region of an anchor element and
  // the document area. This should be in the range [0, 1].
  const double ratio_area;

  // Whether the url increments the current page's url by 1.
  const bool is_url_incremented_by_one;

  // Rank in terms of anchor element area. It starts at 0, a lower rank implies
  // a larger area. Capped at 100.
  const size_t area_rank;

  // Calculated navigation score, based on |area_rank| and other metrics.
  double score;

  // The distance from the top of the document to the anchor element, expressed
  // as a ratio with the length of the document.
  const double ratio_distance_root_top;

  // Multiple anchor elements may point to the same |url|. |contains_image| is
  // true if at least one of the anchor elements pointing to |url| contains an
  // image.
  const bool contains_image;

  // |is_in_iframe| is true if at least one of the anchor elements point to
  // |url| is in an iframe.
  const bool is_in_iframe;

  // An index reported to UKM.
  const size_t index;

  // Rank of the |score| in this document. It starts at 0, a lower rank implies
  // a higher |score|.
  base::Optional<size_t> score_rank;
};

NavigationPredictor::NavigationPredictor(
    content::RenderFrameHost* render_frame_host)
    : browser_context_(
          render_frame_host->GetSiteInstance()->GetBrowserContext()),
      ratio_area_scale_(base::GetFieldTrialParamByFeatureAsInt(
          blink::features::kNavigationPredictor,
          "ratio_area_scale",
          100)),
      is_in_iframe_scale_(base::GetFieldTrialParamByFeatureAsInt(
          blink::features::kNavigationPredictor,
          "is_in_iframe_scale",
          0)),
      is_same_host_scale_(base::GetFieldTrialParamByFeatureAsInt(
          blink::features::kNavigationPredictor,
          "is_same_host_scale",
          0)),
      contains_image_scale_(base::GetFieldTrialParamByFeatureAsInt(
          blink::features::kNavigationPredictor,
          "contains_image_scale",
          50)),
      is_url_incremented_scale_(base::GetFieldTrialParamByFeatureAsInt(
          blink::features::kNavigationPredictor,
          "is_url_incremented_scale",
          100)),
      area_rank_scale_(base::GetFieldTrialParamByFeatureAsInt(
          blink::features::kNavigationPredictor,
          "area_rank_scale",
          100)),
      ratio_distance_root_top_scale_(base::GetFieldTrialParamByFeatureAsInt(
          blink::features::kNavigationPredictor,
          "ratio_distance_root_top_scale",
          0)),
      link_total_scale_(base::GetFieldTrialParamByFeatureAsInt(
          blink::features::kNavigationPredictor,
          "link_total_scale",
          0)),
      iframe_link_total_scale_(base::GetFieldTrialParamByFeatureAsInt(
          blink::features::kNavigationPredictor,
          "iframe_link_total_scale",
          0)),
      increment_link_total_scale_(base::GetFieldTrialParamByFeatureAsInt(
          blink::features::kNavigationPredictor,
          "increment_link_total_scale",
          0)),
      same_origin_link_total_scale_(base::GetFieldTrialParamByFeatureAsInt(
          blink::features::kNavigationPredictor,
          "same_origin_link_total_scale",
          0)),
      image_link_total_scale_(base::GetFieldTrialParamByFeatureAsInt(
          blink::features::kNavigationPredictor,
          "image_link_total_scale",
          0)),
      clickable_space_scale_(base::GetFieldTrialParamByFeatureAsInt(
          blink::features::kNavigationPredictor,
          "clickable_space_scale",
          0)),
      median_link_location_scale_(base::GetFieldTrialParamByFeatureAsInt(
          blink::features::kNavigationPredictor,
          "median_link_location_scale",
          0)),
      viewport_height_scale_(base::GetFieldTrialParamByFeatureAsInt(
          blink::features::kNavigationPredictor,
          "viewport_height_scale",
          0)),
      viewport_width_scale_(base::GetFieldTrialParamByFeatureAsInt(
          blink::features::kNavigationPredictor,
          "viewport_width_scale",
          0)),
      sum_link_scales_(ratio_area_scale_ + is_in_iframe_scale_ +
                       is_same_host_scale_ + contains_image_scale_ +
                       is_url_incremented_scale_ + area_rank_scale_ +
                       ratio_distance_root_top_scale_),
      sum_page_scales_(link_total_scale_ + iframe_link_total_scale_ +
                       increment_link_total_scale_ +
                       same_origin_link_total_scale_ + image_link_total_scale_ +
                       clickable_space_scale_ + median_link_location_scale_ +
                       viewport_height_scale_ + viewport_width_scale_),
      is_low_end_device_(base::SysInfo::IsLowEndDevice()),
      prefetch_url_score_threshold_(base::GetFieldTrialParamByFeatureAsInt(
          blink::features::kNavigationPredictor,
          "prefetch_url_score_threshold",
          0)),
      prefetch_enabled_(base::GetFieldTrialParamByFeatureAsBool(
          blink::features::kNavigationPredictor,
          "prefetch_after_preconnect",
          false)),
      normalize_navigation_scores_(base::GetFieldTrialParamByFeatureAsBool(
          blink::features::kNavigationPredictor,
          "normalize_scores",
          true)),
      render_frame_host_(render_frame_host) {
  DCHECK(browser_context_);
  DETACH_FROM_SEQUENCE(sequence_checker_);
  DCHECK(render_frame_host_);

  if (!IsMainFrame(render_frame_host))
    return;

  if (browser_context_->IsOffTheRecord())
    return;

  ukm_recorder_ = ukm::UkmRecorder::Get();

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  current_visibility_ = web_contents->GetVisibility();
  ukm_source_id_ = web_contents->GetLastCommittedSourceId();
  Observe(web_contents);
}

NavigationPredictor::~NavigationPredictor() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Observe(nullptr);

  if (prerender_handle_) {
    prerender_handle_->SetObserver(nullptr);
    prerender_handle_->OnNavigateAway();
  }
}

void NavigationPredictor::Create(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::AnchorElementMetricsHost> receiver) {
  DCHECK(base::FeatureList::IsEnabled(blink::features::kNavigationPredictor));

  // Only valid for the main frame.
  if (render_frame_host->GetParent())
    return;

  mojo::MakeSelfOwnedReceiver(
      std::make_unique<NavigationPredictor>(render_frame_host),
      std::move(receiver));
}

bool NavigationPredictor::IsValidMetricFromRenderer(
    const blink::mojom::AnchorElementMetrics& metric) const {
  return metric.target_url.SchemeIsHTTPOrHTTPS() &&
         metric.source_url.SchemeIsHTTPOrHTTPS();
}


void NavigationPredictor::RecordTimingOnClick() {
  base::TimeTicks current_timing = base::TimeTicks::Now();

  // This is the first click in the document.
  // Note that multiple clicks can happen on the same document. For example,
  // if the click opens a new tab, then the old document is not necessarily
  // destroyed. The user can return to the old document and click.
  if (last_click_timing_ == base::TimeTicks()) {
    // Document may have not loaded yet when click happens.
    UMA_HISTOGRAM_TIMES("AnchorElementMetrics.Clicked.DurationLoadToFirstClick",
                        document_loaded_timing_ > base::TimeTicks()
                            ? current_timing - document_loaded_timing_
                            : base::TimeDelta());
  } else {
    UMA_HISTOGRAM_TIMES("AnchorElementMetrics.Clicked.ClickIntervals",
                        current_timing - last_click_timing_);
  }
  last_click_timing_ = current_timing;
}

void NavigationPredictor::RecordActionAccuracyOnClick(
    const GURL& target_url) const {
  // We don't pre-render default search engine at all, so measuring metrics here
  // doesn't make sense.
  if (source_is_default_search_engine_page_)
    return;

  bool is_cross_origin =
      url::Origin::Create(document_url_) != url::Origin::Create(target_url);

  auto prefetch_result = is_cross_origin ? PrerenderResult::kCrossOriginNotSeen
                                         : PrerenderResult::kSameOriginNotSeen;

  if ((prefetch_url_ && prefetch_url_.value() == target_url) ||
      base::Contains(partial_prerfetches_, target_url)) {
    prefetch_result = PrerenderResult::kSameOriginPrefetchPartiallyComplete;
  } else if (base::Contains(urls_prefetched_, target_url)) {
    prefetch_result = PrerenderResult::kSameOriginPrefetchFinished;
  } else if (std::find(urls_to_prefetch_.begin(), urls_to_prefetch_.end(),
                       target_url) != urls_to_prefetch_.end()) {
    prefetch_result = PrerenderResult::kSameOriginPrefetchInQueue;
  } else if (!is_cross_origin &&
             base::Contains(urls_above_threshold_, target_url)) {
    prefetch_result = PrerenderResult::kSameOriginPrefetchSkipped;
  } else if (base::Contains(urls_above_threshold_, target_url)) {
    prefetch_result = PrerenderResult::kCrossOriginAboveThreshold;
  } else if (!is_cross_origin &&
             base::Contains(navigation_scores_map_, target_url.spec())) {
    prefetch_result = PrerenderResult::kSameOriginBelowThreshold;
  } else if (base::Contains(navigation_scores_map_, target_url.spec())) {
    prefetch_result = PrerenderResult::kCrossOriginBelowThreshold;
  }

  UMA_HISTOGRAM_ENUMERATION("NavigationPredictor.LinkClickedPrerenderResult",
                            prefetch_result);
}

void NavigationPredictor::RecordActionAccuracyOnTearDown() {
  auto document_origin = url::Origin::Create(document_url_);
  int cross_origin_urls_above_threshold =
      std::count_if(urls_above_threshold_.begin(), urls_above_threshold_.end(),
                    [document_origin](const GURL& url) {
                      return document_origin != url::Origin::Create(url);
                    });

  UMA_HISTOGRAM_COUNTS_100("NavigationPredictor.CountOfURLsAboveThreshold",
                           urls_above_threshold_.size());

  UMA_HISTOGRAM_COUNTS_100(
      "NavigationPredictor.CountOfURLsAboveThreshold.CrossOrigin",
      cross_origin_urls_above_threshold);

  UMA_HISTOGRAM_COUNTS_100(
      "NavigationPredictor.CountOfURLsAboveThreshold.SameOrigin",
      urls_above_threshold_.size() - cross_origin_urls_above_threshold);

  int cross_origin_urls_above_threshold_in_top_n = std::count_if(
      urls_above_threshold_.begin(),
      urls_above_threshold_.begin() +
          std::min(urls_above_threshold_.size(),
                   static_cast<size_t>(base::GetFieldTrialParamByFeatureAsInt(
                       kNavigationPredictorMultiplePrerenders,
                       "prerender_limit", 1))),
      [document_origin](const GURL& url) {
        return document_origin != url::Origin::Create(url);
      });

  int same_origin_urls_above_threshold_in_top_n =
      std::min(
          static_cast<size_t>(base::GetFieldTrialParamByFeatureAsInt(
              kNavigationPredictorMultiplePrerenders, "prerender_limit", 1)),
          urls_above_threshold_.size()) -
      cross_origin_urls_above_threshold_in_top_n;

  UMA_HISTOGRAM_COUNTS_100(
      "NavigationPredictor.CountOfURLsInPredictedSet.CrossOrigin",
      cross_origin_urls_above_threshold_in_top_n);
  UMA_HISTOGRAM_COUNTS_100(
      "NavigationPredictor.CountOfURLsInPredictedSet.SameOrigin",
      same_origin_urls_above_threshold_in_top_n);
  UMA_HISTOGRAM_COUNTS_100("NavigationPredictor.CountOfURLsInPredictedSet",
                           cross_origin_urls_above_threshold_in_top_n +
                               same_origin_urls_above_threshold_in_top_n);

  UMA_HISTOGRAM_COUNTS_100("NavigationPredictor.CountOfStartedPrerenders",
                           urls_prefetched_.size());
}

void NavigationPredictor::OnVisibilityChanged(content::Visibility visibility) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (current_visibility_ == visibility)
    return;

  // Check if the visibility changed from VISIBLE to HIDDEN. Since navigation
  // predictor is currently restricted to Android, it is okay to disregard the
  // occluded state.
  if (current_visibility_ != content::Visibility::HIDDEN ||
      visibility != content::Visibility::VISIBLE) {
    current_visibility_ = visibility;

    if (prerender_handle_) {
      prerender_handle_->SetObserver(nullptr);
      prerender_handle_->OnNavigateAway();
      prerender_handle_.reset();
      partial_prerfetches_.emplace(prefetch_url_.value());
      prefetch_url_ = base::nullopt;
    }
    return;
  }

  current_visibility_ = visibility;

  MaybePrefetch();
}

void NavigationPredictor::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  if (next_navigation_started_)
    return;

  RecordActionAccuracyOnTearDown();

  // Don't start new prerenders.
  next_navigation_started_ = true;

  // If there is no ongoing prerender, there is nothing to do.
  if (!prefetch_url_.has_value())
    return;

  // Let the prerender continue if it matches the navigation URL.
  if (navigation_handle->GetURL() == prefetch_url_.value())
    return;

  if (!prerender_handle_)
    return;

  // Stop prerender to reduce network contention during main frame fetch.
  prerender_handle_->SetObserver(nullptr);
  prerender_handle_->OnNavigateAway();
  prerender_handle_.reset();
  partial_prerfetches_.emplace(prefetch_url_.value());
  prefetch_url_ = base::nullopt;
}

void NavigationPredictor::RecordAction(Action log_action) {
  std::string action_histogram_name =
      source_is_default_search_engine_page_
          ? "NavigationPredictor.OnDSE.ActionTaken"
          : "NavigationPredictor.OnNonDSE.ActionTaken";
  base::UmaHistogramEnumeration(action_histogram_name, log_action);
}

void NavigationPredictor::MaybeSendMetricsToUkm() const {
  if (!ukm_recorder_) {
    return;
  }

  ukm::builders::NavigationPredictorPageLinkMetrics page_link_builder(
      ukm_source_id_);

  page_link_builder.SetNumberOfAnchors_Total(
      GetBucketMinForPageMetrics(number_of_anchors_));
  page_link_builder.SetNumberOfAnchors_SameHost(
      GetBucketMinForPageMetrics(number_of_anchors_same_host_));
  page_link_builder.SetNumberOfAnchors_ContainsImage(
      GetBucketMinForPageMetrics(number_of_anchors_contains_image_));
  page_link_builder.SetNumberOfAnchors_InIframe(
      GetBucketMinForPageMetrics(number_of_anchors_in_iframe_));
  page_link_builder.SetNumberOfAnchors_URLIncremented(
      GetBucketMinForPageMetrics(number_of_anchors_url_incremented_));
  page_link_builder.SetTotalClickableSpace(
      GetBucketMinForPageMetrics(static_cast<int>(total_clickable_space_)));
  page_link_builder.SetMedianLinkLocation(
      GetLinearBucketForLinkLocation(median_link_location_));
  page_link_builder.SetViewport_Height(
      GetBucketMinForPageMetrics(viewport_size_.height()));
  page_link_builder.SetViewport_Width(
      GetBucketMinForPageMetrics(viewport_size_.width()));

  page_link_builder.Record(ukm_recorder_);

  for (const auto& navigation_score_tuple : navigation_scores_map_) {
    const auto& navigation_score = navigation_score_tuple.second;
    ukm::builders::NavigationPredictorAnchorElementMetrics
        anchor_element_builder(ukm_source_id_);

    // Offset index to be 1-based indexing.
    anchor_element_builder.SetAnchorIndex(navigation_score->index);
    anchor_element_builder.SetIsInIframe(navigation_score->is_in_iframe);
    anchor_element_builder.SetIsURLIncrementedByOne(
        navigation_score->is_url_incremented_by_one);
    anchor_element_builder.SetContainsImage(navigation_score->contains_image);
    anchor_element_builder.SetSameOrigin(
        url::Origin::Create(navigation_score->url) ==
        url::Origin::Create(document_url_));

    // Convert the ratio area and ratio distance from [0,1] to [0,100].
    int percent_ratio_area =
        static_cast<int>(navigation_score->ratio_area * 100);
    int percent_ratio_distance_root_top =
        static_cast<int>(navigation_score->ratio_distance_root_top * 100);

    anchor_element_builder.SetPercentClickableArea(
        GetLinearBucketForRatioArea(percent_ratio_area));
    anchor_element_builder.SetPercentVerticalDistance(
        GetLinearBucketForLinkLocation(percent_ratio_distance_root_top));

    anchor_element_builder.Record(ukm_recorder_);
  }
}

int NavigationPredictor::GetBucketMinForPageMetrics(int value) const {
  return ukm::GetExponentialBucketMin(value, 1.3);
}

int NavigationPredictor::GetLinearBucketForLinkLocation(int value) const {
  return ukm::GetLinearBucketMin(static_cast<int64_t>(value), 10);
}

int NavigationPredictor::GetLinearBucketForRatioArea(int value) const {
  return ukm::GetLinearBucketMin(static_cast<int64_t>(value), 5);
}

void NavigationPredictor::MaybeSendClickMetricsToUkm(
    const std::string& clicked_url) const {
  if (!ukm_recorder_) {
    return;
  }

  if (clicked_count_ > 10)
    return;

  auto nav_score = navigation_scores_map_.find(clicked_url);

  int anchor_element_index = (nav_score == navigation_scores_map_.end())
                                 ? 0
                                 : nav_score->second->index;

  ukm::builders::NavigationPredictorPageLinkClick builder(ukm_source_id_);
  builder.SetAnchorElementIndex(anchor_element_index);
  builder.Record(ukm_recorder_);
}

TemplateURLService* NavigationPredictor::GetTemplateURLService() const {
  return TemplateURLServiceFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context_));
}

void NavigationPredictor::ReportAnchorElementMetricsOnClick(
    blink::mojom::AnchorElementMetricsPtr metrics) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(base::FeatureList::IsEnabled(blink::features::kNavigationPredictor));

  if (browser_context_->IsOffTheRecord())
    return;

  if (!IsValidMetricFromRenderer(*metrics)) {
    mojo::ReportBadMessage("Bad anchor element metrics: onClick.");
    return;
  }

  source_is_default_search_engine_page_ =
      GetTemplateURLService() &&
      GetTemplateURLService()->IsSearchResultsPageFromDefaultSearchProvider(
          metrics->source_url);
  if (!metrics->source_url.SchemeIsCryptographic() ||
      !metrics->target_url.SchemeIsCryptographic()) {
    return;
  }

  RecordTimingOnClick();
  clicked_count_++;

  document_url_ = metrics->source_url;

  RecordActionAccuracyOnClick(metrics->target_url);
  MaybeSendClickMetricsToUkm(metrics->target_url.spec());

  // Look up the clicked URL in |navigation_scores_map_|. Record if we find it.
  auto iter = navigation_scores_map_.find(metrics->target_url.spec());
  if (iter == navigation_scores_map_.end())
    return;

  UMA_HISTOGRAM_COUNTS_100("AnchorElementMetrics.Clicked.AreaRank",
                           static_cast<int>(iter->second->area_rank));
  UMA_HISTOGRAM_COUNTS_100("AnchorElementMetrics.Clicked.NavigationScore",
                           static_cast<int>(iter->second->score));
  UMA_HISTOGRAM_COUNTS_100("AnchorElementMetrics.Clicked.NavigationScoreRank",
                           static_cast<int>(iter->second->score_rank.value()));

  // Guaranteed to be non-zero since we have found the clicked link in
  // |navigation_scores_map_|.
  DCHECK_LT(0, number_of_anchors_);
  if (metrics->is_same_host) {
    UMA_HISTOGRAM_PERCENTAGE(
        "AnchorElementMetrics.Clicked.RatioSameHost_SameHost",
        (number_of_anchors_same_host_ * 100) / number_of_anchors_);
  } else {
    UMA_HISTOGRAM_PERCENTAGE(
        "AnchorElementMetrics.Clicked.RatioSameHost_DiffHost",
        (number_of_anchors_same_host_ * 100) / number_of_anchors_);
  }

  if (source_is_default_search_engine_page_) {
    UMA_HISTOGRAM_BOOLEAN("AnchorElementMetrics.Clicked.OnDSE.SameHost",
                          metrics->is_same_host);
  } else {
    UMA_HISTOGRAM_BOOLEAN("AnchorElementMetrics.Clicked.OnNonDSE.SameHost",
                          metrics->is_same_host);
  }

  // Check if the clicked anchor element contains image or if any other anchor
  // element pointing to the same url contains an image.
  if (metrics->contains_image || iter->second->contains_image) {
    UMA_HISTOGRAM_PERCENTAGE(
        "AnchorElementMetrics.Clicked.RatioContainsImage_ContainsImage",
        (number_of_anchors_contains_image_ * 100) / number_of_anchors_);
  } else {
    UMA_HISTOGRAM_PERCENTAGE(
        "AnchorElementMetrics.Clicked.RatioContainsImage_NoImage",
        (number_of_anchors_contains_image_ * 100) / number_of_anchors_);
  }

  if (metrics->is_in_iframe) {
    UMA_HISTOGRAM_PERCENTAGE(
        "AnchorElementMetrics.Clicked.RatioInIframe_InIframe",
        (number_of_anchors_in_iframe_ * 100) / number_of_anchors_);
  } else {
    UMA_HISTOGRAM_PERCENTAGE(
        "AnchorElementMetrics.Clicked.RatioInIframe_NotInIframe",
        (number_of_anchors_in_iframe_ * 100) / number_of_anchors_);
  }

  if (metrics->is_url_incremented_by_one) {
    UMA_HISTOGRAM_PERCENTAGE(
        "AnchorElementMetrics.Clicked.RatioUrlIncremented_UrlIncremented",
        (number_of_anchors_url_incremented_ * 100) / number_of_anchors_);
  } else {
    UMA_HISTOGRAM_PERCENTAGE(
        "AnchorElementMetrics.Clicked.RatioUrlIncremented_NotIncremented",
        (number_of_anchors_url_incremented_ * 100) / number_of_anchors_);
  }
}

void NavigationPredictor::MergeMetricsSameTargetUrl(
    std::vector<blink::mojom::AnchorElementMetricsPtr>* metrics) const {
  UMA_HISTOGRAM_COUNTS_100(
      "AnchorElementMetrics.Visible.NumberOfAnchorElements", metrics->size());

  // Maps from target url (href) to anchor element metrics from renderer.
  std::unordered_map<std::string, blink::mojom::AnchorElementMetricsPtr>
      metrics_map;

  // This size reserve is aggressive since |metrics_map| may contain fewer
  // elements than metrics->size() after merge.
  metrics_map.reserve(metrics->size());

  for (auto& metric : *metrics) {
    // Do not include anchor elements that point to the same URL as the URL of
    // the current navigation since these are unlikely to be clicked. Also,
    // exclude the anchor elements that differ from the URL of the current
    // navigation by only the ref param.
    if (AreGURLsEqualExcludingRefParams(metric->target_url,
                                        metric->source_url)) {
      continue;
    }

    if (!metric->target_url.SchemeIsCryptographic())
      continue;

    // Currently, all predictions are made based on elements that are within the
    // main frame since it is unclear if we can pre* the target of the elements
    // within iframes.
    if (metric->is_in_iframe)
      continue;

    // Skip ref params when merging the anchor elements. This ensures that two
    // anchor elements which differ only in the ref params are combined
    // together.
    const std::string& key = GetURLWithoutRefParams(metric->target_url);
    auto iter = metrics_map.find(key);
    if (iter == metrics_map.end()) {
      metrics_map[key] = std::move(metric);
    } else {
      auto& prev_metric = iter->second;
      prev_metric->ratio_area += metric->ratio_area;
      prev_metric->ratio_visible_area += metric->ratio_visible_area;

      // After merging, value of |ratio_area| can go beyond 1.0. This can
      // happen, e.g., when there are 2 anchor elements pointing to the same
      // target. The first anchor element occupies 90% of the viewport. The
      // second one has size 0.8 times the viewport, and only part of it is
      // visible in the viewport. In that case, |ratio_area| may be 1.7.
      if (prev_metric->ratio_area > 1.0)
        prev_metric->ratio_area = 1.0;
      DCHECK_LE(0.0, prev_metric->ratio_area);
      DCHECK_GE(1.0, prev_metric->ratio_area);

      DCHECK_GE(1.0, prev_metric->ratio_visible_area);

      // Position related metrics are tricky to merge. Another possible way to
      // merge is simply add up the calculated navigation scores.
      prev_metric->ratio_distance_root_top =
          std::min(prev_metric->ratio_distance_root_top,
                   metric->ratio_distance_root_top);
      prev_metric->ratio_distance_root_bottom =
          std::max(prev_metric->ratio_distance_root_bottom,
                   metric->ratio_distance_root_bottom);
      prev_metric->ratio_distance_top_to_visible_top =
          std::min(prev_metric->ratio_distance_top_to_visible_top,
                   metric->ratio_distance_top_to_visible_top);
      prev_metric->ratio_distance_center_to_visible_top =
          std::min(prev_metric->ratio_distance_center_to_visible_top,
                   metric->ratio_distance_center_to_visible_top);

      // Anchor element is not considered in an iframe as long as at least one
      // of them is not in an iframe.
      prev_metric->is_in_iframe =
          prev_metric->is_in_iframe && metric->is_in_iframe;
      prev_metric->contains_image =
          prev_metric->contains_image || metric->contains_image;
      DCHECK_EQ(prev_metric->is_same_host, metric->is_same_host);
    }
  }

  metrics->clear();

  if (metrics_map.empty())
    return;

  metrics->reserve(metrics_map.size());
  for (auto& metric_mapping : metrics_map) {
    metrics->push_back(std::move(metric_mapping.second));
  }

  DCHECK(!metrics->empty());
  UMA_HISTOGRAM_COUNTS_100(
      "AnchorElementMetrics.Visible.NumberOfAnchorElementsAfterMerge",
      metrics->size());
}

void NavigationPredictor::ReportAnchorElementMetricsOnLoad(
    std::vector<blink::mojom::AnchorElementMetricsPtr> metrics,
    const gfx::Size& viewport_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(base::FeatureList::IsEnabled(blink::features::kNavigationPredictor));

  // Each document should only report metrics once when page is loaded.
  DCHECK(navigation_scores_map_.empty());

  if (browser_context_->IsOffTheRecord())
    return;

  if (metrics.empty()) {
    mojo::ReportBadMessage("Bad anchor element metrics: empty.");
    return;
  }

  for (const auto& metric : metrics) {
    if (!IsValidMetricFromRenderer(*metric)) {
      mojo::ReportBadMessage("Bad anchor element metrics: onLoad.");
      return;
    }
  }

  if (!metrics[0]->source_url.SchemeIsCryptographic())
    return;

  document_loaded_timing_ = base::TimeTicks::Now();

  source_is_default_search_engine_page_ =
      GetTemplateURLService() &&
      GetTemplateURLService()->IsSearchResultsPageFromDefaultSearchProvider(
          metrics[0]->source_url);
  MergeMetricsSameTargetUrl(&metrics);

  if (metrics.empty() || viewport_size.IsEmpty())
    return;

  number_of_anchors_ = metrics.size();
  viewport_size_ = viewport_size;

  // Count the number of anchors that have specific metrics.
  std::vector<double> link_locations;
  link_locations.reserve(metrics.size());

  for (const auto& metric : metrics) {
    number_of_anchors_same_host_ += static_cast<int>(metric->is_same_host);
    number_of_anchors_contains_image_ +=
        static_cast<int>(metric->contains_image);
    number_of_anchors_in_iframe_ += static_cast<int>(metric->is_in_iframe);
    number_of_anchors_url_incremented_ +=
        static_cast<int>(metric->is_url_incremented_by_one);

    link_locations.push_back(metric->ratio_distance_top_to_visible_top);
    total_clickable_space_ += metric->ratio_visible_area * 100.0;
  }

  sort(link_locations.begin(), link_locations.end());
  median_link_location_ = link_locations[link_locations.size() / 2] * 100;
  double page_metrics_score = GetPageMetricsScore();

  // Sort metric by area in descending order to get area rank, which is a
  // derived feature to calculate navigation score.
  std::sort(metrics.begin(), metrics.end(), [](const auto& a, const auto& b) {
    return a->ratio_area > b->ratio_area;
  });

  // Loop |metrics| to compute navigation scores.
  std::vector<std::unique_ptr<NavigationScore>> navigation_scores;
  navigation_scores.reserve(metrics.size());
  double total_score = 0.0;

  std::vector<int> indices(metrics.size());
  std::generate(indices.begin(), indices.end(),
                [n = 1]() mutable { return n++; });

  // Shuffle the indices to keep metrics less identifiable in UKM.
  base::RandomShuffle(indices.begin(), indices.end());

  for (size_t i = 0; i != metrics.size(); ++i) {
    const auto& metric = metrics[i];
    RecordMetricsOnLoad(*metric);

    // Anchor elements with the same area are assigned with the same rank.
    size_t area_rank = i;
    if (i > 0 && metric->ratio_area == metrics[i - 1]->ratio_area)
      area_rank = navigation_scores[navigation_scores.size() - 1]->area_rank;

    double score =
        CalculateAnchorNavigationScore(*metric, area_rank) + page_metrics_score;
    total_score += score;

    navigation_scores.push_back(std::make_unique<NavigationScore>(
        metric->target_url, static_cast<double>(metric->ratio_area),
        metric->is_url_incremented_by_one, area_rank, score,
        metric->ratio_distance_root_top, metric->contains_image,
        metric->is_in_iframe, indices[i]));
  }

  if (normalize_navigation_scores_) {
    // Normalize |score| to a total sum of 100.0 across all anchor elements
    // received.
    if (total_score > 0.0) {
      for (auto& navigation_score : navigation_scores) {
        navigation_score->score = navigation_score->score / total_score * 100.0;
      }
    }
  }

  // Sort scores by the calculated navigation score in descending order. This
  // score rank is used by MaybeTakeActionOnLoad, and stored in
  // |navigation_scores_map_|.
  std::sort(navigation_scores.begin(), navigation_scores.end(),
            [](const auto& a, const auto& b) { return a->score > b->score; });

  document_url_ = metrics[0]->source_url;
  MaybeTakeActionOnLoad(document_url_, navigation_scores);

  // Store navigation scores in |navigation_scores_map_| for fast look up upon
  // clicks.
  navigation_scores_map_.reserve(navigation_scores.size());
  for (size_t i = 0; i != navigation_scores.size(); ++i) {
    navigation_scores[i]->score_rank = base::make_optional(i);
    std::string url_spec = navigation_scores[i]->url.spec();
    navigation_scores_map_[url_spec] = std::move(navigation_scores[i]);
  }

  MaybeSendMetricsToUkm();
}

double NavigationPredictor::CalculateAnchorNavigationScore(
    const blink::mojom::AnchorElementMetrics& metrics,
    int area_rank) const {
  DCHECK(!browser_context_->IsOffTheRecord());

  if (sum_link_scales_ == 0)
    return 0.0;

  double area_rank_score =
      (double)((number_of_anchors_ - area_rank)) / number_of_anchors_;

  DCHECK_LE(0, metrics.ratio_visible_area);
  DCHECK_GE(1, metrics.ratio_visible_area);

  DCHECK_LE(0, metrics.is_in_iframe);
  DCHECK_GE(1, metrics.is_in_iframe);

  DCHECK_LE(0, metrics.is_same_host);
  DCHECK_GE(1, metrics.is_same_host);

  DCHECK_LE(0, metrics.contains_image);
  DCHECK_GE(1, metrics.contains_image);

  DCHECK_LE(0, metrics.is_url_incremented_by_one);
  DCHECK_GE(1, metrics.is_url_incremented_by_one);

  DCHECK_LE(0, area_rank_score);
  DCHECK_GE(1, area_rank_score);

  double host_score = 0.0;
  // On pages from default search engine, give higher weight to target URLs that
  // link to a different host. On non-default search engine pages, give higher
  // weight to target URLs that link to the same host.
  if (!source_is_default_search_engine_page_ && metrics.is_same_host) {
    host_score = is_same_host_scale_;
  } else if (source_is_default_search_engine_page_ && !metrics.is_same_host) {
    host_score = is_same_host_scale_;
  }

  // TODO(chelu): https://crbug.com/850624/. Experiment with other heuristic
  // algorithms for computing the anchor elements score.
  double score =
      (ratio_area_scale_ * GetLinearBucketForRatioArea(
                               static_cast<int>(metrics.ratio_area * 100.0))) +
      (metrics.is_in_iframe ? is_in_iframe_scale_ : 0.0) +
      (metrics.contains_image ? contains_image_scale_ : 0.0) + host_score +
      (metrics.is_url_incremented_by_one ? is_url_incremented_scale_ : 0.0) +
      (area_rank_scale_ * area_rank_score) +
      (ratio_distance_root_top_scale_ *
       GetLinearBucketForLinkLocation(
           static_cast<int>(metrics.ratio_distance_root_top * 100.0)));

  if (normalize_navigation_scores_) {
    score = score / sum_link_scales_ * 100.0;
    DCHECK_LE(0.0, score);
  }

  return score;
}

double NavigationPredictor::GetPageMetricsScore() const {
  if (sum_page_scales_ == 0.0) {
    return 0;
  } else {
    DCHECK(!viewport_size_.IsEmpty());
    return (link_total_scale_ *
            GetBucketMinForPageMetrics(number_of_anchors_)) +
           (iframe_link_total_scale_ *
            GetBucketMinForPageMetrics(number_of_anchors_in_iframe_)) +
           (increment_link_total_scale_ *
            GetBucketMinForPageMetrics(number_of_anchors_url_incremented_)) +
           (same_origin_link_total_scale_ *
            GetBucketMinForPageMetrics(number_of_anchors_same_host_)) +
           (image_link_total_scale_ *
            GetBucketMinForPageMetrics(number_of_anchors_contains_image_)) +
           (clickable_space_scale_ *
            GetBucketMinForPageMetrics(total_clickable_space_)) +
           (median_link_location_scale_ *
            GetLinearBucketForLinkLocation(median_link_location_)) +
           (viewport_width_scale_ *
            GetBucketMinForPageMetrics(viewport_size_.width())) +
           (viewport_height_scale_ *
            GetBucketMinForPageMetrics(viewport_size_.height()));
  }
}

void NavigationPredictor::NotifyPredictionUpdated(
    const std::vector<std::unique_ptr<NavigationScore>>&
        sorted_navigation_scores) {
  NavigationPredictorKeyedService* service =
      NavigationPredictorKeyedServiceFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context_));
  DCHECK(service);
  std::vector<GURL> top_urls;
  top_urls.reserve(sorted_navigation_scores.size());
  for (const auto& nav_score : sorted_navigation_scores) {
    top_urls.push_back(nav_score->url);
  }
  service->OnPredictionUpdated(render_frame_host_, document_url_, top_urls);
}

void NavigationPredictor::MaybeTakeActionOnLoad(
    const GURL& document_url,
    const std::vector<std::unique_ptr<NavigationScore>>&
        sorted_navigation_scores) {
  DCHECK(!browser_context_->IsOffTheRecord());

  NotifyPredictionUpdated(sorted_navigation_scores);

  // Try prefetch first.
  urls_to_prefetch_ = GetUrlsToPrefetch(document_url, sorted_navigation_scores);
  RecordAction(urls_to_prefetch_.empty() ? Action::kNone : Action::kPrefetch);
  MaybePrefetch();
}

void NavigationPredictor::MaybePrefetch() {
  // If prefetches aren't allowed here, this URL has already
  // been prefetched, or the current tab is hidden,
  // we shouldn't prefetch again.
  if (!prefetch_enabled_ || urls_to_prefetch_.empty() ||
      current_visibility_ == content::Visibility::HIDDEN) {
    return;
  }

  // Already an on-going prefetch.
  if (prefetch_url_.has_value())
    return;

  // Don't prerender if the next navigation started.
  if (next_navigation_started_)
    return;

  prerender::PrerenderManager* prerender_manager =
      prerender::PrerenderManagerFactory::GetForBrowserContext(
          browser_context_);

  if (prerender_manager) {
    GURL url_to_prefetch = urls_to_prefetch_.front();
    urls_to_prefetch_.pop_front();
    Prefetch(prerender_manager, url_to_prefetch);
  }
}

void NavigationPredictor::Prefetch(
    prerender::PrerenderManager* prerender_manager,
    const GURL& url_to_prefetch) {
  DCHECK(!prerender_handle_);
  DCHECK(!prefetch_url_);

  content::SessionStorageNamespace* session_storage_namespace =
      web_contents()->GetController().GetDefaultSessionStorageNamespace();
  gfx::Size size = web_contents()->GetContainerBounds().size();

  prerender_handle_ = prerender_manager->AddPrerenderFromNavigationPredictor(
      url_to_prefetch, session_storage_namespace, size);

  // Prerender was prevented for some reason, try next URL.
  if (!prerender_handle_) {
    MaybePrefetch();
    return;
  }

  prefetch_url_ = url_to_prefetch;
  urls_prefetched_.emplace(url_to_prefetch);

  prerender_handle_->SetObserver(this);
}

void NavigationPredictor::OnPrerenderStop(prerender::PrerenderHandle* handle) {
  DCHECK_EQ(prerender_handle_.get(), handle);
  prerender_handle_.reset();
  prefetch_url_ = base::nullopt;

  MaybePrefetch();
}

std::deque<GURL> NavigationPredictor::GetUrlsToPrefetch(
    const GURL& document_url,
    const std::vector<std::unique_ptr<NavigationScore>>&
        sorted_navigation_scores) {
  urls_above_threshold_.clear();
  std::deque<GURL> urls_to_prefetch;
  // Currently, prefetch is disabled on low-end devices since prefetch may
  // increase memory usage.
  if (is_low_end_device_)
    return urls_to_prefetch;

  // On search engine results page, next navigation is likely to be a different
  // origin. Currently, the prefetch is only allowed for same orgins. Hence,
  // prefetch is currently disabled on search engine results page.
  if (source_is_default_search_engine_page_)
    return urls_to_prefetch;

  if (sorted_navigation_scores.empty())
    return urls_to_prefetch;

  // Place in order the top n scoring links. If the top n scoring links contain
  // a cross origin link, only place n-1 links. All links must score above
  // |prefetch_url_score_threshold_|.
  for (size_t i = 0; i < sorted_navigation_scores.size(); ++i) {
    double navigation_score = sorted_navigation_scores[i]->score;
    GURL url_to_prefetch = sorted_navigation_scores[i]->url;

    // If the prediction score of the highest scoring URL is less than the
    // threshold, then return.
    if (navigation_score < prefetch_url_score_threshold_)
      break;

    // Log the links above the threshold.
    urls_above_threshold_.push_back(url_to_prefetch);

    if (i >=
        static_cast<size_t>(base::GetFieldTrialParamByFeatureAsInt(
            kNavigationPredictorMultiplePrerenders, "prerender_limit", 1))) {
      continue;
    }

    // Only the same origin URLs are eligible for prefetching. If the URL with
    // the highest score is from a different origin, then we skip prefetching
    // since same origin URLs are not likely to be clicked.
    if (url::Origin::Create(url_to_prefetch) !=
        url::Origin::Create(document_url)) {
      continue;
    }

    urls_to_prefetch.emplace_back(url_to_prefetch);
  }

  UMA_HISTOGRAM_COUNTS_100(
      "AnchorElementMetrics.Visible.HighestNavigationScore",
      static_cast<int>(sorted_navigation_scores[0]->score));

  return urls_to_prefetch;
}

void NavigationPredictor::RecordMetricsOnLoad(
    const blink::mojom::AnchorElementMetrics& metric) const {
  DCHECK(!browser_context_->IsOffTheRecord());

  UMA_HISTOGRAM_PERCENTAGE("AnchorElementMetrics.Visible.RatioArea",
                           static_cast<int>(metric.ratio_area * 100));

  UMA_HISTOGRAM_PERCENTAGE("AnchorElementMetrics.Visible.RatioVisibleArea",
                           static_cast<int>(metric.ratio_visible_area * 100));

  UMA_HISTOGRAM_PERCENTAGE(
      "AnchorElementMetrics.Visible.RatioDistanceTopToVisibleTop",
      static_cast<int>(
          std::min(metric.ratio_distance_top_to_visible_top, 1.0f) * 100));

  UMA_HISTOGRAM_PERCENTAGE(
      "AnchorElementMetrics.Visible.RatioDistanceCenterToVisibleTop",
      static_cast<int>(
          std::min(metric.ratio_distance_center_to_visible_top, 1.0f) * 100));

  UMA_HISTOGRAM_COUNTS_10000(
      "AnchorElementMetrics.Visible.RatioDistanceRootTop",
      static_cast<int>(std::min(metric.ratio_distance_root_top, 100.0f) * 100));

  UMA_HISTOGRAM_COUNTS_10000(
      "AnchorElementMetrics.Visible.RatioDistanceRootBottom",
      static_cast<int>(std::min(metric.ratio_distance_root_bottom, 100.0f) *
                       100));

  UMA_HISTOGRAM_BOOLEAN("AnchorElementMetrics.Visible.IsInIFrame",
                        metric.is_in_iframe);

  UMA_HISTOGRAM_BOOLEAN("AnchorElementMetrics.Visible.ContainsImage",
                        metric.contains_image);

  UMA_HISTOGRAM_BOOLEAN("AnchorElementMetrics.Visible.IsSameHost",
                        metric.is_same_host);

  UMA_HISTOGRAM_BOOLEAN("AnchorElementMetrics.Visible.IsUrlIncrementedByOne",
                        metric.is_url_incremented_by_one);
}
