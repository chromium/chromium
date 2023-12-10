// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_REPOSITORY_FAST_PAIR_PENDING_WRITE_STORE_H_
#define ASH_QUICK_PAIR_REPOSITORY_FAST_PAIR_PENDING_WRITE_STORE_H_

#include <optional>
#include <string>
#include <vector>

#include "ash/quick_pair/proto/fastpair_data.pb.h"
#include "base/values.h"

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
                 const nearby::fastpair::FastPairInfo fast_pair_info);
    PendingWrite(PendingWrite&& pending_write);
    ~PendingWrite();

    const std::string mac_address;
    const nearby::fastpair::FastPairInfo fast_pair_info;
  };

  struct PendingDelete {
    PendingDelete(const std::string& mac_address,
                  const std::string& hex_account_key);
    ~PendingDelete();

    const std::string mac_address;
    const std::string hex_account_key;
  };

  // Saves details about a pending request to add a new paired device to
  // Footprints.
  void WritePairedDevice(const std::string& mac_address,
                         const nearby::fastpair::FastPairInfo fast_pair_info);

  // Gets a list of all devices which have been paired but not yet written to
  // the server.
  std::vector<PendingWrite> GetPendingWrites();

  // To be called after a paired device is saved to Footprints, this removes
  // pending device from storage.
  void OnPairedDeviceSaved(const std::string& mac_address);

  // Saves required details about a pending delete request to disk.
  void DeletePairedDevice(const std::string& mac_address,
                          const std::string& hex_account_key);

  // Gets a list of all devices which have been unpaired but not yet removed
  // from the server.
  std::vector<PendingWriteStore::PendingDelete> GetPendingDeletes();

  // To be called after a paired device is successfully deleted from Footprints,
  // this overloaded method removes the pending delete from storage by
  // |mac_address|.
  void OnPairedDeviceDeleted(const std::string& mac_address);

  // To be called after a paired device is successfully deleted from Footprints,
  // this overloaded method removes the pending delete from storage by
  // |account_key|.
  void OnPairedDeviceDeleted(const std::vector<uint8_t>& account_key);
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_REPOSITORY_FAST_PAIR_PENDING_WRITE_STORE_H_
