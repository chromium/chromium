// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/active_directory_test_helper.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/test/js_checker.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_auth_policy_client.h"
#include "chromeos/dbus/fake_cryptohome_client.h"
#include "chromeos/dbus/util/tpm_util.h"
#include "chromeos/login/auth/authpolicy_login_helper.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user_names.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/test/rect_test_util.h"

using ::gfx::test::RectContains;

namespace chromeos {
namespace {

const char kPassword[] = "password";

constexpr char kAdOfflineAuthId[] = "offline-ad-auth";

constexpr char kTestActiveDirectoryUser[] = "test-user";
constexpr char kTestUserRealm[] = "user.realm";
constexpr char kAdMachineInput[] = "$.machineNameInput";
constexpr char kAdMoreOptionsButton[] = "$.moreOptionsBtn";
constexpr char kAdUserInput[] = "$.userInput";
constexpr char kAdPasswordInput[] = "$.passwordInput";
constexpr char kAdCredsButton[] = "$$('#nextButton')";
constexpr char kAdPasswordChangeButton[] = "$.inputForm.$.button";
constexpr char kAdAutocompleteRealm[] = "$.userInput.querySelector('span')";

constexpr char kAdPasswordChangeId[] = "active-directory-password-change";
constexpr char kAdAnimatedPages[] = "$.animatedPages";
constexpr char kAdOldPasswordInput[] = "$.oldPassword";
constexpr char kAdNewPassword1Input[] = "$.newPassword1";
constexpr char kAdNewPassword2Input[] = "$.newPassword2";
constexpr char kNewPassword[] = "new_password";
constexpr char kDifferentNewPassword[] = "different_new_password";

constexpr char kCloseButtonId[] = "$.navigation.$.closeButton";

class ActiveDirectoryLoginTest : public LoginManagerTest {
 public:
  ActiveDirectoryLoginTest()
      : LoginManagerTest(true, true),
        // Using the same realm as supervised user domain. Should be treated as
        // normal realm.
        test_realm_(user_manager::kSupervisedUserDomain),
        test_user_(kTestActiveDirectoryUser + ("@" + test_realm_)) {}

  ~ActiveDirectoryLoginTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    LoginManagerTest::SetUpInProcessBrowserTestFixture();

    auto fake_client = std::make_unique<FakeAuthPolicyClient>();
    fake_auth_policy_client_ = fake_client.get();
    fake_auth_policy_client_->DisableOperationDelayForTesting();
    DBusThreadManager::GetSetterForTesting()->SetAuthPolicyClient(
        std::move(fake_client));

    // Note: FakeCryptohomeClient needs paths to be set to load install attribs.
    active_directory_test_helper::OverridePaths();
    DBusThreadManager::GetSetterForTesting()->SetCryptohomeClient(
        std::make_unique<FakeCryptohomeClient>());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    LoginManagerTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kOobeSkipPostLogin);
  }

  void SetUpOnMainThread() override {
    // Set the threshold to a max value to disable the offline message screen
    // on slow configurations like MSAN, where it otherwise triggers on every
    // run.
    LoginDisplayHost::default_host()
        ->GetOobeUI()
        ->signin_screen_handler()
        ->SetOfflineTimeoutForTesting(base::TimeDelta::Max());
    LoginManagerTest::SetUpOnMainThread();
  }

  void MarkAsActiveDirectoryEnterprise() {
    StartupUtils::MarkOobeCompleted();
    active_directory_test_helper::PrepareLogin(test_user_);
  }

  void TriggerPasswordChangeScreen() {
    OobeScreenWaiter screen_waiter(
        OobeScreen::SCREEN_ACTIVE_DIRECTORY_PASSWORD_CHANGE);

    fake_auth_policy_client()->set_auth_error(
        authpolicy::ERROR_PASSWORD_EXPIRED);
    SubmitActiveDirectoryCredentials(test_user_, kPassword);
    screen_waiter.Wait();
    TestAdPasswordChangeError(std::string());
  }

  void ClosePasswordChangeScreen() {
    js_checker().Evaluate(JSElement(kAdPasswordChangeId, kCloseButtonId) +
                          ".click()");
  }

  // Checks if Active Directory login is visible.
  void TestLoginVisible() {
    OobeScreenWaiter screen_waiter(OobeScreen::SCREEN_GAIA_SIGNIN);
    screen_waiter.Wait();
    // Checks if Gaia signin is hidden.
    JSExpect("document.querySelector('#signin-frame').hidden");

    // Checks if Active Directory signin is visible.
    JSExpect("!document.querySelector('#offline-ad-auth').hidden");
    JSExpect(JSElement(kAdOfflineAuthId, kAdMachineInput) + ".hidden");
    JSExpect(JSElement(kAdOfflineAuthId, kAdMoreOptionsButton) + ".hidden");
    JSExpect("!" + JSElement(kAdOfflineAuthId, kAdUserInput) + ".hidden");
    JSExpect("!" + JSElement(kAdOfflineAuthId, kAdPasswordInput) + ".hidden");

    std::string autocomplete_domain_ui;
    base::TrimString(
        js_checker().GetString(
            JSElement(kAdOfflineAuthId, kAdAutocompleteRealm) + ".innerText"),
        base::kWhitespaceASCII, &autocomplete_domain_ui);
    // Checks if realm is set to autocomplete username.
    EXPECT_EQ(autocomplete_realm_, autocomplete_domain_ui);

    // Checks if bottom bar is visible.
    JSExpect("!Oobe.getInstance().headerHidden");
  }

  // Checks if Active Directory password change screen is shown.
  void TestPasswordChangeVisible() {
    // Checks if Gaia signin is hidden.
    JSExpect("document.querySelector('#signin-frame').hidden");
    // Checks if Active Directory signin is visible.
    JSExpect("!document.querySelector('#ad-password-change').hidden");
    JSExpect(JSElement(kAdPasswordChangeId, kAdAnimatedPages) +
             ".selected == 0");
    JSExpect("!" + JSElement(kAdPasswordChangeId, kCloseButtonId) + ".hidden");
  }

  // Checks if user input is marked as invalid.
  void TestUserError() {
    TestLoginVisible();
    JSExpect(JSElement(kAdOfflineAuthId, kAdUserInput) + ".invalid");
  }

  void SetUserInput(const std::string& value) {
    js_checker().ExecuteAsync(JSElement(kAdOfflineAuthId, kAdUserInput) +
                              ".value='" + value + "'");
  }

  void TestUserInput(const std::string& value) {
    js_checker().ExpectEQ(JSElement(kAdOfflineAuthId, kAdUserInput) + ".value",
                          value);
  }

  // Checks if password input is marked as invalid.
  void TestPasswordError() {
    TestLoginVisible();
    JSExpect(JSElement(kAdOfflineAuthId, kAdPasswordInput) + ".invalid");
  }

  // Checks that machine, password and user inputs are valid.
  void TestNoError() {
    TestLoginVisible();
    JSExpect("!" + JSElement(kAdOfflineAuthId, kAdMachineInput) + ".invalid");
    JSExpect("!" + JSElement(kAdOfflineAuthId, kAdUserInput) + ".invalid");
    JSExpect("!" + JSElement(kAdOfflineAuthId, kAdPasswordInput) + ".invalid");
  }

  // Checks if autocomplete domain is visible for the user input.
  void TestDomainVisible() {
    JSExpect("!" + JSElement(kAdOfflineAuthId, kAdAutocompleteRealm) +
             ".hidden");
  }

  // Checks if autocomplete domain is hidden for the user input.
  void TestDomainHidden() {
    JSExpect(JSElement(kAdOfflineAuthId, kAdAutocompleteRealm) + ".hidden");
  }

  // Checks if Active Directory password change screen is shown. Also checks if
  // |invalid_element| is invalidated and all the other elements are valid.
  void TestAdPasswordChangeError(const std::string& invalid_element) {
    TestPasswordChangeVisible();
    for (const char* element :
         {kAdOldPasswordInput, kAdNewPassword1Input, kAdNewPassword2Input}) {
      std::string js_assertion =
          JSElement(kAdPasswordChangeId, element) + ".isInvalid";
      if (element != invalid_element)
        js_assertion = "!" + js_assertion;
      JSExpect(js_assertion);
    }
  }

  // Sets username and password for the Active Directory login and submits it.
  void SubmitActiveDirectoryCredentials(const std::string& username,
                                        const std::string& password) {
    js_checker().ExecuteAsync(JSElement(kAdOfflineAuthId, kAdUserInput) +
                              ".value='" + username + "'");
    js_checker().ExecuteAsync(JSElement(kAdOfflineAuthId, kAdPasswordInput) +
                              ".value='" + password + "'");
    js_checker().Evaluate(JSElement(kAdOfflineAuthId, kAdCredsButton) +
                          ".click()");
  }

  // Sets username and password for the Active Directory login and submits it.
  void SubmitActiveDirectoryPasswordChangeCredentials(
      const std::string& old_password,
      const std::string& new_password1,
      const std::string& new_password2) {
    js_checker().ExecuteAsync(
        JSElement(kAdPasswordChangeId, kAdOldPasswordInput) + ".value='" +
        old_password + "'");
    js_checker().ExecuteAsync(
        JSElement(kAdPasswordChangeId, kAdNewPassword1Input) + ".value='" +
        new_password1 + "'");
    js_checker().ExecuteAsync(
        JSElement(kAdPasswordChangeId, kAdNewPassword2Input) + ".value='" +
        new_password2 + "'");
    js_checker().Evaluate(
        JSElement(kAdPasswordChangeId, kAdPasswordChangeButton) + ".click()");
  }

  void SetupActiveDirectoryJSNotifications() {
    js_checker().Evaluate(
        "var testInvalidateAd = login.GaiaSigninScreen.invalidateAd;"
        "login.GaiaSigninScreen.invalidateAd = function(user, errorState) {"
        "  testInvalidateAd(user, errorState);"
        "  window.domAutomationController.send('ShowAuthError');"
        "}");
  }

  void WaitForMessage(content::DOMMessageQueue* message_queue,
                      const std::string& expected_message) {
    std::string message;
    do {
      ASSERT_TRUE(message_queue->WaitForMessage(&message));
    } while (message != expected_message);
  }

 protected:
  // Returns string representing element with id=|element_id| inside Active
  // Directory login element.
  std::string JSElement(const std::string& parent_id,
                        const std::string& selector) {
    return "document.querySelector('#" + parent_id + "')." + selector;
  }
  FakeAuthPolicyClient* fake_auth_policy_client() {
    return fake_auth_policy_client_;
  }

  const std::string test_realm_;
  const std::string test_user_;
  std::string autocomplete_realm_;

 private:
  FakeAuthPolicyClient* fake_auth_policy_client_;

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
    fake_auth_policy_client()->set_device_policy(device_settings);
    autocomplete_realm_ = "@" + std::string(kTestUserRealm);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ActiveDirectoryLoginAutocompleteTest);
};

}  // namespace

// Declares a PRE_ test that calls MarkAsActiveDirectoryEnterprise() and the
// test itself.
#define IN_PROC_BROWSER_TEST_F_WITH_PRE(class_name, test_name) \
  IN_PROC_BROWSER_TEST_F(class_name, PRE_##test_name) {        \
    MarkAsActiveDirectoryEnterprise();                         \
  }                                                            \
  IN_PROC_BROWSER_TEST_F(class_name, test_name)

// Test successful Active Directory login.
IN_PROC_BROWSER_TEST_F_WITH_PRE(ActiveDirectoryLoginTest, LoginSuccess) {
  TestNoError();
  TestDomainHidden();

  content::WindowedNotificationObserver session_start_waiter(
      chrome::NOTIFICATION_SESSION_STARTED,
      content::NotificationService::AllSources());
  SubmitActiveDirectoryCredentials(test_user_, kPassword);
  session_start_waiter.Wait();
}

// Test different UI errors for Active Directory login.
IN_PROC_BROWSER_TEST_F_WITH_PRE(ActiveDirectoryLoginTest, LoginErrors) {
  SetupActiveDirectoryJSNotifications();
  TestNoError();
  TestDomainHidden();

  content::DOMMessageQueue message_queue;

  SubmitActiveDirectoryCredentials("", "");
  TestUserError();
  TestDomainHidden();

  SubmitActiveDirectoryCredentials(test_user_, "");
  TestPasswordError();
  TestDomainHidden();

  SubmitActiveDirectoryCredentials(std::string(kTestActiveDirectoryUser) + "@",
                                   kPassword);
  WaitForMessage(&message_queue, "\"ShowAuthError\"");
  TestUserError();
  TestDomainHidden();

  fake_auth_policy_client()->set_auth_error(authpolicy::ERROR_BAD_USER_NAME);
  SubmitActiveDirectoryCredentials(test_user_, kPassword);
  WaitForMessage(&message_queue, "\"ShowAuthError\"");
  TestUserError();
  TestDomainHidden();

  fake_auth_policy_client()->set_auth_error(authpolicy::ERROR_BAD_PASSWORD);
  SubmitActiveDirectoryCredentials(test_user_, kPassword);
  WaitForMessage(&message_queue, "\"ShowAuthError\"");
  TestPasswordError();
  TestDomainHidden();

  fake_auth_policy_client()->set_auth_error(authpolicy::ERROR_UNKNOWN);
  SubmitActiveDirectoryCredentials(test_user_, kPassword);
  WaitForMessage(&message_queue, "\"ShowAuthError\"");
  // Inputs are not invalidated for the unknown error.
  TestNoError();
  TestDomainHidden();
}

// Test successful Active Directory login from the password change screen.
IN_PROC_BROWSER_TEST_F_WITH_PRE(ActiveDirectoryLoginTest,
                                PasswordChange_LoginSuccess) {
  TestLoginVisible();
  TestDomainHidden();

  TriggerPasswordChangeScreen();

  // Password accepted by AuthPolicyClient.
  fake_auth_policy_client()->set_auth_error(authpolicy::ERROR_NONE);
  content::WindowedNotificationObserver session_start_waiter(
      chrome::NOTIFICATION_SESSION_STARTED,
      content::NotificationService::AllSources());
  SubmitActiveDirectoryPasswordChangeCredentials(kPassword, kNewPassword,
                                                 kNewPassword);
  session_start_waiter.Wait();
}

// Test different UI errors for Active Directory password change screen.
IN_PROC_BROWSER_TEST_F_WITH_PRE(ActiveDirectoryLoginTest,
                                PasswordChange_UIErrors) {
  TestLoginVisible();
  TestDomainHidden();

  TriggerPasswordChangeScreen();
  // Password rejected by UX.
  // Empty passwords.
  SubmitActiveDirectoryPasswordChangeCredentials("", "", "");
  TestAdPasswordChangeError(kAdOldPasswordInput);

  // Empty new password.
  SubmitActiveDirectoryPasswordChangeCredentials(kPassword, "", "");
  TestAdPasswordChangeError(kAdNewPassword1Input);

  // Empty confirmation of the new password.
  SubmitActiveDirectoryPasswordChangeCredentials(kPassword, kNewPassword, "");
  TestAdPasswordChangeError(kAdNewPassword2Input);

  // Confirmation of password is different from new password.
  SubmitActiveDirectoryPasswordChangeCredentials(kPassword, kNewPassword,
                                                 kDifferentNewPassword);
  TestAdPasswordChangeError(kAdNewPassword2Input);

  // Password rejected by AuthPolicyClient.
  fake_auth_policy_client()->set_auth_error(authpolicy::ERROR_BAD_PASSWORD);
  SubmitActiveDirectoryPasswordChangeCredentials(kPassword, kNewPassword,
                                                 kNewPassword);
  TestAdPasswordChangeError(kAdOldPasswordInput);
}

// Test reopening Active Directory password change screen clears errors.
IN_PROC_BROWSER_TEST_F_WITH_PRE(ActiveDirectoryLoginTest,
                                PasswordChange_ReopenClearErrors) {
  TestLoginVisible();
  TestDomainHidden();

  TriggerPasswordChangeScreen();

  // Empty new password.
  SubmitActiveDirectoryPasswordChangeCredentials("", "", "");
  TestAdPasswordChangeError(kAdOldPasswordInput);

  ClosePasswordChangeScreen();
  TestLoginVisible();
  TriggerPasswordChangeScreen();
}

// Tests that autocomplete works. Submits username without domain.
IN_PROC_BROWSER_TEST_F_WITH_PRE(ActiveDirectoryLoginAutocompleteTest,
                                LoginSuccess) {
  TestNoError();
  TestDomainVisible();

  content::WindowedNotificationObserver session_start_waiter(
      chrome::NOTIFICATION_SESSION_STARTED,
      content::NotificationService::AllSources());
  SubmitActiveDirectoryCredentials(kTestActiveDirectoryUser, kPassword);
  session_start_waiter.Wait();
}

// Tests that user could override autocomplete domain.
IN_PROC_BROWSER_TEST_F_WITH_PRE(ActiveDirectoryLoginAutocompleteTest,
                                TestAutocomplete) {
  SetupActiveDirectoryJSNotifications();

  TestLoginVisible();
  TestDomainVisible();
  fake_auth_policy_client()->set_auth_error(authpolicy::ERROR_BAD_PASSWORD);
  content::DOMMessageQueue message_queue;

  // Submit with a different domain.
  SetUserInput(test_user_);
  TestDomainHidden();
  TestUserInput(test_user_);
  SubmitActiveDirectoryCredentials(test_user_, "password");
  WaitForMessage(&message_queue, "\"ShowAuthError\"");
  TestLoginVisible();
  TestDomainHidden();
  TestUserInput(test_user_);

  // Set userinput with the autocomplete domain. JS will remove the autocomplete
  // domain.
  SetUserInput(kTestActiveDirectoryUser + autocomplete_realm_);
  TestDomainVisible();
  TestUserInput(kTestActiveDirectoryUser);
  SubmitActiveDirectoryCredentials(
      kTestActiveDirectoryUser + autocomplete_realm_, "password");
  WaitForMessage(&message_queue, "\"ShowAuthError\"");
  TestLoginVisible();
  TestDomainVisible();
  TestUserInput(kTestActiveDirectoryUser);
}

#undef IN_PROC_BROWSER_TEST_F_WITH_PRE

}  // namespace chromeos
