// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_QUICK_SETTINGS_METRICS_UTIL_H_
#define ASH_SYSTEM_UNIFIED_QUICK_SETTINGS_METRICS_UTIL_H_

#include "ash/constants/quick_settings_catalogs.h"

namespace ash::quick_settings_metrics_util {

// Records any event on a button in the quick settings main page. The value of
// recording type of event (such as: tap/click/stylus etc) is not high. To avoid
// creating a bunch of metrics, this method only records the "catalog name" as
// the enum bucket for now.
void RecordQsButtonActivated(QsButtonCatalogName button_catalog_name);

// Records toggle to enable/disable a feature in the quick settings main page.
// The arg `enable == true` means this feature was disabled and will be enabled
// by this toggle action. If the feature pod is an action feature, such as
// screen caption, always use `true` as the toggled value.
void RecordQsFeatureToggle(QsFeatureCatalogName feature_catalog_name,
                           bool enable);

// Records dive into a feature's details page from the quick settings main page.
void RecordQsFeatureDiveIn(QsFeatureCatalogName feature_catalog_name);

// Records the visible feature pods on the quick settings main page.
void RecordVisibleQsFeature(QsFeatureCatalogName feature_catalog_name);

// Records visible feature pod number in the quick settings main page.
void RecordQsFeaturePodCount(int feature_pod_count, bool is_tablet);

// Records slider value change in the quick settings main page or in the slider
// bubble.
void RecordQsSliderValueChange(QsSliderCatalogName slider_catalog_name,
                               bool going_up);

// Records toggle to enable/disable the corresponding functionality of the
// slider in the quick settings page or the slider bubble. The arg `enable ==
// true` means this feature was disabled and will be enabled by this toggle
// action.
void RecordQsSliderToggle(QsSliderCatalogName slider_catalog_name, bool enable);

// Records the number of quick settings pages the user has. It is better to
// record this when the user closes (rather than opens) the quick settings
// because the number of feature tiles, and thus the number of settings pages,
// may not be accurately represented upon opening (e.g. a feature tile's
// visibility may only be known after some data is fetched asynchronously).
void RecordQsPageCountOnClose(int page_count);

}  // namespace ash::quick_settings_metrics_util

#endif  // ASH_SYSTEM_UNIFIED_QUICK_SETTINGS_METRICS_UTIL_H_
