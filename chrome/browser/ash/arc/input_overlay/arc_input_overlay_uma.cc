// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/arc_input_overlay_uma.h"

#include "base/metrics/histogram_functions.h"

namespace arc::input_overlay {

void RecordInputOverlayFeatureState(bool enable) {
  base::UmaHistogramBoolean("Arc.InputOverlay.FeatureState", enable);
}

void RecordInputOverlayMappingHintState(bool enable) {
  base::UmaHistogramBoolean("Arc.InputOverlay.MappingHintState", enable);
}

void RecordInputOverlayCustomizedUsage() {
  base::UmaHistogramBoolean("Arc.InputOverlay.Customized", true);
}

void RecordInputOverlayActionReposition(RepositionType type) {
  base::UmaHistogramEnumeration(
      "Arc.InputOverlay.ActionRepositionOperationType", type);
}

void RecordInputOverlayMenuEntryReposition(RepositionType type) {
  base::UmaHistogramEnumeration(
      "Arc.InputOverlay.MenuEntryRepositionOperationType", type);
}

void RecordInputOverlayButtonGroupReposition(RepositionType type) {
  base::UmaHistogramEnumeration(
      "Arc.InputOverlay.ButtonGroupRepositionOperationType", type);
}

}  // namespace arc::input_overlay
