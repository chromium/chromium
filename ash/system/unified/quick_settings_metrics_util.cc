// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/quick_settings_metrics_util.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/quick_settings_catalogs.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
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
constexpr char kQuickSettingsFeaturePodCount[] =
    "Ash.QuickSettings.Clamshell.FeaturePodCountOnOpen";
constexpr char kQuickSettingsTabletFeaturePodCount[] =
    "Ash.QuickSettings.Tablet.FeaturePodCountOnOpen";

// For the old view:
constexpr char kUnifiedViewButton[] = "Ash.UnifiedSystemView.Button.Activated";
constexpr char kUnifiedViewFeaturePodEnabled[] =
    "Ash.UnifiedSystemView.FeaturePod.ToggledOn";
constexpr char kUnifiedViewFeaturePodDisabled[] =
    "Ash.UnifiedSystemView.FeaturePod.ToggledOff";
constexpr char kUnifiedViewFeaturePodDiveIn[] =
    "Ash.UnifiedSystemView.FeaturePod.DiveIn";
constexpr char kUnifiedViewFeaturePodCount[] =
    "Ash.UnifiedSystemView.Clamshell.FeaturePodCountOnOpen";
constexpr char kUnifiedViewTabletFeaturePodCount[] =
    "Ash.UnifiedSystemView.Tablet.FeaturePodCountOnOpen";

}  // namespace

namespace quick_settings_metrics_util {

void RecordQsButtonActivated(QsButtonCatalogName button_catalog_name,
                             const ui::Event& event) {
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

void RecordQsFeaturePodCount(int feature_pod_count, bool is_tablet) {
  if (is_tablet) {
    UMA_HISTOGRAM_COUNTS_100(features::IsQsRevampEnabled()
                                 ? kQuickSettingsTabletFeaturePodCount
                                 : kUnifiedViewTabletFeaturePodCount,
                             feature_pod_count);
    return;
  }

  UMA_HISTOGRAM_COUNTS_100(features::IsQsRevampEnabled()
                               ? kQuickSettingsFeaturePodCount
                               : kUnifiedViewFeaturePodCount,
                           feature_pod_count);
}

}  // namespace quick_settings_metrics_util

}  // namespace ash
