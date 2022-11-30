// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_REPOSITORY_FAST_PAIR_DEVICE_ID_MAP_H_
#define ASH_QUICK_PAIR_REPOSITORY_FAST_PAIR_DEVICE_ID_MAP_H_

#include <string>

#include "ash/quick_pair/common/device.h"
#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace device {
class BluetoothAdapter;
}  // namespace device

class PrefRegistrySimple;

namespace ash {
namespace quick_pair {

// Saves a mapping from device ID to model ID. Provides methods to persist
// or evict device ID -> model ID records from local state prefs. Also
// provides convenience methods for adding to the mapping given a device.
class DeviceIdMap {
 public:
  static constexpr char kDeviceIdMapPref[] = "fast_pair.device_id_map";

  // Registers preferences used by this class in the provided |registry|.
  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  explicit DeviceIdMap(scoped_refptr<device::BluetoothAdapter> adapter);
  DeviceIdMap(const DeviceIdMap&) = delete;
  DeviceIdMap& operator=(const DeviceIdMap&) = delete;
  ~DeviceIdMap();

  // Saves device ID -> model ID records for the devices matching both
  // the BLE and Classic address in memory, stored in device_id_to_model_id.
  bool SaveModelIdForDevice(scoped_refptr<Device> device);

  // Persists the device ID -> model ID records for |device|
  // to local state prefs. Returns true if a record was persisted, false
  // otherwise.
  bool PersistRecordsForDevice(scoped_refptr<Device> device);

  // Evicts the |device_id| -> model ID record in device_id_to_model_id_ from
  // local state prefs. Returns true if the record was
  // evicted, false if there was no |device_id| record to evict.
  bool EvictDeviceIdRecord(const std::string& device_id);

  // Returns the model ID for |device_id|, or absl::nullopt if a matching
  // model ID isn't found.
  absl::optional<const std::string> GetModelIdForDeviceId(
      const std::string& device_id);

  // Returns true if there are device ID -> |model_id| records in
  // local state prefs, false otherwise.
  bool HasPersistedRecordsForModelId(const std::string& model_id);

  // Clears the in-memory map and reloads from prefs.
  void RefreshCacheForTest();

 private:
  // Returns the device ID that owns |address|, if found.
  absl::optional<const std::string> GetDeviceIdForAddress(
      const std::string& address);
  // Persists the |device_id| -> model ID record in device_id_to_model_id_
  // to local state prefs. Returns true if the record was persisted, false
  // if no record exists for |device_id| or there was an error when persisting.
  bool PersistDeviceIdRecord(const std::string& device_id);
  // Loads device ID -> model ID records persisted in prefs to
  // device_id_to_model_id_.
  void LoadPersistedRecordsFromPrefs();

  // Used to lazily load saved records from prefs.
  bool loaded_records_from_prefs_ = false;
  scoped_refptr<device::BluetoothAdapter> bluetooth_adapter_;
  base::flat_map<std::string, std::string> device_id_to_model_id_;
  base::WeakPtrFactory<DeviceIdMap> weak_ptr_factory_{this};
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_REPOSITORY_FAST_PAIR_DEVICE_ID_MAP_H_
