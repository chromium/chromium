// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/easy_unlock/easy_unlock_key_manager.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/ash/login/easy_unlock/easy_unlock_key_names.h"
#include "chrome/browser/ash/login/easy_unlock/easy_unlock_tpm_key_manager.h"
#include "chrome/browser/ash/login/easy_unlock/easy_unlock_tpm_key_manager_factory.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "components/account_id/account_id.h"

namespace chromeos {

EasyUnlockKeyManager::EasyUnlockKeyManager() {}

EasyUnlockKeyManager::~EasyUnlockKeyManager() {}

void EasyUnlockKeyManager::RefreshKeys(const UserContext& user_context,
                                       const base::ListValue& remote_devices,
                                       RefreshKeysCallback callback) {
  EasyUnlockTpmKeyManager* tpm_key_manager =
      EasyUnlockTpmKeyManagerFactory::GetInstance()->GetForUser(
          user_context.GetAccountId().GetUserEmail());
  if (!tpm_key_manager) {
    PA_LOG(ERROR) << "No TPM key manager.";
    std::move(callback).Run(false);
    return;
  }

  // This callback will only be executed once but must be marked repeating
  // because it could be discarded by PrepareTpmKey() and invoked here instead.
  auto do_refresh_keys = base::BindRepeating(
      &EasyUnlockKeyManager::RefreshKeysWithTpmKeyPresent,
      weak_ptr_factory_.GetWeakPtr(), user_context,
      base::Owned(remote_devices.DeepCopy()), base::Passed(&callback));

  // Private TPM key is needed only when adding new keys.
  if (remote_devices.empty() ||
      tpm_key_manager->PrepareTpmKey(/*check_private_key=*/false,
                                     do_refresh_keys)) {
    do_refresh_keys.Run();
  } else {
    // In case Chrome is supposed to restart to apply user session flags, the
    // Chrome restart will be postponed until Easy Sign-in keys are refreshed.
    // This is to ensure that creating TPM key does not hang if TPM system
    // loading takes too much time. Note that in normal circumstances the
    // chances that TPM slot cannot be loaded should be extremely low.
    // TODO(tbarzic): Add some metrics to measure if the timeout even gets hit.
    tpm_key_manager->StartGetSystemSlotTimeoutMs(2000);
  }
}

void EasyUnlockKeyManager::RefreshKeysWithTpmKeyPresent(
    const UserContext& user_context,
    base::ListValue* remote_devices,
    RefreshKeysCallback callback) {
  EasyUnlockTpmKeyManager* tpm_key_manager =
      EasyUnlockTpmKeyManagerFactory::GetInstance()->GetForUser(
          user_context.GetAccountId().GetUserEmail());
  const std::string tpm_public_key =
      tpm_key_manager->GetPublicTpmKey(user_context.GetAccountId());

  EasyUnlockDeviceKeyDataList devices;
  if (!RemoteDeviceRefListToDeviceDataList(*remote_devices, &devices))
    devices.clear();

  write_operation_queue_.push_back(
      std::make_unique<EasyUnlockRefreshKeysOperation>(
          user_context, tpm_public_key, devices,
          base::BindOnce(&EasyUnlockKeyManager::OnKeysRefreshed,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback))));
  RunNextOperation();
}

void EasyUnlockKeyManager::GetDeviceDataList(
    const UserContext& user_context,
    GetDeviceDataListCallback callback) {
  read_operation_queue_.push_back(std::make_unique<EasyUnlockGetKeysOperation>(
      user_context,
      base::BindOnce(&EasyUnlockKeyManager::OnKeysFetched,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback))));
  RunNextOperation();
}

// static
void EasyUnlockKeyManager::DeviceDataToRemoteDeviceDictionary(
    const AccountId& account_id,
    const EasyUnlockDeviceKeyData& data,
    base::DictionaryValue* dict) {
  dict->SetString(key_names::kKeyBluetoothAddress, data.bluetooth_address);
  dict->SetString(key_names::kKeyPsk, data.psk);
  std::unique_ptr<base::DictionaryValue> permit_record(
      new base::DictionaryValue);
  dict->Set(key_names::kKeyPermitRecord, std::move(permit_record));
  dict->SetString(key_names::kKeyPermitId, data.public_key);
  dict->SetString(key_names::kKeyPermitData, data.public_key);
  dict->SetString(key_names::kKeyPermitType, key_names::kPermitTypeLicence);
  dict->SetString(key_names::kKeyPermitPermitId,
                  base::StringPrintf(key_names::kPermitPermitIdFormat,
                                     account_id.GetUserEmail().c_str()));
  dict->SetString(key_names::kKeySerializedBeaconSeeds,
                  data.serialized_beacon_seeds);
  dict->SetBoolean(key_names::kKeyUnlockKey, data.unlock_key);
}

// static
bool EasyUnlockKeyManager::RemoteDeviceDictionaryToDeviceData(
    const base::DictionaryValue& dict,
    EasyUnlockDeviceKeyData* data) {
  std::string bluetooth_address;
  std::string public_key;
  std::string psk;

  if (!dict.GetString(key_names::kKeyBluetoothAddress, &bluetooth_address) ||
      !dict.GetString(key_names::kKeyPermitId, &public_key) ||
      !dict.GetString(key_names::kKeyPsk, &psk)) {
    return false;
  }

  std::string serialized_beacon_seeds;
  if (dict.GetString(key_names::kKeySerializedBeaconSeeds,
                     &serialized_beacon_seeds)) {
    data->serialized_beacon_seeds = serialized_beacon_seeds;
  } else {
    PA_LOG(ERROR) << "Failed to parse key data: "
                  << "expected serialized_beacon_seeds.";
  }

  bool unlock_key;
  if (!dict.GetBoolean(key_names::kKeyUnlockKey, &unlock_key)) {
    // If GetBoolean() fails, that means we're reading a Dictionary from
    // user prefs which did not include the bool when it was stored. That means
    // it's an older Dictionary that didn't include this `unlock_key` field --
    // only one device was persisted, and it was implicitly assumed to be the
    // unlock key -- thus `unlock_key` should default to being true.
    unlock_key = true;
  }
  data->unlock_key = unlock_key;

  data->bluetooth_address.swap(bluetooth_address);
  data->public_key.swap(public_key);
  data->psk.swap(psk);
  return true;
}

// static
void EasyUnlockKeyManager::DeviceDataListToRemoteDeviceList(
    const AccountId& account_id,
    const EasyUnlockDeviceKeyDataList& data_list,
    base::ListValue* device_list) {
  device_list->Clear();
  for (size_t i = 0; i < data_list.size(); ++i) {
    std::unique_ptr<base::DictionaryValue> device_dict(
        new base::DictionaryValue);
    DeviceDataToRemoteDeviceDictionary(account_id, data_list[i],
                                       device_dict.get());
    device_list->Append(std::move(device_dict));
  }
}

// static
bool EasyUnlockKeyManager::RemoteDeviceRefListToDeviceDataList(
    const base::ListValue& device_list,
    EasyUnlockDeviceKeyDataList* data_list) {
  EasyUnlockDeviceKeyDataList parsed_devices;
  for (base::ListValue::const_iterator it = device_list.begin();
       it != device_list.end(); ++it) {
    const base::DictionaryValue* dict;
    if (!it->GetAsDictionary(&dict) || !dict)
      return false;

    EasyUnlockDeviceKeyData data;
    if (!RemoteDeviceDictionaryToDeviceData(*dict, &data))
      return false;

    parsed_devices.push_back(data);
  }

  data_list->swap(parsed_devices);
  return true;
}

// static
std::string EasyUnlockKeyManager::GetKeyLabel(size_t key_index) {
  return base::StringPrintf("%s%zu", key_names::kKeyLabelPrefix, key_index);
}

void EasyUnlockKeyManager::RunNextOperation() {
  if (pending_write_operation_ || pending_read_operation_)
    return;

  if (!write_operation_queue_.empty()) {
    pending_write_operation_ = std::move(write_operation_queue_.front());
    write_operation_queue_.pop_front();
    pending_write_operation_->Start();
  } else if (!read_operation_queue_.empty()) {
    pending_read_operation_ = std::move(read_operation_queue_.front());
    read_operation_queue_.pop_front();
    pending_read_operation_->Start();
  }
}

void EasyUnlockKeyManager::OnKeysRefreshed(RefreshKeysCallback callback,
                                           bool refresh_success) {
  if (!callback.is_null())
    std::move(callback).Run(refresh_success);

  DCHECK(pending_write_operation_);
  pending_write_operation_.reset();
  RunNextOperation();
}

void EasyUnlockKeyManager::OnKeysFetched(
    GetDeviceDataListCallback callback,
    bool fetch_success,
    const EasyUnlockDeviceKeyDataList& fetched_data) {
  if (!callback.is_null())
    std::move(callback).Run(fetch_success, fetched_data);

  DCHECK(pending_read_operation_);
  pending_read_operation_.reset();
  RunNextOperation();
}

}  // namespace chromeos
