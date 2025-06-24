// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/user_session_activity/user_session_activity_reporter.h"

#include <unistd.h>

#include "ash/public/cpp/login_screen_test_api.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/lock/screen_locker_tester.h"
#include "chrome/browser/ash/login/session/user_session_manager_test_api.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/scoped_policy_update.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/browser/ash/policy/core/device_cloud_policy_manager_ash.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/policy/messaging_layer/proto/synced/user_session_activity.pb.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/components/login/auth/stub_authenticator_builder.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/dbus/missive/missive_client_test_observer.h"
#include "components/account_id/account_id.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/session_manager/core/session.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/test/browser_test.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/user_activity/user_activity_detector.h"

using chromeos::MissiveClientTestObserver;
using ::reporting::Destination;
using ::reporting::Priority;
using ::reporting::Record;
using ::reporting::UserSessionActivityRecord;

namespace ash {

class UserSessionActivityReporterBrowserTest
    : public policy::DevicePolicyCrosBrowserTest {
 public:
  UserSessionActivityReporterBrowserTest(
      const UserSessionActivityReporterBrowserTest&) = delete;
  UserSessionActivityReporterBrowserTest& operator=(
      const UserSessionActivityReporterBrowserTest&) = delete;

  ~UserSessionActivityReporterBrowserTest() override = default;

 protected:
  UserSessionActivityReporterBrowserTest() {
    login_manager_.set_session_restore_enabled();

    scoped_feature_list_.InitAndEnableFeature(
        ::policy::kEnableUserSessionActivityReporting);
  }

  void SetUpOnMainThread() override {
    login_manager_.SetShouldLaunchBrowser(true);

    FakeSessionManagerClient::Get()->set_supports_browser_restart(true);

    policy::DevicePolicyCrosBrowserTest::SetUpOnMainThread();
  }

  void LoginUser() {
    const UserContext user_context =
        LoginManagerMixin::CreateDefaultUserContext(test_user_);

    auto authenticator_builder =
        std::make_unique<StubAuthenticatorBuilder>(user_context);

    test::UserSessionManagerTestApi(UserSessionManager::GetInstance())
        .InjectAuthenticatorBuilder(std::move(authenticator_builder));

    const std::string& password = user_context.GetKey()->GetSecret();
    LoginScreenTestApi::SubmitPassword(test_user_.account_id, password,
                                       /*check_if_submittable=*/true);
  }

  void SetReportActivityTimesPolicy(bool value) {
    auto device_policy_update = device_state_.RequestDevicePolicyUpdate();
    device_policy_update->policy_payload()
        ->mutable_device_reporting()
        ->set_report_activity_times(value);
  }

  const LoginManagerMixin::TestUserInfo test_user_{
      AccountId::FromUserEmailGaiaId(FakeGaiaMixin::kFakeUserEmail,
                                     FakeGaiaMixin::kFakeUserGaiaId)};

  LoginManagerMixin login_manager_{&mixin_host_, {test_user_}};

  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(UserSessionActivityReporterBrowserTest,
                       ReportSessionActivity) {
  // Enable policy.
  SetReportActivityTimesPolicy(true);

  MissiveClientTestObserver missive_observer(
      Destination::USER_SESSION_ACTIVITY);

  // Start session by logging in a user.
  LoginUser();
  test::WaitForPrimaryUserSessionStart();

  // End session by logging out.
  Shell::Get()->session_controller()->RequestSignOut();
  base::RunLoop().RunUntilIdle();

  // Verify that a record is enqueued when session ends.
  EXPECT_TRUE(missive_observer.HasNewEnqueuedRecord());
  const auto [priority, enqueued_record] =
      missive_observer.GetNextEnqueuedRecord();
  EXPECT_EQ(enqueued_record.source_info().source(),
            ::reporting::SourceInfo::ASH);

  UserSessionActivityRecord activity_record;
  ASSERT_TRUE(activity_record.ParseFromString(enqueued_record.data()));

  EXPECT_TRUE(activity_record.has_unaffiliated_user());
  EXPECT_TRUE(activity_record.unaffiliated_user().has_user_id_num());
  EXPECT_TRUE(activity_record.has_session_end());
  EXPECT_EQ(activity_record.session_end().reason(),
            ::reporting::SessionEndEvent_Reason_LOGOUT);
}

IN_PROC_BROWSER_TEST_F(UserSessionActivityReporterBrowserTest,
                       ReportSessionActivityWhenUserLocksDevice) {
  // Enable policy.
  SetReportActivityTimesPolicy(true);

  MissiveClientTestObserver missive_observer(
      Destination::USER_SESSION_ACTIVITY);

  // Trigger a session start.
  LoginUser();
  test::WaitForPrimaryUserSessionStart();

  // Lock the screen. This triggers a session end.
  const AccountId account_id =
      session_manager::SessionManager::Get()->GetPrimarySession()->account_id();
  ScreenLockerTester screen_locker_tester;
  screen_locker_tester.Lock();
  ASSERT_TRUE(screen_locker_tester.IsLocked());

  // Verify that a record is enqueued when session ends.
  EXPECT_TRUE(missive_observer.HasNewEnqueuedRecord());
  const auto [priority, enqueued_record] =
      missive_observer.GetNextEnqueuedRecord();

  EXPECT_EQ(enqueued_record.source_info().source(),
            ::reporting::SourceInfo::ASH);

  UserSessionActivityRecord activity_record;
  ASSERT_TRUE(activity_record.ParseFromString(enqueued_record.data()));

  // Verify the record contains the correct data.
  EXPECT_TRUE(activity_record.has_session_start());
  EXPECT_EQ(activity_record.session_start().reason(),
            ::reporting::SessionStartEvent_Reason_LOGIN);
  EXPECT_TRUE(activity_record.has_session_end());
  EXPECT_EQ(activity_record.session_end().reason(),
            ::reporting::SessionEndEvent_Reason_LOCK);
  EXPECT_TRUE(activity_record.has_unaffiliated_user());
  EXPECT_TRUE(activity_record.unaffiliated_user().has_user_id_num());

  // Start session by unlocking device.
  screen_locker_tester.SetUnlockPassword(account_id, "pass");
  screen_locker_tester.UnlockWithPassword(account_id, "pass");
  screen_locker_tester.WaitForUnlock();

  // End session by logging out.
  Shell::Get()->session_controller()->RequestSignOut();
  base::RunLoop().RunUntilIdle();

  // Verify that a second record is enqueued when the second session ends.
  EXPECT_TRUE(missive_observer.HasNewEnqueuedRecord());
  const auto [second_priority, second_enqueued_record] =
      missive_observer.GetNextEnqueuedRecord();

  EXPECT_EQ(second_enqueued_record.source_info().source(),
            ::reporting::SourceInfo::ASH);

  UserSessionActivityRecord second_activity_record;
  ASSERT_TRUE(
      second_activity_record.ParseFromString(second_enqueued_record.data()));

  // Verify the record contains the correct data.
  EXPECT_TRUE(second_activity_record.has_session_start());
  EXPECT_EQ(second_activity_record.session_start().reason(),
            ::reporting::SessionStartEvent_Reason_UNLOCK);
  EXPECT_TRUE(second_activity_record.has_session_end());
  EXPECT_EQ(second_activity_record.session_end().reason(),
            ::reporting::SessionEndEvent_Reason_LOGOUT);
  EXPECT_TRUE(second_activity_record.has_unaffiliated_user());
  EXPECT_TRUE(second_activity_record.unaffiliated_user().has_user_id_num());
}

IN_PROC_BROWSER_TEST_F(UserSessionActivityReporterBrowserTest,
                       DoesNotReportWhenPolicyIsDisabled) {
  // Turn off policy
  SetReportActivityTimesPolicy(false);

  MissiveClientTestObserver missive_observer(
      Destination::USER_SESSION_ACTIVITY);

  // Trigger a session start.
  LoginUser();
  test::WaitForPrimaryUserSessionStart();

  // Lock the screen. This triggers a session end.
  ScreenLockerTester screen_locker_tester;
  screen_locker_tester.Lock();
  ASSERT_TRUE(screen_locker_tester.IsLocked());

  // Verify no record is enqueued when session ends.
  EXPECT_FALSE(missive_observer.HasNewEnqueuedRecord());
}

}  // namespace ash
