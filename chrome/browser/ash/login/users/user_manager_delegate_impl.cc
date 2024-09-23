// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/users/user_manager_delegate_impl.h"

#include <optional>

#include "base/check.h"
#include "base/check_is_test.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/ash/login/users/chrome_user_manager_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/dbus/cryptohome/UserDataAuth.pb.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "components/policy/core/common/device_local_account_type.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_type.h"
#include "content/public/common/content_switches.h"

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

std::optional<user_manager::UserType>
UserManagerDelegateImpl::GetDeviceLocalAccountUserType(std::string_view email) {
  auto type = policy::GetDeviceLocalAccountType(email);
  if (!type.has_value()) {
    NOTREACHED_IN_MIGRATION();
    return std::nullopt;
  }
  return chrome_user_manager_util::DeviceLocalAccountTypeToUserType(*type);
}

// If we don't have a mounted profile directory we're in trouble.
// TODO(davemoore): Once we have better api this check should ensure that
// our profile directory is the one that's mounted, and that it's mounted
// as the current user.
void UserManagerDelegateImpl::CheckProfileOnLogin(
    const user_manager::User& user) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          ::switches::kTestType)) {
    return;
  }

  UserDataAuthClient::Get()->IsMounted(
      user_data_auth::IsMountedRequest(),
      base::BindOnce([](std::optional<user_data_auth::IsMountedReply> result) {
        if (!result.has_value()) {
          LOG(ERROR) << "IsMounted call failed.";
          return;
        }

        LOG_IF(ERROR, !result->is_mounted()) << "Cryptohome is not mounted.";
      }));

  base::FilePath user_profile_dir =
      ash::BrowserContextHelper::Get()->GetBrowserContextPathByUserIdHash(
          user.username_hash());
  CHECK(
      !g_browser_process->profile_manager()->GetProfileByPath(user_profile_dir))
      << "The user profile was loaded before we mounted the cryptohome.";
}

void UserManagerDelegateImpl::RemoveProfileByAccountId(
    const AccountId& account_id) {
  g_browser_process->profile_manager()
      ->GetProfileAttributesStorage()
      .RemoveProfileByAccountId(account_id);
}

void UserManagerDelegateImpl::RemoveCryptohomeAsync(
    const AccountId& account_id) {
  cryptohome::AccountIdentifier identifier =
      cryptohome::CreateAccountIdentifierFromAccountId(account_id);
  mount_performer_.RemoveUserDirectoryByIdentifier(
      identifier, base::BindOnce(
                      [](const AccountId& account_id,
                         std::optional<AuthenticationError> error) {
                        if (error.has_value()) {
                          LOG(ERROR) << "Removal of cryptohome for "
                                     << account_id.Serialize()
                                     << " failed, return code: "
                                     << error->get_cryptohome_error();
                        }
                      },
                      account_id));
}

}  // namespace ash
