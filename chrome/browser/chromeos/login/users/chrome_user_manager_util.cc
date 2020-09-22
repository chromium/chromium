// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/users/chrome_user_manager_util.h"

#include "base/values.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_provider.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_names.h"
#include "components/user_manager/user_type.h"

namespace chromeos {
namespace chrome_user_manager_util {

bool AreAllUsersAllowed(const user_manager::UserList& users,
                        const enterprise_management::ChromeDeviceSettingsProto&
                            device_settings_proto) {
  PrefValueMap decoded_policies;
  DeviceSettingsProvider::DecodePolicies(device_settings_proto,
                                         &decoded_policies);

  bool supervised_users_allowed = false;
  decoded_policies.GetBoolean(kAccountsPrefSupervisedUsersEnabled,
                              &supervised_users_allowed);

  bool is_guest_allowed = false;
  decoded_policies.GetBoolean(kAccountsPrefAllowGuest, &is_guest_allowed);

  const base::Value* value;
  const base::ListValue* allowlist;
  if (decoded_policies.GetValue(kAccountsPrefUsers, &value)) {
    value->GetAsList(&allowlist);
  }

  bool allow_new_user = false;
  decoded_policies.GetBoolean(kAccountsPrefAllowNewUser, &allow_new_user);

  for (user_manager::User* user : users) {
    bool is_user_allowlisted =
        user->HasGaiaAccount() &&
        CrosSettings::FindEmailInList(
            allowlist, user->GetAccountId().GetUserEmail(), nullptr);
    if (!IsUserAllowed(
            *user, supervised_users_allowed, is_guest_allowed,
            user->HasGaiaAccount() && (allow_new_user || is_user_allowlisted)))
      return false;
  }
  return true;
}

bool IsUserAllowed(const user_manager::User& user,
                   bool supervised_users_allowed,
                   bool is_guest_allowed,
                   bool is_user_allowlisted) {
  DCHECK(user.GetType() == user_manager::USER_TYPE_REGULAR ||
         user.GetType() == user_manager::USER_TYPE_GUEST ||
         user.GetType() == user_manager::USER_TYPE_SUPERVISED ||
         user.GetType() == user_manager::USER_TYPE_CHILD);

  if (user.GetType() == user_manager::USER_TYPE_GUEST && !is_guest_allowed) {
    return false;
  }
  if (user.GetType() == user_manager::USER_TYPE_SUPERVISED &&
      !supervised_users_allowed) {
    return false;
  }
  if (user.HasGaiaAccount() && !is_user_allowlisted) {
    return false;
  }
  return true;
}

bool IsPublicSessionOrEphemeralLogin() {
  const user_manager::UserManager* user_manager =
      user_manager::UserManager::Get();
  return user_manager->IsLoggedInAsPublicAccount() ||
         (user_manager->IsCurrentUserNonCryptohomeDataEphemeral() &&
          user_manager->GetActiveUser()->GetType() !=
              user_manager::USER_TYPE_REGULAR);
}

}  // namespace chrome_user_manager_util
}  // namespace chromeos
