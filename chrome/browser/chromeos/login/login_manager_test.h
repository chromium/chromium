// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_MANAGER_TEST_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_MANAGER_TEST_H_

#include <string>

#include "base/macros.h"
#include "chrome/browser/chromeos/login/test/embedded_test_server_mixin.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"

class AccountId;

namespace chromeos {

class UserContext;

// Base class for Chrome OS out-of-box/login WebUI tests.
// If no special configuration is done launches out-of-box WebUI.
// To launch login UI use PRE_* test that will register user(s) and mark
// out-of-box as completed.
// Guarantees that WebUI has been initialized by waiting for
// NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE notification.
class LoginManagerTest : public MixinBasedInProcessBrowserTest {
 public:
  LoginManagerTest(bool should_launch_browser,
                   bool should_initialize_webui);
  ~LoginManagerTest() override;

  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpOnMainThread() override;

  // Registers the user with the given |user_id| on the device.
  // This method should be called in PRE_* test.
  // TODO(dzhioev): Add the ability to register users without a PRE_* test.
  void RegisterUser(const AccountId& account_id);

  // Set expected credentials for next login attempt.
  void SetExpectedCredentials(const UserContext& user_context);

  // Tries to login with the credentials in |user_context|. The return value
  // indicates whether the login attempt succeeded.
  bool TryToLogin(const UserContext& user_context);

  // Tries to add the user identified and authenticated by |user_context| to the
  // session. The return value indicates whether the attempt succeeded. This
  // method does the same as TryToLogin() but doesn't verify that the new user
  // has become the active user.
  bool AddUserToSession(const UserContext& user_context);

  // Log in user with |user_id|. User should be registered using RegisterUser().
  void LoginUser(const AccountId& account_id);

  // Add user with |user_id| to session.
  void AddUser(const AccountId& user_id);

  void set_force_webui_login(bool force) { force_webui_login_ = force; }

 private:
  // If set, the tests will use deprecated webui login.
  // TODO(tbarzic): Migrate all tests to work with views login implementation.
  bool force_webui_login_ = true;
  const bool should_launch_browser_;
  const bool should_initialize_webui_;
  EmbeddedTestServerSetupMixin embedded_test_server_{&mixin_host_,
                                                     embedded_test_server()};

  DISALLOW_COPY_AND_ASSIGN(LoginManagerTest);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_MANAGER_TEST_H_
