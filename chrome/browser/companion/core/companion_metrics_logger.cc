// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/companion/core/companion_metrics_logger.h"

#include <algorithm>

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "chrome/browser/companion/core/constants.h"
#include "chrome/browser/companion/core/features.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "url/gurl.h"

namespace companion {
namespace {

// The ceiling to use when clamping the number of child elements that a list
// surface can have for UMA/UKM collection.
const int kMaxNumChildElements = 10;

// The max count of text queries to report up to for UKM.
const int kMaxNumTextSearches = 10;

// Bucket spacing param to add noise to |VisualSuggestionsMetrics|.
const double kBucketSpacing = 1.3;

// Helper method to determine whether a UI surface is a list surface. List
// surfaces are surfaces that take the form of a list with one or more items
// inside it, e.g page entities.
bool IsListSurface(UiSurface ui_surface) {
  switch (ui_surface) {
    case UiSurface::kUnknown:
    case UiSurface::kSearchBox:
    case UiSurface::kRegionSearch:
    case UiSurface::kATX:
    case UiSurface::kPH:
      return false;
    case UiSurface::kCQ:
    case UiSurface::kVQ:
    case UiSurface::kRelQr:
    case UiSurface::kRelQs:
    case UiSurface::kPageEntities:
    case UiSurface::kPHResult:
      return true;
  }
}

std::string UiSurfaceToHistogramVariant(UiSurface ui_surface) {
  switch (ui_surface) {
    case UiSurface::kUnknown:
      NOTREACHED_IN_MIGRATION();
      return "Unknown";
    case UiSurface::kPH:
      return "PH";
    case UiSurface::kCQ:
      return "CQ";
    case UiSurface::kRegionSearch:
      return "RegionSearch";
    case UiSurface::kSearchBox:
      return "SearchBox";
    case UiSurface::kVQ:
      return "VQ";
    case UiSurface::kRelQr:
      return "RelQr";
    case UiSurface::kRelQs:
      return "RelQs";
    case UiSurface::kPageEntities:
      return "PageEntities";
    case UiSurface::kATX:
      return "ATX";
    case UiSurface::kPHResult:
      return "PHResult";
    default:
      NOTREACHED_IN_MIGRATION();
      return "Unknown";
  }
}

PromoEvent ToPromoEventEnum(PromoType promo_type, PromoAction promo_action) {
  if (promo_type == PromoType::kSignin) {
    if (promo_action == PromoAction::kShown) {
      return PromoEvent::kSignInShown;
    }
    if (promo_action == PromoAction::kAccepted) {
      return PromoEvent::kSignInAccepted;
    }
    if (promo_action == PromoAction::kRejected) {
      return PromoEvent::kSignInRejected;
    }
  }
  if (promo_type == PromoType::kMsbb) {
    if (promo_action == PromoAction::kShown) {
      return PromoEvent::kMsbbShown;
    }
    if (promo_action == PromoAction::kAccepted) {
      return PromoEvent::kMsbbAccepted;
    }
    if (promo_action == PromoAction::kRejected) {
      return PromoEvent::kMsbbRejected;
    }
  }
  if (promo_type == PromoType::kExps) {
    if (promo_action == PromoAction::kShown) {
      return PromoEvent::kExpsShown;
    }
    if (promo_action == PromoAction::kAccepted) {
      return PromoEvent::kExpsAccepted;
    }
    if (promo_action == PromoAction::kRejected) {
      return PromoEvent::kExpsRejected;
    }
  }
  if (promo_type == PromoType::kPco) {
    if (promo_action == PromoAction::kShown) {
      return PromoEvent::kPcoShown;
    }
    if (promo_action == PromoAction::kAccepted) {
      return PromoEvent::kPcoAccepted;
    }
    if (promo_action == PromoAction::kRejected) {
      return PromoEvent::kPcoRejected;
    }
  }
  return PromoEvent::kUnknown;
}

}  // namespace

CompanionMetricsLogger::CompanionMetricsLogger(ukm::SourceId ukm_source_id)
    : ukm_source_id_(ukm_source_id) {}

CompanionMetricsLogger::~CompanionMetricsLogger() {
  FlushStats();
}

void CompanionMetricsLogger::RecordOpenTrigger(
    std::optional<SidePanelOpenTrigger> open_trigger) {
  open_trigger_ = open_trigger;
  if (open_trigger.has_value()) {
    base::UmaHistogramEnumeration("Companion.SidePanel.OpenTrigger",
                                  open_trigger.value());
  }
}

void CompanionMetricsLogger::RecordUiSurfaceShown(
    UiSurface ui_surface,
    int32_t ui_surface_position,
    int32_t child_element_available_count,
    int32_t child_element_shown_count) {
  UiSurfaceMetrics& surface = ui_surface_metrics_[ui_surface];
  surface.last_event = UiEvent::kShown;
  // Clamped to record as having max 10 child elements.
  surface.ui_surface_position = ui_surface_position;
  surface.child_element_available_count =
      std::clamp(child_element_available_count, 0, kMaxNumChildElements);
  surface.child_element_shown_count =
      std::clamp(child_element_shown_count, 0, kMaxNumChildElements);

  DCHECK(!IsListSurface(ui_surface) || child_element_shown_count > 0);
  base::UmaHistogramBoolean(
      "Companion." + UiSurfaceToHistogramVariant(ui_surface) + ".Shown", true);
  // For surfaces that aren't in the form of a list, the child element count
  // should be -1. Regardless, don't record histograms when the count is -1.
  if (!IsListSurface(ui_surface) || child_element_shown_count < 0) {
    return;
  }
  base::UmaHistogramExactLinear(
      "Companion." + UiSurfaceToHistogramVariant(ui_surface) +
          ".ChildElementCount",
      surface.child_element_shown_count, kMaxNumChildElements);
}

void CompanionMetricsLogger::RecordUiSurfaceClicked(UiSurface ui_surface,
                                                    int32_t click_position) {
  UiSurfaceMetrics& surface = ui_surface_metrics_[ui_surface];
  surface.last_event = UiEvent::kClicked;
  surface.click_count++;
  surface.click_position = click_position;

  base::UmaHistogramBoolean(
      "Companion." + UiSurfaceToHistogramVariant(ui_surface) + ".Clicked",
      true);
  if (!IsListSurface(ui_surface)) {
    return;
  }

  base::UmaHistogramExactLinear(
      "Companion." + UiSurfaceToHistogramVariant(ui_surface) + ".ClickPosition",
      click_position, kMaxNumChildElements);
}

void CompanionMetricsLogger::OnPromoAction(PromoType promo_type,
                                           PromoAction promo_action) {
  last_promo_event_ = ToPromoEventEnum(promo_type, promo_action);
  base::UmaHistogramEnumeration("Companion.PromoEvent",
                                last_promo_event_.value());
}

void CompanionMetricsLogger::OnPhFeedback(PhFeedback ph_feedback) {
  last_ph_feedback_ = ph_feedback;
}

void CompanionMetricsLogger::OnExpsOptInStatusAvailable(
    bool is_exps_opted_in) const {
  base::UmaHistogramBoolean("Companion.IsUserOptedInToExps", is_exps_opted_in);
}

void CompanionMetricsLogger::OnVisualSuggestionsResult(
    const VisualSuggestionsMetrics& metrics) {
  visual_suggestions_ = metrics;
  visual_suggestions_->eligible_count =
      ukm::GetExponentialBucketMin(metrics.eligible_count, kBucketSpacing);
  visual_suggestions_->shoppy_count =
      ukm::GetExponentialBucketMin(metrics.shoppy_count, kBucketSpacing);
  visual_suggestions_->sensitive_count =
      ukm::GetExponentialBucketMin(metrics.sensitive_count, kBucketSpacing);
  visual_suggestions_->shoppy_nonsensitive_count = ukm::GetExponentialBucketMin(
      metrics.shoppy_nonsensitive_count, kBucketSpacing);
  visual_suggestions_->results_count =
      ukm::GetExponentialBucketMin(metrics.results_count, kBucketSpacing);
}

void CompanionMetricsLogger::OnServerSideUrlFilterEvent() {
  base::UmaHistogramBoolean("Companion.ServerSideUrlFilterEvent", true);
}

void CompanionMetricsLogger::FlushStats() {
  ukm::builders::Companion_PageView ukm_builder(ukm_source_id_);

  // Open trigger.
  if (open_trigger_.has_value()) {
    ukm_builder.SetOpenTrigger(static_cast<int>(open_trigger_.value()));
  }

  // Text search.
  auto iter = ui_surface_metrics_.find(UiSurface::kSearchBox);
  if (iter != ui_surface_metrics_.end()) {
    ukm_builder.SetTextSearchCount(
        std::clamp(iter->second.click_count, 0, kMaxNumTextSearches));
  }

  // Region search.
  iter = ui_surface_metrics_.find(UiSurface::kRegionSearch);
  if (iter != ui_surface_metrics_.end()) {
    ukm_builder.SetRegionSearch_ClickCount(iter->second.click_count);
  }

  // Promo state.
  if (last_promo_event_.has_value()) {
    ukm_builder.SetPromoEvent(static_cast<int>(last_promo_event_.value()));
  }

  // PH surface shown before requesting PH to be genenerated.
  iter = ui_surface_metrics_.find(UiSurface::kPH);
  if (iter != ui_surface_metrics_.end()) {
    ukm_builder.SetPH_ComponentPosition(iter->second.ui_surface_position);
    ukm_builder.SetPH_LastEvent(static_cast<int64_t>(iter->second.last_event));
  }

  // PHResult surface shown after PH request was made by clicking the
  // generate button.
  iter = ui_surface_metrics_.find(UiSurface::kPHResult);
  if (iter != ui_surface_metrics_.end()) {
    ukm_builder.SetPHResult_LastEvent(
        static_cast<int64_t>(iter->second.last_event));
    ukm_builder.SetPHResult_ComponentPosition(iter->second.ui_surface_position);
    ukm_builder.SetPHResult_NumEntriesAvailable(
        iter->second.child_element_available_count);
    ukm_builder.SetPHResult_NumEntriesShown(
        iter->second.child_element_shown_count);

    auto click_position = iter->second.click_position;
    if (click_position != kInvalidPosition) {
      ukm_builder.SetPHResult_ClickPosition(click_position);
    }
  }

  // PH feedback.
  if (last_ph_feedback_.has_value()) {
    ukm_builder.SetPH_Feedback(static_cast<int>(last_ph_feedback_.value()));
    base::UmaHistogramEnumeration("Companion.PHFeedback.Result",
                                  last_ph_feedback_.value());
  }

  if (visual_suggestions_.has_value()) {
    ukm_builder.SetVQS_VisualSearchTriggeredCount(
        static_cast<unsigned int>(visual_suggestions_.value().results_count));
    ukm_builder.SetVQS_VisualEligibleImagesCount(
        static_cast<unsigned int>(visual_suggestions_.value().eligible_count));
    ukm_builder.SetVQS_ImageSensitiveCount(
        static_cast<unsigned int>(visual_suggestions_.value().sensitive_count));
    ukm_builder.SetVQS_ImageShoppyCount(
        static_cast<unsigned int>(visual_suggestions_.value().shoppy_count));
    ukm_builder.SetVQS_ImageShoppyNotSensitiveCount(static_cast<unsigned int>(
        visual_suggestions_.value().shoppy_nonsensitive_count));
  }

  // CQ surface.
  iter = ui_surface_metrics_.find(UiSurface::kCQ);
  if (iter != ui_surface_metrics_.end()) {
    ukm_builder.SetCQ_LastEvent(static_cast<int64_t>(iter->second.last_event));
    ukm_builder.SetCQ_ComponentPosition(iter->second.ui_surface_position);
    ukm_builder.SetCQ_NumEntriesAvailable(
        iter->second.child_element_available_count);
    ukm_builder.SetCQ_NumEntriesShown(iter->second.child_element_shown_count);

    auto click_position = iter->second.click_position;
    if (click_position != kInvalidPosition) {
      ukm_builder.SetCQ_ClickPosition(click_position);
    }
  }

  // VQ surface.
  iter = ui_surface_metrics_.find(UiSurface::kVQ);
  if (iter != ui_surface_metrics_.end()) {
    ukm_builder.SetVQ_LastEvent(static_cast<int64_t>(iter->second.last_event));
    ukm_builder.SetVQ_ComponentPosition(iter->second.ui_surface_position);
    ukm_builder.SetVQ_NumEntriesAvailable(
        iter->second.child_element_available_count);
    ukm_builder.SetVQ_NumEntriesShown(iter->second.child_element_shown_count);

    auto click_position = iter->second.click_position;
    if (click_position != kInvalidPosition) {
      ukm_builder.SetVQ_ClickPosition(click_position);
    }
  }

  // RelQr surface.
  iter = ui_surface_metrics_.find(UiSurface::kRelQr);
  if (iter != ui_surface_metrics_.end()) {
    ukm_builder.SetRelQr_LastEvent(
        static_cast<int64_t>(iter->second.last_event));
    ukm_builder.SetRelQr_ComponentPosition(iter->second.ui_surface_position);
    ukm_builder.SetRelQr_NumEntriesAvailable(
        iter->second.child_element_available_count);
    ukm_builder.SetRelQr_NumEntriesShown(
        iter->second.child_element_shown_count);

    auto click_position = iter->second.click_position;
    if (click_position != kInvalidPosition) {
      ukm_builder.SetRelQr_ClickPosition(click_position);
    }
  }

  // RelQs surface.
  iter = ui_surface_metrics_.find(UiSurface::kRelQs);
  if (iter != ui_surface_metrics_.end()) {
    ukm_builder.SetRelQs_LastEvent(
        static_cast<int64_t>(iter->second.last_event));
    ukm_builder.SetRelQs_ComponentPosition(iter->second.ui_surface_position);
    ukm_builder.SetRelQs_NumEntriesAvailable(
        iter->second.child_element_available_count);
    ukm_builder.SetRelQs_NumEntriesShown(
        iter->second.child_element_shown_count);

    auto click_position = iter->second.click_position;
    if (click_position != kInvalidPosition) {
      ukm_builder.SetRelQs_ClickPosition(click_position);
    }
  }

  // Page entities surface.
  iter = ui_surface_metrics_.find(UiSurface::kPageEntities);
  if (iter != ui_surface_metrics_.end()) {
    ukm_builder.SetPageEntities_LastEvent(
        static_cast<int64_t>(iter->second.last_event));
    ukm_builder.SetPageEntities_ComponentPosition(
        iter->second.ui_surface_position);
    ukm_builder.SetPageEntities_NumEntriesAvailable(
        iter->second.child_element_available_count);
    ukm_builder.SetPageEntities_NumEntriesShown(
        iter->second.child_element_shown_count);

    auto click_position = iter->second.click_position;
    if (click_position != kInvalidPosition) {
      ukm_builder.SetPageEntities_ClickPosition(click_position);
    }
  }

  // ATX surface.
  iter = ui_surface_metrics_.find(UiSurface::kATX);
  if (iter != ui_surface_metrics_.end()) {
    ukm_builder.SetATX_LastEvent(static_cast<int64_t>(iter->second.last_event));
    ukm_builder.SetATX_ComponentPosition(iter->second.ui_surface_position);
  }

  ukm_builder.Record(ukm::UkmRecorder::Get());
}

}  // namespace companion
