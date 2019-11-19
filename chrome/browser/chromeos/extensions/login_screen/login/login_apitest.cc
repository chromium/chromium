// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/login_screen/login/login_api.h"

#include <memory>
#include <string>

#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/extensions/login_screen/login_screen_apitest_base.h"
#include "chrome/browser/chromeos/login/test/local_policy_test_server_mixin.h"
#include "chrome/browser/chromeos/login/test/session_manager_state_waiter.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/session_manager_types.h"
#include "components/user_manager/user_type.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/api/test/test_api.h"
#include "extensions/test/extension_test_message_listener.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kAccountId[] = "public-session@test";
constexpr char kData[] = "some data";
constexpr char kLaunchManagedGuestSession[] = "LoginLaunchManagedGuestSession";
constexpr char kLaunchManagedGuestSessionNoAccounts[] =
    "LoginLaunchManagedGuestSessionNoAccounts";
constexpr char kExitCurrentSession[] = "LoginExitCurrentSession";
constexpr char kFetchDataForNextLoginAttempt[] =
    "LoginFetchDataForNextLoginAttempt";

}  // namespace

namespace chromeos {

class LoginApitest : public LoginScreenApitestBase {
 public:
  LoginApitest() : LoginScreenApitestBase(version_info::Channel::STABLE) {}
  ~LoginApitest() override = default;

  void SetUpDeviceLocalAccountPolicy() {
    enterprise_management::DeviceLocalAccountsProto* const
        device_local_accounts =
            device_policy()->payload().mutable_device_local_accounts();
    enterprise_management::DeviceLocalAccountInfoProto* const account =
        device_local_accounts->add_account();
    account->set_account_id(kAccountId);
    account->set_type(enterprise_management::DeviceLocalAccountInfoProto::
                          ACCOUNT_TYPE_PUBLIC_SESSION);
    RefreshDevicePolicy();
  }

  void SetTestCustomArg(const std::string custom_arg) {
    config_.SetKey("customArg", base::Value(custom_arg));
    extensions::TestGetConfigFunction::set_test_config_state(&config_);
  }

 private:
  chromeos::LocalPolicyTestServerMixin local_policy_mixin_{&mixin_host_};
  base::DictionaryValue config_;

  DISALLOW_COPY_AND_ASSIGN(LoginApitest);
};

IN_PROC_BROWSER_TEST_F(LoginApitest, LaunchManagedGuestSession) {
  SetUpDeviceLocalAccountPolicy();
  SetUpExtensionAndRunTest(kLaunchManagedGuestSession);
  SessionStateWaiter(session_manager::SessionState::ACTIVE).Wait();

  // Check that the active user is of type |USER_TYPE_PUBLIC_ACCOUNT|.
  // We cannot use the email as an identifier as a different email is generated
  // for managed guest sessions.
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  EXPECT_TRUE(user_manager->GetActiveUser()->GetType() ==
              user_manager::USER_TYPE_PUBLIC_ACCOUNT);
}

IN_PROC_BROWSER_TEST_F(LoginApitest, LaunchManagedGuestSessionNoAccounts) {
  SetUpExtensionAndRunTest(kLaunchManagedGuestSessionNoAccounts);
}

IN_PROC_BROWSER_TEST_F(LoginApitest, ExitCurrentSession) {
  SetUpDeviceLocalAccountPolicy();
  SetTestCustomArg(kData);
  content::WindowedNotificationObserver termination_waiter(
      chrome::NOTIFICATION_APP_TERMINATING,
      content::NotificationService::AllSources());

  SetUpExtensionAndRunTest(kExitCurrentSession, /*assert_test_succeed=*/false);
  termination_waiter.Wait();

  PrefService* local_state = g_browser_process->local_state();
  EXPECT_EQ(kData, local_state->GetString(
                       prefs::kLoginExtensionApiDataForNextLoginAttempt));
}

IN_PROC_BROWSER_TEST_F(LoginApitest, FetchDataForNextLoginAttempt) {
  SetTestCustomArg(kData);
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetString(prefs::kLoginExtensionApiDataForNextLoginAttempt,
                         kData);
  SetUpExtensionAndRunTest(kFetchDataForNextLoginAttempt);

  EXPECT_EQ("", local_state->GetString(
                    prefs::kLoginExtensionApiDataForNextLoginAttempt));
}

}  // namespace chromeos
