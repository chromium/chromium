// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/focus_mode_tray.h"

#include <optional>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/api/tasks/fake_tasks_client.h"
#include "ash/constants/ash_features.h"
#include "ash/glanceables/common/glanceables_util.h"
#include "ash/root_window_controller.h"
#include "ash/screen_util.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/pill_button.h"
#include "ash/system/focus_mode/focus_mode_controller.h"
#include "ash/system/focus_mode/focus_mode_countdown_view.h"
#include "ash/system/focus_mode/focus_mode_ending_moment_view.h"
#include "ash/system/focus_mode/focus_mode_task_test_utils.h"
#include "ash/system/focus_mode/focus_mode_util.h"
#include "ash/system/progress_indicator/progress_indicator.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/system/tray/tray_bubble_wrapper.h"
#include "ash/system/tray/tray_container.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/image_button.h"
#include "url/gurl.h"

namespace ash {

namespace {

constexpr base::TimeDelta kStartAnimationDelay = base::Milliseconds(300);

}  // namespace

class FocusModeTrayTest : public AshTestBase {
 public:
  FocusModeTrayTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        feature_list_(features::kFocusMode) {}
  ~FocusModeTrayTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    // `g_network_handler` is null in tests, we need to manually set the network
    // connected state. Also, the button and the label under the task item view
    // will be enabled only when the user is online.
    glanceables_util::SetIsNetworkConnectedForTest(true);

    auto& tasks_client =
        CreateFakeTasksClient(AccountId::FromUserEmail("user0@tray"));
    tasks_client.set_http_error(google_apis::ApiErrorCode::HTTP_SUCCESS);
    AddFakeTaskList(tasks_client, "default");
    AddFakeTask(tasks_client, "default", "task1", "Task 1");

    focus_mode_tray_ =
        StatusAreaWidgetTestHelper::GetStatusAreaWidget()->focus_mode_tray();
  }

  void TearDown() override {
    focus_mode_tray_ = nullptr;
    AshTestBase::TearDown();
  }

  void AdvanceClock(base::TimeDelta time_delta) {
    // Note that AdvanceClock() is used here instead of FastForwardBy() to
    // prevent long run time during an ash test session.
    task_environment()->AdvanceClock(time_delta);
    task_environment()->RunUntilIdle();
  }

  // Advances the clock for all but 5 seconds of the supplied `minutes`, and
  // then fast forward for the last 5 seconds in order to give callbacks that
  // are called once every second a chance to run.
  void SkipMinutes(int minutes) {
    task_environment()->AdvanceClock(base::Seconds(minutes * 60 - 5));
    task_environment()->FastForwardBy(base::Seconds(5));
  }

  TrayBubbleView* GetBubbleView() {
    return focus_mode_tray_->bubble_->bubble_view();
  }

  FocusModeTray::TaskItemView* GetTaskItemView() {
    return focus_mode_tray_->task_item_view_.get();
  }

  ProgressIndicator* GetProgressIndicator() {
    return focus_mode_tray_->progress_indicator_.get();
  }

  PillButton* GetEndingMomentExtendTimeButton() {
    return focus_mode_tray_->ending_moment_view_for_testing()
        ->extend_session_duration_button_;
  }

  views::Label* GetCountdownTimeRemainingLabel() {
    return focus_mode_tray_->countdown_view_for_testing()
        ->time_remaining_label_;
  }

  std::optional<TaskId> GetSelectedTask() {
    return focus_mode_tray_->selected_task_;
  }

  bool IsCountdownViewVisible() const {
    return focus_mode_tray_->countdown_view_for_testing()->GetVisible();
  }

  bool IsEndingMomentViewVisible() const {
    return focus_mode_tray_->ending_moment_view_for_testing()->GetVisible();
  }

  // Click outside of the bubble (in this case, the center of the screen).
  void ClickOutsideBubble() {
    auto* event_generator = GetEventGenerator();
    const gfx::Rect work_area =
        screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
            Shell::GetPrimaryRootWindow());
    event_generator->MoveMouseTo(work_area.CenterPoint());
    event_generator->ClickLeftButton();
  }

  void HandleCompleteTaskButton() {
    focus_mode_tray_->HandleCompleteTaskButton();
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  raw_ptr<FocusModeTray> focus_mode_tray_ = nullptr;
};

// Tests that the focus mode tray exists and is not visible or active by
// default.
TEST_F(FocusModeTrayTest, DefaultVisibility) {
  EXPECT_TRUE(focus_mode_tray_);
  EXPECT_FALSE(focus_mode_tray_->GetVisible());
  EXPECT_FALSE(focus_mode_tray_->is_active());
}

// Tests that the focus mode tray appears on the shelf when focus mode begins,
// and disappears when focus mode is turned off.
TEST_F(FocusModeTrayTest, ActiveVisibility) {
  FocusModeController* controller = FocusModeController::Get();
  EXPECT_FALSE(controller->in_focus_session());
  EXPECT_FALSE(focus_mode_tray_->GetVisible());

  // Start the focus session, the tray should appear on the shelf.
  controller->ToggleFocusMode();
  EXPECT_TRUE(controller->in_focus_session());
  EXPECT_TRUE(focus_mode_tray_->GetVisible());

  // End the session, the tray should disappear.
  controller->ToggleFocusMode();
  EXPECT_FALSE(controller->in_focus_session());
  EXPECT_FALSE(focus_mode_tray_->GetVisible());
}

// Tests that the focus mode tray is hidden in the lock screen.
TEST_F(FocusModeTrayTest, HiddenInLockScreen) {
  FocusModeController* controller = FocusModeController::Get();
  EXPECT_FALSE(controller->in_focus_session());
  EXPECT_FALSE(focus_mode_tray_->GetVisible());

  // Start the focus session, the tray should appear on the shelf.
  controller->ToggleFocusMode();
  EXPECT_TRUE(controller->in_focus_session());
  EXPECT_TRUE(focus_mode_tray_->GetVisible());

  // Lock the screen, the tray should be hidden.
  GetSessionControllerClient()->LockScreen();
  EXPECT_FALSE(focus_mode_tray_->GetVisible());

  // Unlock the screen, the tray should be back on the shelf.
  GetSessionControllerClient()->UnlockScreen();
  EXPECT_TRUE(focus_mode_tray_->GetVisible());
}

// Tests that the focus mode tray is hidden and doesn't animate in the lock
// screen.
TEST_F(FocusModeTrayTest, LockScreenNoAnimationAndStaysHidden) {
  FocusModeController* controller = FocusModeController::Get();
  EXPECT_FALSE(controller->in_focus_session());
  EXPECT_FALSE(focus_mode_tray_->GetVisible());

  // Start the focus session, the tray should appear on the shelf.
  controller->ToggleFocusMode();
  EXPECT_TRUE(controller->in_focus_session());
  EXPECT_TRUE(focus_mode_tray_->GetVisible());

  // Lock the screen, the tray should be hidden.
  GetSessionControllerClient()->LockScreen();
  EXPECT_FALSE(focus_mode_tray_->GetVisible());

  // End the session, the tray animation shouldn't trigger.
  controller->ToggleFocusMode();
  EXPECT_FALSE(focus_mode_tray_->GetVisible());

  // Unlock the screen, the tray should still be hidden.
  GetSessionControllerClient()->UnlockScreen();
  EXPECT_FALSE(focus_mode_tray_->GetVisible());
}

// Tests that the focus mode tray can be activated by being clicked, and can
// be deactivated by clicking anywhere outside of the bubble (including on the
// tray again).
TEST_F(FocusModeTrayTest, ClickActivateDeactivate) {
  FocusModeController* controller = FocusModeController::Get();
  EXPECT_FALSE(controller->in_focus_session());
  EXPECT_FALSE(focus_mode_tray_->GetVisible());

  // Start focus mode. The tray should not be active.
  controller->ToggleFocusMode();
  EXPECT_TRUE(focus_mode_tray_->GetVisible());
  EXPECT_FALSE(focus_mode_tray_->is_active());
  EXPECT_EQ(1, GetProgressIndicator()->layer()->opacity());

  // Click the tray to activate the button. The tray should be active.
  LeftClickOn(focus_mode_tray_);
  EXPECT_TRUE(focus_mode_tray_->is_active());
  EXPECT_EQ(0, GetProgressIndicator()->layer()->opacity());

  // Clicking the tray button again should deactivate it.
  LeftClickOn(focus_mode_tray_);
  EXPECT_FALSE(focus_mode_tray_->is_active());
  EXPECT_EQ(1, GetProgressIndicator()->layer()->opacity());

  // Clicking anywhere outside of the bubble should also deactivate the tray.
  LeftClickOn(focus_mode_tray_);
  EXPECT_TRUE(focus_mode_tray_->is_active());
  EXPECT_EQ(0, GetProgressIndicator()->layer()->opacity());
  ClickOutsideBubble();
  EXPECT_FALSE(focus_mode_tray_->is_active());
  EXPECT_EQ(1, GetProgressIndicator()->layer()->opacity());
}

// Tests that when the user clicks the radio button to mark a selected task as
// completed, `TaskItemView` will be animated to be removed from the bubble
// view.
TEST_F(FocusModeTrayTest, MarkTaskAsCompleted) {
  // Enable animations.
  ui::ScopedAnimationDurationScaleMode duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  FocusModeTask task;
  task.task_id = {.list_id = "default", .id = "task1"};
  task.title = "make a travel plan";
  task.updated = base::Time::Now();

  FocusModeController* controller = FocusModeController::Get();
  controller->SetSelectedTask(task);

  //  Start focus mode and click the tray to activate the button.
  controller->ToggleFocusMode();
  LeftClickOn(focus_mode_tray_);
  EXPECT_TRUE(focus_mode_tray_->is_active());

  // A `TaskItemView` will be created because we have a selected task.
  EXPECT_TRUE(GetTaskItemView());

  const auto* const radio_button = focus_mode_tray_->GetRadioButtonForTesting();
  EXPECT_TRUE(radio_button);

  // Click the radio button to mark the selected task as completed.
  LeftClickOn(radio_button);

  AdvanceClock(kStartAnimationDelay);

  auto* bubble_view = GetBubbleView();
  ui::Layer* bubble_view_layer = bubble_view->layer();

  auto* animator = bubble_view_layer->GetAnimator();
  EXPECT_TRUE(animator &&
              animator->IsAnimatingProperty(
                  ui::LayerAnimationElement::AnimatableProperty::BOUNDS));
  // Layer top edge animates down.
  EXPECT_GT(bubble_view_layer->bounds().y(), bubble_view->y());
  // `task_item_view` will be removed at the start of the animation.
  EXPECT_FALSE(GetTaskItemView());
}

// Tests that when the user double clicks the radio button to mark a selected
// task as completed, there is no crash from `AnimateBubbleResize()` getting run
// multiple times in succession.
// Regression test for b/363291923.
TEST_F(FocusModeTrayTest, MarkTaskAsCompletedDoubleClick) {
  // Enable animations.
  ui::ScopedAnimationDurationScaleMode duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  FocusModeTask task;
  task.task_id = {.list_id = "default", .id = "task1"};
  task.title = "make a travel plan";
  task.updated = base::Time::Now();

  FocusModeController* controller = FocusModeController::Get();
  controller->SetSelectedTask(task);

  //  Start focus mode and click the tray to activate the button.
  controller->ToggleFocusMode();
  LeftClickOn(focus_mode_tray_);
  EXPECT_TRUE(focus_mode_tray_->is_active());

  // A `TaskItemView` will be created because we have a selected task.
  EXPECT_TRUE(GetSelectedTask());
  EXPECT_TRUE(GetTaskItemView());

  const auto* const radio_button = focus_mode_tray_->GetRadioButtonForTesting();
  EXPECT_TRUE(radio_button);

  // Double click the radio button to mark the selected task as completed.
  // Theoretically only the first one should do anything, since the second click
  // should return early since `selected_task_` has been reset.
  LeftClickOn(radio_button);
  EXPECT_FALSE(GetSelectedTask());
  LeftClickOn(radio_button);

  AdvanceClock(kStartAnimationDelay);

  // `task_item_view` will be removed at the start of the animation.
  EXPECT_FALSE(GetTaskItemView());
}

// Tests that there is no crash if the user clicks the radio button to mark a
// selected task as completed immediately before the model is attempting to
// update the tray.
// Regression test for b/363291923.
TEST_F(FocusModeTrayTest, MarkTaskAsCompletedBeforeModelUpdate) {
  // Enable animations.
  ui::ScopedAnimationDurationScaleMode duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  FocusModeTask task;
  task.task_id = {.list_id = "default", .id = "task1"};
  task.title = "make a travel plan";
  task.updated = base::Time::Now();

  FocusModeController* controller = FocusModeController::Get();
  controller->SetSelectedTask(task);

  //  Start focus mode and click the tray to activate the button.
  controller->ToggleFocusMode();
  LeftClickOn(focus_mode_tray_);
  EXPECT_TRUE(focus_mode_tray_->is_active());

  // A `TaskItemView` will be created because we have a selected task.
  EXPECT_TRUE(GetSelectedTask());
  EXPECT_TRUE(GetTaskItemView());

  const auto* const radio_button = focus_mode_tray_->GetRadioButtonForTesting();
  EXPECT_TRUE(radio_button);

  // Simulate the case where the button is triggered immediately before the
  // model has updated the task as completed.
  HandleCompleteTaskButton();
  focus_mode_tray_->OnTaskCompleted(task);
  focus_mode_tray_->OnSelectedTaskChanged(std::nullopt);

  AdvanceClock(kStartAnimationDelay);

  // `task_item_view` will be removed at the start of the animation.
  EXPECT_FALSE(GetTaskItemView());
}

// Tests that there is no crash if the user clicks the radio button to mark a
// selected task as completed immediately after the model has updated the tray.
// Regression test for b/363291923.
TEST_F(FocusModeTrayTest, MarkTaskAsCompletedAfterModelUpdate) {
  // Enable animations.
  ui::ScopedAnimationDurationScaleMode duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  FocusModeTask task;
  task.task_id = {.list_id = "default", .id = "task1"};
  task.title = "make a travel plan";
  task.updated = base::Time::Now();

  FocusModeController* controller = FocusModeController::Get();
  controller->SetSelectedTask(task);

  //  Start focus mode and click the tray to activate the button.
  controller->ToggleFocusMode();
  LeftClickOn(focus_mode_tray_);
  EXPECT_TRUE(focus_mode_tray_->is_active());

  // A `TaskItemView` will be created because we have a selected task.
  EXPECT_TRUE(GetSelectedTask());
  EXPECT_TRUE(GetTaskItemView());

  const auto* const radio_button = focus_mode_tray_->GetRadioButtonForTesting();
  EXPECT_TRUE(radio_button);

  // Simulate the case where the button is triggered immediately after the
  // model has updated the task as completed and the UI hasn't had a chance to
  // update yet.
  focus_mode_tray_->OnTaskCompleted(task);
  focus_mode_tray_->OnSelectedTaskChanged(std::nullopt);
  HandleCompleteTaskButton();

  AdvanceClock(kStartAnimationDelay);

  // `task_item_view` will be removed at the start of the animation.
  EXPECT_FALSE(GetTaskItemView());
}

// Tests that the progress indicator progresses as the focus session progresses.
TEST_F(FocusModeTrayTest, ProgressIndicatorProgresses) {
  FocusModeController* controller = FocusModeController::Get();
  controller->SetInactiveSessionDuration(base::Minutes(40));
  controller->ToggleFocusMode();

  // Define a margin of error for floating point math.
  constexpr float allowed_difference = 0.01f;

  // Progress one quarter the way through the session should be near 0.25.
  SkipMinutes(10);
  EXPECT_NEAR(0.25, GetProgressIndicator()->progress().value(),
              allowed_difference);

  // Progress half way through the session should be near .5.
  SkipMinutes(10);
  EXPECT_NEAR(0.5, GetProgressIndicator()->progress().value(),
              allowed_difference);
}

// Tests that the progress indicator is centered within the tray and is the
// correct size.
TEST_F(FocusModeTrayTest, ProgressIndicatorCentered) {
  FocusModeController* controller = FocusModeController::Get();
  controller->ToggleFocusMode();
  EXPECT_EQ(focus_mode_tray_->tray_container()->GetLocalBounds().CenterPoint(),
            GetProgressIndicator()->layer()->bounds().CenterPoint());

  // Check the size since it is set dynamically based on the lamp icon.
  EXPECT_EQ(gfx::Size(32, 32),
            GetProgressIndicator()->layer()->bounds().size());

  // The indicator should still be centered when the shelf is vertically
  // aligned.
  Shelf* shelf = Shell::GetPrimaryRootWindowController()->shelf();
  shelf->SetAlignment(ShelfAlignment::kLeft);
  EXPECT_EQ(focus_mode_tray_->tray_container()->GetLocalBounds().CenterPoint(),
            GetProgressIndicator()->layer()->bounds().CenterPoint());
  EXPECT_EQ(gfx::Size(32, 32),
            GetProgressIndicator()->layer()->bounds().size());

  shelf->SetAlignment(ShelfAlignment::kRight);
  EXPECT_EQ(focus_mode_tray_->tray_container()->GetLocalBounds().CenterPoint(),
            GetProgressIndicator()->layer()->bounds().CenterPoint());
  EXPECT_EQ(gfx::Size(32, 32),
            GetProgressIndicator()->layer()->bounds().size());

  // The indicator should still be centered in tablet mode.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(focus_mode_tray_->tray_container()->GetLocalBounds().CenterPoint(),
            GetProgressIndicator()->layer()->bounds().CenterPoint());
  EXPECT_EQ(gfx::Size(32, 32),
            GetProgressIndicator()->layer()->bounds().size());
}

// Tests that the bubble and all of its focusable components are keyboard
// traversable and have the correct accessible names.
TEST_F(FocusModeTrayTest, BubbleTabbingAndAccessibility) {
  AccessibilityController* accessibility_controller =
      Shell::Get()->accessibility_controller();
  accessibility_controller->spoken_feedback().SetEnabled(true);
  EXPECT_TRUE(accessibility_controller->spoken_feedback().enabled());

  FocusModeController* controller = FocusModeController::Get();
  const std::string task_name = "Task 1";
  const base::TimeDelta session_duration = base::Minutes(40);
  const std::u16string time_remaining = focus_mode_util::GetDurationString(
      session_duration, /*digital_format=*/false);
  controller->SetInactiveSessionDuration(session_duration);

  FocusModeTask task;
  task.task_id = {.list_id = "default", .id = "task1"};
  task.title = task_name;
  task.updated = base::Time::Now();

  controller->SetSelectedTask(task);
  controller->ToggleFocusMode();

  LeftClickOn(focus_mode_tray_);
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_ASH_STATUS_TRAY_FOCUS_MODE_TRAY_BUBBLE_TASK_ACCESSIBLE_NAME,
                time_remaining, base::UTF8ToUTF16(task_name)),
            focus_mode_tray_->GetAccessibleNameForBubble());

  PressAndReleaseKey(ui::VKEY_TAB, ui::EF_NONE);
  views::FocusManager* focus_manager =
      GetBubbleView()->GetWidget()->GetFocusManager();
  EXPECT_EQ(
      l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_FOCUS_MODE_TOGGLE_END_BUTTON_ACCESSIBLE_NAME),
      focus_manager->GetFocusedView()->GetViewAccessibility().GetCachedName());

  PressAndReleaseKey(ui::VKEY_TAB, ui::EF_NONE);
  EXPECT_EQ(
      l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_FOCUS_MODE_INCREASE_TEN_MINUTES_BUTTON_ACCESSIBLE_NAME),
      focus_manager->GetFocusedView()->GetViewAccessibility().GetCachedName());

  PressAndReleaseKey(ui::VKEY_TAB, ui::EF_NONE);
  views::ViewAccessibility& focused_view_a11y =
      focus_manager->GetFocusedView()->GetViewAccessibility();
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_ASH_STATUS_TRAY_FOCUS_MODE_TASK_VIEW_RADIO_BUTTON),
            focused_view_a11y.GetCachedName());
  EXPECT_EQ(base::UTF8ToUTF16(task_name),
            focused_view_a11y.GetCachedDescription());
}

// Tests basic ending moment functionality. If the time expires for the ending
// moment, the tray icon will persist until user interacts with it.
TEST_F(FocusModeTrayTest, EndingMoment) {
  FocusModeController* controller = FocusModeController::Get();
  EXPECT_FALSE(controller->in_focus_session());
  EXPECT_FALSE(focus_mode_tray_->GetVisible());

  // Start a focus session.
  controller->ToggleFocusMode();
  EXPECT_TRUE(controller->in_focus_session());
  EXPECT_TRUE(focus_mode_tray_->GetVisible());

  // Wait for the session duration to complete, and verify that the tray icon is
  // still visible, even though the focus session has ended.
  AdvanceClock(controller->GetSessionDuration());
  EXPECT_FALSE(controller->in_focus_session());
  EXPECT_TRUE(controller->in_ending_moment());
  EXPECT_TRUE(focus_mode_tray_->GetVisible());

  // Verify that the tray icon persists until the user interacts and dismisses
  // it.
  AdvanceClock(base::Minutes(10));
  EXPECT_TRUE(focus_mode_tray_->GetVisible());

  FocusModeTray* focus_mode_tray =
      StatusAreaWidgetTestHelper::GetStatusAreaWidget()->focus_mode_tray();
  LeftClickOn(focus_mode_tray);
  LeftClickOn(focus_mode_tray);
  // controller->ResetFocusSession();
  EXPECT_FALSE(focus_mode_tray_->GetVisible());
}

// Tests that if the tray bubble is open during the ending moment, that the
// bubble will persist until user action terminates it.
TEST_F(FocusModeTrayTest, EndingMomentPersists) {
  base::HistogramTester histogram_tester;

  // Start a focus session.
  FocusModeController* controller = FocusModeController::Get();
  controller->ToggleFocusMode();
  EXPECT_TRUE(controller->in_focus_session());
  EXPECT_TRUE(focus_mode_tray_->GetVisible());

  // Focus session ends.
  AdvanceClock(controller->GetSessionDuration());
  EXPECT_FALSE(controller->in_focus_session());
  EXPECT_TRUE(controller->in_ending_moment());
  EXPECT_TRUE(focus_mode_tray_->GetVisible());
  AdvanceClock(base::Minutes(2));
  EXPECT_TRUE(controller->in_ending_moment());
  EXPECT_TRUE(focus_mode_tray_->GetVisible());

  // Open the tray bubble and wait for an arbitrarily long time. Verify that
  // the bubble is not closed automatically.
  LeftClickOn(focus_mode_tray_);
  EXPECT_TRUE(focus_mode_tray_->is_active());
  AdvanceClock(base::Minutes(2));
  EXPECT_TRUE(focus_mode_tray_->is_active());
  EXPECT_TRUE(focus_mode_tray_->GetVisible());
  EXPECT_TRUE(controller->in_ending_moment());

  // Clicks outside of the tray bubble should close it and terminate the ending
  // moment as well.
  ClickOutsideBubble();
  EXPECT_FALSE(focus_mode_tray_->is_active());
  EXPECT_FALSE(focus_mode_tray_->GetVisible());
  EXPECT_FALSE(controller->in_ending_moment());

  // Verify the histogram.
  histogram_tester.ExpectBucketCount(
      /*name=*/focus_mode_histogram_names::kEndingMomentBubbleActionHistogram,
      /*sample=*/
      focus_mode_histogram_names::EndingMomentBubbleClosedReason::kOpended,
      /*expected_count=*/1);
}

// Verifies that the tray contents are updated between an in-session state and
// the ending moment state.
TEST_F(FocusModeTrayTest, EndingMomentPanelFunctionality) {
  base::TimeDelta kSessionDuration = base::Minutes(20);
  FocusModeController* controller = FocusModeController::Get();
  EXPECT_FALSE(controller->in_focus_session());
  EXPECT_FALSE(focus_mode_tray_->GetVisible());

  controller->SetInactiveSessionDuration(kSessionDuration);

  // Start a focus session
  controller->ToggleFocusMode();
  EXPECT_TRUE(controller->in_focus_session());
  EXPECT_TRUE(focus_mode_tray_->GetVisible());

  // Open the contextual panel and verify that the countdown view is showing.
  LeftClickOn(focus_mode_tray_);
  EXPECT_TRUE(focus_mode_tray_->is_active());
  EXPECT_TRUE(IsCountdownViewVisible());

  // When the focus session ends, verify that the bubble contents have changed
  // and that the ending moment view is showing.
  AdvanceClock(kSessionDuration);
  EXPECT_FALSE(controller->in_focus_session());
  EXPECT_TRUE(controller->in_ending_moment());
  EXPECT_TRUE(focus_mode_tray_->GetVisible());
  ASSERT_TRUE(focus_mode_tray_->is_active());
  EXPECT_FALSE(IsCountdownViewVisible());
  EXPECT_TRUE(IsEndingMomentViewVisible());

  // Verify that the ending moment isn't terminated and that the bubble doesn't
  // close due to time passing.
  AdvanceClock(base::Minutes(2));
  EXPECT_TRUE(controller->in_ending_moment());
  EXPECT_TRUE(focus_mode_tray_->GetVisible());
  ASSERT_TRUE(focus_mode_tray_->is_active());
  EXPECT_TRUE(IsEndingMomentViewVisible());
}

// Tests the `+10 min` button functionality during the ending moment, as well as
// the case where the button should be disabled.
TEST_F(FocusModeTrayTest, EndingMomentUpdateSessionDuration) {
  // Set the session duration to 20 minutes shy of the maximum duration and turn
  // on focus mode. This will allow us to extend twice in this test.
  const base::TimeDelta kStartingDuration =
      focus_mode_util::kMaximumDuration -
      (2 * focus_mode_util::kExtendDuration);
  FocusModeController* controller = FocusModeController::Get();
  controller->SetInactiveSessionDuration(kStartingDuration);
  controller->ToggleFocusMode();

  // Advance the clock to end the focus session, then verify that the correct
  // ending moment UI is displayed.
  AdvanceClock(kStartingDuration);
  EXPECT_TRUE(focus_mode_tray_->GetVisible());
  LeftClickOn(focus_mode_tray_);
  EXPECT_TRUE(IsEndingMomentViewVisible());

  // Since the session duration isn't at the maximum, the button should be
  // enabled to allow adding more time.
  auto* button = GetEndingMomentExtendTimeButton();
  EXPECT_TRUE(button->GetEnabled());

  // Wait a minute and verify that the bubble hasn't closed.
  AdvanceClock(base::Minutes(1));
  EXPECT_TRUE(focus_mode_tray_->GetBubbleView());

  // Extend the session duration and verify that the UI has swapped back to the
  // countdown view.
  LeftClickOn(button);
  EXPECT_TRUE(focus_mode_tray_->GetVisible());
  EXPECT_TRUE(focus_mode_tray_->GetBubbleView());
  EXPECT_TRUE(IsCountdownViewVisible());
  EXPECT_FALSE(IsEndingMomentViewVisible());

  // Verify that the session duration and ending time wasn't affected by the
  // time the user was waiting in the ending moment.
  EXPECT_EQ(kStartingDuration + focus_mode_util::kExtendDuration,
            controller->GetSessionDuration());
  EXPECT_EQ(u"10:00", GetCountdownTimeRemainingLabel()->GetText());

  // Allow the timer to expire, then extend by another `kExtendDuration` to
  // reach the maximum session duration.
  AdvanceClock(focus_mode_util::kExtendDuration);
  LeftClickOn(button);
  AdvanceClock(focus_mode_util::kExtendDuration);
  // Verify that when the ending moment triggers, that the `+10 min` button is
  // now disabled since the session duration can no longer be extended.
  EXPECT_TRUE(focus_mode_tray_->GetVisible());
  EXPECT_TRUE(focus_mode_tray_->GetBubbleView());
  EXPECT_FALSE(IsCountdownViewVisible());
  EXPECT_TRUE(IsEndingMomentViewVisible());
  EXPECT_FALSE(button->GetEnabled());
  EXPECT_EQ(focus_mode_util::kMaximumDuration,
            controller->current_session()->session_duration());
}

// Tests that the ending moment functions correctly on multiple displays and
// does not terminate unexpectedly.
// Regression test for b/323982290.
TEST_F(FocusModeTrayTest, EndingMomentMultiDisplay) {
  UpdateDisplay("800x600,800x600");
  FocusModeTray* first_tray = focus_mode_tray_;
  FocusModeTray* second_tray =
      StatusAreaWidgetTestHelper::GetSecondaryStatusAreaWidget()
          ->focus_mode_tray();

  // Start a focus session and verify both trays are visible.
  FocusModeController* controller = FocusModeController::Get();
  controller->ToggleFocusMode();
  EXPECT_TRUE(controller->in_focus_session());
  EXPECT_TRUE(first_tray->GetVisible());
  EXPECT_TRUE(second_tray->GetVisible());

  // Trigger the ending moment.
  AdvanceClock(controller->GetSessionDuration());
  EXPECT_FALSE(controller->in_focus_session());
  EXPECT_TRUE(controller->in_ending_moment());
  EXPECT_TRUE(first_tray->GetVisible());
  EXPECT_TRUE(second_tray->GetVisible());

  // Click on the tray on the first display to open the associated tray bubble.
  LeftClickOn(first_tray);
  EXPECT_TRUE(first_tray->is_active());
  EXPECT_TRUE(first_tray->GetBubbleView());
  EXPECT_FALSE(second_tray->is_active());
  EXPECT_FALSE(second_tray->GetBubbleView());

  // Click on the tray on the second display. The ending moment should persist.
  // This should also close the bubble on the first display and show the bubble
  // on the second display.
  LeftClickOn(second_tray);
  EXPECT_TRUE(controller->in_ending_moment());
  EXPECT_TRUE(first_tray->GetVisible());
  EXPECT_FALSE(first_tray->is_active());
  EXPECT_FALSE(first_tray->GetBubbleView());
  EXPECT_TRUE(second_tray->GetVisible());
  EXPECT_TRUE(second_tray->is_active());
  EXPECT_TRUE(second_tray->GetBubbleView());

  // Clicking the same (second) tray again should close the bubble and terminate
  // the ending moment as well.
  LeftClickOn(second_tray);
  EXPECT_FALSE(controller->in_ending_moment());
  EXPECT_FALSE(first_tray->GetVisible());
  EXPECT_FALSE(second_tray->GetVisible());
  EXPECT_FALSE(second_tray->GetBubbleView());
}

}  // namespace ash
