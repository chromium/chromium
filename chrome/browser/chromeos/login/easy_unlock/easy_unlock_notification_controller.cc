// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_notification_controller.h"

#include "base/guid.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "chromeos/components/proximity_auth/screenlock_bridge.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/message_center/public/cpp/notification_types.h"

namespace chromeos {

namespace {

const char kEasyUnlockChromebookAddedNotifierId[] =
    "easyunlock_notification_ids.chromebook_added";

const char kEasyUnlockPairingChangeNotifierId[] =
    "easyunlock_notification_ids.pairing_change";

const char kEasyUnlockPairingChangeAppliedNotifierId[] =
    "easyunlock_notification_ids.pairing_change_applied";

// Convenience function for creating a Notification.
std::unique_ptr<message_center::Notification> CreateNotification(
    const std::string& id,
    const base::string16& title,
    const base::string16& message,
    const gfx::Image& icon,
    const message_center::RichNotificationData& rich_notification_data,
    message_center::NotificationDelegate* delegate) {
  return std::make_unique<message_center::Notification>(
      message_center::NotificationType::NOTIFICATION_TYPE_SIMPLE, id, title,
      message, icon, base::string16() /* display_source */,
      GURL() /* origin_url */,
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 id),
      rich_notification_data, delegate);
}

}  // namespace

EasyUnlockNotificationController::EasyUnlockNotificationController(
    Profile* profile)
    : profile_(profile) {}

EasyUnlockNotificationController::~EasyUnlockNotificationController() {}

void EasyUnlockNotificationController::ShowChromebookAddedNotification() {
  message_center::RichNotificationData rich_notification_data;
  rich_notification_data.buttons.push_back(
      message_center::ButtonInfo(l10n_util::GetStringUTF16(
          IDS_EASY_UNLOCK_CHROMEBOOK_ADDED_NOTIFICATION_ABOUT_BUTTON)));

  ShowNotification(CreateNotification(
      kEasyUnlockChromebookAddedNotifierId,
      l10n_util::GetStringUTF16(
          IDS_EASY_UNLOCK_CHROMEBOOK_ADDED_NOTIFICATION_TITLE),
      l10n_util::GetStringFUTF16(
          IDS_EASY_UNLOCK_CHROMEBOOK_ADDED_NOTIFICATION_MESSAGE,
          ui::GetChromeOSDeviceName()),
      ui::ResourceBundle::GetSharedInstance().GetImageNamed(
          IDR_NOTIFICATION_EASYUNLOCK_ENABLED),
      rich_notification_data,
      new NotificationDelegate(kEasyUnlockChromebookAddedNotifierId,
                               weak_ptr_factory_.GetWeakPtr())));
}

void EasyUnlockNotificationController::ShowPairingChangeNotification() {
  message_center::RichNotificationData rich_notification_data;
  rich_notification_data.buttons.push_back(
      message_center::ButtonInfo(l10n_util::GetStringUTF16(
          IDS_EASY_UNLOCK_PAIRING_CHANGED_NOTIFICATION_UPDATE_BUTTON)));
  rich_notification_data.buttons.push_back(
      message_center::ButtonInfo(l10n_util::GetStringUTF16(
          IDS_EASY_UNLOCK_CHROMEBOOK_ADDED_NOTIFICATION_ABOUT_BUTTON)));

  ShowNotification(CreateNotification(
      kEasyUnlockPairingChangeNotifierId,
      l10n_util::GetStringUTF16(
          IDS_EASY_UNLOCK_PAIRING_CHANGED_NOTIFICATION_TITLE),
      l10n_util::GetStringFUTF16(
          IDS_EASY_UNLOCK_PAIRING_CHANGED_NOTIFICATION_MESSAGE,
          ui::GetChromeOSDeviceName()),
      ui::ResourceBundle::GetSharedInstance().GetImageNamed(
          IDR_NOTIFICATION_EASYUNLOCK_ENABLED),
      rich_notification_data,
      new NotificationDelegate(kEasyUnlockPairingChangeNotifierId,
                               weak_ptr_factory_.GetWeakPtr())));
}

void EasyUnlockNotificationController::ShowPairingChangeAppliedNotification(
    const std::string& phone_name) {
  // Remove the pairing change notification if it is still being shown.
  NotificationDisplayService::GetForProfile(profile_)->Close(
      NotificationHandler::Type::TRANSIENT, kEasyUnlockPairingChangeNotifierId);

  message_center::RichNotificationData rich_notification_data;
  rich_notification_data.buttons.push_back(
      message_center::ButtonInfo(l10n_util::GetStringUTF16(
          IDS_EASY_UNLOCK_CHROMEBOOK_ADDED_NOTIFICATION_ABOUT_BUTTON)));

  ShowNotification(CreateNotification(
      kEasyUnlockPairingChangeAppliedNotifierId,
      l10n_util::GetStringUTF16(
          IDS_EASY_UNLOCK_PAIRING_CHANGE_APPLIED_NOTIFICATION_TITLE),
      l10n_util::GetStringFUTF16(
          IDS_EASY_UNLOCK_PAIRING_CHANGE_APPLIED_NOTIFICATION_MESSAGE,
          base::UTF8ToUTF16(phone_name), ui::GetChromeOSDeviceName()),
      ui::ResourceBundle::GetSharedInstance().GetImageNamed(
          IDR_NOTIFICATION_EASYUNLOCK_ENABLED),
      rich_notification_data,
      new NotificationDelegate(kEasyUnlockPairingChangeAppliedNotifierId,
                               weak_ptr_factory_.GetWeakPtr())));
}

void EasyUnlockNotificationController::ShowNotification(
    std::unique_ptr<message_center::Notification> notification) {
  notification->SetSystemPriority();
  NotificationDisplayService::GetForProfile(profile_)->Display(
      NotificationHandler::Type::TRANSIENT, *notification,
      /*metadata=*/nullptr);
}

void EasyUnlockNotificationController::LaunchEasyUnlockSettings() {
  chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
      profile_, chrome::kSmartLockSettingsSubPage);
}

void EasyUnlockNotificationController::LockScreen() {
  proximity_auth::ScreenlockBridge::Get()->Lock();
}

EasyUnlockNotificationController::NotificationDelegate::NotificationDelegate(
    const std::string& notification_id,
    const base::WeakPtr<EasyUnlockNotificationController>&
        notification_controller)
    : notification_id_(notification_id),
      notification_controller_(notification_controller) {}

EasyUnlockNotificationController::NotificationDelegate::
    ~NotificationDelegate() {}

void EasyUnlockNotificationController::NotificationDelegate::Click(
    const base::Optional<int>& button_index,
    const base::Optional<base::string16>& reply) {
  if (!notification_controller_)
    return;

  if (notification_id_ == kEasyUnlockPairingChangeNotifierId) {
    if (!button_index)
      return;

    if (*button_index == 0) {
      notification_controller_->LockScreen();
      return;
    }

    DCHECK_EQ(1, *button_index);
  }

  notification_controller_->LaunchEasyUnlockSettings();
}

}  // namespace chromeos
