// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_APP_MODE_TEST_MANAGED_GUEST_SESSION_TEST_HELPERS_H_
#define CHROME_BROWSER_ASH_LOGIN_APP_MODE_TEST_MANAGED_GUEST_SESSION_TEST_HELPERS_H_

#include "components/account_id/account_id.h"
#include "components/policy/proto/chrome_device_policy.pb.h"

namespace ash::test {

// Add a MGS account to auto launch policy.
void AppendAutoLaunchManagedGuestSessionAccount(
    enterprise_management::ChromeDeviceSettingsProto* policy_payload);

// Launches MGS for the given `account_id`, and returns true if the user session
// has already started successfully.
bool LaunchManagedGuestSession(const AccountId& account_id);

// Waits for MGS launch and returns true if it succeeded.
[[nodiscard]] bool WaitForManagedGuestSessionLaunch();

}  // namespace ash::test

#endif  // CHROME_BROWSER_ASH_LOGIN_APP_MODE_TEST_MANAGED_GUEST_SESSION_TEST_HELPERS_H_
