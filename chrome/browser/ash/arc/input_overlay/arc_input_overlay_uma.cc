// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/arc_input_overlay_uma.h"

#include "ash/wm/window_state.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ash/arc/input_overlay/arc_input_overlay_ukm.h"
#include "ui/aura/client/aura_constants.h"

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
    const std::string& package_name,
    RepositionType reposition_type,
    InputOverlayWindowStateType state_type) {
  base::UmaHistogramEnumeration(
      "Arc.InputOverlay.ActionRepositionOperationType", reposition_type);
  base::UmaHistogramEnumeration(
      "Arc.InputOverlay.ActionRepositionWindowStateType", state_type);
  InputOverlayUkm::RecordInputOverlayActionReposition(
      package_name, reposition_type, state_type);
}

void RecordInputOverlayMenuEntryReposition(
    const std::string& package_name,
    RepositionType reposition_type,
    InputOverlayWindowStateType state_type) {
  base::UmaHistogramEnumeration(
      "Arc.InputOverlay.MenuEntryRepositionOperationType", reposition_type);
  base::UmaHistogramEnumeration(
      "Arc.InputOverlay.MenuEntryRepositionWindowStateType", state_type);
  InputOverlayUkm::RecordInputOverlayMenuEntryReposition(
      package_name, reposition_type, state_type);
}

void RecordInputOverlayButtonGroupReposition(
    const std::string& package_name,
    RepositionType reposition_type,
    InputOverlayWindowStateType state_type) {
  base::UmaHistogramEnumeration(
      "Arc.InputOverlay.ButtonGroupRepositionOperationType", reposition_type);
  base::UmaHistogramEnumeration(
      "Arc.InputOverlay.ButtonGroupRepositionWindowStateType", state_type);
  InputOverlayUkm::RecordInputOverlayButtonGroupReposition(
      package_name, reposition_type, state_type);
}

}  // namespace arc::input_overlay
