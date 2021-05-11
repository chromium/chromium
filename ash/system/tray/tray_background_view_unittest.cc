// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tray_background_view.h"

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/accessibility/dictation_button_tray.h"
#include "ash/system/status_area_widget_delegate.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/test/ash_test_base.h"
#include "base/test/task_environment.h"
#include "components/user_manager/user_manager.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/scoped_layer_animation_settings.h"

namespace ash {

class TrayBackgroundViewTestView : public TrayBackgroundView {
 public:
  explicit TrayBackgroundViewTestView(Shelf* shelf)
      : TrayBackgroundView(shelf) {}

  TrayBackgroundViewTestView(const TrayBackgroundViewTestView&) = delete;
  TrayBackgroundViewTestView& operator=(const TrayBackgroundViewTestView&) =
      delete;

  ~TrayBackgroundViewTestView() override = default;

  void ClickedOutsideBubble() override {}
  std::u16string GetAccessibleNameForTray() override {
    return u"TrayBackgroundViewTestView";
  }
  void HandleLocaleChange() override {}
  void HideBubbleWithView(const TrayBubbleView* bubble_view) override {}
};

class TrayBackgroundViewTest : public AshTestBase {
 public:
  TrayBackgroundViewTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  TrayBackgroundViewTest(const TrayBackgroundViewTest&) = delete;
  TrayBackgroundViewTest& operator=(const TrayBackgroundViewTest&) = delete;

  ~TrayBackgroundViewTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();

    test_view = std::make_unique<TrayBackgroundViewTestView>(GetPrimaryShelf());

    // Adds this `test_view` to the mock `StatusAreaWidget`. We need to remove
    // the layout manager from the delegate before adding a new child, since
    // there's an DCHECK in the `GridLayout` to assert no more child can be
    // added.
    StatusAreaWidgetTestHelper::GetStatusAreaWidget()
        ->status_area_widget_delegate()
        ->SetLayoutManager(nullptr);
    StatusAreaWidgetTestHelper::GetStatusAreaWidget()->AddTrayButton(
        test_view.get());

    // Set Dictation button to be visible.
    AccessibilityControllerImpl* controller =
        Shell::Get()->accessibility_controller();
    controller->dictation().SetDialogAccepted();
    controller->dictation().SetEnabled(true);
  }

  void TearDown() override {
    test_view.reset();
    AshTestBase::TearDown();
  }

 protected:
  // Here we use dictation tray for testing secondary screen.
  DictationButtonTray* GetPrimaryDictationTray() {
    return StatusAreaWidgetTestHelper::GetStatusAreaWidget()
        ->dictation_button_tray();
  }

  DictationButtonTray* GetSecondaryDictationTray() {
    return StatusAreaWidgetTestHelper::GetSecondaryStatusAreaWidget()
        ->dictation_button_tray();
  }

  std::unique_ptr<TrayBackgroundViewTestView> test_view;
};

TEST_F(TrayBackgroundViewTest, ShowingAnimationAbortedByHideAnimation) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);

  // Starts showing up animation.
  test_view->SetVisiblePreferred(true);
  EXPECT_TRUE(test_view->GetVisible());
  EXPECT_TRUE(test_view->layer()->GetTargetVisibility());
  EXPECT_TRUE(test_view->layer()->GetAnimator()->is_animating());

  // Starts hide animation. The view is visible but the layer's target
  // visibility is false.
  test_view->SetVisiblePreferred(false);
  EXPECT_TRUE(test_view->GetVisible());
  EXPECT_FALSE(test_view->layer()->GetTargetVisibility());
  EXPECT_TRUE(test_view->layer()->GetAnimator()->is_animating());

  // Here we wait until the animation is finished and we give it one more second
  // to finish the callbacks in `OnVisibilityAnimationFinished()`.
  StatusAreaWidgetTestHelper::WaitForLayerAnimationEnd(test_view->layer());
  task_environment()->FastForwardBy(base::TimeDelta::FromSeconds(1));

  // After the hide animation is finished, `test_view` is not visible.
  EXPECT_FALSE(test_view->GetVisible());
  EXPECT_FALSE(test_view->layer()->GetAnimator()->is_animating());
}

TEST_F(TrayBackgroundViewTest, HandleSessionChange) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);

  // Not showing animation after logging in.
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOGIN_PRIMARY);
  // Gives it a small duration to let the session get changed. This duration is
  // way smaller than the animation duration, so that the animation will not
  // finish when this duration ends. The same for the other places below.
  task_environment()->FastForwardBy(base::TimeDelta::FromMilliseconds(20));

  test_view->SetVisiblePreferred(false);
  test_view->SetVisiblePreferred(true);
  task_environment()->FastForwardBy(base::TimeDelta::FromMilliseconds(20));
  EXPECT_TRUE(test_view->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(test_view->GetVisible());

  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::ACTIVE);
  task_environment()->FastForwardBy(base::TimeDelta::FromMilliseconds(20));
  EXPECT_FALSE(test_view->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(test_view->GetVisible());

  // Enable the animation after session state get changed.
  task_environment()->FastForwardBy(base::TimeDelta::FromMilliseconds(20));
  test_view->SetVisiblePreferred(false);
  test_view->SetVisiblePreferred(true);
  task_environment()->FastForwardBy(base::TimeDelta::FromMilliseconds(20));
  EXPECT_TRUE(test_view->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(test_view->GetVisible());

  // Not showing animation after unlocking screen.
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOCKED);
  task_environment()->FastForwardBy(base::TimeDelta::FromMilliseconds(20));

  test_view->SetVisiblePreferred(false);
  test_view->SetVisiblePreferred(true);
  task_environment()->FastForwardBy(base::TimeDelta::FromMilliseconds(20));
  EXPECT_TRUE(test_view->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(test_view->GetVisible());

  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::ACTIVE);
  task_environment()->FastForwardBy(base::TimeDelta::FromMilliseconds(20));
  EXPECT_FALSE(test_view->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(test_view->GetVisible());

  // Not showing animation when switching users.
  GetSessionControllerClient()->AddUserSession("a");
  test_view->SetVisiblePreferred(false);
  test_view->SetVisiblePreferred(true);
  EXPECT_TRUE(test_view->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(test_view->GetVisible());

  // Simulates user switching by changing the order of session_ids.
  Shell::Get()->session_controller()->SetUserSessionOrder({2u, 1u});
  task_environment()->FastForwardBy(base::TimeDelta::FromMilliseconds(20));
  EXPECT_FALSE(test_view->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(test_view->GetVisible());
}

TEST_F(TrayBackgroundViewTest, SecondaryDisplay) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);

  // Add secondary screen.
  UpdateDisplay("800x600,800x600");

  // Switch the primary and secondary screen.
  SwapPrimaryDisplay();
  task_environment()->FastForwardBy(base::TimeDelta::FromMilliseconds(20));
  EXPECT_FALSE(
      GetPrimaryDictationTray()->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(GetPrimaryDictationTray()->GetVisible());
  EXPECT_FALSE(
      GetSecondaryDictationTray()->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(GetSecondaryDictationTray()->GetVisible());

  // Enable the animation after showing up on the secondary screen.
  task_environment()->FastForwardBy(base::TimeDelta::FromMilliseconds(20));
  GetPrimaryDictationTray()->SetVisiblePreferred(false);
  GetPrimaryDictationTray()->SetVisiblePreferred(true);
  GetSecondaryDictationTray()->SetVisiblePreferred(false);
  GetSecondaryDictationTray()->SetVisiblePreferred(true);
  task_environment()->FastForwardBy(base::TimeDelta::FromMilliseconds(20));
  EXPECT_TRUE(
      GetPrimaryDictationTray()->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(GetPrimaryDictationTray()->GetVisible());
  EXPECT_TRUE(
      GetSecondaryDictationTray()->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(GetSecondaryDictationTray()->GetVisible());
  task_environment()->FastForwardBy(base::TimeDelta::FromSeconds(3));

  // Remove the secondary screen.
  UpdateDisplay("800x600");

  task_environment()->FastForwardBy(base::TimeDelta::FromMilliseconds(20));
  EXPECT_FALSE(
      GetPrimaryDictationTray()->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(GetPrimaryDictationTray()->GetVisible());
}

}  // namespace ash