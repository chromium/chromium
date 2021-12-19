// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_USB_PERIPHERAL_USB_PERIPHERAL_NOTIFICATION_CONTROLLER_H_
#define ASH_SYSTEM_USB_PERIPHERAL_USB_PERIPHERAL_NOTIFICATION_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/components/peripheral_notification/peripheral_notification_manager.h"

namespace message_center {
class MessageCenter;
}  // namespace message_center

namespace ash {

class ASH_EXPORT UsbPeripheralNotificationController
    : public PeripheralNotificationManager::Observer {
 public:
  explicit UsbPeripheralNotificationController(
      message_center::MessageCenter* message_center);
  UsbPeripheralNotificationController(
      const UsbPeripheralNotificationController&) = delete;
  UsbPeripheralNotificationController& operator=(
      const UsbPeripheralNotificationController&) = delete;
  ~UsbPeripheralNotificationController() override;

  // Called after parent class is initialized.
  void OnPeripheralNotificationManagerInitialized();

  // PeripheralNotificationManager::Observer:
  void OnInvalidDpCableWarning() override;

  // Stubs from PCIE Notification controller
  void OnLimitedPerformancePeripheralReceived() override {}
  void OnGuestModeNotificationReceived(bool is_thunderbolt_only) override {}
  void OnPeripheralBlockedReceived() override {}
  void OnBillboardDeviceConnected() override {}

 private:
  message_center::MessageCenter* const message_center_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_USB_PERIPHERAL_USB_PERIPHERAL_NOTIFICATION_CONTROLLER_H_
