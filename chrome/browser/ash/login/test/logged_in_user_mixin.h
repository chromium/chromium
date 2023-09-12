// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_TEST_LOGGED_IN_USER_MIXIN_H_
#define CHROME_BROWSER_ASH_LOGIN_TEST_LOGGED_IN_USER_MIXIN_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/login/test/embedded_test_server_setup_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/user_policy_mixin.h"
#include "chrome/browser/ash/policy/core/user_policy_test_helper.h"
#include "chrome/browser/ash/policy/test_support/embedded_policy_test_server_mixin.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class AccountId;

namespace ash {

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
  // after successful login.
  // |account_id| is the desired test account id for logging in. The default
  // test account already works for the majority of test cases, unless an
  // enterprise account is needed for setting up policy.
  // |include_initial_user| if true, then the user already exists on the login
  // screen. Otherwise, the user is newly added to the device and the OOBE Gaia
  // screen will show on start-up.
  // |use_embedded_policy_server| determines if the
  // EmbeddedPolicyTestServerMixin should be passed into the UserPolicyMixin.
  LoggedInUserMixin(InProcessBrowserTestMixinHost* mixin_host,
                    LogInType type,
                    net::EmbeddedTestServer* embedded_test_server,
                    InProcessBrowserTest* test_base,
                    bool should_launch_browser = true,
                    absl::optional<AccountId> account_id = absl::nullopt,
                    bool include_initial_user = true,
                    // TODO(crbug/1112885): Remove this parameter.
                    bool use_embedded_policy_server = true);
  LoggedInUserMixin(const LoggedInUserMixin&) = delete;
  LoggedInUserMixin& operator=(const LoggedInUserMixin&) = delete;
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
                 bool request_policy_update = true);

  LoginManagerMixin* GetLoginManagerMixin() { return &login_manager_; }

  UserPolicyMixin* GetUserPolicyMixin() { return &user_policy_; }

  EmbeddedPolicyTestServerMixin* GetEmbeddedPolicyTestServerMixin() {
    return &embedded_policy_server_;
  }

  policy::UserPolicyTestHelper* GetUserPolicyTestHelper() {
    return &user_policy_helper_;
  }

  const AccountId& GetAccountId() { return user_.account_id; }

  FakeGaiaMixin* GetFakeGaiaMixin() { return &fake_gaia_; }

 private:
  LoginManagerMixin::TestUserInfo user_;
  LoginManagerMixin login_manager_;

  EmbeddedPolicyTestServerMixin embedded_policy_server_;
  UserPolicyMixin user_policy_;
  policy::UserPolicyTestHelper user_policy_helper_;

  EmbeddedTestServerSetupMixin embedded_test_server_setup_;
  FakeGaiaMixin fake_gaia_;

  raw_ptr<InProcessBrowserTest, ExperimentalAsh> test_base_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_TEST_LOGGED_IN_USER_MIXIN_H_
