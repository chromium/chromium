// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DINO_DINO_METRICS_H_
#define CHROME_BROWSER_DINO_DINO_METRICS_H_

#include "ui/base/page_transition_types.h"

namespace dino {

inline constexpr char kDinoImpressionSourceHistogram[] =
    "WebUI.Dino.ImpressionSource";

// Impression sources (reasons the user saw the Chrome Dino game page).
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(DinoImpressionSource)
enum class DinoImpressionSource {
  kTyped = 0,
  kOmniboxGenerated = 1,
  kLink = 2,
  kOther = 3,
  kNetworkErrorPage = 4,
  kMaxValue = kNetworkErrorPage,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/ui/enums.xml:DinoImpressionSource)

// Records the impression source for a Dino game page load.
void RecordDinoImpressionSource(DinoImpressionSource source);

// Maps a `ui::PageTransition` from an intentional `chrome://dino` navigation
// to its corresponding `DinoImpressionSource` bucket.
DinoImpressionSource GetDinoImpressionSourceFromTransition(
    ui::PageTransition transition);

}  // namespace dino

#endif  // CHROME_BROWSER_DINO_DINO_METRICS_H_
