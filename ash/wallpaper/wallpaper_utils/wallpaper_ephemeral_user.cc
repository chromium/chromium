// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/wallpaper_utils/wallpaper_ephemeral_user.h"

#include "ash/public/cpp/session/session_types.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user_names.h"
#include "components/user_manager/user_type.h"

namespace ash {

// Returns true if the user's wallpaper is to be treated as ephemeral.
bool IsEphemeralUser(const AccountId& account_id) {
  if (account_id == user_manager::GuestAccountId()) {
    // Guest user should always be ephemeral.
    return true;
  }

  if (account_id == user_manager::StubAccountId()) {
    // Stub user is never ephemeral. Mirrors the logic in
    // UserManagerImpl::IsEphemeralAccountId.
    return false;
  }

  const UserSession* user_session =
      Shell::Get()->session_controller()->GetUserSessionByAccountId(account_id);
  if (!user_session) {
    // User is not logged in. Thus, they are not ephemeral.
    return false;
  }

  // Public account(e.g. demo mode) should always be ephemeral.
  return user_session->user_info.is_ephemeral ||
         user_session->user_info.type == user_manager::UserType::kPublicAccount;
}

}  // namespace ash
