// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharesheet/sharesheet_metrics.h"

#include "base/metrics/histogram_functions.h"

namespace sharesheet {

const char kSharesheetUserActionResultHistogram[] =
    "ChromeOS.Sharesheet.UserAction";
const char kSharesheetAppCountAllResultHistogram[] =
    "ChromeOS.Sharesheet.AppCount2.All";
const char kSharesheetAppCountArcResultHistogram[] =
    "ChromeOS.Sharesheet.AppCount2.Arc";
const char kSharesheetAppCountWebResultHistogram[] =
    "ChromeOS.Sharesheet.AppCount2.Web";
const char kSharesheetShareActionResultHistogram[] =
    "ChromeOS.Sharesheet.ActionCount";
const char kSharesheetFormFactorResultHistogram[] =
    "ChromeOS.Sharesheet.FormFactor";
const char kSharesheetLaunchSourceResultHistogram[] =
    "ChromeOS.Sharesheet.LaunchSource";
const char kSharesheetFileCountResultHistogram[] =
    "ChromeOS.Sharesheet.FileCount";
const char kSharesheetIsDriveFolderResultHistogram[] =
    "ChromeOS.Sharesheet.IsDriveFolder";
const char kSharesheetIsImagePressedResultHistogram[] =
    "ChromeOS.Sharesheet.IsImagePreviewPressed";
const char kSharesheetCopyToClipboardMimeTypeResultHistogram[] =
    "ChromeOS.Sharesheet.CopyToClipboard.MimeType";

SharesheetMetrics::SharesheetMetrics() = default;

void SharesheetMetrics::RecordSharesheetActionMetrics(const UserAction action) {
  base::UmaHistogramEnumeration(kSharesheetUserActionResultHistogram, action);
}

void SharesheetMetrics::RecordSharesheetAppCount(const int app_count) {
  base::UmaHistogramCounts100(kSharesheetAppCountAllResultHistogram, app_count);
}

void SharesheetMetrics::RecordSharesheetArcAppCount(const int app_count) {
  base::UmaHistogramCounts100(kSharesheetAppCountArcResultHistogram, app_count);
}

void SharesheetMetrics::RecordSharesheetWebAppCount(const int app_count) {
  base::UmaHistogramCounts100(kSharesheetAppCountWebResultHistogram, app_count);
}

void SharesheetMetrics::RecordSharesheetShareAction(const UserAction action) {
  DCHECK(action == UserAction::kNearbyAction ||
         action == UserAction::kDriveAction ||
         action == UserAction::kCopyAction);
  base::UmaHistogramEnumeration(kSharesheetShareActionResultHistogram, action);
}

void SharesheetMetrics::RecordSharesheetFormFactor(
    const FormFactor form_factor) {
  base::UmaHistogramEnumeration(kSharesheetFormFactorResultHistogram,
                                form_factor);
}

void SharesheetMetrics::RecordSharesheetLaunchSource(
    const LaunchSource source) {
  base::UmaHistogramEnumeration(kSharesheetLaunchSourceResultHistogram, source);
}

void SharesheetMetrics::RecordSharesheetFilesSharedCount(const int file_count) {
  base::UmaHistogramCounts100(kSharesheetFileCountResultHistogram, file_count);
}

void SharesheetMetrics::RecordSharesheetIsDriveFolder(
    const bool is_drive_folder) {
  base::UmaHistogramBoolean(kSharesheetIsDriveFolderResultHistogram,
                            is_drive_folder);
}

void SharesheetMetrics::RecordSharesheetImagePreviewPressed(
    const bool is_pressed) {
  base::UmaHistogramBoolean(kSharesheetIsImagePressedResultHistogram,
                            is_pressed);
}

void SharesheetMetrics::RecordCopyToClipboardShareActionMimeType(
    const MimeType mime_type) {
  base::UmaHistogramEnumeration(
      kSharesheetCopyToClipboardMimeTypeResultHistogram, mime_type);
}

}  // namespace sharesheet
