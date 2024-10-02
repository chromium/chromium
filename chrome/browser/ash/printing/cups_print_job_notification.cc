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
#include "chrome/browser/notifications/notification_display_service_factory.h"
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

constexpr uint32_t kPrintManagementPageButtonIndex = 0;

// This button only appears in notifications for print jobs initiated by the Web
// Printing API.
constexpr uint32_t kWebPrintingContentSettingsButtonIndex = 1;

bool IsPrintJobInitiatedByWebPrintingAPI(const CupsPrintJob& job) {
  return job.source() == crosapi::mojom::PrintJob::Source::kIsolatedWebApp;
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
      is_web_printing_api_initiated_(
          IsPrintJobInitiatedByWebPrintingAPI(*print_job)),
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
  std::vector<message_center::ButtonInfo> buttons;
  buttons.emplace_back(
      l10n_util::GetStringUTF16(IDS_PRINT_JOB_PRINTING_PRINT_MANAGEMENT_PAGE));
  if (is_web_printing_api_initiated_) {
    buttons.emplace_back(l10n_util::GetStringUTF16(
        IDS_PRINT_JOB_PRINTING_CONTENT_SETTINGS_PAGE));
  }
  notification_->set_buttons(buttons);
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
  if (!button_index) {
    return;
  }

  switch (*button_index) {
    case kPrintManagementPageButtonIndex:
      chrome::ShowPrintManagementApp(profile_);
      break;
    case kWebPrintingContentSettingsButtonIndex:
      CHECK(is_web_printing_api_initiated_)
          << "Regular print jobs are not supposed to show permissions button.";
      // Navigates to `chrome://settings/content/webPrinting`.
      chrome::ShowContentSettingsExceptionsForProfile(
          profile_, ContentSettingsType::WEB_PRINTING);
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }
}

message_center::Notification*
CupsPrintJobNotification::GetNotificationDataForTesting() {
  return notification_.get();
}

void CupsPrintJobNotification::CleanUpNotification() {
  NotificationDisplayServiceFactory::GetForProfile(profile_)->Close(
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
    NotificationDisplayServiceFactory::GetForProfile(profile_)->Display(
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
