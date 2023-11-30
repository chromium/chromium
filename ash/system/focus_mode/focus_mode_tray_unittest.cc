// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/focus_mode_tray.h"

#include "ash/constants/ash_features.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/system/focus_mode/focus_mode_controller.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/system/tray/tray_bubble_wrapper.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
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

  // Click the tray to activate the button. The tray should be active.
  LeftClickOn(focus_mode_tray_);
  EXPECT_TRUE(focus_mode_tray_->is_active());

  // Clicking the tray button again should deactivate it.
  LeftClickOn(focus_mode_tray_);
  EXPECT_FALSE(focus_mode_tray_->is_active());

  // Clicking anywhere outside of the bubble, in this case the center of the
  // screen, should also deactivate the tray.
  LeftClickOn(focus_mode_tray_);
  EXPECT_TRUE(focus_mode_tray_->is_active());
  auto* event_generator = GetEventGenerator();
  const gfx::Rect work_area =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          Shell::GetPrimaryRootWindow());
  event_generator->MoveMouseTo(work_area.CenterPoint());
  event_generator->ClickLeftButton();
  EXPECT_FALSE(focus_mode_tray_->is_active());
}

// Tests that when the user clicks the radio button to mark a selected task as
// completed, `TaskItemView` will be animated to be removed from the bubble
// view.
TEST_F(FocusModeTrayTest, MarkTaskAsCompleted) {
  // Enable animations.
  ui::ScopedAnimationDurationScaleMode duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  FocusModeController* controller = FocusModeController::Get();
  controller->set_selected_task_title(u"make a travel plan");

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

}  // namespace ash
