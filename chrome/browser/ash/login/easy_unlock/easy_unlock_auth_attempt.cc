// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/easy_unlock/easy_unlock_auth_attempt.h"

#include "base/bind.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "build/build_config.h"
#include "chrome/browser/ash/login/easy_unlock/easy_unlock_key_manager.h"
#include "chromeos/ash/components/proximity_auth/screenlock_bridge.h"
#include "crypto/encryptor.h"
#include "crypto/symmetric_key.h"

namespace ash {
namespace {

// Decrypts the secret that should be used to login from `wrapped_secret` using
// raw AES key `raw_key`.
// In a case of error, an empty string is returned.
std::string UnwrapSecret(const std::string& wrapped_secret,
                         const std::string& raw_key) {
  if (raw_key.empty())
    return std::string();

  // Import the key structure.
  std::unique_ptr<crypto::SymmetricKey> key(
      crypto::SymmetricKey::Import(crypto::SymmetricKey::AES, raw_key));

  if (!key)
    return std::string();

  std::string iv(raw_key.size(), ' ');
  crypto::Encryptor encryptor;
  if (!encryptor.Init(key.get(), crypto::Encryptor::CBC, iv))
    return std::string();

  std::string secret;
  if (!encryptor.Decrypt(wrapped_secret, &secret))
    return std::string();

  return secret;
}

void OnAuthAttemptFinalized(EasyUnlockAuthAttempt::Type auth_attempt_type,
                            bool success,
                            const AccountId& account_id,
                            const std::string& key_secret,
                            const std::string& key_label) {
  if (!proximity_auth::ScreenlockBridge::Get()->IsLocked())
    return;

  switch (auth_attempt_type) {
    case EasyUnlockAuthAttempt::TYPE_UNLOCK:
      if (success) {
        proximity_auth::ScreenlockBridge::Get()->lock_handler()->Unlock(
            account_id);
      } else {
        proximity_auth::ScreenlockBridge::Get()->lock_handler()->EnableInput();
      }
      return;
    case EasyUnlockAuthAttempt::TYPE_SIGNIN:
      if (success) {
        proximity_auth::ScreenlockBridge::Get()
            ->lock_handler()
            ->AttemptEasySignin(account_id, key_secret, key_label);
      } else {
        // Attempting signin with an empty secret is equivalent to canceling the
        // attempt.
        proximity_auth::ScreenlockBridge::Get()
            ->lock_handler()
            ->AttemptEasySignin(account_id, std::string(), std::string());
      }
      return;
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

void EasyUnlockAuthAttempt::FinalizeSignin(const AccountId& account_id,
                                           const std::string& wrapped_secret,
                                           const std::string& raw_session_key) {
  if (state_ != STATE_RUNNING || account_id != account_id_)
    return;

  if (!proximity_auth::ScreenlockBridge::Get()->IsLocked())
    return;

  if (type_ != TYPE_SIGNIN) {
    Cancel(account_id_);
    return;
  }

  if (wrapped_secret.empty()) {
    Cancel(account_id_);
    return;
  }

  std::string unwrapped_secret = UnwrapSecret(wrapped_secret, raw_session_key);
  std::string key_label = EasyUnlockKeyManager::GetKeyLabel(0u);

  const bool kSuccess = true;
  OnAuthAttemptFinalized(type_, kSuccess, account_id, unwrapped_secret,
                         key_label);
  state_ = STATE_DONE;
}

void EasyUnlockAuthAttempt::Cancel(const AccountId& account_id) {
  state_ = STATE_DONE;

  const bool kFailure = false;
  OnAuthAttemptFinalized(type_, kFailure, account_id, std::string(),
                         std::string());
}

}  // namespace ash
