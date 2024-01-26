// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/cups_print_job_notification.h"

#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/notification_utils.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ash/printing/cups_print_job.h"
#include "chrome/browser/ash/printing/cups_print_job_notification_manager.h"
#include "chrome/browser/ash/printing/cups_print_job_notification_utils.h"
#include "chrome/browser/chromeos/printing/printer_error_codes.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/image/image.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

namespace ash {

namespace {

constexpr char kCupsPrintJobNotificationId[] =
    "chrome://settings/printing/cups-print-job-notification";

constexpr int64_t kSuccessTimeoutSeconds = 8;

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
      /*title=*/std::u16string(), /*body=*/std::u16string(),
      /*icon=*/ui::ImageModel(),
      /*display_source=*/
      l10n_util::GetStringUTF16(IDS_PRINT_JOB_NOTIFICATION_DISPLAY_SOURCE),
      GURL(kCupsPrintJobNotificationId),
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kCupsPrintJobNotificationId,
                                 NotificationCatalogName::kCupsPrintJob),
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
    const std::optional<int>& button_index,
    const std::optional<std::u16string>& reply) {
  // If we are in guest mode then we need to use the OffTheRecord profile to
  // open the Print Manageament App. There is a check in Browser::Browser
  // that only OffTheRecord profiles can open browser windows in guest mode.
  chrome::ShowPrintManagementApp(
      profile_->IsGuestSession()
          ? profile_->GetPrimaryOTRProfile(/*create_if_needed=*/true)
          : profile_.get());
}

message_center::Notification*
CupsPrintJobNotification::GetNotificationDataForTesting() {
  return notification_.get();
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

  // |STATE_STARTED| and |STATE_PAGE_DONE| are special since if the user closes
  // the notification in the middle, which means they're not interested in the
  // printing progress, we should prevent showing the following printing
  // progress to the user.
  if ((print_job_->state() != CupsPrintJob::State::STATE_STARTED &&
       print_job_->state() != CupsPrintJob::State::STATE_PAGE_DONE) ||
      !closed_in_middle_) {
    NotificationDisplayService::GetForProfile(profile_)->Display(
        NotificationHandler::Type::TRANSIENT, *notification_,
        /*metadata=*/nullptr);
    if (print_job_->state() == CupsPrintJob::State::STATE_DOCUMENT_DONE) {
      success_timer_->Start(
          FROM_HERE, base::Seconds(kSuccessTimeoutSeconds),
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
  if (!print_job_) {
    return;
  }
  printing::internal::UpdateNotificationTitle(notification_.get(), *print_job_);
}

void CupsPrintJobNotification::UpdateNotificationIcon() {
  if (!print_job_) {
    return;
  }
  printing::internal::UpdateNotificationIcon(notification_.get(), *print_job_);
}

void CupsPrintJobNotification::UpdateNotificationBodyMessage() {
  if (!print_job_) {
    return;
  }
  printing::internal::UpdateNotificationBodyMessage(notification_.get(),
                                                    *print_job_, *profile_);
}

}  // namespace ash
