// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/test/logged_in_user_mixin.h"

#include <vector>

#include "chrome/browser/ash/login/test/user_auth_config.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/components/login/auth/stub_authenticator_builder.h"
#include "components/account_id/account_id.h"
#include "net/dns/mock_host_resolver.h"

namespace ash {
namespace {

user_manager::UserType ConvertUserType(LoggedInUserMixin::LogInType type) {
  switch (type) {
    case LoggedInUserMixin::LogInType::kChild:
      return user_manager::USER_TYPE_CHILD;
    case LoggedInUserMixin::LogInType::kRegular:
      return user_manager::USER_TYPE_REGULAR;
  }
}

std::vector<LoginManagerMixin::TestUserInfo> GetInitialUsers(
    const LoginManagerMixin::TestUserInfo& user,
    bool include_initial_user) {
  if (include_initial_user)
    return {user};
  return {};
}

}  // namespace

LoggedInUserMixin::LoggedInUserMixin(
    InProcessBrowserTestMixinHost* mixin_host,
    LogInType type,
    net::EmbeddedTestServer* embedded_test_server,
    InProcessBrowserTest* test_base,
    bool should_launch_browser,
    absl::optional<AccountId> account_id,
    bool include_initial_user,
    bool use_embedded_policy_server)
    : InProcessBrowserTestMixin(mixin_host),
      user_(account_id.value_or(
                AccountId::FromUserEmailGaiaId(FakeGaiaMixin::kFakeUserEmail,
                                               FakeGaiaMixin::kFakeUserGaiaId)),
            test::kDefaultAuthSetup,
            ConvertUserType(type)),
      login_manager_(mixin_host,
                     GetInitialUsers(user_, include_initial_user),
                     &fake_gaia_),
      embedded_policy_server_(mixin_host),
      user_policy_(
          mixin_host,
          user_.account_id,
          use_embedded_policy_server ? &embedded_policy_server_ : nullptr),
      user_policy_helper_(user_.account_id.GetUserEmail(),
                          &embedded_policy_server_),
      embedded_test_server_setup_(mixin_host, embedded_test_server),
      fake_gaia_(mixin_host),
      test_base_(test_base) {
  // By default, LoginManagerMixin will set up user session manager not to
  // launch browser as part of user session setup - use this to override that
  // behavior.
  login_manager_.set_should_launch_browser(should_launch_browser);
}

LoggedInUserMixin::~LoggedInUserMixin() = default;

void LoggedInUserMixin::SetUpOnMainThread() {
  // By default, browser tests block anything that doesn't go to localhost, so
  // account.google.com requests would never reach fake GAIA server without
  // this.
  test_base_->host_resolver()->AddRule("*", "127.0.0.1");
}

void LoggedInUserMixin::LogInUser(bool issue_any_scope_token,
                                  bool wait_for_active_session,
                                  bool request_policy_update,
                                  bool skip_post_login_screens) {
  if (skip_post_login_screens) {
    // Ensures logging in doesn't hang on the post login Gaia screens.
    login_manager_.SkipPostLoginScreens();
  } else {
    CHECK(!wait_for_active_session)
        << "wait_for_active_session must be false if skip_post_login_screen is "
           "false as there might not be an active session after a login.";
  }

  UserContext user_context = LoginManagerMixin::CreateDefaultUserContext(user_);
  user_context.SetRefreshToken(FakeGaiaMixin::kFakeRefreshToken);
  if (user_.user_type == user_manager::USER_TYPE_CHILD) {
    fake_gaia_.SetupFakeGaiaForChildUser(
        user_.account_id.GetUserEmail(), user_.account_id.GetGaiaId(),
        FakeGaiaMixin::kFakeRefreshToken, issue_any_scope_token);
  } else {
    fake_gaia_.SetupFakeGaiaForLogin(user_.account_id.GetUserEmail(),
                                     user_.account_id.GetGaiaId(),
                                     FakeGaiaMixin::kFakeRefreshToken);
  }
  if (request_policy_update) {
    // Child users require user policy, set up an empty one so the user can get
    // through login.
    GetUserPolicyMixin()->RequestPolicyUpdate();
  }
  if (wait_for_active_session) {
    login_manager_.LoginAndWaitForActiveSession(user_context);
    // If should_launch_browser was set to true, then ensures
    // InProcessBrowserTest::browser() doesn't return nullptr.
    test_base_->SelectFirstBrowser();
  } else {
    login_manager_.AttemptLoginUsingAuthenticator(
        user_context, std::make_unique<StubAuthenticatorBuilder>(user_context));
  }
}

}  // namespace ash
