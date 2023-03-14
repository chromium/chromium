// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/arc_input_overlay_ukm.h"

#include "components/ukm/app_source_url_recorder.h"
#include "services/metrics/public/cpp/delegating_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"

namespace arc::input_overlay {

constexpr bool kCustomizationUsed = true;

void InputOverlayUkm::RecordInputOverlayFeatureStateUkm(
    std::string package_name,
    bool enable) {
  ukm::builders::GamingInputOverlay_Feature(
      ukm::AppSourceUrlRecorder::GetSourceIdForArcPackageName(package_name))
      .SetFeatureStateOn(enable)
      .Record(ukm::UkmRecorder::Get());
}

void InputOverlayUkm::RecordInputOverlayMappingHintStateUkm(
    std::string package_name,
    bool enable) {
  ukm::builders::GamingInputOverlay_MappingHint(
      ukm::AppSourceUrlRecorder::GetSourceIdForArcPackageName(package_name))
      .SetMappingHintStateOn(enable)
      .Record(ukm::UkmRecorder::Get());
}

void InputOverlayUkm::RecordInputOverlayCustomizedUsageUkm(
    std::string package_name) {
  ukm::builders::GamingInputOverlay_Customization(
      ukm::AppSourceUrlRecorder::GetSourceIdForArcPackageName(package_name))
      .SetCustomizationUsed(kCustomizationUsed)
      .Record(ukm::UkmRecorder::Get());
}

void InputOverlayUkm::RecordInputOverlayActionReposition(
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

void InputOverlayUkm::RecordInputOverlayMenuEntryReposition(
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

void InputOverlayUkm::RecordInputOverlayButtonGroupReposition(
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

}  // namespace arc::input_overlay
