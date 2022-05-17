// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/arc_input_overlay_uma.h"

#include "base/metrics/histogram_functions.h"

namespace arc {
namespace input_overlay {

void RecordInputOverlayFeatureState(const std::string& package_name,
                                    bool enable) {
  base::UmaHistogramBoolean("Arc.InputOverlay.FeatureState", enable);
  base::UmaHistogramBoolean("Arc.InputOverlay.FeatureState." + package_name,
                            enable);
}

void RecordInputOverlayMappingHintState(const std::string& package_name,
                                        bool enable) {
  base::UmaHistogramBoolean("Arc.InputOverlay.MappingHintState", enable);
  base::UmaHistogramBoolean("Arc.InputOverlay.MappingHintState." + package_name,
                            enable);
}

void RecordInputOverlayCustomizedUsage(const std::string& package_name) {
  base::UmaHistogramBoolean("Arc.InputOverlay.Customized", true);
  base::UmaHistogramBoolean("Arc.InputOverlay.Customized." + package_name,
                            true);
}

}  // namespace input_overlay
}  // namespace arc
