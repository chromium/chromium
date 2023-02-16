// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/easy_unlock/easy_unlock_auth_attempt.h"

#include "chromeos/ash/components/proximity_auth/screenlock_bridge.h"

namespace ash {
namespace {

void OnAuthAttemptFinalized(bool success, const AccountId& account_id) {
  if (!proximity_auth::ScreenlockBridge::Get()->IsLocked())
    return;

  if (success) {
    proximity_auth::ScreenlockBridge::Get()->lock_handler()->Unlock(account_id);
  } else {
    proximity_auth::ScreenlockBridge::Get()->lock_handler()->EnableInput();
  }
}

}  // namespace

EasyUnlockAuthAttempt::EasyUnlockAuthAttempt(const AccountId& account_id)
    : state_(STATE_IDLE), account_id_(account_id) {}

EasyUnlockAuthAttempt::~EasyUnlockAuthAttempt() {
  if (state_ == STATE_RUNNING)
    Cancel(account_id_);
}

bool EasyUnlockAuthAttempt::Start() {
  DCHECK_EQ(STATE_IDLE, state_);

  if (!proximity_auth::ScreenlockBridge::Get()->IsLocked())
    return false;

  proximity_auth::mojom::AuthType auth_type =
      proximity_auth::ScreenlockBridge::Get()->lock_handler()->GetAuthType(
          account_id_);

  if (auth_type != proximity_auth::mojom::AuthType::USER_CLICK) {
    Cancel(account_id_);
    return false;
  }

  state_ = STATE_RUNNING;

  return true;
}

void EasyUnlockAuthAttempt::FinalizeUnlock(const AccountId& account_id,
                                           bool success) {
  if (state_ != STATE_RUNNING || account_id != account_id_)
    return;

  if (!proximity_auth::ScreenlockBridge::Get()->IsLocked())
    return;

  OnAuthAttemptFinalized(success, account_id);
  state_ = STATE_DONE;
}

void EasyUnlockAuthAttempt::Cancel(const AccountId& account_id) {
  state_ = STATE_DONE;

  const bool kFailure = false;
  OnAuthAttemptFinalized(kFailure, account_id);
}

}  // namespace ash
