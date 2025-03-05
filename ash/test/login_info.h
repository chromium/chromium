// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_LOGIN_INFO_H_
#define ASH_TEST_LOGIN_INFO_H_

#include <memory>
#include <optional>
#include <string_view>

#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_type.h"

namespace ash {

struct LoginInfo {
  // The user's non canonicalized email address.
  const std::optional<std::string_view> display_email;

  // The user type of the account. See `user_manager::UserType` for more
  // details.
  const user_manager::UserType user_type = user_manager::UserType::kRegular;

  // True if the user's non-cryptohome data (wallpaper, avatar etc.) should be
  // ephemeral.
  const bool is_ephemeral = false;

  // True indicates whether the logged-in account is new.
  const bool is_new_profile = false;

  // The user's given name.
  const std::optional<std::string_view> given_name;

  // True if the account should be under policy management.
  const bool is_account_managed = false;

  // True to set the `SessionController`'s state to ACTIVE state.
  const bool activate_session = true;
};

inline constexpr std::string_view kDefaultUserEmail("user0@tray");

inline constexpr LoginInfo kRegularUserLoginInfo = {kDefaultUserEmail};

}  // namespace ash

#endif  // ASH_TEST_LOGIN_INFO_H_
