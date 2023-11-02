// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_EASY_UNLOCK_EASY_UNLOCK_KEY_MANAGER_H_
#define CHROME_BROWSER_ASH_LOGIN_EASY_UNLOCK_EASY_UNLOCK_KEY_MANAGER_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <string>

#include "base/callback.h"
#include "base/containers/circular_deque.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/login/easy_unlock/easy_unlock_get_keys_operation.h"
#include "chrome/browser/ash/login/easy_unlock/easy_unlock_refresh_keys_operation.h"
#include "chrome/browser/ash/login/easy_unlock/easy_unlock_types.h"

class AccountId;

namespace ash {

class UserContext;

// A class to manage Easy unlock cryptohome keys.
class EasyUnlockKeyManager {
 public:
  using RefreshKeysCallback =
      EasyUnlockRefreshKeysOperation::RefreshKeysCallback;
  using GetDeviceDataListCallback = EasyUnlockGetKeysOperation::GetKeysCallback;

  EasyUnlockKeyManager();

  EasyUnlockKeyManager(const EasyUnlockKeyManager&) = delete;
  EasyUnlockKeyManager& operator=(const EasyUnlockKeyManager&) = delete;

  ~EasyUnlockKeyManager();

  // Clears existing Easy unlock keys and creates new ones for the given
  // `remote_devices` and the given `user_context`. `user_context` must have
  // secret to allow keys to be created.
  void RefreshKeys(const UserContext& user_context,
                   const base::Value::List& remote_devices,
                   RefreshKeysCallback callback);

  // Retrieves the remote device data from cryptohome keys for the given
  // `user_context`.
  void GetDeviceDataList(const UserContext& user_context,
                         GetDeviceDataListCallback callback);

  // Helpers to convert between DeviceData and remote device dictionary.
  // DeviceDataToRemoteDeviceDictionary fills the remote device dictionary and
  // always succeeds. RemoteDeviceDictionaryToDeviceData returns false if the
  // conversion fails (missing required propery). Note that
  // EasyUnlockDeviceKeyData contains a sub set of the remote device dictionary.
  static void DeviceDataToRemoteDeviceDictionary(
      const AccountId& account_id,
      const EasyUnlockDeviceKeyData& data,
      base::Value::Dict* dict);
  static bool RemoteDeviceDictionaryToDeviceData(const base::Value::Dict& dict,
                                                 EasyUnlockDeviceKeyData* data);

  // Helpers to convert between EasyUnlockDeviceKeyDataList and remote devices
  // base::Value::List.
  static void DeviceDataListToRemoteDeviceList(
      const AccountId& account_id,
      const EasyUnlockDeviceKeyDataList& data_list,
      base::Value::List* device_list);
  static bool RemoteDeviceRefListToDeviceDataList(
      const base::Value::List& device_list,
      EasyUnlockDeviceKeyDataList* data_list);

  // Gets key label for the given key index.
  static std::string GetKeyLabel(size_t key_index);

 private:
  // Runs the next operation if there is one. We first run all the operations in
  // the `write_operation_queue_` and then run all the operations in the
  // `read_operation_queue_`.
  void RunNextOperation();

  // Called when the TPM key is ready to be used for creating Easy Unlock key
  // challenges.
  void RefreshKeysWithTpmKeyPresent(const UserContext& user_context,
                                    base::Value::List remote_devices,
                                    RefreshKeysCallback callback);

  // Callback invoked after refresh keys operation.
  void OnKeysRefreshed(RefreshKeysCallback callback, bool create_success);

  // Callback invoked after get keys op.
  void OnKeysFetched(GetDeviceDataListCallback callback,
                     bool fetch_success,
                     const EasyUnlockDeviceKeyDataList& fetched_data);

  base::circular_deque<std::unique_ptr<EasyUnlockRefreshKeysOperation>>
      write_operation_queue_;
  base::circular_deque<std::unique_ptr<EasyUnlockGetKeysOperation>>
      read_operation_queue_;

  // Stores the current operation in progress. At most one of these variables
  // can be non-null at any time.
  std::unique_ptr<EasyUnlockRefreshKeysOperation> pending_write_operation_;
  std::unique_ptr<EasyUnlockGetKeysOperation> pending_read_operation_;

  base::WeakPtrFactory<EasyUnlockKeyManager> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_EASY_UNLOCK_EASY_UNLOCK_KEY_MANAGER_H_
