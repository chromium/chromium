// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_LOGIN_MANAGER_TEST_H_
#define CHROME_BROWSER_ASH_LOGIN_LOGIN_MANAGER_TEST_H_

#include <string>

#include "chrome/browser/ash/login/test/embedded_test_server_setup_mixin.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"

class AccountId;

namespace ash {

class UserContext;

// Base class for Chrome OS Login tests. Should be used if you need to start at
// the Chrome OS Login screen (especially with existing users). For the tests
// that are focused more on OOBE - prefer OobeBaseTest. Use LoginManagerMixin to
// configure users for tests.
class LoginManagerTest : public MixinBasedInProcessBrowserTest {
 public:
  LoginManagerTest();

  LoginManagerTest(const LoginManagerTest&) = delete;
  LoginManagerTest& operator=(const LoginManagerTest&) = delete;

  ~LoginManagerTest() override;

  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpOnMainThread() override;

  // Could be used to registers the user with the given `account_id` on the
  // device. This method should be called in PRE_* test. Use only if necessary,
  // prefer LoginManagerMixin instead.
  void RegisterUser(const AccountId& account_id);

  static const char kPassword[];
  static const char kLocalPassword[];
  UserContext CreateUserContext(const AccountId& account_id,
                                const std::string& password);

  UserContext CreateUserContextWithLocalPassword(const AccountId& account_id,
                                                 const std::string& password);
  // Set expected credentials for next login attempt.
  void SetExpectedCredentials(const UserContext& user_context);

  // Tries to login with the credentials in `user_context`. The return value
  // indicates whether the login attempt succeeded.
  bool TryToLogin(const UserContext& user_context);

  // Tries to add the user identified and authenticated by `user_context` to the
  // session. The return value indicates whether the attempt succeeded. This
  // method does the same as TryToLogin() but doesn't verify that the new user
  // has become the active user.
  bool AddUserToSession(const UserContext& user_context);

  // Log in user with `user_id`. User should be registered using RegisterUser().
  void LoginUser(const AccountId& account_id);

  // Log in user with local password.
  void LoginUserWithLocalPassword(const AccountId& account_id);

  // Add user with `user_id` to session.
  void AddUser(const AccountId& user_id);

  // TODO(b/260718534): Fully switch from StubAuthenticator to
  // FakeUserDataAuthClient.
  void LoginUserWithDbusClient(const AccountId& account_id,
                               const std::string& password);
  void AddUserWithDbusClient(const AccountId& account_id,
                             const std::string& password);
  void SetExpectedCredentialsWithDbusClient(const AccountId& account_id,
                                            const std::string& password);

  void set_should_launch_browser(bool launch) {
    should_launch_browser_ = launch;
  }

 private:
  bool should_launch_browser_ = false;
  EmbeddedTestServerSetupMixin embedded_test_server_{&mixin_host_,
                                                     embedded_test_server()};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_LOGIN_MANAGER_TEST_H_
