// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharesheet/sharesheet_metrics.h"

#include "base/metrics/histogram_functions.h"

namespace {
// Should be comfortably larger than any max number of apps
// a user could have installed.
constexpr size_t kMaxAppCount = 1000;
}  // namespace

namespace sharesheet {

SharesheetMetrics::SharesheetMetrics() = default;

void SharesheetMetrics::RecordSharesheetActionMetrics(const UserAction action) {
  base::UmaHistogramEnumeration("ChromeOS.Sharesheet.UserAction", action);
}

void SharesheetMetrics::RecordSharesheetAppCount(const int app_count) {
  base::UmaHistogramExactLinear("ChromeOS.Sharesheet.AppCount.All", app_count,
                                kMaxAppCount);
}

void SharesheetMetrics::RecordSharesheetArcAppCount(const int app_count) {
  base::UmaHistogramExactLinear("ChromeOS.Sharesheet.AppCount.Arc", app_count,
                                kMaxAppCount);
}

void SharesheetMetrics::RecordSharesheetWebAppCount(const int app_count) {
  base::UmaHistogramExactLinear("ChromeOS.Sharesheet.AppCount.Web", app_count,
                                kMaxAppCount);
}

void SharesheetMetrics::RecordSharesheetShareAction(const UserAction action) {
  DCHECK(action == UserAction::kNearbyAction ||
         action == UserAction::kDriveAction);
  base::UmaHistogramEnumeration("ChromeOS.Sharesheet.ActionCount", action);
}

void SharesheetMetrics::RecordSharesheetFormFactor(
    const FormFactor form_factor) {
  base::UmaHistogramEnumeration("ChromeOS.Sharesheet.FormFactor", form_factor);
}

void SharesheetMetrics::RecordSharesheetLaunchSource(
    const LaunchSource source) {
  base::UmaHistogramEnumeration("ChromeOS.Sharesheet.LaunchSource", source);
}

}  // namespace sharesheet
