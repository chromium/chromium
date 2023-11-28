// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_REPOSITORY_FAST_PAIR_DEVICE_ADDRESS_MAP_H_
#define ASH_QUICK_PAIR_REPOSITORY_FAST_PAIR_DEVICE_ADDRESS_MAP_H_

#include <optional>
#include <string>

#include "ash/quick_pair/common/device.h"
#include "base/containers/flat_map.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"

class PrefRegistrySimple;

namespace ash::quick_pair {

// Saves a mapping from mac address to model ID. Provides methods to persist
// or evict mac address -> model ID records from local state prefs. Also
// provides convenience methods for adding to the mapping given a device.
class DeviceAddressMap {
 public:
  // TODO(235117226): Migrate this pref to clear old entries/make sense with
  // renaming.
  static constexpr char kDeviceAddressMapPref[] = "fast_pair.device_id_map";

  // Registers preferences used by this class in the provided |registry|.
  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  DeviceAddressMap();
  DeviceAddressMap(const DeviceAddressMap&) = delete;
  DeviceAddressMap& operator=(const DeviceAddressMap&) = delete;
  ~DeviceAddressMap();

  // Saves mac address -> model ID records for the devices for both
  // the BLE and Classic address in memory, stored in mac_address_to_model_id.
  bool SaveModelIdForDevice(scoped_refptr<Device> device);

  // Persists the mac address -> model ID records for |device|
  // to local state prefs. Returns true if a record was persisted, false
  // otherwise.
  bool PersistRecordsForDevice(scoped_refptr<Device> device);

  // Evicts the |mac_address| -> model ID record in mac_address_to_model_id_
  // from local state prefs. Returns true if the record was evicted, false if
  // there was no |mac_address| record to evict.
  bool EvictMacAddressRecord(const std::string& mac_address);

  // Returns the model ID for |mac_address|, or std::nullopt if a matching
  // model ID isn't found.
  std::optional<const std::string> GetModelIdForMacAddress(
      const std::string& mac_address);

  // Returns true if there are mac address -> |model_id| records in
  // local state prefs, false otherwise.
  bool HasPersistedRecordsForModelId(const std::string& model_id);

 private:
  FRIEND_TEST_ALL_PREFIXES(FastPairRepositoryImplTest, PersistDeviceImages);
  FRIEND_TEST_ALL_PREFIXES(FastPairRepositoryImplTest,
                           PersistDeviceImagesNoMacAddress);
  FRIEND_TEST_ALL_PREFIXES(FastPairRepositoryImplTest, EvictDeviceImages);

  // Persists the |mac_address| -> model ID record in mac_address_to_model_id_
  // to local state prefs. Returns true if the record was persisted, false
  // if no record exists for |mac_address| or there was an error when
  // persisting.
  bool PersistMacAddressRecord(const std::string& mac_address);

  // Loads mac address -> model ID records persisted in prefs to
  // mac_address_to_model_id_.
  void LoadPersistedRecordsFromPrefs();

  // Clears the in-memory map and reloads from prefs. Used by tests.
  void RefreshCacheForTest();

  // Used to lazily load saved records from prefs.
  bool loaded_records_from_prefs_ = false;
  base::flat_map<std::string, std::string> mac_address_to_model_id_;
  base::WeakPtrFactory<DeviceAddressMap> weak_ptr_factory_{this};
};

}  // namespace ash::quick_pair

#endif  // ASH_QUICK_PAIR_REPOSITORY_FAST_PAIR_DEVICE_ADDRESS_MAP_H_
