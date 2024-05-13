// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/focus_mode_controller.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/ash_prefs.h"
#include "ash/shell.h"
#include "ash/system/focus_mode/focus_mode_controller.h"
#include "ash/system/focus_mode/focus_mode_session.h"
#include "ash/system/focus_mode/focus_mode_task_test_utils.h"
#include "ash/system/focus_mode/focus_mode_tray.h"
#include "ash/system/focus_mode/focus_mode_util.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/system/toast/anchored_nudge_manager_impl.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/test/ash_test_base.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "url/gurl.h"

namespace ash {

namespace {

constexpr char kUser1Email[] = "user1@focusmode";
constexpr char kUser2Email[] = "user2@focusmode";

bool IsEndingMomentNudgeShown() {
  return Shell::Get()->anchored_nudge_manager()->IsNudgeShown(
      focus_mode_util::kFocusModeEndingMomentNudgeId);
}

}  // namespace

class FocusModeControllerMultiUserTest : public NoSessionAshTestBase {
 public:
  FocusModeControllerMultiUserTest()
      : NoSessionAshTestBase(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        scoped_feature_(features::kFocusMode) {}
  ~FocusModeControllerMultiUserTest() override = default;

  TestingPrefServiceSimple* user_1_prefs() { return user_1_prefs_; }
  TestingPrefServiceSimple* user_2_prefs() { return user_2_prefs_; }

  // NoSessionAshTestBase:
  void SetUp() override {
    NoSessionAshTestBase::SetUp();

    TestSessionControllerClient* session_controller =
        GetSessionControllerClient();
    session_controller->Reset();

    // Inject our own PrefServices for each user which enables us to setup the
    // Focus Mode restore data before the user signs in.
    auto user_1_prefs = std::make_unique<TestingPrefServiceSimple>();
    user_1_prefs_ = user_1_prefs.get();
    RegisterUserProfilePrefs(user_1_prefs_->registry(), /*country=*/"",
                             /*for_test=*/true);
    auto user_2_prefs = std::make_unique<TestingPrefServiceSimple>();
    user_2_prefs_ = user_2_prefs.get();
    RegisterUserProfilePrefs(user_2_prefs_->registry(), /*country=*/"",
                             /*for_test=*/true);
    session_controller->AddUserSession(kUser1Email,
                                       user_manager::UserType::kRegular,
                                       /*provide_pref_service=*/false);
    session_controller->SetUserPrefService(GetUser1AccountId(),
                                           std::move(user_1_prefs));
    session_controller->AddUserSession(kUser2Email,
                                       user_manager::UserType::kRegular,
                                       /*provide_pref_service=*/false);
    session_controller->SetUserPrefService(GetUser2AccountId(),
                                           std::move(user_2_prefs));
  }

  void TearDown() override {
    user_1_prefs_ = nullptr;
    user_2_prefs_ = nullptr;
    NoSessionAshTestBase::TearDown();
  }

  AccountId GetUser1AccountId() const {
    return AccountId::FromUserEmail(kUser1Email);
  }

  AccountId GetUser2AccountId() const {
    return AccountId::FromUserEmail(kUser2Email);
  }

  void SwitchActiveUser(const AccountId& account_id) {
    GetSessionControllerClient()->SwitchActiveUser(account_id);
  }

  void SimulateUserLogin(const AccountId& account_id) {
    SwitchActiveUser(account_id);
    GetSessionControllerClient()->SetSessionState(
        session_manager::SessionState::ACTIVE);
  }

  void AdvanceClock(base::TimeDelta time_delta) {
    // Note that AdvanceClock() is used here instead of FastForwardBy() to
    // prevent long run time during an ash test session.
    task_environment()->AdvanceClock(time_delta);
    task_environment()->RunUntilIdle();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_;
  raw_ptr<TestingPrefServiceSimple> user_1_prefs_ = nullptr;
  raw_ptr<TestingPrefServiceSimple> user_2_prefs_ = nullptr;
};

// Tests that the default Focus Mode prefs are registered, and that they are
// read correctly by `FocusModeController`. Also test that switching users will
// load new user prefs.
TEST_F(FocusModeControllerMultiUserTest, LoadUserPrefsAndSwitchUsers) {
  constexpr base::TimeDelta kDefaultSessionDuration = base::Minutes(25);
  constexpr bool kDefaultDNDState = true;
  constexpr base::TimeDelta kUser2SessionDuration = base::Minutes(200);
  constexpr bool kUser2DNDState = false;

  // Set the secondary user2's Focus Mode prefs.
  user_2_prefs()->SetTimeDelta(prefs::kFocusModeSessionDuration,
                               kUser2SessionDuration);
  user_2_prefs()->SetBoolean(prefs::kFocusModeDoNotDisturb, kUser2DNDState);

  // Log in and check to see that the user1 prefs are the default values, since
  // there should have been nothing previously.
  SimulateUserLogin(GetUser1AccountId());
  EXPECT_EQ(kDefaultSessionDuration,
            user_1_prefs()->GetTimeDelta(prefs::kFocusModeSessionDuration));
  EXPECT_EQ(kDefaultDNDState,
            user_1_prefs()->GetBoolean(prefs::kFocusModeDoNotDisturb));

  // Verify that `FocusModeController` has loaded the user prefs.
  auto* controller = FocusModeController::Get();
  EXPECT_EQ(kDefaultSessionDuration, controller->GetSessionDuration());
  EXPECT_EQ(kDefaultDNDState, controller->turn_on_do_not_disturb());

  // Switch users and verify that `FocusModeController` has loaded the new user
  // prefs.
  SwitchActiveUser(GetUser2AccountId());
  EXPECT_EQ(kUser2SessionDuration, controller->GetSessionDuration());
  EXPECT_EQ(kUser2DNDState, controller->turn_on_do_not_disturb());
}

TEST_F(FocusModeControllerMultiUserTest, ToggleClosesSystemBubble) {
  SimulateUserLogin(GetUser1AccountId());

  auto* controller = FocusModeController::Get();
  EXPECT_FALSE(controller->in_focus_session());

  // Show the bubble.
  auto* system_tray = GetPrimaryUnifiedSystemTray();
  system_tray->ShowBubble();

  // Toggle focus mode on, and verify that the bubble is closed.
  controller->ToggleFocusMode();
  EXPECT_TRUE(controller->in_focus_session());
  EXPECT_FALSE(system_tray->IsBubbleShown());

  // Show the bubble again.
  system_tray->ShowBubble();

  // Toggle focus mode off, and verify that this doesn't affect the bubble
  // visibility.
  controller->ToggleFocusMode();
  EXPECT_FALSE(controller->in_focus_session());
  EXPECT_TRUE(system_tray->IsBubbleShown());
}

// Tests that we can determine if a focus session has started before.
TEST_F(FocusModeControllerMultiUserTest, FirstTimeUserFlow) {
  SimulateUserLogin(GetUser1AccountId());
  auto* controller = FocusModeController::Get();
  EXPECT_FALSE(controller->HasStartedSessionBefore());

  FocusModeController::Get()->ToggleFocusMode();
  EXPECT_TRUE(controller->HasStartedSessionBefore());
}

// Tests adding and completing tasks.
TEST_F(FocusModeControllerMultiUserTest, TasksFlow) {
  SimulateUserLogin(GetUser1AccountId());

  const std::string task_list_id = "list1";
  const std::string task_id = "task1";
  const std::string title = "Focus Task";

  auto& tasks_client = CreateFakeTasksClient(GetUser1AccountId());
  AddFakeTaskList(tasks_client, task_list_id);
  AddFakeTask(tasks_client, task_list_id, task_id, title);

  FocusModeTask task;
  task.task_list_id = task_list_id;
  task.task_id = task_id;
  task.title = title;
  task.updated = base::Time::Now();

  // Verify that initially there is no selected task.
  auto* controller = FocusModeController::Get();
  EXPECT_FALSE(controller->HasSelectedTask());

  // Select a task, and verify that the task data is accurate.
  controller->SetSelectedTask(task);
  EXPECT_TRUE(controller->HasSelectedTask());
  EXPECT_EQ(task_id, controller->selected_task_id());
  EXPECT_EQ(title, controller->selected_task_title());

  // Complete the task, and verify that the task data is cleared.
  controller->CompleteTask();
  EXPECT_FALSE(controller->HasSelectedTask());
}

// Tests basic ending moment functionality. Includes starting a new session
// after the previous ending moment terminates.
TEST_F(FocusModeControllerMultiUserTest, EndingMoment) {
  SimulateUserLogin(GetUser1AccountId());
  base::TimeDelta kSessionDuration = base::Minutes(20);

  auto* controller = FocusModeController::Get();
  controller->SetInactiveSessionDuration(kSessionDuration);
  EXPECT_FALSE(controller->current_session().has_value());

  // Toggling focus mode on creates a `current_session`. Verify that we are not
  // in the ending moment.
  controller->ToggleFocusMode();
  EXPECT_TRUE(controller->current_session().has_value());
  EXPECT_TRUE(controller->in_focus_session());

  // Once the session expires, the ending moment should start.
  AdvanceClock(kSessionDuration);
  EXPECT_TRUE(controller->in_ending_moment());

  // Verifies that the ending moment terminates after the specified duration and
  // that there is no longer an existing `current_session`.
  AdvanceClock(focus_mode_util::kEndingMomentDuration);
  EXPECT_FALSE(controller->current_session().has_value());

  // Toggling on a focus session after the previous one expires creates
  // a new `current_session`.
  controller->ToggleFocusMode();
  EXPECT_TRUE(controller->current_session().has_value());
  EXPECT_TRUE(controller->in_focus_session());
}

// Tests that we can start a new/separate focus session during an ongoing ending
// moment.
TEST_F(FocusModeControllerMultiUserTest, StartNewSessionDuringEndingMoment) {
  SimulateUserLogin(GetUser1AccountId());
  base::TimeDelta kSessionDuration = base::Minutes(20);

  // Case 1: Normal ending moment timeout.
  auto* controller = FocusModeController::Get();
  controller->SetInactiveSessionDuration(kSessionDuration);
  EXPECT_FALSE(controller->current_session().has_value());

  // Toggle focus mode on, and verify that we are not in the ending moment.
  controller->ToggleFocusMode();
  EXPECT_TRUE(controller->in_focus_session());

  // Once the session expires, the ending moment should start.
  AdvanceClock(kSessionDuration);
  EXPECT_TRUE(controller->in_ending_moment());

  // Toggling focus mode during the ending moment will start a new session and
  // terminate the ending moment.
  controller->ToggleFocusMode();
  EXPECT_TRUE(controller->current_session().has_value());
  EXPECT_TRUE(controller->in_focus_session());
}

// Tests basic ending moment nudge functionality. Includes the nudge appearing
// and disappearing.
TEST_F(FocusModeControllerMultiUserTest, EndingMomentNudgeTest) {
  SimulateUserLogin(GetUser1AccountId());
  base::TimeDelta kSessionDuration = base::Minutes(20);

  auto* controller = FocusModeController::Get();
  controller->SetInactiveSessionDuration(kSessionDuration);

  auto trigger_ending_moment = [this, controller, kSessionDuration]() {
    // Toggling focus mode on and verify that we are not in the ending moment.
    controller->ToggleFocusMode();
    EXPECT_TRUE(controller->in_focus_session());

    // Once the session expires, the ending moment should start.
    AdvanceClock(kSessionDuration);
    EXPECT_TRUE(controller->in_ending_moment());
  };

  // Verify that the nudge is visible when the ending moment is triggered.
  trigger_ending_moment();
  EXPECT_TRUE(IsEndingMomentNudgeShown());

  // Verify that the ending moment terminating also hides the nudge.
  AdvanceClock(focus_mode_util::kEndingMomentDuration);
  EXPECT_FALSE(controller->current_session().has_value());
  EXPECT_FALSE(IsEndingMomentNudgeShown());

  // Trigger the ending moment, then check that the notification disappears when
  // we open the tray bubble.
  trigger_ending_moment();
  EXPECT_TRUE(IsEndingMomentNudgeShown());
  FocusModeTray* focus_mode_tray =
      StatusAreaWidgetTestHelper::GetStatusAreaWidget()->focus_mode_tray();
  LeftClickOn(focus_mode_tray);
  EXPECT_TRUE(controller->in_ending_moment());
  EXPECT_FALSE(IsEndingMomentNudgeShown());
}

// Verify that when toggling a focus mode, the histogram will record the initial
// session duration.
TEST_F(FocusModeControllerMultiUserTest, CheckInitialDurationHistograms) {
  base::HistogramTester histogram_tester;

  auto* controller = FocusModeController::Get();
  EXPECT_FALSE(controller->in_focus_session());

  // 1. Start a focus session with the minimum session duration.
  controller->SetInactiveSessionDuration(focus_mode_util::kMinimumDuration);
  controller->ToggleFocusMode();
  EXPECT_TRUE(controller->in_focus_session());
  histogram_tester.ExpectBucketCount(
      focus_mode_histogram_names::kInitialDurationOnSessionStartsHistogramName,
      focus_mode_util::kMinimumDuration.InMinutes(), 1);

  controller->ToggleFocusMode();
  EXPECT_FALSE(controller->in_focus_session());

  // 2. Start a new focus session with the maximum session duration.
  controller->SetInactiveSessionDuration(focus_mode_util::kMaximumDuration);
  controller->ToggleFocusMode();
  EXPECT_TRUE(controller->in_focus_session());
  histogram_tester.ExpectBucketCount(
      focus_mode_histogram_names::kInitialDurationOnSessionStartsHistogramName,
      focus_mode_util::kMaximumDuration.InMinutes(), 1);

  histogram_tester.ExpectTotalCount(
      focus_mode_histogram_names::kInitialDurationOnSessionStartsHistogramName,
      2);
}

// Verify that when we start a focus session, the histogram will record whether
// there is a selected task or not.
TEST_F(FocusModeControllerMultiUserTest, CheckHasSelectedTaskHistogram) {
  base::HistogramTester histogram_tester;

  auto* controller = FocusModeController::Get();
  EXPECT_FALSE(controller->in_focus_session());
  histogram_tester.ExpectTotalCount(
      focus_mode_histogram_names::kHasSelectedTaskOnSessionStartHistogramName,
      0);

  // 1. Start a focus session without a selected task.
  EXPECT_FALSE(controller->HasSelectedTask());
  controller->ToggleFocusMode();
  EXPECT_TRUE(controller->in_focus_session());
  histogram_tester.ExpectBucketCount(
      focus_mode_histogram_names::kHasSelectedTaskOnSessionStartHistogramName,
      false, 1);
  controller->ToggleFocusMode();
  EXPECT_FALSE(controller->in_focus_session());

  // 2. Start a focus session with a selected task.
  FocusModeTask task;
  task.task_list_id = "abc";
  task.task_id = "1";
  task.title = "Focus Task";
  task.updated = base::Time::Now();

  controller->SetSelectedTask(task);
  EXPECT_TRUE(controller->HasSelectedTask());

  controller->ToggleFocusMode();
  EXPECT_TRUE(controller->in_focus_session());
  histogram_tester.ExpectBucketCount(
      focus_mode_histogram_names::kHasSelectedTaskOnSessionStartHistogramName,
      true, 1);
}

// Tests that the histogram will record how many tasks we selected during a
// focus session.
TEST_F(FocusModeControllerMultiUserTest, CheckTasksSelectedHistogram) {
  base::HistogramTester histogram_tester;

  auto* controller = FocusModeController::Get();
  EXPECT_FALSE(controller->in_focus_session());

  // Start a focus session.
  controller->ToggleFocusMode();
  EXPECT_TRUE(controller->in_focus_session());
  EXPECT_FALSE(controller->HasSelectedTask());

  // Select a task during the session.
  FocusModeTask task;
  task.task_list_id = "abc";
  task.task_id = "1";
  task.title = "Focus Task";
  task.updated = base::Time::Now();

  controller->SetSelectedTask(task);
  EXPECT_TRUE(controller->HasSelectedTask());

  // Remove the task and select a new one.
  controller->SetSelectedTask({});

  task.task_list_id = "abc";
  task.task_id = "2";
  task.title = "A New Focus Task";
  task.updated = base::Time::Now();
  controller->SetSelectedTask(task);

  // End the focus session and check the count.
  controller->ToggleFocusMode();
  EXPECT_FALSE(controller->in_focus_session());

  histogram_tester.ExpectBucketCount(
      focus_mode_histogram_names::kTasksSelectedHistogramName, /*sample=*/2,
      /*expected_count=*/1);
}

// Verify that when a session is over, the histogram will record which type of
// the DND state.
TEST_F(FocusModeControllerMultiUserTest, CheckDNDStateOnFocusEndHistogram) {
  base::HistogramTester histogram_tester;

  auto* controller = FocusModeController::Get();
  auto* message_center = message_center::MessageCenter::Get();

  // 1. FocusModeOn - DND enabled by FocusMode (default behavior).
  EXPECT_FALSE(controller->in_focus_session());
  EXPECT_TRUE(controller->turn_on_do_not_disturb());
  EXPECT_FALSE(message_center->IsQuietMode());

  // Turning on Focus Mode will also turn on DND by default.
  controller->ToggleFocusMode();
  EXPECT_TRUE(controller->in_focus_session());
  EXPECT_TRUE(message_center->IsQuietMode());

  controller->ToggleFocusMode();
  EXPECT_FALSE(controller->in_focus_session());
  histogram_tester.ExpectBucketCount(
      /*name=*/focus_mode_histogram_names::kDNDStateOnFocusEndHistogramName,
      /*sample=*/
      focus_mode_histogram_names::DNDStateOnFocusEndType::kFocusModeOn,
      /*expected_count=*/1);

  // 2. AlreadyOn - DND was already on before FocusMode started and was on when
  // we finished (with NO user interaction during the session).
  message_center->SetQuietMode(true);
  EXPECT_TRUE(message_center->IsQuietMode());

  // Start and end a Focus Mode session.
  controller->ToggleFocusMode();
  EXPECT_TRUE(controller->in_focus_session());

  controller->ToggleFocusMode();
  EXPECT_FALSE(controller->in_focus_session());
  EXPECT_TRUE(message_center->IsQuietMode());
  histogram_tester.ExpectBucketCount(
      /*name=*/focus_mode_histogram_names::kDNDStateOnFocusEndHistogramName,
      /*sample=*/
      focus_mode_histogram_names::DNDStateOnFocusEndType::kAlreadyOn,
      /*expected_count=*/1);

  // 3. AlreadyOff - DND was off when FocusMode started, and is still off (with
  // NO user interactions during the session).
  message_center->SetQuietMode(false);
  EXPECT_FALSE(message_center->IsQuietMode());

  // Set `turn_on_do_not_disturb` to prevent Focus Mode from automatically
  // turning on DND when a session starts.
  controller->set_turn_on_do_not_disturb(false);

  // Start a session and end it.
  controller->ToggleFocusMode();
  EXPECT_TRUE(controller->in_focus_session());
  EXPECT_FALSE(message_center->IsQuietMode());

  controller->ToggleFocusMode();
  EXPECT_FALSE(controller->in_focus_session());
  EXPECT_FALSE(message_center->IsQuietMode());
  histogram_tester.ExpectBucketCount(
      /*name=*/focus_mode_histogram_names::kDNDStateOnFocusEndHistogramName,
      /*sample=*/
      focus_mode_histogram_names::DNDStateOnFocusEndType::kAlreadyOff,
      /*expected_count=*/1);

  // 4. TurnedOn - The user manually toggled DND during the focus session, and
  // the session ends with DND on.
  controller->ToggleFocusMode();
  EXPECT_TRUE(controller->in_focus_session());
  EXPECT_FALSE(message_center->IsQuietMode());

  // Turn on DND during the session.
  message_center->SetQuietMode(true);
  controller->ToggleFocusMode();
  EXPECT_FALSE(controller->in_focus_session());
  EXPECT_TRUE(message_center->IsQuietMode());
  histogram_tester.ExpectBucketCount(
      /*name=*/focus_mode_histogram_names::kDNDStateOnFocusEndHistogramName,
      /*sample=*/
      focus_mode_histogram_names::DNDStateOnFocusEndType::kTurnedOn,
      /*expected_count=*/1);

  // 5. TurnedOff - The user manually toggled DND during the focus session, and
  // the session ends with DND off.
  controller->ToggleFocusMode();
  EXPECT_TRUE(controller->in_focus_session());

  // Turn off DND during the session.
  message_center->SetQuietMode(false);
  controller->ToggleFocusMode();
  EXPECT_FALSE(controller->in_focus_session());
  EXPECT_FALSE(message_center->IsQuietMode());
  histogram_tester.ExpectBucketCount(
      /*name=*/focus_mode_histogram_names::kDNDStateOnFocusEndHistogramName,
      /*sample=*/
      focus_mode_histogram_names::DNDStateOnFocusEndType::kTurnedOff,
      /*expected_count=*/1);
}

}  // namespace ash
