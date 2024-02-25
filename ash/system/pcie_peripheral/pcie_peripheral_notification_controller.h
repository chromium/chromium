// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PCIE_PERIPHERAL_PCIE_PERIPHERAL_NOTIFICATION_CONTROLLER_H_
#define ASH_SYSTEM_PCIE_PERIPHERAL_PCIE_PERIPHERAL_NOTIFICATION_CONTROLLER_H_

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/peripheral_notification/peripheral_notification_manager.h"

class PrefRegistrySimple;

namespace message_center {
class MessageCenter;
}  // namespace message_center

namespace ash {

// Manages showing notifications for Pciguard and TypeC daemon events.
// We display a CRITICAL notification if a guest user is attempting to use a
// Thunderbolt-only peripheral, which we prevent due to security risks with
// direct memory accessing. Other WARNING notifications are used to inform users
// that their peripherals may not be working due to data access protection
// enabled in OS Settings.
class ASH_EXPORT PciePeripheralNotificationController
    : public PeripheralNotificationManager::Observer {
 public:
  explicit PciePeripheralNotificationController(
      message_center::MessageCenter* message_center);
  PciePeripheralNotificationController(
      const PciePeripheralNotificationController&) = delete;
  PciePeripheralNotificationController& operator=(
      const PciePeripheralNotificationController&) = delete;
  ~PciePeripheralNotificationController() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Call when PeripheralNotificationManager is initialized so that this
  // class can start observering requests for notifications.
  void OnPeripheralNotificationManagerInitialized();

  // chromeos::PciePeripheral::Observer
  void OnLimitedPerformancePeripheralReceived() override;
  void OnGuestModeNotificationReceived(bool is_thunderbolt_only) override;
  void OnPeripheralBlockedReceived() override;
  void OnBillboardDeviceConnected() override;

  // Call to show a notification to indicate that the recently plugged in
  // Thunderbolt/USB4 peripheral performance is limited.
  void NotifyLimitedPerformance();

  // Call to show a notification to indicate to the Guest user of the current
  // state of their Thunderbolt/USB4 peripheral.
  void NotifyGuestModeNotification(bool is_thunderbolt_only);

  // Call to show a notification to indicate to the user that their
  // Thunderbolt/USB4 peripheral is not allowed due to security reasons.
  void NotifyPeripheralBlockedNotification();

  // Call to show a notification that a billboard device that was connected
  // is not supported by the board.
  void NotifyBillboardDevice();

  // Stubs from usb peripheral notification controller
  void OnInvalidDpCableWarning() override {}
  void OnInvalidUSB4ValidTBTCableWarning() override {}
  void OnInvalidUSB4CableWarning() override {}
  void OnInvalidTBTCableWarning() override {}
  void OnSpeedLimitingCableWarning() override {}

 private:
  friend class PciePeripheralNotificationControllerTest;

  // MessageCenter for adding notifications.
  const raw_ptr<message_center::MessageCenter> message_center_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PCIE_PERIPHERAL_PCIE_PERIPHERAL_NOTIFICATION_CONTROLLER_H_
