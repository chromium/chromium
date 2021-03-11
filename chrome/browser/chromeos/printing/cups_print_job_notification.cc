// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/cups_print_job_notification.h"

#include "ash/public/cpp/notification_utils.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/chromeos/printing/cups_print_job.h"
#include "chrome/browser/chromeos/printing/cups_print_job_notification_manager.h"
#include "chrome/browser/chromeos/printing/printer_error_codes.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

namespace chromeos {

namespace {

const char kCupsPrintJobNotificationId[] =
    "chrome://settings/printing/cups-print-job-notification";

const int64_t kSuccessTimeoutSeconds = 8;

std::u16string GetNotificationTitleForError(
    const base::WeakPtr<CupsPrintJob>& print_job) {
  DCHECK_EQ(CupsPrintJob::State::STATE_ERROR, print_job->state());

  switch (print_job->error_code()) {
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
    default:
      return l10n_util::GetStringUTF16(IDS_PRINT_JOB_ERROR_NOTIFICATION_TITLE);
  }
}

}  // namespace

CupsPrintJobNotification::CupsPrintJobNotification(
    CupsPrintJobNotificationManager* manager,
    base::WeakPtr<CupsPrintJob> print_job,
    Profile* profile)
    : notification_manager_(manager),
      notification_id_(print_job->GetUniqueId()),
      print_job_(print_job),
      profile_(profile),
      success_timer_(std::make_unique<base::OneShotTimer>()) {
  // Create a notification for the print job. The title, body, and icon of the
  // notification will be updated in UpdateNotification().
  notification_ = std::make_unique<message_center::Notification>(
      message_center::NOTIFICATION_TYPE_SIMPLE, notification_id_,
      std::u16string(),  // title
      std::u16string(),  // body
      gfx::Image(),      // icon
      l10n_util::GetStringUTF16(IDS_PRINT_JOB_NOTIFICATION_DISPLAY_SOURCE),
      GURL(kCupsPrintJobNotificationId),
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kCupsPrintJobNotificationId),
      message_center::RichNotificationData(),
      base::MakeRefCounted<message_center::ThunkNotificationDelegate>(
          weak_factory_.GetWeakPtr()));
  UpdateNotification();
}

CupsPrintJobNotification::~CupsPrintJobNotification() = default;

void CupsPrintJobNotification::OnPrintJobStatusUpdated() {
  UpdateNotification();
}

void CupsPrintJobNotification::Close(bool by_user) {
  if (!by_user)
    return;

  closed_in_middle_ = true;
  if (!print_job_ ||
      print_job_->state() == CupsPrintJob::State::STATE_SUSPENDED) {
    notification_manager_->OnPrintJobNotificationRemoved(this);
  }
}

void CupsPrintJobNotification::Click(
    const base::Optional<int>& button_index,
    const base::Optional<std::u16string>& reply) {
  // If we are in guest mode then we need to use the OffTheRecord profile to
  // open the Print Manageament App. There is a check in Browser::Browser
  // that only OffTheRecord profiles can open browser windows in guest mode.
  chrome::ShowPrintManagementApp(
      profile_->IsGuestSession() ? profile_->GetPrimaryOTRProfile() : profile_);
}

void CupsPrintJobNotification::CleanUpNotification() {
  NotificationDisplayService::GetForProfile(profile_)->Close(
      NotificationHandler::Type::TRANSIENT, notification_id_);
  notification_manager_->OnPrintJobNotificationRemoved(this);
}

void CupsPrintJobNotification::UpdateNotification() {
  if (!print_job_)
    return;

  if (print_job_->state() == CupsPrintJob::State::STATE_CANCELLED) {
    // Handles the state in which print job was cancelled by the print
    // management app.
    print_job_ = nullptr;
    CleanUpNotification();
    return;
  }

  UpdateNotificationTitle();
  UpdateNotificationIcon();
  UpdateNotificationBodyMessage();
  UpdateNotificationTimeout();

  // |STATE_STARTED| and |STATE_PAGE_DONE| are special since if the user closes
  // the notification in the middle, which means they're not interested in the
  // printing progress, we should prevent showing the following printing
  // progress to the user.
  NotificationDisplayService* display_service =
      NotificationDisplayService::GetForProfile(profile_);
  if ((print_job_->state() != CupsPrintJob::State::STATE_STARTED &&
       print_job_->state() != CupsPrintJob::State::STATE_PAGE_DONE) ||
      !closed_in_middle_) {
    display_service->Display(NotificationHandler::Type::TRANSIENT,
                             *notification_, /*metadata=*/nullptr);
    if (print_job_->state() == CupsPrintJob::State::STATE_DOCUMENT_DONE) {
      success_timer_->Start(
          FROM_HERE, base::TimeDelta::FromSeconds(kSuccessTimeoutSeconds),
          base::BindOnce(&CupsPrintJobNotification::CleanUpNotification,
                         base::Unretained(this)));
    }
  }

  // |print_job_| will be deleted by CupsPrintJobManager if the job is finished
  // and we are not supposed to get any notification update after that.
  if (print_job_->IsJobFinished())
    print_job_ = nullptr;
}

void CupsPrintJobNotification::UpdateNotificationTitle() {
  if (!print_job_)
    return;
  std::u16string title;
  switch (print_job_->state()) {
    case CupsPrintJob::State::STATE_WAITING:
    case CupsPrintJob::State::STATE_STARTED:
    case CupsPrintJob::State::STATE_PAGE_DONE:
    case CupsPrintJob::State::STATE_SUSPENDED:
    case CupsPrintJob::State::STATE_RESUMED:
      title =
          l10n_util::GetStringUTF16(IDS_PRINT_JOB_PRINTING_NOTIFICATION_TITLE);
      break;
    case CupsPrintJob::State::STATE_DOCUMENT_DONE:
      title = l10n_util::GetStringUTF16(IDS_PRINT_JOB_DONE_NOTIFICATION_TITLE);
      break;
    case CupsPrintJob::State::STATE_CANCELLED:
    case CupsPrintJob::State::STATE_FAILED:
      title = l10n_util::GetStringUTF16(IDS_PRINT_JOB_ERROR_NOTIFICATION_TITLE);
      break;
    case CupsPrintJob::State::STATE_ERROR:
      title = GetNotificationTitleForError(print_job_);
      break;
    default:
      break;
  }
  notification_->set_title(title);
}

void CupsPrintJobNotification::UpdateNotificationIcon() {
  if (!print_job_)
    return;
  switch (print_job_->state()) {
    case CupsPrintJob::State::STATE_WAITING:
    case CupsPrintJob::State::STATE_STARTED:
    case CupsPrintJob::State::STATE_PAGE_DONE:
    case CupsPrintJob::State::STATE_SUSPENDED:
    case CupsPrintJob::State::STATE_RESUMED:
      notification_->set_accent_color(ash::kSystemNotificationColorNormal);
      notification_->set_vector_small_image(kNotificationPrintingIcon);
      break;
    case CupsPrintJob::State::STATE_DOCUMENT_DONE:
      notification_->set_accent_color(ash::kSystemNotificationColorNormal);
      notification_->set_vector_small_image(kNotificationPrintingDoneIcon);
      break;
    case CupsPrintJob::State::STATE_CANCELLED:
    case CupsPrintJob::State::STATE_FAILED:
    case CupsPrintJob::State::STATE_ERROR:
      notification_->set_accent_color(ash::kSystemNotificationColorWarning);
      notification_->set_vector_small_image(kNotificationPrintingWarningIcon);
      break;
    case CupsPrintJob::State::STATE_NONE:
      break;
  }
}

void CupsPrintJobNotification::UpdateNotificationBodyMessage() {
  if (!print_job_)
    return;
  std::u16string message;
  if (print_job_->total_page_number() > 1) {
    message = l10n_util::GetStringFUTF16(
        IDS_PRINT_JOB_NOTIFICATION_MESSAGE,
        base::NumberToString16(print_job_->total_page_number()),
        base::UTF8ToUTF16(print_job_->printer().display_name()));
  } else {
    message = l10n_util::GetStringFUTF16(
        IDS_PRINT_JOB_NOTIFICATION_SINGLE_PAGE_MESSAGE,
        base::UTF8ToUTF16(print_job_->printer().display_name()));
  }
  notification_->set_message(message);
}

void CupsPrintJobNotification::UpdateNotificationTimeout() {
  if (!print_job_)
    return;

  switch (print_job_->state()) {
    case CupsPrintJob::State::STATE_WAITING:
    case CupsPrintJob::State::STATE_STARTED:
    case CupsPrintJob::State::STATE_PAGE_DONE:
    case CupsPrintJob::State::STATE_SUSPENDED:
    case CupsPrintJob::State::STATE_RESUMED:
    case CupsPrintJob::State::STATE_ERROR:
      break;
    case CupsPrintJob::State::STATE_NONE:
    case CupsPrintJob::State::STATE_DOCUMENT_DONE:
    case CupsPrintJob::State::STATE_FAILED:
    case CupsPrintJob::State::STATE_CANCELLED:
      notification_->set_never_timeout(/*never_timeout=*/false);
      break;
  }
}

}  // namespace chromeos
