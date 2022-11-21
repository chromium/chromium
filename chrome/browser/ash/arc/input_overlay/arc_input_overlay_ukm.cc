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

}  // namespace arc::input_overlay
