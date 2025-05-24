// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/firmware_update/firmware_update_notification_controller.h"

#include <optional>

#include "ash/constants/ash_features.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/model/system_tray_model.h"
#include "base/strings/utf_string_conversions.h"
#include "components/user_manager/user_type.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "url/gurl.h"

namespace ash {

namespace {
const char kNotifierFirmwareUpdate[] = "ash.firmware_update";
const char kFirmwareUpdateNotificationId[] =
    "cros_firmware_update_notification_id";

// Represents the buttons in the notification.
enum ButtonIndex { kUpdate };

void ShowFirmwareUpdate() {
  Shell::Get()->system_tray_model()->client()->ShowFirmwareUpdate();
}

void RemoveNotification(const std::string& notification_id) {
  message_center::MessageCenter::Get()->RemoveNotification(notification_id,
                                                           /*from_user=*/true);
}

void OnFirmwareUpdateAvailableNotificationClicked(
    std::optional<int> button_index) {
  // Clicked on body.
  if (!button_index) {
    ShowFirmwareUpdate();
    RemoveNotification(kFirmwareUpdateNotificationId);
    return;
  }

  // TODO(michaelcheco): Add "Remind me later" button.
  switch (*button_index) {
    case ButtonIndex::kUpdate:
      ShowFirmwareUpdate();
      break;
  }
  RemoveNotification(kFirmwareUpdateNotificationId);
}

bool ShouldShowNotification() {
  const std::optional<user_manager::UserType> user_type =
      Shell::Get()->session_controller()->GetUserType();
  if (!user_type) {
    return false;
  }

  switch (*user_type) {
    case user_manager::UserType::kPublicAccount:
    case user_manager::UserType::kGuest:
    case user_manager::UserType::kKioskApp:
    case user_manager::UserType::kWebKioskApp:
    case user_manager::UserType::kKioskIWA:
      return false;
    case user_manager::UserType::kRegular:
    case user_manager::UserType::kChild:
      return true;
  }
}

}  // namespace

FirmwareUpdateNotificationController::FirmwareUpdateNotificationController(
    message_center::MessageCenter* message_center)
    : message_center_(message_center) {
  DCHECK(message_center_);
  if (ash::FirmwareUpdateManager::IsInitialized()) {
    ash::FirmwareUpdateManager::Get()->AddObserver(this);
  }
}

FirmwareUpdateNotificationController::~FirmwareUpdateNotificationController() {
  if (ash::FirmwareUpdateManager::IsInitialized()) {
    ash::FirmwareUpdateManager::Get()->RemoveObserver(this);
  }
}

void FirmwareUpdateNotificationController::NotifyFirmwareUpdateAvailable() {
  message_center::RichNotificationData optional;
  optional.buttons.push_back(
      message_center::ButtonInfo(l10n_util::GetStringUTF16(
          IDS_ASH_FIRMWARE_UPDATE_NOTIFICATION_UPDATE_BUTTON_TEXT)));
  std::unique_ptr<message_center::Notification> notification =
      CreateSystemNotificationPtr(
          message_center::NOTIFICATION_TYPE_SIMPLE,
          kFirmwareUpdateNotificationId,
          l10n_util::GetStringUTF16(
              IDS_ASH_FIRMWARE_UPDATE_NOTIFICATION_UPDATE_AVAILABLE_TITLE),
          l10n_util::GetStringUTF16(
              IDS_ASH_FIRMWARE_UPDATE_NOTIFICATION_UPDATE_AVAILABLE_BODY),
          /*display_source=*/std::u16string(), GURL(),
          message_center::NotifierId(
              message_center::NotifierType::SYSTEM_COMPONENT,
              kNotifierFirmwareUpdate,
              NotificationCatalogName::kFirmwareUpdate),
          optional,
          base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
              base::BindRepeating(
                  &OnFirmwareUpdateAvailableNotificationClicked)),
          kSettingsIcon,
          message_center::SystemNotificationWarningLevel::NORMAL);

  message_center_->AddNotification(std::move(notification));
}

void FirmwareUpdateNotificationController::OnFirmwareUpdateReceived() {
  if (should_show_notification_for_test_ || ShouldShowNotification()) {
    NotifyFirmwareUpdateAvailable();
  }
}

}  // namespace ash
