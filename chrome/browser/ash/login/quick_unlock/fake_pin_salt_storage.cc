// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/quick_unlock/fake_pin_salt_storage.h"

#include <string>

#include "base/containers/flat_map.h"
#include "chrome/browser/ash/login/quick_unlock/pin_salt_storage.h"
#include "components/account_id/account_id.h"

namespace ash {
namespace quick_unlock {

FakePinSaltStorage::FakePinSaltStorage() = default;
FakePinSaltStorage::~FakePinSaltStorage() = default;

std::string FakePinSaltStorage::GetSalt(const AccountId& account_id) const {
  auto salt = salt_storage_.find(account_id);
  if (salt != salt_storage_.end()) {
    return salt->second;
  }
  return std::string();
}

void FakePinSaltStorage::WriteSalt(const AccountId& account_id,
                                   const std::string& salt) {
  salt_storage_.insert_or_assign(account_id, salt);
}

}  // namespace quick_unlock
}  // namespace ash
