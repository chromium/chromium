// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_FIRMWARE_UPDATE_FIRMWARE_UPDATE_NOTIFICATION_CONTROLLER_H_
#define ASH_SYSTEM_FIRMWARE_UPDATE_FIRMWARE_UPDATE_NOTIFICATION_CONTROLLER_H_

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/fwupd/firmware_update_manager.h"

namespace message_center {
class MessageCenter;
}  // namespace message_center

namespace ash {

// Manages showing notifications for fwupd daemon events.
// We display a WARNING notification on startup if a critical firmware update
// is available.
class ASH_EXPORT FirmwareUpdateNotificationController
    : public FirmwareUpdateManager::Observer {
 public:
  explicit FirmwareUpdateNotificationController(
      message_center::MessageCenter* message_center);
  FirmwareUpdateNotificationController(
      const FirmwareUpdateNotificationController&) = delete;
  FirmwareUpdateNotificationController& operator=(
      const FirmwareUpdateNotificationController&) = delete;
  ~FirmwareUpdateNotificationController() override;

  // chromeos::FirmwareUpdateManager::Observer
  void OnFirmwareUpdateReceived() override;

  // Call to show a notification to indicate that a firmware update is
  // available.
  void NotifyFirmwareUpdateAvailable();

  void set_should_show_notification_for_test(bool show_notification) {
    should_show_notification_for_test_ = show_notification;
  }

 private:
  friend class FirmwareUpdateNotificationControllerTest;

  bool should_show_notification_for_test_ = false;

  // MessageCenter for adding notifications.
  const raw_ptr<message_center::MessageCenter, DanglingUntriaged>
      message_center_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_FIRMWARE_UPDATE_FIRMWARE_UPDATE_NOTIFICATION_CONTROLLER_H_
