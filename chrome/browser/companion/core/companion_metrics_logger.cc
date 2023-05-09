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
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "url/gurl.h"

namespace companion {
namespace {

// The ceiling to use when clamping the number of child elements that a list
// surface can have for UMA/UKM collection.
const int kMaxNumChildElements = 10;

// Helper method to determine whether a UI surface is a list surface. List
// surfaces are surfaces that take the form of a list with one or more items
// inside it, e.g page entities.
bool IsListSurface(UiSurface ui_surface) {
  switch (ui_surface) {
    case UiSurface::kUnknown:
    case UiSurface::kRegionSearch:
      return false;
    case UiSurface::kCQ:
    case UiSurface::kPH:
      return true;
  }
}

std::string UiSurfaceToHistogramVariant(UiSurface ui_surface) {
  switch (ui_surface) {
    case UiSurface::kUnknown:
      NOTREACHED();
      return "Unknown";
    case UiSurface::kCQ:
      return "CQ";
    case UiSurface::kPH:
      return "PH";
    case UiSurface::kRegionSearch:
      return "RegionSearch";
    default:
      NOTREACHED();
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
  return PromoEvent::kUnknown;
}

}  // namespace

CompanionMetricsLogger::CompanionMetricsLogger(ukm::SourceId ukm_source_id)
    : ukm_source_id_(ukm_source_id) {}

CompanionMetricsLogger::~CompanionMetricsLogger() {
  FlushStats();
}

void CompanionMetricsLogger::RecordOpenTrigger(OpenTrigger open_trigger) {
  open_trigger_ = open_trigger;
}

void CompanionMetricsLogger::RecordUiSurfaceShown(
    UiSurface ui_surface,
    uint32_t child_element_count) {
  UiSurfaceMetrics& surface = ui_surface_metrics_[ui_surface];
  surface.last_event = UiEvent::kShown;
  // Clamped to record as having max 10 child elements.
  surface.child_element_count = std::clamp(
      child_element_count, 0u, static_cast<unsigned int>(kMaxNumChildElements));

  if (child_element_count > 0) {
    base::UmaHistogramBoolean(
        "Companion." + UiSurfaceToHistogramVariant(ui_surface) + ".Shown",
        true);
  }
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

void CompanionMetricsLogger::FlushStats() {
  ukm::builders::Companion_PageView ukm_builder(ukm_source_id_);

  // Open trigger.
  if (open_trigger_.has_value()) {
    ukm_builder.SetOpenTrigger(static_cast<int>(open_trigger_.value()));
  }

  // CQ surface.
  auto iter = ui_surface_metrics_.find(UiSurface::kCQ);
  if (iter != ui_surface_metrics_.end()) {
    ukm_builder.SetCQ_LastEvent(static_cast<int64_t>(iter->second.last_event));
    ukm_builder.SetCQ_ChildElementCount(iter->second.child_element_count);

    auto click_position = iter->second.click_position;
    if (click_position != kInvalidPosition) {
      ukm_builder.SetCQ_ClickPosition(click_position);
    }
  }

  // PH surface.
  iter = ui_surface_metrics_.find(UiSurface::kPH);
  if (iter != ui_surface_metrics_.end()) {
    ukm_builder.SetPH_LastEvent(static_cast<int64_t>(iter->second.last_event));
  }

  // PH feedback.
  if (last_ph_feedback_.has_value()) {
    ukm_builder.SetPH_Feedback(static_cast<int>(last_ph_feedback_.value()));
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

  ukm_builder.Record(ukm::UkmRecorder::Get());
}

}  // namespace companion
