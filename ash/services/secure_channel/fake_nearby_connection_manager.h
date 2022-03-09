// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SERVICES_SECURE_CHANNEL_FAKE_NEARBY_CONNECTION_MANAGER_H_
#define ASH_SERVICES_SECURE_CHANNEL_FAKE_NEARBY_CONNECTION_MANAGER_H_

#include "ash/services/secure_channel/nearby_connection_manager.h"

namespace ash::secure_channel {

class FakeNearbyConnectionManager : public NearbyConnectionManager {
 public:
  FakeNearbyConnectionManager();
  FakeNearbyConnectionManager(const FakeNearbyConnectionManager&) = delete;
  FakeNearbyConnectionManager& operator=(const FakeNearbyConnectionManager&) =
      delete;
  ~FakeNearbyConnectionManager() override;

  using NearbyConnectionManager::DoesAttemptExist;
  using NearbyConnectionManager::GetDeviceIdPairsForRemoteDevice;
  using NearbyConnectionManager::NotifyNearbyInitiatorConnectionSuccess;
  using NearbyConnectionManager::NotifyNearbyInitiatorFailure;

 private:
  // NearbyConnectionManager:
  void PerformAttemptNearbyInitiatorConnection(
      const DeviceIdPair& device_id_pair) override;
  void PerformCancelNearbyInitiatorConnectionAttempt(
      const DeviceIdPair& device_id_pair) override;
};

}  // namespace ash::secure_channel

#endif  // ASH_SERVICES_SECURE_CHANNEL_FAKE_NEARBY_CONNECTION_MANAGER_H_
