// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/home_button.h"

#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/assistant/assistant_controller_impl.h"
#include "ash/assistant/model/assistant_ui_model.h"
#include "ash/assistant/test/test_assistant_service.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/assistant/controller/assistant_ui_controller.h"
#include "ash/public/cpp/tablet_mode.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_navigation_widget.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shelf/shelf_view_test_api.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/animation/bounds_animator.h"

namespace ash {

namespace {

ui::GestureEvent CreateGestureEvent(ui::GestureEventDetails details) {
  return ui::GestureEvent(0, 0, ui::EF_NONE, base::TimeTicks(), details);
}

class HomeButtonTest
    : public AshTestBase,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  HomeButtonTest() = default;
  ~HomeButtonTest() override = default;

  // AshTestBase:
  void SetUp() override {
    std::vector<base::Feature> enabled_features;
    std::vector<base::Feature> disabled_features;

    if (IsHotseatEnabled())
      enabled_features.push_back(chromeos::features::kShelfHotseat);
    else
      disabled_features.push_back(chromeos::features::kShelfHotseat);

    if (IsHideShelfControlsInTabletModeEnabled())
      enabled_features.push_back(features::kHideShelfControlsInTabletMode);
    else
      disabled_features.push_back(features::kHideShelfControlsInTabletMode);

    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);

    AshTestBase::SetUp();
  }

  void SendGestureEvent(ui::GestureEvent* event) {
    HomeButton* const home_button =
        GetPrimaryShelf()->navigation_widget()->GetHomeButton();
    ASSERT_TRUE(home_button);
    home_button->OnGestureEvent(event);
  }

  void SendGestureEventToSecondaryDisplay(ui::GestureEvent* event) {
    // Add secondary display.
    UpdateDisplay("1+1-1000x600,1002+0-600x400");
    ASSERT_TRUE(GetPrimaryShelf()
                    ->shelf_widget()
                    ->navigation_widget()
                    ->GetHomeButton());
    // Send the gesture event to the secondary display.
    Shelf::ForWindow(Shell::GetAllRootWindows()[1])
        ->shelf_widget()
        ->navigation_widget()
        ->GetHomeButton()
        ->OnGestureEvent(event);
  }

  bool IsHotseatEnabled() const { return std::get<0>(GetParam()); }

  bool IsHideShelfControlsInTabletModeEnabled() const {
    return std::get<1>(GetParam());
  }

  const HomeButton* home_button() const {
    return GetPrimaryShelf()
        ->shelf_widget()
        ->navigation_widget()
        ->GetHomeButton();
  }

  AssistantState* assistant_state() const { return AssistantState::Get(); }

  PrefService* prefs() {
    return Shell::Get()->session_controller()->GetPrimaryUserPrefService();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(HomeButtonTest);
};

// Tests home button visibility animations.
class HomeButtonAnimationTest : public AshTestBase {
 public:
  HomeButtonAnimationTest() {
    scoped_feature_list_.InitWithFeatures(
        {chromeos::features::kShelfHotseat,
         features::kHideShelfControlsInTabletMode},
        {});
  }
  ~HomeButtonAnimationTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    animation_duration_.emplace(
        ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  }

  void TearDown() override {
    animation_duration_.reset();
    AshTestBase::TearDown();
  }

 private:
  base::Optional<ui::ScopedAnimationDurationScaleMode> animation_duration_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

enum class TestAccessibilityFeature {
  kTabletModeShelfNavigationButtons,
  kSpokenFeedback,
  kAutoclick,
  kSwitchAccess
};

// Tests home button visibility with number of accessibility setting enabled,
// with kHideControlsInTabletModeFeature.
class HomeButtonVisibilityWithAccessibilityFeaturesTest
    : public AshTestBase,
      public ::testing::WithParamInterface<TestAccessibilityFeature> {
 public:
  HomeButtonVisibilityWithAccessibilityFeaturesTest() {
    scoped_feature_list_.InitWithFeatures(
        {chromeos::features::kShelfHotseat,
         features::kHideShelfControlsInTabletMode},
        {});
  }
  ~HomeButtonVisibilityWithAccessibilityFeaturesTest() override = default;

  void SetTestA11yFeatureEnabled(bool enabled) {
    switch (GetParam()) {
      case TestAccessibilityFeature::kTabletModeShelfNavigationButtons:
        Shell::Get()
            ->accessibility_controller()
            ->SetTabletModeShelfNavigationButtonsEnabled(enabled);
        break;
      case TestAccessibilityFeature::kSpokenFeedback:
        Shell::Get()->accessibility_controller()->SetSpokenFeedbackEnabled(
            enabled, A11Y_NOTIFICATION_NONE);
        break;
      case TestAccessibilityFeature::kAutoclick:
        Shell::Get()->accessibility_controller()->autoclick().SetEnabled(
            enabled);
        break;
      case TestAccessibilityFeature::kSwitchAccess:
        Shell::Get()->accessibility_controller()->switch_access().SetEnabled(
            enabled);
        break;
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace

// The parameters indicate whether the kShelfHotseat and
// kHideShelfControlsInTabletMode features are enabled.
INSTANTIATE_TEST_SUITE_P(All,
                         HomeButtonTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

TEST_P(HomeButtonTest, SwipeUpToOpenFullscreenAppList) {
  Shelf* shelf = GetPrimaryShelf();
  EXPECT_EQ(ShelfAlignment::kBottom, shelf->alignment());

  // Start the drags from the center of the shelf.
  const ShelfView* shelf_view = shelf->GetShelfViewForTesting();
  gfx::Point start =
      gfx::Point(shelf_view->width() / 2, shelf_view->height() / 2);
  views::View::ConvertPointToScreen(shelf_view, &start);
  // Swiping up less than the threshold should trigger a peeking app list.
  gfx::Point end = start;
  end.set_y(shelf->GetIdealBounds().bottom() -
            AppListView::kDragSnapToPeekingThreshold + 10);
  GetEventGenerator()->GestureScrollSequence(
      start, end, base::TimeDelta::FromMilliseconds(100), 4 /* steps */);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(true);
  GetAppListTestHelper()->CheckState(AppListViewState::kPeeking);

  // Closing the app list.
  GetAppListTestHelper()->DismissAndRunLoop();
  GetAppListTestHelper()->CheckVisibility(false);
  GetAppListTestHelper()->CheckState(AppListViewState::kClosed);

  // Swiping above the threshold should trigger a fullscreen app list.
  end.set_y(shelf->GetIdealBounds().bottom() -
            AppListView::kDragSnapToPeekingThreshold - 10);
  GetEventGenerator()->GestureScrollSequence(
      start, end, base::TimeDelta::FromMilliseconds(100), 4 /* steps */);
  base::RunLoop().RunUntilIdle();
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(true);
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);
}

TEST_P(HomeButtonTest, ClickToOpenAppList) {
  Shelf* shelf = GetPrimaryShelf();
  EXPECT_EQ(ShelfAlignment::kBottom, shelf->alignment());

  ShelfNavigationWidget::TestApi test_api(
      GetPrimaryShelf()->navigation_widget());
  ASSERT_TRUE(test_api.IsHomeButtonVisible());
  ASSERT_TRUE(home_button());

  gfx::Point center = home_button()->GetBoundsInScreen().CenterPoint();
  GetEventGenerator()->MoveMouseTo(center);

  // Click on the home button should toggle the app list.
  GetEventGenerator()->ClickLeftButton();
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(true);
  GetAppListTestHelper()->CheckState(AppListViewState::kPeeking);
  GetEventGenerator()->ClickLeftButton();
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(false);
  GetAppListTestHelper()->CheckState(AppListViewState::kClosed);

  // Shift-click should open the app list in fullscreen.
  GetEventGenerator()->set_flags(ui::EF_SHIFT_DOWN);
  GetEventGenerator()->ClickLeftButton();
  GetEventGenerator()->set_flags(0);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(true);
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);

  // Another shift-click should close the app list.
  GetEventGenerator()->set_flags(ui::EF_SHIFT_DOWN);
  GetEventGenerator()->ClickLeftButton();
  GetEventGenerator()->set_flags(0);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(false);
  GetAppListTestHelper()->CheckState(AppListViewState::kClosed);
}

TEST_P(HomeButtonTest, ClickToOpenAppListInTabletMode) {
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);

  Shelf* shelf = GetPrimaryShelf();
  EXPECT_EQ(ShelfAlignment::kBottom, shelf->alignment());

  ShelfNavigationWidget::TestApi test_api(shelf->navigation_widget());

  // Home button is expected to be hidden in tablet mode if shelf controls
  // should be hidden - this feature is available only with hotseat enabled.
  const bool should_show_home_button =
      !(IsHotseatEnabled() && IsHideShelfControlsInTabletModeEnabled());
  EXPECT_EQ(should_show_home_button, test_api.IsHomeButtonVisible());
  ASSERT_EQ(should_show_home_button, static_cast<bool>(home_button()));
  if (!should_show_home_button)
    return;

  // App list should be shown by default in tablet mode.
  GetAppListTestHelper()->CheckVisibility(true);
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);

  // Click on the home button should not close the app list.
  gfx::Point center = home_button()->GetBoundsInScreen().CenterPoint();
  GetEventGenerator()->MoveMouseTo(center);
  GetEventGenerator()->ClickLeftButton();
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(true);
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);

  // Shift-click should not close the app list.
  GetEventGenerator()->set_flags(ui::EF_SHIFT_DOWN);
  GetEventGenerator()->ClickLeftButton();
  GetEventGenerator()->set_flags(0);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(true);
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);
}

TEST_P(HomeButtonTest, ButtonPositionInTabletMode) {
  // Finish all setup tasks. In particular we want to finish the
  // GetSwitchStates post task in (Fake)PowerManagerClient which is triggered
  // by TabletModeController otherwise this will cause tablet mode to exit
  // while we wait for animations in the test.
  base::RunLoop().RunUntilIdle();

  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);

  Shelf* const shelf = GetPrimaryShelf();
  ShelfViewTestAPI shelf_test_api(shelf->GetShelfViewForTesting());
  ShelfNavigationWidget::TestApi test_api(shelf->navigation_widget());

  // Home button is expected to be hidden in tablet mode if shelf controls
  // should be hidden - this feature is available only with hotseat enabled.
  const bool should_show_home_button =
      !(IsHotseatEnabled() && IsHideShelfControlsInTabletModeEnabled());
  EXPECT_EQ(should_show_home_button, test_api.IsHomeButtonVisible());
  EXPECT_EQ(should_show_home_button, static_cast<bool>(home_button()));

  // When hotseat is enabled, home button position changes between in-app shelf
  // and home shelf, so test in-app when hotseat is enabled.
  if (IsHotseatEnabled()) {
    // Wait for the navigation widget's animation.
    shelf_test_api.RunMessageLoopUntilAnimationsDone(
        test_api.GetBoundsAnimator());

    EXPECT_EQ(should_show_home_button, test_api.IsHomeButtonVisible());
    ASSERT_EQ(should_show_home_button, static_cast<bool>(home_button()));

    if (should_show_home_button) {
      EXPECT_EQ(home_button()->bounds().x(),
                ShelfConfig::Get()->control_button_edge_spacing(
                    true /* is_primary_axis_edge */));
    }

    // Switch to in-app shelf.
    std::unique_ptr<views::Widget> widget = CreateTestWidget();
  }

  // Wait for the navigation widget's animation.
  shelf_test_api.RunMessageLoopUntilAnimationsDone(
      test_api.GetBoundsAnimator());

  EXPECT_EQ(should_show_home_button, test_api.IsHomeButtonVisible());
  EXPECT_EQ(should_show_home_button, static_cast<bool>(home_button()));

  if (should_show_home_button)
    EXPECT_GT(home_button()->bounds().x(), 0);

  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);
  shelf_test_api.RunMessageLoopUntilAnimationsDone(
      test_api.GetBoundsAnimator());

  EXPECT_TRUE(test_api.IsHomeButtonVisible());
  ASSERT_TRUE(home_button());

  // The space between button and screen edge is within the widget.
  EXPECT_EQ(ShelfConfig::Get()->control_button_edge_spacing(
                true /* is_primary_axis_edge */),
            home_button()->bounds().x());
}

// Verifies that home button visibility updates are animated.
TEST_F(HomeButtonAnimationTest, VisibilityAnimation) {
  views::View* const home_button_view =
      GetPrimaryShelf()->shelf_widget()->navigation_widget()->GetHomeButton();
  ASSERT_TRUE(home_button_view);
  EXPECT_TRUE(home_button_view->GetVisible());
  EXPECT_EQ(1.0f, home_button_view->layer()->opacity());
  EXPECT_EQ(1.0f, home_button_view->layer()->GetTargetOpacity());

  // Switch to tablet mode changes the button visibility.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);

  // Verify that the button view is still visible, and animating to 0 opacity.
  EXPECT_TRUE(home_button_view->GetVisible());
  EXPECT_EQ(1.0f, home_button_view->layer()->opacity());
  EXPECT_EQ(0.0f, home_button_view->layer()->GetTargetOpacity());

  // Once the opacity animation finishes, the button should not be visible.
  home_button_view->layer()->GetAnimator()->StopAnimating();
  EXPECT_FALSE(home_button_view->GetVisible());

  // Tablet mode exit should schedule animation to the visible state.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);
  EXPECT_TRUE(home_button_view->GetVisible());
  EXPECT_EQ(0.0f, home_button_view->layer()->opacity());
  EXPECT_EQ(1.0f, home_button_view->layer()->GetTargetOpacity());

  home_button_view->layer()->GetAnimator()->StopAnimating();
  EXPECT_TRUE(home_button_view->GetVisible());
  EXPECT_EQ(1.0f, home_button_view->layer()->opacity());
  EXPECT_EQ(1.0f, home_button_view->layer()->GetTargetOpacity());
}

// Verifies that home button visibility updates if the button gets hidden while
// it's still being shown.
TEST_F(HomeButtonAnimationTest, HideWhileAnimatingToShow) {
  views::View* const home_button_view =
      GetPrimaryShelf()->shelf_widget()->navigation_widget()->GetHomeButton();
  ASSERT_TRUE(home_button_view);

  EXPECT_TRUE(home_button_view->GetVisible());
  EXPECT_EQ(1.0f, home_button_view->layer()->opacity());
  EXPECT_EQ(1.0f, home_button_view->layer()->GetTargetOpacity());

  // Switch to tablet mode to initiate home button hide animation.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  EXPECT_TRUE(home_button_view->GetVisible());
  EXPECT_EQ(1.0f, home_button_view->layer()->opacity());
  EXPECT_EQ(0.0f, home_button_view->layer()->GetTargetOpacity());
  home_button_view->layer()->GetAnimator()->StopAnimating();

  // Tablet mode exit should schedule an animation to the visible state.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);
  EXPECT_TRUE(home_button_view->GetVisible());
  EXPECT_EQ(0.0f, home_button_view->layer()->opacity());
  EXPECT_EQ(1.0f, home_button_view->layer()->GetTargetOpacity());

  // Enter tablet mode immediately, to interrupt the show animation.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  EXPECT_TRUE(home_button_view->GetVisible());
  EXPECT_EQ(0.0f, home_button_view->layer()->opacity());
  EXPECT_EQ(0.0f, home_button_view->layer()->GetTargetOpacity());

  home_button_view->layer()->GetAnimator()->StopAnimating();
  EXPECT_FALSE(home_button_view->GetVisible());
}

// Verifies that home button becomes visible if reshown while a hide animation
// is still in progress.
TEST_F(HomeButtonAnimationTest, ShowWhileAnimatingToHide) {
  views::View* const home_button_view =
      GetPrimaryShelf()->shelf_widget()->navigation_widget()->GetHomeButton();
  ASSERT_TRUE(home_button_view);

  EXPECT_TRUE(home_button_view->GetVisible());
  EXPECT_EQ(1.0f, home_button_view->layer()->opacity());
  EXPECT_EQ(1.0f, home_button_view->layer()->GetTargetOpacity());

  // Switch to tablet mode to initiate the home button hide animation.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);

  EXPECT_TRUE(home_button_view->GetVisible());
  EXPECT_EQ(1.0f, home_button_view->layer()->opacity());
  EXPECT_EQ(0.0f, home_button_view->layer()->GetTargetOpacity());

  // Tablet mode exit should schedule an animation to the visible state.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);
  EXPECT_TRUE(home_button_view->GetVisible());
  EXPECT_EQ(1.0f, home_button_view->layer()->opacity());
  EXPECT_EQ(1.0f, home_button_view->layer()->GetTargetOpacity());

  // Verify that the button ends up in the visible state.
  home_button_view->layer()->GetAnimator()->StopAnimating();
  EXPECT_TRUE(home_button_view->GetVisible());
  EXPECT_EQ(1.0f, home_button_view->layer()->opacity());
  EXPECT_EQ(1.0f, home_button_view->layer()->GetTargetOpacity());
}

// Verifies that unanimated navigation widget layout update interrupts in
// progress button animation.
TEST_F(HomeButtonAnimationTest, NonAnimatedLayoutDuringAnimation) {
  Shelf* const shelf = GetPrimaryShelf();
  views::View* const home_button_view =
      shelf->shelf_widget()->navigation_widget()->GetHomeButton();
  ASSERT_TRUE(home_button_view);
  EXPECT_TRUE(home_button_view->GetVisible());
  EXPECT_EQ(1.0f, home_button_view->layer()->opacity());
  EXPECT_EQ(1.0f, home_button_view->layer()->GetTargetOpacity());

  // Switch to tablet mode changes the button visibility.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);

  ShelfViewTestAPI shelf_test_api(shelf->GetShelfViewForTesting());
  ShelfNavigationWidget::TestApi test_api(shelf->navigation_widget());

  // Verify the button bounds are animating.
  EXPECT_TRUE(test_api.GetBoundsAnimator()->IsAnimating(home_button_view));

  // Verify that the button visibility is animating.
  EXPECT_TRUE(home_button_view->GetVisible());
  EXPECT_EQ(1.0f, home_button_view->layer()->opacity());
  EXPECT_EQ(0.0f, home_button_view->layer()->GetTargetOpacity());

  // Request non-animated navigation widget layout, and verify the button is not
  // animating any longer.
  shelf->navigation_widget()->UpdateLayout(/*animate=*/false);

  EXPECT_FALSE(home_button_view->GetVisible());
  EXPECT_FALSE(home_button_view->layer()->GetAnimator()->is_animating());
  EXPECT_FALSE(test_api.GetBoundsAnimator()->IsAnimating(home_button_view));

  // Tablet mode exit should schedule animation to the visible state.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);

  EXPECT_TRUE(test_api.GetBoundsAnimator()->IsAnimating(home_button_view));
  EXPECT_TRUE(home_button_view->GetVisible());
  EXPECT_EQ(0.0f, home_button_view->layer()->opacity());
  EXPECT_EQ(1.0f, home_button_view->layer()->GetTargetOpacity());

  // Request non-animated navigation widget layout, and verify the button is not
  // animating any longer.
  shelf->navigation_widget()->UpdateLayout(/*animate=*/false);

  EXPECT_FALSE(test_api.GetBoundsAnimator()->IsAnimating(home_button_view));
  EXPECT_TRUE(home_button_view->GetVisible());
  EXPECT_FALSE(home_button_view->layer()->GetAnimator()->is_animating());
  EXPECT_EQ(1.0f, home_button_view->layer()->opacity());
}

TEST_P(HomeButtonTest, LongPressGesture) {
  // Simulate two users with primary user as active.
  CreateUserSessions(2);

  // Enable the Assistant in system settings.
  prefs()->SetBoolean(chromeos::assistant::prefs::kAssistantEnabled, true);
  assistant_state()->NotifyFeatureAllowed(
      chromeos::assistant::AssistantAllowedState::ALLOWED);
  assistant_state()->NotifyStatusChanged(
      chromeos::assistant::AssistantStatus::READY);

  ShelfNavigationWidget::TestApi test_api(
      GetPrimaryShelf()->navigation_widget());
  EXPECT_TRUE(test_api.IsHomeButtonVisible());
  ASSERT_TRUE(home_button());

  ui::GestureEvent long_press =
      CreateGestureEvent(ui::GestureEventDetails(ui::ET_GESTURE_LONG_PRESS));
  SendGestureEvent(&long_press);
  GetAppListTestHelper()->WaitUntilIdle();
  EXPECT_EQ(AssistantVisibility::kVisible,
            AssistantUiController::Get()->GetModel()->visibility());

  AssistantUiController::Get()->CloseUi(
      chromeos::assistant::AssistantExitPoint::kUnspecified);
  // Test long press gesture on secondary display.
  SendGestureEventToSecondaryDisplay(&long_press);
  GetAppListTestHelper()->WaitUntilIdle();
  EXPECT_EQ(AssistantVisibility::kVisible,
            AssistantUiController::Get()->GetModel()->visibility());
}

TEST_P(HomeButtonTest, LongPressGestureInTabletMode) {
  // Simulate two users with primary user as active.
  CreateUserSessions(2);

  // Enable the Assistant in system settings.
  prefs()->SetBoolean(chromeos::assistant::prefs::kAssistantEnabled, true);
  assistant_state()->NotifyFeatureAllowed(
      chromeos::assistant::AssistantAllowedState::ALLOWED);
  assistant_state()->NotifyStatusChanged(
      chromeos::assistant::AssistantStatus::READY);

  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);

  ShelfNavigationWidget::TestApi test_api(
      GetPrimaryShelf()->navigation_widget());
  const bool should_show_home_button =
      !(IsHotseatEnabled() && IsHideShelfControlsInTabletModeEnabled());
  EXPECT_EQ(should_show_home_button, test_api.IsHomeButtonVisible());
  ASSERT_EQ(should_show_home_button, static_cast<bool>(home_button()));

  // App list should be shown by default in tablet mode.
  GetAppListTestHelper()->CheckVisibility(true);
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);

  if (!should_show_home_button)
    return;

  ui::GestureEvent long_press =
      CreateGestureEvent(ui::GestureEventDetails(ui::ET_GESTURE_LONG_PRESS));
  SendGestureEvent(&long_press);
  GetAppListTestHelper()->WaitUntilIdle();
  EXPECT_EQ(AssistantVisibility::kVisible,
            AssistantUiController::Get()->GetModel()->visibility());
  GetAppListTestHelper()->CheckVisibility(true);
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);

  // Tap on the home button should close assistant.
  gfx::Point center = home_button()->GetBoundsInScreen().CenterPoint();
  GetEventGenerator()->MoveMouseTo(center);
  GetEventGenerator()->ClickLeftButton();

  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(true);
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);
  EXPECT_EQ(AssistantVisibility::kClosed,
            AssistantUiController::Get()->GetModel()->visibility());

  AssistantUiController::Get()->CloseUi(
      chromeos::assistant::AssistantExitPoint::kUnspecified);
}

TEST_P(HomeButtonTest, LongPressGestureWithSecondaryUser) {
  // Disallowed by secondary user.
  assistant_state()->NotifyFeatureAllowed(
      chromeos::assistant::AssistantAllowedState::
          DISALLOWED_BY_NONPRIMARY_USER);

  // Enable the Assistant in system settings.
  prefs()->SetBoolean(chromeos::assistant::prefs::kAssistantEnabled, true);

  ShelfNavigationWidget::TestApi test_api(
      GetPrimaryShelf()->navigation_widget());
  EXPECT_TRUE(test_api.IsHomeButtonVisible());
  ASSERT_TRUE(home_button());

  ui::GestureEvent long_press =
      CreateGestureEvent(ui::GestureEventDetails(ui::ET_GESTURE_LONG_PRESS));
  SendGestureEvent(&long_press);
  // The Assistant is disabled for secondary user.
  EXPECT_NE(AssistantVisibility::kVisible,
            AssistantUiController::Get()->GetModel()->visibility());

  // Test long press gesture on secondary display.
  SendGestureEventToSecondaryDisplay(&long_press);
  EXPECT_NE(AssistantVisibility::kVisible,
            AssistantUiController::Get()->GetModel()->visibility());
}

TEST_P(HomeButtonTest, LongPressGestureWithSettingsDisabled) {
  // Simulate two user with primary user as active.
  CreateUserSessions(2);

  // Simulate a user who has already completed setup flow, but disabled the
  // Assistant in settings.
  prefs()->SetBoolean(chromeos::assistant::prefs::kAssistantEnabled, false);
  assistant_state()->NotifyFeatureAllowed(
      chromeos::assistant::AssistantAllowedState::ALLOWED);

  ShelfNavigationWidget::TestApi test_api(
      GetPrimaryShelf()->navigation_widget());
  EXPECT_TRUE(test_api.IsHomeButtonVisible());
  ASSERT_TRUE(home_button());

  ui::GestureEvent long_press =
      CreateGestureEvent(ui::GestureEventDetails(ui::ET_GESTURE_LONG_PRESS));
  SendGestureEvent(&long_press);
  EXPECT_NE(AssistantVisibility::kVisible,
            AssistantUiController::Get()->GetModel()->visibility());

  // Test long press gesture on secondary display.
  SendGestureEventToSecondaryDisplay(&long_press);
  EXPECT_NE(AssistantVisibility::kVisible,
            AssistantUiController::Get()->GetModel()->visibility());
}

// Tests that tapping in the bottom left corner in tablet mode results in the
// home button activating.
TEST_P(HomeButtonTest, InteractOutsideHomeButtonBounds) {
  EXPECT_EQ(ShelfAlignment::kBottom, GetPrimaryShelf()->alignment());

  // Tap the bottom left of the shelf. The button should work.
  gfx::Point bottom_left = GetPrimaryShelf()
                               ->shelf_widget()
                               ->GetWindowBoundsInScreen()
                               .bottom_left();
  GetEventGenerator()->GestureTapAt(bottom_left);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(true);

  // Tap the top left of the shelf, the button should work.
  gfx::Point bottom_right = GetPrimaryShelf()
                                ->shelf_widget()
                                ->GetWindowBoundsInScreen()
                                .bottom_right();
  GetEventGenerator()->GestureTapAt(bottom_right);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(false);

  // Test left shelf.
  GetPrimaryShelf()->SetAlignment(ShelfAlignment::kLeft);
  gfx::Point top_left =
      GetPrimaryShelf()->shelf_widget()->GetWindowBoundsInScreen().origin();
  GetEventGenerator()->GestureTapAt(top_left);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(true);

  bottom_left = GetPrimaryShelf()
                    ->shelf_widget()
                    ->GetWindowBoundsInScreen()
                    .bottom_left();
  GetEventGenerator()->GestureTapAt(bottom_left);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(false);

  // Test right shelf.
  GetPrimaryShelf()->SetAlignment(ShelfAlignment::kRight);
  gfx::Point top_right =
      GetPrimaryShelf()->shelf_widget()->GetWindowBoundsInScreen().top_right();
  GetEventGenerator()->GestureTapAt(top_right);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(true);

  bottom_right = GetPrimaryShelf()
                     ->shelf_widget()
                     ->GetWindowBoundsInScreen()
                     .bottom_right();
  GetEventGenerator()->GestureTapAt(bottom_right);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(false);
}

// Tests that clicking the corner of the display opens and closes the AppList.
TEST_P(HomeButtonTest, ClickOnCornerPixel) {
  // Screen corners are extremely easy to reach with a mouse. Let's make sure
  // that a click on the bottom-left corner (or bottom-right corner in RTL)
  // can trigger the home button.
  gfx::Point corner(
      0, display::Screen::GetScreen()->GetPrimaryDisplay().bounds().height());

  ShelfNavigationWidget::TestApi test_api(
      GetPrimaryShelf()->navigation_widget());
  ASSERT_TRUE(test_api.IsHomeButtonVisible());

  GetAppListTestHelper()->CheckVisibility(false);
  GetEventGenerator()->MoveMouseTo(corner);
  GetEventGenerator()->ClickLeftButton();
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(true);

  GetEventGenerator()->ClickLeftButton();
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(false);
}

// Test that for a gesture tap which covers both the shelf navigation widget
// and the home button, the home button is returned as the event target. When
// the home button is the only button within the widget,
// ViewTageterDelegate::TargetForRect() can return the incorrect view. Ensuring
// the center point of the home button is the same as the content view's center
// point will avoid this problem. See http://crbug.com/1083713
TEST_P(HomeButtonTest, GestureHomeButtonHitTest) {
  ShelfNavigationWidget* nav_widget =
      AshTestBase::GetPrimaryShelf()->navigation_widget();
  ShelfNavigationWidget::TestApi test_api(nav_widget);
  gfx::Rect nav_widget_bounds = nav_widget->GetRootView()->bounds();

  // The home button should be the only shown button.
  EXPECT_TRUE(test_api.IsHomeButtonVisible());
  EXPECT_FALSE(test_api.IsBackButtonVisible());

  // The center point of the widget and the center point of the home button
  // should be equally close to the event location.
  gfx::Point home_button_center(
      nav_widget->GetHomeButton()->bounds().CenterPoint());
  gfx::Point nav_widget_center(nav_widget_bounds.CenterPoint());
  EXPECT_EQ(home_button_center, nav_widget_center);

  ui::GestureEventDetails details = ui::GestureEventDetails(ui::ET_GESTURE_TAP);

  // Create and test a gesture-event targeting >60% of the navigation widget,
  // as well as ~60% of the home button.
  gfx::RectF gesture_event_rect(0, 0, .7f * nav_widget_bounds.width(),
                                nav_widget_bounds.height());
  details.set_bounding_box(gesture_event_rect);
  {
    const gfx::Point event_center(gesture_event_rect.width() / 2,
                                  gesture_event_rect.height() / 2);

    ui::GestureEvent gesture(event_center.x(), event_center.y(), 0,
                             base::TimeTicks(), details);

    ui::EventTargeter* targeter = nav_widget->GetRootView()->GetEventTargeter();
    ui::EventTarget* target =
        targeter->FindTargetForEvent(nav_widget->GetRootView(), &gesture);
    EXPECT_TRUE(target);

    // Check that the event target is the home button.
    EXPECT_EQ(target, nav_widget->GetHomeButton());
  }

  // Test a gesture event centered on the top corner of the home button.
  {
    const gfx::Point event_center(nav_widget->GetHomeButton()->bounds().x(),
                                  nav_widget->GetHomeButton()->bounds().y());

    ui::GestureEvent gesture(event_center.x(), event_center.y(), 0,
                             base::TimeTicks(), details);

    ui::EventTargeter* targeter = nav_widget->GetRootView()->GetEventTargeter();
    ui::EventTarget* target =
        targeter->FindTargetForEvent(nav_widget->GetRootView(), &gesture);
    EXPECT_TRUE(target);

    // Check that the event target is the home button.
    EXPECT_EQ(target, nav_widget->GetHomeButton());
  }

  // Test a gesture event centered to the left of the nav_widget's center
  // point.
  {
    const gfx::Point event_center(nav_widget_center.x() - 1,
                                  nav_widget_center.y());

    ui::GestureEvent gesture(event_center.x(), event_center.y(), 0,
                             base::TimeTicks(), details);

    ui::EventTargeter* targeter = nav_widget->GetRootView()->GetEventTargeter();
    ui::EventTarget* target =
        targeter->FindTargetForEvent(nav_widget->GetRootView(), &gesture);
    EXPECT_TRUE(target);

    // Check that the event target is the home button.
    EXPECT_EQ(target, nav_widget->GetHomeButton());
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    HomeButtonVisibilityWithAccessibilityFeaturesTest,
    ::testing::Values(
        TestAccessibilityFeature::kTabletModeShelfNavigationButtons,
        TestAccessibilityFeature::kSpokenFeedback,
        TestAccessibilityFeature::kAutoclick,
        TestAccessibilityFeature::kSwitchAccess));

TEST_P(HomeButtonVisibilityWithAccessibilityFeaturesTest,
       TabletModeSwitchWithA11yFeatureEnabled) {
  SetTestA11yFeatureEnabled(true /*enabled*/);

  ShelfNavigationWidget::TestApi test_api(
      GetPrimaryShelf()->navigation_widget());
  EXPECT_TRUE(test_api.IsHomeButtonVisible());

  // Switch to tablet mode, and verify the home button is still visible.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  EXPECT_TRUE(test_api.IsHomeButtonVisible());

  // The button should be hidden if the feature gets disabled.
  SetTestA11yFeatureEnabled(false /*enabled*/);
  EXPECT_FALSE(test_api.IsHomeButtonVisible());
}

TEST_P(HomeButtonVisibilityWithAccessibilityFeaturesTest,
       FeatureEnabledWhileInTabletMode) {
  ShelfNavigationWidget::TestApi test_api(
      GetPrimaryShelf()->navigation_widget());
  EXPECT_TRUE(test_api.IsHomeButtonVisible());

  // Switch to tablet mode, and verify the home button is hidden.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  EXPECT_FALSE(test_api.IsHomeButtonVisible());

  // The button should be shown if the feature gets enabled.
  SetTestA11yFeatureEnabled(true /*enabled*/);
  EXPECT_TRUE(test_api.IsHomeButtonVisible());
}

}  // namespace ash
