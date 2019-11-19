// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_EASY_UNLOCK_EASY_UNLOCK_REFRESH_KEYS_OPERATION_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_EASY_UNLOCK_EASY_UNLOCK_REFRESH_KEYS_OPERATION_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_types.h"
#include "chromeos/login/auth/user_context.h"

namespace chromeos {

class EasyUnlockCreateKeysOperation;
class EasyUnlockRemoveKeysOperation;
class UserContext;

// The refresh keys operation replaces the existing keys in cryptohome with a
// new list of keys. This operation is a simple sequence of the create and
// remove keys operations.
class EasyUnlockRefreshKeysOperation {
 public:
  typedef base::Callback<void(bool success)> RefreshKeysCallback;
  EasyUnlockRefreshKeysOperation(const UserContext& user_context,
                                 const std::string& tpm_public_key,
                                 const EasyUnlockDeviceKeyDataList& devices,
                                 const RefreshKeysCallback& callback);
  ~EasyUnlockRefreshKeysOperation();

  void Start();

 private:
  void OnKeysCreated(bool success);
  void RemoveKeysStartingFromIndex(size_t key_index);
  void OnKeysRemoved(bool success);

  UserContext user_context_;
  std::string tpm_public_key_;
  EasyUnlockDeviceKeyDataList devices_;
  RefreshKeysCallback callback_;

  std::unique_ptr<EasyUnlockCreateKeysOperation> create_keys_operation_;
  std::unique_ptr<EasyUnlockRemoveKeysOperation> remove_keys_operation_;

  base::WeakPtrFactory<EasyUnlockRefreshKeysOperation> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockRefreshKeysOperation);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_EASY_UNLOCK_EASY_UNLOCK_REFRESH_KEYS_OPERATION_H_
