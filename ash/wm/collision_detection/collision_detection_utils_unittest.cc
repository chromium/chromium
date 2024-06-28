// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/collision_detection/collision_detection_utils.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/keyboard/ui/test/keyboard_test_util.h"
#include "ash/public/cpp/app_list/app_list_controller.h"
#include "ash/public/cpp/keyboard/keyboard_switches.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shell.h"
#include "ash/system/notification_center/ash_message_popup_collection.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/test_window_builder.h"
#include "ash/wm/pip/pip_test_utils.h"
#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "ash/wm/work_area_insets.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "ui/aura/window.h"
#include "ui/display/scoped_display_for_new_windows.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

display::Display GetDisplayForWindow(aura::Window* window) {
  return display::Screen::GetScreen()->GetDisplayNearestWindow(window);
}

gfx::Rect ConvertToScreenForWindow(aura::Window* window,
                                   const gfx::Rect& bounds) {
  gfx::Rect new_bounds = bounds;
  ::wm::ConvertRectToScreen(window->GetRootWindow(), &new_bounds);
  return new_bounds;
}

gfx::Rect ConvertPrimaryToScreen(const gfx::Rect& bounds) {
  return ConvertToScreenForWindow(Shell::GetPrimaryRootWindow(), bounds);
}

}  // namespace

using CollisionDetectionUtilsTest = AshTestBase;

TEST_F(CollisionDetectionUtilsTest,
       RestingPositionSnapsInDisplayWithLargeAspectRatio) {
  UpdateDisplay("1600x400");

  // Snap to the top edge instead of the far left edge.
  EXPECT_EQ(ConvertPrimaryToScreen(gfx::Rect(500, 8, 100, 100)),
            CollisionDetectionUtils::GetRestingPosition(
                GetPrimaryDisplay(),
                ConvertPrimaryToScreen(gfx::Rect(500, 100, 100, 100)),
                CollisionDetectionUtils::RelativePriority::kPictureInPicture));
}

TEST_F(CollisionDetectionUtilsTest, AvoidObstaclesAvoidsUnifiedSystemTray) {
  UpdateDisplay("1000x900");
  auto* unified_system_tray = GetPrimaryUnifiedSystemTray();
  unified_system_tray->ShowBubble();

  auto display = GetPrimaryDisplay();
  gfx::Rect area = CollisionDetectionUtils::GetMovementArea(display);
  gfx::Rect bubble_bounds = unified_system_tray->GetBubbleBoundsInScreen();
  gfx::Rect bounds = gfx::Rect(bubble_bounds.x(), bubble_bounds.y(), 100, 100);
  gfx::Rect moved_bounds = CollisionDetectionUtils::GetRestingPosition(
      display, bounds,
      CollisionDetectionUtils::RelativePriority::kPictureInPicture);

  // Expect that the returned bounds don't intersect the unified system tray
  // but also don't leave the PIP movement area.
  EXPECT_FALSE(moved_bounds.Intersects(bubble_bounds));
  EXPECT_TRUE(area.Contains(moved_bounds));
}

TEST_F(CollisionDetectionUtilsTest, AvoidObstaclesAvoidsPopupNotification) {
  UpdateDisplay("1000x900");
  auto* window = CreateTestWindowInShellWithId(kShellWindowId_ShelfContainer);
  window->SetName(AshMessagePopupCollection::kMessagePopupWidgetName);
  window->Show();

  auto display = GetPrimaryDisplay();
  gfx::Rect area = CollisionDetectionUtils::GetMovementArea(display);
  gfx::Rect popup_bounds = window->GetBoundsInScreen();
  gfx::Rect bounds = gfx::Rect(popup_bounds.x(), popup_bounds.y(), 100, 100);
  gfx::Rect moved_bounds = CollisionDetectionUtils::GetRestingPosition(
      display, bounds,
      CollisionDetectionUtils::RelativePriority::kPictureInPicture);

  // Expect that the returned bounds don't intersect the popup message window
  // but also don't leave the PIP movement area.
  EXPECT_FALSE(moved_bounds.Intersects(popup_bounds));
  EXPECT_TRUE(area.Contains(moved_bounds));
}

TEST_F(CollisionDetectionUtilsTest, AvoidObstaclesAvoidsClamshellLauncher) {
  UpdateDisplay("1000x900");
  AppListController* app_list_controller = AppListController::Get();
  app_list_controller->ShowAppList(AppListShowSource::kSearchKey);

  display::Display display = GetPrimaryDisplay();
  gfx::Rect movement_area = CollisionDetectionUtils::GetMovementArea(display);
  gfx::Rect bubble_bounds =
      app_list_controller->GetWindow()->GetBoundsInScreen();
  // Start with bounds that overlap the bubble window.
  gfx::Rect bounds = gfx::Rect(bubble_bounds.x(), bubble_bounds.y(), 100, 100);
  gfx::Rect moved_bounds = CollisionDetectionUtils::GetRestingPosition(
      display, bounds,
      CollisionDetectionUtils::RelativePriority::kPictureInPicture);

  // Expect that the returned bounds don't intersect the bubble window but also
  // don't leave the PIP movement area.
  EXPECT_FALSE(moved_bounds.Intersects(bubble_bounds));
  EXPECT_TRUE(movement_area.Contains(moved_bounds));
}

class CollisionDetectionUtilsDisplayTest
    : public AshTestBase,
      public ::testing::WithParamInterface<
          std::tuple<std::string, std::size_t>> {
 public:
  void SetUp() override {
    AshTestBase::SetUp();
    SetVirtualKeyboardEnabled(true);

    const std::string& display_string = std::get<0>(GetParam());
    const std::size_t root_window_index = std::get<1>(GetParam());
    UpdateWorkArea(display_string);
    ASSERT_LT(root_window_index, Shell::GetAllRootWindows().size());
    root_window_ = Shell::GetAllRootWindows()[root_window_index].get();
    scoped_display_ =
        std::make_unique<display::ScopedDisplayForNewWindows>(root_window_);
    for (auto* root_window_controller : Shell::GetAllRootWindowControllers()) {
      auto* shelf = root_window_controller->shelf();
      shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlwaysHidden);
    }
  }

  void TearDown() override {
    scoped_display_.reset();
    AshTestBase::TearDown();
  }

 protected:
  display::Display GetDisplay() { return GetDisplayForWindow(root_window_); }

  gfx::Rect GetKeyboardBounds(int keyboard_height) {
    gfx::Rect keyboard_bounds(GetDisplay().bounds().size());
    keyboard_bounds.set_y(keyboard_bounds.bottom() - keyboard_height);
    keyboard_bounds.set_height(100);
    return keyboard_bounds;
  }

  void TransposeIfPortrait(gfx::Rect* rect) {
    bool landscape =
        GetDisplay().bounds().width() > GetDisplay().bounds().height();
    if (!landscape) {
      rect->SetRect(rect->y(), rect->x(), rect->height(), rect->width());
    }
  }

  aura::Window* root_window() { return root_window_; }

  gfx::Rect ConvertToScreen(const gfx::Rect& bounds) {
    return ConvertToScreenForWindow(root_window_, bounds);
  }

  gfx::Rect CallAvoidObstacles(
      const display::Display& display,
      gfx::Rect bounds,
      CollisionDetectionUtils::RelativePriority priority =
          CollisionDetectionUtils::RelativePriority::kPictureInPicture) {
    return CollisionDetectionUtils::AvoidObstacles(display, bounds, priority);
  }

  void UpdateWorkArea(const std::string& bounds) {
    UpdateDisplay(bounds);
    for (aura::Window* root : Shell::GetAllRootWindows()) {
      WorkAreaInsets::ForWindow(root)->UpdateWorkAreaInsetsForTest(
          root, gfx::Rect(), gfx::Insets(), gfx::Insets());
    }
  }

 private:
  std::unique_ptr<display::ScopedDisplayForNewWindows> scoped_display_;
  raw_ptr<aura::Window, DanglingUntriaged> root_window_;
};

TEST_P(CollisionDetectionUtilsDisplayTest, MovementAreaIsInset) {
  gfx::Rect area = CollisionDetectionUtils::GetMovementArea(GetDisplay());
  gfx::Rect expected(8, 8, 484, 384);
  TransposeIfPortrait(&expected);
  EXPECT_EQ(ConvertToScreen(expected), area);
}

TEST_P(CollisionDetectionUtilsDisplayTest,
       MovementAreaIncludesKeyboardIfKeyboardIsShown) {
  auto* keyboard_controller = keyboard::KeyboardUIController::Get();
  keyboard_controller->ShowKeyboardInDisplay(GetDisplay());
  ASSERT_TRUE(keyboard::test::WaitUntilShown());
  aura::Window* keyboard_window = keyboard_controller->GetKeyboardWindow();

  constexpr int keyboard_height = 100;
  gfx::Rect keyboard_bounds = GetKeyboardBounds(keyboard_height);
  keyboard_window->SetBounds(keyboard_bounds);

  gfx::Rect expected = gfx::Rect(GetDisplay().bounds().size());
  expected.Inset(gfx::Insets::TLBR(0, 0, keyboard_height, 0));
  expected.Inset(8);

  gfx::Rect area = CollisionDetectionUtils::GetMovementArea(GetDisplay());
  EXPECT_EQ(ConvertToScreen(expected), area);
}

TEST_P(CollisionDetectionUtilsDisplayTest, RestingPositionSnapsToClosestEdge) {
  auto display = GetDisplay();
  int right = display.bounds().width();
  int bottom = display.bounds().height();

  // Snap near top edge to top.
  EXPECT_EQ(ConvertToScreen(gfx::Rect(100, 8, 100, 100)),
            CollisionDetectionUtils::GetRestingPosition(
                display, ConvertToScreen(gfx::Rect(100, 50, 100, 100)),
                CollisionDetectionUtils::RelativePriority::kPictureInPicture));

  // Snap near bottom edge to bottom.
  EXPECT_EQ(ConvertToScreen(gfx::Rect(100, bottom - 108, 100, 100)),
            CollisionDetectionUtils::GetRestingPosition(
                display, ConvertToScreen(gfx::Rect(100, bottom - 50, 100, 100)),
                CollisionDetectionUtils::RelativePriority::kPictureInPicture));

  // Snap near left edge to left.
  EXPECT_EQ(ConvertToScreen(gfx::Rect(8, 100, 100, 100)),
            CollisionDetectionUtils::GetRestingPosition(
                display, ConvertToScreen(gfx::Rect(50, 100, 100, 100)),
                CollisionDetectionUtils::RelativePriority::kPictureInPicture));

  // Snap near right edge to right.
  EXPECT_EQ(ConvertToScreen(gfx::Rect(right - 108, 100, 100, 100)),
            CollisionDetectionUtils::GetRestingPosition(
                display, ConvertToScreen(gfx::Rect(right - 50, 100, 100, 100)),
                CollisionDetectionUtils::RelativePriority::kPictureInPicture));
}

TEST_P(CollisionDetectionUtilsDisplayTest, RestingPositionSnapsInsideDisplay) {
  auto display = GetDisplay();
  int right = display.bounds().width();
  int bottom = display.bounds().height();

  // Snap near top edge outside movement area to top.
  EXPECT_EQ(ConvertToScreen(gfx::Rect(100, 8, 100, 100)),
            CollisionDetectionUtils::GetRestingPosition(
                display, ConvertToScreen(gfx::Rect(100, -50, 100, 100)),
                CollisionDetectionUtils::RelativePriority::kPictureInPicture));

  // Snap near bottom edge outside movement area to bottom.
  EXPECT_EQ(ConvertToScreen(gfx::Rect(100, bottom - 108, 100, 100)),
            CollisionDetectionUtils::GetRestingPosition(
                display, ConvertToScreen(gfx::Rect(100, 1000, 100, 100)),
                CollisionDetectionUtils::RelativePriority::kPictureInPicture));

  // Snap near left edge outside movement area to left.
  EXPECT_EQ(ConvertToScreen(gfx::Rect(8, 100, 100, 100)),
            CollisionDetectionUtils::GetRestingPosition(
                display, ConvertToScreen(gfx::Rect(-50, 100, 100, 100)),
                CollisionDetectionUtils::RelativePriority::kPictureInPicture));

  // Snap near right edge outside movement area to right.
  EXPECT_EQ(ConvertToScreen(gfx::Rect(right - 108, 100, 100, 100)),
            CollisionDetectionUtils::GetRestingPosition(
                display, ConvertToScreen(gfx::Rect(1000, 100, 100, 100)),
                CollisionDetectionUtils::RelativePriority::kPictureInPicture));
}

TEST_P(CollisionDetectionUtilsDisplayTest,
       RestingPositionWorksIfKeyboardIsDisabled) {
  SetVirtualKeyboardEnabled(false);
  auto display = GetDisplay();

  // Snap near top edge to top.
  EXPECT_EQ(ConvertToScreen(gfx::Rect(100, 8, 100, 100)),
            CollisionDetectionUtils::GetRestingPosition(
                display, ConvertToScreen(gfx::Rect(100, 50, 100, 100)),
                CollisionDetectionUtils::RelativePriority::kPictureInPicture));
}

TEST_P(CollisionDetectionUtilsDisplayTest,
       AvoidObstaclesAvoidsFloatingKeyboard) {
  auto display = GetDisplay();

  auto* keyboard_controller = keyboard::KeyboardUIController::Get();
  keyboard_controller->SetContainerType(keyboard::ContainerType::kFloating,
                                        gfx::Rect(), base::DoNothing());
  keyboard_controller->ShowKeyboardInDisplay(display);
  ASSERT_TRUE(keyboard::test::WaitUntilShown());
  aura::Window* keyboard_window = keyboard_controller->GetKeyboardWindow();
  keyboard_window->SetBounds(gfx::Rect(0, 0, 100, 100));

  gfx::Rect area = CollisionDetectionUtils::GetMovementArea(display);
  gfx::Rect moved_bounds =
      CallAvoidObstacles(display, ConvertToScreen(gfx::Rect(8, 8, 100, 100)));

  // Expect that the returned bounds don't intersect the floating keyboard
  // but also don't leave the movement area.
  EXPECT_FALSE(moved_bounds.Intersects(keyboard_window->GetBoundsInScreen()));
  EXPECT_TRUE(area.Contains(moved_bounds));
}

TEST_P(CollisionDetectionUtilsDisplayTest,
       AvoidObstaclesDoesNotChangeBoundsIfThereIsNoCollision) {
  auto display = GetDisplay();
  EXPECT_EQ(ConvertToScreen(gfx::Rect(100, 100, 100, 100)),
            CallAvoidObstacles(display,
                               ConvertToScreen(gfx::Rect(100, 100, 100, 100))));
}

TEST_P(CollisionDetectionUtilsDisplayTest,
       AvoidObstaclesMovesForHigherPriorityWindow) {
  auto display = GetDisplay();
  aura::Window* accessibility_bubble_container = Shell::GetContainer(
      root_window(), kShellWindowId_AccessibilityBubbleContainer);
  std::unique_ptr<aura::Window> prioritized_window =
      ChildTestWindowBuilder(accessibility_bubble_container,
                             gfx::Rect(100, 100, 100, 10), 0)
          .Build();

  gfx::Rect position_before_collision_detection(100, 100, 100, 100);
  gfx::Rect position_when_moved_by_collision_detection(100, 118, 100, 100);

  const struct {
    CollisionDetectionUtils::RelativePriority window_priority;
    CollisionDetectionUtils::RelativePriority avoid_obstacles_priority;
    gfx::Rect expected_position;
  } kTestCases[] = {
      // If the fixed window is kDefault, all other windows should move.
      {CollisionDetectionUtils::RelativePriority::kDefault,
       CollisionDetectionUtils::RelativePriority::kPictureInPicture,
       position_when_moved_by_collision_detection},
      {CollisionDetectionUtils::RelativePriority::kDefault,
       CollisionDetectionUtils::RelativePriority::kAutomaticClicksMenu,
       position_when_moved_by_collision_detection},
      {CollisionDetectionUtils::RelativePriority::kDefault,
       CollisionDetectionUtils::RelativePriority::kSwitchAccessMenu,
       position_when_moved_by_collision_detection},
      // If the fixed window is PIP, Autoclick or Switch Access should not move.
      {CollisionDetectionUtils::RelativePriority::kPictureInPicture,
       CollisionDetectionUtils::RelativePriority::kAutomaticClicksMenu,
       position_before_collision_detection},
      {CollisionDetectionUtils::RelativePriority::kPictureInPicture,
       CollisionDetectionUtils::RelativePriority::kSwitchAccessMenu,
       position_before_collision_detection},
      // The PIP should not move for itself.
      {CollisionDetectionUtils::RelativePriority::kPictureInPicture,
       CollisionDetectionUtils::RelativePriority::kPictureInPicture,
       position_before_collision_detection},
      // If the fixed window is the Switch Access menu, the PIP should move, but
      // the Autoclick menu should not.
      {CollisionDetectionUtils::RelativePriority::kSwitchAccessMenu,
       CollisionDetectionUtils::RelativePriority::kPictureInPicture,
       position_when_moved_by_collision_detection},
      {CollisionDetectionUtils::RelativePriority::kSwitchAccessMenu,
       CollisionDetectionUtils::RelativePriority::kAutomaticClicksMenu,
       position_before_collision_detection},
      // The Switch Access menu should not move for itself.
      {CollisionDetectionUtils::RelativePriority::kSwitchAccessMenu,
       CollisionDetectionUtils::RelativePriority::kSwitchAccessMenu,
       position_before_collision_detection},
      // If the fixed window is Automatic Clicks, both the PIP and the the
      // Switch Access menu should move.
      {CollisionDetectionUtils::RelativePriority::kAutomaticClicksMenu,
       CollisionDetectionUtils::RelativePriority::kPictureInPicture,
       position_when_moved_by_collision_detection},
      {CollisionDetectionUtils::RelativePriority::kAutomaticClicksMenu,
       CollisionDetectionUtils::RelativePriority::kSwitchAccessMenu,
       position_when_moved_by_collision_detection},
      // Autoclicks menu should not move for itself.
      {CollisionDetectionUtils::RelativePriority::kAutomaticClicksMenu,
       CollisionDetectionUtils::RelativePriority::kAutomaticClicksMenu,
       position_before_collision_detection},
  };

  for (const auto& test : kTestCases) {
    CollisionDetectionUtils::MarkWindowPriorityForCollisionDetection(
        prioritized_window.get(), test.window_priority);
    EXPECT_EQ(ConvertToScreen(test.expected_position),
              CallAvoidObstacles(
                  display, ConvertToScreen(position_before_collision_detection),
                  test.avoid_obstacles_priority));
  }
}

TEST_P(CollisionDetectionUtilsDisplayTest, GetRestingPositionAvoidsKeyboard) {
  auto display = GetDisplay();

  auto* keyboard_controller = keyboard::KeyboardUIController::Get();
  keyboard_controller->ShowKeyboardInDisplay(display);
  ASSERT_TRUE(keyboard::test::WaitUntilShown());
  aura::Window* keyboard_window = keyboard_controller->GetKeyboardWindow();

  constexpr int keyboard_height = 100;
  gfx::Rect keyboard_bounds = GetKeyboardBounds(keyboard_height);
  keyboard_window->SetBounds(keyboard_bounds);

  gfx::Rect expected =
      gfx::Rect(8, display.bounds().height() - 100 - 108, 100, 100);
  EXPECT_EQ(ConvertToScreen(expected),
            CollisionDetectionUtils::GetRestingPosition(
                display, ConvertToScreen(gfx::Rect(8, 500, 100, 100)),
                CollisionDetectionUtils::RelativePriority::kPictureInPicture));
}

TEST_P(CollisionDetectionUtilsDisplayTest, AutoHideShownShelfAffectsWindow) {
  auto* shelf = Shelf::ForWindow(root_window());
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  auto shelf_bounds = shelf->GetWindow()->GetBoundsInScreen();
  // Use a smaller window so it is guaranteed to find a free space to move to.
  auto bounds = CallAvoidObstacles(
      GetDisplay(), gfx::Rect(shelf_bounds.CenterPoint(), gfx::Size(1, 1)));
  EXPECT_FALSE(shelf_bounds.Intersects(bounds));
}

TEST_P(CollisionDetectionUtilsDisplayTest,
       AvoidObstaclesWorksWithHorizontalShelf) {
  auto* shelf = Shelf::ForWindow(root_window());
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  shelf->SetAlignment(ShelfAlignment::kLeft);
  EXPECT_FALSE(shelf->IsHorizontalAlignment());
  ShelfLayoutManager* manager = shelf->shelf_layout_manager();
  manager->LayoutShelf();

  auto shelf_bounds = shelf->GetWindow()->GetBoundsInScreen();
  {
    auto initial_bounds = gfx::Rect(shelf_bounds.right() - 10,
                                    shelf_bounds.CenterPoint().y(), 1, 1);
    auto bounds = CallAvoidObstacles(GetDisplay(), initial_bounds);
    EXPECT_NE(initial_bounds, bounds);
  }
  {
    auto initial_bounds = gfx::Rect(shelf_bounds.right() + 10,
                                    shelf_bounds.CenterPoint().y(), 1, 1);
    auto bounds = CallAvoidObstacles(GetDisplay(), initial_bounds);
    EXPECT_EQ(initial_bounds, bounds);
  }
}

// TODO: UpdateDisplay() doesn't support different layouts of multiple displays.
// We should add some way to try multiple layouts.
INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    CollisionDetectionUtilsDisplayTest,
    testing::Values(std::make_tuple("500x400", 0u),
                    std::make_tuple("500x400/r", 0u),
                    std::make_tuple("500x400/u", 0u),
                    std::make_tuple("500x400/l", 0u),
                    std::make_tuple("1000x800*2", 0u),
                    std::make_tuple("500x400,500x400", 0u),
                    std::make_tuple("500x400,500x400", 1u)));

class CollisionDetectionUtilsLogicTest : public ::testing::Test {
 public:
  gfx::Rect CallAvoidObstaclesInternal(
      const gfx::Rect& work_area,
      const std::vector<gfx::Rect>& rects,
      const gfx::Rect& bounds,
      CollisionDetectionUtils::RelativePriority priority =
          CollisionDetectionUtils::RelativePriority::kPictureInPicture) {
    return CollisionDetectionUtils::AvoidObstaclesInternal(work_area, rects,
                                                           bounds, priority);
  }
};

TEST_F(CollisionDetectionUtilsLogicTest,
       AvoidObstaclesDoesNotMoveBoundsIfThereIsNoIntersection) {
  const gfx::Rect area(0, 0, 400, 400);

  // Check no collision with Rect.
  EXPECT_EQ(gfx::Rect(200, 0, 100, 100),
            CallAvoidObstaclesInternal(area, {gfx::Rect(0, 0, 100, 100)},
                                       gfx::Rect(200, 0, 100, 100)));

  // Check no collision with edges of the work area. Provide an obstacle so
  // it has something to stick to, to distinguish failure from correctly
  // not moving the window bounds.
  // Check corners:
  EXPECT_EQ(gfx::Rect(0, 0, 100, 100),
            CallAvoidObstaclesInternal(area, {gfx::Rect(300, 300, 1, 1)},
                                       gfx::Rect(0, 0, 100, 100)));
  EXPECT_EQ(gfx::Rect(300, 0, 100, 100),
            CallAvoidObstaclesInternal(area, {gfx::Rect(10, 10, 1, 1)},
                                       gfx::Rect(300, 0, 100, 100)));
  EXPECT_EQ(gfx::Rect(0, 300, 100, 100),
            CallAvoidObstaclesInternal(area, {gfx::Rect(10, 10, 1, 1)},
                                       gfx::Rect(0, 300, 100, 100)));
  EXPECT_EQ(gfx::Rect(300, 300, 100, 100),
            CallAvoidObstaclesInternal(area, {gfx::Rect(10, 10, 1, 1)},
                                       gfx::Rect(300, 300, 100, 100)));

  // Check edges:
  EXPECT_EQ(gfx::Rect(100, 0, 100, 100),
            CallAvoidObstaclesInternal(area, {gfx::Rect(10, 10, 1, 1)},
                                       gfx::Rect(100, 0, 100, 100)));
  EXPECT_EQ(gfx::Rect(0, 100, 100, 100),
            CallAvoidObstaclesInternal(area, {gfx::Rect(10, 10, 1, 1)},
                                       gfx::Rect(0, 100, 100, 100)));
  EXPECT_EQ(gfx::Rect(300, 100, 100, 100),
            CallAvoidObstaclesInternal(area, {gfx::Rect(10, 10, 1, 1)},
                                       gfx::Rect(300, 100, 100, 100)));
  EXPECT_EQ(gfx::Rect(100, 300, 100, 100),
            CallAvoidObstaclesInternal(area, {gfx::Rect(10, 10, 1, 1)},
                                       gfx::Rect(100, 300, 100, 100)));
}

TEST_F(CollisionDetectionUtilsLogicTest, AvoidObstaclesOffByOneCases) {
  const gfx::Rect area(0, 0, 400, 400);

  // Test 1x1 window intersecting a 1x1 obstacle.
  EXPECT_EQ(gfx::Rect(9, 10, 1, 1),
            CallAvoidObstaclesInternal(area, {gfx::Rect(10, 10, 1, 1)},
                                       gfx::Rect(10, 10, 1, 1)));
  // Test 1x1 window adjacent to a 1x1 obstacle.
  EXPECT_EQ(gfx::Rect(9, 10, 1, 1),
            CallAvoidObstaclesInternal(area, {gfx::Rect(10, 10, 1, 1)},
                                       gfx::Rect(9, 10, 1, 1)));
  EXPECT_EQ(gfx::Rect(11, 10, 1, 1),
            CallAvoidObstaclesInternal(area, {gfx::Rect(10, 10, 1, 1)},
                                       gfx::Rect(11, 10, 1, 1)));
  EXPECT_EQ(gfx::Rect(10, 9, 1, 1),
            CallAvoidObstaclesInternal(area, {gfx::Rect(10, 10, 1, 1)},
                                       gfx::Rect(10, 9, 1, 1)));
  EXPECT_EQ(gfx::Rect(10, 11, 1, 1),
            CallAvoidObstaclesInternal(area, {gfx::Rect(10, 10, 1, 1)},
                                       gfx::Rect(10, 11, 1, 1)));
  EXPECT_EQ(gfx::Rect(9, 9, 1, 1),
            CallAvoidObstaclesInternal(area, {gfx::Rect(10, 10, 1, 1)},
                                       gfx::Rect(9, 9, 1, 1)));
  EXPECT_EQ(gfx::Rect(11, 11, 1, 1),
            CallAvoidObstaclesInternal(area, {gfx::Rect(10, 10, 1, 1)},
                                       gfx::Rect(11, 11, 1, 1)));
  EXPECT_EQ(gfx::Rect(11, 9, 1, 1),
            CallAvoidObstaclesInternal(area, {gfx::Rect(10, 10, 1, 1)},
                                       gfx::Rect(11, 9, 1, 1)));
  EXPECT_EQ(gfx::Rect(9, 11, 1, 1),
            CallAvoidObstaclesInternal(area, {gfx::Rect(10, 10, 1, 1)},
                                       gfx::Rect(9, 11, 1, 1)));

  // Test 1x1 window intersecting a 2x2 obstacle.
  EXPECT_EQ(gfx::Rect(9, 10, 1, 1),
            CallAvoidObstaclesInternal(area, {gfx::Rect(10, 10, 2, 2)},
                                       gfx::Rect(10, 10, 1, 1)));
  EXPECT_EQ(gfx::Rect(9, 11, 1, 1),
            CallAvoidObstaclesInternal(area, {gfx::Rect(10, 10, 2, 2)},
                                       gfx::Rect(10, 11, 1, 1)));
  EXPECT_EQ(gfx::Rect(12, 10, 1, 1),
            CallAvoidObstaclesInternal(area, {gfx::Rect(10, 10, 2, 2)},
                                       gfx::Rect(11, 10, 1, 1)));
  EXPECT_EQ(gfx::Rect(12, 11, 1, 1),
            CallAvoidObstaclesInternal(area, {gfx::Rect(10, 10, 2, 2)},
                                       gfx::Rect(11, 11, 1, 1)));

  // Test 1x1 window adjacent to a 2x2 obstacle.
  EXPECT_EQ(gfx::Rect(9, 10, 1, 1),
            CallAvoidObstaclesInternal(area, {gfx::Rect(10, 10, 2, 2)},
                                       gfx::Rect(9, 10, 1, 1)));
  EXPECT_EQ(gfx::Rect(9, 11, 1, 1),
            CallAvoidObstaclesInternal(area, {gfx::Rect(10, 10, 2, 2)},
                                       gfx::Rect(9, 11, 1, 1)));
  EXPECT_EQ(gfx::Rect(9, 12, 1, 1),
            CallAvoidObstaclesInternal(area, {gfx::Rect(10, 10, 2, 2)},
                                       gfx::Rect(9, 12, 1, 1)));
  EXPECT_EQ(gfx::Rect(10, 12, 1, 1),
            CallAvoidObstaclesInternal(area, {gfx::Rect(10, 10, 2, 2)},
                                       gfx::Rect(10, 12, 1, 1)));
  EXPECT_EQ(gfx::Rect(11, 12, 1, 1),
            CallAvoidObstaclesInternal(area, {gfx::Rect(10, 10, 2, 2)},
                                       gfx::Rect(11, 12, 1, 1)));
  EXPECT_EQ(gfx::Rect(12, 12, 1, 1),
            CallAvoidObstaclesInternal(area, {gfx::Rect(10, 10, 2, 2)},
                                       gfx::Rect(12, 12, 1, 1)));
  EXPECT_EQ(gfx::Rect(12, 11, 1, 1),
            CallAvoidObstaclesInternal(area, {gfx::Rect(10, 10, 2, 2)},
                                       gfx::Rect(12, 11, 1, 1)));
  EXPECT_EQ(gfx::Rect(12, 10, 1, 1),
            CallAvoidObstaclesInternal(area, {gfx::Rect(10, 10, 2, 2)},
                                       gfx::Rect(12, 10, 1, 1)));
  EXPECT_EQ(gfx::Rect(12, 9, 1, 1),
            CallAvoidObstaclesInternal(area, {gfx::Rect(10, 10, 2, 2)},
                                       gfx::Rect(12, 9, 1, 1)));
  EXPECT_EQ(gfx::Rect(11, 9, 1, 1),
            CallAvoidObstaclesInternal(area, {gfx::Rect(10, 10, 2, 2)},
                                       gfx::Rect(11, 9, 1, 1)));
  EXPECT_EQ(gfx::Rect(10, 9, 1, 1),
            CallAvoidObstaclesInternal(area, {gfx::Rect(10, 10, 2, 2)},
                                       gfx::Rect(10, 9, 1, 1)));
  EXPECT_EQ(gfx::Rect(9, 9, 1, 1),
            CallAvoidObstaclesInternal(area, {gfx::Rect(10, 10, 2, 2)},
                                       gfx::Rect(9, 9, 1, 1)));

  // Test 2x2 window intersecting a 2x2 obstacle.
  EXPECT_EQ(gfx::Rect(8, 9, 2, 2),
            CallAvoidObstaclesInternal(area, {gfx::Rect(10, 10, 2, 2)},
                                       gfx::Rect(9, 9, 2, 2)));
  EXPECT_EQ(gfx::Rect(12, 9, 2, 2),
            CallAvoidObstaclesInternal(area, {gfx::Rect(10, 10, 2, 2)},
                                       gfx::Rect(11, 9, 2, 2)));
  EXPECT_EQ(gfx::Rect(12, 11, 2, 2),
            CallAvoidObstaclesInternal(area, {gfx::Rect(10, 10, 2, 2)},
                                       gfx::Rect(11, 11, 2, 2)));
  EXPECT_EQ(gfx::Rect(8, 11, 2, 2),
            CallAvoidObstaclesInternal(area, {gfx::Rect(10, 10, 2, 2)},
                                       gfx::Rect(9, 11, 2, 2)));

  // Test 3x3 window intersecting a 2x2 obstacle.
  EXPECT_EQ(gfx::Rect(7, 8, 3, 3),
            CallAvoidObstaclesInternal(area, {gfx::Rect(10, 10, 2, 2)},
                                       gfx::Rect(8, 8, 3, 3)));
  EXPECT_EQ(gfx::Rect(12, 8, 3, 3),
            CallAvoidObstaclesInternal(area, {gfx::Rect(10, 10, 2, 2)},
                                       gfx::Rect(11, 8, 3, 3)));
  EXPECT_EQ(gfx::Rect(12, 11, 3, 3),
            CallAvoidObstaclesInternal(area, {gfx::Rect(10, 10, 2, 2)},
                                       gfx::Rect(11, 11, 3, 3)));
  EXPECT_EQ(gfx::Rect(7, 11, 3, 3),
            CallAvoidObstaclesInternal(area, {gfx::Rect(10, 10, 2, 2)},
                                       gfx::Rect(8, 11, 3, 3)));

  // Test 3x3 window adjacent to a 2x2 obstacle.
  EXPECT_EQ(gfx::Rect(7, 10, 3, 3),
            CallAvoidObstaclesInternal(area, {gfx::Rect(10, 10, 2, 2)},
                                       gfx::Rect(7, 10, 3, 3)));
  EXPECT_EQ(gfx::Rect(12, 10, 3, 3),
            CallAvoidObstaclesInternal(area, {gfx::Rect(10, 10, 2, 2)},
                                       gfx::Rect(12, 10, 3, 3)));
  EXPECT_EQ(gfx::Rect(9, 7, 3, 3),
            CallAvoidObstaclesInternal(area, {gfx::Rect(10, 10, 2, 2)},
                                       gfx::Rect(9, 7, 3, 3)));
  EXPECT_EQ(gfx::Rect(9, 12, 3, 3),
            CallAvoidObstaclesInternal(area, {gfx::Rect(10, 10, 2, 2)},
                                       gfx::Rect(9, 12, 3, 3)));
}

TEST_F(CollisionDetectionUtilsLogicTest, AvoidObstaclesNestedObstacle) {
  const gfx::Rect area(0, 0, 400, 400);
  EXPECT_EQ(gfx::Rect(9, 16, 1, 1),
            CallAvoidObstaclesInternal(
                area, {gfx::Rect(15, 15, 5, 5), gfx::Rect(10, 10, 15, 15)},
                gfx::Rect(16, 16, 1, 1)));
}

TEST_F(CollisionDetectionUtilsLogicTest, AvoidObstaclesAvoidsTwoObstacles) {
  const gfx::Rect area(0, 0, 400, 400);
  const std::vector<gfx::Rect> obstacles = {gfx::Rect(4, 1, 4, 5),
                                            gfx::Rect(2, 4, 4, 5)};

  // Test a 2x2 window in the intersection between the obstacles.
  EXPECT_EQ(gfx::Rect(2, 2, 2, 2),
            CallAvoidObstaclesInternal(area, obstacles, gfx::Rect(4, 4, 2, 2)));
  // Test a 2x2 window in the lower obstacle.
  EXPECT_EQ(gfx::Rect(0, 7, 2, 2),
            CallAvoidObstaclesInternal(area, obstacles, gfx::Rect(2, 7, 2, 2)));
  // Test a 2x2 window in the upper obstacle.
  EXPECT_EQ(gfx::Rect(2, 1, 2, 2),
            CallAvoidObstaclesInternal(area, obstacles, gfx::Rect(4, 1, 2, 2)));
}

TEST_F(CollisionDetectionUtilsLogicTest, AvoidObstaclesAvoidsThreeObstacles) {
  const gfx::Rect area(0, 0, 400, 400);
  const std::vector<gfx::Rect> obstacles = {
      gfx::Rect(4, 1, 4, 5), gfx::Rect(2, 4, 4, 5), gfx::Rect(2, 1, 3, 4)};

  // Test a 2x2 window intersecting the top two obstacles.
  EXPECT_EQ(gfx::Rect(0, 2, 2, 2),
            CallAvoidObstaclesInternal(area, obstacles, gfx::Rect(3, 2, 2, 2)));
  // Test a 2x2 window intersecting all three obstacles.
  EXPECT_EQ(gfx::Rect(0, 3, 2, 2),
            CallAvoidObstaclesInternal(area, obstacles, gfx::Rect(3, 3, 2, 2)));
}

TEST_F(CollisionDetectionUtilsLogicTest,
       AvoidObstaclesDoesNotPositionBoundsOutsideOfWorkArea) {
  // Position the bounds such that moving it the least distance to stop
  // intersecting |obstacle| would put it outside of |area|. It should go
  // instead to the position of second least distance, which would be below
  // |obstacle|.
  const gfx::Rect area(0, 0, 400, 400);
  const gfx::Rect obstacle(50, 0, 100, 100);
  const gfx::Rect bounds(25, 0, 100, 100);
  EXPECT_EQ(gfx::Rect(25, 100, 100, 100),
            CallAvoidObstaclesInternal(area, {obstacle}, bounds));
}

TEST_F(CollisionDetectionUtilsLogicTest,
       AvoidObstaclesPositionsBoundsWithLeastDisplacement) {
  const gfx::Rect area(0, 0, 400, 400);
  const gfx::Rect obstacle(200, 200, 100, 100);

  // Intersecting slightly on the left.
  EXPECT_EQ(gfx::Rect(100, 200, 100, 100),
            CallAvoidObstaclesInternal(area, {obstacle},
                                       gfx::Rect(150, 200, 100, 100)));

  // Intersecting slightly on the right.
  EXPECT_EQ(gfx::Rect(300, 200, 100, 100),
            CallAvoidObstaclesInternal(area, {obstacle},
                                       gfx::Rect(250, 200, 100, 100)));

  // Intersecting slightly on the bottom.
  EXPECT_EQ(gfx::Rect(200, 300, 100, 100),
            CallAvoidObstaclesInternal(area, {obstacle},
                                       gfx::Rect(200, 250, 100, 100)));

  // Intersecting slightly on the top.
  EXPECT_EQ(gfx::Rect(200, 100, 100, 100),
            CallAvoidObstaclesInternal(area, {obstacle},
                                       gfx::Rect(200, 150, 100, 100)));
}

}  // namespace ash
