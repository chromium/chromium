// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/easy_unlock/easy_unlock_create_keys_operation.h"

#include <memory>

#include "base/base64url.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "chrome/browser/ash/login/easy_unlock/easy_unlock_types.h"
#include "chromeos/ash/components/cryptohome/common_types.h"

namespace ash {

/////////////////////////////////////////////////////////////////////////////
// EasyUnlockCreateKeysOperation

EasyUnlockCreateKeysOperation::EasyUnlockCreateKeysOperation(
    const UserContext& user_context,
    const std::string& tpm_public_key,
    const EasyUnlockDeviceKeyDataList& devices,
    CreateKeysCallback callback)
    : user_context_(user_context), callback_(std::move(callback)) {
  // Must have the secret and callback.
  DCHECK(!user_context_.GetKey()->GetSecret().empty());
  DCHECK(!callback_.is_null());
}

EasyUnlockCreateKeysOperation::~EasyUnlockCreateKeysOperation() {}

void EasyUnlockCreateKeysOperation::Start() {
  std::move(callback_).Run(false);
}

}  // namespace ash
