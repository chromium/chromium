// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/arc_input_overlay_metrics.h"

#include <cstdint>

#include "ash/wm/window_state.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "components/ukm/app_source_url_recorder.h"
#include "services/metrics/public/cpp/delegating_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "ui/aura/client/aura_constants.h"

namespace arc::input_overlay {

namespace {

constexpr char kGameControlsHistogramNameRoot[] = "Arc.GameControls";
constexpr char kGameControlsUkmEventNameRoot[] = "GameControls";

}  // namespace

class InputOverlayUkm {
 public:
  static void RecordInputOverlayFeatureState(std::string package_name,
                                             bool enable) {
    ukm::builders::GamingInputOverlay_Feature(
        ukm::AppSourceUrlRecorder::GetSourceIdForArcPackageName(package_name))
        .SetFeatureStateOn(enable)
        .Record(ukm::UkmRecorder::Get());
  }

  static void RecordInputOverlayMappingHintState(std::string package_name,
                                                 bool enable) {
    ukm::builders::GamingInputOverlay_MappingHint(
        ukm::AppSourceUrlRecorder::GetSourceIdForArcPackageName(package_name))
        .SetMappingHintStateOn(enable)
        .Record(ukm::UkmRecorder::Get());
  }

  static void RecordInputOverlayCustomizedUsage(std::string package_name) {
    ukm::builders::GamingInputOverlay_Customization(
        ukm::AppSourceUrlRecorder::GetSourceIdForArcPackageName(package_name))
        .SetCustomizationUsed(true)
        .Record(ukm::UkmRecorder::Get());
  }

  static void RecordInputOverlayActionReposition(
      const std::string& package_name,
      RepositionType reposition_type,
      InputOverlayWindowStateType state_type) {
    ukm::builders::GamingInputOverlay_ActionRepositionOperationType(
        ukm::AppSourceUrlRecorder::GetSourceIdForArcPackageName(package_name))
        .SetOperationType(static_cast<int64_t>(reposition_type))
        .Record(ukm::UkmRecorder::Get());
    ukm::builders::GamingInputOverlay_ActionRepositionWindowStateType(
        ukm::AppSourceUrlRecorder::GetSourceIdForArcPackageName(package_name))
        .SetWindowStateType(static_cast<int64_t>(state_type))
        .Record(ukm::UkmRecorder::Get());
  }

  static void RecordInputOverlayMenuEntryReposition(
      const std::string& package_name,
      RepositionType reposition_type,
      InputOverlayWindowStateType state_type) {
    ukm::builders::GamingInputOverlay_MenuEntryRepositionOperationType(
        ukm::AppSourceUrlRecorder::GetSourceIdForArcPackageName(package_name))
        .SetOperationType(static_cast<int64_t>(reposition_type))
        .Record(ukm::UkmRecorder::Get());
    ukm::builders::GamingInputOverlay_MenuEntryRepositionWindowStateType(
        ukm::AppSourceUrlRecorder::GetSourceIdForArcPackageName(package_name))
        .SetWindowStateType(static_cast<int64_t>(state_type))
        .Record(ukm::UkmRecorder::Get());
  }

  static void RecordInputOverlayButtonGroupReposition(
      const std::string& package_name,
      RepositionType reposition_type,
      InputOverlayWindowStateType state_type) {
    ukm::builders::GamingInputOverlay_ButtonGroupRepositionOperationType(
        ukm::AppSourceUrlRecorder::GetSourceIdForArcPackageName(package_name))
        .SetOperationType(static_cast<int64_t>(reposition_type))
        .Record(ukm::UkmRecorder::Get());
    ukm::builders::GamingInputOverlay_ButtonGroupRepositionWindowStateType(
        ukm::AppSourceUrlRecorder::GetSourceIdForArcPackageName(package_name))
        .SetWindowStateType(static_cast<int64_t>(state_type))
        .Record(ukm::UkmRecorder::Get());
  }

  static void RecordToggleWithMappingSource(const std::string& package_name,
                                            bool is_feature,
                                            bool is_on,
                                            MappingSource source) {
    ukm::builders::GameControls_ToggleWithMappingSource(
        ukm::AppSourceUrlRecorder::GetSourceIdForArcPackageName(package_name))
        .SetFunction(static_cast<int64_t>(
            is_feature ? GameControlsToggleFunction::kFeature
                       : GameControlsToggleFunction::kMappingHint))
        .SetToggleOn(is_on)
        .SetMappingSource(static_cast<int64_t>(source))
        .Record(ukm::UkmRecorder::Get());
  }

  static void RecordEditingListFunctionTriggered(
      const std::string& package_name,
      EditingListFunction function) {
    ukm::builders::GameControls_EditingListFunctionTriggered(
        ukm::AppSourceUrlRecorder::GetSourceIdForArcPackageName(package_name))
        .SetFunction(static_cast<int64_t>(function))
        .Record(ukm::UkmRecorder::Get());
  }

  static void RecordButtonOptionsMenuFunctionTriggered(
      const std::string& package_name,
      ButtonOptionsMenuFunction function) {
    ukm::builders::GameControls_ButtonOptionsMenuFunctionTriggered(
        ukm::AppSourceUrlRecorder::GetSourceIdForArcPackageName(package_name))
        .SetFunction(static_cast<int64_t>(function))
        .Record(ukm::UkmRecorder::Get());
  }

  static void RecordEditDeleteMenuFunctionTriggered(
      const std::string& package_name,
      EditDeleteMenuFunction function) {
    ukm::builders::GameControls_EditDeleteMenuFuctionTriggered(
        ukm::AppSourceUrlRecorder::GetSourceIdForArcPackageName(package_name))
        .SetFunction(static_cast<int64_t>(function))
        .Record(ukm::UkmRecorder::Get());
  }
};

std::string BuildGameControlsHistogramName(const std::string& name) {
  return base::JoinString({kGameControlsHistogramNameRoot, name},
                          kGameControlsHistogramSeparator);
}

std::string BuildGameControlsUkmEventName(const std::string& name) {
  return base::JoinString({kGameControlsUkmEventNameRoot, name},
                          kGameControlsHistogramSeparator);
}

void RecordInputOverlayFeatureState(const std::string& package_name,
                                    bool enable) {
  base::UmaHistogramBoolean("Arc.InputOverlay.FeatureState", enable);
  InputOverlayUkm::RecordInputOverlayFeatureState(package_name, enable);
}

void RecordInputOverlayMappingHintState(const std::string& package_name,
                                        bool enable) {
  base::UmaHistogramBoolean("Arc.InputOverlay.MappingHintState", enable);
  InputOverlayUkm::RecordInputOverlayMappingHintState(package_name, enable);
}

void RecordInputOverlayCustomizedUsage(const std::string& package_name) {
  base::UmaHistogramBoolean("Arc.InputOverlay.Customized", true);
  InputOverlayUkm::RecordInputOverlayCustomizedUsage(package_name);
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

void RecordEditingListFunctionTriggered(const std::string& package_name,
                                        EditingListFunction function) {
  base::UmaHistogramEnumeration(
      BuildGameControlsHistogramName(kEditingListFunctionTriggeredHistogram),
      function);
  InputOverlayUkm::RecordEditingListFunctionTriggered(package_name, function);
}

void RecordButtonOptionsMenuFunctionTriggered(
    const std::string& package_name,
    ButtonOptionsMenuFunction function) {
  base::UmaHistogramEnumeration(
      BuildGameControlsHistogramName(
          kButtonOptionsMenuFunctionTriggeredHistogram),
      function);
  InputOverlayUkm::RecordButtonOptionsMenuFunctionTriggered(package_name,
                                                            function);
}

void RecordEditDeleteMenuFunctionTriggered(const std::string& package_name,
                                           EditDeleteMenuFunction function) {
  base::UmaHistogramEnumeration(
      BuildGameControlsHistogramName(kEditDeleteMenuFunctionTriggeredHistogram),
      function);

  InputOverlayUkm::RecordEditDeleteMenuFunctionTriggered(package_name,
                                                         function);
}

void RecordToggleWithMappingSource(const std::string& package_name,
                                   bool is_feature,
                                   bool is_on,
                                   MappingSource source) {
  base::UmaHistogramEnumeration(
      BuildGameControlsHistogramName(
          base::JoinString(
              {(is_feature ? kFeatureHistogramName : kHintHistogramName),
               kToggleWithMappingSourceHistogram},
              ""))
          .append(kGameControlsHistogramSeparator)
          .append(is_on ? kToggleOnHistogramName : kToggleOffHistogramName),
      source);
  InputOverlayUkm::RecordToggleWithMappingSource(package_name, is_feature,
                                                 is_on, source);
}

}  // namespace arc::input_overlay
