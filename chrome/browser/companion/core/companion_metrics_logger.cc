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

void CompanionMetricsLogger::RecordUiSurfaceShown(
    UiSurface ui_surface,
    uint32_t child_element_count) {
  UiSurfaceMetrics& surface = ui_surface_metrics_[ui_surface];
  surface.last_event = UiEvent::kShown;
  // Clamped to record as having max 10 child elements.
  surface.child_element_count = std::clamp(child_element_count, 0u, 10u);

  base::UmaHistogramBoolean(
      "Companion." + UiSurfaceToHistogramVariant(ui_surface) + ".Shown", true);
}

void CompanionMetricsLogger::RecordUiSurfaceClicked(UiSurface ui_surface) {
  UiSurfaceMetrics& surface = ui_surface_metrics_[ui_surface];
  surface.last_event = UiEvent::kClicked;
  surface.click_count++;

  base::UmaHistogramBoolean(
      "Companion." + UiSurfaceToHistogramVariant(ui_surface) + ".Clicked",
      true);
}

void CompanionMetricsLogger::OnPromoAction(PromoType promo_type,
                                           PromoAction promo_action) {
  last_promo_event_ = ToPromoEventEnum(promo_type, promo_action);
  base::UmaHistogramEnumeration("Companion.PromoEvent",
                                last_promo_event_.value());
}

void CompanionMetricsLogger::FlushStats() {
  ukm::builders::Companion_PageView ukm_builder(ukm_source_id_);

  // CQ surface.
  auto iter = ui_surface_metrics_.find(UiSurface::kCQ);
  if (iter != ui_surface_metrics_.end()) {
    ukm_builder.SetCQ_LastEvent(static_cast<int64_t>(iter->second.last_event));
    ukm_builder.SetCQ_ChildElementCount(iter->second.child_element_count);
  }

  // PH surface.
  iter = ui_surface_metrics_.find(UiSurface::kPH);
  if (iter != ui_surface_metrics_.end()) {
    ukm_builder.SetPH_LastEvent(static_cast<int64_t>(iter->second.last_event));
  }

  // Region search.
  iter = ui_surface_metrics_.find(UiSurface::kPH);
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
