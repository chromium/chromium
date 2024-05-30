// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_TEST_LOGIN_MANAGER_MIXIN_H_
#define CHROME_BROWSER_ASH_LOGIN_TEST_LOGIN_MANAGER_MIXIN_H_

#include <initializer_list>
#include <memory>
#include <optional>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/login/test/local_state_mixin.h"
#include "chrome/browser/ash/login/test/session_flags_manager.h"
#include "chrome/browser/ash/login/test/user_auth_config.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_type.h"

namespace ash {

namespace test {

// These probably could be removed and replaced by
// FakeGaiaMixin::kFakeUserEmail/kFakeUserGaiaId.
inline constexpr char kTestEmail[] = "fake-email@gmail.com";
inline constexpr char kTestGaiaId[] = "fake-gaia-id";

}  // namespace test

class CryptohomeMixin;
class StubAuthenticatorBuilder;

// Mixin browser tests can use for setting up test login manager environment.
// It sets up command line so test starts on the login screen UI, and
// initializes user manager with a list of pre-registered users.
// The mixin will mark the OOBE flow as complete during test setup, so it's not
// suitable for OOBE tests.
class LoginManagerMixin : public InProcessBrowserTestMixin,
                          public LocalStateMixin::Delegate {
 public:
  // Represents test user.
  struct TestUserInfo {
    // Creates test user with regular user type from the given `account_id`.
    explicit TestUserInfo(const AccountId& account_id,
                          std::initializer_list<ash::AshAuthFactor> factors =
                              test::kDefaultAuthSetup)
        : TestUserInfo(account_id, factors, user_manager::UserType::kRegular) {}

    TestUserInfo(const AccountId& account_id, test::UserAuthConfig auth_config)
        : TestUserInfo(account_id,
                       auth_config,
                       user_manager::UserType::kRegular) {}

    // Creates test user with `user_type` from the given `account_id`.
    TestUserInfo(const AccountId& account_id,
                 std::initializer_list<ash::AshAuthFactor> factors,
                 user_manager::UserType user_type)
        : TestUserInfo(account_id,
                       test::UserAuthConfig::Create(factors),
                       user_type) {}

    TestUserInfo(const AccountId& account_id,
                 test::UserAuthConfig auth_config,
                 user_manager::UserType user_type)
        : account_id(account_id),
          auth_config(auth_config),
          user_type(user_type) {}

    const AccountId account_id;
    const test::UserAuthConfig auth_config;
    const user_manager::UserType user_type;
  };

  using UserList = std::vector<TestUserInfo>;

  // Convenience method for creating default UserContext for an account ID. The
  // result can be used with Login* methods below.
  static UserContext CreateDefaultUserContext(const TestUserInfo& account_id);

  // Convenience method for creating several test accounts.
  static AccountId CreateConsumerAccountId(int unique_number);

  // Should be called before any InProcessBrowserTestMixin functions.
  void AppendRegularUsers(int n);
  void AppendChildUsers(int n);
  void AppendManagedUsers(int n);

  explicit LoginManagerMixin(InProcessBrowserTestMixinHost* host);
  LoginManagerMixin(InProcessBrowserTestMixinHost* host,
                    const UserList& initial_users);
  LoginManagerMixin(InProcessBrowserTestMixinHost* host,
                    const UserList& initial_users,
                    FakeGaiaMixin* gaia_mixin);
  // When LoginManagerMixin is provided a handle to CryptohomeMixin,
  // all users added to LoginManagerMixin through
  // |LoginManagerMixin::AppendRegularUsers| and
  // |LoginManagerMixin::AppendManagedUsers| will also be forwarded to
  // CryptohomeMixin.
  LoginManagerMixin(InProcessBrowserTestMixinHost* host,
                    const UserList& initial_users,
                    FakeGaiaMixin* gaia_mixin,
                    CryptohomeMixin* cryptohome_mixin);

  LoginManagerMixin(const LoginManagerMixin&) = delete;
  LoginManagerMixin& operator=(const LoginManagerMixin&) = delete;

  ~LoginManagerMixin() override;

  // Enables session restore between multi-step test run (not very useful unless
  // the browser test has PRE part).
  // Should be called before mixin SetUp() is called to take effect.
  void set_session_restore_enabled() { session_restore_enabled_ = true; }


  // By default, LoginManagerMixin will set wait for successful profile
  // initialization. If test expects some errors during profile initialization
  // this gives an option to bypass the wait.
  void set_should_wait_for_profile(bool value) { wait_for_profile_ = value; }

  const UserList& users() const { return initial_users_; }

  // By default, LoginManagerMixin will set up user session manager not to
  // launch browser as part of user session setup - use this to override that
  // behavior.
  void SetShouldLaunchBrowser(bool value);

  void SetShouldObtainHandle(bool value);

  // Sets the list of default policy switches to be added to command line on the
  // login screen.
  void SetDefaultLoginSwitches(
      const std::vector<test::SessionFlagsManager::Switch>& swiches);

  // InProcessBrowserTestMixin:
  bool SetUpUserDataDirectory() override;
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

  // LocalStateMixin::Delegate:
  void SetUpLocalState() override;

  // Starts login attempt for a user, using the stub authenticator provided by
  // `authenticator_builder`.
  // Note that this will not wait for the login attempt to finish.
  void AttemptLoginUsingAuthenticator(
      const UserContext& user_context,
      std::unique_ptr<StubAuthenticatorBuilder> authenticator_builder);

  // Starts login attempt for a user, using actual authenticator backed by
  // FakeUserDataAuthClient. It is assumed that user already exists on the
  // device. Note that this will not wait for the login attempt to finish.
  void AttemptLoginUsingFakeDataAuthClient(const UserContext& user_context);

  // Starts login attempt for a user, assuming that user is a new user that have
  // just completed GAIA authentication.
  // Note that this will not wait for the login attempt to finish.
  void AttemptNewUserLoginUsingFakeDataAuthClient(
      const UserContext& user_context);

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

  // Logs in a user using with CreateDefaultUserContext(user_info) context.
  // When |wait_for_profile_prepared| is true, it waits until user profile is
  // fully initialized. This is used for regular user login.
  void LoginWithDefaultContext(const TestUserInfo& user_info);

  // Logs in as a regular user with default user context. Should be used for
  // proceeding into the session from the login screen.
  // If |user_context| is not set, built-in default will be used.
  void LoginAsNewRegularUser(
      std::optional<UserContext> user_context = std::nullopt);

  // Logs in as an enterprise user with default user context. Should be used
  // for proceeding into the session from the login screen.
  void LoginAsNewEnterpriseUser();

  // Logs in as a child user with default user context. Should be used for
  // proceeding into the session from the login screen.
  void LoginAsNewChildUser();

  // Forces skipping post login screens.
  void SkipPostLoginScreens();

 private:
  UserList initial_users_;

  // If set, session_flags_manager_ will be set up with session restore logic
  // enabled (it will restore session state between test runs for multi-step
  // browser tests).
  bool session_restore_enabled_ = false;
  test::SessionFlagsManager session_flags_manager_;

  // Whether `SetUpOnMainThread` was already called.
  bool set_up_on_main_thread_ = false;

  // Whether the user session manager should skip browser launch steps for
  // testing.
  bool should_launch_browser_ = false;

  // Whether the user session manager should try to obtain token handles.
  bool should_obtain_handles_ = false;

  // Whether the user will skip post login screens.
  bool skip_post_login_screens_ = false;

  // Whether we should wait for profile creation upon login.
  bool wait_for_profile_ = true;

  LocalStateMixin local_state_mixin_;
  raw_ptr<FakeGaiaMixin> fake_gaia_mixin_;
  raw_ptr<CryptohomeMixin> cryptohome_mixin_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_TEST_LOGIN_MANAGER_MIXIN_H_
