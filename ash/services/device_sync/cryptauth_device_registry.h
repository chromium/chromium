// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_DEVICE_REGISTRY_H_
#define ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_DEVICE_REGISTRY_H_

#include <ostream>
#include <string>

#include "ash/services/device_sync/cryptauth_device.h"
#include "base/containers/flat_map.h"
#include "base/values.h"

namespace ash {

namespace device_sync {

// Stores the devices managed by CryptAuth v2 DeviceSync.
class CryptAuthDeviceRegistry {
 public:
  using InstanceIdToDeviceMap = base::flat_map<std::string, CryptAuthDevice>;

  CryptAuthDeviceRegistry(const CryptAuthDeviceRegistry&) = delete;
  CryptAuthDeviceRegistry& operator=(const CryptAuthDeviceRegistry&) = delete;

  virtual ~CryptAuthDeviceRegistry();

  // Returns a map from Instance ID to CryptAuthDevice.
  const InstanceIdToDeviceMap& instance_id_to_device_map() const;

  // Returns the device with Instance ID |instance_id| if it exists in the
  // registry, and returns null if it cannot be found.
  const CryptAuthDevice* GetDevice(const std::string& instance_id) const;

  // Adds |device| to the registry. If a device with the same Instance ID
  // already exists in the registry, the existing device will be overwritten.
  // Returns true if the registry changes.
  bool AddDevice(const CryptAuthDevice& device);

  // Removes the device with corresponding |instance_id|.  Returns true if the
  // registry changes.
  bool DeleteDevice(const std::string& instance_id);

  // Replaces the entire registry with |instance_id_to_device_map|. Returns true
  // if the registry changes.
  bool SetRegistry(const InstanceIdToDeviceMap& instance_id_to_device_map);

  // Converts the registry to a human-readable dictionary.
  base::Value AsReadableDictionary() const;

 protected:
  CryptAuthDeviceRegistry();

  // Invoked when the device map changes.
  virtual void OnDeviceRegistryUpdated() = 0;

 private:
  InstanceIdToDeviceMap instance_id_to_device_map_;
};

std::ostream& operator<<(std::ostream& stream,
                         const CryptAuthDeviceRegistry& registry);

}  // namespace device_sync

}  // namespace ash

// TODO(https://crbug.com/1164001): remove when the migration is finished.
namespace chromeos::device_sync {
using ash::device_sync::CryptAuthDeviceRegistry;
}

#endif  // ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_DEVICE_REGISTRY_H_
