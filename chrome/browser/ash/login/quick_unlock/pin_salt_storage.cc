// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/quick_unlock/pin_salt_storage.h"

#include <string>

#include "ash/constants/ash_pref_names.h"
#include "chrome/browser/browser_process.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/known_user.h"

namespace ash {
namespace quick_unlock {

PinSaltStorage::PinSaltStorage() = default;
PinSaltStorage::~PinSaltStorage() = default;

// Read the salt from local state.
std::string PinSaltStorage::GetSalt(const AccountId& account_id) const {
  user_manager::KnownUser known_user(g_browser_process->local_state());
  if (const std::string* salt =
          known_user.FindStringPath(account_id, prefs::kQuickUnlockPinSalt)) {
    return *salt;
  }
  return std::string();
}

// Write the salt to local state.
void PinSaltStorage::WriteSalt(const AccountId& account_id,
                               const std::string& salt) {
  user_manager::KnownUser known_user(g_browser_process->local_state());
  known_user.SetStringPref(account_id, prefs::kQuickUnlockPinSalt, salt);
}

}  // namespace quick_unlock
}  // namespace ash
