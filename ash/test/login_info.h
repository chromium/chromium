// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_LOGIN_INFO_H_
#define ASH_TEST_LOGIN_INFO_H_

#include <memory>
#include <optional>
#include <string_view>

#include "base/values.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_type.h"

namespace ash {

struct LoginInfo {
  // The account ID of the user. If not specified, a default AccountId is used
  // in `TestSessionControllerClient::AddUserSession()`.
  // Note: If a test refers to a user or session by `AccountId`, the test must
  // explicitly provide `account_id` and must not rely on the default AccountId
  // chosen by `TestSessionControllerClient`.
  std::optional<AccountId> account_id;

  // The user's non canonicalized email address. If not specified,
  // `account_id->GetUserEmail()` is used.
  std::optional<std::string_view> display_email;

  // The user type of the account. See `user_manager::UserType` for more
  // details.
  user_manager::UserType user_type = user_manager::UserType::kRegular;

  // Initial preference values for the user's `PrefService`, if any.
  std::optional<base::DictValue> init_pref_service_value;

  // True if the user's non-cryptohome data (wallpaper, avatar etc.) should be
  // ephemeral.
  bool is_ephemeral = false;

  // True indicates whether the logged-in account is new.
  bool is_new_profile = false;

  // The user's given name.
  std::optional<std::string_view> given_name;

  // True if the account should be under policy management.
  bool is_account_managed = false;
};

inline constexpr LoginInfo kRegularUserLoginInfo = {};

}  // namespace ash

#endif  // ASH_TEST_LOGIN_INFO_H_
