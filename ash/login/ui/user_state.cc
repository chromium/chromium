// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/user_state.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "chromeos/ash/components/proximity_auth/public/mojom/auth_type.mojom.h"

namespace ash {

UserState::UserState(const LoginUserInfo& user_info)
    : account_id(user_info.basic_user_info.account_id) {
  fingerprint_state = user_info.fingerprint_state;
  smart_lock_state = user_info.smart_lock_state;
  if (user_info.auth_type == proximity_auth::mojom::AuthType::ONLINE_SIGN_IN) {
    force_online_sign_in = true;
  }
  show_pin_pad_for_password = user_info.show_pin_pad_for_password;
  disable_auth = !user_info.is_multi_user_sign_in_allowed &&
                 Shell::Get()->session_controller()->GetSessionState() ==
                     session_manager::SessionState::LOGIN_SECONDARY;
}

UserState::UserState(UserState&&) = default;

UserState::~UserState() = default;

}  // namespace ash
