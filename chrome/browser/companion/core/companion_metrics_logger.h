// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPANION_CORE_COMPANION_METRICS_LOGGER_H_
#define CHROME_BROWSER_COMPANION_CORE_COMPANION_METRICS_LOGGER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/companion/core/mojom/companion.mojom.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "url/gurl.h"

using side_panel::mojom::PromoAction;
using side_panel::mojom::PromoType;
using side_panel::mojom::UiSurface;

namespace companion {

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

  // The number of child elements shown within the surface, e.g. number of
  // related queries inside related queries component.
  size_t child_element_count = 0;

  // The number of times user clicked on the surface.
  size_t click_count = 0;
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
  kMaxValue = kExpsRejected,
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

  void RecordUiSurfaceShown(UiSurface ui_surface, uint32_t child_element_count);
  void RecordUiSurfaceClicked(UiSurface ui_surface);
  void OnPromoAction(PromoType promo_type, PromoAction promo_action);

 private:
  // Meant to be called at destruction. Flushes the UKM metrics.
  void FlushStats();

  // The UKM source ID for the page being shown.
  ukm::SourceId ukm_source_id_;

  // In-memory accumulator of UI surface metrics.
  std::map<UiSurface, UiSurfaceMetrics> ui_surface_metrics_;

  // Last event on the promo surfaces.
  absl::optional<PromoEvent> last_promo_event_;
};

}  // namespace companion

#endif  // CHROME_BROWSER_COMPANION_CORE_COMPANION_METRICS_LOGGER_H_
