// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/per_user_state_manager_chromeos.h"

#include "base/run_loop.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/guest_session_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_exit_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screens_utils.h"
#include "chrome/browser/ash/login/test/scoped_policy_update.h"
#include "chrome/browser/ash/login/test/user_policy_mixin.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash_factory.h"
#include "chrome/browser/ash/policy/core/device_policy_builder.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/ash/policy/test_support/embedded_policy_test_server_mixin.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/ash/settings/stats_reporting_controller.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/metrics/chrome_metrics_service_client.h"
#include "chrome/browser/metrics/profile_pref_names.h"
#include "chrome/browser/metrics/testing/metrics_reporting_pref_helper.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/webui/ash/login/consolidated_consent_screen_handler.h"
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

// Create fake test users in order to test different owner and secondary user
// consent flows.
constexpr char kTestUser1[] = "test-user1@gmail.com";
constexpr char kTestUser1GaiaId[] = "1111111111";
constexpr char kTestUser2[] = "test-user2@gmail.com";
constexpr char kTestUser2GaiaId[] = "2222222222";

}  // namespace

// Use LoginManagerTest to mimic actual Chrome OS users being logged in and
// out.
class ChromeOSPerUserMetricsBrowserTestBase : public ash::LoginManagerTest {
 public:
  ChromeOSPerUserMetricsBrowserTestBase() = default;
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
};

class ChromeOSPerUserRegularUserTest
    : public ChromeOSPerUserMetricsBrowserTestBase,
      public ::testing::WithParamInterface<std::pair<bool, bool>> {
 public:
  ChromeOSPerUserRegularUserTest()
      : owner_key_util_(new ownership::MockOwnerKeyUtil()) {
    login_mixin_.AppendRegularUsers(1);
  }
  ~ChromeOSPerUserRegularUserTest() override = default;

  ash::LoginManagerMixin login_mixin_{&mixin_host_, {}, &fake_gaia_};
  AccountId account_id_{
      AccountId::FromUserEmailGaiaId(kTestUser1, kTestUser1GaiaId)};

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

  FakeGaiaMixin fake_gaia_{&mixin_host_};
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

  // Secondary user consent is initially disabled until after user consent is
  // given.
  EXPECT_FALSE(GetLocalStateMetricsConsent());

  // Try and toggle user metrics consent to |user_consent_|.
  ChangeUserMetricsConsent(user_consent_);

  // Propagating metrics consent through the services happens async.
  base::RunLoop().RunUntilIdle();

  // User metrics consent is independent of owners consent.
  EXPECT_THAT(GetLocalStateMetricsConsent(), Eq(user_consent_));

  // User-Id will be set if user consent is enabled.
  EXPECT_THAT(
      g_browser_process->metrics_service()->GetCurrentUserId().has_value(),
      Eq(user_consent_));
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

  // Secondary users always set ephemeral partition.
  EXPECT_TRUE(log_store->has_alternate_ongoing_log_store());

  // Guests should have a user id if guest consent is set.
  EXPECT_EQ(metrics_service->GetCurrentUserId().has_value(), guest_consent);

  // Device settings consent should remain disabled since this is a guest
  // session.
  EXPECT_FALSE(ash::StatsReportingController::Get()->IsEnabled());
}

INSTANTIATE_TEST_SUITE_P(MetricsConsentForGuestWithNoOwner,
                         ChromeOSPerUserGuestUserWithNoOwnerTest,
                         ::testing::Bool());

class ChromeOSPerUserOobeConsentTest : public ash::OobeBaseTest {
 public:
  ChromeOSPerUserOobeConsentTest() = default;

  void SetUpOnMainThread() override {
    ash::LoginDisplayHost::default_host()
        ->GetWizardContext()
        ->is_branded_build = true;

    ash::OobeBaseTest::SetUpOnMainThread();
  }

  ChromeOSPerUserOobeConsentTest(const ChromeOSPerUserOobeConsentTest&) =
      delete;
  ChromeOSPerUserOobeConsentTest& operator=(
      const ChromeOSPerUserOobeConsentTest&) = delete;
  ~ChromeOSPerUserOobeConsentTest() override = default;

  void ValidateCurrentUser(const user_manager::User* user) {}

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

  bool GetLocalStateMetricsConsent() const {
    return g_browser_process->local_state()->GetBoolean(
        prefs::kMetricsReportingEnabled);
  }

  ash::LoginManagerMixin login_manager_mixin_{&mixin_host_, {}, &fake_gaia_};

 private:
  FakeGaiaMixin fake_gaia_{&mixin_host_};
};

IN_PROC_BROWSER_TEST_F(ChromeOSPerUserOobeConsentTest,
                       PRE_OwnerConsentAndRegularSecondaryConsent) {
  // Create and login as device owner.
  auto context = ash::LoginManagerMixin::CreateDefaultUserContext(
      ash::LoginManagerMixin::TestUserInfo(
          AccountId::FromUserEmailGaiaId(kTestUser1, kTestUser1GaiaId)));
  login_manager_mixin_.LoginAsNewRegularUser(context);
  ash::OobeScreenExitWaiter(ash::OobeBaseTest::GetFirstSigninScreen()).Wait();

  // These are enough to test OOBE consent enabled/disabled.
  ash::test::WaitForConsolidatedConsentScreen();

  base::RunLoop().RunUntilIdle();

  // Device owner has not consented to reporting in OOBE flow yet.
  EXPECT_FALSE(GetLocalStateMetricsConsent());

  // Device owner accepted consent.
  ash::test::TapConsolidatedConsentAccept();
  ash::test::WaitForSyncConsentScreen();

  // Device owner has consented to reporting in OOBE.
  EXPECT_TRUE(GetLocalStateMetricsConsent());
}

IN_PROC_BROWSER_TEST_F(ChromeOSPerUserOobeConsentTest,
                       OwnerConsentAndRegularSecondaryConsent) {
  auto context = ash::LoginManagerMixin::CreateDefaultUserContext(
      ash::LoginManagerMixin::TestUserInfo(
          AccountId::FromUserEmailGaiaId(kTestUser2, kTestUser2GaiaId)));
  login_manager_mixin_.LoginAsNewRegularUser(context);
  ash::OobeScreenExitWaiter(ash::OobeBaseTest::GetFirstSigninScreen()).Wait();

  // Secondary user has not consented to reporting in OOBE flow yet.
  EXPECT_FALSE(GetLocalStateMetricsConsent());

  ash::test::WaitForConsolidatedConsentScreen();
  ash::test::TapConsolidatedConsentAccept();
  ash::test::WaitForSyncConsentScreen();

  // Secondary user was able to consent.
  EXPECT_TRUE(GetLocalStateMetricsConsent());

  // Try and disable regular user metrics consent.
  // Propagating metrics consent through the services happens async.
  ChangeUserMetricsConsent(false);
  base::RunLoop().RunUntilIdle();

  // User metrics consent is independent of owners consent.
  // The local state metrics consent represents the current active users
  // consent. Owner consent is managed by |ash::StatsReportingController|.
  EXPECT_TRUE(ash::StatsReportingController::Get()->IsEnabled());
  EXPECT_FALSE(GetLocalStateMetricsConsent());

  // User-Id will not be set if user consent is disabled.
  EXPECT_THAT(
      g_browser_process->metrics_service()->GetCurrentUserId().has_value(),
      Eq(false));
}

// Device owner consent is managed by |ash::StatsReportingController|.
// Test verifies that the |PerUserStateManagerChromeOS| allows control of
// secondary user consent regardless of the owners consent.
IN_PROC_BROWSER_TEST_F(ChromeOSPerUserOobeConsentTest,
                       DeviceOwnerDoesNotUsePerUserConsent) {
  auto context = ash::LoginManagerMixin::CreateDefaultUserContext(
      ash::LoginManagerMixin::TestUserInfo(
          AccountId::FromUserEmailGaiaId(kTestUser1, kTestUser1GaiaId),
          ash::test::kDefaultAuthSetup, user_manager::UserType::kRegular));
  login_manager_mixin_.LoginAsNewRegularUser(context);
  ash::OobeScreenExitWaiter(ash::OobeBaseTest::GetFirstSigninScreen()).Wait();

  // Device owner has not consented to reporting in OOBE flow yet.
  EXPECT_FALSE(GetLocalStateMetricsConsent());

  ash::test::WaitForConsolidatedConsentScreen();
  ash::test::TapConsolidatedConsentAccept();
  ash::test::WaitForSyncConsentScreen();

  // Owner user consented during OOBE.
  EXPECT_TRUE(GetLocalStateMetricsConsent());

  // User-Id is never set for device owner.
  EXPECT_THAT(
      g_browser_process->metrics_service()->GetCurrentUserId().has_value(),
      Eq(false));

  // Try and disable device owner user metrics consent.
  // Device owner should not controller by per user metrics consent.
  // Propagating metrics consent through the services happens async.
  ChangeUserMetricsConsent(false);
  base::RunLoop().RunUntilIdle();

  // Owner consent cannot be updated by ChangeUserMetricsConsent since device
  // owner consent is not controller by per user logic.
  EXPECT_TRUE(GetLocalStateMetricsConsent());

  // User-Id is never set for device owner.
  EXPECT_THAT(
      g_browser_process->metrics_service()->GetCurrentUserId().has_value(),
      Eq(false));
}

class ChromeOSPerUserManagedOobeConsentTest
    : public ash::OobeBaseTest,
      public ::testing::WithParamInterface<bool> {
 public:
  ChromeOSPerUserManagedOobeConsentTest() = default;

  void SetUpOnMainThread() override {
    ash::LoginDisplayHost::default_host()
        ->GetWizardContext()
        ->is_branded_build = true;

    ash::OobeBaseTest::SetUpOnMainThread();

    // Sets ownership.
    device_policy_helper_.InstallOwnerKey();

    // Configure device policy.
    auto* device_policy = device_policy_helper_.device_policy();
    device_policy->payload().mutable_metrics_enabled()->set_metrics_enabled(
        GetParam());
    device_policy_helper_.RefreshDevicePolicy();

    // Wait for device policies to be synced.
    base::RunLoop().RunUntilIdle();
  }

  bool SetUpUserDataDirectory() override {
    base::FilePath local_state_path =
        metrics::SetUpUserDataDirectoryForTesting(true);
    return !local_state_path.empty();
  }

  ChromeOSPerUserManagedOobeConsentTest(
      const ChromeOSPerUserManagedOobeConsentTest&) = delete;
  ChromeOSPerUserManagedOobeConsentTest& operator=(
      const ChromeOSPerUserManagedOobeConsentTest&) = delete;
  ~ChromeOSPerUserManagedOobeConsentTest() override = default;

  void ValidateCurrentUser(const user_manager::User* user) {}

  // Assumes that a user has logged in.
  void ChangeUserMetricsConsent(bool user_metrics_consent) {
    g_browser_process->metrics_service()->UpdateCurrentUserMetricsConsent(
        user_metrics_consent);
  }

  bool GetLocalStateMetricsConsent() const {
    return g_browser_process->local_state()->GetBoolean(
        prefs::kMetricsReportingEnabled);
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

 private:
  FakeGaiaMixin fake_gaia_{&mixin_host_};
  const ash::LoginManagerMixin::TestUserInfo managed_user_{
      AccountId::FromUserEmailGaiaId(kTestUser1, kTestUser1GaiaId)};
  ash::LoginManagerMixin login_mixin_{&mixin_host_,
                                      {managed_user_},
                                      &fake_gaia_};
  ash::UserPolicyMixin user_policy_mixin_{&mixin_host_,
                                          managed_user_.account_id};

  // Policy and device state.
  policy::DevicePolicyCrosTestHelper device_policy_helper_;
  ash::EmbeddedPolicyTestServerMixin policy_test_server_mixin_{&mixin_host_};
  ash::DeviceStateMixin device_state_{
      &mixin_host_,
      ash::DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
};

IN_PROC_BROWSER_TEST_P(ChromeOSPerUserManagedOobeConsentTest,
                       ManagedDeviceDoesNotUsePerUserConsent) {
  PerUserStateManagerChromeOS::SetIsManagedForTesting(true);

  auto* metrics_service = g_browser_process->metrics_service();
  MetricsLogStore* log_store = metrics_service->LogStoreForTest();

  // Pre-login state.
  bool policy_consent = GetParam();
  EXPECT_EQ(ash::StatsReportingController::Get()->IsEnabled(), policy_consent);
  EXPECT_EQ(GetLocalStateMetricsConsent(), policy_consent);
  EXPECT_FALSE(log_store->has_alternate_ongoing_log_store());

  LoginManagedUser();
  ash::OobeScreenExitWaiter(ash::OobeBaseTest::GetFirstSigninScreen()).Wait();

  // Post-login state.
  EXPECT_THAT(user_manager::UserManager::Get()->GetActiveUser()->GetType(),
              Eq(user_manager::UserType::kRegular));
  EXPECT_TRUE(log_store->has_alternate_ongoing_log_store());

  // Should still follow policy_consent.
  EXPECT_EQ(GetLocalStateMetricsConsent(), policy_consent);

  // Users should not have a user id since they do not have control over the
  // metrics consent.
  EXPECT_THAT(metrics_service->GetCurrentUserId(), Eq(std::nullopt));

  // Try to change the user consent.
  metrics_service->UpdateCurrentUserMetricsConsent(!policy_consent);

  // Managed device users cannot control metrics consent.
  EXPECT_EQ(GetLocalStateMetricsConsent(), policy_consent);
}

INSTANTIATE_TEST_SUITE_P(ManagedDeviceDoesNotUsePerUserConsent,
                         ChromeOSPerUserManagedOobeConsentTest,
                         ::testing::Bool());

}  // namespace metrics
