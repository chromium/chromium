// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/smart_lock/smart_lock_notification_controller.h"

#include "ash/constants/notifier_catalogs.h"
#include "ash/webui/settings/public/constants/routes.mojom.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "chromeos/ash/components/proximity_auth/screenlock_bridge.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/message_center/public/cpp/notification_types.h"

namespace ash {

namespace {

// These notification IDs refer to Smart Lock with the deprecated "Easy Unlock"
// name.
const char kEasyUnlockChromebookAddedNotifierId[] =
    "easyunlock_notification_ids.chromebook_added";

const char kEasyUnlockPairingChangeNotifierId[] =
    "easyunlock_notification_ids.pairing_change";

const char kEasyUnlockPairingChangeAppliedNotifierId[] =
    "easyunlock_notification_ids.pairing_change_applied";

// Convenience function for creating a Notification.
std::unique_ptr<message_center::Notification> CreateNotification(
    const std::string& id,
    const NotificationCatalogName& catalog_name,
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
                                 id, catalog_name),
      rich_notification_data, delegate);
}

}  // namespace

SmartLockNotificationController::SmartLockNotificationController(
    Profile* profile)
    : profile_(profile) {}

SmartLockNotificationController::~SmartLockNotificationController() {}

void SmartLockNotificationController::ShowChromebookAddedNotification() {
  message_center::RichNotificationData rich_notification_data;
  rich_notification_data.buttons.push_back(
      message_center::ButtonInfo(l10n_util::GetStringUTF16(
          IDS_EASY_UNLOCK_CHROMEBOOK_ADDED_NOTIFICATION_ABOUT_BUTTON)));

  ShowNotification(CreateNotification(
      kEasyUnlockChromebookAddedNotifierId,
      NotificationCatalogName::kEasyUnlockChromebookAdded,
      l10n_util::GetStringUTF16(
          IDS_EASY_UNLOCK_CHROMEBOOK_ADDED_NOTIFICATION_TITLE),
      l10n_util::GetStringFUTF16(
          IDS_EASY_UNLOCK_CHROMEBOOK_ADDED_NOTIFICATION_MESSAGE,
          ui::GetChromeOSDeviceName()),
      ui::ImageModel::FromImage(
          ui::ResourceBundle::GetSharedInstance().GetImageNamed(
              IDR_NOTIFICATION_EASYUNLOCK_ENABLED)),
      rich_notification_data,
      new NotificationDelegate(kEasyUnlockChromebookAddedNotifierId,
                               weak_ptr_factory_.GetWeakPtr())));
}

void SmartLockNotificationController::ShowPairingChangeNotification() {
  message_center::RichNotificationData rich_notification_data;
  rich_notification_data.buttons.push_back(
      message_center::ButtonInfo(l10n_util::GetStringUTF16(
          IDS_EASY_UNLOCK_PAIRING_CHANGED_NOTIFICATION_UPDATE_BUTTON)));
  rich_notification_data.buttons.push_back(
      message_center::ButtonInfo(l10n_util::GetStringUTF16(
          IDS_EASY_UNLOCK_CHROMEBOOK_ADDED_NOTIFICATION_ABOUT_BUTTON)));

  ShowNotification(CreateNotification(
      kEasyUnlockPairingChangeNotifierId,
      NotificationCatalogName::kEasyUnlockPairingChange,
      l10n_util::GetStringUTF16(
          IDS_EASY_UNLOCK_PAIRING_CHANGED_NOTIFICATION_TITLE),
      l10n_util::GetStringFUTF16(
          IDS_EASY_UNLOCK_PAIRING_CHANGED_NOTIFICATION_MESSAGE,
          ui::GetChromeOSDeviceName()),
      ui::ImageModel::FromImage(
          ui::ResourceBundle::GetSharedInstance().GetImageNamed(
              IDR_NOTIFICATION_EASYUNLOCK_ENABLED)),
      rich_notification_data,
      new NotificationDelegate(kEasyUnlockPairingChangeNotifierId,
                               weak_ptr_factory_.GetWeakPtr())));
}

void SmartLockNotificationController::ShowPairingChangeAppliedNotification(
    const std::string& phone_name) {
  // Remove the pairing change notification if it is still being shown.
  NotificationDisplayServiceFactory::GetForProfile(profile_)->Close(
      NotificationHandler::Type::TRANSIENT, kEasyUnlockPairingChangeNotifierId);

  message_center::RichNotificationData rich_notification_data;
  rich_notification_data.buttons.push_back(
      message_center::ButtonInfo(l10n_util::GetStringUTF16(
          IDS_EASY_UNLOCK_CHROMEBOOK_ADDED_NOTIFICATION_ABOUT_BUTTON)));

  ShowNotification(CreateNotification(
      kEasyUnlockPairingChangeAppliedNotifierId,
      NotificationCatalogName::kEasyUnlockPairingChangeApplied,
      l10n_util::GetStringUTF16(
          IDS_EASY_UNLOCK_PAIRING_CHANGE_APPLIED_NOTIFICATION_TITLE),
      l10n_util::GetStringFUTF16(
          IDS_EASY_UNLOCK_PAIRING_CHANGE_APPLIED_NOTIFICATION_MESSAGE,
          base::UTF8ToUTF16(phone_name), ui::GetChromeOSDeviceName()),
      ui::ImageModel::FromImage(
          ui::ResourceBundle::GetSharedInstance().GetImageNamed(
              IDR_NOTIFICATION_EASYUNLOCK_ENABLED)),
      rich_notification_data,
      new NotificationDelegate(kEasyUnlockPairingChangeAppliedNotifierId,
                               weak_ptr_factory_.GetWeakPtr())));
}

void SmartLockNotificationController::ShowNotification(
    std::unique_ptr<message_center::Notification> notification) {
  notification->SetSystemPriority();
  NotificationDisplayServiceFactory::GetForProfile(profile_)->Display(
      NotificationHandler::Type::TRANSIENT, *notification,
      /*metadata=*/nullptr);
}

void SmartLockNotificationController::LaunchMultiDeviceSettings() {
  chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
      profile_, chromeos::settings::mojom::kMultiDeviceFeaturesSubpagePath);
}

void SmartLockNotificationController::LockScreen() {
  proximity_auth::ScreenlockBridge::Get()->Lock();
}

SmartLockNotificationController::NotificationDelegate::NotificationDelegate(
    const std::string& notification_id,
    const base::WeakPtr<SmartLockNotificationController>&
        notification_controller)
    : notification_id_(notification_id),
      notification_controller_(notification_controller) {}

SmartLockNotificationController::NotificationDelegate::~NotificationDelegate() {
}

void SmartLockNotificationController::NotificationDelegate::Click(
    const std::optional<int>& button_index,
    const std::optional<std::u16string>& reply) {
  if (!notification_controller_) {
    return;
  }

  if (notification_id_ == kEasyUnlockPairingChangeNotifierId) {
    if (!button_index) {
      return;
    }

    if (*button_index == 0) {
      notification_controller_->LockScreen();
      return;
    }

    DCHECK_EQ(1, *button_index);
  }

  notification_controller_->LaunchMultiDeviceSettings();
}

}  // namespace ash
