// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/back_button.h"

#include <memory>

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/accessibility/accessibility_controller.h"
#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/keyboard/keyboard_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_navigation_widget.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shelf/shelf_view_test_api.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "ash/wm/window_state.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/test_accelerator_target.h"
#include "ui/compositor/layer.h"
#include "ui/events/test/event_generator.h"

namespace ash {

namespace {

class BackButtonTest : public AshTestBase,
                       public testing::WithParamInterface<bool> {
 public:
  BackButtonTest() = default;

  BackButtonTest(const BackButtonTest&) = delete;
  BackButtonTest& operator=(const BackButtonTest&) = delete;

  ~BackButtonTest() override = default;

  BackButton* back_button() {
    return test_api_->shelf_view()
        ->shelf_widget()
        ->navigation_widget()
        ->GetBackButton();
  }

  ShelfViewTestAPI* test_api() { return test_api_.get(); }

  void SetUp() override {
    AshTestBase::SetUp();
    // Set a11y setting to show back button in tablet mode.
    Shell::Get()
        ->accessibility_controller()
        ->SetTabletModeShelfNavigationButtonsEnabled(true);

    test_api_ = std::make_unique<ShelfViewTestAPI>(
        GetPrimaryShelf()->GetShelfViewForTesting());

    // Finish all setup tasks. In particular we want to finish the
    // GetSwitchStates post task in (Fake)PowerManagerClient which is triggered
    // by TabletModeController otherwise this will cause tablet mode to exit
    // while we wait for animations in the test.
    base::RunLoop().RunUntilIdle();
  }

  bool IsBackButtonVisible() const {
    return ShelfNavigationWidget::TestApi(
               test_api_->shelf_view()->shelf_widget()->navigation_widget())
        .IsBackButtonVisible();
  }

 protected:
  std::unique_ptr<ShelfViewTestAPI> test_api_;
};

enum class TestAccessibilityFeature {
  kTabletModeShelfNavigationButtons,
  kSpokenFeedback,
  kAutoclick,
  kSwitchAccess
};

// Tests back button visibility with number of accessibility setting enabled,
// with kHideControlsInTabletModeFeature.
class BackButtonVisibilityWithAccessibilityFeaturesTest
    : public AshTestBase,
      public ::testing::WithParamInterface<TestAccessibilityFeature> {
 public:
  BackButtonVisibilityWithAccessibilityFeaturesTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kHideShelfControlsInTabletMode);
  }
  ~BackButtonVisibilityWithAccessibilityFeaturesTest() override = default;

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
        Shell::Get()
            ->accessibility_controller()
            ->DisableSwitchAccessDisableConfirmationDialogTesting();
        break;
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace

// Verify that the back button is visible in tablet mode.
TEST_F(BackButtonTest, Visibility) {
  EXPECT_FALSE(back_button());
  EXPECT_FALSE(IsBackButtonVisible());

  ash::TabletModeControllerTestApi().EnterTabletMode();
  test_api()->RunMessageLoopUntilAnimationsDone();

  // The back button should only be visible for in-app shelf in tablet mode.
  EXPECT_FALSE(IsBackButtonVisible());
  ASSERT_TRUE(!back_button());

  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  test_api()->RunMessageLoopUntilAnimationsDone();

  EXPECT_TRUE(IsBackButtonVisible());
  EXPECT_EQ(1.f, back_button()->layer()->opacity());

  ash::TabletModeControllerTestApi().LeaveTabletMode();
  test_api()->RunMessageLoopUntilAnimationsDone();

  EXPECT_FALSE(IsBackButtonVisible());
}

// Verify that the back button is visible in tablet mode, if the initial shelf
// alignment is on the left or right.
TEST_F(BackButtonTest, VisibilityWithVerticalShelf) {
  test_api()->shelf_view()->shelf()->SetAlignment(ShelfAlignment::kLeft);
  EXPECT_FALSE(back_button());
  EXPECT_FALSE(IsBackButtonVisible());

  ash::TabletModeControllerTestApi().EnterTabletMode();
  // Create a test widget to transition to in-app shelf.
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);

  test_api()->RunMessageLoopUntilAnimationsDone();
  EXPECT_TRUE(back_button());
  EXPECT_TRUE(IsBackButtonVisible());

  ash::TabletModeControllerTestApi().LeaveTabletMode();
  test_api()->RunMessageLoopUntilAnimationsDone();

  EXPECT_FALSE(back_button());
  EXPECT_FALSE(IsBackButtonVisible());
}

TEST_F(BackButtonTest, BackKeySequenceGenerated) {
  // Enter tablet mode; the back button is not visible in non tablet mode.
  ash::TabletModeControllerTestApi().EnterTabletMode();
  // Create a test widget to transition to in-app shelf.
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);

  ShelfNavigationWidget::TestApi navigation_widget_test_api(
      GetPrimaryShelf()->navigation_widget());
  // Wait for the navigation widget's animation.
  test_api()->RunMessageLoopUntilAnimationsDone(
      navigation_widget_test_api.GetBoundsAnimator());

  AcceleratorControllerImpl* controller =
      Shell::Get()->accelerator_controller();

  // Register an accelerator that looks for back presses. Note there is already
  // an accelerator on AppListView, which will handle the accelerator since it
  // is targeted before AcceleratorController (switching to tablet mode with no
  // other windows activates the app list). First remove that accelerator. In
  // release, there's only the AppList's accelerator, so it's always hit when
  // the app list is active. (ash/accelerators.cc has VKEY_BROWSER_BACK, but it
  // also needs Ctrl pressed).
  GetAppListTestHelper()->GetAppListView()->ResetAccelerators();

  ui::Accelerator accelerator_back_press(ui::VKEY_BROWSER_BACK, ui::EF_NONE);
  accelerator_back_press.set_key_state(ui::Accelerator::KeyState::PRESSED);
  ui::TestAcceleratorTarget target_back_press;
  controller->Register({accelerator_back_press}, &target_back_press);

  // Register an accelerator that looks for back releases.
  ui::Accelerator accelerator_back_release(ui::VKEY_BROWSER_BACK, ui::EF_NONE);
  accelerator_back_release.set_key_state(ui::Accelerator::KeyState::RELEASED);
  ui::TestAcceleratorTarget target_back_release;
  controller->Register({accelerator_back_release}, &target_back_release);

  // Verify that by pressing the back button no event is generated on the press,
  // but there is one generated on the release.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(back_button()->GetBoundsInScreen().CenterPoint());
  generator->PressLeftButton();
  EXPECT_EQ(0, target_back_press.accelerator_count());
  EXPECT_EQ(0, target_back_release.accelerator_count());

  generator->ReleaseLeftButton();
  EXPECT_EQ(1, target_back_press.accelerator_count());
  EXPECT_EQ(1, target_back_release.accelerator_count());
}

// Tests the back button behavior when an Android IME is visible. Due to the
// way the Android IME is implemented, a lot of this test is fake behavior, but
// it will help catch regressions.
TEST_F(BackButtonTest, BackButtonWithAndroidKeyboard) {
  Shell::Get()
      ->accessibility_controller()
      ->SetTabletModeShelfNavigationButtonsEnabled(false);

  // Enter tablet mode; the back button is not visible in non tablet mode.
  ash::TabletModeControllerTestApi().EnterTabletMode();

  AcceleratorControllerImpl* controller =
      Shell::Get()->accelerator_controller();

  // Register an accelerator that looks for back presses. Note there is already
  // an accelerator on AppListView, which will handle the accelerator since it
  // is targeted before AcceleratorController (switching to tablet mode with no
  // other windows activates the app list). First remove that accelerator. In
  // release, there's only the AppList's accelerator, so it's always hit when
  // the app list is active. (ash/accelerators.cc has VKEY_BROWSER_BACK, but it
  // also needs Ctrl pressed).
  GetAppListTestHelper()->GetAppListView()->ResetAccelerators();

  ui::Accelerator accelerator_back_press(ui::VKEY_BROWSER_BACK, ui::EF_NONE);
  accelerator_back_press.set_key_state(ui::Accelerator::KeyState::PRESSED);
  ui::TestAcceleratorTarget target_back_press;
  controller->Register({accelerator_back_press}, &target_back_press);

  // Register an accelerator that looks for back releases.
  ui::Accelerator accelerator_back_release(ui::VKEY_BROWSER_BACK, ui::EF_NONE);
  accelerator_back_release.set_key_state(ui::Accelerator::KeyState::RELEASED);
  ui::TestAcceleratorTarget target_back_release;
  controller->Register({accelerator_back_release}, &target_back_release);

  // Create a test widget to transition to in-app shelf.
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);

  // Fakes showing a virtual keyboard.
  VirtualKeyboardModel* keyboard =
      Shell::Get()->system_tray_model()->virtual_keyboard();
  ASSERT_TRUE(keyboard);
  keyboard->OnArcInputMethodBoundsChanged(gfx::Rect(400, 400));
  EXPECT_TRUE(keyboard->arc_keyboard_visible());

  EXPECT_TRUE(IsBackButtonVisible());
  ASSERT_TRUE(back_button());

  // Wait for the navigation widget's animation.
  ShelfNavigationWidget* navigation_widget =
      GetPrimaryShelf()->navigation_widget();
  ShelfNavigationWidget::TestApi navigation_widget_test_api(navigation_widget);
  test_api()->RunMessageLoopUntilAnimationsDone(
      navigation_widget_test_api.GetBoundsAnimator());

  ui::test::EventGenerator* generator = GetEventGenerator();
  // Click within the navigation widget where back button is expected to be.
  // Not using back button screen bounds directly because `GetBoundsInScreen()`
  // returns bounds outside navigation widget when called on the back button.
  // `GetBoundsInScreen()` converts the view bounds by applying layer transform
  // on the view's origin, and uses the transformed origin as the origin of
  // converted bounds. Assumption that the transformed origin point is the
  // origin point of transformed bounds does not hold after rotation (which is
  // the case for the back button).
  generator->MoveMouseTo(
      navigation_widget->GetWindowBoundsInScreen().origin() +
      back_button()->bounds().CenterPoint().OffsetFromOrigin());
  generator->ClickLeftButton();

  // Unfortunately we cannot hook this all the way up to see if the Android IME
  // is hidden, but we can check that back key events are generated.
  EXPECT_EQ(1, target_back_press.accelerator_count());
  EXPECT_EQ(1, target_back_release.accelerator_count());

  // Verify that the test widget has not been minimized.
  EXPECT_FALSE(WindowState::Get(widget->GetNativeWindow())->IsMinimized());
}

// Tests that the back button does not show a context menu.
TEST_F(BackButtonTest, NoContextMenuOnBackButton) {
  ui::test::EventGenerator* generator = GetEventGenerator();

  // Enable tablet mode to show the back button. Wait for tablet mode animations
  // to finish in order for the back button to move out from under the
  // home button.
  ash::TabletModeControllerTestApi().EnterTabletMode();

  // Create a test widget to transition to in-app shelf.
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);

  // Wait for the navigation widget's animation.
  ShelfNavigationWidget::TestApi navigation_widget_test_api(
      GetPrimaryShelf()->navigation_widget());
  test_api()->RunMessageLoopUntilAnimationsDone(
      navigation_widget_test_api.GetBoundsAnimator());

  generator->MoveMouseTo(back_button()->GetBoundsInScreen().CenterPoint());
  generator->PressRightButton();

  EXPECT_FALSE(test_api_->CloseMenu());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    BackButtonVisibilityWithAccessibilityFeaturesTest,
    ::testing::Values(
        TestAccessibilityFeature::kTabletModeShelfNavigationButtons,
        TestAccessibilityFeature::kSpokenFeedback,
        TestAccessibilityFeature::kAutoclick,
        TestAccessibilityFeature::kSwitchAccess));

TEST_P(BackButtonVisibilityWithAccessibilityFeaturesTest,
       TabletModeSwitchWithA11yFeatureEnabled) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);

  SetTestA11yFeatureEnabled(true /*enabled*/);

  ShelfNavigationWidget::TestApi test_api(
      GetPrimaryShelf()->navigation_widget());
  // Back button is not shown in clamshell.
  EXPECT_FALSE(test_api.IsBackButtonVisible());

  // Switch to tablet mode, and verify the back button is now visible.
  ash::TabletModeControllerTestApi().EnterTabletMode();
  EXPECT_TRUE(test_api.IsBackButtonVisible());

  // The button should be hidden if the feature gets disabled.
  SetTestA11yFeatureEnabled(false /*enabled*/);
  EXPECT_FALSE(test_api.IsBackButtonVisible());
}

TEST_P(BackButtonVisibilityWithAccessibilityFeaturesTest,
       FeatureEnabledWhileInTabletMode) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);

  ShelfNavigationWidget::TestApi test_api(
      GetPrimaryShelf()->navigation_widget());
  // Back button is not shown in clamshell.
  EXPECT_FALSE(test_api.IsBackButtonVisible());

  // Switch to tablet mode, and verify the back button is still hidden.
  ash::TabletModeControllerTestApi().EnterTabletMode();
  EXPECT_FALSE(test_api.IsBackButtonVisible());

  // The button should be shown if the feature gets enabled.
  SetTestA11yFeatureEnabled(true /*enabled*/);
  EXPECT_TRUE(test_api.IsBackButtonVisible());
}

}  // namespace ash
