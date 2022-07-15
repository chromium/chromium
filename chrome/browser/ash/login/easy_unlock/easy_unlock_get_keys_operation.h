// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_EASY_UNLOCK_EASY_UNLOCK_GET_KEYS_OPERATION_H_
#define CHROME_BROWSER_ASH_LOGIN_EASY_UNLOCK_EASY_UNLOCK_GET_KEYS_OPERATION_H_

#include <stddef.h>

#include "ash/components/login/auth/public/user_context.h"
#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/easy_unlock/easy_unlock_types.h"
#include "chromeos/dbus/cryptohome/UserDataAuth.pb.h"
#include "chromeos/dbus/cryptohome/rpc.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

class EasyUnlockGetKeysOperation {
 public:
  using GetKeysCallback =
      base::OnceCallback<void(bool success,
                              const EasyUnlockDeviceKeyDataList& data_list)>;
  EasyUnlockGetKeysOperation(const UserContext& user_context,
                             GetKeysCallback callback);

  EasyUnlockGetKeysOperation(const EasyUnlockGetKeysOperation&) = delete;
  EasyUnlockGetKeysOperation& operator=(const EasyUnlockGetKeysOperation&) =
      delete;

  ~EasyUnlockGetKeysOperation();

  // Starts the operation. If the cryptohome service is not yet available, the
  // request will be deferred until it is ready.
  void Start();

 private:
  // Called once when the cryptohome service is available.
  void OnCryptohomeAvailable(bool available);

  // Asynchronously requests data for `key_index_` from cryptohome.
  void GetKeyData();

  // Callback for GetKeyData(). Updates `devices_`, increments `key_index_`, and
  // calls GetKeyData() again.
  void OnGetKeyData(absl::optional<user_data_auth::GetKeyDataReply> reply);

  UserContext user_context_;
  GetKeysCallback callback_;

  size_t key_index_;
  EasyUnlockDeviceKeyDataList devices_;

  base::WeakPtrFactory<EasyUnlockGetKeysOperation> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_EASY_UNLOCK_EASY_UNLOCK_GET_KEYS_OPERATION_H_
