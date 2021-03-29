// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_EASY_UNLOCK_EASY_UNLOCK_CREATE_KEYS_OPERATION_H_
#define CHROME_BROWSER_ASH_LOGIN_EASY_UNLOCK_EASY_UNLOCK_CREATE_KEYS_OPERATION_H_

#include <stddef.h>

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/easy_unlock/easy_unlock_types.h"
#include "chromeos/dbus/cryptohome/UserDataAuth.pb.h"
#include "chromeos/login/auth/user_context.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

class UserContext;

// A class to create Easy unlock cryptohome keys for the given user and devices.
class EasyUnlockCreateKeysOperation {
 public:
  using CreateKeysCallback = base::OnceCallback<void(bool success)>;
  EasyUnlockCreateKeysOperation(const UserContext& user_context,
                                const std::string& tpm_public_key,
                                const EasyUnlockDeviceKeyDataList& devices,
                                CreateKeysCallback callback);
  ~EasyUnlockCreateKeysOperation();

  void Start();

  // The UserContext returned will contain the new key if called after the
  // operation has completed successfully.
  const UserContext& user_context() const { return user_context_; }

 private:
  class ChallengeCreator;

  void CreateKeyForDeviceAtIndex(size_t index);
  void OnChallengeCreated(size_t index, bool success);
  void OnGetSystemSalt(size_t index, const std::string& system_salt);
  void OnKeyCreated(size_t index,
                    const Key& user_key,
                    base::Optional<::user_data_auth::AddKeyReply> reply);

  UserContext user_context_;
  std::string tpm_public_key_;
  EasyUnlockDeviceKeyDataList devices_;
  CreateKeysCallback callback_;

  // Index of the key to be created.
  size_t key_creation_index_;

  std::unique_ptr<ChallengeCreator> challenge_creator_;

  base::WeakPtrFactory<EasyUnlockCreateKeysOperation> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockCreateKeysOperation);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_LOGIN_EASY_UNLOCK_EASY_UNLOCK_CREATE_KEYS_OPERATION_H_
