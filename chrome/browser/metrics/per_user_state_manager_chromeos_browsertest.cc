// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/per_user_state_manager_chromeos.h"

#include "ash/constants/ash_features.h"
#include "base/run_loop.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/embedded_policy_test_server_mixin.h"
#include "chrome/browser/ash/login/test/guest_session_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/scoped_policy_update.h"
#include "chrome/browser/ash/login/test/user_policy_mixin.h"
#include "chrome/browser/ash/login/users/chrome_user_manager.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash_factory.h"
#include "chrome/browser/ash/policy/core/device_policy_builder.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/ash/settings/stats_reporting_controller.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/metrics/chrome_metrics_service_client.h"
#include "chrome/browser/metrics/profile_pref_names.h"
#include "chrome/browser/metrics/testing/metrics_reporting_pref_helper.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "components/metrics/metrics_log_store.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/metrics_switches.h"
#include "components/ownership/mock_owner_key_util.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_names.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {

namespace {

using ::testing::Eq;
using ::testing::Ne;

}  // namespace

// Use LoginManagerTest to mimic actual Chrome OS users being logged in and
// out.
class ChromeOSPerUserMetricsBrowserTestBase : public ash::LoginManagerTest {
 public:
  ChromeOSPerUserMetricsBrowserTestBase() {
    feature_list_.InitAndEnableFeature(::ash::features::kPerUserMetrics);
  }
  ~ChromeOSPerUserMetricsBrowserTestBase() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    EnableMetricsRecordingOnlyForTesting(command_line);
    ash::LoginManagerTest::SetUpCommandLine(command_line);
  }

  void Initialize() {
    // Metrics service must explicitly be told to start recording or nothing
    // will happen.
    g_browser_process->metrics_service()->StartRecordingForTests();

    // Wait for metrics service to finish initializing.
    base::RunLoop().RunUntilIdle();
  }

  // Assumes that a user has logged in.
  void ChangeUserMetricsConsent(bool user_metrics_consent) {
    g_browser_process->metrics_service()->UpdateCurrentUserMetricsConsent(
        user_metrics_consent);
  }

  void CreatedBrowserMainParts(content::BrowserMainParts* parts) override {
    LoginManagerTest::CreatedBrowserMainParts(parts);
    // IsMetricsReportingEnabled() in non-official builds always returns false.
    // Force to check the pref in order to test proper reporting consent.
    ChromeMetricsServiceAccessor::SetForceIsMetricsReportingEnabledPrefLookup(
        true);
  }

  bool GetLocalStateMetricsConsent() const {
    return g_browser_process->local_state()->GetBoolean(
        prefs::kMetricsReportingEnabled);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

class ChromeOSPerUserRegularUserTest
    : public ChromeOSPerUserMetricsBrowserTestBase,
      public ::testing::WithParamInterface<std::pair<bool, bool>> {
 public:
  ChromeOSPerUserRegularUserTest()
      : owner_key_util_(new ownership::MockOwnerKeyUtil()) {
    login_mixin_.AppendRegularUsers(1);
    account_id_ = login_mixin_.users()[0].account_id;
  }
  ~ChromeOSPerUserRegularUserTest() override = default;

 protected:
  void SetUpInProcessBrowserTestFixture() override {
    ChromeOSPerUserMetricsBrowserTestBase::SetUpInProcessBrowserTestFixture();
    owner_consent_ = GetParam().first;
    user_consent_ = GetParam().second;

    // Set the owner parameter.
    test_cros_settings_.device_settings()->SetBoolean(ash::kStatsReportingPref,
                                                      owner_consent_);

    // Establish ownership of the device.
    ash::OwnerSettingsServiceAshFactory::GetInstance()
        ->SetOwnerKeyUtilForTesting(owner_key_util_);
    owner_key_util_->SetPublicKeyFromPrivateKey(
        *policy_helper_.device_policy()->GetSigningKey());
  }

  scoped_refptr<ownership::MockOwnerKeyUtil> owner_key_util_;
  policy::DevicePolicyCrosTestHelper policy_helper_;
  ash::ScopedTestingCrosSettings test_cros_settings_;
  ash::DeviceStateMixin device_state_{
      &mixin_host_,
      ash::DeviceStateMixin::State::OOBE_COMPLETED_CONSUMER_OWNED};

  bool owner_consent_ = false;
  bool user_consent_ = false;

  ash::LoginManagerMixin login_mixin_{&mixin_host_};
  AccountId account_id_;
};

IN_PROC_BROWSER_TEST_P(ChromeOSPerUserRegularUserTest,
                       MetricsConsentForRegularUser) {
  Initialize();

  LoginUser(account_id_);
  base::RunLoop().RunUntilIdle();

  MetricsLogStore* log_store =
      g_browser_process->metrics_service()->LogStoreForTest();

  // Should be using alternate ongoing log store for regular users regardless of
  // owner consent.
  EXPECT_TRUE(log_store->has_alternate_ongoing_log_store());

  // Since new user consent inherits owner consent initially, this should be the
  // same as owner consent.
  EXPECT_THAT(GetLocalStateMetricsConsent(), Eq(owner_consent_));

  // Try and toggle user metrics consent to |user_consent_|.
  ChangeUserMetricsConsent(user_consent_);

  // Propagating metrics consent through the services happens async.
  base::RunLoop().RunUntilIdle();

  // If owner consent is on, then user metrics consent should be respected and
  // metrics service should be toggled off. If owner consent is off, user
  // metrics consent should not be changeable and no-op to respect that device
  // owner consent is off.
  if (owner_consent_)
    EXPECT_THAT(GetLocalStateMetricsConsent(), Eq(user_consent_));
  else
    EXPECT_FALSE(GetLocalStateMetricsConsent());

  // Users should only have a user ID if both owner consent and user consent are
  // on.
  EXPECT_THAT(
      g_browser_process->metrics_service()->GetCurrentUserId().has_value(),
      Eq(user_consent_ && owner_consent_));
}

INSTANTIATE_TEST_SUITE_P(MetricsConsentForRegularUser,
                         ChromeOSPerUserRegularUserTest,
                         testing::ValuesIn({
                             std::make_pair(true, true),
                             std::make_pair(true, false),
                             std::make_pair(false, true),
                             std::make_pair(false, false),
                         }));

class ChromeOSPerUserGuestUserWithNoOwnerTest
    : public ChromeOSPerUserMetricsBrowserTestBase,
      public ::testing::WithParamInterface<bool> {
 public:
  ChromeOSPerUserGuestUserWithNoOwnerTest() = default;
  ~ChromeOSPerUserGuestUserWithNoOwnerTest() override = default;

 protected:
  ash::GuestSessionMixin guest_session_{&mixin_host_};
};

IN_PROC_BROWSER_TEST_P(ChromeOSPerUserGuestUserWithNoOwnerTest,
                       MetricsConsentForGuestWithNoOwner) {
  Initialize();

  auto* metrics_service = g_browser_process->metrics_service();
  MetricsLogStore* log_store = metrics_service->LogStoreForTest();

  // Device consent should be false if device is not owned.
  EXPECT_FALSE(ash::StatsReportingController::Get()->IsEnabled());
  EXPECT_FALSE(GetLocalStateMetricsConsent());

  bool guest_consent = GetParam();
  ChangeUserMetricsConsent(guest_consent);

  // Propagating metrics consent through the services happens async.
  base::RunLoop().RunUntilIdle();

  // Once consent is set for the first time, log store should be set
  // appropriately. Log store should be the inverse of the first consent since
  // consent means that log store used should be local state.
  EXPECT_THAT(GetLocalStateMetricsConsent(), Eq(guest_consent));

  // No owner means that ephemeral partition should always be used.
  EXPECT_TRUE(log_store->has_alternate_ongoing_log_store());

  // Guests do not have a user id.
  EXPECT_THAT(metrics_service->GetCurrentUserId(), Eq(absl::nullopt));

  // Device settings consent should remain disabled since this is a guest
  // session.
  EXPECT_FALSE(ash::StatsReportingController::Get()->IsEnabled());
}

INSTANTIATE_TEST_SUITE_P(MetricsConsentForGuestWithNoOwner,
                         ChromeOSPerUserGuestUserWithNoOwnerTest,
                         ::testing::Bool());

class ChromeOSPerUserGuestTestWithDeviceOwner
    : public ChromeOSPerUserMetricsBrowserTestBase,
      public ::testing::WithParamInterface<bool> {
 public:
  ChromeOSPerUserGuestTestWithDeviceOwner()
      : owner_key_util_(new ownership::MockOwnerKeyUtil()) {}
  ~ChromeOSPerUserGuestTestWithDeviceOwner() override = default;

 protected:
  void SetUpInProcessBrowserTestFixture() override {
    ChromeOSPerUserMetricsBrowserTestBase::SetUpInProcessBrowserTestFixture();

    // Set the owner parameter.
    test_cros_settings_.device_settings()->SetBoolean(ash::kStatsReportingPref,
                                                      GetParam());

    // Establish ownership of the device.
    ash::OwnerSettingsServiceAshFactory::GetInstance()
        ->SetOwnerKeyUtilForTesting(owner_key_util_);
    owner_key_util_->SetPublicKeyFromPrivateKey(
        *policy_helper_.device_policy()->GetSigningKey());
  }

  ash::GuestSessionMixin guest_session_mixin_{&mixin_host_};

  scoped_refptr<ownership::MockOwnerKeyUtil> owner_key_util_;
  policy::DevicePolicyCrosTestHelper policy_helper_;
  ash::ScopedTestingCrosSettings test_cros_settings_;
  ash::DeviceStateMixin device_state_{
      &mixin_host_,
      ash::DeviceStateMixin::State::OOBE_COMPLETED_CONSUMER_OWNED};
};

IN_PROC_BROWSER_TEST_P(ChromeOSPerUserGuestTestWithDeviceOwner,
                       MetricsConsentForGuestWithOwner) {
  Initialize();
  bool owner_consent = GetParam();

  EXPECT_THAT(user_manager::UserManager::Get()->GetActiveUser()->GetType(),
              Eq(user_manager::USER_TYPE_GUEST));
  EXPECT_THAT(ash::DeviceSettingsService::Get()->GetOwnershipStatus(),
              Eq(ash::DeviceSettingsService::OWNERSHIP_TAKEN));

  // Ensure that guest session is using owner consent.
  EXPECT_THAT(ash::StatsReportingController::Get()->IsEnabled(),
              Eq(owner_consent));
  EXPECT_THAT(GetLocalStateMetricsConsent(), Eq(owner_consent));

  auto* metrics_service = g_browser_process->metrics_service();
  MetricsLogStore* log_store = metrics_service->LogStoreForTest();

  // Alternate ongoing log store should not be set if owner consent is true.
  // Guest session cryptohome is ephemeral, so we want persistent metrics logs
  // to be in the local store.
  EXPECT_THAT(log_store->has_alternate_ongoing_log_store(), Ne(owner_consent));

  // Guests do not have a user id.
  EXPECT_THAT(metrics_service->GetCurrentUserId(), Eq(absl::nullopt));
}

INSTANTIATE_TEST_SUITE_P(MetricsConsentForGuestWithOwner,
                         ChromeOSPerUserGuestTestWithDeviceOwner,
                         ::testing::Bool());

class ChromeOSPerUserManagedDeviceTest
    : public ChromeOSPerUserMetricsBrowserTestBase,
      public ::testing::WithParamInterface<bool> {
 public:
  ChromeOSPerUserManagedDeviceTest() = default;

  bool SetUpUserDataDirectory() override {
    base::FilePath local_state_path =
        metrics::SetUpUserDataDirectoryForTesting(true);
    return !local_state_path.empty();
  }

  void SetUpInProcessBrowserTestFixture() override {
    ChromeOSPerUserMetricsBrowserTestBase::SetUpInProcessBrowserTestFixture();

    // Sets ownership.
    device_policy_helper_.InstallOwnerKey();

    // Configure device policy.
    auto* device_policy = device_policy_helper_.device_policy();
    device_policy->payload().mutable_metrics_enabled()->set_metrics_enabled(
        GetParam());
    device_policy_helper_.RefreshDevicePolicy();
  }

  void LoginManagedUser() {
    user_policy_mixin_.RequestPolicyUpdate();

    auto context =
        ash::LoginManagerMixin::CreateDefaultUserContext(managed_user_);
    login_mixin_.LoginAndWaitForActiveSession(context);
    EXPECT_EQ(user_manager::UserManager::Get()->GetLoggedInUsers().size(), 1u);
    EXPECT_EQ(session_manager::SessionManager::Get()->session_state(),
              session_manager::SessionState::ACTIVE);
  }

 protected:
  const ash::LoginManagerMixin::TestUserInfo managed_user_{
      AccountId::FromUserEmailGaiaId("user@fake-domain.com", "11")};
  ash::LoginManagerMixin login_mixin_{&mixin_host_, {managed_user_}};
  ash::UserPolicyMixin user_policy_mixin_{&mixin_host_,
                                          managed_user_.account_id};

  // Policy and device state.
  policy::DevicePolicyCrosTestHelper device_policy_helper_;
  ash::EmbeddedPolicyTestServerMixin policy_test_server_mixin_{&mixin_host_};
  ash::DeviceStateMixin device_state_{
      &mixin_host_,
      ash::DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
};

IN_PROC_BROWSER_TEST_P(ChromeOSPerUserManagedDeviceTest,
                       MetricsConsentForManagedUsers) {
  Initialize();

  bool policy_consent = GetParam();
  PerUserStateManagerChromeOS::SetIsManagedForTesting(policy_consent);

  auto* metrics_service = g_browser_process->metrics_service();
  MetricsLogStore* log_store = metrics_service->LogStoreForTest();

  // Pre-login state.
  EXPECT_EQ(ash::StatsReportingController::Get()->IsEnabled(), policy_consent);
  EXPECT_EQ(GetLocalStateMetricsConsent(), policy_consent);
  EXPECT_FALSE(log_store->has_alternate_ongoing_log_store());

  LoginManagedUser();

  // Post-login state.
  EXPECT_THAT(user_manager::UserManager::Get()->GetActiveUser()->GetType(),
              Eq(user_manager::USER_TYPE_REGULAR));
  EXPECT_TRUE(log_store->has_alternate_ongoing_log_store());

  // Should still follow policy_consent.
  EXPECT_EQ(GetLocalStateMetricsConsent(), policy_consent);

  // Users should not have a user id since they do not have control over the
  // metrics consent.
  EXPECT_THAT(metrics_service->GetCurrentUserId(), Eq(absl::nullopt));

  // Try to change the user consent.
  metrics_service->UpdateCurrentUserMetricsConsent(!policy_consent);

  // Managed device users cannot control metrics consent.
  EXPECT_EQ(GetLocalStateMetricsConsent(), policy_consent);
}

INSTANTIATE_TEST_SUITE_P(MetricsConsentForManagedUsers,
                         ChromeOSPerUserManagedDeviceTest,
                         ::testing::Bool());

}  // namespace metrics
