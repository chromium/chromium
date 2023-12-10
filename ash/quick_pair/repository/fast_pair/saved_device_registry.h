// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_REPOSITORY_FAST_PAIR_SAVED_DEVICE_REGISTRY_H_
#define ASH_QUICK_PAIR_REPOSITORY_FAST_PAIR_SAVED_DEVICE_REGISTRY_H_

#include <optional>
#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/values.h"

class PrefRegistrySimple;
class PrefService;

namespace device {
class BluetoothAdapter;
}  // namespace device

namespace ash {
namespace quick_pair {

// Saves the Fast Pair account key to disk for peripherals that have been
// paired with this device for the active user, using the bluetooth MAC address
// as a lookup key, and user prefs for storage.
class SavedDeviceRegistry {
 public:
  explicit SavedDeviceRegistry(scoped_refptr<device::BluetoothAdapter> adapter);
  SavedDeviceRegistry(const SavedDeviceRegistry&) = delete;
  SavedDeviceRegistry& operator=(const SavedDeviceRegistry&) = delete;
  ~SavedDeviceRegistry();

  // Registers preferences used by this class in the provided |registry|.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Saves the account association (|mac_address|, |account_key|) to disk.
  // Returns true on success, false on failure.
  bool SaveAccountAssociation(const std::string& mac_address,
                              const std::vector<uint8_t>& account_key);

  // Deletes the |mac_address| -> account key record from prefs based on
  // |mac address|. Returns true on success, false on failure.
  bool DeleteAccountKey(const std::string& mac_address);

  // Deletes the mac address -> |account_key| record from prefs based on
  // |account_key|. Returns true on success, false on failure.
  bool DeleteAccountKey(const std::vector<uint8_t>& account_key);

  // Retrieves an account key from disk if available, otherwise returns an
  // empty vector.
  std::optional<const std::vector<uint8_t>> GetAccountKey(
      const std::string& mac_address);

  // Checks if the account key is in the registry.
  bool IsAccountKeySavedToRegistry(const std::vector<uint8_t>& account_key);

 private:
  // Cross check the list of devices in the registry with the devices paired
  // to the BluetoothAdapter to see if any devices need to be removed from the
  // registry if they are no longer paired to the adapter. This can happen if
  // a primary user pairs and saves a device to their account, logs out, then a
  // secondary user logs in, forgets the device from the Bluetooth pairing
  // menu. When the primary user logs back in, we need to reflect the device is
  // no longer paired locally in the registry.
  void RemoveDevicesIfRemovedFromDifferentUser(PrefService* pref_service);

  // Flags cross checking the registry with the devices paired to the adapter
  // to see if any devices need to be removed from the registry (see
  // |RemoveDevicesIfRemovedFromDifferentUser|). This only needs to happen once
  // per session, which is why it is flagged. Everything else following during
  // the user session will be immediately reflected here.
  bool has_updated_saved_devices_registry_ = false;

  scoped_refptr<device::BluetoothAdapter> adapter_;
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_REPOSITORY_FAST_PAIR_SAVED_DEVICE_REGISTRY_H_
