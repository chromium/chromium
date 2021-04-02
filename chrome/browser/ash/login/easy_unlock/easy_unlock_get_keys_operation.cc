// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/easy_unlock/easy_unlock_get_keys_operation.h"

#include <stdint.h>

#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "chrome/browser/ash/login/easy_unlock/easy_unlock_key_manager.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/cryptohome/userdataauth_util.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/userdataauth/userdataauth_client.h"
#include "components/account_id/account_id.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace chromeos {

EasyUnlockGetKeysOperation::EasyUnlockGetKeysOperation(
    const UserContext& user_context,
    GetKeysCallback callback)
    : user_context_(user_context),
      callback_(std::move(callback)),
      key_index_(0) {}

EasyUnlockGetKeysOperation::~EasyUnlockGetKeysOperation() {}

void EasyUnlockGetKeysOperation::Start() {
  // Register for asynchronous notification of cryptohome being ready.
  UserDataAuthClient::Get()->WaitForServiceToBeAvailable(
      base::BindOnce(&EasyUnlockGetKeysOperation::OnCryptohomeAvailable,
                     weak_ptr_factory_.GetWeakPtr()));
}

void EasyUnlockGetKeysOperation::OnCryptohomeAvailable(bool available) {
  if (!available) {
    PA_LOG(ERROR) << "Failed to wait for cryptohome to become available";
    std::move(callback_).Run(false, EasyUnlockDeviceKeyDataList());
    return;
  }

  // Start the asynchronous key fetch.
  // TODO(xiyuan): Use ListKeyEx.
  key_index_ = 0;
  GetKeyData();
}

void EasyUnlockGetKeysOperation::GetKeyData() {
  user_data_auth::GetKeyDataRequest request;
  *request.mutable_account_id() =
      cryptohome::CreateAccountIdentifierFromAccountId(
          user_context_.GetAccountId());
  request.mutable_authorization_request();
  request.mutable_key()->mutable_data()->set_label(
      EasyUnlockKeyManager::GetKeyLabel(key_index_));

  UserDataAuthClient::Get()->GetKeyData(
      request, base::BindOnce(&EasyUnlockGetKeysOperation::OnGetKeyData,
                              weak_ptr_factory_.GetWeakPtr()));
}

void EasyUnlockGetKeysOperation::OnGetKeyData(
    base::Optional<user_data_auth::GetKeyDataReply> reply) {
  cryptohome::MountError return_code = user_data_auth::ReplyToMountError(reply);
  std::vector<cryptohome::KeyDefinition> key_definitions =
      user_data_auth::GetKeyDataReplyToKeyDefinitions(reply);
  if (return_code != cryptohome::MOUNT_ERROR_NONE || key_definitions.empty()) {
    // MOUNT_ERROR_KEY_FAILURE is considered as success.
    // Other error codes are treated as failures.
    if (return_code == cryptohome::MOUNT_ERROR_NONE ||
        return_code == cryptohome::MOUNT_ERROR_KEY_FAILURE) {
      // Prior to the introduction of the `unlock_key` field, only one
      // EasyUnlockDeviceKeyData was peristed, and implicitly assumed to be the
      // unlock key. Now, multiple EasyUnlockDeviceKeyData objects are
      // persisted, and this deserializing logic cannot assume that a given
      // object is the unlock key. To handle the case of migrating from the old
      // paradigm of a single persisted EasyUnlockDeviceKeyData, the
      // `unlock_key` field is defaulted to true if only a single device entry
      // exists, in order to correctly mark that old entry as the unlock key.
      if (devices_.size() == 1)
        devices_[0].unlock_key = true;

      std::move(callback_).Run(true, devices_);
      return;
    }

    PA_LOG(ERROR) << "Easy unlock failed to get key data, code=" << return_code;
    std::move(callback_).Run(false, EasyUnlockDeviceKeyDataList());
    return;
  }

  DCHECK_EQ(1u, key_definitions.size());

  const std::vector<cryptohome::KeyDefinition::ProviderData>& provider_data =
      key_definitions.front().provider_data;

  EasyUnlockDeviceKeyData device;
  for (size_t i = 0; i < provider_data.size(); ++i) {
    const cryptohome::KeyDefinition::ProviderData& entry = provider_data[i];
    if (entry.name == kEasyUnlockKeyMetaNameBluetoothAddress) {
      if (entry.bytes)
        device.bluetooth_address = *entry.bytes;
      else
        NOTREACHED();
    } else if (entry.name == kEasyUnlockKeyMetaNamePubKey) {
      if (entry.bytes)
        device.public_key = *entry.bytes;
      else
        NOTREACHED();
    } else if (entry.name == kEasyUnlockKeyMetaNamePsk) {
      if (entry.bytes)
        device.psk = *entry.bytes;
      else
        NOTREACHED();
    } else if (entry.name == kEasyUnlockKeyMetaNameChallenge) {
      if (entry.bytes)
        device.challenge = *entry.bytes;
      else
        NOTREACHED();
    } else if (entry.name == kEasyUnlockKeyMetaNameWrappedSecret) {
      if (entry.bytes)
        device.wrapped_secret = *entry.bytes;
      else
        NOTREACHED();
    } else if (entry.name == kEasyUnlockKeyMetaNameSerializedBeaconSeeds) {
      if (entry.bytes)
        device.serialized_beacon_seeds = *entry.bytes;
      else
        NOTREACHED();
    } else if (entry.name == kEasyUnlockKeyMetaNameUnlockKey) {
      // ProviderData only has the std::string `bytes` and int64_t `number`
      // fields for persistence -- the number field is used to store this
      // boolean. The boolean was stored as either a 1 or 0 in as an int64_t.
      // Cast it back to bool here.
      if (entry.number) {
        DCHECK(*entry.number == 0 || *entry.number == 1);
        device.unlock_key = static_cast<bool>(*entry.number);
      } else {
        NOTREACHED();
      }
    } else {
      PA_LOG(WARNING) << "Unknown EasyUnlock key data entry, name="
                      << entry.name;
    }
  }
  devices_.push_back(device);

  ++key_index_;
  GetKeyData();
}

}  // namespace chromeos
