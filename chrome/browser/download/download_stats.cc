// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_stats.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "components/download/public/common/download_content.h"
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

void RecordDownloadOpen(ChromeDownloadOpenMethod open_method,
                        const std::string& mime_type_string) {
  base::RecordAction(base::UserMetricsAction("Download.Open"));
  base::UmaHistogramEnumeration("Download.OpenMethod", open_method,
                                DOWNLOAD_OPEN_METHOD_LAST_ENTRY);
  download::DownloadContent download_content =
      download::DownloadContentFromMimeType(
          mime_type_string, /*record_content_subcategory=*/false);
  if (download_content == download::DownloadContent::DOCUMENT ||
      download_content == download::DownloadContent::PDF ||
      download_content == download::DownloadContent::SPREADSHEET ||
      download_content == download::DownloadContent::TEXT ||
      download_content == download::DownloadContent::UNRECOGNIZED) {
    // TODO(crbug.com/1372476): Remove this histogram after debugging.
    base::UmaHistogramEnumeration(
        "Download.OpenMethod." +
            download::GetDownloadContentString(download_content),
        open_method, DOWNLOAD_OPEN_METHOD_LAST_ENTRY);
  }
  base::UmaHistogramEnumeration("Download.Open.ContentType", download_content,
                                download::DownloadContent::MAX);
}

void RecordDownloadOpenButtonPressed(bool is_download_completed) {
  base::UmaHistogramBoolean("Download.OpenButtonPressed.IsDownloadCompleted",
                            is_download_completed);
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

void RecordDownloadShelfDragInfo(DownloadDragInfo drag_info) {
  base::UmaHistogramEnumeration("Download.Shelf.DragInfo", drag_info,
                                DownloadDragInfo::COUNT);
}

void RecordDownloadBubbleDragInfo(DownloadDragInfo drag_info) {
  base::UmaHistogramEnumeration("Download.Bubble.DragInfo", drag_info,
                                DownloadDragInfo::COUNT);
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
    case DownloadCommands::Command::LEARN_MORE_INSECURE_DOWNLOAD:
      return clicked ? DownloadShelfContextMenuAction::
                           kLearnMoreInsecureDownloadClicked
                     : DownloadShelfContextMenuAction::
                           kLearnMoreInsecureDownloadEnabled;
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

    // The following are not actually visible in the context menu so should
    // never be logged.
    case DownloadCommands::Command::REVIEW:
    case DownloadCommands::Command::RETRY:
      NOTREACHED();
      return DownloadShelfContextMenuAction::kNotReached;
  }
}
