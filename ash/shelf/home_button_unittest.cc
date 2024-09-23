// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/home_button.h"

#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/assistant/assistant_controller_impl.h"
#include "ash/assistant/model/assistant_ui_model.h"
#include "ash/assistant/test/test_assistant_service.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/assistant/controller/assistant_ui_controller.h"
#include "ash/public/cpp/tablet_mode.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_navigation_widget.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shelf/shelf_view_test_api.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_util.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/test/layer_animation_stopped_waiter.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/views/animation/bounds_animator.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

ui::GestureEvent CreateGestureEvent(ui::GestureEventDetails details) {
  return ui::GestureEvent(0, 0, ui::EF_NONE, base::TimeTicks(), details);
}

class HomeButtonTestBase : public AshTestBase {
 public:
  HomeButtonTestBase() = default;
  HomeButtonTestBase(const HomeButtonTestBase&) = delete;
  HomeButtonTestBase& operator=(const HomeButtonTestBase&) = delete;
  ~HomeButtonTestBase() override = default;

  void SendGestureEvent(ui::GestureEvent* event) {
    ASSERT_TRUE(home_button());
    home_button()->OnGestureEvent(event);
  }

  HomeButton* home_button() const {
    return GetPrimaryShelf()
        ->shelf_widget()
        ->navigation_widget()
        ->GetHomeButton();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class HomeButtonTest : public HomeButtonTestBase,
                       public testing::WithParamInterface<bool> {
 public:
  // HomeButtonTestBase:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatureState(
        features::kHideShelfControlsInTabletMode,
        IsHideShelfControlsInTabletModeEnabled());

    HomeButtonTestBase::SetUp();
  }

  void SendGestureEventToSecondaryDisplay(ui::GestureEvent* event) {
    // Add secondary display.
    UpdateDisplay("1+1-1000x600,1002+0-600x400");
    ASSERT_TRUE(Shelf::ForWindow(Shell::GetAllRootWindows()[1])
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

  bool IsHideShelfControlsInTabletModeEnabled() const { return GetParam(); }

  AssistantState* assistant_state() const { return AssistantState::Get(); }

  PrefService* prefs() {
    return Shell::Get()->session_controller()->GetPrimaryUserPrefService();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests home button visibility animations.
class HomeButtonAnimationTest : public HomeButtonTestBase {
 public:
  HomeButtonAnimationTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kHideShelfControlsInTabletMode);
  }
  ~HomeButtonAnimationTest() override = default;

  // HomeButtonTestBase:
  void SetUp() override {
    HomeButtonTestBase::SetUp();

    animation_duration_.emplace(
        ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  }

  void TearDown() override {
    animation_duration_.reset();
    HomeButtonTestBase::TearDown();
  }

 private:
  std::optional<ui::ScopedAnimationDurationScaleMode> animation_duration_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

class HomeButtonWithTextTest : public HomeButtonTestBase {
 public:
  HomeButtonWithTextTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kHomeButtonWithText);
  }
  ~HomeButtonWithTextTest() override = default;

  bool IsLabelVisible() const {
    if (!home_button())
      return false;
    auto* label_container = home_button()->expandable_container_for_test();
    return label_container->GetVisible() &&
           label_container->layer()->visible() &&
           home_button()->nudge_label_for_test();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class HomeButtonWithQuickAppAccess : public HomeButtonTestBase {
 public:
  HomeButtonWithQuickAppAccess() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kHomeButtonQuickAppAccess);
  }
  ~HomeButtonWithQuickAppAccess() override = default;

  bool IsQuickAppVisible() const {
    if (!home_button()) {
      return false;
    }
    auto* expandable_container = home_button()->expandable_container_for_test();
    if (!expandable_container) {
      return false;
    }

    return expandable_container->GetVisible() &&
           expandable_container->layer()->visible() &&
           home_button()->quick_app_button_for_test();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Test that setting an existing app item as the quick app shows a working
// clickable quick app button.
TEST_F(HomeButtonWithQuickAppAccess, Basic) {
  EXPECT_FALSE(IsQuickAppVisible());

  const std::string quick_app_id = "Quick App Item";
  GetAppListTestHelper()->model()->CreateAndAddItem(quick_app_id);
  EXPECT_TRUE(
      Shell::Get()->app_list_controller()->SetHomeButtonQuickApp(quick_app_id));

  EXPECT_TRUE(IsQuickAppVisible());

  GetPrimaryShelf()->shelf_layout_manager()->LayoutShelf();
  gfx::Point quick_app_center = home_button()
                                    ->quick_app_button_for_test()
                                    ->GetBoundsInScreen()
                                    .CenterPoint();

  // Test launching the quick app.
  GetEventGenerator()->MoveMouseTo(quick_app_center);
  GetEventGenerator()->ClickLeftButton();
  EXPECT_EQ(1, GetTestAppListClient()->activate_item_count());
  EXPECT_EQ(quick_app_id, GetTestAppListClient()->activate_item_last_id());

  // Quick app button should be hidden after clicking it.
  EXPECT_FALSE(IsQuickAppVisible());
}

// Test that setting a quick app which is not in the app list model does not
// show the quick app button.
TEST_F(HomeButtonWithQuickAppAccess, NonExistentApp) {
  EXPECT_FALSE(IsQuickAppVisible());
  EXPECT_FALSE(Shell::Get()->app_list_controller()->SetHomeButtonQuickApp(
      "Quick App Item"));
  EXPECT_FALSE(IsQuickAppVisible());
}

// Test that when setting a quick app with no icon, the quick app button doesn't
// show until an icon is loaded.
TEST_F(HomeButtonWithQuickAppAccess, AppWithNoIconThenLoaded) {
  base::HistogramTester histogram_tester;
  EXPECT_FALSE(IsQuickAppVisible());

  const std::string quick_app_id = "Quick App Item";
  AppListItem* item = new AppListItem(quick_app_id);
  GetAppListTestHelper()->model()->AddItem(item);

  EXPECT_TRUE(Shell::Get()->app_list_controller()->SetHomeButtonQuickApp(
      "Quick App Item"));

  // Check that the quick app item with no icon is not visible, and that icon
  // load was requested.
  EXPECT_FALSE(IsQuickAppVisible());
  EXPECT_EQ(std::vector<std::string>{quick_app_id},
            GetTestAppListClient()->load_icon_app_ids());

  // Set the default icon and check that the quick app button is visible after.
  item->SetDefaultIconAndColor(
      CreateSolidColorTestImage(gfx::Size(32, 32), SK_ColorRED), IconColor(),
      /*is_placeholder_icon=*/false);
  EXPECT_TRUE(IsQuickAppVisible());

  histogram_tester.ExpectTotalCount("Apps.QuickAppIconLoadTime", 1);
}

// Test that the quick app button image changes when setting a new quick app
// with a quick app button already shown.
TEST_F(HomeButtonWithQuickAppAccess, IconUpdatesOnNewQuickAppSet) {
  EXPECT_FALSE(IsQuickAppVisible());

  const std::string quick_app_id = "Quick App Item";
  AppListItem* item = new AppListItem(quick_app_id);
  GetAppListTestHelper()->model()->AddItem(item);
  item->SetDefaultIconAndColor(
      CreateSolidColorTestImage(gfx::Size(32, 32), SK_ColorRED), IconColor(),
      /*is_placeholder_icon=*/false);

  const std::string quick_app_id_two = "Quick App Item Two";
  AppListItem* item_two = new AppListItem(quick_app_id_two);
  GetAppListTestHelper()->model()->AddItem(item_two);
  item_two->SetDefaultIconAndColor(
      CreateSolidColorTestImage(gfx::Size(32, 32), SK_ColorBLUE), IconColor(),
      /*is_placeholder_icon=*/false);

  EXPECT_TRUE(
      Shell::Get()->app_list_controller()->SetHomeButtonQuickApp(quick_app_id));
  EXPECT_TRUE(IsQuickAppVisible());

  gfx::ImageSkia image_one =
      home_button()->quick_app_button_for_test()->GetImage(
          views::Button::STATE_NORMAL);

  EXPECT_TRUE(Shell::Get()->app_list_controller()->SetHomeButtonQuickApp(
      quick_app_id_two));
  EXPECT_TRUE(IsQuickAppVisible());

  gfx::ImageSkia image_two =
      home_button()->quick_app_button_for_test()->GetImage(
          views::Button::STATE_NORMAL);

  // Check that the quick app button image is changed after setting a new quick
  // app.
  EXPECT_FALSE(
      gfx::test::AreImagesEqual(gfx::Image(image_one), gfx::Image(image_two)));
}

// Test that the quick app button is hidden when the home button is pressed.
TEST_F(HomeButtonWithQuickAppAccess, HomeButtonPressed) {
  EXPECT_FALSE(IsQuickAppVisible());

  const std::string quick_app_id = "Quick App Item";
  GetAppListTestHelper()->model()->CreateAndAddItem(quick_app_id);
  EXPECT_TRUE(
      Shell::Get()->app_list_controller()->SetHomeButtonQuickApp(quick_app_id));

  EXPECT_TRUE(IsQuickAppVisible());

  gfx::Point center = home_button()->GetBoundsInScreen().CenterPoint();
  GetEventGenerator()->MoveMouseTo(center);
  GetEventGenerator()->ClickLeftButton();

  EXPECT_FALSE(IsQuickAppVisible());
}

// Test that the quick app button is hidden when the app list is opened.
TEST_F(HomeButtonWithQuickAppAccess, AppListOpened) {
  EXPECT_FALSE(IsQuickAppVisible());

  const std::string quick_app_id = "Quick App Item";
  GetAppListTestHelper()->model()->CreateAndAddItem(quick_app_id);
  EXPECT_TRUE(
      Shell::Get()->app_list_controller()->SetHomeButtonQuickApp(quick_app_id));

  EXPECT_TRUE(IsQuickAppVisible());

  GetAppListTestHelper()->ShowAppList();

  EXPECT_FALSE(IsQuickAppVisible());
}

// Test that the quick app button on both displays get shown and hidden
// together.
TEST_F(HomeButtonWithQuickAppAccess, TwoDisplays) {
  UpdateDisplay("10+10-500x400,600+10-1000x600/r");

  HomeButton* second_home_button =
      Shelf::ForWindow(Shell::GetAllRootWindows()[1])
          ->shelf_widget()
          ->navigation_widget()
          ->GetHomeButton();

  EXPECT_NE(home_button(), second_home_button);
  EXPECT_FALSE(second_home_button->quick_app_button_for_test());
  EXPECT_FALSE(home_button()->quick_app_button_for_test());

  // Set the quick app and ensure the quick app button is shown on both
  // displays.
  const std::string quick_app_id = "Quick App Item";
  GetAppListTestHelper()->model()->CreateAndAddItem(quick_app_id);
  EXPECT_TRUE(
      Shell::Get()->app_list_controller()->SetHomeButtonQuickApp(quick_app_id));

  EXPECT_TRUE(second_home_button->quick_app_button_for_test());
  EXPECT_TRUE(home_button()->quick_app_button_for_test());

  // Click the home button on the first display.
  gfx::Point center = home_button()->GetBoundsInScreen().CenterPoint();
  GetEventGenerator()->MoveMouseTo(center);
  GetEventGenerator()->ClickLeftButton();

  // Both quick app buttons should be hidden.
  EXPECT_FALSE(second_home_button->quick_app_button_for_test());
  EXPECT_FALSE(home_button()->quick_app_button_for_test());
}

// Test that setting an empty string as the quick app id hides the existing
// quick app button.
TEST_F(HomeButtonWithQuickAppAccess, EmptyAppId) {
  EXPECT_FALSE(IsQuickAppVisible());

  const std::string quick_app_id = "Quick App Item";
  GetAppListTestHelper()->model()->CreateAndAddItem(quick_app_id);

  // Setting the quick app to emtpy app id initially does not show the quick app
  // button.
  EXPECT_FALSE(Shell::Get()->app_list_controller()->SetHomeButtonQuickApp(""));
  EXPECT_FALSE(IsQuickAppVisible());

  // Set quick app id shows the quick app button.
  EXPECT_TRUE(
      Shell::Get()->app_list_controller()->SetHomeButtonQuickApp(quick_app_id));
  EXPECT_TRUE(IsQuickAppVisible());

  // Setting to an empty app id hides the quick app button.
  EXPECT_TRUE(Shell::Get()->app_list_controller()->SetHomeButtonQuickApp(""));
  EXPECT_FALSE(IsQuickAppVisible());
}

// Test that the quick app button animates when showing and hiding.
TEST_F(HomeButtonWithQuickAppAccess, QuickAppButtonAnimation) {
  EXPECT_FALSE(IsQuickAppVisible());
  ui::ScopedAnimationDurationScaleMode regular_animations(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  const std::string quick_app_id = "Quick App Item";
  GetAppListTestHelper()->model()->CreateAndAddItem(quick_app_id);
  EXPECT_TRUE(
      Shell::Get()->app_list_controller()->SetHomeButtonQuickApp(quick_app_id));

  // The quick app button should be animating when shown.
  EXPECT_TRUE(IsQuickAppVisible());
  auto* quick_app_button = home_button()->quick_app_button_for_test();
  EXPECT_EQ(0.0f, quick_app_button->layer()->opacity());
  EXPECT_EQ(1.0f, quick_app_button->layer()->GetTargetOpacity());
  EXPECT_TRUE(quick_app_button->layer()->GetAnimator()->is_animating());

  // Wait for quick app button to finish show animation.
  ui::LayerAnimationStoppedWaiter quick_app_button_animation_waiter;
  quick_app_button_animation_waiter.Wait(quick_app_button->layer());
  EXPECT_FALSE(quick_app_button->layer()->GetAnimator()->is_animating());

  const int quick_app_margin = 8;
  EXPECT_EQ(ShelfConfig::Get()->control_size() + quick_app_margin,
            quick_app_button->bounds().x());
  EXPECT_EQ(0, quick_app_button->bounds().y());

  // Show the app list to begin the hide quick app button animation.
  GetAppListTestHelper()->ShowAppList();
  EXPECT_TRUE(IsQuickAppVisible());
  EXPECT_EQ(1.0f, quick_app_button->layer()->opacity());
  EXPECT_EQ(0.0f, quick_app_button->layer()->GetTargetOpacity());
  EXPECT_TRUE(quick_app_button->layer()->GetAnimator()->is_animating());

  quick_app_button_animation_waiter.Wait(quick_app_button->layer());
  EXPECT_FALSE(IsQuickAppVisible());
}

// Test the left shelf quick app button position.
TEST_F(HomeButtonWithQuickAppAccess, LeftShelf) {
  Shelf* shelf = GetPrimaryShelf();
  EXPECT_EQ(ShelfAlignment::kBottom, shelf->alignment());

  const std::string quick_app_id = "Quick App Item";
  GetAppListTestHelper()->model()->CreateAndAddItem(quick_app_id);
  EXPECT_TRUE(
      Shell::Get()->app_list_controller()->SetHomeButtonQuickApp(quick_app_id));

  const int quick_app_margin = 8;

  // Set shelf to left alignment and check quick app button bounds.
  GetPrimaryShelf()->SetAlignment(ShelfAlignment::kLeft);

  auto* quick_app_button = home_button()->quick_app_button_for_test();
  EXPECT_EQ(home_button()->width() + quick_app_margin,
            quick_app_button->bounds().y());
  EXPECT_EQ(0, quick_app_button->bounds().x());

  // Test launching the quick app on the left aligned shelf.
  GetEventGenerator()->MoveMouseTo(
      quick_app_button->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->ClickLeftButton();
  EXPECT_FALSE(IsQuickAppVisible());
}

// Test the right shelf quick app button position.
TEST_F(HomeButtonWithQuickAppAccess, RightShelf) {
  Shelf* shelf = GetPrimaryShelf();
  EXPECT_EQ(ShelfAlignment::kBottom, shelf->alignment());

  const std::string quick_app_id = "Quick App Item";
  GetAppListTestHelper()->model()->CreateAndAddItem(quick_app_id);
  EXPECT_TRUE(
      Shell::Get()->app_list_controller()->SetHomeButtonQuickApp(quick_app_id));

  const int quick_app_margin = 8;

  // Set shelf to right alignment and check quick app button bounds.
  GetPrimaryShelf()->SetAlignment(ShelfAlignment::kRight);

  auto* quick_app_button = home_button()->quick_app_button_for_test();
  EXPECT_EQ(home_button()->width() + quick_app_margin,
            quick_app_button->bounds().y());
  EXPECT_EQ(0, quick_app_button->bounds().x());

  // Test launching the quick app on the right aligned shelf.
  GetEventGenerator()->MoveMouseTo(
      quick_app_button->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->ClickLeftButton();
  EXPECT_FALSE(IsQuickAppVisible());
}

// Change the quick app access model and test that the quick app button is
// updated.
TEST_F(HomeButtonWithQuickAppAccess, ModelChange) {
  EXPECT_FALSE(IsQuickAppVisible());

  const std::string quick_app_id = "Quick App Item";
  GetAppListTestHelper()->model()->CreateAndAddItem(quick_app_id);
  EXPECT_TRUE(
      Shell::Get()->app_list_controller()->SetHomeButtonQuickApp(quick_app_id));
  EXPECT_TRUE(IsQuickAppVisible());

  auto model_override = std::make_unique<test::AppListTestModel>();
  auto search_model_override = std::make_unique<SearchModel>();
  auto quick_app_access_model = std::make_unique<QuickAppAccessModel>();

  // Switch to new models, and verify that the quick app button is hidden.
  Shell::Get()->app_list_controller()->SetActiveModel(
      /*profile_id=*/1, model_override.get(), search_model_override.get(),
      quick_app_access_model.get());
  EXPECT_FALSE(IsQuickAppVisible());

  // Switch to original models, and verify the quick app button is shown again.
  Shell::Get()->app_list_controller()->SetActiveModel(
      /*profile_id=*/1, GetAppListTestHelper()->model(),
      GetAppListTestHelper()->search_model(),
      GetAppListTestHelper()->quick_app_access_model());
  EXPECT_TRUE(IsQuickAppVisible());
}

// Test once the quick app is hidden due to button activation, that setting
// the same quick app again will show it.
TEST_F(HomeButtonWithQuickAppAccess, SetSameQuickAppAfterActivation) {
  EXPECT_FALSE(IsQuickAppVisible());

  const std::string quick_app_id = "Quick App Item";
  GetAppListTestHelper()->model()->CreateAndAddItem(quick_app_id);
  EXPECT_TRUE(
      Shell::Get()->app_list_controller()->SetHomeButtonQuickApp(quick_app_id));
  EXPECT_TRUE(IsQuickAppVisible());

  auto* quick_app_button = home_button()->quick_app_button_for_test();

  // Activate and hide the quick app.
  GetEventGenerator()->MoveMouseTo(
      quick_app_button->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->ClickLeftButton();
  EXPECT_FALSE(IsQuickAppVisible());

  // Set the same app as quick app and check that the button exists again.
  EXPECT_TRUE(
      Shell::Get()->app_list_controller()->SetHomeButtonQuickApp(quick_app_id));
  EXPECT_TRUE(IsQuickAppVisible());
}

// Test once the quick app is hidden due to app list being shown, that
// setting the same quick app again will show it.
TEST_F(HomeButtonWithQuickAppAccess, SetSameQuickAppAfterAppListShown) {
  EXPECT_FALSE(IsQuickAppVisible());

  const std::string quick_app_id = "Quick App Item";
  GetAppListTestHelper()->model()->CreateAndAddItem(quick_app_id);
  EXPECT_TRUE(
      Shell::Get()->app_list_controller()->SetHomeButtonQuickApp(quick_app_id));
  EXPECT_TRUE(IsQuickAppVisible());

  // Open app list and expect quick app button to be hidden.
  GetAppListTestHelper()->ShowAppList();
  EXPECT_FALSE(IsQuickAppVisible());

  // Set the same app as quick app and check that the button exists again.
  EXPECT_TRUE(
      Shell::Get()->app_list_controller()->SetHomeButtonQuickApp(quick_app_id));
  EXPECT_TRUE(IsQuickAppVisible());
}

enum class TestAccessibilityFeature {
  kTabletModeShelfNavigationButtons,
  kSpokenFeedback,
  kAutoclick,
  kSwitchAccess
};

// Tests home button visibility with number of accessibility setting enabled,
// with kHideControlsInTabletModeFeature.
class HomeButtonVisibilityWithAccessibilityFeaturesTest
    : public HomeButtonTestBase,
      public ::testing::WithParamInterface<TestAccessibilityFeature> {
 public:
  HomeButtonVisibilityWithAccessibilityFeaturesTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kHideShelfControlsInTabletMode);
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

// The parameter indicates whether the kHideShelfControlsInTabletMode feature
// is enabled.
INSTANTIATE_TEST_SUITE_P(All, HomeButtonTest, testing::Bool());

// Tests that the shelf navigation widget clip rect is not clipping the intended
// home button bounds.
TEST_P(HomeButtonTest, ClipRectDoesNotClipHomeButtonBounds) {
  ShelfNavigationWidget* const nav_widget =
      GetPrimaryShelf()->navigation_widget();
  ShelfNavigationWidget::TestApi test_api(nav_widget);
  ASSERT_TRUE(test_api.IsHomeButtonVisible());
  ASSERT_TRUE(home_button());

  auto home_button_bounds = [&]() -> gfx::Rect {
    return home_button()->GetBoundsInScreen();
  };

  auto clip_rect_bounds = [&]() -> gfx::Rect {
    gfx::Rect clip_bounds = nav_widget->GetLayer()->clip_rect();
    wm::ConvertRectToScreen(nav_widget->GetNativeWindow(), &clip_bounds);
    return clip_bounds;
  };

  std::string display_configs[] = {
      "1+1-1200x1000",
      "1+1-1000x1200",
      "1+1-800x600",
      "1+1-600x800",
  };

  for (const auto& display_config : display_configs) {
    SCOPED_TRACE(display_config);
    UpdateDisplay(display_config);

    EXPECT_TRUE(clip_rect_bounds().Contains(home_button_bounds()));

    // Enter tablet mode - note that home button may be invisible in this case.
    ash::TabletModeControllerTestApi().EnterTabletMode();
    ShelfViewTestAPI shelf_test_api(
        GetPrimaryShelf()->GetShelfViewForTesting());
    shelf_test_api.RunMessageLoopUntilAnimationsDone(
        test_api.GetBoundsAnimator());

    if (home_button() && test_api.IsHomeButtonVisible())
      EXPECT_TRUE(clip_rect_bounds().Contains(home_button_bounds()));

    // Create a test widget to transition to in-app shelf.
    std::unique_ptr<views::Widget> widget =
        CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
    shelf_test_api.RunMessageLoopUntilAnimationsDone(
        test_api.GetBoundsAnimator());

    if (home_button() && test_api.IsHomeButtonVisible())
      EXPECT_TRUE(clip_rect_bounds().Contains(home_button_bounds()));

    // Back to home launcher shelf.
    widget.reset();
    shelf_test_api.RunMessageLoopUntilAnimationsDone(
        test_api.GetBoundsAnimator());

    if (home_button() && test_api.IsHomeButtonVisible())
      EXPECT_TRUE(clip_rect_bounds().Contains(home_button_bounds()));

    // Open another window and go back to clamshell.
    ash::TabletModeControllerTestApi().LeaveTabletMode();
    widget =
        CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
    shelf_test_api.RunMessageLoopUntilAnimationsDone(
        test_api.GetBoundsAnimator());

    EXPECT_TRUE(clip_rect_bounds().Contains(home_button_bounds()));

    // Verify bounds after the test widget is closed.
    widget.reset();
    shelf_test_api.RunMessageLoopUntilAnimationsDone(
        test_api.GetBoundsAnimator());

    EXPECT_TRUE(clip_rect_bounds().Contains(home_button_bounds()));
  }
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
  GetEventGenerator()->ClickLeftButton();
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(false);
}

TEST_P(HomeButtonTest, ClickToOpenAppListInTabletMode) {
  ash::TabletModeControllerTestApi().EnterTabletMode();

  Shelf* shelf = GetPrimaryShelf();
  EXPECT_EQ(ShelfAlignment::kBottom, shelf->alignment());

  ShelfNavigationWidget::TestApi test_api(shelf->navigation_widget());

  // Home button is expected to be hidden in tablet mode if shelf controls
  // should be hidden.
  const bool should_show_home_button =
      !IsHideShelfControlsInTabletModeEnabled();
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

  ash::TabletModeControllerTestApi().EnterTabletMode();

  Shelf* const shelf = GetPrimaryShelf();
  ShelfViewTestAPI shelf_test_api(shelf->GetShelfViewForTesting());
  ShelfNavigationWidget::TestApi test_api(shelf->navigation_widget());

  // Home button is expected to be hidden in tablet mode if shelf controls
  // should be hidden.
  const bool should_show_home_button =
      !IsHideShelfControlsInTabletModeEnabled();
  EXPECT_EQ(should_show_home_button, test_api.IsHomeButtonVisible());
  EXPECT_EQ(should_show_home_button, static_cast<bool>(home_button()));

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
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);

  // Wait for the navigation widget's animation.
  shelf_test_api.RunMessageLoopUntilAnimationsDone(
      test_api.GetBoundsAnimator());

  EXPECT_EQ(should_show_home_button, test_api.IsHomeButtonVisible());
  EXPECT_EQ(should_show_home_button, static_cast<bool>(home_button()));

  if (should_show_home_button)
    EXPECT_GT(home_button()->bounds().x(), 0);

  ash::TabletModeControllerTestApi().LeaveTabletMode();
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
  views::View* const home_button_view = home_button();
  ASSERT_TRUE(home_button_view);
  EXPECT_TRUE(home_button_view->GetVisible());
  EXPECT_EQ(1.0f, home_button_view->layer()->opacity());
  EXPECT_EQ(1.0f, home_button_view->layer()->GetTargetOpacity());

  // Switch to tablet mode changes the button visibility.
  ash::TabletModeControllerTestApi().EnterTabletMode();

  // Verify that the button view is still visible, and animating to 0 opacity.
  EXPECT_TRUE(home_button_view->GetVisible());
  EXPECT_EQ(1.0f, home_button_view->layer()->opacity());
  EXPECT_EQ(0.0f, home_button_view->layer()->GetTargetOpacity());

  // Once the opacity animation finishes, the button should not be visible.
  home_button_view->layer()->GetAnimator()->StopAnimating();
  EXPECT_FALSE(home_button_view->GetVisible());

  // Tablet mode exit should schedule animation to the visible state.
  ash::TabletModeControllerTestApi().LeaveTabletMode();
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
  views::View* const home_button_view = home_button();
  ASSERT_TRUE(home_button_view);

  EXPECT_TRUE(home_button_view->GetVisible());
  EXPECT_EQ(1.0f, home_button_view->layer()->opacity());
  EXPECT_EQ(1.0f, home_button_view->layer()->GetTargetOpacity());

  // Switch to tablet mode to initiate home button hide animation.
  ash::TabletModeControllerTestApi().EnterTabletMode();
  EXPECT_TRUE(home_button_view->GetVisible());
  EXPECT_EQ(1.0f, home_button_view->layer()->opacity());
  EXPECT_EQ(0.0f, home_button_view->layer()->GetTargetOpacity());
  home_button_view->layer()->GetAnimator()->StopAnimating();

  // Tablet mode exit should schedule an animation to the visible state.
  ash::TabletModeControllerTestApi().LeaveTabletMode();
  EXPECT_TRUE(home_button_view->GetVisible());
  EXPECT_EQ(0.0f, home_button_view->layer()->opacity());
  EXPECT_EQ(1.0f, home_button_view->layer()->GetTargetOpacity());

  // Enter tablet mode immediately, to interrupt the show animation.
  ash::TabletModeControllerTestApi().EnterTabletMode();
  EXPECT_TRUE(home_button_view->GetVisible());
  EXPECT_EQ(0.0f, home_button_view->layer()->opacity());
  EXPECT_EQ(0.0f, home_button_view->layer()->GetTargetOpacity());

  home_button_view->layer()->GetAnimator()->StopAnimating();
  EXPECT_FALSE(home_button_view->GetVisible());
}

// Verifies that home button becomes visible if reshown while a hide animation
// is still in progress.
TEST_F(HomeButtonAnimationTest, ShowWhileAnimatingToHide) {
  views::View* const home_button_view = home_button();
  ASSERT_TRUE(home_button_view);

  EXPECT_TRUE(home_button_view->GetVisible());
  EXPECT_EQ(1.0f, home_button_view->layer()->opacity());
  EXPECT_EQ(1.0f, home_button_view->layer()->GetTargetOpacity());

  // Switch to tablet mode to initiate the home button hide animation.
  ash::TabletModeControllerTestApi().EnterTabletMode();

  EXPECT_TRUE(home_button_view->GetVisible());
  EXPECT_EQ(1.0f, home_button_view->layer()->opacity());
  EXPECT_EQ(0.0f, home_button_view->layer()->GetTargetOpacity());

  // Tablet mode exit should schedule an animation to the visible state.
  ash::TabletModeControllerTestApi().LeaveTabletMode();
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
  views::View* const home_button_view = home_button();
  ASSERT_TRUE(home_button_view);
  EXPECT_TRUE(home_button_view->GetVisible());
  EXPECT_EQ(1.0f, home_button_view->layer()->opacity());
  EXPECT_EQ(1.0f, home_button_view->layer()->GetTargetOpacity());

  // Switch to tablet mode changes the button visibility.
  ash::TabletModeControllerTestApi().EnterTabletMode();

  Shelf* const shelf = GetPrimaryShelf();
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
  ash::TabletModeControllerTestApi().LeaveTabletMode();

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
  prefs()->SetBoolean(assistant::prefs::kAssistantEnabled, true);
  assistant_state()->NotifyFeatureAllowed(
      assistant::AssistantAllowedState::ALLOWED);
  assistant_state()->NotifyStatusChanged(assistant::AssistantStatus::READY);

  ShelfNavigationWidget::TestApi test_api(
      GetPrimaryShelf()->navigation_widget());
  EXPECT_TRUE(test_api.IsHomeButtonVisible());
  ASSERT_TRUE(home_button());

  ui::GestureEvent long_press = CreateGestureEvent(
      ui::GestureEventDetails(ui::EventType::kGestureLongPress));
  SendGestureEvent(&long_press);
  GetAppListTestHelper()->WaitUntilIdle();
  EXPECT_EQ(AssistantVisibility::kVisible,
            AssistantUiController::Get()->GetModel()->visibility());

  AssistantUiController::Get()->CloseUi(
      assistant::AssistantExitPoint::kUnspecified);
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
  prefs()->SetBoolean(assistant::prefs::kAssistantEnabled, true);
  assistant_state()->NotifyFeatureAllowed(
      assistant::AssistantAllowedState::ALLOWED);
  assistant_state()->NotifyStatusChanged(assistant::AssistantStatus::READY);

  ash::TabletModeControllerTestApi().EnterTabletMode();

  ShelfNavigationWidget::TestApi test_api(
      GetPrimaryShelf()->navigation_widget());
  const bool should_show_home_button =
      !IsHideShelfControlsInTabletModeEnabled();
  EXPECT_EQ(should_show_home_button, test_api.IsHomeButtonVisible());
  ASSERT_EQ(should_show_home_button, static_cast<bool>(home_button()));

  // App list should be shown by default in tablet mode.
  GetAppListTestHelper()->CheckVisibility(true);
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);

  if (!should_show_home_button)
    return;

  ui::GestureEvent long_press = CreateGestureEvent(
      ui::GestureEventDetails(ui::EventType::kGestureLongPress));
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
      assistant::AssistantExitPoint::kUnspecified);
}

TEST_P(HomeButtonTest, LongPressGestureWithSecondaryUser) {
  // Disallowed by secondary user.
  assistant_state()->NotifyFeatureAllowed(
      assistant::AssistantAllowedState::DISALLOWED_BY_NONPRIMARY_USER);

  // Enable the Assistant in system settings.
  prefs()->SetBoolean(assistant::prefs::kAssistantEnabled, true);

  ShelfNavigationWidget::TestApi test_api(
      GetPrimaryShelf()->navigation_widget());
  EXPECT_TRUE(test_api.IsHomeButtonVisible());
  ASSERT_TRUE(home_button());

  ui::GestureEvent long_press = CreateGestureEvent(
      ui::GestureEventDetails(ui::EventType::kGestureLongPress));
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
  prefs()->SetBoolean(assistant::prefs::kAssistantEnabled, false);
  assistant_state()->NotifyFeatureAllowed(
      assistant::AssistantAllowedState::ALLOWED);

  ShelfNavigationWidget::TestApi test_api(
      GetPrimaryShelf()->navigation_widget());
  EXPECT_TRUE(test_api.IsHomeButtonVisible());
  ASSERT_TRUE(home_button());

  ui::GestureEvent long_press = CreateGestureEvent(
      ui::GestureEventDetails(ui::EventType::kGestureLongPress));
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
  ShelfNavigationWidget* nav_widget = GetPrimaryShelf()->navigation_widget();
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

  ui::GestureEventDetails details =
      ui::GestureEventDetails(ui::EventType::kGestureTap);

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

// Checks the basic behavior of the label beside the home button when the
// HomeButtonWithText feature is enabled.
TEST_F(HomeButtonWithTextTest, Basic) {
  // Verify that the label is visible at the beginning.
  EXPECT_TRUE(IsLabelVisible());

  // Open the app list and check that the label still exists.
  gfx::Point center = home_button()->GetBoundsInScreen().CenterPoint();
  GetEventGenerator()->MoveMouseTo(center);
  GetEventGenerator()->ClickLeftButton();
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(true);
  EXPECT_TRUE(IsLabelVisible());

  // Change to tablet mode, where the label and home button shouldn't be
  // visible.
  ash::TabletModeControllerTestApi().EnterTabletMode();
  ShelfNavigationWidget::TestApi test_api(
      GetPrimaryShelf()->navigation_widget());
  EXPECT_FALSE(test_api.IsHomeButtonVisible());
  EXPECT_FALSE(IsLabelVisible());

  // Change back to clamshell mode. The label should be visible again.
  ash::TabletModeControllerTestApi().LeaveTabletMode();
  EXPECT_TRUE(IsLabelVisible());
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
  ash::TabletModeControllerTestApi().EnterTabletMode();
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
  ash::TabletModeControllerTestApi().EnterTabletMode();
  EXPECT_FALSE(test_api.IsHomeButtonVisible());

  // The button should be shown if the feature gets enabled.
  SetTestA11yFeatureEnabled(true /*enabled*/);
  EXPECT_TRUE(test_api.IsHomeButtonVisible());
}

}  // namespace ash
