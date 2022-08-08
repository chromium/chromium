// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_EASY_UNLOCK_EASY_UNLOCK_REMOVE_KEYS_OPERATION_H_
#define CHROME_BROWSER_ASH_LOGIN_EASY_UNLOCK_EASY_UNLOCK_REMOVE_KEYS_OPERATION_H_

#include <stddef.h>

#include "ash/components/login/auth/public/user_context.h"
#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/dbus/cryptohome/UserDataAuth.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

// A class to remove existing Easy unlock cryptohome keys starting at given
// index.
class EasyUnlockRemoveKeysOperation {
 public:
  using RemoveKeysCallback = base::OnceCallback<void(bool success)>;
  EasyUnlockRemoveKeysOperation(const UserContext& user_context,
                                size_t start_index,
                                RemoveKeysCallback callback);

  EasyUnlockRemoveKeysOperation(const EasyUnlockRemoveKeysOperation&) = delete;
  EasyUnlockRemoveKeysOperation& operator=(
      const EasyUnlockRemoveKeysOperation&) = delete;

  ~EasyUnlockRemoveKeysOperation();

  void Start();

 private:
  void OnGetSystemSalt(const std::string& system_salt);

  void RemoveKey();
  void OnKeyRemoved(absl::optional<::user_data_auth::RemoveKeyReply> reply);

  UserContext user_context_;
  RemoveKeysCallback callback_;
  size_t key_index_;
  base::WeakPtrFactory<EasyUnlockRemoveKeysOperation> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_EASY_UNLOCK_EASY_UNLOCK_REMOVE_KEYS_OPERATION_H_
