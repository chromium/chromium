// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/diagnostics/diagnostics_browser_delegate_impl.h"

#include "base/files/file_path.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "components/user_manager/user_manager.h"

namespace ash {
namespace diagnostics {

base::FilePath DiagnosticsBrowserDelegateImpl::GetActiveUserProfileDir() {
  // Handle no user logged in.
  if (!user_manager::UserManager::IsInitialized() ||
      !user_manager::UserManager::Get()->IsUserLoggedIn()) {
    return base::FilePath();
  }

  auto* user = user_manager::UserManager::Get()->GetActiveUser();
  DCHECK(user);
  auto* profile = ProfileHelper::Get()->GetProfileByUser(user);

  // Profile may be null if called before profile load is complete.
  if (profile == nullptr) {
    return base::FilePath();
  }

  return profile->GetPath();
}

}  // namespace diagnostics
}  // namespace ash
