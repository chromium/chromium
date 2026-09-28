// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/cups_print_job_notification.h"

#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/check_deref.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/printing/cups_print_job.h"
#include "chrome/browser/ash/printing/cups_print_job_notification_manager.h"
#include "chrome/browser/ash/printing/cups_print_job_notification_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_utils.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/user_manager/user.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

namespace ash {

namespace {

constexpr char kSettingsPrintingCupsPrintJobUrl[] =
    "chrome://settings/printing/cups-print-job-notification";

constexpr int64_t kSuccessTimeoutSeconds = 8;

constexpr uint32_t kPrintManagementPageButtonIndex = 0;

// This button only appears in notifications for print jobs initiated by the Web
// Printing API.
constexpr uint32_t kWebPrintingContentSettingsButtonIndex = 1;

bool IsPrintJobInitiatedByWebPrintingAPI(const CupsPrintJob& job) {
  return job.source() == ::printing::PrintJob::Source::kIsolatedWebApp;
}

}  // namespace

CupsPrintJobNotification::CupsPrintJobNotification(
    CupsPrintJobNotificationManager* manager,
    base::WeakPtr<CupsPrintJob> print_job,
    Profile* profile)
    : notification_manager_(manager),
      user_(CHECK_DEREF(
          BrowserContextHelper::Get()->GetUserByBrowserContext(profile))),
      notification_id_(CreateUserScopedNotificationId(print_job->GetUniqueId(),
                                                      user_->username_hash())),
      print_job_(print_job),
      profile_(profile),
      is_web_printing_api_initiated_(
          IsPrintJobInitiatedByWebPrintingAPI(*print_job)),
      success_timer_(std::make_unique<base::OneShotTimer>()) {
  OnPrintJobStatusUpdated();
}

std::unique_ptr<message_center::Notification>
CupsPrintJobNotification::CreateNotification() {
  CHECK(print_job_);
  message_center::NotifierId notifier_id(
      message_center::NotifierType::SYSTEM_COMPONENT,
      kSettingsPrintingCupsPrintJobUrl, NotificationCatalogName::kCupsPrintJob);
  notifier_id.profile_id = user_->GetAccountId().GetUserEmail();

  message_center::RichNotificationData optional_fields;
  optional_fields.buttons.emplace_back(
      l10n_util::GetStringUTF16(IDS_PRINT_JOB_PRINTING_PRINT_MANAGEMENT_PAGE));
  if (is_web_printing_api_initiated_) {
    optional_fields.buttons.emplace_back(l10n_util::GetStringUTF16(
        IDS_PRINT_JOB_PRINTING_CONTENT_SETTINGS_PAGE));
  }

  std::unique_ptr<message_center::Notification> notification =
      CreateSystemNotificationPtr(
          message_center::NOTIFICATION_TYPE_SIMPLE, notification_id_,
          /*title=*/std::u16string(), /*message=*/std::u16string(),
          /*display_source=*/
          l10n_util::GetStringUTF16(IDS_PRINT_JOB_NOTIFICATION_DISPLAY_SOURCE),
          notifier_id, optional_fields,
          base::MakeRefCounted<message_center::ThunkNotificationDelegate>(
              weak_factory_.GetWeakPtr()),
          gfx::VectorIcon::EmptyIcon(),
          message_center::SystemNotificationWarningLevel::NORMAL);

  printing::internal::UpdateNotificationTitle(notification.get(), *print_job_);
  printing::internal::UpdateNotificationIcon(notification.get(), *print_job_);
  printing::internal::UpdateNotificationBodyMessage(notification.get(),
                                                    *print_job_, *profile_);
  return notification;
}

CupsPrintJobNotification::~CupsPrintJobNotification() = default;

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
      ash::ShowPrintManagementApp(profile_);
      break;
    case kWebPrintingContentSettingsButtonIndex:
      CHECK(is_web_printing_api_initiated_)
          << "Regular print jobs are not supposed to show permissions button.";
      // Navigates to `chrome://settings/content/webPrinting`.
      chrome::ShowContentSettingsExceptionsForProfile(
          profile_, ContentSettingsType::WEB_PRINTING);
      break;
    default:
      NOTREACHED();
  }
}

void CupsPrintJobNotification::CleanUpNotification() {
  message_center::MessageCenter::Get()->RemoveNotification(notification_id_,
                                                           /*by_user=*/false);
  notification_manager_->OnPrintJobNotificationRemoved(this);
}

void CupsPrintJobNotification::OnPrintJobStatusUpdated() {
  if (!print_job_)
    return;

  if (print_job_->state() == CupsPrintJob::State::STATE_CANCELLED) {
    // Handles the state in which print job was cancelled by the print
    // management app.
    print_job_ = nullptr;
    CleanUpNotification();
    return;
  }

  // |STATE_STARTED| and |STATE_PAGE_DONE| are special since if the user closes
  // the notification in the middle, which means they're not interested in the
  // printing progress, we should prevent showing the following printing
  // progress to the user.
  if ((print_job_->state() != CupsPrintJob::State::STATE_STARTED &&
       print_job_->state() != CupsPrintJob::State::STATE_PAGE_DONE) ||
      !closed_in_middle_) {
    message_center::MessageCenter::Get()->AddNotification(CreateNotification());
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

}  // namespace ash
