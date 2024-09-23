// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_USER_STATE_H_
#define ASH_LOGIN_UI_USER_STATE_H_

#include <optional>

#include "ash/public/cpp/login_types.h"
#include "ash/public/cpp/smartlock_state.h"
#include "base/time/time.h"
#include "chromeos/ash/components/cryptohome/auth_factor.h"
#include "components/account_id/account_id.h"

namespace ash {

class UserState {
 public:
  explicit UserState(const LoginUserInfo& user_info);
  UserState(UserState&&);

  UserState(const UserState&) = delete;
  UserState& operator=(const UserState&) = delete;

  ~UserState();

  AccountId account_id;
  bool show_password = true;
  bool show_pin = false;
  bool show_challenge_response_auth = false;
  bool enable_tap_auth = false;
  bool force_online_sign_in = false;
  bool disable_auth = false;
  bool show_pin_pad_for_password = false;
  size_t autosubmit_pin_length = 0;
  FingerprintState fingerprint_state = FingerprintState::UNAVAILABLE;
  SmartLockState smart_lock_state = SmartLockState::kDisabled;
  bool auth_factor_is_hiding_password = false;
  // When present, indicates that the TPM is locked.
  std::optional<base::TimeDelta> time_until_tpm_unlock = std::nullopt;
  // When present, indicates that the PIN is soft locked.
  cryptohome::PinLockAvailability pin_available_at = std::nullopt;
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_USER_STATE_H_
