// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharesheet/sharesheet_metrics.h"

#include "base/metrics/histogram_functions.h"

namespace sharesheet {

SharesheetMetrics::SharesheetMetrics() = default;

void SharesheetMetrics::RecordSharesheetActionMetrics(const UserAction action) {
  base::UmaHistogramEnumeration("ChromeOS.Sharesheet.UserAction", action);
}

void SharesheetMetrics::RecordSharesheetAppCount(const int app_count) {
  base::UmaHistogramCounts100("ChromeOS.Sharesheet.AppCount2.All", app_count);
}

void SharesheetMetrics::RecordSharesheetArcAppCount(const int app_count) {
  base::UmaHistogramCounts100("ChromeOS.Sharesheet.AppCount2.Arc", app_count);
}

void SharesheetMetrics::RecordSharesheetWebAppCount(const int app_count) {
  base::UmaHistogramCounts100("ChromeOS.Sharesheet.AppCount2.Web", app_count);
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

void SharesheetMetrics::RecordSharesheetFilesSharedCount(const int file_count) {
  base::UmaHistogramCounts100("ChromeOS.Sharesheet.FileCount", file_count);
}

void SharesheetMetrics::RecordSharesheetIsDriveFolder(
    const bool is_drive_folder) {
  base::UmaHistogramBoolean("ChromeOS.Sharesheet.IsDriveFolder",
                            is_drive_folder);
}

void SharesheetMetrics::RecordSharesheetImagePreviewPressed(
    const bool is_pressed) {
  base::UmaHistogramBoolean("ChromeOS.Sharesheet.IsImagePreviewPressed",
                            is_pressed);
}

}  // namespace sharesheet
