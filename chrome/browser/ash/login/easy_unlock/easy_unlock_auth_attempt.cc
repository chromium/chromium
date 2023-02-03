// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/easy_unlock/easy_unlock_auth_attempt.h"

#include "chromeos/ash/components/proximity_auth/screenlock_bridge.h"

namespace ash {
namespace {

void OnAuthAttemptFinalized(EasyUnlockAuthAttempt::Type auth_attempt_type,
                            bool success,
                            const AccountId& account_id,
                            const std::string& key_secret,
                            const std::string& key_label) {
  if (!proximity_auth::ScreenlockBridge::Get()->IsLocked())
    return;

  // TODO(b/227674947): Remove type parameter since there is only one type of
  // EasyUnlockService now.
  DCHECK_EQ(EasyUnlockAuthAttempt::TYPE_UNLOCK, auth_attempt_type);
  if (success) {
    proximity_auth::ScreenlockBridge::Get()->lock_handler()->Unlock(account_id);
  } else {
    proximity_auth::ScreenlockBridge::Get()->lock_handler()->EnableInput();
  }
}

}  // namespace

EasyUnlockAuthAttempt::EasyUnlockAuthAttempt(const AccountId& account_id,
                                             Type type)
    : state_(STATE_IDLE), account_id_(account_id), type_(type) {}

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

  if (type_ != TYPE_UNLOCK) {
    Cancel(account_id_);
    return;
  }

  OnAuthAttemptFinalized(type_, success, account_id, std::string(),
                         std::string());
  state_ = STATE_DONE;
}

void EasyUnlockAuthAttempt::Cancel(const AccountId& account_id) {
  state_ = STATE_DONE;

  const bool kFailure = false;
  OnAuthAttemptFinalized(type_, kFailure, account_id, std::string(),
                         std::string());
}

}  // namespace ash
