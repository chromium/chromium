// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <string>

#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/login_screen_test_api.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/auto_reset.h"
#include "base/run_loop.h"
#include "chrome/browser/ash/app_mode/kiosk_test_helper.h"
#include "chrome/browser/ash/login/app_mode/test/kiosk_apps_mixin.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/embedded_test_server_setup_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screens_utils.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/browser/ash/login/test/user_policy_mixin.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/core/device_local_account_policy_service.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/ash/settings/stub_cros_settings_provider.h"
#include "chrome/browser/extensions/browsertest_util.h"
#include "chrome/browser/policy/messaging_layer/proto/synced/add_remove_user_event.pb.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/dbus/missive/missive_client.h"
#include "chromeos/dbus/missive/missive_client_test_observer.h"
#include "components/account_id/account_id.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_store.h"
#include "components/policy/core/common/device_local_account_type.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::chromeos::MissiveClientTestObserver;
using ::enterprise_management::ChromeDeviceSettingsProto;
using ::enterprise_management::DeviceLocalAccountInfoProto;
using ::reporting::Destination;
using ::reporting::Priority;
using ::reporting::Record;
using ::testing::Eq;
using ::testing::InvokeWithoutArgs;
using ::testing::StrEq;

namespace ash::reporting {
namespace {

constexpr char kTestUserEmail[] = "test@example.com";
constexpr char kTestAffiliationId[] = "test_affiliation_id";
constexpr char kPublicSessionUserEmail[] = "public_session_user@localhost";

Record GetNextUserAddedRemovedRecord(MissiveClientTestObserver* observer) {
  const std::tuple<Priority, Record>& enqueued_record =
      observer->GetNextEnqueuedRecord();
  Priority priority = std::get<0>(enqueued_record);
  Record record = std::get<1>(enqueued_record);

  EXPECT_THAT(priority, Eq(Priority::IMMEDIATE));
  return record;
}

std::optional<Record> MaybeGetEnqueuedUserAddedRemovedRecord() {
  const std::vector<Record>& records =
      chromeos::MissiveClient::Get()->GetTestInterface()->GetEnqueuedRecords(
          Priority::IMMEDIATE);
  for (const Record& record : records) {
    if (record.destination() == Destination::ADDED_REMOVED_EVENTS) {
      return record;
    }
  }
  return std::nullopt;
}

// Waiter used by tests during public session user creation.
class PublicSessionUserCreationWaiter
    : public user_manager::UserManager::Observer {
 public:
  PublicSessionUserCreationWaiter() = default;

  PublicSessionUserCreationWaiter(const PublicSessionUserCreationWaiter&) =
      delete;
  PublicSessionUserCreationWaiter& operator=(
      const PublicSessionUserCreationWaiter&) = delete;

  ~PublicSessionUserCreationWaiter() override = default;

  void Wait(const AccountId& public_session_account_id) {
    if (user_manager::UserManager::Get()->IsKnownUser(
            public_session_account_id)) {
      return;
    }

    local_state_changed_run_loop_ = std::make_unique<base::RunLoop>();
    user_manager::UserManager::Get()->AddObserver(this);
    local_state_changed_run_loop_->Run();
    user_manager::UserManager::Get()->RemoveObserver(this);
  }

  // user_manager::UserManager::Observer:
  void LocalStateChanged(user_manager::UserManager* user_manager) override {
    local_state_changed_run_loop_->Quit();
  }

 private:
  std::unique_ptr<base::RunLoop> local_state_changed_run_loop_;
};

class UserAddedRemovedReporterBrowserTest
    : public policy::DevicePolicyCrosBrowserTest {
 protected:
  UserAddedRemovedReporterBrowserTest() {
    // Add unaffiliated user for testing purposes.
    login_manager_mixin_.AppendRegularUsers(1);

    login_manager_mixin_.set_session_restore_enabled();
    scoped_testing_cros_settings_.device_settings()->SetBoolean(
        kReportDeviceLoginLogout, true);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kLoginManager);
    command_line->AppendSwitch(switches::kAllowFailedPolicyFetchForTest);
  }

  void SetUpOnMainThread() override {
    login_manager_mixin_.SetShouldLaunchBrowser(true);
    FakeSessionManagerClient::Get()->set_supports_browser_restart(true);
    policy::DevicePolicyCrosBrowserTest::SetUpOnMainThread();
  }

  void SetUpInProcessBrowserTestFixture() override {
    policy::DevicePolicyCrosBrowserTest::SetUpInProcessBrowserTestFixture();

    // Set up affiliation for the test user.
    auto device_policy_update = device_state_.RequestDevicePolicyUpdate();
    auto user_policy_update = user_policy_mixin_.RequestPolicyUpdate();

    device_policy_update->policy_data()->add_device_affiliation_ids(
        kTestAffiliationId);
    user_policy_update->policy_data()->add_user_affiliation_ids(
        kTestAffiliationId);
  }

  const AccountId test_account_id_ = AccountId::FromUserEmailGaiaId(
      kTestUserEmail,
      signin::GetTestGaiaIdForEmail(kTestUserEmail));
  UserPolicyMixin user_policy_mixin_{&mixin_host_, test_account_id_};

  FakeGaiaMixin fake_gaia_mixin_{&mixin_host_};
  LoginManagerMixin login_manager_mixin_{
      &mixin_host_, LoginManagerMixin::UserList(), &fake_gaia_mixin_};

  ScopedTestingCrosSettings scoped_testing_cros_settings_;
};

IN_PROC_BROWSER_TEST_F(UserAddedRemovedReporterBrowserTest,
                       ReportNewUnaffiliatedUser) {
  MissiveClientTestObserver observer(Destination::ADDED_REMOVED_EVENTS);
  login_manager_mixin_.LoginAsNewRegularUser();
  test::WaitForPrimaryUserSessionStart();

  const Record& record = GetNextUserAddedRemovedRecord(&observer);
  ASSERT_TRUE(record.has_source_info());
  EXPECT_THAT(record.source_info().source(), Eq(::reporting::SourceInfo::ASH));
  ::reporting::UserAddedRemovedRecord record_data;
  ASSERT_TRUE(record_data.ParseFromString(record.data()));
  EXPECT_TRUE(record_data.has_user_added_event());
  EXPECT_FALSE(record_data.has_affiliated_user());
}

IN_PROC_BROWSER_TEST_F(UserAddedRemovedReporterBrowserTest,
                       ReportRemovedUnaffiliatedUser) {
  MissiveClientTestObserver observer(Destination::ADDED_REMOVED_EVENTS);
  ASSERT_TRUE(LoginScreenTestApi::RemoveUser(
      login_manager_mixin_.users()[0].account_id));

  const Record& record = GetNextUserAddedRemovedRecord(&observer);
  ASSERT_TRUE(record.has_source_info());
  EXPECT_THAT(record.source_info().source(), Eq(::reporting::SourceInfo::ASH));
  ::reporting::UserAddedRemovedRecord record_data;
  ASSERT_TRUE(record_data.ParseFromString(record.data()));
  ASSERT_TRUE(record_data.has_user_removed_event());
  EXPECT_THAT(record_data.user_removed_event().reason(),
              Eq(::reporting::UserRemovalReason::LOCAL_USER_INITIATED));
  EXPECT_FALSE(record_data.has_affiliated_user());
}

IN_PROC_BROWSER_TEST_F(UserAddedRemovedReporterBrowserTest,
                       ReportNewAffiliatedUser) {
  MissiveClientTestObserver observer(Destination::ADDED_REMOVED_EVENTS);
  const LoginManagerMixin::TestUserInfo user_info(test_account_id_);
  const auto& context = LoginManagerMixin::CreateDefaultUserContext(user_info);
  login_manager_mixin_.LoginAsNewRegularUser(context);
  test::WaitForPrimaryUserSessionStart();

  const Record& record = GetNextUserAddedRemovedRecord(&observer);
  ASSERT_TRUE(record.has_source_info());
  EXPECT_THAT(record.source_info().source(), Eq(::reporting::SourceInfo::ASH));
  ::reporting::UserAddedRemovedRecord record_data;
  ASSERT_TRUE(record_data.ParseFromString(record.data()));
  EXPECT_TRUE(record_data.has_user_added_event());
  EXPECT_TRUE(record_data.has_affiliated_user());
  EXPECT_TRUE(record_data.affiliated_user().has_user_email());
  EXPECT_THAT(record_data.affiliated_user().user_email(),
              StrEq(kTestUserEmail));
}

// Login as a new affiliated user and sign out so we are prepared to test for
// user removed reporting from the login screen.
IN_PROC_BROWSER_TEST_F(UserAddedRemovedReporterBrowserTest,
                       PRE_ReportRemovedAffiliatedUser) {
  const LoginManagerMixin::TestUserInfo user_info(test_account_id_);
  const auto& context = LoginManagerMixin::CreateDefaultUserContext(user_info);
  login_manager_mixin_.SkipPostLoginScreens();
  login_manager_mixin_.LoginAsNewRegularUser(context);
  login_manager_mixin_.WaitForActiveSession();
  Shell::Get()->session_controller()->RequestSignOut();
}

IN_PROC_BROWSER_TEST_F(UserAddedRemovedReporterBrowserTest,
                       ReportRemovedAffiliatedUser) {
  MissiveClientTestObserver observer(Destination::ADDED_REMOVED_EVENTS);
  ASSERT_TRUE(LoginScreenTestApi::RemoveUser(test_account_id_));

  const Record& record = GetNextUserAddedRemovedRecord(&observer);
  ASSERT_TRUE(record.has_source_info());
  EXPECT_THAT(record.source_info().source(), Eq(::reporting::SourceInfo::ASH));
  ::reporting::UserAddedRemovedRecord record_data;
  ASSERT_TRUE(record_data.ParseFromString(record.data()));
  ASSERT_TRUE(record_data.has_user_removed_event());
  EXPECT_EQ(record_data.user_removed_event().reason(),
            ::reporting::UserRemovalReason::LOCAL_USER_INITIATED);
  ASSERT_TRUE(record_data.has_affiliated_user());
  ASSERT_TRUE(record_data.affiliated_user().has_user_email());
  EXPECT_THAT(record_data.affiliated_user().user_email(),
              StrEq(kTestUserEmail));
}

IN_PROC_BROWSER_TEST_F(UserAddedRemovedReporterBrowserTest,
                       PRE_DoesNotReportGuestUser) {
  base::RunLoop restart_job_waiter;
  FakeSessionManagerClient::Get()->set_restart_job_callback(
      restart_job_waiter.QuitClosure());

  ASSERT_TRUE(LoginScreenTestApi::IsGuestButtonShown());
  ASSERT_TRUE(LoginScreenTestApi::ClickGuestButton());

  restart_job_waiter.Run();
  EXPECT_TRUE(FakeSessionManagerClient::Get()->restart_job_argv().has_value());
}

IN_PROC_BROWSER_TEST_F(UserAddedRemovedReporterBrowserTest,
                       DoesNotReportGuestUser) {
  test::WaitForPrimaryUserSessionStart();
  base::RunLoop().RunUntilIdle();

  const user_manager::UserManager* const user_manager =
      user_manager::UserManager::Get();
  ASSERT_TRUE(user_manager->IsLoggedInAsGuest());

  const std::optional<Record> record = MaybeGetEnqueuedUserAddedRemovedRecord();
  ASSERT_FALSE(record.has_value());
}

class UserAddedRemovedReporterPublicSessionBrowserTest
    : public policy::DevicePolicyCrosBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    policy::DevicePolicyCrosBrowserTest::SetUpOnMainThread();

    // Wait for the public session user to be created.
    PublicSessionUserCreationWaiter public_session_waiter;
    public_session_waiter.Wait(public_session_account_id_);
    EXPECT_TRUE(user_manager::UserManager::Get()->IsKnownUser(
        public_session_account_id_));

    // Wait for the device local account policy to be installed.
    policy::CloudPolicyStore* const store =
        TestingBrowserProcess::GetGlobal()
            ->platform_part()
            ->browser_policy_connector_ash()
            ->GetDeviceLocalAccountPolicyService()
            ->GetBrokerForUser(public_session_account_id_.GetUserEmail())
            ->core()
            ->store();
    if (!store->has_policy()) {
      policy::MockCloudPolicyStoreObserver observer;

      base::RunLoop loop;
      store->AddObserver(&observer);
      EXPECT_CALL(observer, OnStoreLoaded(store))
          .WillOnce(InvokeWithoutArgs(&loop, &base::RunLoop::Quit));
      loop.Run();
      store->RemoveObserver(&observer);
    }
  }

  void SetUpInProcessBrowserTestFixture() override {
    policy::DevicePolicyCrosBrowserTest::SetUpInProcessBrowserTestFixture();

    // Setup the device policy.
    ChromeDeviceSettingsProto& proto(device_policy()->payload());
    DeviceLocalAccountInfoProto* account =
        proto.mutable_device_local_accounts()->add_account();
    account->set_account_id(kPublicSessionUserEmail);
    account->set_type(DeviceLocalAccountInfoProto::ACCOUNT_TYPE_PUBLIC_SESSION);
    // Enable login/logout reporting.
    proto.mutable_device_reporting()->set_report_login_logout(true);
    RefreshDevicePolicy();

    // Setup the device local account policy.
    policy::UserPolicyBuilder device_local_account_policy;
    device_local_account_policy.policy_data().set_username(
        kPublicSessionUserEmail);
    device_local_account_policy.policy_data().set_policy_type(
        policy::dm_protocol::kChromePublicAccountPolicyType);
    device_local_account_policy.policy_data().set_settings_entity_id(
        kPublicSessionUserEmail);
    device_local_account_policy.Build();
    session_manager_client()->set_device_local_account_policy(
        kPublicSessionUserEmail, device_local_account_policy.GetBlob());
  }

  const AccountId public_session_account_id_ =
      AccountId::FromUserEmail(policy::GenerateDeviceLocalAccountUserId(
          kPublicSessionUserEmail,
          policy::DeviceLocalAccountType::kPublicSession));

  const LoginManagerMixin login_manager_mixin_{&mixin_host_,
                                               LoginManagerMixin::UserList()};
};

IN_PROC_BROWSER_TEST_F(UserAddedRemovedReporterPublicSessionBrowserTest,
                       DoesNotReportPublicSessionUser) {
  ASSERT_TRUE(
      LoginScreenTestApi::ExpandPublicSessionPod(public_session_account_id_));
  LoginScreenTestApi::ClickPublicExpandedSubmitButton();
  test::WaitForPrimaryUserSessionStart();
  base::RunLoop().RunUntilIdle();

  const user_manager::UserManager* const user_manager =
      user_manager::UserManager::Get();
  ASSERT_TRUE(user_manager->IsLoggedInAsManagedGuestSession());

  const std::optional<Record> record = MaybeGetEnqueuedUserAddedRemovedRecord();
  ASSERT_FALSE(record.has_value());
}

class UserAddedRemovedReporterKioskBrowserTest
    : public MixinBasedInProcessBrowserTest {
 protected:
  void SetUp() override {
    login_manager_mixin_.set_session_restore_enabled();

    MixinBasedInProcessBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    MixinBasedInProcessBrowserTest::SetUpCommandLine(command_line);

    fake_cws_.Init(embedded_test_server());
    fake_cws_.SetUpdateCrx(GetTestAppId(), GetTestAppId() + ".crx", "1.0.0");
  }

  void SetUpInProcessBrowserTestFixture() override {
    MixinBasedInProcessBrowserTest::SetUpInProcessBrowserTestFixture();

    host_resolver()->AddRule("*", "127.0.0.1");
    FakeSessionManagerClient::Get()->set_supports_browser_restart(true);

    ChromeDeviceSettingsProto& proto(policy_helper_.device_policy()->payload());
    KioskAppsMixin::AppendAutoLaunchKioskAccount(&proto);
    proto.mutable_device_reporting()->set_report_login_logout(true);
    policy_helper_.RefreshDevicePolicy();
  }

  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
    extensions::browsertest_util::CreateAndInitializeLocalCache();
  }

  std::string GetTestAppId() const { return KioskAppsMixin::kTestChromeAppId; }

  FakeCWS fake_cws_;
  policy::DevicePolicyCrosTestHelper policy_helper_;
  base::AutoReset<bool> skip_splash_wait_override_ =
      KioskTestHelper::SkipSplashScreenWait();
  const EmbeddedTestServerSetupMixin embedded_test_server_{
      &mixin_host_, embedded_test_server()};

  const DeviceStateMixin device_state_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
  LoginManagerMixin login_manager_mixin_{&mixin_host_,
                                         LoginManagerMixin::UserList()};
};

IN_PROC_BROWSER_TEST_F(UserAddedRemovedReporterKioskBrowserTest,
                       DoesNotReportKioskUser) {
  ASSERT_TRUE(::ash::LoginState::Get()->IsKioskSession());
  const std::optional<Record> record = MaybeGetEnqueuedUserAddedRemovedRecord();
  ASSERT_FALSE(record.has_value());
}

}  // namespace
}  // namespace ash::reporting
