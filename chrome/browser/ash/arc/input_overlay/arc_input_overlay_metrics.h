// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_ARC_INPUT_OVERLAY_METRICS_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_ARC_INPUT_OVERLAY_METRICS_H_

#include <string>

#include "chrome/browser/ash/arc/input_overlay/constants.h"

namespace arc::input_overlay {

inline constexpr char kEditingListFunctionTriggeredHistogram[] =
    "EditingListFunctionTriggered";
inline constexpr char kButtonOptionsMenuFunctionTriggeredHistogram[] =
    "ButtonOptionsMenuFunctionTriggered";
inline constexpr char kEditDeleteMenuFunctionTriggeredHistogram[] =
    "EditDeleteMenuFuctionTriggered";

inline constexpr char kToggleWithMappingSourceHistogram[] =
    "ToggleWithMappingSource";

inline constexpr char kToggleOnHistogramName[] = "On";
inline constexpr char kToggleOffHistogramName[] = "Off";

inline constexpr char kFeatureHistogramName[] = "Feature";
inline constexpr char kHintHistogramName[] = "Hint";

inline constexpr char kGameControlsHistogramSeparator[] = ".";

// This enum should be kept in sync with the
// `GameControlsButtonOptionsMenuFunction` in
// tools/metrics/histograms/enums.xml.
enum class ButtonOptionsMenuFunction {
  kOptionSingleButton,
  kOptionJoystick,
  kEditLabelFocused,
  kKeyAssigned,
  kDone,
  kDelete,
  kMaxValue = kDelete,
};

// This enum should be kept in sync with the
// `GameControlsEditDeleteMenuFunction` in
// tools/metrics/histograms/enums.xml.
enum class EditDeleteMenuFunction {
  kEdit,
  kDelete,
  kMaxValue = kDelete,
};

// This enum should be kept in sync with the `GameControlsEditingListFunction`
// in tools/metrics/histograms/enums.xml.
enum class EditingListFunction {
  kAdd,
  kDone,
  kHoverListItem,
  kPressListItem,
  kEditLabelFocused,
  kKeyAssigned,
  kMaxValue = kKeyAssigned,
};

// This enum should be kept in sync with the `GameControlsMappingSource`
// in tools/metrics/histograms/enums.xml.
enum class MappingSource {
  kEmpty,
  // Only pre-defined default mapping. May include position change.
  kDefault,
  // Only user-added mapping.
  kUserAdded,
  // Includes default and user-added mapping.
  kDefaultAndUserAdded,
  kMaxValue = kDefaultAndUserAdded,
};

// This enum should be kept in sync with the `GameControlsToggleFunction`
// in tools/metrics/histograms/enums.xml.
enum class GameControlsToggleFunction {
  kFeature,
  kMappingHint,
  kMaxValue = kMappingHint,
};

std::string BuildGameControlsHistogramName(const std::string& name);

std::string BuildGameControlsUkmEventName(const std::string& name);

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

void RecordEditingListFunctionTriggered(const std::string& package_name,
                                        EditingListFunction function);

void RecordButtonOptionsMenuFunctionTriggered(
    const std::string& package_name,
    ButtonOptionsMenuFunction function);

void RecordEditDeleteMenuFunctionTriggered(const std::string& package_name,
                                           EditDeleteMenuFunction function);

// Records feature toggle data if `is_feature` is true. Otherwise, records the
// hint toggle data.
void RecordToggleWithMappingSource(const std::string& package_name,
                                   bool is_feature,
                                   bool is_on,
                                   MappingSource source);

}  // namespace arc::input_overlay

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_ARC_INPUT_OVERLAY_METRICS_H_
