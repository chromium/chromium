// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/page_anchors_metrics_observer.h"

#include "content/public/browser/web_contents_user_data.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"

PageAnchorsMetricsObserver::AnchorsData::AnchorsData(
    content::WebContents* contents)
    : content::WebContentsUserData<AnchorsData>(*contents) {}

PageAnchorsMetricsObserver::AnchorsData::~AnchorsData() = default;

int PageAnchorsMetricsObserver::AnchorsData::MedianLinkLocation() {
  DCHECK(!link_locations_.empty());
  sort(link_locations_.begin(), link_locations_.end());
  size_t idx = link_locations_.size() / 2;
  if (link_locations_.size() % 2 == 0) {
    return (link_locations_[idx - 1] + link_locations_[idx]) * 50;
  }
  return link_locations_[link_locations_.size() / 2] * 100;
}

void PageAnchorsMetricsObserver::AnchorsData::Clear() {
  number_of_anchors_same_host_ = 0;
  number_of_anchors_contains_image_ = 0;
  number_of_anchors_in_iframe_ = 0;
  number_of_anchors_url_incremented_ = 0;
  number_of_anchors_ = 0;
  total_clickable_space_ = 0;
  viewport_height_ = 0;
  viewport_width_ = 0;
  link_locations_.clear();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PageAnchorsMetricsObserver::AnchorsData);

PageAnchorsMetricsObserver::UserInteractionsData::UserInteractionsData(
    content::WebContents* contents)
    : content::WebContentsUserData<UserInteractionsData>(*contents) {}

PageAnchorsMetricsObserver::UserInteractionsData::~UserInteractionsData() =
    default;

void PageAnchorsMetricsObserver::UserInteractionsData::
    RecordUserInteractionMetrics(
        ukm::SourceId ukm_source_id,
        absl::optional<base::TimeDelta> navigation_start_to_now) {
  // In case we don't have a valid |navigation_start_to_click_|, the best we
  // could do is to use |navigation_start_to_now|. It may cause some
  // inconsistency in the measurements but it is better than not recording it.
  if (!navigation_start_to_click_.has_value()) {
    navigation_start_to_click_ = navigation_start_to_now;
  }

  auto get_max_time_ms = [this](auto const& max_time,
                                auto const last_navigation_start_to) {
    int64_t max_time_ms = -1;
    if (last_navigation_start_to.has_value() &&
        navigation_start_to_click_.has_value()) {
      max_time_ms = std::max(max_time_ms, (navigation_start_to_click_.value() -
                                           last_navigation_start_to.value())
                                              .InMilliseconds());
    }
    if (max_time.has_value()) {
      max_time_ms = std::max(max_time_ms, max_time.value().InMilliseconds());
    }
    return max_time_ms;
  };

  auto* ukm_recorder = ukm::UkmRecorder::Get();

  for (const auto& [anchor_index, user_interaction] : user_interactions_) {
    ukm::builders::NavigationPredictorUserInteractions builder(ukm_source_id);
    builder.SetAnchorIndex(anchor_index);
    builder.SetIsInViewport(user_interaction.is_in_viewport);
    builder.SetPointerHoveringOverCount(ukm::GetExponentialBucketMin(
        user_interaction.pointer_hovering_over_count, 1.3));
    builder.SetIsPointerHoveringOver(user_interaction.is_hovered);
    builder.SetMaxEnteredViewportToLeftViewportMs(ukm::GetExponentialBucketMin(
        get_max_time_ms(
            user_interaction.max_time_in_viewport,
            user_interaction.last_navigation_start_to_entered_viewport),
        1.3));
    builder.SetMaxHoverDwellTimeMs(ukm::GetExponentialBucketMin(
        get_max_time_ms(user_interaction.max_hover_dwell_time,
                        user_interaction.last_navigation_start_to_pointer_over),
        1.3));
    builder.Record(ukm_recorder);
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(
    PageAnchorsMetricsObserver::UserInteractionsData);

void PageAnchorsMetricsObserver::RecordUserInteractionDataToUkm() {
  PageAnchorsMetricsObserver::UserInteractionsData* data =
      PageAnchorsMetricsObserver::UserInteractionsData::FromWebContents(
          web_contents_);
  if (!data) {
    return;
  }
  auto ukm_source_id = GetDelegate().GetPageUkmSourceId();
  absl::optional<base::TimeDelta> navigation_start_to_now;
  const base::TimeTicks navigation_start_time =
      GetDelegate().GetNavigationStart();
  if (!navigation_start_time.is_null()) {
    navigation_start_to_now = base::TimeTicks::Now() - navigation_start_time;
  }
  data->RecordUserInteractionMetrics(ukm_source_id, navigation_start_to_now);
}

void PageAnchorsMetricsObserver::RecordAnchorDataToUkm() {
  PageAnchorsMetricsObserver::AnchorsData* data =
      PageAnchorsMetricsObserver::AnchorsData::FromWebContents(web_contents_);
  if (!data || data->number_of_anchors_ == 0) {
    // NavigationPredictor did not record any anchor data, don't log anything.
    return;
  }
  ukm::builders::NavigationPredictorPageLinkMetrics builder(
      GetDelegate().GetPageUkmSourceId());
  builder.SetMedianLinkLocation(data->MedianLinkLocation());
  builder.SetNumberOfAnchors_ContainsImage(
      data->number_of_anchors_contains_image_);
  builder.SetNumberOfAnchors_InIframe(data->number_of_anchors_in_iframe_);
  builder.SetNumberOfAnchors_SameHost(data->number_of_anchors_same_host_);
  builder.SetNumberOfAnchors_Total(data->number_of_anchors_);
  builder.SetNumberOfAnchors_URLIncremented(
      data->number_of_anchors_url_incremented_);
  builder.SetTotalClickableSpace(data->total_clickable_space_);
  builder.SetViewport_Height(data->viewport_height_);
  builder.SetViewport_Width(data->viewport_width_);
  builder.Record(ukm::UkmRecorder::Get());
  // Clear the AnchorsData for the next page load.
  data->Clear();
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
PageAnchorsMetricsObserver::OnPrerenderStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  is_in_prerendered_page_ = true;
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
PageAnchorsMetricsObserver::OnFencedFramesStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // This class is interested only in the primary page's end of life timing,
  // and doesn't need to continue observing FencedFrame pages.
  return STOP_OBSERVING;
}

void PageAnchorsMetricsObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming&) {
  // Do not report Ukm while prerendering.
  if (is_in_prerendered_page_)
    return;

  RecordAnchorDataToUkm();
  RecordUserInteractionDataToUkm();
}
page_load_metrics::PageLoadMetricsObserver::ObservePolicy
PageAnchorsMetricsObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming&) {
  // Do not report Ukm while prerendering.
  if (is_in_prerendered_page_)
    return CONTINUE_OBSERVING;

  RecordAnchorDataToUkm();
  RecordUserInteractionDataToUkm();
  return STOP_OBSERVING;
}

void PageAnchorsMetricsObserver::DidActivatePrerenderedPage(
    content::NavigationHandle* navigation_handle) {
  DCHECK(is_in_prerendered_page_);
  is_in_prerendered_page_ = false;
}
