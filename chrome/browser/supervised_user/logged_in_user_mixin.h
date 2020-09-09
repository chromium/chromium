// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_LOGGED_IN_USER_MIXIN_H_
#define CHROME_BROWSER_SUPERVISED_USER_LOGGED_IN_USER_MIXIN_H_

#include "base/macros.h"
#include "base/optional.h"
#include "chrome/browser/chromeos/login/test/embedded_test_server_mixin.h"
#include "chrome/browser/chromeos/login/test/fake_gaia_mixin.h"
#include "chrome/browser/chromeos/login/test/local_policy_test_server_mixin.h"
#include "chrome/browser/chromeos/login/test/login_manager_mixin.h"
#include "chrome/browser/chromeos/login/test/user_policy_mixin.h"
#include "chrome/browser/chromeos/policy/user_policy_test_helper.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"

class AccountId;

namespace chromeos {

// Compound mixin class for easily logging in as regular or child accounts for
// browser tests. Initiates other mixins required to log in users, sets up their
// user policies and gaia auth.
// To use:
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
  enum class LogInType { kRegular, kChild };

  // |mixin_host| coordinates the other mixins. Since your browser test class
  // inherits from MixinBasedInProcessBrowserTest, there is an inherited
  // mixin_host_ member that can be passed into this constructor.
  // |type| specifies the desired user log in type, currently either regular or
  // child.
  // |embedded_test_server|: your browser test class should already inherit from
  // BrowserTestBase. That means there is an inherited embedded_test_server()
  // that can be passed into this constructor.
  // |test_base|: just pass in a pointer to the browser test class.
  // |should_launch_browser| determines whether a browser instance is launched
  // after successful login. Call SelectFirstBrowser() afterwards to ensure
  // calls to browser() don't return nullptr. LogInUser() already calls
  // SelectFirstBrowser() for convenience.
  // |account_id| is the desired test account id for logging in. The default
  // test account already works for the majority of test cases, unless an
  // enterprise account is needed for setting up policy.
  // |include_initial_user| determines whether the TestUserInfo should be passed
  // to the initial users list of the LoginManagerMixin. Excluding the initial
  // user causes the OOBE GAIA screen to show on start-up.
  // |use_local_policy_server| determines if the LocalPolicyTestServerMixin
  // should be passed into the UserPolicyMixin.
  LoggedInUserMixin(InProcessBrowserTestMixinHost* mixin_host,
                    LogInType type,
                    net::EmbeddedTestServer* embedded_test_server,
                    InProcessBrowserTest* test_base,
                    bool should_launch_browser = true,
                    base::Optional<AccountId> account_id = base::nullopt,
                    bool include_initial_user = true,
                    bool use_local_policy_server = true);
  ~LoggedInUserMixin() override;

  // InProcessBrowserTestMixin:
  void SetUpOnMainThread() override;

  // Log in as regular or child account depending on the |type| argument passed
  // to the constructor.
  // * If |issue_any_scope_token|, FakeGaiaMixin will issue a special all-access
  // token associated with the test refresh token. Only matters for child login.
  // * If |wait_for_active_session|, LoginManagerMixin will wait for the session
  // state to change to ACTIVE after logging in.
  // * If |request_policy_update|, UserPolicyMixin will set up user policy.
  void LogInUser(bool issue_any_scope_token = false,
                 bool wait_for_active_session = true,
                 bool request_policy_update = false);

  LoginManagerMixin* GetLoginManagerMixin() { return &login_manager_; }

  UserPolicyMixin* GetUserPolicyMixin() { return &user_policy_; }

  LocalPolicyTestServerMixin* GetLocalPolicyTestServerMixin() {
    return &local_policy_server_;
  }

  policy::UserPolicyTestHelper* GetUserPolicyTestHelper() {
    return &user_policy_helper_;
  }

  const AccountId& GetAccountId() { return user_.account_id; }

  FakeGaiaMixin* GetFakeGaiaMixin() { return &fake_gaia_; }

 private:
  LoginManagerMixin::TestUserInfo user_;
  LoginManagerMixin login_manager_;

  LocalPolicyTestServerMixin local_policy_server_;
  UserPolicyMixin user_policy_;
  policy::UserPolicyTestHelper user_policy_helper_;

  EmbeddedTestServerSetupMixin embedded_test_server_setup_;
  FakeGaiaMixin fake_gaia_;

  InProcessBrowserTest* test_base_;

  DISALLOW_COPY_AND_ASSIGN(LoggedInUserMixin);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_SUPERVISED_USER_LOGGED_IN_USER_MIXIN_H_
