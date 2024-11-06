// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/user_session_activity/user_session_activity_reporter.h"

#include <memory>
#include <string_view>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/repeating_test_future.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/policy/reporting/user_event_reporter_helper.h"
#include "chrome/browser/ash/policy/reporting/user_event_reporter_helper_testing.h"
#include "chrome/browser/ash/policy/reporting/user_session_activity/user_session_activity_reporter_delegate.h"
#include "chrome/browser/ash/power/ml/idle_event_notifier.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/policy/messaging_layer/proto/synced/user_session_activity.pb.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "components/policy/core/common/device_local_account_type.h"
#include "components/reporting/client/mock_report_queue.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_names.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Eq;
using ::testing::NiceMock;

namespace reporting {

// These tests verify the control flow of
// `reporting::UserSessionActivityReporter`.
//
// Verification of business logic for modifying internal state, policy checks,
// and actual reporting of events is done in the delegate unit tests.
class UserSessionActivityReporterTest : public ::testing::Test {
 protected:
  void SetUp() override {
    chromeos::PowerManagerClient::InitializeFake();

    ash::SessionManagerClient::InitializeFake();

    session_termination_manager_ =
        std::make_unique<ash::SessionTerminationManager>();

    fake_user_manager_.Reset(std::make_unique<ash::FakeChromeUserManager>());
  }

  void TearDown() override { chromeos::PowerManagerClient::Shutdown(); }

  std::unique_ptr<UserSessionActivityReporter> CreateReporter(
      policy::ManagedSessionService* managed_session_service,
      user_manager::UserManager* user_manager,
      std::unique_ptr<UserSessionActivityReporter::Delegate> delegate) {
    return base::WrapUnique(new UserSessionActivityReporter(
        managed_session_service, user_manager, std::move(delegate)));
  }

  user_manager::User* CreateAffiliatedUser() {
    return CreateUser(/*is_affiliated=*/true);
  }

  user_manager::User* CreateUnaffiliatedUser() {
    return CreateUser(/*is_affiliated=*/false);
  }

  user_manager::User* CreateUser(bool is_affiliated) {
    AccountId account_id = AccountId::FromUserEmail(base::StrCat(
        {(is_affiliated ? "affiliated" : "unaffiliated"), "@foobar.com"}));

    return fake_user_manager_->AddUserWithAffiliation(account_id,
                                                      is_affiliated);
  }

  user_manager::User* CreateManagedGuestUser() {
    AccountId account_id =
        AccountId::FromUserEmail(GenerateDeviceLocalAccountUserId(
            "managed_guest", policy::DeviceLocalAccountType::kPublicSession));
    return fake_user_manager_->AddPublicAccountUser(account_id);
  }

  void ActiveUserChanged(UserSessionActivityReporter* reporter,
                         user_manager::User* user) {
    reporter->ActiveUserChanged(user);
  }

  void OnSessionTerminationStarted(UserSessionActivityReporter* reporter,
                                   user_manager::User* user) {
    reporter->OnSessionTerminationStarted(user);
  }

  void OnLocked(UserSessionActivityReporter* reporter) { reporter->OnLocked(); }

  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_;

  std::unique_ptr<ash::SessionTerminationManager> session_termination_manager_;

  session_manager::SessionManager session_manager_;

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

// Mocks all of the reporting::UserSessionActivityReporter::Delegate` class so
// that we don't have to initialize fake singletons in the unit tests.
class MockUserSessionActivityReporterDelegate
    : public UserSessionActivityReporter::Delegate {
 public:
  MOCK_METHOD(ash::power::ml::IdleEventNotifier::ActivityData,
              QueryIdleStatus,
              (),
              (const override));
  MOCK_METHOD(
      bool,
      IsUserActive,
      (const ash::power::ml::IdleEventNotifier::ActivityData& activity_data),
      (const override));
  MOCK_METHOD(void, ReportSessionActivity, (), (override));
  MOCK_METHOD(void,
              AddActiveIdleState,
              (bool, const user_manager::User*),
              (override));
  MOCK_METHOD(void,
              SetSessionStartEvent,
              (reporting::SessionStartEvent::Reason, const user_manager::User*),
              (override));
  MOCK_METHOD(void,
              SetSessionEndEvent,
              (reporting::SessionEndEvent::Reason, const user_manager::User*),
              (override));
};

// Verifies that:
//   Session starts when user logs in.
//   Session ends when user locks the device.
//   Session starts when user unlocks the device.
//   Session ends when user locks the device.
TEST_F(UserSessionActivityReporterTest, StartAndEndSession) {
  policy::ManagedSessionService managed_session_service;

  auto* user = CreateAffiliatedUser();

  auto delegate =
      std::make_unique<NiceMock<MockUserSessionActivityReporterDelegate>>();

  {
    testing::InSequence sequence;

    // Expect session start on login.
    EXPECT_CALL(*delegate,
                SetSessionStartEvent(SessionStartEvent_Reason_LOGIN, user));

    // Expect session end on lock.
    EXPECT_CALL(*delegate,
                SetSessionEndEvent(SessionEndEvent_Reason_LOCK, user));

    // Expect session start on unlock.
    EXPECT_CALL(*delegate,
                SetSessionStartEvent(SessionStartEvent_Reason_UNLOCK, user));

    // Expect session end on logout.
    EXPECT_CALL(*delegate,
                SetSessionEndEvent(SessionEndEvent_Reason_LOGOUT, user));
  }

  std::unique_ptr<UserSessionActivityReporter> reporter = CreateReporter(
      &managed_session_service, fake_user_manager_.Get(), std::move(delegate));

  // Start session by logging in.
  ActiveUserChanged(reporter.get(), user);

  // End session by locking device.
  OnLocked(reporter.get());

  // Start session by unlocking device.
  ActiveUserChanged(reporter.get(), user);

  // End session by logging out.
  OnSessionTerminationStarted(reporter.get(), user);
}

// Verifies that:
//   Session starts when user logs in.
//   Session ends for first user when a second user logs in, and a session
//   starts for the second user.
TEST_F(UserSessionActivityReporterTest, ActiveUserChanged_MultiUserSession) {
  policy::ManagedSessionService managed_session_service;

  auto* next_user = CreateAffiliatedUser();
  auto* current_user = CreateUnaffiliatedUser();

  auto delegate =
      std::make_unique<NiceMock<MockUserSessionActivityReporterDelegate>>();
  {
    testing::InSequence sequence;

    EXPECT_CALL(*delegate, SetSessionStartEvent(SessionStartEvent_Reason_LOGIN,
                                                current_user));
    EXPECT_CALL(*delegate,
                SetSessionEndEvent(SessionEndEvent_Reason_MULTI_USER_SWITCH,
                                   current_user));
    EXPECT_CALL(*delegate,
                SetSessionStartEvent(SessionStartEvent_Reason_MULTI_USER_SWITCH,
                                     next_user));
  }

  std::unique_ptr<UserSessionActivityReporter> reporter = CreateReporter(
      &managed_session_service, fake_user_manager_.Get(), std::move(delegate));

  // Login `current_user`.
  ActiveUserChanged(reporter.get(), current_user);

  // Login `next_user`, ending the session for `current_user` and starting one
  // for `next_user`.
  ActiveUserChanged(reporter.get(), next_user);
}

TEST_F(UserSessionActivityReporterTest, ReportWhenSessionEnds) {
  policy::ManagedSessionService managed_session_service;

  auto* user = CreateAffiliatedUser();

  auto delegate =
      std::make_unique<NiceMock<MockUserSessionActivityReporterDelegate>>();

  // Expect the delegate to report session activity.
  base::RunLoop run_loop;
  EXPECT_CALL(*delegate, ReportSessionActivity())
      .WillOnce(testing::Invoke([&run_loop]() { run_loop.Quit(); }));

  std::unique_ptr<UserSessionActivityReporter> reporter = CreateReporter(
      &managed_session_service, fake_user_manager_.Get(), std::move(delegate));

  // Trigger session start.
  ActiveUserChanged(reporter.get(), user);

  // Trigger session end.
  managed_session_service.OnSessionWillBeTerminated();

  run_loop.Run();
}

TEST_F(UserSessionActivityReporterTest,
       PeriodicallyReportSessionActivityDuringSession) {
  policy::ManagedSessionService managed_session_service;

  auto* user = CreateAffiliatedUser();

  auto delegate =
      std::make_unique<NiceMock<MockUserSessionActivityReporterDelegate>>();

  // Expect ReportSessionActivity() to be called twice when the reporting timer
  // triggers twice.
  EXPECT_CALL(*delegate, ReportSessionActivity()).Times(2);

  std::unique_ptr<UserSessionActivityReporter> reporter = CreateReporter(
      &managed_session_service, fake_user_manager_.Get(), std::move(delegate));

  // Start the reporting timer by starting a session.
  ActiveUserChanged(reporter.get(), user);

  // Trigger the reporting timer twice to verify repeating callback works.
  task_environment_.FastForwardBy(kReportingFrequency);

  task_environment_.FastForwardBy(kReportingFrequency);
}

TEST_F(UserSessionActivityReporterTest, PeriodicallyCollectDeviceIdleState) {
  policy::ManagedSessionService managed_session_service;

  auto delegate =
      std::make_unique<NiceMock<MockUserSessionActivityReporterDelegate>>();

  auto* user = CreateAffiliatedUser();

  // Expect AddActiveIdleState to be called twice with `user` after the
  // collection timer triggers twice.
  EXPECT_CALL(*delegate, AddActiveIdleState(/*user_is_active=*/_, user))
      .Times(2);

  std::unique_ptr<UserSessionActivityReporter> reporter = CreateReporter(
      &managed_session_service, fake_user_manager_.Get(), std::move(delegate));

  // Start the collection timer by starting a session.
  // ActiveUserChanged(reporter.get(), user);
  ActiveUserChanged(reporter.get(), user);

  // Trigger periodic collection twice to ensure repeating callback works.
  task_environment_.FastForwardBy(kActiveIdleStateCollectionFrequency);

  task_environment_.FastForwardBy(kActiveIdleStateCollectionFrequency);
}

TEST_F(UserSessionActivityReporterTest, AllowRegularUsersAndManagedGuestUsers) {
  policy::ManagedSessionService managed_session_service;

  // Allowed types of users.
  user_manager::User* kAllowedUsers[] = {CreateManagedGuestUser(),
                                         CreateAffiliatedUser(),
                                         CreateUnaffiliatedUser()};

  // Verify the session starts for each type of allowed user.
  for (auto* user : kAllowedUsers) {
    auto delegate =
        std::make_unique<NiceMock<MockUserSessionActivityReporterDelegate>>();

    EXPECT_CALL(*delegate, SetSessionStartEvent(_, _));

    std::unique_ptr<UserSessionActivityReporter> reporter =
        CreateReporter(&managed_session_service, fake_user_manager_.Get(),
                       std::move(delegate));

    // Trigger session start.
    ActiveUserChanged(reporter.get(), user);
  }
}

TEST_F(UserSessionActivityReporterTest,
       IgnoreKioskUsersGuestsUsersAndChildUsers) {
  policy::ManagedSessionService managed_session_service;

  AccountId account_id = AccountId::FromUserEmail("testuser@foobar.com");

  // Create a list of users with types that should be ignored.
  user_manager::User* kIgnoredUserTypes[] = {
      fake_user_manager_->AddKioskAppUser(account_id),
      fake_user_manager_->AddWebKioskAppUser(account_id),
      fake_user_manager_->AddGuestUser(),
      fake_user_manager_->AddChildUser(account_id),
  };

  // Verify the session does NOT start for each type of disallowed user.
  for (auto* user : kIgnoredUserTypes) {
    auto delegate =
        std::make_unique<NiceMock<MockUserSessionActivityReporterDelegate>>();

    EXPECT_CALL(*delegate, SetSessionStartEvent(_, _)).Times(0);

    std::unique_ptr<UserSessionActivityReporter> reporter =
        CreateReporter(&managed_session_service, fake_user_manager_.Get(),
                       std::move(delegate));

    // Trigger session start.
    ActiveUserChanged(reporter.get(), user);
  }
}

}  // namespace reporting
