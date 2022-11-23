// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_USERS_CHROME_USER_MANAGER_UTIL_H_
#define CHROME_BROWSER_ASH_LOGIN_USERS_CHROME_USER_MANAGER_UTIL_H_

#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/user_manager/user.h"

namespace ash {
namespace chrome_user_manager_util {

// Returns true if all `users` are allowed depending on the provided device
// policies. Accepted user types: USER_TYPE_REGULAR, USER_TYPE_GUEST,
// USER_TYPE_CHILD.
// This function only checks against the device policies provided, so it does
// not depend on CrosSettings or any other policy store.
bool AreAllUsersAllowed(const user_manager::UserList& users,
                        const enterprise_management::ChromeDeviceSettingsProto&
                            device_settings_proto);

// Returns true if `user` is allowed, according to the given constraints.
// Accepted user types: USER_TYPE_REGULAR, USER_TYPE_GUEST,
// USER_TYPE_CHILD.
bool IsUserAllowed(const user_manager::User& user,
                   bool is_guest_allowed,
                   bool is_user_allowlisted);

// Returns whether the active user is public session user or non-regular
// ephemeral user. Note: it assumes the active user exists (ie. at least one
// user has logged in).
bool IsPublicSessionOrEphemeralLogin();

}  // namespace chrome_user_manager_util
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_USERS_CHROME_USER_MANAGER_UTIL_H_
