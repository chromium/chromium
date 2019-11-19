// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_EASY_UNLOCK_EASY_UNLOCK_REMOVE_KEYS_OPERATION_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_EASY_UNLOCK_EASY_UNLOCK_REMOVE_KEYS_OPERATION_H_

#include <stddef.h>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/login/auth/user_context.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

class UserContext;

// A class to remove existing Easy unlock cryptohome keys starting at given
// index.
class EasyUnlockRemoveKeysOperation {
 public:
  typedef base::Callback<void(bool success)> RemoveKeysCallback;
  EasyUnlockRemoveKeysOperation(const UserContext& user_context,
                                size_t start_index,
                                const RemoveKeysCallback& callback);
  ~EasyUnlockRemoveKeysOperation();

  void Start();

 private:
  void OnGetSystemSalt(const std::string& system_salt);

  void RemoveKey();
  void OnKeyRemoved(bool success, cryptohome::MountError return_code);

  UserContext user_context_;
  RemoveKeysCallback callback_;
  size_t key_index_;
  base::WeakPtrFactory<EasyUnlockRemoveKeysOperation> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockRemoveKeysOperation);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_EASY_UNLOCK_EASY_UNLOCK_REMOVE_KEYS_OPERATION_H_
