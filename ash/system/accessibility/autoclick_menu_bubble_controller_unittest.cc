// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/autoclick_menu_bubble_controller.h"

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/autoclick/autoclick_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/system/accessibility/autoclick_scroll_bubble_controller.h"
#include "ash/test/ash_test_base.h"
#include "base/command_line.h"

namespace ash {

namespace {

// A buffer for checking whether the menu view is near this region of the
// screen. This buffer allows for things like padding and the shelf size,
// but is still smaller than half the screen size, so that we can check the
// general corner in which the menu is displayed.
const int kMenuViewBoundsBuffer = 100;

// The buffers in dips around a scroll point where the scroll menu is shown.
// This should be slightly larger than kScrollPointBufferDips and
// kScrollRectBufferDips respectively from AutoclickScrollBubbleController to
// allow for some wiggle room.
const int kScrollViewBoundsBuffer = 26;
const int kScrollViewBoundsRectBuffer = 18;

ui::GestureEvent CreateTapEvent() {
  return ui::GestureEvent(0, 0, 0, base::TimeTicks(),
                          ui::GestureEventDetails(ui::ET_GESTURE_TAP));
}

}  // namespace

class AutoclickMenuBubbleControllerTest : public AshTestBase {
 public:
  AutoclickMenuBubbleControllerTest() = default;
  ~AutoclickMenuBubbleControllerTest() override = default;

  // testing::Test:
  void SetUp() override {
    AshTestBase::SetUp();
    Shell::Get()->accessibility_controller()->SetAutoclickEnabled(true);
  }

  AutoclickMenuBubbleController* GetBubbleController() {
    return Shell::Get()
        ->autoclick_controller()
        ->GetMenuBubbleControllerForTesting();
  }

  AutoclickMenuView* GetMenuView() {
    return GetBubbleController() ? GetBubbleController()->menu_view_ : nullptr;
  }

  views::View* GetMenuButton(AutoclickMenuView::ButtonId view_id) {
    AutoclickMenuView* menu_view = GetMenuView();
    if (!menu_view)
      return nullptr;
    return menu_view->GetViewByID(static_cast<int>(view_id));
  }

  gfx::Rect GetMenuViewBounds() {
    return GetBubbleController()
               ? GetBubbleController()->menu_view_->GetBoundsInScreen()
               : gfx::Rect(-kMenuViewBoundsBuffer, -kMenuViewBoundsBuffer);
  }

  AutoclickScrollView* GetScrollView() {
    return GetBubbleController()->scroll_bubble_controller_
               ? GetBubbleController()->scroll_bubble_controller_->scroll_view_
               : nullptr;
  }

  views::View* GetScrollButton(AutoclickScrollView::ButtonId view_id) {
    AutoclickScrollView* scroll_view = GetScrollView();
    if (!scroll_view)
      return nullptr;
    return scroll_view->GetViewByID(static_cast<int>(view_id));
  }

  gfx::Rect GetScrollViewBounds() {
    return GetBubbleController()->scroll_bubble_controller_
               ? GetBubbleController()
                     ->scroll_bubble_controller_->scroll_view_
                     ->GetBoundsInScreen()
               : gfx::Rect(-kMenuViewBoundsBuffer, -kMenuViewBoundsBuffer);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AutoclickMenuBubbleControllerTest);
};

TEST_F(AutoclickMenuBubbleControllerTest, ExistsOnlyWhenAutoclickIsRunning) {
  // Cycle a few times to ensure there are no crashes when toggling the feature.
  for (int i = 0; i < 2; i++) {
    EXPECT_TRUE(GetBubbleController());
    EXPECT_TRUE(GetMenuView());
    Shell::Get()->autoclick_controller()->SetEnabled(
        false, false /* do not show dialog */);
    EXPECT_FALSE(GetBubbleController());
    Shell::Get()->autoclick_controller()->SetEnabled(
        true, false /* do not show dialog */);
  }
}

TEST_F(AutoclickMenuBubbleControllerTest, CanSelectAutoclickTypeFromBubble) {
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  // Set to a different event type than the first event in kTestCases.
  controller->SetAutoclickEventType(AutoclickEventType::kRightClick);

  const struct {
    AutoclickMenuView::ButtonId view_id;
    AutoclickEventType expected_event_type;
  } kTestCases[] = {
      {AutoclickMenuView::ButtonId::kLeftClick, AutoclickEventType::kLeftClick},
      {AutoclickMenuView::ButtonId::kRightClick,
       AutoclickEventType::kRightClick},
      {AutoclickMenuView::ButtonId::kDoubleClick,
       AutoclickEventType::kDoubleClick},
      {AutoclickMenuView::ButtonId::kDragAndDrop,
       AutoclickEventType::kDragAndDrop},
      {AutoclickMenuView::ButtonId::kScroll, AutoclickEventType::kScroll},
      {AutoclickMenuView::ButtonId::kPause, AutoclickEventType::kNoAction},
  };

  for (const auto& test : kTestCases) {
    // Find the autoclick action button for this test case.
    views::View* button = GetMenuButton(test.view_id);
    ASSERT_TRUE(button) << "No view for id " << static_cast<int>(test.view_id);

    // Tap the action button.
    ui::GestureEvent event = CreateTapEvent();
    button->OnGestureEvent(&event);

    // Pref change happened.
    EXPECT_EQ(test.expected_event_type, controller->GetAutoclickEventType());
  }
}

TEST_F(AutoclickMenuBubbleControllerTest, UnpausesWhenPauseAlreadySelected) {
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  views::View* pause_button =
      GetMenuButton(AutoclickMenuView::ButtonId::kPause);
  ui::GestureEvent event = CreateTapEvent();

  const struct {
    AutoclickEventType event_type;
  } kTestCases[]{
      {AutoclickEventType::kRightClick},  {AutoclickEventType::kLeftClick},
      {AutoclickEventType::kDoubleClick}, {AutoclickEventType::kDragAndDrop},
      {AutoclickEventType::kScroll},
  };

  for (const auto& test : kTestCases) {
    controller->SetAutoclickEventType(test.event_type);

    // First tap pauses.
    pause_button->OnGestureEvent(&event);
    EXPECT_EQ(AutoclickEventType::kNoAction,
              controller->GetAutoclickEventType());

    // Second tap unpauses and reverts to previous state.
    pause_button->OnGestureEvent(&event);
    EXPECT_EQ(test.event_type, controller->GetAutoclickEventType());
  }
}

TEST_F(AutoclickMenuBubbleControllerTest, CanChangePosition) {
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();

  // Set to a known position for than the first event in kTestCases.
  controller->SetAutoclickMenuPosition(AutoclickMenuPosition::kTopRight);

  // Get the full root window bounds to test the position.
  gfx::Rect window_bounds = Shell::GetPrimaryRootWindow()->bounds();

  // Test cases rotate clockwise.
  const struct {
    gfx::Point expected_location;
    AutoclickMenuPosition expected_position;
  } kTestCases[] = {
      {gfx::Point(window_bounds.width(), window_bounds.height()),
       AutoclickMenuPosition::kBottomRight},
      {gfx::Point(0, window_bounds.height()),
       AutoclickMenuPosition::kBottomLeft},
      {gfx::Point(0, 0), AutoclickMenuPosition::kTopLeft},
      {gfx::Point(window_bounds.width(), 0), AutoclickMenuPosition::kTopRight},
  };

  // Find the autoclick menu position button.
  views::View* button = GetMenuButton(AutoclickMenuView::ButtonId::kPosition);
  ASSERT_TRUE(button) << "No position button found.";

  // Loop through all positions twice.
  for (int i = 0; i < 2; i++) {
    for (const auto& test : kTestCases) {
      // Tap the position button.
      ui::GestureEvent event = CreateTapEvent();
      button->OnGestureEvent(&event);

      // Pref change happened.
      EXPECT_EQ(test.expected_position, controller->GetAutoclickMenuPosition());

      // Menu is in generally the correct screen location.
      EXPECT_LT(
          GetMenuViewBounds().ManhattanDistanceToPoint(test.expected_location),
          kMenuViewBoundsBuffer);
    }
  }
}

TEST_F(AutoclickMenuBubbleControllerTest, DefaultChangesWithTextDirection) {
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  gfx::Rect window_bounds = Shell::GetPrimaryRootWindow()->bounds();

  // RTL should position the menu on the bottom left.
  base::i18n::SetRTLForTesting(true);
  // Force a layout.
  controller->UpdateAutoclickMenuBoundsIfNeeded();
  EXPECT_LT(
      GetMenuViewBounds().ManhattanDistanceToPoint(window_bounds.bottom_left()),
      kMenuViewBoundsBuffer);

  // LTR should position the menu on the bottom right.
  base::i18n::SetRTLForTesting(false);
  // Force a layout.
  controller->UpdateAutoclickMenuBoundsIfNeeded();
  EXPECT_LT(GetMenuViewBounds().ManhattanDistanceToPoint(
                window_bounds.bottom_right()),
            kMenuViewBoundsBuffer);
}

TEST_F(AutoclickMenuBubbleControllerTest, ScrollBubbleShowsAndCloses) {
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  controller->SetAutoclickEventType(AutoclickEventType::kLeftClick);
  // No scroll view yet.
  EXPECT_FALSE(GetScrollView());

  // Scroll type should cause the scroll view to be shown.
  controller->SetAutoclickEventType(AutoclickEventType::kScroll);
  EXPECT_TRUE(GetScrollView());

  // Clicking the scroll close button resets to left click.
  views::View* close_button =
      GetScrollButton(AutoclickScrollView::ButtonId::kCloseScroll);
  ui::GestureEvent event = CreateTapEvent();
  close_button->OnGestureEvent(&event);
  EXPECT_FALSE(GetScrollView());
  EXPECT_EQ(AutoclickEventType::kLeftClick,
            controller->GetAutoclickEventType());
}

TEST_F(AutoclickMenuBubbleControllerTest, ScrollBubbleDefaultPositioning) {
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  controller->SetAutoclickEventType(AutoclickEventType::kScroll);

  const struct { bool is_RTL; } kTestCases[] = {{true}, {false}};
  for (auto& test : kTestCases) {
    // These positions should be relative to the corners of the screen
    // whether we are in RTL or LTR.
    base::i18n::SetRTLForTesting(test.is_RTL);

    // When the menu is in the top right, the scroll view should be directly
    // under it and along the right side of the screen.
    controller->SetAutoclickMenuPosition(AutoclickMenuPosition::kTopRight);
    EXPECT_LT(GetScrollViewBounds().ManhattanDistanceToPoint(
                  GetMenuViewBounds().bottom_center()),
              kMenuViewBoundsBuffer);
    EXPECT_EQ(GetScrollViewBounds().right(), GetMenuViewBounds().right());

    // When the menu is in the bottom right, the scroll view is directly above
    // it and along the right side of the screen.
    controller->SetAutoclickMenuPosition(AutoclickMenuPosition::kBottomRight);
    EXPECT_LT(GetScrollViewBounds().ManhattanDistanceToPoint(
                  GetMenuViewBounds().top_center()),
              kMenuViewBoundsBuffer);
    EXPECT_EQ(GetScrollViewBounds().right(), GetMenuViewBounds().right());

    // When the menu is on the bottom left, the scroll view is directly above it
    // and along the left side of the screen.
    controller->SetAutoclickMenuPosition(AutoclickMenuPosition::kBottomLeft);
    EXPECT_LT(GetScrollViewBounds().ManhattanDistanceToPoint(
                  GetMenuViewBounds().top_center()),
              kMenuViewBoundsBuffer);
    EXPECT_EQ(GetScrollViewBounds().x(), GetMenuViewBounds().x());

    // When the menu is on the top left, the scroll view is directly below it
    // and along the left side of the screen.
    controller->SetAutoclickMenuPosition(AutoclickMenuPosition::kTopLeft);
    EXPECT_LT(GetScrollViewBounds().ManhattanDistanceToPoint(
                  GetMenuViewBounds().bottom_center()),
              kMenuViewBoundsBuffer);
    EXPECT_EQ(GetScrollViewBounds().x(), GetMenuViewBounds().x());
  }
}

TEST_F(AutoclickMenuBubbleControllerTest,
       ScrollBubbleManualPositioningLargeScrollBounds) {
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  controller->SetAutoclickEventType(AutoclickEventType::kScroll);

  // With large scrollable bounds, the scroll bubble should just be positioned
  // near the scroll point. Try with high density bounds and LTR/RTL languages.
  const struct {
    bool is_RTL;
    gfx::Rect scroll_bounds;
    std::string display_spec;
  } kTestCases[] = {
      {true, gfx::Rect(0, 0, 1000, 800), "1000x800"},
      {false, gfx::Rect(0, 0, 1000, 800), "1000x800"},
      {false, gfx::Rect(100, 100, 800, 600), "1000x800"},
      {true, gfx::Rect(200, 0, 600, 600), "1000x800"},
      {true, gfx::Rect(0, 0, 1000, 800), "2000x1600*2"},
      {false, gfx::Rect(0, 0, 1000, 800), "2000x1600*2"},
  };
  for (auto& test : kTestCases) {
    UpdateDisplay(test.display_spec);
    base::i18n::SetRTLForTesting(test.is_RTL);
    gfx::Rect scroll_bounds = test.scroll_bounds;
    controller->SetAutoclickMenuPosition(AutoclickMenuPosition::kTopRight);

    // Start with a point no where near the autoclick menu.
    gfx::Point point = gfx::Point(400, 400);
    GetBubbleController()->SetScrollPosition(scroll_bounds, point);

    // Just below the point in the Y axis.
    EXPECT_LT(abs(GetScrollViewBounds().y() - point.y()),
              kScrollViewBoundsBuffer);

    // Just off to the side in the X axis.
    if (test.is_RTL) {
      EXPECT_LT(abs(GetScrollViewBounds().right() - point.x()),
                kScrollViewBoundsBuffer);
    } else {
      EXPECT_LT(abs(GetScrollViewBounds().x() - point.x()),
                kScrollViewBoundsBuffer);
    }

    // Moving the autoclick bubble doesn't impact the scroll bubble once it
    // has been manually set.
    gfx::Rect bubble_bounds = GetScrollViewBounds();
    controller->SetAutoclickMenuPosition(AutoclickMenuPosition::kBottomRight);
    EXPECT_EQ(bubble_bounds, GetScrollViewBounds());

    // If we position it by the edge of the screen, it should stay on-screen,
    // regardless of LTR vs RTL.
    point = gfx::Point(0, 0);
    GetBubbleController()->SetScrollPosition(scroll_bounds, point);
    EXPECT_LT(abs(GetScrollViewBounds().x() - point.x()),
              kScrollViewBoundsBuffer);
    EXPECT_LT(abs(GetScrollViewBounds().y() - point.y()),
              kScrollViewBoundsBuffer);

    point = gfx::Point(1000, 400);
    GetBubbleController()->SetScrollPosition(scroll_bounds, point);
    EXPECT_LT(abs(GetScrollViewBounds().right() - point.x()),
              kScrollViewBoundsBuffer);
    EXPECT_LT(abs(GetScrollViewBounds().y() - point.y()),
              kScrollViewBoundsBuffer);

    // If it's too close to the bottom, it will be shown above the point.
    point = gfx::Point(500, 700);
    GetBubbleController()->SetScrollPosition(scroll_bounds, point);
    EXPECT_LT(abs(GetScrollViewBounds().bottom() - point.y()),
              kScrollViewBoundsBuffer);
  }
}

TEST_F(AutoclickMenuBubbleControllerTest,
       ScrollBubbleManualPositioningSmallScrollBounds) {
  UpdateDisplay("1200x1000");
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  controller->SetAutoclickEventType(AutoclickEventType::kScroll);

  const struct {
    bool is_RTL;
    gfx::Rect scroll_bounds;
    gfx::Point scroll_point;
    bool expect_bounds_on_right;
    bool expect_bounds_on_left;
    bool expect_bounds_on_top;
    bool expect_bounds_on_bottom;
  } kTestCases[] = {
      // Small centered bounds, point closest to the right side.
      {true, gfx::Rect(400, 350, 300, 300), gfx::Point(555, 500),
       true /* on right */, false, false, false},
      {false, gfx::Rect(400, 350, 300, 300), gfx::Point(555, 500),
       true /* on right */, false, false, false},
      {false, gfx::Rect(400, 350, 300, 300), gfx::Point(650, 400),
       true /* on right */, false, false, false},

      // Small centered bounds, point closest to the left side.
      {true, gfx::Rect(400, 350, 300, 300), gfx::Point(545, 500), false,
       true /* on left */, false, false},
      {false, gfx::Rect(400, 350, 300, 300), gfx::Point(545, 500), false,
       true /* on left */, false, false},
      {false, gfx::Rect(400, 350, 300, 300), gfx::Point(410, 400), false,
       true /* on left */, false, false},

      // Point closest to the top of the bounds.
      {true, gfx::Rect(400, 350, 300, 300), gfx::Point(550, 400), false, false,
       true /* on top */, false},
      {false, gfx::Rect(400, 350, 300, 300), gfx::Point(550, 400), false, false,
       true /* on top */, false},
      {false, gfx::Rect(400, 350, 300, 300), gfx::Point(402, 351), false, false,
       true /* on top */, false},

      // Point closest to the bottom of the bounds.
      {true, gfx::Rect(400, 350, 300, 300), gfx::Point(550, 550), false, false,
       false, true /* on bottom */},
      {false, gfx::Rect(400, 350, 300, 300), gfx::Point(550, 550), false, false,
       false, true /* on bottom */},
      {false, gfx::Rect(400, 350, 300, 300), gfx::Point(450, 649), false, false,
       false, true /* on bottom */},

      // These bounds only have space on the right and bottom. Even points
      // close to the top left get mapped to the right or bottom.
      {false, gfx::Rect(100, 100, 300, 300), gfx::Point(130, 120),
       true /* on right */, false, false, false},
      {true, gfx::Rect(100, 100, 300, 300), gfx::Point(130, 120),
       true /* on right */, false, false, false},
      {false, gfx::Rect(100, 100, 300, 300), gfx::Point(120, 130), false, false,
       false, true /* on bottom */},

      // These bounds only have space on the top and left. Even points
      // close to the bottom right get mapped to the top or left.
      {false, gfx::Rect(900, 600, 300, 300), gfx::Point(1075, 800), false,
       true /* on left */, false, false},
      {false, gfx::Rect(900, 600, 300, 300), gfx::Point(1075, 800), false,
       true /* on left */, false, false},
      {false, gfx::Rect(900, 600, 300, 300), gfx::Point(1100, 775), false,
       false, true /* on top */, false},

      // These bounds have space above/below, but not left/right.
      {false, gfx::Rect(400, 100, 300, 800), gfx::Point(525, 110), false,
       true /* on left */, false, false},
      {false, gfx::Rect(400, 100, 300, 800), gfx::Point(525, 845), false,
       true /* on left */, false, false},
      {false, gfx::Rect(400, 100, 300, 800), gfx::Point(575, 845),
       true /* on right */, false, false, false},

      // These bounds have space left/right, but not above/below.
      {false, gfx::Rect(100, 350, 1000, 300), gfx::Point(200, 450), false,
       false, true /* on top */, false},
      {false, gfx::Rect(100, 350, 1000, 300), gfx::Point(200, 550), false,
       false, false, true /* on bottom */},
      {false, gfx::Rect(100, 350, 1000, 300), gfx::Point(1000, 550), false,
       false, false, true /* on bottom */},
  };
  for (auto& test : kTestCases) {
    base::i18n::SetRTLForTesting(test.is_RTL);
    gfx::Rect scroll_bounds = test.scroll_bounds;
    gfx::Point scroll_point = test.scroll_point;
    GetBubbleController()->SetScrollPosition(scroll_bounds, scroll_point);

    if (test.expect_bounds_on_right) {
      // The scroll view should be on the right of the bounds and centered
      // vertically on the scroll point.
      EXPECT_LT(abs(GetScrollViewBounds().y() +
                    GetScrollViewBounds().height() / 2 - scroll_point.y()),
                kScrollViewBoundsRectBuffer);
      EXPECT_LT(GetScrollViewBounds().x() - scroll_bounds.right(),
                kScrollViewBoundsRectBuffer);
      EXPECT_GT(GetScrollViewBounds().x() - scroll_bounds.right(), 0);
    } else if (test.expect_bounds_on_left) {
      // The scroll view should be on the left of the bounds and centered
      // vertically on the scroll point.
      EXPECT_LT(abs(GetScrollViewBounds().y() +
                    GetScrollViewBounds().height() / 2 - scroll_point.y()),
                kScrollViewBoundsRectBuffer);
      EXPECT_LT(scroll_bounds.x() - GetScrollViewBounds().right(),
                kScrollViewBoundsRectBuffer);
      EXPECT_GT(scroll_bounds.x() - GetScrollViewBounds().right(), 0);
    } else if (test.expect_bounds_on_top) {
      // The scroll view should be on the top of the bounds and centered
      // horizontally on the scroll point.
      EXPECT_LT(abs(GetScrollViewBounds().x() +
                    GetScrollViewBounds().width() / 2 - scroll_point.x()),
                kScrollViewBoundsRectBuffer);
      EXPECT_LT(scroll_bounds.y() - GetScrollViewBounds().bottom(),
                kScrollViewBoundsRectBuffer);
      EXPECT_GT(scroll_bounds.y() - GetScrollViewBounds().bottom(), 0);
    } else if (test.expect_bounds_on_bottom) {
      // The scroll view should be on the bottom of the bounds and centered
      // horizontally on the scroll point.
      EXPECT_LT(abs(GetScrollViewBounds().x() +
                    GetScrollViewBounds().width() / 2 - scroll_point.x()),
                kScrollViewBoundsRectBuffer);
      EXPECT_LT(GetScrollViewBounds().y() - scroll_bounds.bottom(),
                kScrollViewBoundsRectBuffer);
      EXPECT_GT(GetScrollViewBounds().y() - scroll_bounds.bottom(), -1);
    }
  }
}

}  // namespace ash
