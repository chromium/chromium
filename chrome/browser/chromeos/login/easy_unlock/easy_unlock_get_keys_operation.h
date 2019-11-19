// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_EASY_UNLOCK_EASY_UNLOCK_GET_KEYS_OPERATION_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_EASY_UNLOCK_EASY_UNLOCK_GET_KEYS_OPERATION_H_

#include <stddef.h>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_types.h"
#include "chromeos/cryptohome/homedir_methods.h"
#include "chromeos/dbus/cryptohome/rpc.pb.h"
#include "chromeos/login/auth/user_context.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

class EasyUnlockGetKeysOperation {
 public:
  typedef base::Callback<void(bool success,
                              const EasyUnlockDeviceKeyDataList& data_list)>
      GetKeysCallback;
  EasyUnlockGetKeysOperation(const UserContext& user_context,
                             const GetKeysCallback& callback);
  ~EasyUnlockGetKeysOperation();

  // Starts the operation. If the cryptohome service is not yet available, the
  // request will be deferred until it is ready.
  void Start();

 private:
  // Called once when the cryptohome service is available.
  void OnCryptohomeAvailable(bool available);

  // Asynchronously requests data for |key_index_| from cryptohome.
  void GetKeyData();

  // Callback for GetKeyData(). Updates |devices_|, increments |key_index_|, and
  // calls GetKeyData() again.
  void OnGetKeyData(base::Optional<cryptohome::BaseReply> reply);

  UserContext user_context_;
  GetKeysCallback callback_;

  size_t key_index_;
  EasyUnlockDeviceKeyDataList devices_;

  base::WeakPtrFactory<EasyUnlockGetKeysOperation> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockGetKeysOperation);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_EASY_UNLOCK_EASY_UNLOCK_GET_KEYS_OPERATION_H_
