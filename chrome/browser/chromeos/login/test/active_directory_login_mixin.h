// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_TEST_ACTIVE_DIRECTORY_LOGIN_MIXIN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_TEST_ACTIVE_DIRECTORY_LOGIN_MIXIN_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/values.h"
#include "chrome/browser/chromeos/policy/server_backed_state_keys_broker.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/policy/test_support/local_policy_test_server.h"
#include "content/public/test/browser_test_utils.h"

namespace chromeos {

// Handles interaction with Active Directory login screen and Active Directory
// password change screen.
class ActiveDirectoryLoginMixin : public InProcessBrowserTestMixin {
 public:
  explicit ActiveDirectoryLoginMixin(InProcessBrowserTestMixinHost* host);
  ~ActiveDirectoryLoginMixin() override;

  // InProcessBrowserTestMixin:
  void SetUpInProcessBrowserTestFixture() override;
  void SetUpOnMainThread() override;

  void set_autocomplete_realm(const std::string& autocomplete_realm) {
    autocomplete_realm_ = autocomplete_realm;
  }

  // Checks if Active Directory login is visible.
  void TestLoginVisible();
  // Checks if Active Directory password change screen is shown.
  void TestPasswordChangeVisible();
  // Checks if user input is marked as invalid.
  void TestUserError();
  void SetUserInput(const std::string& value);
  void TestUserInput(const std::string& value);
  // Checks if password input is marked as invalid.
  void TestPasswordError();
  // Checks that machine, password and user inputs are valid.
  void TestNoError();
  // Checks if autocomplete domain is visible for the user input.
  void TestDomainVisible();
  // Checks if autocomplete domain is hidden for the user input.
  void TestDomainHidden();

  void TriggerPasswordChangeScreen();
  void ClosePasswordChangeScreen();
  // Checks if Active Directory password change screen is shown. Also checks if
  // |invalid_element| is invalidated and all the other elements are valid.
  void TestPasswordChangeNoErrors();
  void TestPasswordChangeOldPasswordError();
  void TestPasswordChangeNewPasswordError();
  void TestPasswordChangeConfirmNewPasswordError();

  // Sets username and password for the Active Directory login and submits it.
  void SubmitActiveDirectoryCredentials(const std::string& username,
                                        const std::string& password);

  // Sets username and password for the Active Directory login and submits it.
  void SubmitActiveDirectoryPasswordChangeCredentials(
      const std::string& old_password,
      const std::string& new_password1,
      const std::string& new_password2);

  // Waits when Active Directory screen been invalidated from inside Chrome.
  void WaitForAuthError();

 private:
  void SetupActiveDirectoryJSNotifications();
  void TestPasswordChangeError(const std::string& invalid_element);

  std::string autocomplete_realm_;
  std::unique_ptr<content::DOMMessageQueue> message_queue_;

  DISALLOW_COPY_AND_ASSIGN(ActiveDirectoryLoginMixin);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_TEST_ACTIVE_DIRECTORY_LOGIN_MIXIN_H_
