// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/login/login_policy_test_base.h"

#include <memory>

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/values.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/browser/ash/login/test/user_auth_config.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/policy/core/user_policy_test_helper.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "components/policy/core/common/cloud/test/policy_builder.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/policy/proto/cloud_policy.pb.h"
#include "google_apis/gaia/fake_gaia.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

constexpr char kTestAuthCode[] = "fake-auth-code";
constexpr char kTestAuthLoginAccessToken[] = "fake-access-token";
constexpr char kTestRefreshToken[] = "fake-refresh-token";
constexpr char kTestAuthSIDCookie[] = "fake-auth-SID-cookie";
constexpr char kTestAuthLSIDCookie[] = "fake-auth-LSID-cookie";
constexpr char kTestSessionSIDCookie[] = "fake-session-SID-cookie";
constexpr char kTestSessionLSIDCookie[] = "fake-session-LSID-cookie";

// Cannot use LoginManagerMixin default account because it's on gmail.com,
// which cannot be enterprise domain. See kNonManagedDomainPatterns in
// browser_policy_connector.cc.
constexpr char kAccountId[] = "user@example.com";
constexpr char kAccountGaiaId[] = "user-example-com-test-gaia-id";
}  // namespace

LoginPolicyTestBase::LoginPolicyTestBase()
    : account_id_(AccountId::FromUserEmailGaiaId(kAccountId, kAccountGaiaId)) {
  set_open_about_blank_on_browser_launch(false);
  login_manager_.SetShouldLaunchBrowser(true);
}

LoginPolicyTestBase::~LoginPolicyTestBase() = default;

void LoginPolicyTestBase::SetUpCommandLine(base::CommandLine* command_line) {
  OobeBaseTest::SetUpCommandLine(command_line);
  command_line->AppendSwitch(ash::switches::kDisableGaiaServices);
  command_line->AppendSwitch(ash::switches::kSkipForceOnlineSignInForTesting);

  // This will change the verification key to be used by the
  // CloudPolicyValidator. It will allow for the policy provided by the
  // PolicyBuilder to pass the signature validation.
  command_line->AppendSwitchASCII(
      switches::kPolicyVerificationKey,
      PolicyBuilder::GetEncodedPolicyVerificationKey());
}

void LoginPolicyTestBase::SetUpInProcessBrowserTestFixture() {
  OobeBaseTest::SetUpInProcessBrowserTestFixture();
  enterprise_management::CloudPolicySettings settings;
  GetPolicySettings(&settings);
  user_policy_helper_ = std::make_unique<UserPolicyTestHelper>(
      account_id().GetUserEmail(), &policy_test_server_mixin_);
  user_policy_helper_->SetPolicy(settings);
}

void LoginPolicyTestBase::SetUpOnMainThread() {
  SetConfiguration();
  fake_gaia_.SetupFakeGaiaForLogin(account_id().GetUserEmail(),
                                   account_id().GetGaiaId(), kTestRefreshToken);
  OobeBaseTest::SetUpOnMainThread();

  FakeGaia::Configuration params;
  params.id_token = GetIdToken();
  fake_gaia_.fake_gaia()->UpdateConfiguration(params);

  cryptohome_mixin_.ApplyAuthConfigIfUserExists(
      account_id(),
      ash::test::UserAuthConfig::Create(ash::test::kDefaultAuthSetup));
}

std::string LoginPolicyTestBase::GetIdToken() const {
  return std::string();
}

Profile* LoginPolicyTestBase::GetProfileForActiveUser() {
  const user_manager::User* const user =
      user_manager::UserManager::Get()->GetActiveUser();

  EXPECT_NE(user, nullptr);

  return ash::ProfileHelper::Get()->GetProfileByUser(user);
}

void LoginPolicyTestBase::GetPolicySettings(
    enterprise_management::CloudPolicySettings* settings) const {}

void LoginPolicyTestBase::SetConfiguration() {
  FakeGaia::Configuration params;
  params.auth_sid_cookie = kTestAuthSIDCookie;
  params.auth_lsid_cookie = kTestAuthLSIDCookie;
  params.auth_code = kTestAuthCode;
  params.refresh_token = kTestRefreshToken;
  params.access_token = kTestAuthLoginAccessToken;
  params.id_token = GetIdToken();
  params.session_sid_cookie = kTestSessionSIDCookie;
  params.session_lsid_cookie = kTestSessionLSIDCookie;
  params.email = account_id().GetUserEmail();
  fake_gaia_.fake_gaia()->SetConfiguration(params);
}

void LoginPolicyTestBase::SkipToLoginScreen() {
  login_manager_.SkipPostLoginScreens();
  OobeBaseTest::WaitForSigninScreen();
}

void LoginPolicyTestBase::TriggerLogIn() {
  const ash::LoginManagerMixin::TestUserInfo test_user(account_id());
  auto user_context =
      ash::LoginManagerMixin::CreateDefaultUserContext(test_user);
  login_manager_.LoginAsNewRegularUser(user_context);
}

void LoginPolicyTestBase::LogIn() {
  login_manager_.SkipPostLoginScreens();
  TriggerLogIn();
  ash::test::WaitForPrimaryUserSessionStart();
}

}  // namespace policy
