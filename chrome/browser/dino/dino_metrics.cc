// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dino/dino_metrics.h"

#include "base/metrics/histogram_functions.h"

namespace dino {

void RecordDinoHasBookmark(bool has_bookmark) {
  base::UmaHistogramBoolean(kDinoHasBookmarkHistogram, has_bookmark);
}

void RecordDinoImpressionSource(DinoImpressionSource source) {
  base::UmaHistogramEnumeration(kDinoImpressionSourceHistogram, source);
}

DinoImpressionSource GetDinoImpressionSourceFromTransition(
    ui::PageTransition transition) {
  if (ui::PageTransitionCoreTypeIs(transition,
                                   ui::PAGE_TRANSITION_AUTO_BOOKMARK)) {
    return DinoImpressionSource::kBookmark;
  }
  if (ui::PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_TYPED)) {
    return DinoImpressionSource::kTyped;
  }
  if (ui::PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_GENERATED)) {
    return DinoImpressionSource::kOmniboxGenerated;
  }
  if (ui::PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_LINK)) {
    return DinoImpressionSource::kLink;
  }
  return DinoImpressionSource::kOther;
}

}  // namespace dino
