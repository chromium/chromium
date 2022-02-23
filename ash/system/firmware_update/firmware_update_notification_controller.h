// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_FIRMWARE_UPDATE_FIRMWARE_UPDATE_NOTIFICATION_CONTROLLER_H_
#define ASH_SYSTEM_FIRMWARE_UPDATE_FIRMWARE_UPDATE_NOTIFICATION_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/components/fwupd/firmware_update_manager.h"

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

  // Call when FirmwareUpdateManager is initialized so that this class can start
  // observering requests for notifications.
  void OnFirmwareUpdateManagerInitialized();

  // chromeos::FirmwareUpdateManager::Observer
  void OnFirmwareUpdateReceived() override;

  // Call to show a notification to indicate that a firmware update is
  // available.
  void NotifyFirmwareUpdateAvailable();

 private:
  friend class FirmwareUpdateNotificationControllerTest;

  // MessageCenter for adding notifications.
  message_center::MessageCenter* const message_center_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_FIRMWARE_UPDATE_FIRMWARE_UPDATE_NOTIFICATION_CONTROLLER_H_