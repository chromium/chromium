// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SERVICES_DEVICE_SYNC_FAKE_REMOTE_DEVICE_PROVIDER_H_
#define ASH_SERVICES_DEVICE_SYNC_FAKE_REMOTE_DEVICE_PROVIDER_H_

#include "ash/services/device_sync/remote_device_provider.h"

namespace chromeos {

namespace device_sync {

// Test double for RemoteDeviceProvider.
class FakeRemoteDeviceProvider : public RemoteDeviceProvider {
 public:
  FakeRemoteDeviceProvider();

  FakeRemoteDeviceProvider(const FakeRemoteDeviceProvider&) = delete;
  FakeRemoteDeviceProvider& operator=(const FakeRemoteDeviceProvider&) = delete;

  ~FakeRemoteDeviceProvider() override;

  void set_synced_remote_devices(
      const multidevice::RemoteDeviceList& synced_remote_devices) {
    synced_remote_devices_ = synced_remote_devices;
  }

  void NotifyObserversDeviceListChanged();

  // RemoteDeviceProvider:
  const multidevice::RemoteDeviceList& GetSyncedDevices() const override;

 private:
  multidevice::RemoteDeviceList synced_remote_devices_;
};

}  // namespace device_sync

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove when this file is moved to ash.
namespace ash {
namespace device_sync {
using ::chromeos::device_sync::FakeRemoteDeviceProvider;
}  // namespace device_sync
}  // namespace ash

#endif  // ASH_SERVICES_DEVICE_SYNC_FAKE_REMOTE_DEVICE_PROVIDER_H_
