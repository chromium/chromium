// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/arc_input_overlay_uma.h"

#include "ash/wm/window_state.h"
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

void RecordInputOverlayActionReposition(
    RepositionType reposition_type,
    InputOverlayWindowStateType state_type) {
  base::UmaHistogramEnumeration(
      "Arc.InputOverlay.ActionRepositionOperationType", reposition_type);
  base::UmaHistogramEnumeration(
      "Arc.InputOverlay.ActionRepositionWindowStateType", state_type);
}

void RecordInputOverlayMenuEntryReposition(
    RepositionType reposition_type,
    InputOverlayWindowStateType state_type) {
  base::UmaHistogramEnumeration(
      "Arc.InputOverlay.MenuEntryRepositionOperationType", reposition_type);
  base::UmaHistogramEnumeration(
      "Arc.InputOverlay.MenuEntryRepositionWindowStateType", state_type);
}

void RecordInputOverlayButtonGroupReposition(
    RepositionType reposition_type,
    InputOverlayWindowStateType state_type) {
  base::UmaHistogramEnumeration(
      "Arc.InputOverlay.ButtonGroupRepositionOperationType", reposition_type);
  base::UmaHistogramEnumeration(
      "Arc.InputOverlay.ButtonGroupRepositionWindowStateType", state_type);
}

}  // namespace arc::input_overlay
