// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_refresh_keys_operation.h"
#include "base/bind.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_create_keys_operation.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_remove_keys_operation.h"

namespace chromeos {

EasyUnlockRefreshKeysOperation::EasyUnlockRefreshKeysOperation(
    const UserContext& user_context,
    const std::string& tpm_public_key,
    const EasyUnlockDeviceKeyDataList& devices,
    const RefreshKeysCallback& callback)
    : user_context_(user_context),
      tpm_public_key_(tpm_public_key),
      devices_(devices),
      callback_(callback) {}

EasyUnlockRefreshKeysOperation::~EasyUnlockRefreshKeysOperation() {}

void EasyUnlockRefreshKeysOperation::Start() {
  if (devices_.empty()) {
    // Delete all keys from cryptohome so they can not be exploited.
    RemoveKeysStartingFromIndex(0);
    return;
  }

  create_keys_operation_.reset(new EasyUnlockCreateKeysOperation(
      user_context_, tpm_public_key_, devices_,
      base::Bind(&EasyUnlockRefreshKeysOperation::OnKeysCreated,
                 weak_ptr_factory_.GetWeakPtr())));
  create_keys_operation_->Start();
}

void EasyUnlockRefreshKeysOperation::OnKeysCreated(bool success) {
  if (!success) {
    callback_.Run(false);
    return;
  }

  // Update the user context to have the new authorization key after the create
  // keys operation. This is necessary because the old authorization key
  // associated with the user context will be invalidated after the create keys
  // operation.
  if (create_keys_operation_)
    user_context_ = create_keys_operation_->user_context();

  // Remove all keys that weren't overwritten by the create operation.
  RemoveKeysStartingFromIndex(devices_.size());
}

void EasyUnlockRefreshKeysOperation::RemoveKeysStartingFromIndex(
    size_t key_index) {
  remove_keys_operation_.reset(new EasyUnlockRemoveKeysOperation(
      user_context_, key_index,
      base::Bind(&EasyUnlockRefreshKeysOperation::OnKeysRemoved,
                 weak_ptr_factory_.GetWeakPtr())));
  remove_keys_operation_->Start();
}

void EasyUnlockRefreshKeysOperation::OnKeysRemoved(bool success) {
  callback_.Run(success);
}

}  // namespace chromeos
