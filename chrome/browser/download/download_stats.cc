// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_stats.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "chrome/browser/download/download_ui_model.h"
#include "components/download/public/common/download_content.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "components/safe_browsing/content/browser/download/download_stats.h"

void RecordDownloadSource(ChromeDownloadSource source) {
  base::UmaHistogramEnumeration("Download.SourcesChrome", source,
                                CHROME_DOWNLOAD_SOURCE_LAST_ENTRY);
}

void MaybeRecordDangerousDownloadWarningShown(DownloadUIModel& model) {
  if (!model.IsDangerous() ||
      model.GetState() == download::DownloadItem::DownloadState::CANCELLED) {
    return;
  }
  if (model.WasUIWarningShown()) {
    return;
  }
  base::UmaHistogramEnumeration("Download.ShowedDownloadWarning",
                                model.GetDangerType(),
                                download::DOWNLOAD_DANGER_TYPE_MAX);
#if !BUILDFLAG(IS_ANDROID)
  base::UmaHistogramEnumeration("SBClientDownload.TailoredWarningType",
                                model.GetTailoredWarningType());
#endif  // BUILDFLAG(IS_ANDROID)
  safe_browsing::RecordDangerousDownloadWarningShown(
      model.GetDangerType(), model.GetTargetFilePath(),
      model.GetURL().SchemeIs(url::kHttpsScheme), model.HasUserGesture());

  model.SetWasUIWarningShown(true);
}

void RecordDownloadOpen(ChromeDownloadOpenMethod open_method,
                        const std::string& mime_type_string) {
  base::RecordAction(base::UserMetricsAction("Download.Open"));
  base::UmaHistogramEnumeration("Download.OpenMethod", open_method,
                                DOWNLOAD_OPEN_METHOD_LAST_ENTRY);
  download::DownloadContent download_content =
      download::DownloadContentFromMimeType(
          mime_type_string, /*record_content_subcategory=*/false);
  base::UmaHistogramEnumeration("Download.Open.ContentType", download_content,
                                download::DownloadContent::MAX);
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

DownloadShelfContextMenuAction DownloadCommandToShelfAction(
    DownloadCommands::Command download_command,
    bool clicked) {
  switch (download_command) {
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
    case DownloadCommands::BYPASS_DEEP_SCANNING_AND_OPEN:
      return clicked
                 ? DownloadShelfContextMenuAction::kBypassDeepScanningClicked
                 : DownloadShelfContextMenuAction::kBypassDeepScanningEnabled;

    // The following are not actually visible in the context menu so should
    // never be logged.
    case DownloadCommands::Command::REVIEW:
    case DownloadCommands::Command::RETRY:
    case DownloadCommands::Command::CANCEL_DEEP_SCAN:
    case DownloadCommands::Command::LEARN_MORE_DOWNLOAD_BLOCKED:
    case DownloadCommands::Command::OPEN_SAFE_BROWSING_SETTING:
    case DownloadCommands::Command::BYPASS_DEEP_SCANNING:
    case DownloadCommands::Command::OPEN_WITH_MEDIA_APP:
    case DownloadCommands::Command::EDIT_WITH_MEDIA_APP:
      NOTREACHED_IN_MIGRATION();
      return DownloadShelfContextMenuAction::kNotReached;
  }
}
