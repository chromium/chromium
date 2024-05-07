// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/test/logged_in_user_mixin.h"

#include <optional>

#include "base/check.h"
#include "chrome/browser/ash/login/test/cryptohome_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/user_auth_config.h"
#include "chrome/browser/ash/net/delay_network_call.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/components/login/auth/stub_authenticator_builder.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user_type.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace ash {
namespace {

user_manager::UserType ConvertUserType(LoggedInUserMixin::LogInType type) {
  switch (type) {
    case LoggedInUserMixin::LogInType::kChild:
      return user_manager::UserType::kChild;
    case LoggedInUserMixin::LogInType::kConsumer:
    case LoggedInUserMixin::LogInType::kConsumerCustomDomain:
    case LoggedInUserMixin::LogInType::kManaged:
      return user_manager::UserType::kRegular;
  }
}

const AccountId AccountIdForType(LoggedInUserMixin::LogInType type) {
  switch (type) {
    case LoggedInUserMixin::LogInType::kChild:
    case LoggedInUserMixin::LogInType::kConsumer:
      return AccountId::FromUserEmailGaiaId(FakeGaiaMixin::kFakeUserEmail,
                                            FakeGaiaMixin::kFakeUserGaiaId);
    case LoggedInUserMixin::LogInType::kConsumerCustomDomain:
    case LoggedInUserMixin::LogInType::kManaged:
      return AccountId::FromUserEmailGaiaId(
          FakeGaiaMixin::kEnterpriseUser1,
          FakeGaiaMixin::kEnterpriseUser1GaiaId);
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
    std::optional<AccountId> account_id,
    std::optional<test::UserAuthConfig> auth_config,
    bool include_initial_user)
    : InProcessBrowserTestMixin(mixin_host),
      login_type_(type),
      user_(account_id.value_or(AccountIdForType(type)),
            auth_config.value_or(
                test::UserAuthConfig::Create(test::kDefaultAuthSetup)),
            ConvertUserType(type)),
      include_initial_user_(include_initial_user),
      fake_gaia_(mixin_host),
      cryptohome_(mixin_host),
      login_manager_(mixin_host,
                     GetInitialUsers(user_, include_initial_user),
                     &fake_gaia_,
                     &cryptohome_),
      embedded_policy_server_(mixin_host,
                              {EmbeddedPolicyTestServerMixin::Capabilities::
                                   PER_USER_MANAGEMENT_STATUS}),
      user_policy_(mixin_host, user_.account_id, &embedded_policy_server_),
      user_policy_helper_(user_.account_id.GetUserEmail(),
                          &embedded_policy_server_),
      embedded_test_server_setup_(mixin_host, embedded_test_server),
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
  login_manager_.set_should_wait_for_profile(wait_for_active_session);
  UserContext user_context = LoginManagerMixin::CreateDefaultUserContext(user_);
  user_context.SetRefreshToken(FakeGaiaMixin::kFakeRefreshToken);
  if (user_.user_type == user_manager::UserType::kChild) {
    // TODO(b/333450354): Determine why this is necessary and fix.
    SetDelayNetworkCallsForTesting(true);

    fake_gaia_.SetupFakeGaiaForChildUser(
        user_.account_id.GetUserEmail(), user_.account_id.GetGaiaId(),
        FakeGaiaMixin::kFakeRefreshToken, issue_any_scope_token);
  } else {
    fake_gaia_.SetupFakeGaiaForLogin(user_.account_id.GetUserEmail(),
                                     user_.account_id.GetGaiaId(),
                                     FakeGaiaMixin::kFakeRefreshToken);
  }

  if (request_policy_update) {
    if (login_type_ == LogInType::kChild ||
        login_type_ == LogInType::kManaged) {
      embedded_policy_server_.MarkUserAsManaged(user_.account_id);
    }
    // Managed users require user policy, set up an empty one so the user can
    // get through login.
    GetUserPolicyMixin()->RequestPolicyUpdate();
  }
  if (!include_initial_user_) {
    if (user_.user_type == user_manager::UserType::kChild) {
      CHECK(user_.account_id.GetUserEmail() == FakeGaiaMixin::kFakeUserEmail);
      CHECK(user_.account_id.GetGaiaId() == FakeGaiaMixin::kFakeUserGaiaId);
      login_manager_.LoginAsNewChildUser();
    } else {
      login_manager_.LoginAsNewRegularUser(user_context);
    }
  } else {
    login_manager_.AttemptLoginUsingAuthenticator(
        user_context, std::make_unique<StubAuthenticatorBuilder>(user_context));
  }
  if (wait_for_active_session) {
    login_manager_.WaitForActiveSession();
    // If should_launch_browser was set to true, then ensures
    // InProcessBrowserTest::browser() doesn't return nullptr.
    test_base_->SelectFirstBrowser();
  }
}

}  // namespace ash
