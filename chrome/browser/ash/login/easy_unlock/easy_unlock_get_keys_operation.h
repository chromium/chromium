// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_EASY_UNLOCK_EASY_UNLOCK_GET_KEYS_OPERATION_H_
#define CHROME_BROWSER_ASH_LOGIN_EASY_UNLOCK_EASY_UNLOCK_GET_KEYS_OPERATION_H_

#include <stddef.h>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/easy_unlock/easy_unlock_types.h"
#include "chromeos/ash/components/dbus/cryptohome/rpc.pb.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

// TODO(b/227674947) : Remove this class as a part of cleanup;
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
  GetKeysCallback callback_;

  EasyUnlockDeviceKeyDataList devices_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_EASY_UNLOCK_EASY_UNLOCK_GET_KEYS_OPERATION_H_
