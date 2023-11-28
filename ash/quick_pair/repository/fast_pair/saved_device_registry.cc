// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/repository/fast_pair/saved_device_registry.h"

#include <string>

#include "ash/quick_pair/common/quick_pair_browser_delegate.h"
#include "base/base64.h"
#include "base/containers/contains.h"
#include "components/cross_device/logging/logging.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "device/bluetooth//bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"

namespace {

const char kFastPairSavedDevicesPref[] = "fast_pair.saved_devices";

}  // namespace

namespace ash {
namespace quick_pair {

SavedDeviceRegistry::SavedDeviceRegistry(
    scoped_refptr<device::BluetoothAdapter> adapter)
    : adapter_(adapter) {}

SavedDeviceRegistry::~SavedDeviceRegistry() = default;

void SavedDeviceRegistry::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kFastPairSavedDevicesPref);
}

bool SavedDeviceRegistry::SaveAccountAssociation(
    const std::string& mac_address,
    const std::vector<uint8_t>& account_key) {
  PrefService* pref_service =
      QuickPairBrowserDelegate::Get()->GetActivePrefService();
  if (!pref_service) {
    CD_LOG(WARNING, Feature::FP)
        << __func__
        << ": No user pref service available. Failed to write "
           "account association to Saved Device Registry.";
    return false;
  }
  std::string encoded = base::Base64Encode(account_key);
  ScopedDictPrefUpdate update(pref_service, kFastPairSavedDevicesPref);
  update->Set(mac_address, encoded);
  CD_LOG(INFO, Feature::FP) << __func__ << ": Saved account key.";
  return true;
}

bool SavedDeviceRegistry::DeleteAccountKey(const std::string& mac_address) {
  PrefService* pref_service =
      QuickPairBrowserDelegate::Get()->GetActivePrefService();
  if (!pref_service) {
    CD_LOG(WARNING, Feature::FP)
        << __func__ << ": No user pref service available.";
    return false;
  }

  ScopedDictPrefUpdate update(pref_service, kFastPairSavedDevicesPref);
  if (!update->Remove(mac_address)) {
    CD_LOG(WARNING, Feature::FP)
        << __func__
        << ": Failed to delete mac address -> account key record from prefs";
    return false;
  }
  return true;
}

bool SavedDeviceRegistry::DeleteAccountKey(
    const std::vector<uint8_t>& account_key) {
  PrefService* pref_service =
      QuickPairBrowserDelegate::Get()->GetActivePrefService();
  if (!pref_service) {
    CD_LOG(WARNING, Feature::FP)
        << __func__ << ": No user pref service available.";
    return false;
  }

  const base::Value::Dict& saved_devices =
      pref_service->GetDict(kFastPairSavedDevicesPref);
  std::string encoded_key = base::Base64Encode(account_key);
  for (const auto it : saved_devices) {
    const std::string* value = it.second.GetIfString();
    if (value && *value == encoded_key) {
      ScopedDictPrefUpdate update(pref_service, kFastPairSavedDevicesPref);
      return update->Remove(it.first);
    }
  }
  CD_LOG(WARNING, Feature::FP)
      << __func__
      << ": Failed to delete account key record from prefs: "
         "account key not found";
  return false;
}

std::optional<const std::vector<uint8_t>> SavedDeviceRegistry::GetAccountKey(
    const std::string& mac_address) {
  PrefService* pref_service =
      QuickPairBrowserDelegate::Get()->GetActivePrefService();
  if (!pref_service) {
    CD_LOG(WARNING, Feature::FP)
        << __func__ << ": No user pref service available.";
    return std::nullopt;
  }

  const std::string* result = pref_service->GetValue(kFastPairSavedDevicesPref)
                                  .GetDict()
                                  .FindString(mac_address);
  if (!result) {
    return std::nullopt;
  }

  std::string decoded;
  if (!base::Base64Decode(*result, &decoded)) {
    CD_LOG(WARNING, Feature::FP)
        << __func__ << ": Failed to decode the account key from Base64.";
    return std::nullopt;
  }

  return std::vector<uint8_t>(decoded.begin(), decoded.end());
}

bool SavedDeviceRegistry::IsAccountKeySavedToRegistry(
    const std::vector<uint8_t>& account_key) {
  PrefService* pref_service =
      QuickPairBrowserDelegate::Get()->GetActivePrefService();
  if (!pref_service) {
    CD_LOG(WARNING, Feature::FP)
        << __func__ << ": No user pref service available.";
    return false;
  }

  if (!has_updated_saved_devices_registry_) {
    CD_LOG(INFO, Feature::FP)
        << __func__
        << ": checking for changes to the registry by cross checking "
           "the adapter before continuing";
    RemoveDevicesIfRemovedFromDifferentUser(pref_service);
  }

  const base::Value::Dict& saved_devices =
      pref_service->GetDict(kFastPairSavedDevicesPref);
  std::string encoded_key = base::Base64Encode(account_key);
  for (const auto it : saved_devices) {
    const std::string* value = it.second.GetIfString();
    if (value && *value == encoded_key) {
      return true;
    }
  }

  return false;
}

void SavedDeviceRegistry::RemoveDevicesIfRemovedFromDifferentUser(
    PrefService* pref_service) {
  DCHECK(pref_service);

  // Set of currently paired devices, stored by Bluetooth address, used to
  // cross reference the registry for any devices that need to be removed.
  std::set<std::string> paired_devices;
  for (device::BluetoothDevice* device : adapter_->GetDevices()) {
    if (device->IsPaired()) {
      paired_devices.insert(device->GetAddress());
    }
  }

  // Iterate over the list of devices in the registry, and if there are any in
  // the registry that are no longer paired to the adapter (determined by mac
  // address), remove them from the registry.
  const base::Value::Dict& saved_devices =
      pref_service->GetDict(kFastPairSavedDevicesPref);
  for (const auto it : saved_devices) {
    const std::string& mac_address = it.first;
    if (!base::Contains(paired_devices, mac_address)) {
      ScopedDictPrefUpdate update(pref_service, kFastPairSavedDevicesPref);
      update->Remove(it.first);
      CD_LOG(VERBOSE, Feature::FP)
          << __func__
          << ": removed device from registry at address= " << mac_address;
    }
  }

  has_updated_saved_devices_registry_ = true;
}

}  // namespace quick_pair
}  // namespace ash
