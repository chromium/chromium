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

}  // namespace arc::input_overlay
