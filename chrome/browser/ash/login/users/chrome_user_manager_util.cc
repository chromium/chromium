// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/users/chrome_user_manager_util.h"

#include "base/values.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chrome/browser/ash/settings/device_settings_provider.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_names.h"
#include "components/user_manager/user_type.h"

namespace ash::chrome_user_manager_util {

bool AreAllUsersAllowed(const user_manager::UserList& users,
                        const enterprise_management::ChromeDeviceSettingsProto&
                            device_settings_proto) {
  PrefValueMap decoded_policies;
  DeviceSettingsProvider::DecodePolicies(device_settings_proto,
                                         &decoded_policies);

  bool is_guest_allowed = false;
  decoded_policies.GetBoolean(kAccountsPrefAllowGuest, &is_guest_allowed);

  const base::Value* allowlist = nullptr;
  decoded_policies.GetValue(kAccountsPrefUsers, &allowlist);
  DCHECK(allowlist);

  bool allow_family_link = false;
  decoded_policies.GetBoolean(kAccountsPrefFamilyLinkAccountsAllowed,
                              &allow_family_link);

  bool allow_new_user = false;
  decoded_policies.GetBoolean(kAccountsPrefAllowNewUser, &allow_new_user);

  for (user_manager::User* user : users) {
    const bool is_user_allowlisted =
        user->HasGaiaAccount() &&
        CrosSettings::FindEmailInList(
            allowlist->GetList(), user->GetAccountId().GetUserEmail(), nullptr);
    const bool is_allowed_because_family_link =
        allow_family_link && user->IsChild();
    const bool is_gaia_user_allowed =
        allow_new_user || is_user_allowlisted || is_allowed_because_family_link;
    if (!IsUserAllowed(*user, is_guest_allowed,
                       user->HasGaiaAccount() && is_gaia_user_allowed)) {
      return false;
    }
  }
  return true;
}

bool IsUserAllowed(const user_manager::User& user,
                   bool is_guest_allowed,
                   bool is_user_allowlisted) {
  DCHECK(user.GetType() == user_manager::USER_TYPE_REGULAR ||
         user.GetType() == user_manager::USER_TYPE_GUEST ||
         user.GetType() == user_manager::USER_TYPE_CHILD);

  if (user.GetType() == user_manager::USER_TYPE_GUEST && !is_guest_allowed) {
    return false;
  }
  if (user.HasGaiaAccount() && !is_user_allowlisted) {
    return false;
  }
  return true;
}

bool IsManagedGuestSessionOrEphemeralLogin() {
  const user_manager::UserManager* user_manager =
      user_manager::UserManager::Get();
  return user_manager->IsLoggedInAsManagedGuestSession() ||
         user_manager->IsCurrentUserCryptohomeDataEphemeral();
}

}  // namespace ash::chrome_user_manager_util
