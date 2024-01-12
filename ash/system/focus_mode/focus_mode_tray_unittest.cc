// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/focus_mode_tray.h"

#include "ash/accessibility/accessibility_controller.h"
#include "ash/api/tasks/tasks_types.h"
#include "ash/constants/ash_features.h"
#include "ash/root_window_controller.h"
#include "ash/screen_util.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/focus_mode/focus_mode_controller.h"
#include "ash/system/focus_mode/focus_mode_util.h"
#include "ash/system/progress_indicator/progress_indicator.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/system/tray/tray_bubble_wrapper.h"
#include "ash/system/tray/tray_container.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/test/scoped_feature_list.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/views/controls/button/image_button.h"

namespace ash {

namespace {

constexpr base::TimeDelta kStartAnimationDelay = base::Milliseconds(300);

}  // namespace

class FocusModeTrayTest : public AshTestBase {
 public:
  FocusModeTrayTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~FocusModeTrayTest() override = default;

  // AshTestBase:
  void SetUp() override {
    feature_list_.InitAndEnableFeature(features::kFocusMode);
    AshTestBase::SetUp();

    focus_mode_tray_ =
        StatusAreaWidgetTestHelper::GetStatusAreaWidget()->focus_mode_tray();
  }

  void TearDown() override {
    focus_mode_tray_ = nullptr;
    AshTestBase::TearDown();
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

  // Advances the clock for all but 5 seconds of the supplied `minutes`, and
  // then fast forward for the last 5 seconds in order to give callbacks that
  // are called once every second a chance to run.
  void SkipMinutes(int minutes) {
    task_environment()->AdvanceClock(base::Seconds(minutes * 60 - 5));
    task_environment()->FastForwardBy(base::Seconds(5));
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

  // Clicking anywhere outside of the bubble, in this case the center of the
  // screen, should also deactivate the tray.
  LeftClickOn(focus_mode_tray_);
  EXPECT_TRUE(focus_mode_tray_->is_active());
  EXPECT_EQ(0, GetProgressIndicator()->layer()->opacity());
  auto* event_generator = GetEventGenerator();
  const gfx::Rect work_area =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          Shell::GetPrimaryRootWindow());
  event_generator->MoveMouseTo(work_area.CenterPoint());
  event_generator->ClickLeftButton();
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

  FocusModeController* controller = FocusModeController::Get();
  controller->SetSelectedTask(std::make_unique<api::Task>(
                                  /*id=*/base::NumberToString(0),
                                  "make a travel plan", /*completed=*/false,
                                  /*due=*/absl::nullopt, /*has_subtasks=*/false,
                                  /*has_email_link=*/false,
                                  /*has_notes=*/false,
                                  /*updated=*/base::Time::Now())
                                  .get());

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

  task_environment()->FastForwardBy(kStartAnimationDelay);

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
  const std::string task_name = "Podcast interview script";
  const base::TimeDelta session_duration = base::Minutes(40);
  const std::u16string time_remaining = focus_mode_util::GetDurationString(
      session_duration, /*digital_format=*/false);
  controller->SetInactiveSessionDuration(session_duration);
  controller->SetSelectedTask(std::make_unique<api::Task>(
                                  /*id=*/base::NumberToString(1), task_name,
                                  /*completed=*/false,
                                  /*due=*/absl::nullopt, /*has_subtasks=*/false,
                                  /*has_email_link=*/false,
                                  /*has_notes=*/false,
                                  /*updated=*/base::Time::Now())
                                  .get());
  controller->ToggleFocusMode();

  LeftClickOn(focus_mode_tray_);
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_ASH_STATUS_TRAY_FOCUS_MODE_TRAY_BUBBLE_TASK_ACCESSIBLE_NAME,
                time_remaining, base::UTF8ToUTF16(task_name)),
            focus_mode_tray_->GetAccessibleNameForBubble());

  PressAndReleaseKey(ui::VKEY_TAB, ui::EF_NONE);
  views::FocusManager* focus_manager =
      GetBubbleView()->GetWidget()->GetFocusManager();
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_ASH_STATUS_TRAY_FOCUS_MODE_TOGGLE_END_BUTTON),
            focus_manager->GetFocusedView()->GetAccessibleName());

  PressAndReleaseKey(ui::VKEY_TAB, ui::EF_NONE);
  EXPECT_EQ(
      l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_FOCUS_MODE_EXTEND_TEN_MINUTES_BUTTON_ACCESSIBLE_NAME),
      focus_manager->GetFocusedView()->GetAccessibleName());

  PressAndReleaseKey(ui::VKEY_TAB, ui::EF_NONE);
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_ASH_STATUS_TRAY_FOCUS_MODE_TASK_RADIO_BUTTON),
            focus_manager->GetFocusedView()->GetAccessibleName());
}

}  // namespace ash
