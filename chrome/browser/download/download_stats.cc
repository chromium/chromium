// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_stats.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "components/profile_metrics/browser_profile_type.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"

void RecordDownloadCount(ChromeDownloadCountTypes type) {
  base::UmaHistogramEnumeration("Download.CountsChrome", type,
                                CHROME_DOWNLOAD_COUNT_TYPES_LAST_ENTRY);
}

void RecordDownloadSource(ChromeDownloadSource source) {
  base::UmaHistogramEnumeration("Download.SourcesChrome", source,
                                CHROME_DOWNLOAD_SOURCE_LAST_ENTRY);
}

void RecordDangerousDownloadWarningShown(
    download::DownloadDangerType danger_type) {
  base::UmaHistogramEnumeration("Download.ShowedDownloadWarning", danger_type,
                                download::DOWNLOAD_DANGER_TYPE_MAX);
}

void RecordOpenedDangerousConfirmDialog(
    download::DownloadDangerType danger_type) {
  base::UmaHistogramEnumeration(
      "Download.ShowDangerousDownloadConfirmationPrompt", danger_type,
      download::DOWNLOAD_DANGER_TYPE_MAX);
}

void RecordDownloadOpenMethod(ChromeDownloadOpenMethod open_method) {
  base::RecordAction(base::UserMetricsAction("Download.Open"));
  base::UmaHistogramEnumeration("Download.OpenMethod", open_method,
                                DOWNLOAD_OPEN_METHOD_LAST_ENTRY);
}

void RecordDatabaseAvailability(bool is_available) {
  base::UmaHistogramBoolean("Download.Database.IsAvailable", is_available);
}

void RecordDownloadPathGeneration(DownloadPathGenerationEvent event,
                                  bool is_transient) {
  if (is_transient) {
    base::UmaHistogramEnumeration("Download.PathGenerationEvent.Transient",
                                  event, DownloadPathGenerationEvent::COUNT);
  } else {
    base::UmaHistogramEnumeration("Download.PathGenerationEvent.UserDownload",
                                  event, DownloadPathGenerationEvent::COUNT);
  }
}

void RecordDownloadPathValidation(download::PathValidationResult result,
                                  bool is_transient) {
  if (is_transient) {
    base::UmaHistogramEnumeration("Download.PathValidationResult.Transient",
                                  result,
                                  download::PathValidationResult::COUNT);
  } else {
    base::UmaHistogramEnumeration("Download.PathValidationResult.UserDownload",
                                  result,
                                  download::PathValidationResult::COUNT);
  }
}

void RecordDownloadCancelReason(DownloadCancelReason reason) {
  base::UmaHistogramEnumeration("Download.CancelReason", reason);
}

void RecordDownloadShelfDragEvent(DownloadShelfDragEvent drag_event) {
  base::UmaHistogramEnumeration("Download.Shelf.DragEvent", drag_event,
                                DownloadShelfDragEvent::COUNT);
}

void RecordDownloadStartPerProfileType(Profile* profile) {
  base::UmaHistogramEnumeration("Download.Start.PerProfileType",
                                ProfileMetrics::GetBrowserProfileType(profile));
}

#ifdef OS_ANDROID
// Records whether the download dialog is shown to the user.
void RecordDownloadPromptStatus(DownloadPromptStatus status) {
  base::UmaHistogramEnumeration("MobileDownload.DownloadPromptStatus", status,
                                DownloadPromptStatus::MAX_VALUE);
}

void RecordDownloadLaterPromptStatus(DownloadLaterPromptStatus status) {
  base::UmaHistogramEnumeration("MobileDownload.DownloadLaterPromptStatus",
                                status);
}

#endif  // OS_ANDROID
