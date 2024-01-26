// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/cups_print_job_notification_utils.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ash/printing/cups_print_job.h"
#include "chrome/browser/chromeos/printing/printer_error_codes.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/message_center/public/cpp/notification.h"

namespace ash {

namespace {

using ::chromeos::PrinterErrorCode;

std::u16string GetNotificationTitleForFailure(const CupsPrintJob& job) {
  DCHECK_EQ(CupsPrintJob::State::STATE_FAILED, job.state());

  switch (job.error_code()) {
    case PrinterErrorCode::CLIENT_UNAUTHORIZED:
      return l10n_util::GetStringUTF16(
          IDS_PRINT_JOB_AUTHORIZATION_ERROR_NOTIFICATION_TITLE);
    case PrinterErrorCode::EXPIRED_CERTIFICATE:
      return l10n_util::GetStringUTF16(
          IDS_PRINT_JOB_EXPIRED_CERT_ERROR_NOTIFICATION_TITLE);
    default:
      return l10n_util::GetStringUTF16(IDS_PRINT_JOB_ERROR_NOTIFICATION_TITLE);
  }
}

std::u16string GetNotificationTitleForError(const CupsPrintJob& job) {
  DCHECK_EQ(CupsPrintJob::State::STATE_ERROR, job.state());

  switch (job.error_code()) {
    case PrinterErrorCode::PAPER_JAM:
      return l10n_util::GetStringUTF16(
          IDS_PRINT_JOB_PAPER_JAM_NOTIFICATION_TITLE);
    case PrinterErrorCode::OUT_OF_INK:
      return l10n_util::GetStringUTF16(
          IDS_PRINT_JOB_OUT_OF_INK_NOTIFICATION_TITLE);
    case PrinterErrorCode::OUT_OF_PAPER:
      return l10n_util::GetStringUTF16(
          IDS_PRINT_JOB_OUT_OF_PAPER_NOTIFICATION_TITLE);
    case PrinterErrorCode::DOOR_OPEN:
      return l10n_util::GetStringUTF16(
          IDS_PRINT_JOB_DOOR_OPEN_NOTIFICATION_TITLE);
    case PrinterErrorCode::PRINTER_UNREACHABLE:
      return l10n_util::GetStringUTF16(
          IDS_PRINT_JOB_PRINTER_UNREACHABLE_NOTIFICATION_TITLE);
    case PrinterErrorCode::TRAY_MISSING:
      return l10n_util::GetStringUTF16(
          IDS_PRINT_JOB_TRAY_MISSING_NOTIFICATION_TITLE);
    case PrinterErrorCode::OUTPUT_FULL:
      return l10n_util::GetStringUTF16(
          IDS_PRINT_JOB_OUTPUT_FULL_NOTIFICATION_TITLE);
    case PrinterErrorCode::STOPPED:
      return l10n_util::GetStringUTF16(
          IDS_PRINT_JOB_STOPPED_NOTIFICATION_TITLE);
    case PrinterErrorCode::EXPIRED_CERTIFICATE:
      return l10n_util::GetStringUTF16(
          IDS_PRINT_JOB_EXPIRED_CERT_ERROR_NOTIFICATION_TITLE);
    default:
      return l10n_util::GetStringUTF16(IDS_PRINT_JOB_ERROR_NOTIFICATION_TITLE);
  }
}

std::u16string GetNotificationBodyMessageForUnauthorizedClient(
    const CupsPrintJob& job,
    const Profile& profile) {
  const std::u16string printer_name =
      base::UTF8ToUTF16(job.printer().display_name());
  bool send_username_and_filename_policy_enabled =
      profile.GetPrefs()->GetBoolean(
          prefs::kPrintingSendUsernameAndFilenameEnabled);
  if (send_username_and_filename_policy_enabled) {
    return l10n_util::GetStringFUTF16(
        IDS_PRINT_JOB_NOTIFICATION_CLIENT_UNAUTHORIZED_MESSAGE,
        base::UTF8ToUTF16(profile.GetProfileUserName()), printer_name);
  } else {
    return l10n_util::GetStringFUTF16(
        IDS_PRINT_JOB_NOTIFICATION_IDENTIFICATION_REQUIRED_MESSAGE,
        printer_name);
  }
}

std::u16string GetGenericNotificationBodyMessage(const CupsPrintJob& job) {
  const std::u16string printer_name =
      base::UTF8ToUTF16(job.printer().display_name());
  if (job.total_page_number() > 1) {
    const std::u16string pages =
        base::NumberToString16(job.total_page_number());
    return l10n_util::GetStringFUTF16(IDS_PRINT_JOB_NOTIFICATION_MESSAGE, pages,
                                      printer_name);
  } else {
    return l10n_util::GetStringFUTF16(
        IDS_PRINT_JOB_NOTIFICATION_SINGLE_PAGE_MESSAGE, printer_name);
  }
}

}  // namespace

namespace printing::internal {

void UpdateNotificationTitle(message_center::Notification* notification,
                             const CupsPrintJob& job) {
  switch (job.state()) {
    case CupsPrintJob::State::STATE_WAITING:
    case CupsPrintJob::State::STATE_STARTED:
    case CupsPrintJob::State::STATE_PAGE_DONE:
    case CupsPrintJob::State::STATE_SUSPENDED:
    case CupsPrintJob::State::STATE_RESUMED:
      notification->set_title(
          l10n_util::GetStringUTF16(IDS_PRINT_JOB_PRINTING_NOTIFICATION_TITLE));
      break;
    case CupsPrintJob::State::STATE_DOCUMENT_DONE:
      notification->set_title(
          l10n_util::GetStringUTF16(IDS_PRINT_JOB_DONE_NOTIFICATION_TITLE));
      break;
    case CupsPrintJob::State::STATE_CANCELLED:
    case CupsPrintJob::State::STATE_FAILED:
      notification->set_title(GetNotificationTitleForFailure(job));
      break;
    case CupsPrintJob::State::STATE_ERROR:
      notification->set_title(GetNotificationTitleForError(job));
      break;
    default:
      break;
  }
}

void UpdateNotificationIcon(message_center::Notification* notification,
                            const CupsPrintJob& job) {
  switch (job.state()) {
    case CupsPrintJob::State::STATE_WAITING:
    case CupsPrintJob::State::STATE_STARTED:
    case CupsPrintJob::State::STATE_PAGE_DONE:
    case CupsPrintJob::State::STATE_SUSPENDED:
    case CupsPrintJob::State::STATE_RESUMED:
      notification->set_accent_color_id(cros_tokens::kCrosSysPrimary);
      notification->set_vector_small_image(kNotificationPrintingIcon);
      break;
    case CupsPrintJob::State::STATE_DOCUMENT_DONE:
      notification->set_accent_color_id(cros_tokens::kCrosSysPrimary);
      notification->set_vector_small_image(kNotificationPrintingDoneIcon);
      break;
    case CupsPrintJob::State::STATE_CANCELLED:
    case CupsPrintJob::State::STATE_FAILED:
    case CupsPrintJob::State::STATE_ERROR:
      notification->set_accent_color_id(cros_tokens::kCrosSysError);
      notification->set_vector_small_image(kNotificationPrintingWarningIcon);
      break;
    case CupsPrintJob::State::STATE_NONE:
      break;
  }
}

void UpdateNotificationBodyMessage(message_center::Notification* notification,
                                   const CupsPrintJob& job,
                                   Profile& profile) {
  if (job.error_code() == PrinterErrorCode::CLIENT_UNAUTHORIZED) {
    notification->set_message(
        GetNotificationBodyMessageForUnauthorizedClient(job, profile));
    return;
  }
  notification->set_message(GetGenericNotificationBodyMessage(job));
}

}  // namespace printing::internal

}  // namespace ash
