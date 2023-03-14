// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/feature_pod_controller_base.h"

#include "ash/constants/ash_features.h"
#include "ash/system/unified/feature_pod_button.h"
#include "ash/system/unified/feature_tile.h"
#include "ash/system/unified/quick_settings_metrics_util.h"

namespace ash {

void FeaturePodControllerBase::OnLabelPressed() {
  return OnIconPressed();
}

void FeaturePodControllerBase::TrackToggleUMA(bool target_toggle_state) {
  quick_settings_metrics_util::RecordQsFeatureToggle(GetCatalogName(),
                                                     target_toggle_state);
}

void FeaturePodControllerBase::TrackDiveInUMA() {
  quick_settings_metrics_util::RecordQsFeatureDiveIn(GetCatalogName());
}

void FeaturePodControllerBase::TrackVisibilityUMA() {
  quick_settings_metrics_util::RecordVisibleQsFeature(GetCatalogName());
}

}  // namespace ash
