// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/crostini_package_notification.h"

#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "chrome/browser/ash/app_list/app_list_client_impl.h"
#include "chrome/browser/ash/crostini/crostini_package_service.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service_factory.h"
#include "chrome/browser/ash/guest_os/guest_os_terminal.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/ui/views/crostini/crostini_package_install_failure_view.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

namespace crostini {

namespace {

constexpr char kNotifierCrostiniPackageOperation[] =
    "crostini.package_operation";

}  // namespace

int CrostiniPackageNotification::GetButtonCountForTesting() {
  return notification_->buttons().size();
}

const std::string& CrostiniPackageNotification::GetErrorMessageForTesting()
    const {
  return error_message_;
}

CrostiniPackageNotification::NotificationSettings::NotificationSettings() {}
CrostiniPackageNotification::NotificationSettings::NotificationSettings(
    const NotificationSettings& rhs) = default;
CrostiniPackageNotification::NotificationSettings::~NotificationSettings() {}

CrostiniPackageNotification::CrostiniPackageNotification(
    Profile* profile,
    NotificationType notification_type,
    PackageOperationStatus status,
    const guest_os::GuestId& container_id,
    const std::u16string& app_name,
    const std::string& notification_id,
    CrostiniPackageService* package_service)
    : notification_type_(notification_type),
      current_status_(status),
      package_service_(package_service),
      profile_(profile),
      notification_settings_(
          GetNotificationSettingsForTypeAndAppName(notification_type,
                                                   app_name)),
      visible_(true),
      container_id_(container_id) {
  if (status == PackageOperationStatus::RUNNING) {
    running_start_time_ = base::TimeTicks::Now();
    guest_os::GuestOsRegistryServiceFactory::GetForProfile(profile_)
        ->AddObserver(this);
  }
  message_center::RichNotificationData rich_notification_data;
  rich_notification_data.vector_small_image = &ash::kNotificationLinuxIcon;
  rich_notification_data.never_timeout = true;
  rich_notification_data.accent_color_id = cros_tokens::kCrosSysPrimary;

  notification_ = std::make_unique<message_center::Notification>(
      message_center::NOTIFICATION_TYPE_PROGRESS, notification_id,
      std::u16string(), std::u16string(),
      ui::ImageModel(),  // icon
      notification_settings_.source,
      GURL(),  // origin_url
      message_center::NotifierId(
          message_center::NotifierType::SYSTEM_COMPONENT,
          kNotifierCrostiniPackageOperation,
          ash::NotificationCatalogName::kCrostiniPackage),
      rich_notification_data,
      base::MakeRefCounted<message_center::ThunkNotificationDelegate>(
          weak_ptr_factory_.GetWeakPtr()));

  // Sets title and body
  UpdateProgress(status, 0 /*progress_percent*/);
}

CrostiniPackageNotification::~CrostiniPackageNotification() {
  guest_os::GuestOsRegistryServiceFactory::GetForProfile(profile_)
      ->RemoveObserver(this);
}

PackageOperationStatus CrostiniPackageNotification::GetOperationStatus() const {
  return current_status_;
}

void CrostiniPackageNotification::OnRegistryUpdated(
    guest_os::GuestOsRegistryService* registry_service,
    guest_os::VmType vm_type,
    const std::vector<std::string>& updated_apps,
    const std::vector<std::string>& removed_apps,
    const std::vector<std::string>& inserted_apps) {
  if (vm_type != guest_os::VmType::TERMINA) {
    return;
  }
  inserted_apps_.insert(inserted_apps.begin(), inserted_apps.end());
}

// static
CrostiniPackageNotification::NotificationSettings
CrostiniPackageNotification::GetNotificationSettingsForTypeAndAppName(
    NotificationType notification_type,
    const std::u16string& app_name) {
  NotificationSettings result;

  switch (notification_type) {
    case NotificationType::PACKAGE_INSTALL:
      DCHECK(app_name.empty());
      result.source = l10n_util::GetStringUTF16(
          IDS_CROSTINI_PACKAGE_INSTALL_NOTIFICATION_DISPLAY_SOURCE);
      result.queued_title = l10n_util::GetStringUTF16(
          IDS_CROSTINI_PACKAGE_INSTALL_NOTIFICATION_QUEUED_TITLE);
      result.progress_title = l10n_util::GetStringUTF16(
          IDS_CROSTINI_PACKAGE_INSTALL_NOTIFICATION_IN_PROGRESS_TITLE);
      result.progress_body.clear();
      result.success_title = l10n_util::GetStringUTF16(
          IDS_CROSTINI_PACKAGE_INSTALL_NOTIFICATION_COMPLETED_TITLE);
      result.success_body = l10n_util::GetStringUTF16(
          IDS_CROSTINI_PACKAGE_INSTALL_NOTIFICATION_COMPLETED_MESSAGE);
      result.failure_title = l10n_util::GetStringUTF16(
          IDS_CROSTINI_PACKAGE_INSTALL_NOTIFICATION_ERROR_TITLE);
      result.failure_body = l10n_util::GetStringUTF16(
          IDS_CROSTINI_PACKAGE_INSTALL_NOTIFICATION_ERROR_MESSAGE);
      break;

    case NotificationType::APPLICATION_UNINSTALL:
      result.source = l10n_util::GetStringUTF16(
          IDS_CROSTINI_APPLICATION_UNINSTALL_NOTIFICATION_DISPLAY_SOURCE);
      result.queued_title = l10n_util::GetStringFUTF16(
          IDS_CROSTINI_APPLICATION_UNINSTALL_NOTIFICATION_QUEUED_TITLE,
          app_name);
      result.queued_body = l10n_util::GetStringUTF16(
          IDS_CROSTINI_APPLICATION_UNINSTALL_NOTIFICATION_QUEUED_MESSAGE);
      result.progress_title = l10n_util::GetStringFUTF16(
          IDS_CROSTINI_APPLICATION_UNINSTALL_NOTIFICATION_IN_PROGRESS_TITLE,
          app_name);
      result.success_title = l10n_util::GetStringFUTF16(
          IDS_CROSTINI_APPLICATION_UNINSTALL_NOTIFICATION_COMPLETED_TITLE,
          app_name);
      result.success_body = l10n_util::GetStringUTF16(
          IDS_CROSTINI_APPLICATION_UNINSTALL_NOTIFICATION_COMPLETED_MESSAGE);
      result.failure_title = l10n_util::GetStringFUTF16(
          IDS_CROSTINI_APPLICATION_UNINSTALL_NOTIFICATION_ERROR_TITLE,
          app_name);
      result.failure_body = l10n_util::GetStringUTF16(
          IDS_CROSTINI_APPLICATION_UNINSTALL_NOTIFICATION_ERROR_MESSAGE);
      break;

    default:
      NOTREACHED_IN_MIGRATION();
  }

  return result;
}

void CrostiniPackageNotification::UpdateProgress(
    PackageOperationStatus status,
    int progress_percent,
    const std::string& error_message) {
  if (status == PackageOperationStatus::RUNNING &&
      current_status_ != PackageOperationStatus::RUNNING) {
    running_start_time_ = base::TimeTicks::Now();
    guest_os::GuestOsRegistryServiceFactory::GetForProfile(profile_)
        ->AddObserver(this);
  }
  current_status_ = status;

  std::u16string title;
  std::u16string body;
  std::vector<message_center::ButtonInfo> buttons;
  message_center::NotificationType notification_type =
      message_center::NOTIFICATION_TYPE_SIMPLE;
  bool never_timeout = false;
  app_count_ = 0;
  auto* registry_service =
      guest_os::GuestOsRegistryServiceFactory::GetForProfile(profile_);

  switch (status) {
    case PackageOperationStatus::SUCCEEDED:
      title = notification_settings_.success_title;
      body = notification_settings_.success_body;

      if (notification_type_ == NotificationType::PACKAGE_INSTALL) {
        // Try and match up launcher icons with the install we just finished. We
        // don't have a perfect solution to this, but under normal circumstances
        // we shouldn't see icons appearing during an install that aren't
        // because of that install.
        for (const std::string& app_id : inserted_apps_) {
          auto registration = registry_service->GetRegistration(app_id);
          if (registration.has_value() &&
              registration->VmName() == container_id_.vm_name &&
              registration->ContainerName() == container_id_.container_name) {
            app_id_ = app_id;
            app_count_++;
          }
        }
        if (app_count_ == 1) {
          buttons.push_back(
              message_center::ButtonInfo(l10n_util::GetStringUTF16(
                  IDS_CROSTINI_PACKAGE_INSTALL_NOTIFICATION_COMPLETED_BUTTON)));
        }
      }
      registry_service->RemoveObserver(this);

      break;

    case PackageOperationStatus::FAILED: {
      title = notification_settings_.failure_title;
      body = notification_settings_.failure_body;
      error_message_ = error_message;
      notification_->set_accent_color_id(cros_tokens::kCrosSysError);
      break;
    }

    case PackageOperationStatus::WAITING_FOR_APP_REGISTRY_UPDATE:
      // If a notification progress bar is set to a value outside of [0, 100],
      // it becomes in infinite progress bar. Do that here because we have no
      // way to know how long this will take or how close we are to completion.
      progress_percent = -1;
      [[fallthrough]];
    case PackageOperationStatus::RUNNING:
      never_timeout = true;
      notification_type = message_center::NOTIFICATION_TYPE_PROGRESS;
      title = notification_settings_.progress_title;
      if (notification_type_ == NotificationType::APPLICATION_UNINSTALL &&
          progress_percent >= 0) {
        // Uninstalls have a time remaining instead of a fixed message.
        body = GetTimeRemainingMessage(running_start_time_, progress_percent);

        // else leave body blank
      } else {
        body = notification_settings_.progress_body;
      }
      break;

    case PackageOperationStatus::QUEUED:
      title = notification_settings_.queued_title;
      body = notification_settings_.queued_body;
      break;

    default:
      NOTREACHED_IN_MIGRATION();
  }

  notification_->set_title(title);
  notification_->set_message(body);
  notification_->set_buttons(buttons);
  notification_->set_type(notification_type);
  notification_->set_progress(progress_percent);
  notification_->set_never_timeout(never_timeout);
  UpdateDisplayedNotification();
}

void CrostiniPackageNotification::ForceAllowAutoHide() {
  notification_->set_never_timeout(false);
  UpdateDisplayedNotification();
}

void CrostiniPackageNotification::Close(bool by_user) {
  if (current_status_ != PackageOperationStatus::SUCCEEDED &&
      current_status_ != PackageOperationStatus::FAILED) {
    // We don't want to delete ourselves yet; we want to forcibly redisplay
    // when we hit success or failure. Just note that we are hidden.
    visible_ = false;
  } else {
    // This call deletes us.
    package_service_->NotificationCompleted(this);
  }
}

void CrostiniPackageNotification::Click(
    const std::optional<int>& button_index,
    const std::optional<std::u16string>& reply) {
  if (current_status_ == PackageOperationStatus::FAILED) {
    crostini::ShowCrostiniPackageInstallFailureView(error_message_);
  }

  if (current_status_ != PackageOperationStatus::SUCCEEDED) {
    return;
  }

  if (app_count_ == 0) {
    LaunchTerminal(profile_,
                   display::Screen::GetScreen()->GetPrimaryDisplay().id(),
                   DefaultContainerId());
  } else if (app_count_ == 1) {
    DCHECK(!app_id_.empty());
    LaunchCrostiniApp(profile_, app_id_,
                      display::Screen::GetScreen()->GetPrimaryDisplay().id());
  } else {
    AppListClientImpl::GetInstance()->ShowAppList(
        ash::AppListShowSource::kBrowser);
  }
}

void CrostiniPackageNotification::UpdateDisplayedNotification() {
  if (current_status_ == PackageOperationStatus::SUCCEEDED ||
      current_status_ == PackageOperationStatus::FAILED) {
    // If the user closes the notification when it is queued or running, we
    // still want to tell them when it is actually finished. So force the
    // notification back to visibility when we get our success / fail notice.
    // Note that we only get one success / fail notice, so we won't keep
    // reshowing this.
    visible_ = true;
  }

  if (!visible_) {
    // User hid, don't re-display.
    return;
  }

  NotificationDisplayService* display_service =
      NotificationDisplayServiceFactory::GetForProfile(profile_);
  display_service->Display(NotificationHandler::Type::TRANSIENT, *notification_,
                           /*metadata=*/nullptr);
}

}  // namespace crostini
