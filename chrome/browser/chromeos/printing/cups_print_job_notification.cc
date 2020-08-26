// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/cups_print_job_notification.h"

#include "ash/public/cpp/notification_utils.h"
#include "base/feature_list.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/chromeos/printing/cups_print_job.h"
#include "chrome/browser/chromeos/printing/cups_print_job_manager.h"
#include "chrome/browser/chromeos/printing/cups_print_job_manager_factory.h"
#include "chrome/browser/chromeos/printing/cups_print_job_notification_manager.h"
#include "chrome/browser/chromeos/printing/print_management/print_management_uma.h"
#include "chrome/browser/chromeos/printing/printer_error_codes.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/resources/grit/ui_chromeos_resources.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

namespace chromeos {

namespace {

const char kCupsPrintJobNotificationId[] =
    "chrome://settings/printing/cups-print-job-notification";

const int64_t kSuccessTimeoutSeconds = 8;

base::string16 GetNotificationTitleForError(
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
  // Create a notification for the print job. The title, body, icon and buttons
  // of the notification will be updated in UpdateNotification().
  notification_ = std::make_unique<message_center::Notification>(
      message_center::NOTIFICATION_TYPE_SIMPLE, notification_id_,
      base::string16(),  // title
      base::string16(),  // body
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
  if (!base::FeatureList::IsEnabled(features::kPrintJobManagementApp)) {
    // After cancellation, ignore all updates.
    if (cancelled_by_user_) {
      return;
    }
  }

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
    const base::Optional<base::string16>& reply) {
  if (!button_index) {
    if (base::FeatureList::IsEnabled(features::kPrintJobManagementApp)) {
      // If we are in guest mode then we need to use the OffTheRecord profile to
      // open the Print Manageament App. There is a check in Browser::Browser
      // that only OffTheRecord profiles can open browser windows in guest mode.
      chrome::ShowPrintManagementApp(
          profile_->IsGuestSession() ? profile_->GetPrimaryOTRProfile()
                                     : profile_,
          PrintManagementAppEntryPoint::kNotification);
    }
    return;
  }

  if (base::FeatureList::IsEnabled(features::kPrintJobManagementApp)) {
    // Both the "Cancel" and "Get help" buttons are hidden when the print
    // management app is enabled.
    return;
  }

  DCHECK(*button_index >= 0 &&
         static_cast<size_t>(*button_index) < button_commands_.size());

  CupsPrintJobManager* print_job_manager =
      CupsPrintJobManagerFactory::GetForBrowserContext(profile_);

  switch (button_commands_[*button_index]) {
    case ButtonCommand::CANCEL_PRINTING:
      cancelled_by_user_ = true;

      if (print_job_) {
        print_job_manager->CancelPrintJob(print_job_.get());
      }

      // print_job_ was deleted in CancelPrintJob.  Forget the pointer.
      print_job_ = nullptr;

      CleanUpNotification();
      break;
    case ButtonCommand::GET_HELP:
      // Show CUPS printing help page.
      NavigateParams params(profile_, GURL(chrome::kCupsPrintLearnMoreURL),
                            ui::PAGE_TRANSITION_LINK);
      params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
      params.window_action = NavigateParams::SHOW_WINDOW;
      Navigate(&params);
      break;
  }
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
  UpdateNotificationType();
  if (!base::FeatureList::IsEnabled(features::kPrintJobManagementApp)) {
    UpdateNotificationButtons();
  }

  // |STATE_STARTED| and |STATE_PAGE_DONE| are special since if the user closes
  // the notification in the middle, which means they're not interested in the
  // printing progress, we should prevent showing the following printing
  // progress to the user.
  NotificationDisplayService* display_service =
      NotificationDisplayService::GetForProfile(profile_);
  if (print_job_->state() == CupsPrintJob::State::STATE_STARTED ||
      print_job_->state() == CupsPrintJob::State::STATE_PAGE_DONE) {
    // If the notification was closed during the printing, prevent showing the
    // following printing progress.
    if (!closed_in_middle_) {
      display_service->Display(NotificationHandler::Type::TRANSIENT,
                               *notification_, /*metadata=*/nullptr);
    }
  } else {
    display_service->Display(NotificationHandler::Type::TRANSIENT,
                             *notification_, /*metadata=*/nullptr);
    if (print_job_->state() == CupsPrintJob::State::STATE_DOCUMENT_DONE) {
      display_service->Display(NotificationHandler::Type::TRANSIENT,
                               *notification_, /*metadata=*/nullptr);
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
  base::string16 title;
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
  base::string16 message;
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

void CupsPrintJobNotification::UpdateNotificationType() {
  if (!print_job_)
    return;
  switch (print_job_->state()) {
    case CupsPrintJob::State::STATE_WAITING:
    case CupsPrintJob::State::STATE_STARTED:
    case CupsPrintJob::State::STATE_PAGE_DONE:
    case CupsPrintJob::State::STATE_SUSPENDED:
    case CupsPrintJob::State::STATE_RESUMED:
    case CupsPrintJob::State::STATE_ERROR:
      if (base::FeatureList::IsEnabled(features::kPrintJobManagementApp)) {
        // Do not show the progress bar if the print management app is enabled.
        break;
      }
      notification_->set_type(message_center::NOTIFICATION_TYPE_PROGRESS);
      notification_->set_progress(print_job_->printed_page_number() * 100 /
                                  print_job_->total_page_number());
      notification_->set_never_timeout(/*never_timeout=*/true);
      break;
    case CupsPrintJob::State::STATE_NONE:
    case CupsPrintJob::State::STATE_DOCUMENT_DONE:
    case CupsPrintJob::State::STATE_FAILED:
    case CupsPrintJob::State::STATE_CANCELLED:
      notification_->set_never_timeout(/*never_timeout=*/false);
      notification_->set_type(message_center::NOTIFICATION_TYPE_SIMPLE);
      break;
  }
}

void CupsPrintJobNotification::UpdateNotificationButtons() {
  DCHECK(!base::FeatureList::IsEnabled(features::kPrintJobManagementApp));

  std::vector<message_center::ButtonInfo> buttons;
  button_commands_ = GetButtonCommands();
  for (const auto& it : button_commands_) {
    message_center::ButtonInfo button_info =
        message_center::ButtonInfo(GetButtonLabel(it));
    button_info.icon = GetButtonIcon(it);
    buttons.push_back(button_info);
  }
  notification_->set_buttons(buttons);
}

std::vector<CupsPrintJobNotification::ButtonCommand>
CupsPrintJobNotification::GetButtonCommands() const {
  DCHECK(!base::FeatureList::IsEnabled(features::kPrintJobManagementApp));

  if (!print_job_) {
    return {};
  }
  std::vector<CupsPrintJobNotification::ButtonCommand> commands;
  switch (print_job_->state()) {
    case CupsPrintJob::State::STATE_WAITING:
    case CupsPrintJob::State::STATE_STARTED:
    case CupsPrintJob::State::STATE_PAGE_DONE:
    case CupsPrintJob::State::STATE_RESUMED:
    case CupsPrintJob::State::STATE_SUSPENDED:
    case CupsPrintJob::State::STATE_ERROR:
      commands.push_back(ButtonCommand::CANCEL_PRINTING);
      break;
    case CupsPrintJob::State::STATE_FAILED:
    case CupsPrintJob::State::STATE_CANCELLED:
      commands.push_back(ButtonCommand::GET_HELP);
      break;
    default:
      break;
  }
  return commands;
}

base::string16 CupsPrintJobNotification::GetButtonLabel(
    ButtonCommand button) const {
  DCHECK(!base::FeatureList::IsEnabled(features::kPrintJobManagementApp));

  switch (button) {
    case ButtonCommand::CANCEL_PRINTING:
      return l10n_util::GetStringUTF16(
          IDS_PRINT_JOB_NOTIFICATION_CANCEL_BUTTON);
    case ButtonCommand::GET_HELP:
      return l10n_util::GetStringUTF16(
          IDS_PRINT_JOB_NOTIFICATION_GET_HELP_BUTTON);
  }
  return base::string16();
}

gfx::Image CupsPrintJobNotification::GetButtonIcon(ButtonCommand button) const {
  DCHECK(!base::FeatureList::IsEnabled(features::kPrintJobManagementApp));

  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  gfx::Image icon;
  switch (button) {
    case ButtonCommand::CANCEL_PRINTING:
      icon = bundle.GetImageNamed(IDR_PRINT_NOTIFICATION_CANCEL);
      break;
    case ButtonCommand::GET_HELP:
      icon = bundle.GetImageNamed(IDR_PRINT_NOTIFICATION_HELP);
      break;
  }
  return icon;
}

}  // namespace chromeos
