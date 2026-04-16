// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/auth_factors_policy/local_auth_factors_policy_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/login_screen_test_api.h"
#include "ash/public/cpp/reauth_reason.h"
#include "base/run_loop.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/reauth_stats.h"
#include "chrome/browser/ash/login/signin/signin_error_notifier.h"
#include "chrome/browser/ash/login/test/cryptohome_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/user_auth_config.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

namespace {

using test::UserAuthConfig;

}  // namespace

class LocalAuthFactorsPolicyControllerTest : public LoginManagerTest {
 public:
  LocalAuthFactorsPolicyControllerTest() {
    LocalAuthFactorsPolicyController::SetPrefProcessedCallbackForTesting(
        on_pref_processed_future_.GetRepeatingCallback());
    ignore_sync_errors_for_test_ =
        SigninErrorNotifier::IgnoreSyncErrorsForTesting();
  }
  ~LocalAuthFactorsPolicyControllerTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    LoginManagerTest::SetUpInProcessBrowserTestFixture();
    // Initialize user policy.
    provider_.SetDefaultReturns(/*is_initialization_complete_return=*/true,
                                /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    LoginManagerTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kAllowFailedPolicyFetchForTest);
  }

  void WaitForPrefProcessedCallback() {
    ASSERT_TRUE(on_pref_processed_future_.WaitAndClear())
        << "Failed waiting for local auth factor policy pref processed "
           "callback";
  }

  void ClearPendingPrefProcessedCallback() {
    on_pref_processed_future_.Clear();
  }

  void AssertReauthReason(AccountId account_id, ReauthReason expected_reason) {
    auto known_user = user_manager::KnownUser(g_browser_process->local_state());
    auto actual_reason = static_cast<ReauthReason>(
        known_user.FindReauthReason(account_id)
            .value_or(static_cast<int>(ReauthReason::kNone)));
    ASSERT_EQ(expected_reason, actual_reason);
  }

 protected:
  void UpdatePolicy() { provider_.UpdateChromePolicy(policy_map_); }

  void DisableAllAllowedAuthFactorsPolicy() {
    policy_map_.Set(policy::key::kAllowedLocalAuthFactors,
                    policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                    policy::POLICY_SOURCE_CLOUD,
                    base::Value(base::Value::Type::LIST), nullptr);
    UpdatePolicy();
  }

  void SetQuickUnlockAllowedModesPolicy(base::ListValue modes) {
    policy_map_.Set(policy::key::kQuickUnlockModeAllowlist,
                    policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                    policy::POLICY_SOURCE_CLOUD, base::Value(std::move(modes)),
                    nullptr);
    UpdatePolicy();
  }

  void PerformLoginAndVerifyPolicyEnforcement(
      const LoginManagerMixin::TestUserInfo& user,
      ReauthReason expected_reason) {
    LoginUser(user.account_id);
    ClearPendingPrefProcessedCallback();
    DisableAllAllowedAuthFactorsPolicy();
    WaitForPrefProcessedCallback();
    AssertReauthReason(user.account_id, expected_reason);
  }

  const LoginManagerMixin::TestUserInfo local_password_and_pin_user_{
      LoginManagerMixin::CreateEnterpriseAccountId(1),
      UserAuthConfig::Create(
          {AshAuthFactor::kLocalPassword, AshAuthFactor::kCryptohomePin})
          .RequireReauth(/*require_reauth=*/false)};

  const LoginManagerMixin::TestUserInfo pin_only_user_{
      LoginManagerMixin::CreateEnterpriseAccountId(2),
      UserAuthConfig::Create({AshAuthFactor::kCryptohomePin})
          .RequireReauth(/*require_reauth=*/false)};

  const LoginManagerMixin::TestUserInfo local_password_only_user_{
      LoginManagerMixin::CreateEnterpriseAccountId(3),
      UserAuthConfig::Create({AshAuthFactor::kLocalPassword})
          .RequireReauth(/*require_reauth=*/false)};

  const LoginManagerMixin::TestUserInfo gaia_password_and_pin_user_{
      LoginManagerMixin::CreateEnterpriseAccountId(4),
      UserAuthConfig::Create({
                                 AshAuthFactor::kGaiaPassword,
                                 AshAuthFactor::kCryptohomePin,
                             })
          .RequireReauth(/*require_reauth=*/false)};

  const LoginManagerMixin::TestUserInfo gaia_password_user_{
      LoginManagerMixin::CreateEnterpriseAccountId(5),
      UserAuthConfig::Create({AshAuthFactor::kGaiaPassword})
          .RequireReauth(/*require_reauth=*/false)};

  base::test::TestFuture<void> on_pref_processed_future_;
  CryptohomeMixin cryptohome_{&mixin_host_};
  FakeGaiaMixin fake_gaia_{&mixin_host_};
  LoginManagerMixin login_mixin_{
      &mixin_host_,
      {local_password_and_pin_user_, pin_only_user_, local_password_only_user_,
       gaia_password_and_pin_user_, gaia_password_user_},
      &fake_gaia_,
      &cryptohome_};
  std::unique_ptr<base::AutoReset<bool>> ignore_sync_errors_for_test_;
  testing::NiceMock<policy::MockConfigurationPolicyProvider> provider_;
  policy::PolicyMap policy_map_;
  base::test::ScopedFeatureList feature_list_{
      ash::features::kManagedLocalPinAndPassword};
};

IN_PROC_BROWSER_TEST_F(LocalAuthFactorsPolicyControllerTest,
                       PRE_ForceReauthForLocalPasswordAndPin) {
  PerformLoginAndVerifyPolicyEnforcement(
      local_password_and_pin_user_,
      ReauthReason::kForcedByLocalAuthFactorsPolicy);
}

IN_PROC_BROWSER_TEST_F(LocalAuthFactorsPolicyControllerTest,
                       ForceReauthForLocalPasswordAndPin) {
  EXPECT_TRUE(LoginScreenTestApi::IsForcedOnlineSignin(
      local_password_and_pin_user_.account_id));
}

IN_PROC_BROWSER_TEST_F(LocalAuthFactorsPolicyControllerTest,
                       PRE_ForceReauthForPinOnly) {
  PerformLoginAndVerifyPolicyEnforcement(
      pin_only_user_, ReauthReason::kForcedByLocalAuthFactorsPolicy);
}

IN_PROC_BROWSER_TEST_F(LocalAuthFactorsPolicyControllerTest,
                       ForceReauthForPinOnly) {
  EXPECT_TRUE(
      LoginScreenTestApi::IsForcedOnlineSignin(pin_only_user_.account_id));
}

IN_PROC_BROWSER_TEST_F(
    LocalAuthFactorsPolicyControllerTest,
    PRE_NoForceReauthForOnlinePasswordAndPinWhenPinAllowedByQuickUnlock) {
  PerformLoginAndVerifyPolicyEnforcement(gaia_password_and_pin_user_,
                                         ReauthReason::kNone);
}

IN_PROC_BROWSER_TEST_F(
    LocalAuthFactorsPolicyControllerTest,
    NoForceReauthForOnlinePasswordAndPinWhenPinAllowedByQuickUnlock) {
  EXPECT_FALSE(LoginScreenTestApi::IsForcedOnlineSignin(
      gaia_password_and_pin_user_.account_id));
}

IN_PROC_BROWSER_TEST_F(
    LocalAuthFactorsPolicyControllerTest,
    PRE_ForceReauthForOnlinePasswordAndPinWhenPinDisallowedByQuickUnlock) {
  base::ListValue quick_unlock_allowlist;
  // Disallow PIN by providing an empty allowlist.
  SetQuickUnlockAllowedModesPolicy(std::move(quick_unlock_allowlist));
  PerformLoginAndVerifyPolicyEnforcement(
      gaia_password_and_pin_user_,
      ReauthReason::kForcedByLocalAuthFactorsPolicy);
}

IN_PROC_BROWSER_TEST_F(
    LocalAuthFactorsPolicyControllerTest,
    ForceReauthForOnlinePasswordAndPinWhenPinDisallowedByQuickUnlock) {
  EXPECT_TRUE(LoginScreenTestApi::IsForcedOnlineSignin(
      gaia_password_and_pin_user_.account_id));
}

IN_PROC_BROWSER_TEST_F(LocalAuthFactorsPolicyControllerTest,
                       PRE_ForceReauthForLocalPasswordOnly) {
  PerformLoginAndVerifyPolicyEnforcement(
      local_password_only_user_, ReauthReason::kForcedByLocalAuthFactorsPolicy);
}

IN_PROC_BROWSER_TEST_F(LocalAuthFactorsPolicyControllerTest,
                       ForceReauthForLocalPasswordOnly) {
  EXPECT_TRUE(LoginScreenTestApi::IsForcedOnlineSignin(
      local_password_only_user_.account_id));
}

IN_PROC_BROWSER_TEST_F(LocalAuthFactorsPolicyControllerTest,
                       PRE_NoForceReauthForGaiaPassword) {
  PerformLoginAndVerifyPolicyEnforcement(gaia_password_user_,
                                         ReauthReason::kNone);
}

IN_PROC_BROWSER_TEST_F(LocalAuthFactorsPolicyControllerTest,
                       NoForceReauthForGaiaPassword) {
  EXPECT_FALSE(
      LoginScreenTestApi::IsForcedOnlineSignin(gaia_password_user_.account_id));
}

IN_PROC_BROWSER_TEST_F(LocalAuthFactorsPolicyControllerTest,
                       PRE_ReauthForcedWhenPinDisabledAfterPolicyEnforcement) {
  // Pin allowed by QuickUnlock, so we do not expect a Reauth.
  PerformLoginAndVerifyPolicyEnforcement(gaia_password_and_pin_user_,
                                         ReauthReason::kNone);
  // Now we disable PIN via QuickUnlock policy.
  ClearPendingPrefProcessedCallback();
  base::ListValue quick_unlock_allowlist;
  // Empty list means no modes allowed, so PIN is disabled.
  SetQuickUnlockAllowedModesPolicy(std::move(quick_unlock_allowlist));
  WaitForPrefProcessedCallback();

  // Now that PIN is disabled, it's no longer a safe secondary factor.
  // Reauth should be forced.
  AssertReauthReason(gaia_password_and_pin_user_.account_id,
                     ReauthReason::kForcedByLocalAuthFactorsPolicy);
}

IN_PROC_BROWSER_TEST_F(LocalAuthFactorsPolicyControllerTest,
                       ReauthForcedWhenPinDisabledAfterPolicyEnforcement) {
  EXPECT_TRUE(LoginScreenTestApi::IsForcedOnlineSignin(
      gaia_password_and_pin_user_.account_id));
}
}  // namespace ash
