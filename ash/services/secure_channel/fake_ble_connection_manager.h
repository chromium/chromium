// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SERVICES_SECURE_CHANNEL_FAKE_BLE_CONNECTION_MANAGER_H_
#define ASH_SERVICES_SECURE_CHANNEL_FAKE_BLE_CONNECTION_MANAGER_H_

#include "ash/services/secure_channel/ble_connection_manager.h"
#include "ash/services/secure_channel/connection_role.h"
#include "ash/services/secure_channel/device_id_pair.h"
#include "ash/services/secure_channel/public/cpp/shared/connection_priority.h"

namespace ash::secure_channel {

// Test BleConnectionManager implementation. Each of the overridden functions
// has an empty body.
class FakeBleConnectionManager : public BleConnectionManager {
 public:
  FakeBleConnectionManager();

  FakeBleConnectionManager(const FakeBleConnectionManager&) = delete;
  FakeBleConnectionManager& operator=(const FakeBleConnectionManager&) = delete;

  ~FakeBleConnectionManager() override;

  // Make public for testing.
  using BleConnectionManager::GetPriorityForAttempt;
  using BleConnectionManager::GetDetailsForRemoteDevice;
  using BleConnectionManager::DoesAttemptExist;
  using BleConnectionManager::NotifyBleInitiatorFailure;
  using BleConnectionManager::NotifyBleListenerFailure;
  using BleConnectionManager::NotifyConnectionSuccess;

 private:
  // BleConnectionManager:
  void PerformAttemptBleInitiatorConnection(
      const DeviceIdPair& device_id_pair,
      ConnectionPriority connection_priority) override;
  void PerformUpdateBleInitiatorConnectionPriority(
      const DeviceIdPair& device_id_pair,
      ConnectionPriority connection_priority) override;
  void PerformCancelBleInitiatorConnectionAttempt(
      const DeviceIdPair& device_id_pair) override;
  void PerformAttemptBleListenerConnection(
      const DeviceIdPair& device_id_pair,
      ConnectionPriority connection_priority) override;
  void PerformUpdateBleListenerConnectionPriority(
      const DeviceIdPair& device_id_pair,
      ConnectionPriority connection_priority) override;
  void PerformCancelBleListenerConnectionAttempt(
      const DeviceIdPair& device_id_pair) override;
};

}  // namespace ash::secure_channel

#endif  // ASH_SERVICES_SECURE_CHANNEL_FAKE_BLE_CONNECTION_MANAGER_H_
