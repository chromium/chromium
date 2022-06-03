
// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_REPOSITORY_FAST_PAIR_PENDING_WRITE_STORE_H_
#define ASH_QUICK_PAIR_REPOSITORY_FAST_PAIR_PENDING_WRITE_STORE_H_

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
class PendingWriteStore {
 public:
  PendingWriteStore();
  PendingWriteStore(const PendingWriteStore&) = delete;
  PendingWriteStore& operator=(const PendingWriteStore&) = delete;
  ~PendingWriteStore();

  // Registers preferences used by this class in the provided |registry|.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  struct PendingWrite {
    PendingWrite(const std::string& mac_address,
                 const std::string& hex_model_id);
    ~PendingWrite();

    const std::string mac_address;
    const std::string hex_model_id;
  };

  // Saves details about a pending request to add a new paired device to
  // Footprints.
  void AddPairedDevice(const std::string& mac_address,
                       const std::string& hex_model_id);

  // Gets a list of all devices which have been paired but not yet written to
  // the server.
  std::vector<PendingWrite> GetPendingAdds();

  // To be called after a paired device is saved to Footprints, this removes
  // pending device from storage.
  void OnPairedDeviceSaved(const std::string& mac_address);

  // Saves required details about a pending delete request to disk.
  void DeletePairedDevice(const std::string& hex_account_key);

  // Gets a list of the stable MAC address of devices which have been unpaired
  // but not yet removed from the server.
  std::vector<const std::string> GetPendingDeletes();

  // To be called after a paired device is successfully deleted from Footprints.
  void OnPairedDeviceDeleted(const std::string& hex_account_key);
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_REPOSITORY_FAST_PAIR_PENDING_WRITE_STORE_H_
