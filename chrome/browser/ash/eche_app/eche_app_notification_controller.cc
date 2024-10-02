// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/eche_app/eche_app_notification_controller.h"

#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/webui/eche_app_ui/eche_alert_generator.h"
#include "ash/webui/settings/public/constants/routes.mojom.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

namespace ash {
namespace eche_app {

namespace {

// Convenience function for creating a Notification.
std::unique_ptr<message_center::Notification> CreateNotification(
    const std::string& id,
    const NotificationCatalogName& catalog_name,
    const std::u16string& title,
    const std::u16string& message,
    const ui::ImageModel& icon,
    const message_center::RichNotificationData& rich_notification_data,
    scoped_refptr<message_center::NotificationDelegate> delegate) {
  return std::make_unique<message_center::Notification>(
      message_center::NotificationType::NOTIFICATION_TYPE_SIMPLE, id, title,
      message, icon, std::u16string() /* display_source */,
      GURL() /* origin_url */,
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 id, catalog_name),
      rich_notification_data, delegate);
}

}  // namespace

EcheAppNotificationController::EcheAppNotificationController(
    Profile* profile,
    const base::RepeatingCallback<void(Profile*)>& relaunch_callback)
    : profile_(profile), relaunch_callback_(relaunch_callback) {}

EcheAppNotificationController::~EcheAppNotificationController() = default;

void EcheAppNotificationController::LaunchSettings() {
  // TODO(crbug.com/40785967): Wait for UX confirm.
  chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
      profile_, chromeos::settings::mojom::kSecurityAndSignInSubpagePathV2);
}

void EcheAppNotificationController::LaunchNetworkSettings() {
  chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
      profile_, chromeos::settings::mojom::kNetworkSectionPath);
}

void EcheAppNotificationController::LaunchTryAgain() {
  relaunch_callback_.Run(profile_.get());
}

void EcheAppNotificationController::ShowNotificationFromWebUI(
    const std::optional<std::u16string>& title,
    const std::optional<std::u16string>& message,
    absl::variant<LaunchAppHelper::NotificationInfo::NotificationType,
                  mojom::WebNotificationType> type) {
  auto web_type = absl::get<mojom::WebNotificationType>(type);
  PA_LOG(INFO) << "ShowNotificationFromWebUI web_type: " << web_type;
  if (title && message) {
    if (web_type == mojom::WebNotificationType::CONNECTION_FAILED ||
        web_type == mojom::WebNotificationType::CONNECTION_LOST) {
      message_center::RichNotificationData rich_notification_data;
      rich_notification_data.buttons.push_back(
          message_center::ButtonInfo(l10n_util::GetStringUTF16(
              IDS_ECHE_APP_NOTIFICATION_TRY_AGAIN_BUTTON)));
      ShowNotification(CreateNotification(
          kEcheAppRetryConnectionNotifierId,
          NotificationCatalogName::kEcheAppRetryConnection, title.value(),
          message.value(), ui::ImageModel(), rich_notification_data,
          base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
              base::BindRepeating(
                  &EcheAppNotificationController::LaunchTryAgain,
                  weak_ptr_factory_.GetWeakPtr()))));
    } else if (web_type == mojom::WebNotificationType::DEVICE_IDLE) {
      message_center::RichNotificationData rich_notification_data;
      rich_notification_data.buttons.push_back(
          message_center::ButtonInfo(l10n_util::GetStringUTF16(
              IDS_ECHE_APP_NOTIFICATION_OPEN_AGAIN_BUTTON)));
      ShowNotification(CreateNotification(
          kEcheAppInactivityNotifierId,
          NotificationCatalogName::kEcheAppInactivity, title.value(),
          message.value(), ui::ImageModel(), rich_notification_data,
          base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
              base::BindRepeating(
                  &EcheAppNotificationController::LaunchTryAgain,
                  weak_ptr_factory_.GetWeakPtr()))));
    } else if (web_type == mojom::WebNotificationType::WIFI_NOT_READY) {
      message_center::RichNotificationData rich_notification_data;
      // Reuse the setting string for Eche's setting button.
      rich_notification_data.buttons.emplace_back(
          l10n_util::GetStringUTF16(IDS_ECHE_APP_SCREEN_LOCK_SETTINGS_BUTTON));
      ShowNotification(CreateNotification(
          kEcheAppNetworkSettingNotifierId,
          NotificationCatalogName::kEcheAppNetworkSetting, title.value(),
          message.value(), ui::ImageModel(), rich_notification_data,
          base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
              base::BindRepeating(
                  &EcheAppNotificationController::LaunchNetworkSettings,
                  weak_ptr_factory_.GetWeakPtr()))));
    } else {
      // No need to take the action.
      ShowNotification(CreateNotification(
          kEcheAppFromWebWithoutButtonNotifierId,
          NotificationCatalogName::kEcheAppFromWebWithoutButton, title.value(),
          message.value(), ui::ImageModel(),
          message_center::RichNotificationData(),
          base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
              message_center::HandleNotificationClickDelegate::
                  ButtonClickCallback(base::DoNothing()))));
    }
  } else {
    PA_LOG(ERROR)
        << "Cannot find the title or message to show the notification.";
  }
}

void EcheAppNotificationController::ShowScreenLockNotification(
    const std::u16string& title) {
  message_center::RichNotificationData rich_notification_data;
  rich_notification_data.buttons.push_back(message_center::ButtonInfo(
      l10n_util::GetStringUTF16(IDS_ECHE_APP_SCREEN_LOCK_SETTINGS_BUTTON)));
  ShowNotification(CreateNotification(
      kEcheAppScreenLockNotifierId, NotificationCatalogName::kEcheAppScreenLock,
      l10n_util::GetStringFUTF16(IDS_ECHE_APP_SCREEN_LOCK_NOTIFICATION_TITLE,
                                 title),
      l10n_util::GetStringUTF16(IDS_ECHE_APP_SCREEN_LOCK_NOTIFICATION_MESSAGE),
      ui::ImageModel(), rich_notification_data,
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::BindRepeating(&EcheAppNotificationController::LaunchSettings,
                              weak_ptr_factory_.GetWeakPtr()))));
}

void EcheAppNotificationController::ShowNotification(
    std::unique_ptr<message_center::Notification> notification) {
  notification->SetSystemPriority();
  NotificationDisplayServiceFactory::GetForProfile(profile_)->Display(
      NotificationHandler::Type::TRANSIENT, *notification,
      /*metadata=*/nullptr);
}

void EcheAppNotificationController::CloseNotification(
    const std::string& notification_id) {
  NotificationDisplayServiceFactory::GetForProfile(profile_)->Close(
      NotificationHandler::Type::TRANSIENT, notification_id);
}

void EcheAppNotificationController::
    CloseConnectionOrLaunchErrorNotifications() {
  NotificationDisplayServiceFactory::GetForProfile(profile_)->Close(
      NotificationHandler::Type::TRANSIENT, kEcheAppRetryConnectionNotifierId);
  NotificationDisplayServiceFactory::GetForProfile(profile_)->Close(
      NotificationHandler::Type::TRANSIENT, kEcheAppInactivityNotifierId);
  NotificationDisplayServiceFactory::GetForProfile(profile_)->Close(
      NotificationHandler::Type::TRANSIENT,
      kEcheAppFromWebWithoutButtonNotifierId);
  NotificationDisplayServiceFactory::GetForProfile(profile_)->Close(
      NotificationHandler::Type::TRANSIENT, kEcheAppNetworkSettingNotifierId);
}

}  // namespace eche_app
}  // namespace ash
