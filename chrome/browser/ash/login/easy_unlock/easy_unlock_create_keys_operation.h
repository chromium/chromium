// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_EASY_UNLOCK_EASY_UNLOCK_CREATE_KEYS_OPERATION_H_
#define CHROME_BROWSER_ASH_LOGIN_EASY_UNLOCK_EASY_UNLOCK_CREATE_KEYS_OPERATION_H_

#include <stddef.h>

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/easy_unlock/easy_unlock_types.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

// TODO(b/227674947) : Remove this class as a part of cleanup;
//  A class to create Easy unlock cryptohome keys for the given user and
//  devices.
class EasyUnlockCreateKeysOperation {
 public:
  using CreateKeysCallback = base::OnceCallback<void(bool success)>;
  EasyUnlockCreateKeysOperation(const UserContext& user_context,
                                const std::string& tpm_public_key,
                                const EasyUnlockDeviceKeyDataList& devices,
                                CreateKeysCallback callback);

  EasyUnlockCreateKeysOperation(const EasyUnlockCreateKeysOperation&) = delete;
  EasyUnlockCreateKeysOperation& operator=(
      const EasyUnlockCreateKeysOperation&) = delete;

  ~EasyUnlockCreateKeysOperation();

  void Start();

  // The UserContext returned will contain the new key if called after the
  // operation has completed successfully.
  const UserContext& user_context() const { return user_context_; }

 private:
  UserContext user_context_;
  CreateKeysCallback callback_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_EASY_UNLOCK_EASY_UNLOCK_CREATE_KEYS_OPERATION_H_
