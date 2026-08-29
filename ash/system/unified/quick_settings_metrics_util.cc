// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/quick_settings_metrics_util.h"

#include "ash/constants/quick_settings_catalogs.h"
#include "ash/metrics/demo_session_metrics_recorder.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/metrics/histogram_functions.h"

namespace ash {

namespace {
constexpr char kQuickSettingsFeaturePodEnabled[] =
    "Ash.QuickSettings.FeaturePod.ToggledOn";
constexpr char kQuickSettingsFeaturePodDisabled[] =
    "Ash.QuickSettings.FeaturePod.ToggledOff";
constexpr char kQuickSettingsFeaturePodDiveIn[] =
    "Ash.QuickSettings.FeaturePod.DiveIn";
constexpr char kQuickSettingsFeaturePodVisible[] =
    "Ash.QuickSettings.FeaturePod.Visible";
constexpr char kQuickSettingsSliderUp[] = "Ash.QuickSettings.Slider.Up";
constexpr char kQuickSettingsSliderDown[] = "Ash.QuickSettings.Slider.Down";
constexpr char kQuickSettingsSliderEnable[] =
    "Ash.QuickSettings.Slider.EnableFeature";
constexpr char kQuickSettingsSliderDisable[] =
    "Ash.QuickSettings.Slider.DisableFeature";
}  // namespace

namespace quick_settings_metrics_util {

void RecordQsButtonActivated(QsButtonCatalogName button_catalog_name) {
  if (Shell::Get()->session_controller()->IsDemoSession()) {
    // Either "Sign out" or "Restart" button can sign out the session.
    if (button_catalog_name == QsButtonCatalogName::kPowerSignoutMenuButton ||
        button_catalog_name == QsButtonCatalogName::kPowerRestartMenuButton) {
      DemoSessionMetricsRecorder::RecordExitSessionAction(
          DemoSessionMetricsRecorder::ExitSessionFrom::kSystemTrayPowerButton);
    }
  }
}

void RecordQsFeatureToggle(QsFeatureCatalogName feature_catalog_name,
                           bool enable) {
  if (enable) {
    base::UmaHistogramEnumeration(kQuickSettingsFeaturePodEnabled,
                                  feature_catalog_name);
    return;
  }

  base::UmaHistogramEnumeration(kQuickSettingsFeaturePodDisabled,
                                feature_catalog_name);
}

void RecordQsFeatureDiveIn(QsFeatureCatalogName feature_catalog_name) {
  base::UmaHistogramEnumeration(kQuickSettingsFeaturePodDiveIn,
                                feature_catalog_name);
}

void RecordVisibleQsFeature(QsFeatureCatalogName feature_catalog_name) {
  base::UmaHistogramEnumeration(kQuickSettingsFeaturePodVisible,
                                feature_catalog_name);
}

void RecordQsSliderValueChange(QsSliderCatalogName slider_catalog_name,
                               bool going_up) {
  if (going_up) {
    base::UmaHistogramEnumeration(kQuickSettingsSliderUp, slider_catalog_name);
    return;
  }

  base::UmaHistogramEnumeration(kQuickSettingsSliderDown, slider_catalog_name);
}

void RecordQsSliderToggle(QsSliderCatalogName slider_catalog_name,
                          bool enable) {
  if (enable) {
    base::UmaHistogramEnumeration(kQuickSettingsSliderEnable,
                                  slider_catalog_name);
    return;
  }

  base::UmaHistogramEnumeration(kQuickSettingsSliderDisable,
                                slider_catalog_name);
}

}  // namespace quick_settings_metrics_util

}  // namespace ash
