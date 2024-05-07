// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <string>

#include "ash/constants/ash_switches.h"
#include "chrome/browser/ash/login/lock/screen_locker_tester.h"
#include "chrome/browser/ash/login/session/user_session_manager_test_api.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screens_utils.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/browser/ash/login/test/user_policy_mixin.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/ash/settings/stub_cros_settings_provider.h"
#include "chrome/browser/extensions/browsertest_util.h"
#include "chrome/browser/policy/messaging_layer/proto/synced/lock_unlock_event.pb.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/dbus/missive/missive_client.h"
#include "chromeos/dbus/missive/missive_client_test_observer.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::chromeos::MissiveClientTestObserver;
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

struct LockUnlockReporterBrowserTestData {
  bool success;
  std::string user_password;
  std::string entered_password;
};

Record GetNextLockUnlockRecord(MissiveClientTestObserver* observer) {
  std::tuple<Priority, Record> enqueued_record =
      observer->GetNextEnqueuedRecord();
  Priority priority = std::get<0>(enqueued_record);
  Record record = std::get<1>(enqueued_record);

  EXPECT_THAT(priority, Eq(Priority::SECURITY));
  return record;
}

class LockUnlockReporterBrowserTest
    : public policy::DevicePolicyCrosBrowserTest,
      public ::testing::WithParamInterface<LockUnlockReporterBrowserTestData> {
 protected:
  LockUnlockReporterBrowserTest() {
    login_manager_mixin_.AppendRegularUsers(1);

    login_manager_mixin_.set_session_restore_enabled();
    scoped_testing_cros_settings_.device_settings()->SetBoolean(
        kReportDeviceLoginLogout, true);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kLoginManager);
    command_line->AppendSwitch(switches::kForceLoginManagerInTests);
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

  void LoginUser(LoginManagerMixin::TestUserInfo user) {
    login_manager_mixin_.SkipPostLoginScreens();
    auto context = LoginManagerMixin::CreateDefaultUserContext(user);
    login_manager_mixin_.LoginAsNewRegularUser(context);
    test::WaitForPrimaryUserSessionStart();
  }

  const AccountId test_account_id_ = AccountId::FromUserEmailGaiaId(
      kTestUserEmail,
      signin::GetTestGaiaIdForEmail(kTestUserEmail));

  const LoginManagerMixin::TestUserInfo managed_user_{test_account_id_};

  UserPolicyMixin user_policy_mixin_{&mixin_host_, test_account_id_};

  FakeGaiaMixin fake_gaia_mixin_{&mixin_host_};

  LoginManagerMixin login_manager_mixin_{
      &mixin_host_, LoginManagerMixin::UserList(), &fake_gaia_mixin_};

  ScopedTestingCrosSettings scoped_testing_cros_settings_;
};

IN_PROC_BROWSER_TEST_P(LockUnlockReporterBrowserTest, ReportLockAndUnlockTest) {
  bool success = GetParam().success;
  std::string kUserPassword = GetParam().user_password;
  std::string kEnteredPassword = GetParam().entered_password;
  MissiveClientTestObserver observer(Destination::LOCK_UNLOCK_EVENTS);
  LoginUser(managed_user_);

  const AccountId account_id =
      user_manager::UserManager::Get()->GetPrimaryUser()->GetAccountId();
  ScreenLockerTester screen_locker_tester;

  // Lock the screen and verify the event.
  screen_locker_tester.Lock();
  EXPECT_TRUE(screen_locker_tester.IsLocked());

  const Record& lock_record = GetNextLockUnlockRecord(&observer);
  ASSERT_TRUE(lock_record.has_source_info());
  EXPECT_THAT(lock_record.source_info().source(),
              Eq(::reporting::SourceInfo::ASH));
  LockUnlockRecord lock_record_data;
  ASSERT_TRUE(lock_record_data.ParseFromString(lock_record.data()));
  ASSERT_TRUE(lock_record_data.has_lock_event());
  ASSERT_TRUE(lock_record_data.has_affiliated_user());
  EXPECT_THAT(lock_record_data.affiliated_user().user_email(),
              StrEq(kTestUserEmail));

  // Unlock the screen with the provided password and verify the event.
  screen_locker_tester.SetUnlockPassword(account_id, kUserPassword);
  screen_locker_tester.UnlockWithPassword(account_id, kEnteredPassword);
  if (success) {
    screen_locker_tester.WaitForUnlock();
    EXPECT_FALSE(screen_locker_tester.IsLocked());
  } else {
    EXPECT_TRUE(screen_locker_tester.IsLocked());
  }

  const Record& unlock_record = GetNextLockUnlockRecord(&observer);
  ASSERT_TRUE(unlock_record.has_source_info());
  EXPECT_THAT(unlock_record.source_info().source(),
              Eq(::reporting::SourceInfo::ASH));
  LockUnlockRecord unlock_record_data;
  ASSERT_TRUE(unlock_record_data.ParseFromString(unlock_record.data()));
  ASSERT_TRUE(unlock_record_data.has_unlock_event());
  ASSERT_THAT(unlock_record_data.unlock_event().success(), Eq(success));
  EXPECT_THAT(unlock_record_data.unlock_event().unlock_type(),
              Eq(UnlockType::PASSWORD));
  ASSERT_TRUE(unlock_record_data.has_affiliated_user());
  EXPECT_THAT(unlock_record_data.affiliated_user().user_email(),
              StrEq(kTestUserEmail));
}

const LockUnlockReporterBrowserTestData kTestingParams[] = {
    {.success = true, .user_password = "pass", .entered_password = "pass"},
    {.success = false, .user_password = "pass", .entered_password = "password"},
};

INSTANTIATE_TEST_SUITE_P(LockUnlockReporterBrowserTest,
                         LockUnlockReporterBrowserTest,
                         testing::ValuesIn(kTestingParams));

}  // namespace
}  // namespace ash::reporting
