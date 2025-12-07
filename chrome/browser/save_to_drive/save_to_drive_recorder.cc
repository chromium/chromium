// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/save_to_drive/save_to_drive_recorder.h"

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/extensions/api/pdf_viewer_private.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace save_to_drive {
namespace {

// LINT.IfChange(PDFSaveToDrivePrimaryAccountSelectionStatus)
enum class PrimaryAccountSelectionStatus {
  kUnknown = 0,
  kPrimaryAccountSelected = 1,
  kPrimaryAccountNotSelected = 2,
  kPrimaryAccountNotAvailable = 3,
  kMaxValue = kPrimaryAccountNotAvailable,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/pdf/enums.xml:PDFSaveToDrivePrimaryAccountSelectionStatus)

void RecordPrimaryAccountSelectionStatus(Profile* profile,
                                         const std::string& account_email) {
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  if (identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    auto primary_account_info =
        identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
    if (primary_account_info.email != account_email) {
      base::UmaHistogramEnumeration(
          "PDF.SaveToDrive.PrimaryAccountSelectionStatus",
          PrimaryAccountSelectionStatus::kPrimaryAccountNotSelected);
    } else {
      base::UmaHistogramEnumeration(
          "PDF.SaveToDrive.PrimaryAccountSelectionStatus",
          PrimaryAccountSelectionStatus::kPrimaryAccountSelected);
    }
  } else {
    base::UmaHistogramEnumeration(
        "PDF.SaveToDrive.PrimaryAccountSelectionStatus",
        PrimaryAccountSelectionStatus::kPrimaryAccountNotAvailable);
  }
}

}  // namespace

SaveToDriveRecorder::SaveToDriveRecorder(Profile* profile)
    : profile_(profile) {}

SaveToDriveRecorder::~SaveToDriveRecorder() = default;

void SaveToDriveRecorder::Record(
    const extensions::api::pdf_viewer_private::SaveToDriveProgress& progress) {
  using extensions::api::pdf_viewer_private::SaveToDriveStatus;
  switch (progress.status) {
    case SaveToDriveStatus::kUploadStarted:
      start_time_ = base::TimeTicks::Now();
      base::UmaHistogramEnumeration("PDF.SaveToDrive.UploadStatus",
                                    progress.status);
      return;
    case SaveToDriveStatus::kUploadCompleted:
      if (start_time_) {
        base::UmaHistogramMediumTimes("PDF.SaveToDrive.UploadLatency",
                                      base::TimeTicks::Now() - *start_time_);
        start_time_.reset();
      }
      base::UmaHistogramMemoryKB("PDF.SaveToDrive.FileSize",
                                 *progress.file_size_bytes / 1024);
      RecordPrimaryAccountSelectionStatus(profile_, *progress.account_email);
      [[fallthrough]];
    case SaveToDriveStatus::kUploadFailed:
      base::UmaHistogramEnumeration("PDF.SaveToDrive.UploadError",
                                    progress.error_type);
      [[fallthrough]];
    case SaveToDriveStatus::kInitiated:
    case SaveToDriveStatus::kAccountChooserShown:
    case SaveToDriveStatus::kAccountSelected:
    case SaveToDriveStatus::kFetchOauth:
    case SaveToDriveStatus::kFetchParentFolder:
    case SaveToDriveStatus::kNone:
    case SaveToDriveStatus::kNotStarted:
    case SaveToDriveStatus::kAccountAddSelected:
    case SaveToDriveStatus::kUploadRetried:
    case SaveToDriveStatus::kAccountAdded:
      base::UmaHistogramEnumeration("PDF.SaveToDrive.UploadStatus",
                                    progress.status);
      return;
    case SaveToDriveStatus::kUploadInProgress:
      // This will be too noisy to record as a separate status.
      return;
  }
  NOTREACHED();
}

}  // namespace save_to_drive
