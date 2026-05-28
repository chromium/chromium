// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/quick_unlock/pin_salt_storage.h"

#include <string>

#include "ash/constants/ash_pref_names.h"
#include "base/check_deref.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/known_user.h"

namespace ash {
namespace quick_unlock {

PinSaltStorageImpl::PinSaltStorageImpl(PrefService* local_state)
    : local_state_(CHECK_DEREF(local_state)) {}

PinSaltStorageImpl::~PinSaltStorageImpl() = default;

// Read the salt from local state.
std::string PinSaltStorageImpl::GetSalt(const AccountId& account_id) const {
  user_manager::KnownUser known_user(&local_state_.get());
  if (const std::string* salt =
          known_user.FindStringPath(account_id, prefs::kQuickUnlockPinSalt)) {
    return *salt;
  }
  return std::string();
}

// Write the salt to local state.
void PinSaltStorageImpl::WriteSalt(const AccountId& account_id,
                                   const std::string& salt) {
  user_manager::KnownUser known_user(&local_state_.get());
  known_user.SetStringPref(account_id, prefs::kQuickUnlockPinSalt, salt);
}

}  // namespace quick_unlock
}  // namespace ash
