// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/app_mode/test/managed_guest_session_test_helpers.h"

namespace ash {

void AppendAutoLaunchManagedGuestSessionAccount(
    enterprise_management::ChromeDeviceSettingsProto* policy_payload) {
  enterprise_management::DeviceLocalAccountsProto* const device_local_accounts =
      policy_payload->mutable_device_local_accounts();

  enterprise_management::DeviceLocalAccountInfoProto* const account =
      device_local_accounts->add_account();
  account->set_account_id(kManagedGuestSessionAccountId);
  account->set_type(enterprise_management::DeviceLocalAccountInfoProto::
                        ACCOUNT_TYPE_PUBLIC_SESSION);
  device_local_accounts->set_auto_login_id(kManagedGuestSessionAccountId);
  device_local_accounts->set_auto_login_delay(0);
}

}  // namespace ash
