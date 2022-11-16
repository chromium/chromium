// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/easy_unlock/easy_unlock_refresh_keys_operation.h"

#include <memory>

namespace ash {

EasyUnlockRefreshKeysOperation::EasyUnlockRefreshKeysOperation(
    const UserContext& user_context,
    const std::string& tpm_public_key,
    const EasyUnlockDeviceKeyDataList& devices,
    RefreshKeysCallback callback)
    : callback_(std::move(callback)) {}

EasyUnlockRefreshKeysOperation::~EasyUnlockRefreshKeysOperation() {}

void EasyUnlockRefreshKeysOperation::Start() {
  std::move(callback_).Run(false);
}

}  // namespace ash
