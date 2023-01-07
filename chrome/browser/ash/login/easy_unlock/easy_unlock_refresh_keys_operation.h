// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_EASY_UNLOCK_EASY_UNLOCK_REFRESH_KEYS_OPERATION_H_
#define CHROME_BROWSER_ASH_LOGIN_EASY_UNLOCK_EASY_UNLOCK_REFRESH_KEYS_OPERATION_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/easy_unlock/easy_unlock_types.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"

namespace ash {

// TODO(b/227674947) : Remove this class as a part of cleanup;
//  The refresh keys operation replaces the existing keys in cryptohome with a
//  new list of keys. This operation is a simple sequence of the create and
//  remove keys operations.
class EasyUnlockRefreshKeysOperation {
 public:
  using RefreshKeysCallback = base::OnceCallback<void(bool success)>;
  EasyUnlockRefreshKeysOperation(const UserContext& user_context,
                                 const std::string& tpm_public_key,
                                 const EasyUnlockDeviceKeyDataList& devices,
                                 RefreshKeysCallback callback);

  EasyUnlockRefreshKeysOperation(const EasyUnlockRefreshKeysOperation&) =
      delete;
  EasyUnlockRefreshKeysOperation& operator=(
      const EasyUnlockRefreshKeysOperation&) = delete;

  ~EasyUnlockRefreshKeysOperation();

  void Start();

 private:
  RefreshKeysCallback callback_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_EASY_UNLOCK_EASY_UNLOCK_REFRESH_KEYS_OPERATION_H_
