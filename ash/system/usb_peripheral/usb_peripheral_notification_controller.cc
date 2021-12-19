// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/usb_peripheral/usb_peripheral_notification_controller.h"

#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "third_party/cros_system_api/dbus/typecd/dbus-constants.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "url/gurl.h"

namespace ash {

namespace {
const char kNotifierUsbPeripheral[] = "ash.usb_peripheral";
const char kUsbPeripheralInvalidDpCableNotificationId[] =
    "cros_usb_peripheral_invalid_dp_cable_notification_id";
const char kNotificationLandingPageUrl[] =
    "https://support.google.com/chromebook?p=cable_notification";

bool ShouldDisplayNotification() {
  return Shell::Get()->session_controller()->GetSessionState() ==
             session_manager::SessionState::ACTIVE &&
         !Shell::Get()->session_controller()->IsUserSessionBlocked();
}

void OnInvalidDpCableNotificationClicked() {
  NewWindowDelegate::GetInstance()->OpenUrl(GURL(kNotificationLandingPageUrl),
                                            /*from_user_interaction=*/true);
  message_center::MessageCenter::Get()->RemoveNotification(
      kUsbPeripheralInvalidDpCableNotificationId, /*from_user=*/true);
}

}  // namespace

UsbPeripheralNotificationController::UsbPeripheralNotificationController(
    message_center::MessageCenter* message_center)
    : message_center_(message_center) {
  DCHECK(message_center_);
}

UsbPeripheralNotificationController::~UsbPeripheralNotificationController() {
  if (ash::PeripheralNotificationManager::IsInitialized())
    ash::PeripheralNotificationManager::Get()->RemoveObserver(this);
}

void UsbPeripheralNotificationController::
    OnPeripheralNotificationManagerInitialized() {
  DCHECK(ash::PeripheralNotificationManager::IsInitialized());
  ash::PeripheralNotificationManager::Get()->AddObserver(this);
}

// Notify the user that the current cable may not support dp alt mode.
void UsbPeripheralNotificationController::OnInvalidDpCableWarning() {
  if (!ShouldDisplayNotification())
    return;

  std::unique_ptr<message_center::Notification> notification =
      CreateSystemNotification(
          message_center::NOTIFICATION_TYPE_SIMPLE,
          kUsbPeripheralInvalidDpCableNotificationId, u"USB Type-C Warning.",
          u"This cable may not support dp alternate mode.",
          /*display_source=*/std::u16string(), GURL(),
          message_center::NotifierId(
              message_center::NotifierType::SYSTEM_COMPONENT,
              kNotifierUsbPeripheral),
          message_center::RichNotificationData(),
          base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
              base::BindRepeating(&OnInvalidDpCableNotificationClicked)),
          kSettingsIcon,
          message_center::SystemNotificationWarningLevel::WARNING);

  message_center_->AddNotification(std::move(notification));
}

}  // namespace ash
