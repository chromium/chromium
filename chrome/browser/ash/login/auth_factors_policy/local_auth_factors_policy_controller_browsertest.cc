// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/auth_factors_policy/local_auth_factors_policy_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/login_screen_test_api.h"
#include "ash/public/cpp/reauth_reason.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "chrome/browser/ash/login/auth_factors_policy/local_auth_factors_policy_controller_factory.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/reauth_stats.h"
#include "chrome/browser/ash/login/signin/signin_error_notifier.h"
#include "chrome/browser/ash/login/test/cryptohome_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/user_auth_config.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
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
#include "ui/message_center/public/cpp/notification.h"

namespace ash {

namespace {

using test::UserAuthConfig;

constexpr char kComplexityUpdateNotificationId[] =
    "local_auth_factors_policy_controller.complexity_update";

}  // namespace

class LocalAuthFactorsPolicyControllerTest : public LoginManagerTest {
 public:
  LocalAuthFactorsPolicyControllerTest() {
    LocalAuthFactorsPolicyController::SetPrefProcessedCallbackForTesting(
        on_pref_processed_future_.GetRepeatingCallback());
    LocalAuthFactorsPolicyController::SetNotificationShownCallbackForTesting(
        on_notification_shown_future_.GetRepeatingCallback());
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

  void WaitForNotificationShownCallback() {
    ASSERT_TRUE(on_notification_shown_future_.WaitAndClear())
        << "Failed waiting for complexity update notification shown callback";
  }

  void WaitForNotificationClosedCallback() {
    ASSERT_TRUE(on_notification_closed_future_.WaitAndClear())
        << "Failed waiting for complexity update notification closed callback";
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

  void OnFactorChanged(const AccountId& account_id,
                       ash::auth::mojom::AuthFactor factor) {
    LocalAuthFactorsPolicyController* controller =
        LocalAuthFactorsPolicyControllerFactory::GetForProfile(
            GetProfile(account_id));
    CHECK(controller);
    controller->OnFactorChanged(factor);
  }

  void SetComplexityPolicy(LocalAuthFactorsComplexity complexity) {
    policy_map_.Set(policy::key::kLocalAuthFactorsComplexity,
                    policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                    policy::POLICY_SOURCE_CLOUD,
                    base::Value(static_cast<int>(complexity)), nullptr);
    UpdatePolicy();
  }

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
    DisableAllAllowedAuthFactorsPolicy();
    ClearPendingPrefProcessedCallback();
    LoginUser(user.account_id);
    if (LocalAuthFactorsPolicyControllerFactory::GetForProfile(
            GetProfile(user.account_id))) {
      WaitForPrefProcessedCallback();
    }
    AssertReauthReason(user.account_id, expected_reason);
  }

  Profile* GetProfile(const AccountId& account_id) {
    return ash::ProfileHelper::Get()->GetProfileByAccountId(account_id);
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

  const LoginManagerMixin::TestUserInfo saml_user_{
      LoginManagerMixin::CreateEnterpriseAccountId(6),
      UserAuthConfig::Create({AshAuthFactor::kLocalPassword})
          .RequireReauth(/*require_reauth=*/false)};

  const LoginManagerMixin::TestUserInfo unmanaged_user_{
      LoginManagerMixin::CreateConsumerAccountId(7),
      UserAuthConfig::Create({AshAuthFactor::kGaiaPassword})
          .RequireReauth(/*require_reauth=*/false)};

  base::test::TestFuture<void> on_pref_processed_future_;
  base::test::TestFuture<void> on_notification_shown_future_;
  base::test::TestFuture<void> on_notification_closed_future_;
  CryptohomeMixin cryptohome_{&mixin_host_};
  FakeGaiaMixin fake_gaia_{&mixin_host_};
  LoginManagerMixin login_mixin_{
      &mixin_host_,
      {local_password_and_pin_user_, pin_only_user_, local_password_only_user_,
       gaia_password_and_pin_user_, gaia_password_user_, saml_user_},
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
  base::HistogramTester histogram_tester;
  PerformLoginAndVerifyPolicyEnforcement(
      local_password_and_pin_user_,
      ReauthReason::kForcedByLocalAuthFactorsPolicy);
  histogram_tester.ExpectBucketCount(
      "Enterprise.LocalAuthFactorsPolicy.ForcedReauth", true, 1);
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
  base::HistogramTester histogram_tester;
  PerformLoginAndVerifyPolicyEnforcement(gaia_password_user_,
                                         ReauthReason::kNone);
  histogram_tester.ExpectBucketCount(
      "Enterprise.LocalAuthFactorsPolicy.ForcedReauth", false, 1);
}

IN_PROC_BROWSER_TEST_F(LocalAuthFactorsPolicyControllerTest,
                       NoForceReauthForGaiaPassword) {
  EXPECT_FALSE(
      LoginScreenTestApi::IsForcedOnlineSignin(gaia_password_user_.account_id));
}

IN_PROC_BROWSER_TEST_F(LocalAuthFactorsPolicyControllerTest,
                       PRE_NoForceReauthForUnmanagedGaiaPassword) {
  base::HistogramTester histogram_tester;
  PerformLoginAndVerifyPolicyEnforcement(unmanaged_user_, ReauthReason::kNone);
  histogram_tester.ExpectBucketCount(
      "Enterprise.LocalAuthFactorsPolicy.ForcedReauth", false, 0);
  histogram_tester.ExpectBucketCount(
      "Enterprise.LocalAuthFactorsPolicy.ForcedReauth", true, 0);
}

IN_PROC_BROWSER_TEST_F(LocalAuthFactorsPolicyControllerTest,
                       NoForceReauthForUnmanagedGaiaPassword) {
  // Reauth might be forced for other reasons (e.g. invalid tokens in tests),
  // but it should NOT be ReauthReason::kForcedByLocalAuthFactorsPolicy.
  auto known_user = user_manager::KnownUser(g_browser_process->local_state());
  auto actual_reason = static_cast<ReauthReason>(
      known_user.FindReauthReason(unmanaged_user_.account_id)
          .value_or(static_cast<int>(ReauthReason::kNone)));
  EXPECT_NE(actual_reason, ReauthReason::kForcedByLocalAuthFactorsPolicy);
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

IN_PROC_BROWSER_TEST_F(LocalAuthFactorsPolicyControllerTest,
                       ShowComplexityUpdateNotificationOnPolicyUpdate) {
  base::HistogramTester histogram_tester;
  const AccountId& account_id = local_password_and_pin_user_.account_id;
  LoginUser(account_id);
  Profile* profile = GetProfile(account_id);
  NotificationDisplayServiceTester tester(profile);

  // Set complexity policy to Low.
  SetComplexityPolicy(LocalAuthFactorsComplexity::kLow);

  // The notification is shown after an async call to
  // GetAuthFactorsConfiguration.
  WaitForNotificationShownCallback();

  histogram_tester.ExpectBucketCount(
      "Enterprise.LocalAuthFactorsPolicy.PasswordComplexity",
      static_cast<int>(LocalAuthFactorsComplexity::kLow), 1);

  std::optional<message_center::Notification> notification =
      tester.GetNotification(
          "local_auth_factors_policy_controller.complexity_update");
  ASSERT_TRUE(notification.has_value());
  EXPECT_EQ(notification->title(), u"Change your PIN and password");
  EXPECT_EQ(notification->message(),
            u"Your administrator updated the security requirements for your "
            u"device");
}

IN_PROC_BROWSER_TEST_F(LocalAuthFactorsPolicyControllerTest,
                       NotificationPersistsUntilAllFactorsUpdated) {
  base::HistogramTester histogram_tester;
  const AccountId& account_id = local_password_and_pin_user_.account_id;
  LoginUser(account_id);
  Profile* profile = GetProfile(account_id);
  NotificationDisplayServiceTester tester(profile);
  tester.SetNotificationClosedClosure(
      on_notification_closed_future_.GetRepeatingCallback());

  // Set complexity policy to Low to trigger notification.
  SetComplexityPolicy(LocalAuthFactorsComplexity::kLow);

  WaitForNotificationShownCallback();

  {
    std::optional<message_center::Notification> notification =
        tester.GetNotification(kComplexityUpdateNotificationId);
    ASSERT_TRUE(notification.has_value());
    EXPECT_EQ(notification->title(), u"Change your PIN and password");
  }

  // Simulate updating only the password.
  OnFactorChanged(account_id, ash::auth::mojom::AuthFactor::kLocalPassword);

  // The notification should still be shown because PIN also needs update.
  WaitForNotificationShownCallback();
  {
    std::optional<message_center::Notification> notification =
        tester.GetNotification(kComplexityUpdateNotificationId);
    ASSERT_TRUE(notification.has_value());
    // Title should have updated to only mention PIN.
    EXPECT_EQ(notification->title(), u"Change your PIN");
  }

  // Simulate updating the PIN.
  OnFactorChanged(account_id, ash::auth::mojom::AuthFactor::kCryptohomePin);

  // Wait for the notification to be asynchronously dismissed.
  WaitForNotificationClosedCallback();

  // Notification should finally be dismissed.
  EXPECT_FALSE(
      tester.GetNotification(kComplexityUpdateNotificationId).has_value());

  histogram_tester.ExpectBucketCount(
      "Enterprise.LocalAuthFactorsPolicy.LocalAuthFactorChanged",
      static_cast<int>(LocalAuthFactorType::kLocalPassword), 1);
  histogram_tester.ExpectBucketCount(
      "Enterprise.LocalAuthFactorsPolicy.LocalAuthFactorChanged",
      static_cast<int>(LocalAuthFactorType::kPin), 1);
}

IN_PROC_BROWSER_TEST_F(LocalAuthFactorsPolicyControllerTest,
                       NoForceReauthForSAMLUser) {
  user_manager::KnownUser(g_browser_process->local_state())
      .UpdateUsingSAML(saml_user_.account_id, true);
  PerformLoginAndVerifyPolicyEnforcement(saml_user_, ReauthReason::kNone);
}

}  // namespace ash
