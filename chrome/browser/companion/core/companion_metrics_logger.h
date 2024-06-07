// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPANION_CORE_COMPANION_METRICS_LOGGER_H_
#define CHROME_BROWSER_COMPANION_CORE_COMPANION_METRICS_LOGGER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/companion/core/mojom/companion.mojom.h"
#include "chrome/browser/ui/views/side_panel/side_panel_enums.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "url/gurl.h"

using side_panel::mojom::PhFeedback;
using side_panel::mojom::PromoAction;
using side_panel::mojom::PromoType;
using side_panel::mojom::UiSurface;

namespace companion {

// Invalid values.
const int32_t kInvalidPosition = -1;
const int32_t kInvalidNumChildren = -1;

// Types of events on the UI surfaces. Keep in sync with Companion.UiEvent in
// enums.xml.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class UiEvent {
  // The UI surface was not shown.
  kNotAvailable = 1,

  // The UI surface was shown.
  kShown = 2,

  // User clicked on the UI surface.
  kClicked = 3,
};

// Tracks events happening on a single UI surface.
struct UiSurfaceMetrics {
  UiSurfaceMetrics() = default;
  UiSurfaceMetrics(const UiSurfaceMetrics& other) = default;
  UiSurfaceMetrics& operator=(const UiSurfaceMetrics& other) = default;
  ~UiSurfaceMetrics() = default;

  // Events on the surface. The last event wins and gets recorded.
  UiEvent last_event = UiEvent::kNotAvailable;

  // The position of the UI surface relative to the companion page.
  int ui_surface_position = kInvalidPosition;

  // The number of child elements that were considered to be shown within the
  // surface, e.g. number of candidate queries inside related queries component.
  int child_element_available_count = kInvalidNumChildren;

  // The number of child elements shown within the surface, e.g. number of
  // related queries inside related queries component.
  int child_element_shown_count = kInvalidNumChildren;

  // The number of times user clicked on the surface.
  int click_count = 0;

  // The position of the clicked UI element within its parent list. Applicable
  // to surfaces that show a list.
  int click_position = kInvalidPosition;
};

// Tracks visual suggestion metrics for a single page.
struct VisualSuggestionsMetrics {
  VisualSuggestionsMetrics() = default;
  VisualSuggestionsMetrics(const VisualSuggestionsMetrics& other) = default;
  VisualSuggestionsMetrics& operator=(const VisualSuggestionsMetrics& other) =
      default;
  ~VisualSuggestionsMetrics() = default;

  // The number of images that pass visual suggestion requirements.
  // This metric is exponientially bucketed (1.3) and rounded for privacy.
  uint32_t results_count;

  // The number of images eligible for visual classification.
  // This metric is exponientially bucketed (1.3) and rounded for privacy.
  uint32_t eligible_count;

  // The number of images classified as sensitive.
  // This metric is exponientially bucketed (1.3) and rounded for privacy.
  uint32_t sensitive_count;

  // The number of images classifier as shoppy.
  // This metric is exponientially bucketed (1.3) and rounded for privacy.
  uint32_t shoppy_count;

  // The number of images classifier as shoppy and nonsensitive.
  // This metric is exponientially bucketed (1.3) and rounded for privacy.
  uint32_t shoppy_nonsensitive_count;
};

// Various types of events happening on the promo surfaces on the companion
// page. Keep in sync with Companion.PromoEvent in enums.xml. These values are
// persisted to logs. Entries should not be renumbered and numeric values should
// never be reused.
enum class PromoEvent {
  kUnknown = 0,
  kSignInShown = 1,
  kSignInAccepted = 2,
  kSignInRejected = 3,
  kMsbbShown = 4,
  kMsbbAccepted = 5,
  kMsbbRejected = 6,
  kExpsShown = 7,
  kExpsAccepted = 8,
  kExpsRejected = 9,
  kPcoShown = 10,
  kPcoAccepted = 11,
  kPcoRejected = 12,
  kMaxValue = kPcoRejected,
};

// Utility to log UKM and UMA metrics for events happening on the companion
// page. Should be associated with a single navigation. Flushes metrics on
// destruction.
class CompanionMetricsLogger {
 public:
  explicit CompanionMetricsLogger(ukm::SourceId ukm_source_id);
  CompanionMetricsLogger(const CompanionMetricsLogger&) = delete;
  CompanionMetricsLogger& operator=(const CompanionMetricsLogger&) = delete;
  ~CompanionMetricsLogger();

  void RecordOpenTrigger(std::optional<SidePanelOpenTrigger> open_trigger);

  // For the following methods, please refer CompanionPageHandler in
  // companion.mojom for detailed documentation.

  // Logging method corresponding to `RecordUiSurfaceShown` in companion.mojom.
  void RecordUiSurfaceShown(UiSurface ui_surface,
                            int32_t ui_surface_position,
                            int32_t child_element_available_count,
                            int32_t child_element_shown_count);

  // Logging method corresponding to `RecordUiSurfaceClicked` in
  // companion.mojom.
  void RecordUiSurfaceClicked(UiSurface ui_surface, int32_t click_position);

  // Logging method corresponding to `OnPromoAction` in companion.mojom.
  void OnPromoAction(PromoType promo_type, PromoAction promo_action);

  // Logging method corresponding to `OnPhFeedback` in companion.mojom.
  void OnPhFeedback(PhFeedback ph_feedback);

  // Logging method recording the status of whether user is opted-in to exps.
  void OnExpsOptInStatusAvailable(bool is_exps_opted_in) const;

  // Logging method corresponding to visual query suggestions.
  void OnVisualSuggestionsResult(const VisualSuggestionsMetrics& metrics);

  // Logging method corresponding to `OnServerSideUrlFilterEvent`
  // in companion.mojom.
  void OnServerSideUrlFilterEvent();

 private:
  // Meant to be called at destruction. Flushes the UKM metrics.
  void FlushStats();

  // The UKM source ID for the page being shown.
  ukm::SourceId ukm_source_id_;

  // In-memory accumulator of UI surface metrics.
  std::map<UiSurface, UiSurfaceMetrics> ui_surface_metrics_;

  // Last event on the promo surfaces.
  std::optional<PromoEvent> last_promo_event_;

  // Last event on the promo surfaces.
  std::optional<PhFeedback> last_ph_feedback_;

  // Indicates how the companion side panel was opened. Non-empty for the first
  // navigation.
  std::optional<SidePanelOpenTrigger> open_trigger_;

  // Stores metrics for visual query suggestions.
  std::optional<VisualSuggestionsMetrics> visual_suggestions_;
};

}  // namespace companion

#endif  // CHROME_BROWSER_COMPANION_CORE_COMPANION_METRICS_LOGGER_H_
