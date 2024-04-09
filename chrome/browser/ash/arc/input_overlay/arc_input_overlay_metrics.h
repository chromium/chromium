// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_ARC_INPUT_OVERLAY_METRICS_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_ARC_INPUT_OVERLAY_METRICS_H_

#include <string>

#include "chrome/browser/ash/arc/input_overlay/constants.h"

namespace arc::input_overlay {

// Records whether the feature is on or off.
void RecordInputOverlayFeatureState(const std::string& package_name,
                                    bool enable);

// Records whether the mapping hint is on or off.
void RecordInputOverlayMappingHintState(const std::string& package_name,
                                    bool enable);

// Records whether the overlay is customized.
void RecordInputOverlayCustomizedUsage(const std::string& package_name);

// Record when finishing action dragging or releasing arrow key.
void RecordInputOverlayActionReposition(const std::string& package_name,
                                        RepositionType reposition_type,
                                        InputOverlayWindowStateType state_type);

// Record when finishing menu entry dragging or releasing arrow key.
void RecordInputOverlayMenuEntryReposition(
    const std::string& package_name,
    RepositionType reposition_type,
    InputOverlayWindowStateType state_type);

// Record when finishing button group dragging or releasing arrow key.
void RecordInputOverlayButtonGroupReposition(
    const std::string& package_name,
    RepositionType reposition_type,
    InputOverlayWindowStateType state_type);

}  // namespace arc::input_overlay

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_ARC_INPUT_OVERLAY_METRICS_H_
