// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_APP_MODE_TEST_MANAGED_GUEST_SESSION_TEST_HELPERS_H_
#define CHROME_BROWSER_ASH_LOGIN_APP_MODE_TEST_MANAGED_GUEST_SESSION_TEST_HELPERS_H_

#include "components/policy/proto/chrome_device_policy.pb.h"

namespace ash {

inline constexpr char kManagedGuestSessionAccountId[] =
    "managed-guest-session@localhost";

// Add a MGS account to auto launch policy.
void AppendAutoLaunchManagedGuestSessionAccount(
    enterprise_management::ChromeDeviceSettingsProto* policy_payload);

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_APP_MODE_TEST_MANAGED_GUEST_SESSION_TEST_HELPERS_H_
