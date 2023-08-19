// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/quick_settings_metrics_util.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/quick_settings_catalogs.h"
#include "base/metrics/histogram_functions.h"
#include "ui/events/event.h"

namespace ash {

namespace {

// For the revamped view:
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

// For the old view:
constexpr char kUnifiedViewButton[] = "Ash.UnifiedSystemView.Button.Activated";
constexpr char kUnifiedViewFeaturePodEnabled[] =
    "Ash.UnifiedSystemView.FeaturePod.ToggledOn";
constexpr char kUnifiedViewFeaturePodDisabled[] =
    "Ash.UnifiedSystemView.FeaturePod.ToggledOff";
constexpr char kUnifiedViewFeaturePodDiveIn[] =
    "Ash.UnifiedSystemView.FeaturePod.DiveIn";
constexpr char kUnifiedViewFeaturePodVisible[] =
    "Ash.UnifiedSystemView.FeaturePod.Visible";
constexpr char kUnifiedViewFeaturePodCount[] =
    "Ash.UnifiedSystemView.Clamshell.FeaturePodCountOnOpen";
constexpr char kUnifiedViewTabletFeaturePodCount[] =
    "Ash.UnifiedSystemView.Tablet.FeaturePodCountOnOpen";
constexpr char kUnifiedSystemViewSliderUp[] = "Ash.UnifiedSystemView.Slider.Up";
constexpr char kUnifiedSystemViewSliderDown[] =
    "Ash.UnifiedSystemView.Slider.Down";
constexpr char kUnifiedSystemViewSliderEnable[] =
    "Ash.UnifiedSystemView.Slider.EnableFeature";
constexpr char kUnifiedSystemViewSliderDisable[] =
    "Ash.UnifiedSystemView.Slider.DisableFeature";

}  // namespace

namespace quick_settings_metrics_util {

void RecordQsButtonActivated(QsButtonCatalogName button_catalog_name) {
  base::UmaHistogramEnumeration(
      features::IsQsRevampEnabled() ? kQuickSettingsButton : kUnifiedViewButton,
      button_catalog_name);
}

void RecordQsFeatureToggle(QsFeatureCatalogName feature_catalog_name,
                           bool enable) {
  if (enable) {
    base::UmaHistogramEnumeration(features::IsQsRevampEnabled()
                                      ? kQuickSettingsFeaturePodEnabled
                                      : kUnifiedViewFeaturePodEnabled,
                                  feature_catalog_name);
    return;
  }

  base::UmaHistogramEnumeration(features::IsQsRevampEnabled()
                                    ? kQuickSettingsFeaturePodDisabled
                                    : kUnifiedViewFeaturePodDisabled,
                                feature_catalog_name);
}

void RecordQsFeatureDiveIn(QsFeatureCatalogName feature_catalog_name) {
  base::UmaHistogramEnumeration(features::IsQsRevampEnabled()
                                    ? kQuickSettingsFeaturePodDiveIn
                                    : kUnifiedViewFeaturePodDiveIn,
                                feature_catalog_name);
}

void RecordVisibleQsFeature(QsFeatureCatalogName feature_catalog_name) {
  base::UmaHistogramEnumeration(features::IsQsRevampEnabled()
                                    ? kQuickSettingsFeaturePodVisible
                                    : kUnifiedViewFeaturePodVisible,
                                feature_catalog_name);
}

void RecordQsFeaturePodCount(int feature_pod_count, bool is_tablet) {
  if (is_tablet) {
    base::UmaHistogramCounts100(features::IsQsRevampEnabled()
                                    ? kQuickSettingsTabletFeaturePodCount
                                    : kUnifiedViewTabletFeaturePodCount,
                                feature_pod_count);
    return;
  }

  base::UmaHistogramCounts100(features::IsQsRevampEnabled()
                                  ? kQuickSettingsFeaturePodCount
                                  : kUnifiedViewFeaturePodCount,
                              feature_pod_count);
}

void RecordQsSliderValueChange(QsSliderCatalogName slider_catalog_name,
                               bool going_up) {
  if (going_up) {
    base::UmaHistogramEnumeration(features::IsQsRevampEnabled()
                                      ? kQuickSettingsSliderUp
                                      : kUnifiedSystemViewSliderUp,
                                  slider_catalog_name);
    return;
  }

  base::UmaHistogramEnumeration(features::IsQsRevampEnabled()
                                    ? kQuickSettingsSliderDown
                                    : kUnifiedSystemViewSliderDown,
                                slider_catalog_name);
}

void RecordQsSliderToggle(QsSliderCatalogName slider_catalog_name,
                          bool enable) {
  if (enable) {
    base::UmaHistogramEnumeration(features::IsQsRevampEnabled()
                                      ? kQuickSettingsSliderEnable
                                      : kUnifiedSystemViewSliderEnable,
                                  slider_catalog_name);
    return;
  }

  base::UmaHistogramEnumeration(features::IsQsRevampEnabled()
                                    ? kQuickSettingsSliderDisable
                                    : kUnifiedSystemViewSliderDisable,
                                slider_catalog_name);
}

void RecordQsPageCountOnClose(int page_count) {
  CHECK(features::IsQsRevampEnabled());
  base::UmaHistogramCounts100(kQuickSettingsPageCountOnClose, page_count);
}

}  // namespace quick_settings_metrics_util

}  // namespace ash
