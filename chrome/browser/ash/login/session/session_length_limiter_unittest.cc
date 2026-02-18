// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/session/session_length_limiter.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/power_monitor_test.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/demo_mode/utils/demo_session_utils.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power/power_policy_controller.h"
#include "components/prefs/testing_pref_service.h"
#include "components/session_manager/core/fake_session_manager_delegate.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/user_activity/user_activity_detector.h"

namespace ash {

namespace {

// Helper class to simulate wall clock advance during suspend/resume.
class WallClockForwarder {
 public:
  explicit WallClockForwarder(base::TestMockTimeTaskRunner* runner);
  ~WallClockForwarder();
  WallClockForwarder(const WallClockForwarder&) = delete;
  WallClockForwarder& operator=(const WallClockForwarder&) = delete;

  void ForwardWhileSuspended(const base::TimeDelta& delta);

 private:
  // Unowned, must outlive this.
  const raw_ptr<base::TestMockTimeTaskRunner> runner_;

  // Used to simulate a power suspend and resume.
  base::test::ScopedPowerMonitorTestSource fake_power_monitor_source_;
};

}  // namespace

WallClockForwarder::WallClockForwarder(base::TestMockTimeTaskRunner* runner)
    : runner_(runner) {}

WallClockForwarder::~WallClockForwarder() = default;

void WallClockForwarder::ForwardWhileSuspended(const base::TimeDelta& delta) {
  fake_power_monitor_source_.Suspend();

  runner_->AdvanceWallClock(delta);

  fake_power_monitor_source_.Resume();
  runner_->RunUntilIdle();
}

class SessionLengthLimiterTest
    : public testing::Test,
      public session_manager::SessionManagerObserver {
 protected:
  SessionLengthLimiterTest();

  // testing::Test:
  void SetUp() override;
  void TearDown() override;

  // session_manager::SessionManagerObserver:
  void OnSignOutRequested() override { SaveSessionStopTime(); }

  void SetSessionUserActivitySeenPref(bool user_activity_seen);
  void ClearSessionUserActivitySeenPref();
  bool IsSessionUserActivitySeenPrefSet();
  bool GetSessionUserActivitySeenPref();

  void SetSessionStartTimePref(const base::Time& session_start_time);
  void ClearSessionStartTimePref();
  bool IsSessionStartTimePrefSet();
  base::Time GetSessionStartTimePref();

  void SetSessionLengthLimitPref(const base::TimeDelta& session_length_limit);
  void ClearSessionLengthLimitPref();

  void SetWaitForInitialUserActivityPref(bool wait_for_initial_user_activity);

  void SimulateUserActivity();

  void UpdateSessionStartTimeIfWaitingForUserActivity();

  void SaveSessionStopTime();

  // Clears the session state by resetting |user_activity_| and
  // |session_start_time_| and creates a new SessionLengthLimiter.
  void CreateSessionLengthLimiter(bool browser_restarted);

  void DestroySessionLengthLimiter();

  session_manager::FakeSessionManagerDelegate* delegate() {
    return delegate_.get();
  }

  scoped_refptr<base::TestMockTimeTaskRunner> runner_;
  std::unique_ptr<WallClockForwarder> wall_clock_forwarder_;
  base::Time session_start_time_;
  base::Time session_stop_time_;

 private:
  TestingPrefServiceSimple& local_state() {
    return *TestingBrowserProcess::GetGlobal()->GetTestingLocalState();
  }

  bool user_activity_seen_ = false;

  std::unique_ptr<session_manager::SessionManager> session_manager_;
  raw_ptr<session_manager::FakeSessionManagerDelegate> delegate_ = nullptr;
  base::ScopedObservation<session_manager::SessionManager,
                          session_manager::SessionManagerObserver>
      session_manager_observer_{this};
  std::unique_ptr<SessionLengthLimiter> session_length_limiter_;
};

SessionLengthLimiterTest::SessionLengthLimiterTest() = default;

void SessionLengthLimiterTest::SetUp() {
  runner_ = new base::TestMockTimeTaskRunner();
  wall_clock_forwarder_ = std::make_unique<WallClockForwarder>(runner_.get());
  runner_->FastForwardBy(base::TimeDelta::FromInternalValue(1000));

  auto delegate =
      std::make_unique<session_manager::FakeSessionManagerDelegate>();
  delegate_ = delegate.get();
  session_manager_ =
      std::make_unique<session_manager::SessionManager>(std::move(delegate));
  session_manager_observer_.Observe(session_manager_.get());
  ui::UserActivityDetector::Get()->ResetStateForTesting();
}

void SessionLengthLimiterTest::TearDown() {
  session_length_limiter_.reset();
  session_manager_observer_.Reset();
  delegate_ = nullptr;
  session_manager_.reset();
  wall_clock_forwarder_.reset();
  ui::UserActivityDetector::Get()->ResetStateForTesting();
}

void SessionLengthLimiterTest::SetSessionUserActivitySeenPref(
    bool user_activity_seen) {
  local_state().SetUserPref(prefs::kSessionUserActivitySeen,
                            std::make_unique<base::Value>(user_activity_seen));
}

void SessionLengthLimiterTest::ClearSessionUserActivitySeenPref() {
  local_state().ClearPref(prefs::kSessionUserActivitySeen);
}

bool SessionLengthLimiterTest::IsSessionUserActivitySeenPrefSet() {
  return local_state().HasPrefPath(prefs::kSessionUserActivitySeen);
}

bool SessionLengthLimiterTest::GetSessionUserActivitySeenPref() {
  EXPECT_TRUE(IsSessionUserActivitySeenPrefSet());
  return local_state().GetBoolean(prefs::kSessionUserActivitySeen);
}

void SessionLengthLimiterTest::SetSessionStartTimePref(
    const base::Time& session_start_time) {
  local_state().SetUserPref(prefs::kSessionStartTime,
                            std::make_unique<base::Value>(base::NumberToString(
                                session_start_time.ToInternalValue())));
}

void SessionLengthLimiterTest::ClearSessionStartTimePref() {
  local_state().ClearPref(prefs::kSessionStartTime);
}

bool SessionLengthLimiterTest::IsSessionStartTimePrefSet() {
  return local_state().HasPrefPath(prefs::kSessionStartTime);
}

base::Time SessionLengthLimiterTest::GetSessionStartTimePref() {
  EXPECT_TRUE(IsSessionStartTimePrefSet());
  return base::Time::FromInternalValue(
      local_state().GetInt64(prefs::kSessionStartTime));
}

void SessionLengthLimiterTest::SetSessionLengthLimitPref(
    const base::TimeDelta& session_length_limit) {
  local_state().SetUserPref(prefs::kSessionLengthLimit,
                            std::make_unique<base::Value>(static_cast<int>(
                                session_length_limit.InMilliseconds())));
  UpdateSessionStartTimeIfWaitingForUserActivity();
}

void SessionLengthLimiterTest::ClearSessionLengthLimitPref() {
  local_state().RemoveUserPref(prefs::kSessionLengthLimit);
  UpdateSessionStartTimeIfWaitingForUserActivity();
}

void SessionLengthLimiterTest::SetWaitForInitialUserActivityPref(
    bool wait_for_initial_user_activity) {
  UpdateSessionStartTimeIfWaitingForUserActivity();
  local_state().SetUserPref(
      prefs::kSessionWaitForInitialUserActivity,
      std::make_unique<base::Value>(wait_for_initial_user_activity));
}

void SessionLengthLimiterTest::SimulateUserActivity() {
  ui::UserActivityDetector::Get()->HandleExternalUserActivity();
  UpdateSessionStartTimeIfWaitingForUserActivity();
  user_activity_seen_ = true;
}

void SessionLengthLimiterTest::
    UpdateSessionStartTimeIfWaitingForUserActivity() {
  if (!user_activity_seen_ &&
      local_state().GetBoolean(prefs::kSessionWaitForInitialUserActivity)) {
    session_start_time_ = runner_->Now();
  }
}

void SessionLengthLimiterTest::SaveSessionStopTime() {
  session_stop_time_ = runner_->Now();
}

void SessionLengthLimiterTest::CreateSessionLengthLimiter(
    bool browser_restarted) {
  user_activity_seen_ = false;
  session_start_time_ = runner_->Now();
  session_length_limiter_ = std::make_unique<SessionLengthLimiter>(
      TestingBrowserProcess::GetGlobal()->local_state(),
      runner_->GetMockClock(), session_manager_.get(), browser_restarted);
}

void SessionLengthLimiterTest::DestroySessionLengthLimiter() {
  session_length_limiter_.reset();
  delegate_ = nullptr;
}

// Verifies that when not instructed to wait for initial user activity, the
// session start time is set and the pref indicating user activity is cleared
// in local state during login.
TEST_F(SessionLengthLimiterTest, StartDoNotWaitForInitialUserActivity) {
  // Pref indicating user activity not set. Session start time not set.
  ClearSessionUserActivitySeenPref();
  ClearSessionStartTimePref();
  CreateSessionLengthLimiter(false);
  EXPECT_FALSE(IsSessionUserActivitySeenPrefSet());
  EXPECT_EQ(session_start_time_, GetSessionStartTimePref());
  DestroySessionLengthLimiter();

  // Pref indicating user activity set. Session start time not set.
  SetSessionUserActivitySeenPref(true);
  ClearSessionStartTimePref();
  CreateSessionLengthLimiter(false);
  EXPECT_FALSE(IsSessionUserActivitySeenPrefSet());
  EXPECT_EQ(session_start_time_, GetSessionStartTimePref());
  DestroySessionLengthLimiter();

  // Pref indicating user activity not set. Session start time in the future.
  ClearSessionUserActivitySeenPref();
  SetSessionStartTimePref(session_start_time_ + base::Hours(2));
  CreateSessionLengthLimiter(false);
  EXPECT_FALSE(IsSessionUserActivitySeenPrefSet());
  EXPECT_EQ(session_start_time_, GetSessionStartTimePref());
  DestroySessionLengthLimiter();

  // Pref indicating user activity set. Session start time in the future.
  SetSessionUserActivitySeenPref(true);
  SetSessionStartTimePref(session_start_time_ + base::Hours(2));
  CreateSessionLengthLimiter(false);
  EXPECT_FALSE(IsSessionUserActivitySeenPrefSet());
  EXPECT_EQ(session_start_time_, GetSessionStartTimePref());
  DestroySessionLengthLimiter();

  // Pref indicating user activity not set. Session start time valid.
  ClearSessionUserActivitySeenPref();
  SetSessionStartTimePref(session_start_time_ - base::Hours(2));
  CreateSessionLengthLimiter(false);
  EXPECT_FALSE(IsSessionUserActivitySeenPrefSet());
  EXPECT_EQ(session_start_time_, GetSessionStartTimePref());
  DestroySessionLengthLimiter();

  // Pref indicating user activity set. Session start time valid.
  SetSessionUserActivitySeenPref(true);
  SetSessionStartTimePref(session_start_time_ - base::Hours(2));
  CreateSessionLengthLimiter(false);
  EXPECT_FALSE(IsSessionUserActivitySeenPrefSet());
  EXPECT_EQ(session_start_time_, GetSessionStartTimePref());
  DestroySessionLengthLimiter();
}

// Verifies that when instructed to wait for initial user activity, the session
// start time and the pref indicating user activity are cleared in local state
// during login.
TEST_F(SessionLengthLimiterTest, StartWaitForInitialUserActivity) {
   SetWaitForInitialUserActivityPref(true);

  // Pref indicating user activity not set. Session start time not set.
  ClearSessionUserActivitySeenPref();
  ClearSessionStartTimePref();
  CreateSessionLengthLimiter(false);
  EXPECT_FALSE(IsSessionUserActivitySeenPrefSet());
  EXPECT_FALSE(IsSessionStartTimePrefSet());
  DestroySessionLengthLimiter();

  // Pref indicating user activity set. Session start time not set.
  SetSessionUserActivitySeenPref(true);
  ClearSessionStartTimePref();
  CreateSessionLengthLimiter(false);
  EXPECT_FALSE(IsSessionUserActivitySeenPrefSet());
  EXPECT_FALSE(IsSessionStartTimePrefSet());
  DestroySessionLengthLimiter();

  // Pref indicating user activity not set. Session start time in the future.
  ClearSessionUserActivitySeenPref();
  SetSessionStartTimePref(runner_->Now() + base::Hours(2));
  CreateSessionLengthLimiter(false);
  EXPECT_FALSE(IsSessionUserActivitySeenPrefSet());
  EXPECT_FALSE(IsSessionStartTimePrefSet());
  DestroySessionLengthLimiter();

  // Pref indicating user activity set. Session start time in the future.
  SetSessionUserActivitySeenPref(true);
  SetSessionStartTimePref(runner_->Now() + base::Hours(2));
  CreateSessionLengthLimiter(false);
  EXPECT_FALSE(IsSessionUserActivitySeenPrefSet());
  EXPECT_FALSE(IsSessionStartTimePrefSet());
  DestroySessionLengthLimiter();

  // Pref indicating user activity not set. Session start time valid.
  ClearSessionUserActivitySeenPref();
  SetSessionStartTimePref(runner_->Now() - base::Hours(2));
  CreateSessionLengthLimiter(false);
  EXPECT_FALSE(IsSessionUserActivitySeenPrefSet());
  EXPECT_FALSE(IsSessionStartTimePrefSet());
  DestroySessionLengthLimiter();

  // Pref indicating user activity set. Session start time valid.
  SetSessionUserActivitySeenPref(true);
  SetSessionStartTimePref(runner_->Now() - base::Hours(2));
  CreateSessionLengthLimiter(false);
  EXPECT_FALSE(IsSessionUserActivitySeenPrefSet());
  EXPECT_FALSE(IsSessionStartTimePrefSet());
  DestroySessionLengthLimiter();
}

// Verifies that when not instructed to wait for initial user activity, local
// state is correctly updated during restart after a crash:
// * If no valid session start time is found in local state, the session start
//   time is set and the pref indicating user activity is cleared.
// * If a valid session start time is found in local state, the session start
//   time and the pref indicating user activity are *not* modified.
TEST_F(SessionLengthLimiterTest, RestartDoNotWaitForInitialUserActivity) {
  // Pref indicating user activity not set. Session start time not set.
  ClearSessionUserActivitySeenPref();
  ClearSessionStartTimePref();
  CreateSessionLengthLimiter(true);
  EXPECT_FALSE(IsSessionUserActivitySeenPrefSet());
  EXPECT_EQ(session_start_time_, GetSessionStartTimePref());
  DestroySessionLengthLimiter();

  // Pref indicating user activity set. Session start time not set.
  SetSessionUserActivitySeenPref(true);
  ClearSessionStartTimePref();
  CreateSessionLengthLimiter(true);
  EXPECT_FALSE(IsSessionUserActivitySeenPrefSet());
  EXPECT_EQ(session_start_time_, GetSessionStartTimePref());
  DestroySessionLengthLimiter();

  // Pref indicating user activity not set. Session start time in the future.
  ClearSessionUserActivitySeenPref();
  SetSessionStartTimePref(runner_->Now() + base::Hours(2));
  CreateSessionLengthLimiter(true);
  EXPECT_FALSE(IsSessionUserActivitySeenPrefSet());
  EXPECT_EQ(session_start_time_, GetSessionStartTimePref());
  DestroySessionLengthLimiter();

  // Pref indicating user activity set. Session start time in the future.
  SetSessionUserActivitySeenPref(true);
  SetSessionStartTimePref(runner_->Now() + base::Hours(2));
  CreateSessionLengthLimiter(true);
  EXPECT_FALSE(IsSessionUserActivitySeenPrefSet());
  EXPECT_EQ(session_start_time_, GetSessionStartTimePref());
  DestroySessionLengthLimiter();

  const base::Time stored_session_start_time = runner_->Now() - base::Hours(2);

  // Pref indicating user activity not set. Session start time valid.
  ClearSessionUserActivitySeenPref();
  SetSessionStartTimePref(stored_session_start_time);
  CreateSessionLengthLimiter(true);
  EXPECT_FALSE(IsSessionUserActivitySeenPrefSet());
  EXPECT_EQ(stored_session_start_time, GetSessionStartTimePref());
  DestroySessionLengthLimiter();

  // Pref indicating user activity set. Session start time valid.
  SetSessionUserActivitySeenPref(true);
  SetSessionStartTimePref(stored_session_start_time);
  CreateSessionLengthLimiter(true);
  EXPECT_TRUE(IsSessionUserActivitySeenPrefSet());
  EXPECT_TRUE(GetSessionUserActivitySeenPref());
  EXPECT_EQ(stored_session_start_time, GetSessionStartTimePref());
  DestroySessionLengthLimiter();
}

// Verifies that when instructed to wait for initial user activity, local state
// is correctly updated during restart after a crash:
// * If no valid session start time is found in local state, the session start
//   time and the pref indicating user activity are cleared.
// * If a valid session start time is found in local state, the session start
//   time and the pref indicating user activity are *not* modified.
TEST_F(SessionLengthLimiterTest, RestartWaitForInitialUserActivity) {
  SetWaitForInitialUserActivityPref(true);

  // Pref indicating user activity not set. Session start time not set.
  ClearSessionUserActivitySeenPref();
  ClearSessionStartTimePref();
  CreateSessionLengthLimiter(true);
  EXPECT_FALSE(IsSessionUserActivitySeenPrefSet());
  EXPECT_FALSE(IsSessionStartTimePrefSet());
  DestroySessionLengthLimiter();

  // Pref indicating user activity set. Session start time not set.
  SetSessionUserActivitySeenPref(true);
  ClearSessionStartTimePref();
  CreateSessionLengthLimiter(true);
  EXPECT_FALSE(IsSessionUserActivitySeenPrefSet());
  EXPECT_FALSE(IsSessionStartTimePrefSet());
  DestroySessionLengthLimiter();

  // Pref indicating user activity not set. Session start time in the future.
  ClearSessionUserActivitySeenPref();
  SetSessionStartTimePref(runner_->Now() + base::Hours(2));
  CreateSessionLengthLimiter(true);
  EXPECT_FALSE(IsSessionUserActivitySeenPrefSet());
  EXPECT_FALSE(IsSessionStartTimePrefSet());
  DestroySessionLengthLimiter();

  // Pref indicating user activity set. Session start time in the future.
  SetSessionUserActivitySeenPref(true);
  SetSessionStartTimePref(runner_->Now() + base::Hours(2));
  CreateSessionLengthLimiter(true);
  EXPECT_FALSE(IsSessionUserActivitySeenPrefSet());
  EXPECT_FALSE(IsSessionStartTimePrefSet());
  DestroySessionLengthLimiter();

  const base::Time stored_session_start_time = runner_->Now() - base::Hours(2);

  // Pref indicating user activity not set. Session start time valid.
  ClearSessionUserActivitySeenPref();
  SetSessionStartTimePref(stored_session_start_time);
  CreateSessionLengthLimiter(true);
  EXPECT_FALSE(IsSessionUserActivitySeenPrefSet());
  EXPECT_EQ(stored_session_start_time, GetSessionStartTimePref());
  DestroySessionLengthLimiter();

  // Pref indicating user activity set. Session start time valid.
  SetSessionUserActivitySeenPref(true);
  SetSessionStartTimePref(stored_session_start_time);
  CreateSessionLengthLimiter(true);
  EXPECT_TRUE(IsSessionUserActivitySeenPrefSet());
  EXPECT_TRUE(GetSessionUserActivitySeenPref());
  EXPECT_EQ(stored_session_start_time, GetSessionStartTimePref());
  DestroySessionLengthLimiter();
}

// Verifies that local state is correctly updated when waiting for initial user
// activity is toggled and no user activity has occurred yet.
TEST_F(SessionLengthLimiterTest, ToggleWaitForInitialUserActivity) {
  CreateSessionLengthLimiter(false);

  // Verify that the pref indicating user activity was not set and the session
  // start time was set.
  EXPECT_FALSE(IsSessionUserActivitySeenPrefSet());
  EXPECT_EQ(session_start_time_, GetSessionStartTimePref());

  // Enable waiting for initial user activity.
  runner_->FastForwardBy(base::Seconds(1));
  SetWaitForInitialUserActivityPref(true);

  // Verify that the session start time was cleared and the pref indicating user
  // activity was not set.
  EXPECT_FALSE(IsSessionUserActivitySeenPrefSet());
  EXPECT_FALSE(IsSessionStartTimePrefSet());

  // Disable waiting for initial user activity.
  runner_->FastForwardBy(base::Seconds(1));
  SetWaitForInitialUserActivityPref(false);

  // Verify that the pref indicating user activity was not set and the session
  // start time was.
  EXPECT_FALSE(IsSessionUserActivitySeenPrefSet());
  EXPECT_EQ(session_start_time_, GetSessionStartTimePref());
}

// Verifies that local state is correctly updated when instructed not to wait
// for initial user activity and user activity occurs. Also verifies that once
// initial user activity has occurred, neither the session start time nor the
// pref indicating user activity change in local state anymore.
TEST_F(SessionLengthLimiterTest, UserActivityWhileNotWaiting) {
  CreateSessionLengthLimiter(false);

  // Verify that the pref indicating user activity was not set and the session
  // start time was set.
  EXPECT_FALSE(IsSessionUserActivitySeenPrefSet());
  EXPECT_EQ(session_start_time_, GetSessionStartTimePref());

  // Simulate user activity.
  runner_->FastForwardBy(base::Seconds(1));
  SimulateUserActivity();

  // Verify that the pref indicating user activity and the session start time
  // were set.
  EXPECT_TRUE(IsSessionUserActivitySeenPrefSet());
  EXPECT_TRUE(GetSessionUserActivitySeenPref());
  EXPECT_EQ(session_start_time_, GetSessionStartTimePref());

  // Simulate user activity.
  runner_->FastForwardBy(base::Seconds(1));
  SimulateUserActivity();

  // Verify that the pref indicating user activity and the session start time
  // were not changed.
  EXPECT_TRUE(IsSessionUserActivitySeenPrefSet());
  EXPECT_TRUE(GetSessionUserActivitySeenPref());
  EXPECT_EQ(session_start_time_, GetSessionStartTimePref());

  // Enable waiting for initial user activity.
  runner_->FastForwardBy(base::Seconds(1));
  SetWaitForInitialUserActivityPref(true);

  // Verify that the pref indicating user activity and the session start time
  // were not changed.
  EXPECT_TRUE(IsSessionUserActivitySeenPrefSet());
  EXPECT_TRUE(GetSessionUserActivitySeenPref());
  EXPECT_EQ(session_start_time_, GetSessionStartTimePref());
}

// Verifies that local state is correctly updated when instructed to wait for
// initial user activity and user activity occurs. Also verifies that once
// initial user activity has occurred, neither the session start time nor the
// pref indicating user activity change in local state anymore.
TEST_F(SessionLengthLimiterTest, UserActivityWhileWaiting) {
  SetWaitForInitialUserActivityPref(true);

  CreateSessionLengthLimiter(false);

  // Verify that the pref indicating user activity and the session start time
  // were not set.
  EXPECT_FALSE(IsSessionUserActivitySeenPrefSet());
  EXPECT_FALSE(IsSessionStartTimePrefSet());

  // Simulate user activity.
  runner_->FastForwardBy(base::Seconds(1));
  SimulateUserActivity();

  // Verify that the pref indicating user activity and the session start time
  // were set.
  EXPECT_TRUE(IsSessionUserActivitySeenPrefSet());
  EXPECT_TRUE(GetSessionUserActivitySeenPref());
  EXPECT_EQ(session_start_time_, GetSessionStartTimePref());

  // Simulate user activity.
  runner_->FastForwardBy(base::Seconds(1));
  SimulateUserActivity();

  // Verify that the pref indicating user activity and the session start time
  // were not changed.
  EXPECT_TRUE(IsSessionUserActivitySeenPrefSet());
  EXPECT_TRUE(GetSessionUserActivitySeenPref());
  EXPECT_EQ(session_start_time_, GetSessionStartTimePref());

  // Disable waiting for initial user activity.
  runner_->FastForwardBy(base::Seconds(1));
  SetWaitForInitialUserActivityPref(false);

  // Verify that the pref indicating user activity and the session start time
  // were not changed.
  EXPECT_TRUE(IsSessionUserActivitySeenPrefSet());
  EXPECT_TRUE(GetSessionUserActivitySeenPref());
  EXPECT_EQ(session_start_time_, GetSessionStartTimePref());
}

// Creates a SessionLengthLimiter without setting a limit. Verifies that the
// limiter does not start a timer.
TEST_F(SessionLengthLimiterTest, RunWithoutLimit) {
  base::SingleThreadTaskRunner::CurrentDefaultHandle runner_handle(runner_);

  CreateSessionLengthLimiter(false);

  // Verify that no timer fires to terminate the session.
  runner_->FastForwardUntilNoTasksRemain();
}

// Creates a SessionLengthLimiter after setting a limit and instructs it not to
// wait for user activity. Verifies that the limiter starts a timer even if no
// user activity occurs and that when the session length reaches the limit, the
// session is terminated.
TEST_F(SessionLengthLimiterTest, RunWithoutUserActivityWhileNotWaiting) {
  base::SingleThreadTaskRunner::CurrentDefaultHandle runner_handle(runner_);

  // Set a 60 second session time limit.
  SetSessionLengthLimitPref(base::Seconds(60));

  CreateSessionLengthLimiter(false);
  EXPECT_EQ(session_start_time_, GetSessionStartTimePref());

  // Verify that the timer fires and the session is terminated when the session
  // length limit is reached.
  EXPECT_EQ(0, delegate()->request_sign_out_count());
  runner_->FastForwardUntilNoTasksRemain();
  EXPECT_EQ(1, delegate()->request_sign_out_count());
  EXPECT_EQ(session_start_time_ + base::Seconds(60), session_stop_time_);
}

// Creates a SessionLengthLimiter after setting a limit and instructs it to wait
// for initial user activity. Verifies that if no user activity occurs, the
// limiter does not start a timer.
TEST_F(SessionLengthLimiterTest, RunWithoutUserActivityWhileWaiting) {
  base::SingleThreadTaskRunner::CurrentDefaultHandle runner_handle(runner_);
  SetWaitForInitialUserActivityPref(true);

  // Set a 60 second session time limit.
  SetSessionLengthLimitPref(base::Seconds(60));

  CreateSessionLengthLimiter(false);
  EXPECT_FALSE(IsSessionStartTimePrefSet());

  // Verify that no timer fires to terminate the session.
  runner_->FastForwardUntilNoTasksRemain();
  EXPECT_EQ(0, delegate()->request_sign_out_count());
}

// Creates a SessionLengthLimiter after setting a limit and instructs it not to
// wait for user activity. Verifies that the limiter starts a timer and that
// when the session length reaches the limit, the session is terminated. Also
// verifies that user activity does not affect the timer.
TEST_F(SessionLengthLimiterTest, RunWithUserActivityWhileNotWaiting) {
  base::SingleThreadTaskRunner::CurrentDefaultHandle runner_handle(runner_);

  // Set a 60 second session time limit.
  SetSessionLengthLimitPref(base::Seconds(60));

  CreateSessionLengthLimiter(false);
  EXPECT_EQ(session_start_time_, GetSessionStartTimePref());

  // Simulate user activity after 20 seconds.
  runner_->FastForwardBy(base::Seconds(20));
  SimulateUserActivity();
  EXPECT_EQ(session_start_time_, GetSessionStartTimePref());

  // Verify that the timer fires and the session is terminated when the session
  // length limit is reached.
  EXPECT_EQ(0, delegate()->request_sign_out_count());
  runner_->FastForwardUntilNoTasksRemain();
  EXPECT_EQ(1, delegate()->request_sign_out_count());
  EXPECT_EQ(session_start_time_ + base::Seconds(60), session_stop_time_);
}

// Creates a SessionLengthLimiter after setting a limit and instructs it to wait
// for initial user activity. Verifies that once user activity occurs, the
// limiter starts a timer and that when the session length reaches the limit,
// the session is terminated. Also verifies that further user activity does not
// affect the timer.
TEST_F(SessionLengthLimiterTest, RunWithUserActivityWhileWaiting) {
  base::SingleThreadTaskRunner::CurrentDefaultHandle runner_handle(runner_);
  SetWaitForInitialUserActivityPref(true);

  // Set a 60 second session time limit.
  SetSessionLengthLimitPref(base::Seconds(60));

  CreateSessionLengthLimiter(false);
  EXPECT_FALSE(IsSessionStartTimePrefSet());

  // Simulate user activity after 20 seconds.
  runner_->FastForwardBy(base::Seconds(20));
  SimulateUserActivity();
  EXPECT_EQ(session_start_time_, GetSessionStartTimePref());

  // Simulate user activity after 20 seconds.
  runner_->FastForwardBy(base::Seconds(20));
  SimulateUserActivity();
  EXPECT_EQ(session_start_time_, GetSessionStartTimePref());

  // Verify that the timer fires and the session is terminated when the session
  // length limit is reached.
  EXPECT_EQ(0, delegate()->request_sign_out_count());
  runner_->FastForwardUntilNoTasksRemain();
  EXPECT_EQ(1, delegate()->request_sign_out_count());
  EXPECT_EQ(session_start_time_ + base::Seconds(60), session_stop_time_);
}

// Creates a SessionLengthLimiter after setting a 60 second limit, allows 50
// seconds of session time to pass, then increases the limit to 90 seconds.
// Verifies that when the session time reaches the new 90 second limit, the
// session is terminated.
TEST_F(SessionLengthLimiterTest, RunAndIncreaseSessionLengthLimit) {
  base::SingleThreadTaskRunner::CurrentDefaultHandle runner_handle(runner_);

  // Set a 60 second session time limit.
  SetSessionLengthLimitPref(base::Seconds(60));

  CreateSessionLengthLimiter(false);

  // Fast forward the time by 50 seconds, verifying that no timer fires to
  // terminate the session.
  runner_->FastForwardBy(base::Seconds(50));

  // Increase the session length limit to 90 seconds.
  SetSessionLengthLimitPref(base::Seconds(90));

  // Verify that the the timer fires and the session is terminated when the
  // session length limit is reached.
  EXPECT_EQ(0, delegate()->request_sign_out_count());
  runner_->FastForwardUntilNoTasksRemain();
  EXPECT_EQ(1, delegate()->request_sign_out_count());
  EXPECT_EQ(session_start_time_ + base::Seconds(90), session_stop_time_);
}

// Creates a SessionLengthLimiter after setting a 60 second limit, allows 50
// seconds of session time to pass, then decreases the limit to 40 seconds.
// Verifies that when the limit is decreased to 40 seconds after 50 seconds of
// session time have passed, the next timer tick causes the session to be
// terminated.
TEST_F(SessionLengthLimiterTest, RunAndDecreaseSessionLengthLimit) {
  base::SingleThreadTaskRunner::CurrentDefaultHandle runner_handle(runner_);

  // Set a 60 second session time limit.
  SetSessionLengthLimitPref(base::Seconds(60));

  CreateSessionLengthLimiter(false);

  // Fast forward the time by 50 seconds, verifying that no timer fires to
  // terminate the session.
  runner_->FastForwardBy(base::Seconds(50));

  // Verify that reducing the session length limit below the 50 seconds that
  // have already elapsed causes the session to be terminated immediately.
  EXPECT_EQ(0, delegate()->request_sign_out_count());
  SetSessionLengthLimitPref(base::Seconds(40));
  EXPECT_EQ(1, delegate()->request_sign_out_count());
  EXPECT_EQ(session_start_time_ + base::Seconds(50), session_stop_time_);
}

// Creates a SessionLengthLimiter after setting a 60 second limit, allows 50
// seconds of session time to pass, then removes the limit. Verifies that after
// the limit is removed, the session is not terminated when the session time
// reaches the original 60 second limit.
TEST_F(SessionLengthLimiterTest, RunAndRemoveSessionLengthLimit) {
  base::SingleThreadTaskRunner::CurrentDefaultHandle runner_handle(runner_);

  // Set a 60 second session time limit.
  SetSessionLengthLimitPref(base::Seconds(60));

  CreateSessionLengthLimiter(false);

  // Fast forward the time by 50 seconds, verifying that no timer fires to
  // terminate the session.
  runner_->FastForwardBy(base::Seconds(50));

  // Remove the session length limit.
  ClearSessionLengthLimitPref();

  // Verify that no timer fires to terminate the session.
  runner_->FastForwardUntilNoTasksRemain();
}

// Tests that session is stopped immediately if limit was hit with when device
// was suspended.
TEST_F(SessionLengthLimiterTest, SuspendAndStop) {
  base::SingleThreadTaskRunner::CurrentDefaultHandle runner_handle(runner_);

  // Set a 60 second session time limit.
  SetSessionLengthLimitPref(base::Seconds(60));

  CreateSessionLengthLimiter(false);

  // Verify that the timer fires and the session is terminated when the session
  // length limit is reached.
  EXPECT_EQ(0, delegate()->request_sign_out_count());

  // Simulate 30 seconds of the active session, then 60 seconds of sleep
  // (suspended device). Given that session length limit is 60 seconds, it will
  // hit exactly if the middle of sleen (and processed when device is resumed,
  // so real session length will be 90 seconds).
  runner_->FastForwardBy(base::Seconds(30));
  EXPECT_EQ(0, delegate()->request_sign_out_count());
  wall_clock_forwarder_->ForwardWhileSuspended(base::Seconds(60));
  EXPECT_EQ(1, delegate()->request_sign_out_count());
  EXPECT_EQ(session_start_time_ + base::Seconds(90), session_stop_time_);
}

// Tests that session is stopped withing timeout, even when part of session time
// device was suspended.
TEST_F(SessionLengthLimiterTest, SuspendAndRun) {
  base::SingleThreadTaskRunner::CurrentDefaultHandle runner_handle(runner_);

  // Set a 60 second session time limit.
  SetSessionLengthLimitPref(base::Seconds(60));

  CreateSessionLengthLimiter(false);

  // Simulate 20 seconds of the active session, then 30 seconds of sleep
  // (suspended device). Given that session length limit is 60 seconds, and
  // total 50 seconds passed, there will be 10 seconds of the session left (and
  // the second FastForwardBy will hit the limit).
  runner_->FastForwardBy(base::Seconds(20));
  wall_clock_forwarder_->ForwardWhileSuspended(base::Seconds(30));

  // Verify that the timer fires and the session is terminated when the session
  // length limit is reached.

  EXPECT_EQ(0, delegate()->request_sign_out_count());
  runner_->FastForwardBy(base::Seconds(20));
  EXPECT_EQ(1, delegate()->request_sign_out_count());
  EXPECT_EQ(session_start_time_ + base::Seconds(60), session_stop_time_);
}

class DemoModeSessionLengthLimiterTest : public SessionLengthLimiterTest {
 protected:
  void SetUp() override {
    SessionLengthLimiterTest::SetUp();
    features_.InitAndEnableFeature(features::kDemoModeSignIn);

    chromeos::PowerManagerClient::InitializeFake();
    chromeos::PowerPolicyController::Initialize(
        chromeos::FakePowerManagerClient::Get());

    settings_helper_.InstallAttributes()->SetDemoMode();
    settings_helper_.ReplaceDeviceSettingsProviderWithStub();
  }

 private:
  base::test::ScopedFeatureList features_;
  ScopedCrosSettingsTestHelper settings_helper_;
};

// Verifies that local state is correctly updated when waiting for initial user
// activity is toggled and no user activity has occurred yet.
TEST_F(DemoModeSessionLengthLimiterTest, DemoModeForceNotWaitUserActivity) {
  demo_mode::SetDoNothingWhenPowerIdle();
  CreateSessionLengthLimiter(false);

  // Verify that the pref indicating user activity was not set and the session
  // start time was set.
  EXPECT_FALSE(IsSessionUserActivitySeenPrefSet());
  EXPECT_EQ(session_start_time_, GetSessionStartTimePref());

  // Set pref to enable for initial user activity.
  SetWaitForInitialUserActivityPref(true);

  // Verify that the pref indicating user activity was not set and the session
  // start time was.
  EXPECT_FALSE(IsSessionUserActivitySeenPrefSet());
  EXPECT_EQ(session_start_time_, GetSessionStartTimePref());
}

}  // namespace ash
