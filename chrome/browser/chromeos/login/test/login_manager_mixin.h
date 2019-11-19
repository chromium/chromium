// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_TEST_LOGIN_MANAGER_MIXIN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_TEST_LOGIN_MANAGER_MIXIN_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "chrome/browser/chromeos/login/test/session_flags_manager.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user_type.h"

namespace chromeos {

class StubAuthenticatorBuilder;
class UserContext;

// Mixin browser tests can use for setting up test login manager environment.
// It sets up command line so test starts on the login screen UI, and
// initializes user manager with a list of pre-registered users.
// The mixin will mark the OOBE flow as complete during test setup, so it's not
// suitable for OOBE tests.
class LoginManagerMixin : public InProcessBrowserTestMixin {
 public:
  // Represents test user.
  struct TestUserInfo {
    // Creates test user with regular user type from the given |account_id|.
    explicit TestUserInfo(const AccountId& account_id)
        : account_id(account_id), user_type(user_manager::USER_TYPE_REGULAR) {}

    // Creates test user with |user_type| from the given |account_id|.
    TestUserInfo(const AccountId& account_id, user_manager::UserType user_type)
        : account_id(account_id), user_type(user_type) {}

    const AccountId account_id;
    const user_manager::UserType user_type;
  };

  // Convenience method for creating default UserContext for an account ID. The
  // result can be used with Login* methods below.
  static UserContext CreateDefaultUserContext(const TestUserInfo& account_id);

  LoginManagerMixin(InProcessBrowserTestMixinHost* host,
                    const std::vector<TestUserInfo>& initial_users);

  ~LoginManagerMixin() override;

  // Enables session restore between multi-step test run (not very useful unless
  // the browser test has PRE part).
  // Should be called before mixin SetUp() is called to take effect.
  void set_session_restore_enabled() { session_restore_enabled_ = true; }

  // By default, LoginManagerMixin will set up user session manager not to
  // launch browser as part of user session setup - use this to override that
  // behavior.
  void set_should_launch_browser(bool value) { should_launch_browser_ = value; }

  // Sets the list of default policy switches to be added to command line on the
  // login screen.
  void SetDefaultLoginSwitches(
      const std::vector<test::SessionFlagsManager::Switch>& swiches);

  // InProcessBrowserTestMixin:
  bool SetUpUserDataDirectory() override;
  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override;
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

  // Starts login attempt for a user, using the stub authenticator provided by
  // |authenticator_builder|.
  // Note that this will not wait for the login attempt to finish.
  void AttemptLoginUsingAuthenticator(
      const UserContext& user_context,
      std::unique_ptr<StubAuthenticatorBuilder> authenticator_builder);

  // Waits for the session state to change to ACTIVE. Returns immediately if the
  // session is already active.
  void WaitForActiveSession();

  // Logs in a user and waits for the session to become active.
  // This is equivalent to:
  // 1.  calling AttemptLoginUsingAuthenticator with the default stub
  //     authenticator (that succeeds if the provided user credentials match the
  //     credentials expected by the authenticator)
  // 2.  calling WaitForActiveSession().
  // Currently works for the primary user only.
  // Returns whether the newly logged in user is active when the method exits.
  bool LoginAndWaitForActiveSession(const UserContext& user_context);

 private:
  const std::vector<TestUserInfo> initial_users_;

  // If set, session_flags_manager_ will be set up with session restore logic
  // enabled (it will restore session state between test runs for multi-step
  // browser tests).
  bool session_restore_enabled_ = false;
  test::SessionFlagsManager session_flags_manager_;

  // Whether the user session manager should skip browser launch steps for
  // testing.
  bool should_launch_browser_ = false;

  DISALLOW_COPY_AND_ASSIGN(LoginManagerMixin);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_TEST_LOGIN_MANAGER_MIXIN_H_
