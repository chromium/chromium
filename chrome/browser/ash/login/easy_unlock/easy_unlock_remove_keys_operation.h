// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_EASY_UNLOCK_EASY_UNLOCK_REMOVE_KEYS_OPERATION_H_
#define CHROME_BROWSER_ASH_LOGIN_EASY_UNLOCK_EASY_UNLOCK_REMOVE_KEYS_OPERATION_H_

#include <stddef.h>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

// TODO(b/227674947) : Remove this class as a part of cleanup;
//  A class to remove existing Easy unlock cryptohome keys starting at given
//  index.
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
  RemoveKeysCallback callback_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_EASY_UNLOCK_EASY_UNLOCK_REMOVE_KEYS_OPERATION_H_
