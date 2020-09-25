// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/base_paths.h"
#include "base/environment.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/authpolicy/kerberos_files_handler.h"
#include "chrome/browser/chromeos/login/test/active_directory_login_mixin.h"
#include "chrome/browser/chromeos/login/test/device_state_mixin.h"
#include "chrome/browser/chromeos/login/test/oobe_base_test.h"
#include "chrome/browser/chromeos/login/test/oobe_screens_utils.h"
#include "chrome/browser/chromeos/login/test/session_manager_state_waiter.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chromeos/dbus/authpolicy/fake_authpolicy_client.h"
#include "components/user_manager/user_names.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/common/network_service_util.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/sync_call_restrictions.h"
#include "services/network/public/mojom/network_service_test.mojom.h"

namespace chromeos {

namespace {

constexpr char kTestActiveDirectoryUser[] = "test-user";
constexpr char kTestUserRealm[] = "user.realm";
constexpr char kPassword[] = "password";
constexpr char kNewPassword[] = "new_password";
constexpr char kDifferentNewPassword[] = "different_new_password";

void AssertNetworkServiceEnvEquals(const std::string& name,
                                   const std::string& expected_value) {
  std::string value;
  if (content::IsOutOfProcessNetworkService()) {
    mojo::Remote<network::mojom::NetworkServiceTest> network_service_test;
    content::GetNetworkService()->BindTestInterface(
        network_service_test.BindNewPipeAndPassReceiver());
    mojo::ScopedAllowSyncCallForTesting allow_sync_call;
    network_service_test->GetEnvironmentVariableValue(name, &value);
  } else {
    // If the network service is running in-process, we can read the
    // environment variable directly.
    base::Environment::Create()->GetVar(name, &value);
  }
  EXPECT_EQ(value, expected_value);
}

class ActiveDirectoryLoginTest : public OobeBaseTest {
 public:
  ActiveDirectoryLoginTest()
      : OobeBaseTest(),
        // Using the same realm as supervised user domain. Should be treated
        // as normal realm.
        test_realm_(user_manager::kSupervisedUserDomain),
        test_user_(kTestActiveDirectoryUser + ("@" + test_realm_)) {}

  ~ActiveDirectoryLoginTest() override = default;

 protected:
  FakeAuthPolicyClient* fake_authpolicy_client() {
    return FakeAuthPolicyClient::Get();
  }

  const std::string test_realm_;
  const std::string test_user_;
  DeviceStateMixin device_state_{
      &mixin_host_,
      DeviceStateMixin::State::OOBE_COMPLETED_ACTIVE_DIRECTORY_ENROLLED};
  ActiveDirectoryLoginMixin ad_login_{&mixin_host_};

 private:
  DISALLOW_COPY_AND_ASSIGN(ActiveDirectoryLoginTest);
};

class ActiveDirectoryLoginAutocompleteTest : public ActiveDirectoryLoginTest {
 public:
  ActiveDirectoryLoginAutocompleteTest() = default;
  void SetUpInProcessBrowserTestFixture() override {
    ActiveDirectoryLoginTest::SetUpInProcessBrowserTestFixture();

    enterprise_management::ChromeDeviceSettingsProto device_settings;
    device_settings.mutable_login_screen_domain_auto_complete()
        ->set_login_screen_domain_auto_complete(kTestUserRealm);
    fake_authpolicy_client()->set_device_policy(device_settings);
    autocomplete_realm_ = "@" + std::string(kTestUserRealm);
    ad_login_.set_autocomplete_realm(autocomplete_realm_);
  }

  std::string autocomplete_realm_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ActiveDirectoryLoginAutocompleteTest);
};

}  // namespace

// Test successful Active Directory login.
IN_PROC_BROWSER_TEST_F(ActiveDirectoryLoginTest, LoginSuccess) {
  ASSERT_TRUE(InstallAttributes::Get()->IsActiveDirectoryManaged());
  ad_login_.TestNoError();
  ad_login_.TestDomainHidden();
  ad_login_.SubmitActiveDirectoryCredentials(test_user_, kPassword);
  test::WaitForPrimaryUserSessionStart();
}

// Tests that the Kerberos SSO environment variables are set correctly after
// an Active Directory log in.
IN_PROC_BROWSER_TEST_F(ActiveDirectoryLoginTest, KerberosVarsCopied) {
  ad_login_.TestNoError();
  ad_login_.TestDomainHidden();
  ad_login_.SubmitActiveDirectoryCredentials(test_user_, kPassword);
  test::WaitForPrimaryUserSessionStart();

  base::FilePath dir;
  base::PathService::Get(base::DIR_HOME, &dir);
  dir = dir.Append(kKrb5Directory);
  std::string expected_krb5cc_value =
      kKrb5CCFilePrefix + dir.Append(kKrb5CCFile).value();
  AssertNetworkServiceEnvEquals(kKrb5CCEnvName, expected_krb5cc_value);
  std::string expected_krb5_config_value = dir.Append(kKrb5ConfFile).value();
  AssertNetworkServiceEnvEquals(kKrb5ConfEnvName, expected_krb5_config_value);
}

// Test different UI errors for Active Directory login.
IN_PROC_BROWSER_TEST_F(ActiveDirectoryLoginTest, LoginErrors) {
  ASSERT_TRUE(InstallAttributes::Get()->IsActiveDirectoryManaged());
  ad_login_.TestNoError();
  ad_login_.TestDomainHidden();

  ad_login_.SubmitActiveDirectoryCredentials("", "");
  ad_login_.TestUserError();
  ad_login_.TestDomainHidden();

  ad_login_.SubmitActiveDirectoryCredentials(test_user_, "");
  ad_login_.TestPasswordError();
  ad_login_.TestDomainHidden();

  ad_login_.SubmitActiveDirectoryCredentials(
      std::string(kTestActiveDirectoryUser) + "@", kPassword);
  ad_login_.WaitForAuthError();
  ad_login_.TestUserError();
  ad_login_.TestDomainHidden();

  fake_authpolicy_client()->set_auth_error(authpolicy::ERROR_BAD_USER_NAME);
  ad_login_.SubmitActiveDirectoryCredentials(test_user_, kPassword);
  ad_login_.WaitForAuthError();
  ad_login_.TestUserError();
  ad_login_.TestDomainHidden();

  fake_authpolicy_client()->set_auth_error(authpolicy::ERROR_BAD_PASSWORD);
  ad_login_.SubmitActiveDirectoryCredentials(test_user_, kPassword);
  ad_login_.WaitForAuthError();
  ad_login_.TestPasswordError();
  ad_login_.TestDomainHidden();

  fake_authpolicy_client()->set_auth_error(authpolicy::ERROR_UNKNOWN);
  ad_login_.SubmitActiveDirectoryCredentials(test_user_, kPassword);
  ad_login_.WaitForAuthError();
  // Inputs are not invalidated for the unknown error.
  ad_login_.TestNoError();
  ad_login_.TestDomainHidden();
}

// Test successful Active Directory login from the password change screen.
IN_PROC_BROWSER_TEST_F(ActiveDirectoryLoginTest, PasswordChange_LoginSuccess) {
  ASSERT_TRUE(InstallAttributes::Get()->IsActiveDirectoryManaged());
  ad_login_.TestLoginVisible();
  ad_login_.TestDomainHidden();

  ad_login_.TriggerPasswordChangeScreen();

  // Password accepted by AuthPolicyClient.
  fake_authpolicy_client()->set_auth_error(authpolicy::ERROR_NONE);
  ad_login_.SubmitActiveDirectoryPasswordChangeCredentials(
      kPassword, kNewPassword, kNewPassword);
  test::WaitForPrimaryUserSessionStart();
}

// Test different UI errors for Active Directory password change screen.
IN_PROC_BROWSER_TEST_F(ActiveDirectoryLoginTest, PasswordChange_UIErrors) {
  ASSERT_TRUE(InstallAttributes::Get()->IsActiveDirectoryManaged());
  ad_login_.TestLoginVisible();
  ad_login_.TestDomainHidden();

  ad_login_.TriggerPasswordChangeScreen();
  // Password rejected by UX.
  // Empty passwords.
  ad_login_.SubmitActiveDirectoryPasswordChangeCredentials("", "", "");
  ad_login_.TestPasswordChangeOldPasswordError();

  // Empty new password.
  ad_login_.SubmitActiveDirectoryPasswordChangeCredentials(kPassword, "", "");
  ad_login_.TestPasswordChangeNewPasswordError();

  // Empty confirmation of the new password.
  ad_login_.SubmitActiveDirectoryPasswordChangeCredentials(kPassword,
                                                           kNewPassword, "");
  ad_login_.TestPasswordChangeConfirmNewPasswordError();

  // Confirmation of password is different from new password.
  ad_login_.SubmitActiveDirectoryPasswordChangeCredentials(
      kPassword, kNewPassword, kDifferentNewPassword);
  ad_login_.TestPasswordChangeConfirmNewPasswordError();

  // Password rejected by AuthPolicyClient.
  fake_authpolicy_client()->set_auth_error(authpolicy::ERROR_BAD_PASSWORD);
  ad_login_.SubmitActiveDirectoryPasswordChangeCredentials(
      kPassword, kNewPassword, kNewPassword);
  ad_login_.TestPasswordChangeOldPasswordError();
}

// Test reopening Active Directory password change screen clears errors.
IN_PROC_BROWSER_TEST_F(ActiveDirectoryLoginTest,
                       PasswordChange_ReopenClearErrors) {
  ASSERT_TRUE(InstallAttributes::Get()->IsActiveDirectoryManaged());
  ad_login_.TestLoginVisible();
  ad_login_.TestDomainHidden();

  ad_login_.TriggerPasswordChangeScreen();

  // Empty new password.
  ad_login_.SubmitActiveDirectoryPasswordChangeCredentials("", "", "");
  ad_login_.TestPasswordChangeOldPasswordError();

  ad_login_.ClosePasswordChangeScreen();
  ad_login_.TestLoginVisible();
  ad_login_.TriggerPasswordChangeScreen();
  ad_login_.TestPasswordChangeNoErrors();
}

// Tests that autocomplete works. Submits username without domain.
IN_PROC_BROWSER_TEST_F(ActiveDirectoryLoginAutocompleteTest, LoginSuccess) {
  ASSERT_TRUE(InstallAttributes::Get()->IsActiveDirectoryManaged());
  ad_login_.TestNoError();
  ad_login_.TestDomainVisible();

  ad_login_.SubmitActiveDirectoryCredentials(kTestActiveDirectoryUser,
                                             kPassword);
  test::WaitForPrimaryUserSessionStart();
}

// Tests that user could override autocomplete domain.
IN_PROC_BROWSER_TEST_F(ActiveDirectoryLoginAutocompleteTest, TestAutocomplete) {
  ASSERT_TRUE(InstallAttributes::Get()->IsActiveDirectoryManaged());

  ad_login_.TestLoginVisible();
  ad_login_.TestDomainVisible();
  fake_authpolicy_client()->set_auth_error(authpolicy::ERROR_BAD_PASSWORD);

  // Submit with a different domain.
  ad_login_.SetUserInput(test_user_);
  ad_login_.TestDomainHidden();
  ad_login_.TestUserInput(test_user_);
  ad_login_.SubmitActiveDirectoryCredentials(test_user_, "password");
  ad_login_.WaitForAuthError();
  ad_login_.TestLoginVisible();
  ad_login_.TestDomainHidden();
  ad_login_.TestUserInput(test_user_);

  // Set userinput with the autocomplete domain. JS will remove the autocomplete
  // domain.
  ad_login_.SetUserInput(kTestActiveDirectoryUser + autocomplete_realm_);
  ad_login_.TestDomainVisible();
  ad_login_.TestUserInput(kTestActiveDirectoryUser);
  ad_login_.SubmitActiveDirectoryCredentials(
      kTestActiveDirectoryUser + autocomplete_realm_, "password");
  ad_login_.WaitForAuthError();
  ad_login_.TestLoginVisible();
  ad_login_.TestDomainVisible();
  ad_login_.TestUserInput(kTestActiveDirectoryUser);
}

}  // namespace chromeos
