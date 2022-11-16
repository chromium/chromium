// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/easy_unlock/easy_unlock_remove_keys_operation.h"

#include "base/bind.h"
#include "base/logging.h"
#include "chromeos/ash/components/cryptohome/common_types.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace ash {

EasyUnlockRemoveKeysOperation::EasyUnlockRemoveKeysOperation(
    const UserContext& user_context,
    size_t start_index,
    RemoveKeysCallback callback)
    : callback_(std::move(callback)) {}

EasyUnlockRemoveKeysOperation::~EasyUnlockRemoveKeysOperation() {}

void EasyUnlockRemoveKeysOperation::Start() {
  std::move(callback_).Run(false);
}

}  // namespace ash
