// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_stats.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "components/safe_browsing/content/browser/download/download_stats.h"

void RecordDownloadCount(ChromeDownloadCountTypes type) {
  base::UmaHistogramEnumeration("Download.CountsChrome", type,
                                CHROME_DOWNLOAD_COUNT_TYPES_LAST_ENTRY);
}

void RecordDownloadSource(ChromeDownloadSource source) {
  base::UmaHistogramEnumeration("Download.SourcesChrome", source,
                                CHROME_DOWNLOAD_SOURCE_LAST_ENTRY);
}

void RecordDangerousDownloadWarningShown(
    download::DownloadDangerType danger_type,
    const base::FilePath& file_path,
    bool is_https,
    bool has_user_gesture) {
  base::UmaHistogramEnumeration("Download.ShowedDownloadWarning", danger_type,
                                download::DOWNLOAD_DANGER_TYPE_MAX);
  safe_browsing::RecordDangerousDownloadWarningShown(
      danger_type, file_path, is_https, has_user_gesture);
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

void RecordDownloadShelfDragInfo(DownloadShelfDragInfo drag_info) {
  base::UmaHistogramEnumeration("Download.Shelf.DragInfo", drag_info,
                                DownloadShelfDragInfo::COUNT);
}

void RecordDownloadStartPerProfileType(Profile* profile) {
  base::UmaHistogramEnumeration(
      "Download.Start.PerProfileType",
      profile_metrics::GetBrowserProfileType(profile));
}

#if BUILDFLAG(IS_ANDROID)
// Records whether the download dialog is shown to the user.
void RecordDownloadPromptStatus(DownloadPromptStatus status) {
  base::UmaHistogramEnumeration("MobileDownload.DownloadPromptStatus", status,
                                DownloadPromptStatus::MAX_VALUE);
}

void RecordDownloadLaterPromptStatus(DownloadLaterPromptStatus status) {
  base::UmaHistogramEnumeration("MobileDownload.DownloadLaterPromptStatus",
                                status);
}

#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS)
void RecordDownloadNotificationSuppressed() {
  base::UmaHistogramBoolean("Download.Notification.Suppressed", true);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

DownloadShelfContextMenuAction DownloadCommandToShelfAction(
    DownloadCommands::Command download_command,
    bool clicked) {
  switch (download_command) {
    case DownloadCommands::Command::MAX:
      NOTREACHED();
      return DownloadShelfContextMenuAction::kMaxValue;
    case DownloadCommands::Command::SHOW_IN_FOLDER:
      return clicked ? DownloadShelfContextMenuAction::kShowInFolderClicked
                     : DownloadShelfContextMenuAction::kShowInFolderEnabled;
    case DownloadCommands::Command::OPEN_WHEN_COMPLETE:
      return clicked ? DownloadShelfContextMenuAction::kOpenWhenCompleteClicked
                     : DownloadShelfContextMenuAction::kOpenWhenCompleteEnabled;
    case DownloadCommands::Command::ALWAYS_OPEN_TYPE:
      return clicked ? DownloadShelfContextMenuAction::kAlwaysOpenTypeClicked
                     : DownloadShelfContextMenuAction::kAlwaysOpenTypeEnabled;
    case DownloadCommands::Command::PLATFORM_OPEN:
      return clicked ? DownloadShelfContextMenuAction::kPlatformOpenClicked
                     : DownloadShelfContextMenuAction::kPlatformOpenEnabled;
    case DownloadCommands::Command::CANCEL:
      return clicked ? DownloadShelfContextMenuAction::kCancelClicked
                     : DownloadShelfContextMenuAction::kCancelEnabled;
    case DownloadCommands::Command::PAUSE:
      return clicked ? DownloadShelfContextMenuAction::kPauseClicked
                     : DownloadShelfContextMenuAction::kPauseEnabled;
    case DownloadCommands::Command::RESUME:
      return clicked ? DownloadShelfContextMenuAction::kResumeClicked
                     : DownloadShelfContextMenuAction::kResumeEnabled;
    case DownloadCommands::Command::DISCARD:
      return clicked ? DownloadShelfContextMenuAction::kDiscardClicked
                     : DownloadShelfContextMenuAction::kDiscardEnabled;
    case DownloadCommands::Command::KEEP:
      return clicked ? DownloadShelfContextMenuAction::kKeepClicked
                     : DownloadShelfContextMenuAction::kKeepEnabled;
    case DownloadCommands::Command::LEARN_MORE_SCANNING:
      return clicked
                 ? DownloadShelfContextMenuAction::kLearnMoreScanningClicked
                 : DownloadShelfContextMenuAction::kLearnMoreScanningEnabled;
    case DownloadCommands::Command::LEARN_MORE_INTERRUPTED:
      return clicked
                 ? DownloadShelfContextMenuAction::kLearnMoreInterruptedClicked
                 : DownloadShelfContextMenuAction::kLearnMoreInterruptedEnabled;
    case DownloadCommands::Command::LEARN_MORE_MIXED_CONTENT:
      return clicked
                 ? DownloadShelfContextMenuAction::kLearnMoreMixedContentClicked
                 : DownloadShelfContextMenuAction::
                       kLearnMoreMixedContentEnabled;
    case DownloadCommands::Command::COPY_TO_CLIPBOARD:
      return clicked ? DownloadShelfContextMenuAction::kCopyToClipboardClicked
                     : DownloadShelfContextMenuAction::kCopyToClipboardEnabled;
    case DownloadCommands::Command::DEEP_SCAN:
      return clicked ? DownloadShelfContextMenuAction::kDeepScanClicked
                     : DownloadShelfContextMenuAction::kDeepScanEnabled;
    case DownloadCommands::Command::BYPASS_DEEP_SCANNING:
      return clicked
                 ? DownloadShelfContextMenuAction::kBypassDeepScanningClicked
                 : DownloadShelfContextMenuAction::kBypassDeepScanningEnabled;
  }
}
