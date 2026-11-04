// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/diagnostics/diagnostics_browser_delegate_impl.h"

#include <optional>

#include "base/files/file_path.h"
#include "components/session_manager/core/session.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"

namespace ash {
namespace diagnostics {

std::optional<base::FilePath>
DiagnosticsBrowserDelegateImpl::GetActiveUserProfileDir() {
  // TODO(crbug.com/563546692): Pass the AccountId in from the caller instead
  // of looking the active session up here.
  const session_manager::Session* session =
      session_manager::SessionManager::Get()->GetActiveSession();
  // Handle no user logged in.
  if (!session) {
    return std::nullopt;
  }

  const user_manager::User* user =
      user_manager::UserManager::Get()->FindUser(session->account_id());
  if (!user) {
    return std::nullopt;
  }

  // The path is unset until the user's cryptohome is mounted.
  return user->GetPath();
}

}  // namespace diagnostics
}  // namespace ash
