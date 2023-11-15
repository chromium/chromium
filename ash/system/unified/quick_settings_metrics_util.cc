// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/quick_settings_metrics_util.h"

#include "ash/constants/quick_settings_catalogs.h"
#include "base/metrics/histogram_functions.h"

namespace ash {

namespace {
constexpr char kQuickSettingsButton[] = "Ash.QuickSettings.Button.Activated";
constexpr char kQuickSettingsFeaturePodEnabled[] =
    "Ash.QuickSettings.FeaturePod.ToggledOn";
constexpr char kQuickSettingsFeaturePodDisabled[] =
    "Ash.QuickSettings.FeaturePod.ToggledOff";
constexpr char kQuickSettingsFeaturePodDiveIn[] =
    "Ash.QuickSettings.FeaturePod.DiveIn";
constexpr char kQuickSettingsFeaturePodVisible[] =
    "Ash.QuickSettings.FeaturePod.Visible";
constexpr char kQuickSettingsFeaturePodCount[] =
    "Ash.QuickSettings.Clamshell.FeaturePodCountOnOpen";
constexpr char kQuickSettingsTabletFeaturePodCount[] =
    "Ash.QuickSettings.Tablet.FeaturePodCountOnOpen";
constexpr char kQuickSettingsPageCountOnClose[] =
    "Ash.QuickSettings.PageCountOnClose";
constexpr char kQuickSettingsSliderUp[] = "Ash.QuickSettings.Slider.Up";
constexpr char kQuickSettingsSliderDown[] = "Ash.QuickSettings.Slider.Down";
constexpr char kQuickSettingsSliderEnable[] =
    "Ash.QuickSettings.Slider.EnableFeature";
constexpr char kQuickSettingsSliderDisable[] =
    "Ash.QuickSettings.Slider.DisableFeature";
}  // namespace

namespace quick_settings_metrics_util {

void RecordQsButtonActivated(QsButtonCatalogName button_catalog_name) {
  base::UmaHistogramEnumeration(kQuickSettingsButton, button_catalog_name);
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

void RecordQsFeaturePodCount(int feature_pod_count, bool is_tablet) {
  if (is_tablet) {
    base::UmaHistogramCounts100(kQuickSettingsTabletFeaturePodCount,
                                feature_pod_count);
    return;
  }

  base::UmaHistogramCounts100(kQuickSettingsFeaturePodCount, feature_pod_count);
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

void RecordQsPageCountOnClose(int page_count) {
  base::UmaHistogramCounts100(kQuickSettingsPageCountOnClose, page_count);
}

}  // namespace quick_settings_metrics_util

}  // namespace ash
