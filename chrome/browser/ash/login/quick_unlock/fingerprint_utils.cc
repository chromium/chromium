// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/quick_unlock/fingerprint_utils.h"

#include "ash/public/cpp/login_types.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_factory.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_storage.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_utils.h"
#include "components/user_manager/user.h"

namespace ash {
namespace quick_unlock {

FingerprintState GetFingerprintStateForUser(const user_manager::User* user,
                                            Purpose purpose) {
  QuickUnlockStorage* quick_unlock_storage =
      QuickUnlockFactory::GetForUser(user);
  // Quick unlock storage must be available.
  if (!user->is_logged_in() || !quick_unlock_storage)
    return FingerprintState::UNAVAILABLE;
  return quick_unlock_storage->GetFingerprintState(purpose);
}

}  // namespace quick_unlock
}  // namespace ash
