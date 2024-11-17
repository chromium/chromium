// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/crostini_export_import_status_tracker.h"

#include "base/check.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/crostini/crostini_export_import.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/text/bytes_formatting.h"

namespace crostini {

CrostiniExportImportStatusTracker::CrostiniExportImportStatusTracker(
    ExportImportType type,
    base::FilePath path)
    : type_(type), path_(path) {
  DCHECK(type == ExportImportType::EXPORT || type == ExportImportType::IMPORT ||
         type == ExportImportType::EXPORT_DISK_IMAGE ||
         type == ExportImportType::IMPORT_DISK_IMAGE);
}

CrostiniExportImportStatusTracker::~CrostiniExportImportStatusTracker() =
    default;

void CrostiniExportImportStatusTracker::SetStatusRunning(int progress_percent) {
  DCHECK(status_ == Status::NONE || status_ == Status::RUNNING ||
         status_ == Status::CANCELLING);
  // Progress updates can still be received while the notification is being
  // cancelled. These should not be displayed, as the operation will eventually
  // cancel (or fail to cancel).
  if (status_ == Status::CANCELLING) {
    return;
  }

  status_ = Status::RUNNING;
  SetStatusRunningUI(progress_percent);
}

void CrostiniExportImportStatusTracker::SetStatusCancelling() {
  DCHECK(status_ == Status::RUNNING);

  status_ = Status::CANCELLING;
  SetStatusCancellingUI();
}

void CrostiniExportImportStatusTracker::SetStatusDone() {
  DCHECK(status_ == Status::RUNNING ||
         (type() == ExportImportType::IMPORT && status_ == Status::CANCELLING));

  status_ = Status::DONE;
  SetStatusDoneUI();
}

void CrostiniExportImportStatusTracker::SetStatusCancelled() {
  DCHECK(status_ == Status::NONE || status_ == Status::CANCELLING);

  status_ = Status::CANCELLED;
  SetStatusCancelledUI();
}

void CrostiniExportImportStatusTracker::SetStatusFailed() {
  SetStatusFailedWithMessage(
      Status::FAILED_UNKNOWN_REASON,
      l10n_util::GetStringUTF16(
          (type() == ExportImportType::EXPORT ||
           type() == ExportImportType::EXPORT_DISK_IMAGE)
              ? IDS_CROSTINI_EXPORT_NOTIFICATION_MESSAGE_FAILED
              : IDS_CROSTINI_IMPORT_NOTIFICATION_MESSAGE_FAILED));
}

void CrostiniExportImportStatusTracker::SetStatusFailedArchitectureMismatch(
    const std::string& architecture_container,
    const std::string& architecture_device) {
  DCHECK(type() == ExportImportType::IMPORT);
  SetStatusFailedWithMessage(
      Status::FAILED_ARCHITECTURE_MISMATCH,
      l10n_util::GetStringFUTF16(
          IDS_CROSTINI_IMPORT_NOTIFICATION_MESSAGE_FAILED_ARCHITECTURE,
          base::ASCIIToUTF16(architecture_container),
          base::ASCIIToUTF16(architecture_device)));
}

void CrostiniExportImportStatusTracker::SetStatusFailedInsufficientSpace(
    uint64_t additional_required_space) {
  DCHECK(type() == ExportImportType::IMPORT);
  SetStatusFailedWithMessage(
      Status::FAILED_INSUFFICIENT_SPACE,
      l10n_util::GetStringFUTF16(
          IDS_CROSTINI_IMPORT_NOTIFICATION_MESSAGE_FAILED_SPACE,
          ui::FormatBytes(additional_required_space)));
}

void CrostiniExportImportStatusTracker::
    SetStatusFailedInsufficientSpaceUnknownAmount() {
  DCHECK(type() == ExportImportType::IMPORT ||
         type() == ExportImportType::IMPORT_DISK_IMAGE);
  SetStatusFailedWithMessage(
      Status::FAILED_INSUFFICIENT_SPACE,
      l10n_util::GetStringUTF16(
          IDS_CROSTINI_IMPORT_NOTIFICATION_MESSAGE_FAILED_SPACE_UNKNOWN_AMOUNT));
}

void CrostiniExportImportStatusTracker::SetStatusFailedConcurrentOperation(
    ExportImportType in_progress_operation_type) {
  SetStatusFailedWithMessage(
      Status::FAILED_CONCURRENT_OPERATION,
      l10n_util::GetStringUTF16(
          in_progress_operation_type == ExportImportType::EXPORT
              ? IDS_CROSTINI_EXPORT_NOTIFICATION_MESSAGE_FAILED_IN_PROGRESS
              : IDS_CROSTINI_IMPORT_NOTIFICATION_MESSAGE_FAILED_IN_PROGRESS));
}

void CrostiniExportImportStatusTracker::SetStatusFailedWithMessage(
    Status status,
    const std::u16string& message) {
  DCHECK(status_ == Status::RUNNING || status_ == Status::CANCELLING);
  status_ = status;
  SetStatusFailedWithMessageUI(status, message);
}

}  //  namespace crostini
