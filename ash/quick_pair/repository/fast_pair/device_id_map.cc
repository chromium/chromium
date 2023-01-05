// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/repository/fast_pair/device_id_map.h"

#include "ash/quick_pair/common/logging.h"
#include "ash/shell.h"
#include "base/values.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_device.h"

namespace ash {
namespace quick_pair {

// static
constexpr char DeviceIdMap::kDeviceIdMapPref[];

// static
void DeviceIdMap::RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kDeviceIdMapPref);
}

DeviceIdMap::DeviceIdMap(scoped_refptr<device::BluetoothAdapter> adapter)
    : bluetooth_adapter_(adapter) {}

DeviceIdMap::~DeviceIdMap() = default;

bool DeviceIdMap::SaveModelIdForDevice(scoped_refptr<Device> device) {
  // In some cases, BLE and classic address can map to different devices (with
  // the same model ID) so we want to capture both device ID -> model ID
  // records.
  bool did_save = false;
  absl::optional<const std::string> ble_device_id =
      GetDeviceIdForAddress(device->ble_address());
  if (ble_device_id) {
    did_save = true;
    device_id_to_model_id_[ble_device_id.value()] = device->metadata_id();
  }

  absl::optional<const std::string> classic_address = device->classic_address();
  if (!classic_address) {
    return did_save;
  }

  absl::optional<const std::string> classic_device_id =
      GetDeviceIdForAddress(classic_address.value());
  if (classic_device_id) {
    did_save = true;
    device_id_to_model_id_[classic_device_id.value()] = device->metadata_id();
  }
  return did_save;
}

bool DeviceIdMap::PersistRecordsForDevice(scoped_refptr<Device> device) {
  // In some cases, BLE and classic address can map to different devices (with
  // the same model ID) so we want to capture both device ID -> model ID
  // records.
  bool did_persist = false;
  absl::optional<const std::string> ble_device_id =
      GetDeviceIdForAddress(device->ble_address());
  if (ble_device_id) {
    did_persist = PersistDeviceIdRecord(ble_device_id.value());
  }

  absl::optional<const std::string> classic_address = device->classic_address();
  if (!classic_address) {
    return did_persist;
  }

  absl::optional<const std::string> classic_device_id =
      GetDeviceIdForAddress(classic_address.value());
  if (classic_device_id) {
    did_persist |= PersistDeviceIdRecord(classic_device_id.value());
  }
  return did_persist;
}

bool DeviceIdMap::PersistDeviceIdRecord(const std::string& device_id) {
  const std::string& model_id = device_id_to_model_id_[device_id];

  if (model_id.empty()) {
    QP_LOG(WARNING)
        << __func__
        << ": Can't persist null device ID -> model ID record for device ID: " +
               device_id;
    return false;
  }

  PrefService* local_state = Shell::Get()->local_state();
  if (!local_state) {
    QP_LOG(WARNING) << __func__ << ": No shell local state available.";
    return false;
  }

  ScopedDictPrefUpdate device_id_map_dict(local_state, kDeviceIdMapPref);
  if (!device_id_map_dict->Set(device_id, model_id)) {
    QP_LOG(WARNING)
        << __func__
        << ": Failed to persist device ID -> model ID record for device ID: " +
               device_id;
    return false;
  }
  return true;
}

bool DeviceIdMap::EvictDeviceIdRecord(const std::string& device_id) {
  PrefService* local_state = Shell::Get()->local_state();
  if (!local_state) {
    QP_LOG(WARNING) << __func__ << ": No shell local state available.";
    return false;
  }

  ScopedDictPrefUpdate device_id_map_dict(local_state, kDeviceIdMapPref);
  if (!device_id_map_dict->Remove(device_id)) {
    QP_LOG(WARNING) << __func__
                    << ": Failed to evict device ID -> model ID record from "
                       "prefs for device ID: " +
                           device_id;
    return false;
  }
  return true;
}

absl::optional<const std::string> DeviceIdMap::GetModelIdForDeviceId(
    const std::string& device_id) {
  // Lazily load saved records from prefs the first time we get an ID.
  if (!loaded_records_from_prefs_) {
    loaded_records_from_prefs_ = true;
    LoadPersistedRecordsFromPrefs();
  }

  std::string& saved_model_id = device_id_to_model_id_[device_id];
  if (saved_model_id.empty()) {
    return absl::nullopt;
  }
  return saved_model_id;
}

bool DeviceIdMap::HasPersistedRecordsForModelId(const std::string& model_id) {
  QP_LOG(INFO) << __func__;
  PrefService* local_state = Shell::Get()->local_state();
  if (!local_state) {
    QP_LOG(WARNING) << __func__ << ": No shell local state available.";
    return false;
  }

  const base::Value::Dict& device_id_map_dict =
      local_state->GetDict(kDeviceIdMapPref);
  for (std::pair<const std::string&, const base::Value&> record :
       device_id_map_dict) {
    if (record.second.GetString() == model_id)
      return true;
  }
  return false;
}

void DeviceIdMap::RefreshCacheForTest() {
  QP_LOG(INFO) << __func__;
  device_id_to_model_id_.clear();
  LoadPersistedRecordsFromPrefs();
}

void DeviceIdMap::LoadPersistedRecordsFromPrefs() {
  QP_LOG(INFO) << __func__;
  PrefService* local_state = Shell::Get()->local_state();
  if (!local_state) {
    QP_LOG(WARNING) << __func__ << ": No shell local state available.";
    return;
  }

  const base::Value::Dict& device_id_map_dict =
      local_state->GetDict(kDeviceIdMapPref);
  for (std::pair<const std::string&, const base::Value&> record :
       device_id_map_dict) {
    device_id_to_model_id_[record.first] = record.second.GetString();
  }
}

absl::optional<const std::string> DeviceIdMap::GetDeviceIdForAddress(
    const std::string& address) {
  if (!bluetooth_adapter_) {
    QP_LOG(WARNING) << __func__ << ": Can't fetch device ID without adapter.";
    return absl::nullopt;
  }

  const device::BluetoothDevice* device =
      bluetooth_adapter_->GetDevice(address);
  if (!device) {
    QP_LOG(WARNING) << __func__ << ": Can't find matching bluetooth device";
    return absl::nullopt;
  }
  return device->GetIdentifier();
}

}  // namespace quick_pair
}  // namespace ash
