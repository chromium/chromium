// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/osauth/remove_local_auth_factors_screen.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_paths.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/signin/signin_error_notifier.h"
#include "chrome/browser/ash/login/test/auth_ui_utils.h"
#include "chrome/browser/ash/login/test/cryptohome_mixin.h"
#include "chrome/browser/ash/login/test/fake_recovery_service_mixin.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screens_utils.h"
#include "chrome/browser/ash/login/test/user_auth_config.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/webui/ash/login/cryptohome_recovery_setup_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/remove_local_auth_factors_screen_handler.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_userdataauth_client.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/components/osauth/public/auth_session_storage.h"
#include "chromeos/ash/components/osauth/public/common_types.h"
#include "components/account_id/account_id.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/policy_constants.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_test.h"

namespace ash {
namespace {

using test::UserAuthConfig;

enum class UserType {
  kLocalPasswordUser = 0,
  kPinUser,
  kLocalPasswordAndPinUser,
  kGaiaPasswordAndPinUser,
  kGaiaPasswordUser,
};

const test::UIPath kDoneButtonPath = {"remove-local-auth-factors",
                                      "doneButton"};
}  // namespace

// Test suite for the RemoveLocalAuthFactorsScreen.
// This screen is shown during the login flow when a user needs to
// re-authenticate and their current set of local authentication factors (like
// PIN or local password) are no longer allowed by policy. The screen ensures
// that these disallowed factors are removed.
class RemoveLocalAuthFactorsScreenTest : public LoginManagerTest {
 public:
  RemoveLocalAuthFactorsScreenTest() {
    feature_list_.InitWithFeatures(
        {features::kManagedLocalPinAndPassword, features::kRecoveryFlowReorder},
        {});
    UserDataAuthClient::InitializeFake();
    FakeUserDataAuthClient::TestApi::Get()
        ->set_supports_low_entropy_credentials(/*supports=*/true);
    ignore_sync_errors_for_test_ =
        SigninErrorNotifier::IgnoreSyncErrorsForTesting();
  }
  ~RemoveLocalAuthFactorsScreenTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    LoginManagerTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kAllowFailedPolicyFetchForTest);
  }

  void SetUpInProcessBrowserTestFixture() override {
    LoginManagerTest::SetUpInProcessBrowserTestFixture();
    // Initialize user policy.
    provider_.SetDefaultReturns(/*is_initialization_complete_return=*/true,
                                /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
  }

  void ReauthUser(const AccountId& account_id) {
    fake_gaia_.SetupFakeGaiaForLogin(account_id.GetUserEmail(),
                                     account_id.GetGaiaId(),
                                     FakeGaiaMixin::kFakeRefreshToken);

    test::OnLoginScreen()->SelectUserPod(account_id);
    auto gaia = test::AwaitGaiaSigninUI();

    gaia->ReauthConfirmEmail(account_id);
    gaia->TypePassword(test::kLocalPassword);
    gaia->ContinueLogin();
  }

  void ReauthUserWithLocalPassword(const AccountId& account_id) {
    ReauthUser(account_id);
    auto local_authentication =
        test::OnLoginScreen()->WaitForLocalAuthenticationDialog();

    local_authentication->SubmitPassword(test::kLocalPassword);
  }

  void ReauthUserWithPin(const AccountId& account_id) {
    ReauthUser(account_id);
    auto local_authentication =
        test::OnLoginScreen()->WaitForLocalAuthenticationDialog();

    local_authentication->SubmitPin(test::kAuthPin);
  }

  void VerifyLocalAuthFactorsRemovedAndSessionStarted(
      const AccountId& account_id) {
    EXPECT_FALSE(cryptohome_.HasPinFactor(account_id));
    EXPECT_FALSE(cryptohome_.HasLocalPasswordFactor(account_id));
    EXPECT_TRUE(cryptohome_.HasGaiaPasswordFactor(account_id));

    test::OobeJS().ClickOnPath(kDoneButtonPath);

    login_manager_mixin_.WaitForActiveSession();
  }

  void WaitForDoneButton() {
    test::OobeJS().CreateVisibilityWaiter(true, kDoneButtonPath)->Wait();
    test::OobeJS().ExpectEnabledPath(kDoneButtonPath);
  }

  void WaitForRemoveLocalAuthFactorsSuccessScreen() {
    OobeScreenWaiter(RemoveLocalAuthFactorsScreenView::kScreenId).Wait();
    WaitForDoneButton();
  }

  void DisableAllAllowedAuthFactorsPolicy() {
    base::Value allowed_auth_factors(base::Value::Type::LIST);
    policy::PolicyMap user_policy;
    user_policy.Set(policy::key::kAllowedLocalAuthFactors,
                    policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                    policy::POLICY_SOURCE_CLOUD,
                    std::move(allowed_auth_factors), nullptr);
    provider_.UpdateChromePolicy(user_policy);
  }

  void CreateEarlyPrefsDirectory(const AccountId& account_id) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    auto* active_user = user_manager::UserManager::Get()->FindUser(account_id);
    base::FilePath early_prefs_dir;
    bool success =
        base::PathService::Get(ash::DIR_HOMEDIR_MOUNT, &early_prefs_dir);
    CHECK(success);
    early_prefs_dir = early_prefs_dir.Append(active_user->username_hash());
    base::CreateDirectory(early_prefs_dir);
  }

  void LoginOfflineAndSetPolicy(const AccountId& account_id) {
    fake_gaia_.SetupFakeGaiaForLogin(account_id.GetUserEmail(),
                                     account_id.GetGaiaId(),
                                     FakeGaiaMixin::kFakeRefreshToken);
    LoginUser(account_id);
    login_manager_mixin_.WaitForActiveSession();
    CreateEarlyPrefsDirectory(account_id);
    DisableAllAllowedAuthFactorsPolicy();
    user_manager::UserManager::Get()->SaveForceOnlineSignin(
        account_id, /*force_online_signin=*/true);
  }

 protected:
  const LoginManagerMixin::TestUserInfo local_password_only_user_{
      LoginManagerMixin::CreateEnterpriseAccountId(
          static_cast<int>(UserType::kLocalPasswordUser)),
      UserAuthConfig::Create({AshAuthFactor::kLocalPassword})
          .RequireReauth(/*require_reauth=*/true)};

  const LoginManagerMixin::TestUserInfo pin_only_user_{
      LoginManagerMixin::CreateEnterpriseAccountId(
          static_cast<int>(UserType::kPinUser)),
      UserAuthConfig::Create({AshAuthFactor::kCryptohomePin})
          .RequireReauth(/*require_reauth=*/true)};

  const LoginManagerMixin::TestUserInfo local_password_and_pin_user_{
      LoginManagerMixin::CreateEnterpriseAccountId(
          static_cast<int>(UserType::kLocalPasswordAndPinUser)),
      UserAuthConfig::Create(
          {AshAuthFactor::kLocalPassword, AshAuthFactor::kCryptohomePin})
          .RequireReauth(/*require_reauth=*/true)};

  const LoginManagerMixin::TestUserInfo gaia_password_and_pin_user_{
      LoginManagerMixin::CreateEnterpriseAccountId(
          static_cast<int>(UserType::kGaiaPasswordAndPinUser)),
      UserAuthConfig::Create({
                                 AshAuthFactor::kGaiaPassword,
                                 AshAuthFactor::kCryptohomePin,
                             })
          .RequireReauth(/*require_reauth=*/true)};

  const LoginManagerMixin::TestUserInfo gaia_password_user_{
      LoginManagerMixin::CreateEnterpriseAccountId(
          static_cast<int>(UserType::kGaiaPasswordUser)),
      UserAuthConfig::Create({
                                 AshAuthFactor::kGaiaPassword,
                             })
          .RequireReauth(/*require_reauth=*/true)};

  CryptohomeMixin cryptohome_{&mixin_host_};
  FakeGaiaMixin fake_gaia_{&mixin_host_};
  LoginManagerMixin login_manager_mixin_{
      &mixin_host_,
      {local_password_and_pin_user_, pin_only_user_, local_password_only_user_,
       gaia_password_and_pin_user_, gaia_password_user_},
      &fake_gaia_,
      &cryptohome_};
  FakeRecoveryServiceMixin fake_recovery_service_{&mixin_host_,
                                                  embedded_test_server()};
  std::unique_ptr<base::AutoReset<bool>> ignore_sync_errors_for_test_;

 private:
  testing::NiceMock<policy::MockConfigurationPolicyProvider> provider_;

  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(RemoveLocalAuthFactorsScreenTest,
                       PRE_AuthFactorsRemovedForPinUser) {
  // Test Setup: Log the user in offline and apply a policy disabling all
  // local auth factors. This sets the initial state for the next test step.
  LoginOfflineAndSetPolicy(pin_only_user_.account_id);
}

IN_PROC_BROWSER_TEST_F(RemoveLocalAuthFactorsScreenTest,
                       AuthFactorsRemovedForPinUser) {
  // Test Setup is handled by the PRE_ test above.

  // Test Execution: User attempts to re-authenticate with their PIN,
  // triggering the screen to handle disallowed factors.
  ReauthUserWithPin(pin_only_user_.account_id);
  WaitForRemoveLocalAuthFactorsSuccessScreen();

  // Test Verification: Confirm local factors (PIN/Local password) are removed
  // and the user successfully proceeds into an active session.
  VerifyLocalAuthFactorsRemovedAndSessionStarted(pin_only_user_.account_id);
}

IN_PROC_BROWSER_TEST_F(RemoveLocalAuthFactorsScreenTest,
                       PRE_AuthSessionKeptAlive) {
  LoginOfflineAndSetPolicy(pin_only_user_.account_id);
}

IN_PROC_BROWSER_TEST_F(RemoveLocalAuthFactorsScreenTest, AuthSessionKeptAlive) {
  ReauthUserWithPin(pin_only_user_.account_id);
  WaitForRemoveLocalAuthFactorsSuccessScreen();

  // Verify that the auth session has a keep alive while on this screen.
  auto* wizard_context = LoginDisplayHost::default_host()->GetWizardContext();
  ASSERT_TRUE(wizard_context->extra_factors_token.has_value());
  EXPECT_TRUE(AuthSessionStorage::Get()->CheckHasKeepAliveForTesting(
      wizard_context->extra_factors_token.value()));

  VerifyLocalAuthFactorsRemovedAndSessionStarted(pin_only_user_.account_id);
}

IN_PROC_BROWSER_TEST_F(RemoveLocalAuthFactorsScreenTest,
                       PRE_AuthFactorsRemovedForGaiaPasswordPinUser) {
  // Test Setup: Log the user in offline and apply a policy disabling all
  // local auth factors.
  LoginOfflineAndSetPolicy(gaia_password_and_pin_user_.account_id);
}

IN_PROC_BROWSER_TEST_F(RemoveLocalAuthFactorsScreenTest,
                       AuthFactorsRemovedForGaiaPasswordPinUser) {
  // Test Setup is handled by the PRE_ test above.

  // Test Execution: User attempts to re-authenticate (via Gaia),
  // triggering the local auth factors removal flow.
  ReauthUser(gaia_password_and_pin_user_.account_id);
  WaitForRemoveLocalAuthFactorsSuccessScreen();

  // Test Verification: Confirm local factors are removed while Gaia
  // remains intact, and the session starts.
  VerifyLocalAuthFactorsRemovedAndSessionStarted(
      gaia_password_and_pin_user_.account_id);
}

IN_PROC_BROWSER_TEST_F(
    RemoveLocalAuthFactorsScreenTest,
    PRE_AuthFactorsRemovedScreenNotShownForGaiaPasswordUser) {
  // Test Setup: Log the user in offline and apply a policy disabling all
  // local auth factors (although this user only has a Gaia password).
  LoginOfflineAndSetPolicy(gaia_password_user_.account_id);
}

IN_PROC_BROWSER_TEST_F(RemoveLocalAuthFactorsScreenTest,
                       AuthFactorsRemovedScreenNotShownForGaiaPasswordUser) {
  // Test Setup is handled by the PRE_ test above.

  // Test Execution: User with only a Gaia password attempts to re-authenticate.
  ReauthUser(gaia_password_user_.account_id);

  // Test Verification: Because the user had no disallowed local auth factors,
  // the removal screen is bypassed entirely and the active session starts
  // immediately.
  login_manager_mixin_.WaitForActiveSession();
}

class RemoveLocalAuthFactorsScreenTestParameterizedBase
    : public RemoveLocalAuthFactorsScreenTest,
      public ::testing::WithParamInterface<UserType> {
 public:
  AccountId GetAccountId() {
    return LoginManagerMixin::CreateEnterpriseAccountId(
        static_cast<int>(GetParam()));
  }
};

class RemoveLocalAuthFactorsScreenTestWithLocalPassword
    : public RemoveLocalAuthFactorsScreenTestParameterizedBase {};

IN_PROC_BROWSER_TEST_P(RemoveLocalAuthFactorsScreenTestWithLocalPassword,
                       PRE_AuthFactorsRemovedForUserWithLocalPassword) {
  // Test Setup: Log the parameterized user in offline and apply the policy
  // that disables all local auth factors.
  LoginOfflineAndSetPolicy(GetAccountId());
}

IN_PROC_BROWSER_TEST_P(RemoveLocalAuthFactorsScreenTestWithLocalPassword,
                       AuthFactorsRemovedForUserWithLocalPassword) {
  // Test Setup is handled by the PRE_ test above.

  // Test Execution: User re-authenticates with their local password,
  // encountering the screen to clear disallowed factors.
  ReauthUserWithLocalPassword(GetAccountId());
  WaitForRemoveLocalAuthFactorsSuccessScreen();

  // Test Verification: Ensure local factors are completely removed and
  // the user proceeds into an active session.
  VerifyLocalAuthFactorsRemovedAndSessionStarted(GetAccountId());
}

class RemoveLocalAuthFactorsScreenTestWithRecovery
    : public RemoveLocalAuthFactorsScreenTestParameterizedBase {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    cryptohome_.AddRecoveryFactor(GetAccountId());
    RemoveLocalAuthFactorsScreenTest::SetUpInProcessBrowserTestFixture();
  }
};

IN_PROC_BROWSER_TEST_P(RemoveLocalAuthFactorsScreenTestWithRecovery,
                       PRE_AuthFactorsRemovedForUserWithRecovery) {
  // Test Setup: Log the user in offline and apply the policy that disables
  // all local auth factors (Cryptohome recovery factor is already set up).
  LoginOfflineAndSetPolicy(GetAccountId());
}

IN_PROC_BROWSER_TEST_P(RemoveLocalAuthFactorsScreenTestWithRecovery,
                       AuthFactorsRemovedForUserWithRecovery) {
  // Test Setup is handled by the PRE_ test above.

  // Test Execution: Re-authenticate the user, forcing them through the
  // local auth factor removal UI flow.
  ReauthUser(GetAccountId());
  WaitForRemoveLocalAuthFactorsSuccessScreen();

  // Test Verification: Confirm that disallowed factors are removed and
  // an active user session successfully starts.
  VerifyLocalAuthFactorsRemovedAndSessionStarted(GetAccountId());
}

// Instantiate the parameterized tests for users with recovery factors.
INSTANTIATE_TEST_SUITE_P(
    RemoveLocalAuthFactorsScreenTestWithRecoveryInstantiation,
    RemoveLocalAuthFactorsScreenTestWithRecovery,
    ::testing::ValuesIn({UserType::kLocalPasswordUser, UserType::kPinUser,
                         UserType::kLocalPasswordAndPinUser,
                         UserType::kGaiaPasswordAndPinUser}));

// Instantiate the parameterized tests for users with local passwords.
INSTANTIATE_TEST_SUITE_P(
    RemoveLocalAuthFactorsScreenTestWithLocalPasswordInstantiation,
    RemoveLocalAuthFactorsScreenTestWithLocalPassword,
    ::testing::ValuesIn({UserType::kLocalPasswordUser,
                         UserType::kLocalPasswordAndPinUser}));

IN_PROC_BROWSER_TEST_F(RemoveLocalAuthFactorsScreenTest, PRE_SkipForSAMLUser) {
  LoginOfflineAndSetPolicy(local_password_and_pin_user_.account_id);
  user_manager::KnownUser(g_browser_process->local_state())
      .UpdateUsingSAML(local_password_and_pin_user_.account_id, true);
}

IN_PROC_BROWSER_TEST_F(RemoveLocalAuthFactorsScreenTest, SkipForSAMLUser) {
  // Test Setup is handled by the PRE_ test above.

  // Test Execution: Re-authenticate the user

  ReauthUserWithLocalPassword(local_password_and_pin_user_.account_id);

  // Test Verification: Should bypass the screen and start session.
  login_manager_mixin_.WaitForActiveSession();
}

}  // namespace ash
