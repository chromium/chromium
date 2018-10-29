// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/navigation_predictor/navigation_predictor.h"

#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/site_instance.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

struct NavigationPredictor::NavigationScore {
  NavigationScore(const GURL& url,
                  size_t area_rank,
                  double score,
                  bool contains_image)
      : url(url),
        area_rank(area_rank),
        score(score),
        contains_image(contains_image) {}
  // URL of the target link.
  const GURL url;

  // Rank in terms of anchor element area. It starts at 0, a lower rank implies
  // a larger area.
  const size_t area_rank;

  // Calculated navigation score, based on |area_rank| and other metrics.
  const double score;

  // Multiple anchor elements may point to the same |url|. |contains_image| is
  // true if at least one of the anchor elements pointing to |url| contains an
  // image.
  const bool contains_image;

  // Rank of the |score| in this document. It starts at 0, a lower rank implies
  // a higher |score|.
  base::Optional<size_t> score_rank;
};

NavigationPredictor::NavigationPredictor(
    content::RenderFrameHost* render_frame_host)
    : browser_context_(
          render_frame_host->GetSiteInstance()->GetBrowserContext()),
      ratio_area_scale_(base::GetFieldTrialParamByFeatureAsInt(
          blink::features::kRecordAnchorMetricsVisible,
          "ratio_area_scale",
          100)),
      is_in_iframe_scale_(base::GetFieldTrialParamByFeatureAsInt(
          blink::features::kRecordAnchorMetricsVisible,
          "is_in_iframe_scale",
          0)),
      is_same_host_scale_(base::GetFieldTrialParamByFeatureAsInt(
          blink::features::kRecordAnchorMetricsVisible,
          "is_same_host_scale",
          0)),
      contains_image_scale_(base::GetFieldTrialParamByFeatureAsInt(
          blink::features::kRecordAnchorMetricsVisible,
          "contains_image_scale",
          50)),
      is_url_incremented_scale_(base::GetFieldTrialParamByFeatureAsInt(
          blink::features::kRecordAnchorMetricsVisible,
          "is_url_incremented_scale",
          100)),
      source_engagement_score_scale_(base::GetFieldTrialParamByFeatureAsInt(
          blink::features::kRecordAnchorMetricsVisible,
          "source_engagement_score_scale",
          100)),
      target_engagement_score_scale_(base::GetFieldTrialParamByFeatureAsInt(
          blink::features::kRecordAnchorMetricsVisible,
          "target_engagement_score_scale",
          100)),
      area_rank_scale_(base::GetFieldTrialParamByFeatureAsInt(
          blink::features::kRecordAnchorMetricsVisible,
          "area_rank_scale",
          100)),
      sum_scales_(ratio_area_scale_ + is_in_iframe_scale_ +
                  is_same_host_scale_ + contains_image_scale_ +
                  is_url_incremented_scale_ + source_engagement_score_scale_ +
                  target_engagement_score_scale_ + area_rank_scale_) {
  DCHECK(browser_context_);
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

NavigationPredictor::~NavigationPredictor() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void NavigationPredictor::Create(
    blink::mojom::AnchorElementMetricsHostRequest request,
    content::RenderFrameHost* render_frame_host) {
  // Only valid for the main frame.
  if (render_frame_host->GetParent())
    return;

  mojo::MakeStrongBinding(
      std::make_unique<NavigationPredictor>(render_frame_host),
      std::move(request));
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

SiteEngagementService* NavigationPredictor::GetEngagementService() const {
  Profile* profile = Profile::FromBrowserContext(browser_context_);
  SiteEngagementService* service = SiteEngagementService::Get(profile);
  DCHECK(service);
  return service;
}

void NavigationPredictor::ReportAnchorElementMetricsOnClick(
    blink::mojom::AnchorElementMetricsPtr metrics) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsValidMetricFromRenderer(*metrics)) {
    mojo::ReportBadMessage("Bad anchor element metrics: onClick.");
    return;
  }

  RecordTimingOnClick();

  SiteEngagementService* engagement_service = GetEngagementService();

  UMA_HISTOGRAM_COUNTS_100(
      "AnchorElementMetrics.Clicked.DocumentEngagementScore",
      static_cast<int>(engagement_service->GetScore(metrics->source_url)));

  double target_score = engagement_service->GetScore(metrics->target_url);
  UMA_HISTOGRAM_COUNTS_100("AnchorElementMetrics.Clicked.HrefEngagementScore2",
                           static_cast<int>(target_score));
  if (target_score > 0) {
    UMA_HISTOGRAM_COUNTS_100(
        "AnchorElementMetrics.Clicked.HrefEngagementScorePositive",
        static_cast<int>(target_score));
  }
  if (!metrics->is_same_host) {
    UMA_HISTOGRAM_COUNTS_100(
        "AnchorElementMetrics.Clicked.HrefEngagementScoreExternal",
        static_cast<int>(target_score));
  }

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
  int number_of_anchors = static_cast<int>(navigation_scores_map_.size());
  if (metrics->is_same_host) {
    UMA_HISTOGRAM_PERCENTAGE(
        "AnchorElementMetrics.Clicked.RatioSameHost_SameHost",
        (number_of_anchors_same_host_ * 100) / number_of_anchors);
  } else {
    UMA_HISTOGRAM_PERCENTAGE(
        "AnchorElementMetrics.Clicked.RatioSameHost_DiffHost",
        (number_of_anchors_same_host_ * 100) / number_of_anchors);
  }

  // Check if the clicked anchor element contains image or if any other anchor
  // element pointing to the same url contains an image.
  if (metrics->contains_image || iter->second->contains_image) {
    UMA_HISTOGRAM_PERCENTAGE(
        "AnchorElementMetrics.Clicked.RatioContainsImage_ContainsImage",
        (number_of_anchors_contains_image_ * 100) / number_of_anchors);
  } else {
    UMA_HISTOGRAM_PERCENTAGE(
        "AnchorElementMetrics.Clicked.RatioContainsImage_NoImage",
        (number_of_anchors_contains_image_ * 100) / number_of_anchors);
  }

  if (metrics->is_in_iframe) {
    UMA_HISTOGRAM_PERCENTAGE(
        "AnchorElementMetrics.Clicked.RatioInIframe_InIframe",
        (number_of_anchors_in_iframe_ * 100) / number_of_anchors);
  } else {
    UMA_HISTOGRAM_PERCENTAGE(
        "AnchorElementMetrics.Clicked.RatioInIframe_NotInIframe",
        (number_of_anchors_in_iframe_ * 100) / number_of_anchors);
  }

  if (metrics->is_url_incremented_by_one) {
    UMA_HISTOGRAM_PERCENTAGE(
        "AnchorElementMetrics.Clicked.RatioUrlIncremented_UrlIncremented",
        (number_of_anchors_url_incremented_ * 100) / number_of_anchors);
  } else {
    UMA_HISTOGRAM_PERCENTAGE(
        "AnchorElementMetrics.Clicked.RatioUrlIncremented_NotIncremented",
        (number_of_anchors_url_incremented_ * 100) / number_of_anchors);
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
    // the current navigation since these are unlikely to be clicked.
    if (metric->target_url == metric->source_url)
      continue;

    const std::string& key = metric->target_url.spec();
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
    std::vector<blink::mojom::AnchorElementMetricsPtr> metrics) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Each document should only report metrics once when page is loaded.
  DCHECK(navigation_scores_map_.empty());

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

  document_loaded_timing_ = base::TimeTicks::Now();

  MergeMetricsSameTargetUrl(&metrics);

  if (metrics.empty())
    return;

  // Count the number of anchors that have specific metrics.
  for (const auto& metric : metrics) {
    number_of_anchors_same_host_ += static_cast<int>(metric->is_same_host);
    number_of_anchors_contains_image_ +=
        static_cast<int>(metric->contains_image);
    number_of_anchors_in_iframe_ += static_cast<int>(metric->is_in_iframe);
    number_of_anchors_url_incremented_ +=
        static_cast<int>(metric->is_url_incremented_by_one);
  }

  // Retrieve site engagement score of the document. |metrics| is guaranteed to
  // be non-empty. All |metrics| have the same source_url.
  SiteEngagementService* engagement_service = GetEngagementService();
  double document_engagement_score =
      engagement_service->GetScore(metrics[0]->source_url);
  DCHECK(document_engagement_score >= 0 &&
         document_engagement_score <= engagement_service->GetMaxPoints());
  UMA_HISTOGRAM_COUNTS_100(
      "AnchorElementMetrics.Visible.DocumentEngagementScore",
      static_cast<int>(document_engagement_score));

  // Sort metric by area in descending order to get area rank, which is a
  // derived feature to calculate navigation score.
  std::sort(metrics.begin(), metrics.end(), [](const auto& a, const auto& b) {
    return a->ratio_area > b->ratio_area;
  });

  // Loop |metrics| to compute navigation scores.
  std::vector<std::unique_ptr<NavigationScore>> navigation_scores;
  navigation_scores.reserve(metrics.size());
  for (size_t i = 0; i != metrics.size(); ++i) {
    const auto& metric = metrics[i];
    RecordMetricsOnLoad(*metric);

    const double target_engagement_score =
        engagement_service->GetScore(metric->target_url);
    DCHECK(target_engagement_score >= 0 &&
           target_engagement_score <= engagement_service->GetMaxPoints());
    UMA_HISTOGRAM_COUNTS_100(
        "AnchorElementMetrics.Visible.HrefEngagementScore2",
        static_cast<int>(target_engagement_score));
    if (!metric->is_same_host) {
      UMA_HISTOGRAM_COUNTS_100(
          "AnchorElementMetrics.Visible.HrefEngagementScoreExternal",
          static_cast<int>(target_engagement_score));
    }

    // Anchor elements with the same area are assigned with the same rank.
    size_t area_rank = i;
    if (i > 0 && metric->ratio_area == metrics[i - 1]->ratio_area)
      area_rank = navigation_scores[navigation_scores.size() - 1]->area_rank;

    double score = CalculateAnchorNavigationScore(
        *metric, document_engagement_score, target_engagement_score, area_rank,
        metrics.size());

    navigation_scores.push_back(std::make_unique<NavigationScore>(
        metric->target_url, area_rank, score, metric->contains_image));
  }

  // Sort scores by the calculated navigation score in descending order. This
  // score rank is used by MaybeTakeActionOnLoad, and stored in
  // |navigation_scores_map_|.
  std::sort(navigation_scores.begin(), navigation_scores.end(),
            [](const auto& a, const auto& b) { return a->score > b->score; });

  MaybeTakeActionOnLoad(navigation_scores);

  // Store navigation scores in |navigation_scores_map_| for fast look up upon
  // clicks.
  navigation_scores_map_.reserve(navigation_scores.size());
  for (size_t i = 0; i != navigation_scores.size(); ++i) {
    navigation_scores[i]->score_rank = base::make_optional(i);
    navigation_scores_map_[navigation_scores[i]->url.spec()] =
        std::move(navigation_scores[i]);
  }
}

double NavigationPredictor::CalculateAnchorNavigationScore(
    const blink::mojom::AnchorElementMetrics& metrics,
    double document_engagement_score,
    double target_engagement_score,
    int area_rank,
    int number_of_anchors) const {
  if (sum_scales_ == 0)
    return 0.0;

  double max_engagement_points = GetEngagementService()->GetMaxPoints();
  document_engagement_score /= max_engagement_points;
  target_engagement_score /= max_engagement_points;

  double area_rank_score =
      (double)((number_of_anchors - area_rank)) / number_of_anchors;

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

  DCHECK_LE(0, document_engagement_score);
  DCHECK_GE(1, document_engagement_score);

  DCHECK_LE(0, target_engagement_score);
  DCHECK_GE(1, target_engagement_score);

  DCHECK_LE(0, area_rank_score);
  DCHECK_GE(1, area_rank_score);

  // TODO(chelu): https://crbug.com/850624/. Experiment with other heuristic
  // algorithms for computing the anchor elements score.
  double score = ratio_area_scale_ * metrics.ratio_visible_area +
                 is_in_iframe_scale_ * metrics.is_in_iframe +
                 is_same_host_scale_ * metrics.is_same_host +
                 contains_image_scale_ * metrics.contains_image +
                 is_url_incremented_scale_ * metrics.is_url_incremented_by_one +
                 source_engagement_score_scale_ * document_engagement_score +
                 target_engagement_score_scale_ * target_engagement_score +
                 area_rank_scale_ * (area_rank_score);

  // Normalize to 100.
  score = score / sum_scales_ * 100.0;
  DCHECK_LE(0.0, score);
  DCHECK_GE(100.0, score);
  return score;
}

void NavigationPredictor::MaybeTakeActionOnLoad(
    const std::vector<std::unique_ptr<NavigationScore>>&
        sorted_navigation_scores) const {
  // TODO(chelu): https://crbug.com/850624/. Given the calculated navigation
  // scores, this function decides which action to take, or decides not to do
  // anything. Example actions including preresolve, preload, prerendering, etc.

  // |sorted_navigation_scores| are sorted in descending order, the first one
  // has the highest navigation score.
  UMA_HISTOGRAM_COUNTS_100(
      "AnchorElementMetrics.Visible.HighestNavigationScore",
      static_cast<int>(sorted_navigation_scores[0]->score));
}

void NavigationPredictor::RecordMetricsOnLoad(
    const blink::mojom::AnchorElementMetrics& metric) const {
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
