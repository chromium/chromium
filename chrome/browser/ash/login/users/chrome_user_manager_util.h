// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_USERS_CHROME_USER_MANAGER_UTIL_H_
#define CHROME_BROWSER_ASH_LOGIN_USERS_CHROME_USER_MANAGER_UTIL_H_

#include <optional>

#include "components/policy/core/common/device_local_account_type.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_type.h"

namespace ash::chrome_user_manager_util {

// Returns true if all `users` are allowed depending on the provided device
// policies. Accepted user types: kRegular, kGuest, kChild.
// This function only checks against the device policies provided, so it does
// not depend on CrosSettings or any other policy store.
bool AreAllUsersAllowed(const user_manager::UserList& users,
                        const enterprise_management::ChromeDeviceSettingsProto&
                            device_settings_proto);

// Returns UserType corresponding to the given DeviceLocalAccountType.
std::optional<user_manager::UserType> DeviceLocalAccountTypeToUserType(
    policy::DeviceLocalAccountType device_local_account_type);

// Returns whether the active user is a managed guest session or non-regular
// ephemeral user. Note: it assumes the active user exists (ie. at least one
// user has logged in).
bool IsManagedGuestSessionOrEphemeralLogin();

}  // namespace ash::chrome_user_manager_util

#endif  // CHROME_BROWSER_ASH_LOGIN_USERS_CHROME_USER_MANAGER_UTIL_H_
