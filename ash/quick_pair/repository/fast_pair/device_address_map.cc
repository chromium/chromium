// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/repository/fast_pair/device_address_map.h"

#include "ash/shell.h"
#include "base/values.h"
#include "components/cross_device/logging/logging.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace ash::quick_pair {

// static
constexpr char DeviceAddressMap::kDeviceAddressMapPref[];

// static
void DeviceAddressMap::RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kDeviceAddressMapPref);
}

DeviceAddressMap::DeviceAddressMap() = default;

DeviceAddressMap::~DeviceAddressMap() = default;

bool DeviceAddressMap::SaveModelIdForDevice(scoped_refptr<Device> device) {
  if (!device->classic_address()) {
    return false;
  }

  mac_address_to_model_id_[device->classic_address().value()] =
      device->metadata_id();
  return true;
}

bool DeviceAddressMap::PersistRecordsForDevice(scoped_refptr<Device> device) {
  if (!device->classic_address()) {
    return false;
  }

  return PersistMacAddressRecord(device->classic_address().value());
}

bool DeviceAddressMap::PersistMacAddressRecord(const std::string& mac_address) {
  const std::string& model_id = mac_address_to_model_id_[mac_address];

  if (model_id.empty()) {
    CD_LOG(VERBOSE, Feature::FP)
        << __func__
        << ": Can't persist null mac address -> model ID record "
           "for mac address: " +
               mac_address;
    return false;
  }

  PrefService* local_state = Shell::Get()->local_state();
  if (!local_state) {
    CD_LOG(WARNING, Feature::FP)
        << __func__ << ": No shell local state available.";
    return false;
  }

  ScopedDictPrefUpdate device_address_map_dict(local_state,
                                               kDeviceAddressMapPref);
  if (!device_address_map_dict->Set(mac_address, model_id)) {
    CD_LOG(VERBOSE, Feature::FP)
        << __func__
        << ": Failed to persist mac address -> model ID record for "
           "mac address: " +
               mac_address;
    return false;
  }
  return true;
}

bool DeviceAddressMap::EvictMacAddressRecord(const std::string& mac_address) {
  PrefService* local_state = Shell::Get()->local_state();
  if (!local_state) {
    CD_LOG(WARNING, Feature::FP)
        << __func__ << ": No shell local state available.";
    return false;
  }

  ScopedDictPrefUpdate device_address_map_dict(local_state,
                                               kDeviceAddressMapPref);
  if (!device_address_map_dict->Remove(mac_address)) {
    CD_LOG(VERBOSE, Feature::FP)
        << __func__
        << ": Failed to evict mac address -> model ID record from "
           "prefs for mac address: " +
               mac_address;
    return false;
  }
  return true;
}

std::optional<const std::string> DeviceAddressMap::GetModelIdForMacAddress(
    const std::string& mac_address) {
  // Lazily load saved records from prefs the first time we get a model ID.
  if (!loaded_records_from_prefs_) {
    loaded_records_from_prefs_ = true;
    LoadPersistedRecordsFromPrefs();
  }

  std::string& saved_model_id = mac_address_to_model_id_[mac_address];
  if (saved_model_id.empty()) {
    return std::nullopt;
  }
  return saved_model_id;
}

bool DeviceAddressMap::HasPersistedRecordsForModelId(
    const std::string& model_id) {
  CD_LOG(INFO, Feature::FP) << __func__;
  PrefService* local_state = Shell::Get()->local_state();
  if (!local_state) {
    CD_LOG(WARNING, Feature::FP)
        << __func__ << ": No shell local state available.";
    return false;
  }

  const base::Value::Dict& device_address_map_dict =
      local_state->GetDict(kDeviceAddressMapPref);
  for (std::pair<const std::string&, const base::Value&> record :
       device_address_map_dict) {
    if (record.second.GetString() == model_id) {
      return true;
    }
  }
  return false;
}

void DeviceAddressMap::RefreshCacheForTest() {
  CD_LOG(INFO, Feature::FP) << __func__;
  mac_address_to_model_id_.clear();
  LoadPersistedRecordsFromPrefs();
}

void DeviceAddressMap::LoadPersistedRecordsFromPrefs() {
  CD_LOG(INFO, Feature::FP) << __func__;
  PrefService* local_state = Shell::Get()->local_state();
  if (!local_state) {
    CD_LOG(WARNING, Feature::FP)
        << __func__ << ": No shell local state available.";
    return;
  }

  const base::Value::Dict& device_address_map_dict =
      local_state->GetDict(kDeviceAddressMapPref);
  for (std::pair<const std::string&, const base::Value&> record :
       device_address_map_dict) {
    mac_address_to_model_id_[record.first] = record.second.GetString();
  }
}

}  // namespace ash::quick_pair
