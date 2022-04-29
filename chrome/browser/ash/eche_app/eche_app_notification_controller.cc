// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/eche_app/eche_app_notification_controller.h"

#include "ash/components/multidevice/logging/logging.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/webui/settings/chromeos/constants/routes.mojom.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/message_center/message_center.h"

namespace ash {
namespace eche_app {

const char kEcheAppScreenLockNotifierId[] =
    "eche_app_notification_ids.screen_lock";

// The notification type from WebUI is CONNECTION_FAILED or CONNECTION_LOST
// allow users to retry.
const char kEcheAppRetryConnectionNotifierId[] =
    "eche_app_notification_ids.retry_connection";

// The notification type from WebUI is DEVICE_IDLE
// allow users to retry.
const char kEcheAppInactivityNotifierId[] =
    "eche_app_notification_ids.inactivity";

// The notification type from WebUI without any actions need to do.
const char kEcheAppFromWebWithoudButtonNotifierId[] =
    "eche_app_notification_ids.from_web_without_button";

// TODO(crbug.com/1241352): This should probably have a ?p=<FEATURE_NAME> at
// some point.
const char kEcheAppLearnMoreUrl[] = "https://support.google.com/chromebook";

// TODO(b/193583292): Wait for UX to build help site.
const char kEcheAppHelpUrl[] = "https://support.google.com/chromebook";

namespace {

// Convenience function for creating a Notification.
std::unique_ptr<message_center::Notification> CreateNotification(
    const std::string& id,
    const std::u16string& title,
    const std::u16string& message,
    const ui::ImageModel& icon,
    const message_center::RichNotificationData& rich_notification_data,
    message_center::NotificationDelegate* delegate) {
  return std::make_unique<message_center::Notification>(
      message_center::NotificationType::NOTIFICATION_TYPE_SIMPLE, id, title,
      message, icon, std::u16string() /* display_source */,
      GURL() /* origin_url */,
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 id),
      rich_notification_data, delegate);
}

}  // namespace

EcheAppNotificationController::EcheAppNotificationController(
    Profile* profile,
    const base::RepeatingCallback<void(Profile*)>& relaunch_callback)
    : profile_(profile), relaunch_callback_(relaunch_callback) {}

EcheAppNotificationController::~EcheAppNotificationController() {}
void EcheAppNotificationController::LaunchSettings() {
  // TODO(crbug.com/1241352): Wait for UX confirm.
  chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
      profile_, chromeos::settings::mojom::kSecurityAndSignInSubpagePathV2);
}

void EcheAppNotificationController::LaunchLearnMore() {
  // TODO(crbug.com/1241352): Wait for UX confirm.
  NewWindowDelegate::GetPrimary()->OpenUrl(
      GURL(kEcheAppLearnMoreUrl),
      NewWindowDelegate::OpenUrlFrom::kUserInteraction);
}

void EcheAppNotificationController::LaunchTryAgain() {
  relaunch_callback_.Run(profile_);
}

void EcheAppNotificationController::LaunchHelp() {
  NewWindowDelegate::GetPrimary()->OpenUrl(
      GURL(kEcheAppHelpUrl), NewWindowDelegate::OpenUrlFrom::kUserInteraction);
}

void EcheAppNotificationController::ShowNotificationFromWebUI(
    const absl::optional<std::u16string>& title,
    const absl::optional<std::u16string>& message,
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
      rich_notification_data.buttons.push_back(message_center::ButtonInfo(
          l10n_util::GetStringUTF16(IDS_ECHE_APP_NOTIFICATION_HELP_BUTTON)));
      ShowNotification(CreateNotification(
          kEcheAppRetryConnectionNotifierId, title.value(), message.value(),
          ui::ImageModel(), rich_notification_data,
          new NotificationDelegate(kEcheAppRetryConnectionNotifierId,
                                   weak_ptr_factory_.GetWeakPtr())));
    } else if (web_type == mojom::WebNotificationType::DEVICE_IDLE) {
      message_center::RichNotificationData rich_notification_data;
      rich_notification_data.buttons.push_back(
          message_center::ButtonInfo(l10n_util::GetStringUTF16(
              IDS_ECHE_APP_NOTIFICATION_OPEN_AGAIN_BUTTON)));
      ShowNotification(CreateNotification(
          kEcheAppInactivityNotifierId, title.value(), message.value(),
          ui::ImageModel(), rich_notification_data,
          new NotificationDelegate(kEcheAppInactivityNotifierId,
                                   weak_ptr_factory_.GetWeakPtr())));
    } else {
      // No need to take the action.
      ShowNotification(CreateNotification(
          kEcheAppFromWebWithoudButtonNotifierId, title.value(),
          message.value(), ui::ImageModel(),
          message_center::RichNotificationData(),
          new NotificationDelegate(kEcheAppFromWebWithoudButtonNotifierId,
                                   weak_ptr_factory_.GetWeakPtr())));
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
  rich_notification_data.buttons.push_back(message_center::ButtonInfo(
      l10n_util::GetStringUTF16(IDS_ECHE_APP_SCREEN_LOCK_LEARN_MORE)));
  ShowNotification(CreateNotification(
      kEcheAppScreenLockNotifierId,
      l10n_util::GetStringUTF16(IDS_ECHE_APP_SCREEN_LOCK_NOTIFICATION_TITLE),
      l10n_util::GetStringFUTF16(IDS_ECHE_APP_SCREEN_LOCK_NOTIFICATION_MESSAGE,
                                 title),
      ui::ImageModel(), rich_notification_data,
      new NotificationDelegate(kEcheAppScreenLockNotifierId,
                               weak_ptr_factory_.GetWeakPtr())));
}

void EcheAppNotificationController::ShowNotification(
    std::unique_ptr<message_center::Notification> notification) {
  notification->SetSystemPriority();
  NotificationDisplayService::GetForProfile(profile_)->Display(
      NotificationHandler::Type::TRANSIENT, *notification,
      /*metadata=*/nullptr);
}

void EcheAppNotificationController::CloseNotification(
    const std::string& notification_id) {
  NotificationDisplayService::GetForProfile(profile_)->Close(
      NotificationHandler::Type::TRANSIENT, notification_id);
}

void EcheAppNotificationController::
    CloseConnectionOrLaunchErrorNotifications() {
  NotificationDisplayService::GetForProfile(profile_)->Close(
      NotificationHandler::Type::TRANSIENT, kEcheAppRetryConnectionNotifierId);
  NotificationDisplayService::GetForProfile(profile_)->Close(
      NotificationHandler::Type::TRANSIENT, kEcheAppInactivityNotifierId);
  NotificationDisplayService::GetForProfile(profile_)->Close(
      NotificationHandler::Type::TRANSIENT,
      kEcheAppFromWebWithoudButtonNotifierId);
}

EcheAppNotificationController::NotificationDelegate::NotificationDelegate(
    const std::string& notification_id,
    const base::WeakPtr<EcheAppNotificationController>& notification_controller)
    : notification_id_(notification_id),
      notification_controller_(notification_controller) {}

EcheAppNotificationController::NotificationDelegate::~NotificationDelegate() {}
void EcheAppNotificationController::NotificationDelegate::Click(
    const absl::optional<int>& button_index,
    const absl::optional<std::u16string>& reply) {
  if (!button_index)
    return;

  if (notification_id_ == kEcheAppScreenLockNotifierId) {
    if (*button_index == 0) {
      notification_controller_->LaunchSettings();
    } else {
      DCHECK_EQ(1, *button_index);
      notification_controller_->LaunchLearnMore();
    }
  } else if (notification_id_ == kEcheAppRetryConnectionNotifierId) {
    if (*button_index == 0) {
      notification_controller_->LaunchTryAgain();
    } else {
      DCHECK_EQ(1, *button_index);
      notification_controller_->LaunchHelp();
    }
  } else if (notification_id_ == kEcheAppInactivityNotifierId) {
    if (*button_index == 0) {
      notification_controller_->LaunchTryAgain();
    }
  }
}

}  // namespace eche_app
}  // namespace ash
