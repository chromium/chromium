// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/users/user_manager_delegate_impl.h"

#include "base/check_is_test.h"
#include "base/path_service.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/browser_process.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"

namespace ash {

UserManagerDelegateImpl::UserManagerDelegateImpl() = default;
UserManagerDelegateImpl::~UserManagerDelegateImpl() = default;

const std::string& UserManagerDelegateImpl::GetApplicationLocale() {
  return g_browser_process->GetApplicationLocale();
}

void UserManagerDelegateImpl::OverrideDirHome(
    const user_manager::User& primary_user) {
  // ash::BrowserContextHelper implicitly depends on ProfileManager,
  // so check its existence here. Maybe nullptr in tests.
  if (!g_browser_process->profile_manager()) {
    CHECK_IS_TEST();
    return;
  }

  base::FilePath homedir =
      ash::BrowserContextHelper::Get()->GetBrowserContextPathByUserIdHash(
          primary_user.username_hash());
  // This path has been either created by cryptohome (on real Chrome OS
  // device) or by ProfileManager (on chromeos=1 desktop builds).
  base::PathService::OverrideAndCreateIfNeeded(base::DIR_HOME, homedir,
                                               /*is_absolute=*/true,
                                               /*create=*/false);
}

bool UserManagerDelegateImpl::IsUserSessionRestoreInProgress() {
  return UserSessionManager::GetInstance()->UserSessionsRestoreInProgress();
}

}  // namespace ash
