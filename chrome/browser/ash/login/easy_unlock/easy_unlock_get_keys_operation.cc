// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/easy_unlock/easy_unlock_get_keys_operation.h"

#include <vector>

#include "base/logging.h"

namespace ash {

EasyUnlockGetKeysOperation::EasyUnlockGetKeysOperation(
    const UserContext& user_context,
    GetKeysCallback callback)
    : callback_(std::move(callback)) {}

EasyUnlockGetKeysOperation::~EasyUnlockGetKeysOperation() {}

void EasyUnlockGetKeysOperation::Start() {
  EasyUnlockDeviceKeyDataList list;
  std::move(callback_).Run(true, list);
}

}  // namespace ash
