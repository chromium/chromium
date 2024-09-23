// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "navigation_predictor_metrics_document_data.h"

#include <algorithm>

#include "chrome/browser/navigation_predictor/navigation_predictor_metrics_document_data.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"

namespace {

std::optional<ukm::SourceId> GetUkmSourceId(
    content::RenderFrameHost* render_frame_host) {
  if (render_frame_host->IsInLifecycleState(
          content::RenderFrameHost::LifecycleState::kPrerendering)) {
    // We don't collect UKM while prerendering due to data collection policy.
    return std::nullopt;
  }
  return render_frame_host->GetPageUkmSourceId();
}

}  // namespace

NavigationPredictorMetricsDocumentData::UserInteractionsData::
    UserInteractionsData() = default;
NavigationPredictorMetricsDocumentData::UserInteractionsData::
    UserInteractionsData(const UserInteractionsData&) = default;
NavigationPredictorMetricsDocumentData::UserInteractionsData&
NavigationPredictorMetricsDocumentData::UserInteractionsData::operator=(
    const UserInteractionsData&) = default;

NavigationPredictorMetricsDocumentData::NavigationPredictorMetricsDocumentData(
    content::RenderFrameHost* render_frame_host)
    : DocumentUserData<NavigationPredictorMetricsDocumentData>(
          render_frame_host),
      ukm_source_id_(GetUkmSourceId(render_frame_host)) {}

NavigationPredictorMetricsDocumentData::
    ~NavigationPredictorMetricsDocumentData() {
  if (ukm_source_id_.has_value()) {
    RecordDataToUkm(ukm_source_id_.value());
  }
}
DOCUMENT_USER_DATA_KEY_IMPL(NavigationPredictorMetricsDocumentData);

NavigationPredictorMetricsDocumentData::AnchorsData::AnchorsData() = default;
NavigationPredictorMetricsDocumentData::AnchorsData::~AnchorsData() = default;

int NavigationPredictorMetricsDocumentData::AnchorsData::MedianLinkLocation() {
  DCHECK(!link_locations_.empty());
  size_t median_idx = link_locations_.size() / 2;
  std::nth_element(link_locations_.begin(),
                   link_locations_.begin() + median_idx, link_locations_.end());
  int median = link_locations_[median_idx];
  if (link_locations_.size() % 2 == 0) {
    auto median2_it = std::max_element(link_locations_.begin(),
                                       link_locations_.begin() + median_idx);
    return (median + *median2_it) * 50;
  }
  return median * 100;
}

void NavigationPredictorMetricsDocumentData::RecordAnchorData(
    ukm::SourceId ukm_source_id) {
  if (!ukm_source_id_.has_value() || anchor_data_.number_of_anchors_ == 0) {
    // NavigationPredictor did not record any anchor data, don't log anything.
    return;
  }
  DCHECK(ukm_source_id == ukm_source_id_);
  auto* ukm_recorder = ukm::UkmRecorder::Get();

  ukm::builders::NavigationPredictorPageLinkMetrics builder(
      ukm_source_id_.value());
  builder.SetMedianLinkLocation(anchor_data_.MedianLinkLocation());
  builder.SetNumberOfAnchors_ContainsImage(
      anchor_data_.number_of_anchors_contains_image_);
  builder.SetNumberOfAnchors_InIframe(
      anchor_data_.number_of_anchors_in_iframe_);
  builder.SetNumberOfAnchors_SameHost(
      anchor_data_.number_of_anchors_same_host_);
  builder.SetNumberOfAnchors_Total(anchor_data_.number_of_anchors_);
  builder.SetNumberOfAnchors_URLIncremented(
      anchor_data_.number_of_anchors_url_incremented_);
  builder.SetTotalClickableSpace(anchor_data_.total_clickable_space_);
  builder.SetViewport_Height(anchor_data_.viewport_height_);
  builder.SetViewport_Width(anchor_data_.viewport_width_);
  builder.Record(ukm_recorder);
  // `AnchorData` should persist and be logged again in case of BFCache
  // navigation.
}

void NavigationPredictorMetricsDocumentData::AddPageLinkClickData(
    PageLinkClickData data) {
  page_link_clicks_.push_back(std::move(data));
}
void NavigationPredictorMetricsDocumentData::ClearPageLinkClickData() {
  page_link_clicks_.clear();
}
void NavigationPredictorMetricsDocumentData::RecordPageLinkClickData(
    ukm::SourceId ukm_source_id) {
  if (!ukm_source_id_.has_value() || page_link_clicks_.empty()) {
    return;
  }
  DCHECK(ukm_source_id == ukm_source_id_);
  auto* ukm_recorder = ukm::UkmRecorder::Get();

  for (const auto& page_link_click : page_link_clicks_) {
    ukm::builders::NavigationPredictorPageLinkClick builder(
        ukm_source_id_.value());
    builder.SetAnchorElementIndex(page_link_click.anchor_element_index_);
    if (page_link_click.href_unchanged_.has_value()) {
      builder.SetHrefUnchanged(page_link_click.href_unchanged_.value());
    }
    builder.SetNavigationStartToLinkClickedMs(ukm::GetExponentialBucketMin(
        page_link_click.navigation_start_to_link_clicked_.InMilliseconds(),
        1.3));
    builder.Record(ukm_recorder);
  }
  // Clear the PageLinkClickData for the next page load.
  ClearPageLinkClickData();
}

NavigationPredictorMetricsDocumentData::AnchorElementMetricsData::
    AnchorElementMetricsData() = default;
NavigationPredictorMetricsDocumentData::AnchorElementMetricsData::
    AnchorElementMetricsData(AnchorElementMetricsData&&) = default;

NavigationPredictorMetricsDocumentData::AnchorElementMetricsData&
NavigationPredictorMetricsDocumentData::AnchorElementMetricsData::operator=(
    AnchorElementMetricsData&&) = default;

void NavigationPredictorMetricsDocumentData::AddAnchorElementMetricsData(
    int anchor_index,
    AnchorElementMetricsData data) {
  if (!ukm_source_id_.has_value()) {
    return;
  }
  RecordAnchorElementMetricsData(anchor_index, data);
  anchor_element_metrics_[anchor_index] = std::move(data);
}

void NavigationPredictorMetricsDocumentData::RecordAnchorElementMetricsData(
    int anchor_index,
    const AnchorElementMetricsData& metrics) {
  DCHECK(ukm_source_id_.has_value());
  auto* ukm_recorder = ukm::UkmRecorder::Get();

  ukm::builders::NavigationPredictorAnchorElementMetrics builder(
      ukm_source_id_.value());
  builder.SetAnchorIndex(anchor_index);
  builder.SetIsInIframe(metrics.is_in_iframe_);
  builder.SetIsURLIncrementedByOne(metrics.is_url_incremented_by_one_);
  builder.SetContainsImage(metrics.contains_image_);
  builder.SetSameOrigin(metrics.is_same_host_);
  builder.SetHasTextSibling(metrics.has_text_sibling_);
  builder.SetIsBold(metrics.is_bold_);
  builder.SetNavigationStartToLinkLoggedMs(ukm::GetExponentialBucketMin(
      metrics.navigation_start_to_link_logged.InMilliseconds(), 1.3));
  builder.SetFontSize(metrics.font_size_bucket_);
  builder.SetPathLength(metrics.path_length_);
  builder.SetPathDepth(metrics.path_depth_);
  builder.SetBucketedPathHash(metrics.bucketed_path_hash_);
  builder.SetPercentClickableArea(metrics.percent_clickable_area_);
  builder.SetPercentVerticalDistance(metrics.percent_vertical_distance_);

  builder.Record(ukm_recorder);
}

void NavigationPredictorMetricsDocumentData::RecordAnchorElementMetricsData(
    ukm::SourceId ukm_source_id) {
  if (!ukm_source_id_.has_value() || anchor_element_metrics_.empty()) {
    return;
  }
  DCHECK(ukm_source_id == ukm_source_id_);

  for (const auto& [anchor_index, metrics] : anchor_element_metrics_) {
    RecordAnchorElementMetricsData(anchor_index, metrics);
  }
}

void NavigationPredictorMetricsDocumentData::AddUserInteractionsData(
    int anchor_index,
    UserInteractionsData data) {
  user_interactions_[anchor_index] = std::move(data);
}

void NavigationPredictorMetricsDocumentData::ClearUserInteractionsData() {
  user_interactions_.clear();
  navigation_start_to_click_.reset();
}

void NavigationPredictorMetricsDocumentData::RecordUserInteractionsData(
    ukm::SourceId ukm_source_id) {
  if (!ukm_source_id_.has_value() || user_interactions_.empty()) {
    return;
  }
  DCHECK(ukm_source_id == ukm_source_id_);

  std::optional<base::TimeDelta> navigation_start_to_now;
  if (!navigation_start_time_.is_null()) {
    navigation_start_to_now = base::TimeTicks::Now() - navigation_start_time_;
  }
  // In case we don't have a valid |navigation_start_to_click_|, the best we
  // could do is to use |navigation_start_to_now|. It may cause some
  // inconsistency in the measurements but it is better than not recording it.
  if (!navigation_start_to_click_.has_value()) {
    navigation_start_to_click_ = navigation_start_to_now;
  }

  auto get_max_time_ms =
      [this](const std::optional<base::TimeDelta>& max_time,
             const std::optional<base::TimeDelta>& last_navigation_start_to) {
        int64_t max_time_ms = -1;
        if (last_navigation_start_to.has_value() &&
            navigation_start_to_click_.has_value()) {
          max_time_ms =
              std::max(max_time_ms, (navigation_start_to_click_.value() -
                                     last_navigation_start_to.value())
                                        .InMilliseconds());
        }
        if (max_time.has_value()) {
          max_time_ms =
              std::max(max_time_ms, max_time.value().InMilliseconds());
        }
        return max_time_ms;
      };

  auto* ukm_recorder = ukm::UkmRecorder::Get();
  auto get_exponential_bucket_for_signed_values = [](int64_t sample,
                                                     double bucket_spacing) {
    return ukm::GetExponentialBucketMin(std::abs(sample), bucket_spacing) *
           (sample >= 0 ? 1 : -1);
  };

  for (const auto& [anchor_index, user_interaction] : user_interactions_) {
    ukm::builders::NavigationPredictorUserInteractions builder(ukm_source_id);
    builder.SetAnchorIndex(anchor_index);
    builder.SetIsInViewport(user_interaction.is_in_viewport);
    builder.SetPointerHoveringOverCount(ukm::GetExponentialBucketMin(
        user_interaction.pointer_hovering_over_count, 1.3));
    builder.SetEnteredViewportCount(ukm::GetExponentialBucketMin(
        user_interaction.entered_viewport_count, 1.3));
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
    builder.SetMouseVelocity(get_exponential_bucket_for_signed_values(
        user_interaction.mouse_velocity.value_or(0.0), 1.3));
    builder.SetMouseAcceleration(get_exponential_bucket_for_signed_values(
        user_interaction.mouse_acceleration.value_or(0.0), 1.3));
    if (user_interaction.percent_vertical_position.has_value()) {
      int64_t value = user_interaction.percent_vertical_position.value();
      int64_t bucketed_value = ukm::GetLinearBucketMin(value, 10);
      builder.SetVerticalPositionInViewport(
          std::clamp<int64_t>(bucketed_value, 0, 100));
    }
    if (user_interaction.percent_distance_from_pointer_down.has_value()) {
      // |value| is typically between -100 and 100, but there may be outliers.
      int64_t value =
          user_interaction.percent_distance_from_pointer_down.value();
      int64_t bucketed_value = ukm::GetLinearBucketMin(value, 10);
      builder.SetDistanceFromLastPointerDown(
          std::clamp<int64_t>(bucketed_value, -100, 100));
    }
    builder.Record(ukm_recorder);
  }
  // Clear the UserInteractionData for the next page load.
  ClearUserInteractionsData();
}

void NavigationPredictorMetricsDocumentData::AddPreloadOnHoverData(
    PreloadOnHoverData data) {
  preload_on_hover_.push_back(std::move(data));
}

void NavigationPredictorMetricsDocumentData::RecordPreloadOnHoverData(
    ukm::SourceId ukm_source_id) {
  if (!ukm_source_id_.has_value() || preload_on_hover_.empty()) {
    return;
  }
  DCHECK(ukm_source_id == ukm_source_id_);

  auto* ukm_recorder = ukm::UkmRecorder::Get();
  for (const auto& on_hover_data : preload_on_hover_) {
    ukm::builders::NavigationPredictorPreloadOnHover builder(
        ukm_source_id_.value());

    if (on_hover_data.taken) {
      if (on_hover_data.hover_dwell_time.has_value()) {
        builder.SetHoverTakenMs(ukm::GetExponentialBucketMin(
            on_hover_data.hover_dwell_time.value().InMilliseconds(), 1.3));
      }
      if (on_hover_data.pointer_down_duration.has_value()) {
        builder.SetMouseDownTakenMs(ukm::GetExponentialBucketMin(
            on_hover_data.pointer_down_duration.value().InMilliseconds(), 1.3));
      }

    } else {
      if (on_hover_data.hover_dwell_time.has_value()) {
        builder.SetHoverNotTakenMs(ukm::GetExponentialBucketMin(
            on_hover_data.hover_dwell_time.value().InMilliseconds(), 1.3));
      }
      if (on_hover_data.pointer_down_duration.has_value()) {
        builder.SetMouseDownNotTakenMs(ukm::GetExponentialBucketMin(
            on_hover_data.pointer_down_duration.value().InMilliseconds(), 1.3));
      }
    }
    builder.Record(ukm_recorder);
  }
  // Clear the data for the next navigation.
  preload_on_hover_.clear();
}

void NavigationPredictorMetricsDocumentData::RecordDataToUkm(
    ukm::SourceId ukm_source_id) {
  // `AnchorElementMetricsData` are already recorded to UKM as we receive them,
  // and we don't need to record them again here. The edge case scenario is
  // handled separately in
  // `PageAnchorsMetricsObserver::OnRestoreFromBackForwardCache`.
  RecordPageLinkClickData(ukm_source_id);
  RecordAnchorData(ukm_source_id);
  RecordUserInteractionsData(ukm_source_id);
  RecordPreloadOnHoverData(ukm_source_id);
}
