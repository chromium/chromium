// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_highlight_controller.h"

#include "ash/public/cpp/ash_features.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desk_mini_view.h"
#include "ash/wm/desks/desks_bar_view.h"
#include "ash/wm/desks/desks_test_util.h"
#include "ash/wm/desks/new_desk_button.h"
#include "ash/wm/overview/overview_constants.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/overview/overview_item_view.h"
#include "ash/wm/overview/overview_test_util.h"
#include "ash/wm/overview/scoped_overview_transform_window.h"
#include "ash/wm/window_util.h"
#include "base/test/scoped_feature_list.h"
#include "ui/aura/window.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/widget/widget.h"

namespace ash {

class OverviewHighlightControllerTest : public AshTestBase {
 public:
  OverviewHighlightControllerTest() = default;
  ~OverviewHighlightControllerTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    ScopedOverviewTransformWindow::SetImmediateCloseForTests();
  }

  OverviewHighlightController* GetHighlightController() {
    return GetOverviewSession()->highlight_controller();
  }

  // Press the key repeatedly until a window is highlighted, i.e. ignoring any
  // desk items.
  void SendKeyUntilOverviewItemIsHighlighted(ui::KeyboardCode key) {
    do {
      SendKey(key);
    } while (!GetOverviewHighlightedWindow());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(OverviewHighlightControllerTest);
};

// Tests traversing some windows in overview mode with the tab key.
TEST_F(OverviewHighlightControllerTest, BasicTabKeyNavigation) {
  std::unique_ptr<aura::Window> window2(CreateTestWindow());
  std::unique_ptr<aura::Window> window1(CreateTestWindow());

  ToggleOverview();
  const std::vector<std::unique_ptr<OverviewItem>>& overview_windows =
      GetOverviewItemsForRoot(0);
  SendKeyUntilOverviewItemIsHighlighted(ui::VKEY_TAB);
  EXPECT_EQ(overview_windows[0]->GetWindow(), GetOverviewHighlightedWindow());
  SendKeyUntilOverviewItemIsHighlighted(ui::VKEY_TAB);
  EXPECT_EQ(overview_windows[1]->GetWindow(), GetOverviewHighlightedWindow());
  SendKeyUntilOverviewItemIsHighlighted(ui::VKEY_TAB);
  EXPECT_EQ(overview_windows[0]->GetWindow(), GetOverviewHighlightedWindow());
}

// Tests that pressing Ctrl+W while a window is selected in overview closes it.
TEST_F(OverviewHighlightControllerTest, CloseWindowWithKey) {
  std::unique_ptr<views::Widget> widget(CreateTestWidget());
  ToggleOverview();

  SendKeyUntilOverviewItemIsHighlighted(ui::VKEY_RIGHT);
  EXPECT_EQ(widget->GetNativeWindow(), GetOverviewHighlightedWindow());
  SendKey(ui::VKEY_W, ui::EF_CONTROL_DOWN);
  EXPECT_TRUE(widget->IsClosed());
}

// Tests traversing some windows in overview mode with the arrow keys in every
// possible direction.
TEST_F(OverviewHighlightControllerTest, BasicArrowKeyNavigation) {
  const size_t test_windows = 9;
  UpdateDisplay("800x600");
  std::vector<std::unique_ptr<aura::Window>> windows;
  for (size_t i = test_windows; i > 0; --i) {
    windows.push_back(
        std::unique_ptr<aura::Window>(CreateTestWindowInShellWithId(i)));
  }

  ui::KeyboardCode arrow_keys[] = {ui::VKEY_RIGHT, ui::VKEY_DOWN, ui::VKEY_LEFT,
                                   ui::VKEY_UP};
  // The rows contain variable number of items making vertical navigation not
  // feasible. [Down] is equivalent to [Right] and [Up] is equivalent to [Left].
  int index_path_for_direction[][test_windows + 1] = {
      {1, 2, 3, 4, 5, 6, 7, 8, 9, 1},  // Right
      {1, 2, 3, 4, 5, 6, 7, 8, 9, 1},  // Down (same as Right)
      {9, 8, 7, 6, 5, 4, 3, 2, 1, 9},  // Left
      {9, 8, 7, 6, 5, 4, 3, 2, 1, 9}   // Up (same as Left)
  };

  for (size_t key_index = 0; key_index < base::size(arrow_keys); ++key_index) {
    ToggleOverview();
    const std::vector<std::unique_ptr<OverviewItem>>& overview_windows =
        GetOverviewItemsForRoot(0);
    for (size_t i = 0; i < test_windows + 1; ++i) {
      SendKeyUntilOverviewItemIsHighlighted(arrow_keys[key_index]);
      // TODO(flackr): Add a more readable error message by constructing a
      // string from the window IDs.
      const int index = index_path_for_direction[key_index][i];
      EXPECT_EQ(GetOverviewHighlightedWindow()->id(),
                overview_windows[index - 1]->GetWindow()->id());
    }
    ToggleOverview();
  }
}

// Tests that when an item is removed while highlighted, the highlight
// disappears, and when we tab again we pick up where we left off.
TEST_F(OverviewHighlightControllerTest, ItemClosed) {
  auto widget1 = CreateTestWidget();
  auto widget2 = CreateTestWidget();
  auto widget3 = CreateTestWidget();
  ToggleOverview();

  SendKeyUntilOverviewItemIsHighlighted(ui::VKEY_TAB);
  SendKeyUntilOverviewItemIsHighlighted(ui::VKEY_TAB);
  EXPECT_EQ(widget2->GetNativeWindow(), GetOverviewHighlightedWindow());

  // Remove |widget2| by closing it with ctrl + W. Test that the highlight
  // becomes invisible.
  SendKey(ui::VKEY_W, ui::EF_CONTROL_DOWN);
  EXPECT_TRUE(widget2->IsClosed());
  widget2.reset();
  EXPECT_FALSE(GetHighlightController()->IsFocusHighlightVisible());

  // Tests that on pressing tab, the highlight becomes visible and we highlight
  // the window that comes after the deleted one.
  SendKeyUntilOverviewItemIsHighlighted(ui::VKEY_TAB);
  EXPECT_TRUE(GetHighlightController()->IsFocusHighlightVisible());
  EXPECT_EQ(widget1->GetNativeWindow(), GetOverviewHighlightedWindow());
}

// Tests basic selection across multiple monitors.
TEST_F(OverviewHighlightControllerTest, BasicMultiMonitorArrowKeyNavigation) {
  UpdateDisplay("400x400,400x400");
  const gfx::Rect bounds1(100, 100);
  const gfx::Rect bounds2(450, 0, 100, 100);
  std::unique_ptr<aura::Window> window4(CreateTestWindow(bounds2));
  std::unique_ptr<aura::Window> window3(CreateTestWindow(bounds2));
  std::unique_ptr<aura::Window> window2(CreateTestWindow(bounds1));
  std::unique_ptr<aura::Window> window1(CreateTestWindow(bounds1));

  ToggleOverview();

  const std::vector<std::unique_ptr<OverviewItem>>& overview_root1 =
      GetOverviewItemsForRoot(0);
  const std::vector<std::unique_ptr<OverviewItem>>& overview_root2 =
      GetOverviewItemsForRoot(1);
  SendKeyUntilOverviewItemIsHighlighted(ui::VKEY_RIGHT);
  EXPECT_EQ(GetOverviewHighlightedWindow(), overview_root1[0]->GetWindow());
  SendKeyUntilOverviewItemIsHighlighted(ui::VKEY_RIGHT);
  EXPECT_EQ(GetOverviewHighlightedWindow(), overview_root1[1]->GetWindow());
  SendKeyUntilOverviewItemIsHighlighted(ui::VKEY_RIGHT);
  EXPECT_EQ(GetOverviewHighlightedWindow(), overview_root2[0]->GetWindow());
  SendKeyUntilOverviewItemIsHighlighted(ui::VKEY_RIGHT);
  EXPECT_EQ(GetOverviewHighlightedWindow(), overview_root2[1]->GetWindow());
}

// Tests first monitor when display order doesn't match left to right screen
// positions.
TEST_F(OverviewHighlightControllerTest, MultiMonitorReversedOrder) {
  UpdateDisplay("400x400,400x400");
  Shell::Get()->display_manager()->SetLayoutForCurrentDisplays(
      display::test::CreateDisplayLayout(display_manager(),
                                         display::DisplayPlacement::LEFT, 0));
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  std::unique_ptr<aura::Window> window2(CreateTestWindow(gfx::Rect(100, 100)));
  std::unique_ptr<aura::Window> window1(
      CreateTestWindow(gfx::Rect(-350, 0, 100, 100)));
  EXPECT_EQ(root_windows[1], window1->GetRootWindow());
  EXPECT_EQ(root_windows[0], window2->GetRootWindow());

  ToggleOverview();

  // Coming from the left to right, we should select window1 first being on the
  // display to the left.
  SendKeyUntilOverviewItemIsHighlighted(ui::VKEY_RIGHT);
  EXPECT_EQ(GetOverviewHighlightedWindow(), window1.get());

  // Exit and reenter overview.
  ToggleOverview();
  ToggleOverview();

  // Coming from right to left, we should select window2 first being on the
  // display on the right.
  SendKeyUntilOverviewItemIsHighlighted(ui::VKEY_LEFT);
  EXPECT_EQ(GetOverviewHighlightedWindow(), window2.get());
}

// Tests three monitors where the grid becomes empty on one of the monitors.
TEST_F(OverviewHighlightControllerTest, ThreeMonitor) {
  UpdateDisplay("400x400,400x400,400x400");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  std::unique_ptr<aura::Window> window3(
      CreateTestWindow(gfx::Rect(800, 0, 100, 100)));
  std::unique_ptr<aura::Window> window2(
      CreateTestWindow(gfx::Rect(400, 0, 100, 100)));
  std::unique_ptr<aura::Window> window1(CreateTestWindow(gfx::Rect(100, 100)));
  EXPECT_EQ(root_windows[0], window1->GetRootWindow());
  EXPECT_EQ(root_windows[1], window2->GetRootWindow());
  EXPECT_EQ(root_windows[2], window3->GetRootWindow());

  ToggleOverview();

  SendKeyUntilOverviewItemIsHighlighted(ui::VKEY_RIGHT);
  SendKeyUntilOverviewItemIsHighlighted(ui::VKEY_RIGHT);
  SendKeyUntilOverviewItemIsHighlighted(ui::VKEY_RIGHT);
  EXPECT_EQ(window3.get(), GetOverviewHighlightedWindow());

  // If the selected window is closed, then nothing should be selected.
  window3.reset();
  EXPECT_EQ(nullptr, GetOverviewHighlightedWindow());
  ToggleOverview();

  window3 = CreateTestWindow(gfx::Rect(800, 0, 100, 100));
  ToggleOverview();
  SendKeyUntilOverviewItemIsHighlighted(ui::VKEY_RIGHT);
  SendKeyUntilOverviewItemIsHighlighted(ui::VKEY_RIGHT);
  SendKeyUntilOverviewItemIsHighlighted(ui::VKEY_RIGHT);

  // If the window on the second display is removed, the selected window should
  // remain window3.
  EXPECT_EQ(window3.get(), GetOverviewHighlightedWindow());
  window2.reset();
  EXPECT_EQ(window3.get(), GetOverviewHighlightedWindow());
}

// Tests selecting a window in overview mode with the return key.
TEST_F(OverviewHighlightControllerTest, HighlightOverviewWindowWithReturnKey) {
  std::unique_ptr<aura::Window> window2(CreateTestWindow());
  std::unique_ptr<aura::Window> window1(CreateTestWindow());
  ToggleOverview();

  // Pressing the return key on an item that is not highlighted should not do
  // anything.
  SendKey(ui::VKEY_RETURN);
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());

  // Highlight the first window.
  ASSERT_TRUE(HighlightOverviewWindow(window1.get()));
  SendKey(ui::VKEY_RETURN);
  EXPECT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());
  EXPECT_TRUE(wm::IsActiveWindow(window1.get()));

  // Highlight the second window.
  ToggleOverview();
  ASSERT_TRUE(HighlightOverviewWindow(window2.get()));
  SendKey(ui::VKEY_RETURN);
  EXPECT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());
  EXPECT_TRUE(wm::IsActiveWindow(window2.get()));
}

// Tests that the location of the overview highlight is as expected while
// dragging an overview item.
TEST_F(OverviewHighlightControllerTest, HighlightLocationWhileDragging) {
  std::unique_ptr<aura::Window> window1(CreateTestWindow(gfx::Rect(200, 200)));
  std::unique_ptr<aura::Window> window2(CreateTestWindow(gfx::Rect(200, 200)));
  std::unique_ptr<aura::Window> window3(CreateTestWindow(gfx::Rect(200, 200)));

  ToggleOverview();

  // Tab once to show the highlight.
  SendKeyUntilOverviewItemIsHighlighted(ui::VKEY_TAB);
  EXPECT_EQ(window3.get(), GetOverviewHighlightedWindow());
  OverviewItem* item = GetOverviewItemForWindow(window3.get());

  // Tests that while dragging an item, tabbing does not change which item the
  // highlight is hovered over, but the highlight is hidden. Drag the item in a
  // way which does not enter splitview, or close overview.
  const gfx::PointF start_point = item->target_bounds().CenterPoint();
  const gfx::PointF end_point(20.f, 20.f);
  GetOverviewSession()->InitiateDrag(item, start_point,
                                     /*is_touch_dragging=*/true);
  GetOverviewSession()->Drag(item, end_point);
  SendKeyUntilOverviewItemIsHighlighted(ui::VKEY_TAB);
  EXPECT_EQ(window3.get(), GetOverviewHighlightedWindow());
  EXPECT_FALSE(GetHighlightController()->IsFocusHighlightVisible());

  // Tests that on releasing the item, the highlighted window remains the same.
  GetOverviewSession()->Drag(item, start_point);
  GetOverviewSession()->CompleteDrag(item, start_point);
  EXPECT_EQ(window3.get(), GetOverviewHighlightedWindow());
  EXPECT_TRUE(GetHighlightController()->IsFocusHighlightVisible());

  // Tests that on tabbing after releasing, the highlighted window is the next
  // one.
  SendKeyUntilOverviewItemIsHighlighted(ui::VKEY_TAB);
  EXPECT_EQ(window2.get(), GetOverviewHighlightedWindow());
}

// ----------------------------------------------------------------------------
// DesksOverviewHighlightControllerTest:

class DesksOverviewHighlightControllerTest
    : public OverviewHighlightControllerTest {
 public:
  DesksOverviewHighlightControllerTest() = default;
  ~DesksOverviewHighlightControllerTest() override = default;

  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kVirtualDesks);

    AshTestBase::SetUp();

    // All tests in this suite require the desks bar to be visible in overview,
    // which requires at least two desks.
    auto* desk_controller = DesksController::Get();
    desk_controller->NewDesk(DesksCreationRemovalSource::kButton);
    ASSERT_EQ(2u, desk_controller->desks().size());
  }

  OverviewHighlightController::OverviewHighlightableView* GetHighlightedView() {
    return OverviewHighlightController::TestApi(GetHighlightController())
        .GetHighlightView();
  }

  const DesksBarView* GetDesksBarViewForRoot(aura::Window* root_window) {
    OverviewGrid* grid =
        GetOverviewSession()->GetGridWithRootWindow(root_window);
    DCHECK(grid->IsDesksBarViewActive());
    return grid->desks_bar_view();
  }

  bool OverviewHighlightShown() {
    if (!Shell::Get()->overview_controller()->InOverviewSession())
      return false;

    OverviewHighlightController::TestApi test_api(GetHighlightController());
    return !!test_api.GetHighlightWidget();
  }

  // Checks to see if a view is completely covered by the overview highlight.
  bool CoveredByOverviewHighlight(views::View* view) {
    if (!OverviewHighlightShown())
      return false;

    const gfx::Rect highlight_bounds =
        OverviewHighlightController::TestApi(GetHighlightController())
            .GetHighlightBoundsInScreen();
    DCHECK(!highlight_bounds.IsEmpty());

    // The highlight bounds will be a bit smaller than the view it
    // highlights, because it is meant to highlight the visible area of the
    // view.
    const int tolerance = kOverviewMargin;
    const gfx::Rect view_bounds = view->GetBoundsInScreen();
    return highlight_bounds.ApproximatelyEqual(view_bounds, tolerance);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  DISALLOW_COPY_AND_ASSIGN(DesksOverviewHighlightControllerTest);
};

// Tests that we can tab through the desk mini views, new desk button and
// overview items in the correct order. Overview items will have the overview
// highlight shown when highlighted, but desks items will not.
TEST_F(DesksOverviewHighlightControllerTest, TabbingBasic) {
  std::unique_ptr<aura::Window> window1(CreateTestWindow(gfx::Rect(200, 200)));
  std::unique_ptr<aura::Window> window2(CreateTestWindow(gfx::Rect(200, 200)));

  ToggleOverview();
  const auto* desk_bar_view =
      GetDesksBarViewForRoot(Shell::GetPrimaryRootWindow());
  EXPECT_EQ(2u, desk_bar_view->mini_views().size());

  // Tests that the first highlighted item is the first mini view.
  SendKey(ui::VKEY_TAB);
  EXPECT_EQ(desk_bar_view->mini_views()[0].get(), GetHighlightedView());
  EXPECT_FALSE(OverviewHighlightShown());

  // Tests that after tabbing through the mini views, we highlight the new desk
  // button.
  SendKey(ui::VKEY_TAB);
  SendKey(ui::VKEY_TAB);
  EXPECT_EQ(desk_bar_view->new_desk_button(), GetHighlightedView());
  EXPECT_FALSE(OverviewHighlightShown());

  // Tests that the overview item gets highlighted after the new desk button.
  SendKey(ui::VKEY_TAB);
  auto* item2 = GetOverviewItemForWindow(window2.get());
  EXPECT_EQ(item2->overview_item_view(), GetHighlightedView());
  EXPECT_TRUE(OverviewHighlightShown());

  // Tests that after tabbing through the overview items, we go back to the
  // first mini view.
  SendKey(ui::VKEY_TAB);
  SendKey(ui::VKEY_TAB);
  EXPECT_EQ(desk_bar_view->mini_views()[0].get(), GetHighlightedView());
  EXPECT_FALSE(OverviewHighlightShown());
}

// Tests that we can reverse tab through the desk mini views, new desk button
// and overview items in the correct order.
TEST_F(DesksOverviewHighlightControllerTest, TabbingReverse) {
  std::unique_ptr<aura::Window> window1(CreateTestWindow(gfx::Rect(200, 200)));
  std::unique_ptr<aura::Window> window2(CreateTestWindow(gfx::Rect(200, 200)));

  ToggleOverview();
  const auto* desk_bar_view =
      GetDesksBarViewForRoot(Shell::GetPrimaryRootWindow());
  EXPECT_EQ(2u, desk_bar_view->mini_views().size());

  // Tests that the first highlighted item when reversing is the last overview
  // item.
  SendKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
  auto* item1 = GetOverviewItemForWindow(window1.get());
  EXPECT_EQ(item1->overview_item_view(), GetHighlightedView());

  // Tests that after reverse tabbing through the overview items, we highlight
  // the new desk button.
  SendKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
  SendKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
  EXPECT_EQ(desk_bar_view->new_desk_button(), GetHighlightedView());

  // Tests that after the new desk button comes the the mini views in reverse
  // order.
  SendKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
  EXPECT_EQ(desk_bar_view->mini_views()[1].get(), GetHighlightedView());
  SendKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
  EXPECT_EQ(desk_bar_view->mini_views()[0].get(), GetHighlightedView());

  // Tests that we return to the last overview item after reverse tabbing from
  // the first mini view.
  SendKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
  EXPECT_EQ(item1->overview_item_view(), GetHighlightedView());
}

// Tests that tabbing with desk items and multiple displays works as expected.
TEST_F(DesksOverviewHighlightControllerTest, TabbingMultiDisplay) {
  UpdateDisplay("600x400,600x400,600x400");
  std::vector<aura::Window*> roots = Shell::GetAllRootWindows();
  ASSERT_EQ(3u, roots.size());

  // Create two windows on the first display, and one each on the second and
  // third displays.
  std::unique_ptr<aura::Window> window1(CreateTestWindow(gfx::Rect(200, 200)));
  std::unique_ptr<aura::Window> window2(CreateTestWindow(gfx::Rect(200, 200)));
  std::unique_ptr<aura::Window> window3(
      CreateTestWindow(gfx::Rect(600, 0, 200, 200)));
  std::unique_ptr<aura::Window> window4(
      CreateTestWindow(gfx::Rect(1200, 0, 200, 200)));
  ASSERT_EQ(roots[0], window1->GetRootWindow());
  ASSERT_EQ(roots[0], window2->GetRootWindow());
  ASSERT_EQ(roots[1], window3->GetRootWindow());
  ASSERT_EQ(roots[2], window4->GetRootWindow());

  ToggleOverview();
  const auto* desk_bar_view1 = GetDesksBarViewForRoot(roots[0]);
  EXPECT_EQ(2u, desk_bar_view1->mini_views().size());

  // Tests that tabbing initially will go through the desk mini views, then
  // the new desk button on the first display.
  SendKey(ui::VKEY_TAB);
  EXPECT_EQ(desk_bar_view1->mini_views()[0].get(), GetHighlightedView());
  SendKey(ui::VKEY_TAB);
  EXPECT_EQ(desk_bar_view1->mini_views()[1].get(), GetHighlightedView());
  SendKey(ui::VKEY_TAB);
  EXPECT_EQ(desk_bar_view1->new_desk_button(), GetHighlightedView());

  // Tests that two more tabs, will highlight the two overview items on the
  // first display.
  SendKey(ui::VKEY_TAB);
  auto* item2 = GetOverviewItemForWindow(window2.get());
  EXPECT_EQ(item2->overview_item_view(), GetHighlightedView());
  SendKey(ui::VKEY_TAB);
  auto* item1 = GetOverviewItemForWindow(window1.get());
  EXPECT_EQ(item1->overview_item_view(), GetHighlightedView());

  // Tests that the next tab will bring us to the first mini view on the
  // second display.
  SendKey(ui::VKEY_TAB);
  const auto* desk_bar_view2 = GetDesksBarViewForRoot(roots[1]);
  EXPECT_EQ(desk_bar_view2->mini_views()[0].get(), GetHighlightedView());

  // Tab through all items on the second display.
  SendKey(ui::VKEY_TAB);
  SendKey(ui::VKEY_TAB);
  EXPECT_EQ(desk_bar_view2->new_desk_button(), GetHighlightedView());
  SendKey(ui::VKEY_TAB);
  auto* item3 = GetOverviewItemForWindow(window3.get());
  EXPECT_EQ(item3->overview_item_view(), GetHighlightedView());

  // Tests that after tabbing through the items on the second display, the
  // next tab will bring us to the first mini view on the third display.
  SendKey(ui::VKEY_TAB);
  const auto* desk_bar_view3 = GetDesksBarViewForRoot(roots[2]);
  EXPECT_EQ(desk_bar_view3->mini_views()[0].get(), GetHighlightedView());

  // Tab through all items on the third display.
  SendKey(ui::VKEY_TAB);
  SendKey(ui::VKEY_TAB);
  EXPECT_EQ(desk_bar_view3->new_desk_button(), GetHighlightedView());
  SendKey(ui::VKEY_TAB);
  auto* item4 = GetOverviewItemForWindow(window4.get());
  EXPECT_EQ(item4->overview_item_view(), GetHighlightedView());

  // Tests that after tabbing through the items on the third display, the next
  // tab will bring us to the first mini view on the first display.
  SendKey(ui::VKEY_TAB);
  EXPECT_EQ(desk_bar_view1->mini_views()[0].get(), GetHighlightedView());
}

// Tests that the location of the overview highlight is fully covering each
// views bounds.
TEST_F(DesksOverviewHighlightControllerTest,
       TabbingMultiDisplayHighlightLocation) {
  UpdateDisplay("600x400,600x400,600x400");
  std::vector<aura::Window*> roots = Shell::GetAllRootWindows();
  ASSERT_EQ(3u, roots.size());

  std::unique_ptr<aura::Window> window1(CreateTestWindow(gfx::Rect(200, 200)));
  std::unique_ptr<aura::Window> window2(
      CreateTestWindow(gfx::Rect(600, 0, 200, 200)));
  ASSERT_EQ(roots[0], window1->GetRootWindow());
  ASSERT_EQ(roots[1], window2->GetRootWindow());

  ToggleOverview();
  const auto* desk_bar_view1 = GetDesksBarViewForRoot(roots[0]);
  EXPECT_EQ(2u, desk_bar_view1->mini_views().size());
  EXPECT_FALSE(OverviewHighlightShown());

  SendKey(ui::VKEY_TAB);
  SendKey(ui::VKEY_TAB);
  EXPECT_FALSE(OverviewHighlightShown());

  SendKey(ui::VKEY_TAB);
  EXPECT_EQ(desk_bar_view1->new_desk_button(), GetHighlightedView());
  EXPECT_FALSE(OverviewHighlightShown());

  SendKey(ui::VKEY_TAB);
  auto* item1 = GetOverviewItemForWindow(window1.get());
  EXPECT_TRUE(CoveredByOverviewHighlight(item1->overview_item_view()));

  const auto* desk_bar_view2 = GetDesksBarViewForRoot(roots[1]);
  SendKey(ui::VKEY_TAB);
  EXPECT_FALSE(OverviewHighlightShown());
  SendKey(ui::VKEY_TAB);
  SendKey(ui::VKEY_TAB);
  EXPECT_EQ(desk_bar_view2->new_desk_button(), GetHighlightedView());
  EXPECT_FALSE(OverviewHighlightShown());
  SendKey(ui::VKEY_TAB);
  auto* item2 = GetOverviewItemForWindow(window2.get());
  EXPECT_TRUE(CoveredByOverviewHighlight(item2->overview_item_view()));

  const auto* desk_bar_view3 = GetDesksBarViewForRoot(roots[2]);
  SendKey(ui::VKEY_TAB);
  EXPECT_FALSE(OverviewHighlightShown());
  SendKey(ui::VKEY_TAB);
  SendKey(ui::VKEY_TAB);
  EXPECT_EQ(desk_bar_view3->new_desk_button(), GetHighlightedView());
  EXPECT_FALSE(OverviewHighlightShown());
}

TEST_F(DesksOverviewHighlightControllerTest,
       TabbingDisplayHighlightLocationAfterItemRemoval) {
  std::unique_ptr<views::Widget> widget3(CreateTestWidget());
  std::unique_ptr<aura::Window> window2(CreateTestWindow(gfx::Rect(200, 200)));
  std::unique_ptr<views::Widget> widget1(CreateTestWidget());

  ToggleOverview();
  const auto* desk_bar_view =
      GetDesksBarViewForRoot(Shell::GetAllRootWindows()[0]);
  EXPECT_EQ(2u, desk_bar_view->mini_views().size());

  // Tab until we highlight |window2|.
  SendKeyUntilOverviewItemIsHighlighted(ui::VKEY_TAB);
  SendKey(ui::VKEY_TAB);
  auto* item2 = GetOverviewItemForWindow(window2.get());
  EXPECT_TRUE(CoveredByOverviewHighlight(item2->overview_item_view()));

  // Tests that if we delete items on the right and left of item2, the overview
  // highlight bounds still contains item2's bounds.
  auto* item1 = GetOverviewItemForWindow(widget1->GetNativeWindow());
  item1->CloseWindow();
  EXPECT_TRUE(CoveredByOverviewHighlight(item2->overview_item_view()));
  auto* item3 = GetOverviewItemForWindow(widget3->GetNativeWindow());
  item3->CloseWindow();
  EXPECT_TRUE(CoveredByOverviewHighlight(item2->overview_item_view()));
}

TEST_F(DesksOverviewHighlightControllerTest,
       ActivateCloseHighlightOnNewDeskButton) {
  ToggleOverview();
  const auto* desk_bar_view =
      GetDesksBarViewForRoot(Shell::GetPrimaryRootWindow());
  const auto* new_desk_button = desk_bar_view->new_desk_button();
  const auto* desks_controller = DesksController::Get();

  // Use the keyboard to navigate to the new desk button.
  SendKey(ui::VKEY_TAB);
  SendKey(ui::VKEY_TAB);
  SendKey(ui::VKEY_TAB);
  ASSERT_EQ(new_desk_button, GetHighlightedView());

  SendKey(ui::VKEY_RETURN);
  EXPECT_EQ(3u, desks_controller->desks().size());
  EXPECT_TRUE(new_desk_button->GetEnabled());

  // Tests that pressing the key command to close a highlighted item does
  // nothing.
  SendKey(ui::VKEY_W, ui::EF_CONTROL_DOWN);
  EXPECT_EQ(3u, desks_controller->desks().size());

  // Keep adding new desks until we reach the maximum allowed amount. Verify the
  // amount of desks is indeed the maximum allowed and that the new desk button
  // is disabled.
  while (desks_controller->CanCreateDesks())
    SendKey(ui::VKEY_RETURN);
  EXPECT_FALSE(new_desk_button->GetEnabled());
  EXPECT_EQ(desks_util::kMaxNumberOfDesks, desks_controller->desks().size());

  // Tests that after the button is disabled, it is no longer highlighted.
  EXPECT_FALSE(GetHighlightedView());
}

TEST_F(DesksOverviewHighlightControllerTest, ActivateHighlightOnMiniView) {
  // We are initially on desk 1.
  const auto* desks_controller = DesksController::Get();
  auto& desks = desks_controller->desks();
  ASSERT_EQ(desks_controller->active_desk(), desks[0].get());

  ToggleOverview();
  const auto* desk_bar_view =
      GetDesksBarViewForRoot(Shell::GetPrimaryRootWindow());

  // Use keyboard to navigate to the miniview associated with desk 2.
  SendKey(ui::VKEY_TAB);
  SendKey(ui::VKEY_TAB);
  ASSERT_EQ(desk_bar_view->mini_views()[1].get(), GetHighlightedView());

  // Tests that after hitting the return key on the highlighted mini view
  // associated with desk 2, we switch to desk 2.
  DeskSwitchAnimationWaiter waiter;
  SendKey(ui::VKEY_RETURN);
  waiter.Wait();
  EXPECT_EQ(desks_controller->active_desk(), desks[1].get());
}

TEST_F(DesksOverviewHighlightControllerTest, CloseHighlightOnMiniView) {
  const auto* desks_controller = DesksController::Get();
  ASSERT_EQ(2u, desks_controller->desks().size());
  auto* desk1 = desks_controller->desks()[0].get();
  auto* desk2 = desks_controller->desks()[1].get();
  ASSERT_EQ(desk1, desks_controller->active_desk());

  ToggleOverview();
  const auto* desk_bar_view =
      GetDesksBarViewForRoot(Shell::GetPrimaryRootWindow());
  auto* mini_view1 = desk_bar_view->mini_views()[0].get();
  auto* mini_view2 = desk_bar_view->mini_views()[1].get();

  // Use keyboard to navigate to the miniview associated with desk 2.
  SendKey(ui::VKEY_TAB);
  SendKey(ui::VKEY_TAB);
  ASSERT_EQ(mini_view2, GetHighlightedView());

  // Tests that after hitting ctrl-w on the highlighted miniview associated
  // with desk 2, desk 2 is destroyed.
  SendKey(ui::VKEY_W, ui::EF_CONTROL_DOWN);
  EXPECT_EQ(1u, desks_controller->desks().size());
  EXPECT_NE(desk2, desks_controller->desks()[0].get());

  // Tests that hitting ctrl-w on the highlighted miniview if it is the last one
  // does nothing.
  while (mini_view1 != GetHighlightedView())
    SendKey(ui::VKEY_TAB);
  SendKey(ui::VKEY_W, ui::EF_CONTROL_DOWN);
  EXPECT_EQ(1u, desks_controller->desks().size());
}

}  // namespace ash
