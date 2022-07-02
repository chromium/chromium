
// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_REPOSITORY_FAST_PAIR_SAVED_DEVICE_REGISTRY_H_
#define ASH_QUICK_PAIR_REPOSITORY_FAST_PAIR_SAVED_DEVICE_REGISTRY_H_

#include <string>
#include <vector>

#include "base/values.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class PrefRegistrySimple;

namespace ash {
namespace quick_pair {

// Saves the Fast Pair account key to disk for peripherals that have been
// paired with this device for the active user, using the bluetooth MAC address
// as a lookup key, and user prefs for storage.
class SavedDeviceRegistry {
 public:
  SavedDeviceRegistry();
  SavedDeviceRegistry(const SavedDeviceRegistry&) = delete;
  SavedDeviceRegistry& operator=(const SavedDeviceRegistry&) = delete;
  ~SavedDeviceRegistry();

  // Registers preferences used by this class in the provided |registry|.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Saves an account key to disk.
  void SaveAccountKey(const std::string& mac_address,
                      const std::vector<uint8_t>& account_key);

  // Deletes the |mac_address| -> account key record from prefs. Returns true
  // on success, false on failure.
  bool DeleteAccountKey(const std::string& mac_address);

  // Retrieves an account key from disk if available, otherwise returns an
  // empty vector.
  absl::optional<const std::vector<uint8_t>> GetAccountKey(
      const std::string& mac_address);

  // Checks if the account key is in the registry.
  bool IsAccountKeySavedToRegistry(const std::vector<uint8_t>& account_key);
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_REPOSITORY_FAST_PAIR_SAVED_DEVICE_REGISTRY_H_
