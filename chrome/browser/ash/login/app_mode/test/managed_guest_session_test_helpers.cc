// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/app_mode/test/managed_guest_session_test_helpers.h"

#include <string_view>

#include "base/check_deref.h"
#include "chrome/browser/ash/login/existing_user_controller.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "components/account_id/account_id.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/user_manager/user_manager.h"

namespace ash::test {

static constexpr std::string_view kManagedGuestSessionAccountId{
    "managed-guest-session@localhost"};

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

bool LaunchManagedGuestSession(const AccountId& account_id) {
  // Start login into the device-local account.
  CHECK_DEREF(ash::LoginDisplayHost::default_host()).StartSignInScreen();

  ash::UserContext user_context(user_manager::UserType::kPublicAccount,
                                account_id);

  CHECK_DEREF(ash::ExistingUserController::current_controller())
      .Login(user_context, ash::SigninSpecifics());

  return session_manager::SessionManager::Get()->IsSessionStarted();
}

bool WaitForManagedGuestSessionLaunch() {
  ash::test::WaitForPrimaryUserSessionStart();
  return user_manager::UserManager::Get()->IsLoggedInAsManagedGuestSession();
}

}  // namespace ash::test
