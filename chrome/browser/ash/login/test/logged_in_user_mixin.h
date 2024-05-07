// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_TEST_LOGGED_IN_USER_MIXIN_H_
#define CHROME_BROWSER_ASH_LOGIN_TEST_LOGGED_IN_USER_MIXIN_H_

#include <initializer_list>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/login/test/cryptohome_mixin.h"
#include "chrome/browser/ash/login/test/embedded_test_server_setup_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/user_auth_config.h"
#include "chrome/browser/ash/login/test/user_policy_mixin.h"
#include "chrome/browser/ash/policy/core/user_policy_test_helper.h"
#include "chrome/browser/ash/policy/test_support/embedded_policy_test_server_mixin.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

class AccountId;

namespace test {
class UserAuthConfig;
}

namespace ash {

// Compound mixin class for easily logging in as regular, managed or child
// accounts for browser tests. Initiates other mixins required to log in users,
// sets up their user policies and gaia auth. To use:
// * Make your browser test class inherit from MixinBasedInProcessBrowserTest.
// * Instantiate this class while passing in the inherited mixin_host_ member to
// the constructor.
// Note: the desired LogInType must be known at construction time.
// * Pass the inherited embedded_test_server() and pointer to
// InProcessBrowserTest instance into the constructor as well.
// * Call LogInUser() to log in.
// Example:
/*
class MyBrowserTestClass : public MixinBasedInProcessBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
    // The call below logs in as child user.
    logged_in_user_mixin_.LogInUser();
  }

 private:
  LoggedInUserMixin logged_in_user_mixin_{&mixin_host_,
                                          LoggedInUserMixin::LogInType::kChild,
                                          embedded_test_server(), this};
};
*/
class LoggedInUserMixin : public InProcessBrowserTestMixin {
 public:
  enum class LogInType {
    // Regular consumer user, default account id at gmail.com.
    kConsumer,
    // Regular consumer user, default account id at custom domain, not managed.
    kConsumerCustomDomain,
    // Child consumer user, default account id at gmail.com, managed.
    kChild,
    // Enterprise managed user, default account id at custom domain, managed.
    kManaged,
  };

  // Flags for fine-tuning login flow in some edge cases.
  enum class LoginDetails {
    // Prevents browser launch after successful login.
    kNoBrowserLaunch,
    // FakeGaiaMixin will issue a special all-access
    // token associated with the test refresh token. Only matters for child
    // login.
    kUseAnyScopeToken,
    // Without this LoginManagerMixin will wait for the session
    // state to change to ACTIVE after logging in. Use if some errors
    // are expected during login, or if user expected to go through
    // onboarding after login.
    kDontWaitForSession,
    // This can be used by policy-related test to set up
    // situations when policy can not be fetched for the user.
    kNoPolicyForUser,
    // By default mixin would skip onboarding in OOBE even for
    // new users. This can be used to prevent that and
    // let test interact with respective UIs.
    // Implies `kDontWaitForSession`.
    kUserOnboarding,
  };

  // `mixin_host` coordinates the other mixins. Since your browser test class
  // inherits from MixinBasedInProcessBrowserTest, there is an inherited
  // `test_base`: just pass in a pointer to the browser test class.
  // mixin_host_ member that can be passed into this constructor.
  // `embedded_test_server`: your browser test class should already inherit from
  // BrowserTestBase. That means there is an inherited embedded_test_server()
  // that can be passed into this constructor.
  // `type` specifies the desired user log in type, see `LogInType` above.
  // `include_initial_user` if true, then the user already exists on the login
  // screen. Otherwise, the user is newly added to the device and the OOBE Gaia
  // screen will show on start-up.
  // `account_id` is the desired test account id for logging in. The default
  // test account already works for the majority of test cases, unless an
  // enterprise account is needed for setting up policy.
  // `auth_config` defines the factors set up for the user. The default user
  // will have the (gaia) password set to `ash::test:kGaiaPassword`. This
  // parameter allows tests to define more complex configurations if needed.
  LoggedInUserMixin(
      InProcessBrowserTestMixinHost* mixin_host,
      InProcessBrowserTest* test_base,
      net::EmbeddedTestServer* embedded_test_server,
      LogInType type,
      bool include_initial_user = true,
      std::optional<AccountId> account_id = std::nullopt,
      std::optional<test::UserAuthConfig> auth_config = std::nullopt);
  LoggedInUserMixin(const LoggedInUserMixin&) = delete;
  LoggedInUserMixin& operator=(const LoggedInUserMixin&) = delete;
  ~LoggedInUserMixin() override;

  // InProcessBrowserTestMixin:
  void SetUpOnMainThread() override;

  // Log in as regular or child account depending on the `type` argument passed
  // to the constructor.
  // * If `issue_any_scope_token`, FakeGaiaMixin will issue a special all-access
  // token associated with the test refresh token. Only matters for child login.
  // * If `wait_for_active_session`, LoginManagerMixin will wait for the session
  // state to change to ACTIVE after logging in.
  // * If `request_policy_update`, UserPolicyMixin will set up user policy.
  // * If `skip_post_login_screens`, LoginManagerMixin will skip post login
  // screens. Default value is true (skip). Note that `wait_for_active_session`
  // must be false if this value is false as there won't be no active session
  // immediately after login.
  void LogInUser(std::initializer_list<LoginDetails> login_details = {});
  void LogInUser(base::flat_set<LoginDetails> login_details);

  LoginManagerMixin* GetLoginManagerMixin() { return &login_manager_; }

  UserPolicyMixin* GetUserPolicyMixin() { return &user_policy_; }

  EmbeddedPolicyTestServerMixin* GetEmbeddedPolicyTestServerMixin() {
    return &embedded_policy_server_;
  }

  CryptohomeMixin& GetCryptohomeMixin() { return cryptohome_; }

  policy::UserPolicyTestHelper* GetUserPolicyTestHelper() {
    return &user_policy_helper_;
  }

  const AccountId& GetAccountId() { return user_.account_id; }

  FakeGaiaMixin* GetFakeGaiaMixin() { return &fake_gaia_; }

 private:
  LogInType login_type_;
  LoginManagerMixin::TestUserInfo user_;
  bool include_initial_user_;
  FakeGaiaMixin fake_gaia_;
  CryptohomeMixin cryptohome_;
  LoginManagerMixin login_manager_;

  EmbeddedPolicyTestServerMixin embedded_policy_server_;
  UserPolicyMixin user_policy_;
  policy::UserPolicyTestHelper user_policy_helper_;

  EmbeddedTestServerSetupMixin embedded_test_server_setup_;

  raw_ptr<InProcessBrowserTest> test_base_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_TEST_LOGGED_IN_USER_MIXIN_H_
