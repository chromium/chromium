// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_highlight_controller.h"

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/style/close_button.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desk_mini_view.h"
#include "ash/wm/desks/desk_name_view.h"
#include "ash/wm/desks/desk_preview_view.h"
#include "ash/wm/desks/desks_test_util.h"
#include "ash/wm/desks/expanded_desks_bar_button.h"
#include "ash/wm/desks/legacy_desk_bar_view.h"
#include "ash/wm/desks/templates/saved_desk_util.h"
#include "ash/wm/desks/zero_state_button.h"
#include "ash/wm/overview/overview_constants.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/overview/overview_item_view.h"
#include "ash/wm/overview/overview_test_base.h"
#include "ash/wm/overview/overview_test_util.h"
#include "ash/wm/overview/scoped_overview_transform_window.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "ash/wm/window_util.h"
#include "base/feature_list.h"
#include "base/test/scoped_feature_list.h"
#include "ui/aura/window.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

struct OverviewHighlightControllerTestParams {
  bool desk_templates_enabled = false;
};

}  // namespace

class OverviewHighlightControllerTest
    : public OverviewTestBase,
      public testing::WithParamInterface<
          OverviewHighlightControllerTestParams> {
 public:
  OverviewHighlightControllerTest() = default;
  OverviewHighlightControllerTest(const OverviewHighlightControllerTest&) =
      delete;
  OverviewHighlightControllerTest& operator=(
      const OverviewHighlightControllerTest&) = delete;
  ~OverviewHighlightControllerTest() override = default;

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

  // Helper to make tests more readable.
  bool AreDeskTemplatesEnabled() const {
    return GetParam().desk_templates_enabled;
  }

  // OverviewTestBase:
  void SetUp() override {
    std::vector<base::test::FeatureRef> enabled_features, disabled_features;
    if (AreDeskTemplatesEnabled()) {
      enabled_features.push_back(features::kDesksTemplates);
    } else {
      disabled_features.push_back(features::kDesksTemplates);
    }
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);

    OverviewTestBase::SetUp();
    ScopedOverviewTransformWindow::SetImmediateCloseForTests(true);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests traversing some windows in overview mode with the tab key.
TEST_P(OverviewHighlightControllerTest, BasicTabKeyNavigation) {
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
  SendKeyUntilOverviewItemIsHighlighted(ui::VKEY_RIGHT);
  EXPECT_EQ(overview_windows[1]->GetWindow(), GetOverviewHighlightedWindow());
  SendKeyUntilOverviewItemIsHighlighted(ui::VKEY_LEFT);
  EXPECT_EQ(overview_windows[0]->GetWindow(), GetOverviewHighlightedWindow());
}

// Same as above but for tablet mode. Regression test for crbug.com/1036140.
TEST_P(OverviewHighlightControllerTest, BasicTabKeyNavigationTablet) {
  std::unique_ptr<aura::Window> window1(CreateTestWindow());
  std::unique_ptr<aura::Window> window2(CreateTestWindow());
  std::unique_ptr<aura::Window> window3(CreateTestWindow());

  TabletModeControllerTestApi().EnterTabletMode();
  ToggleOverview();
  const std::vector<std::unique_ptr<OverviewItem>>& overview_windows =
      GetOverviewItemsForRoot(0);
  SendKeyUntilOverviewItemIsHighlighted(ui::VKEY_TAB);
  EXPECT_EQ(overview_windows[0]->GetWindow(), GetOverviewHighlightedWindow());
  SendKeyUntilOverviewItemIsHighlighted(ui::VKEY_TAB);
  EXPECT_EQ(overview_windows[1]->GetWindow(), GetOverviewHighlightedWindow());
  SendKeyUntilOverviewItemIsHighlighted(ui::VKEY_RIGHT);
  EXPECT_EQ(overview_windows[2]->GetWindow(), GetOverviewHighlightedWindow());
  SendKeyUntilOverviewItemIsHighlighted(ui::VKEY_LEFT);
  EXPECT_EQ(overview_windows[1]->GetWindow(), GetOverviewHighlightedWindow());
}

// Tests that pressing Ctrl+W while a window is selected in overview closes it.
TEST_P(OverviewHighlightControllerTest, CloseWindowWithKey) {
  std::unique_ptr<views::Widget> widget(CreateTestWidget());
  ToggleOverview();

  SendKeyUntilOverviewItemIsHighlighted(ui::VKEY_RIGHT);
  EXPECT_EQ(widget->GetNativeWindow(), GetOverviewHighlightedWindow());
  SendKey(ui::VKEY_W, ui::EF_CONTROL_DOWN);
  EXPECT_TRUE(widget->IsClosed());
}

// Tests traversing some windows in overview mode with the arrow keys in every
// possible direction.
TEST_P(OverviewHighlightControllerTest, BasicArrowKeyNavigation) {
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

  for (size_t key_index = 0; key_index < std::size(arrow_keys); ++key_index) {
    ToggleOverview();
    const std::vector<std::unique_ptr<OverviewItem>>& overview_windows =
        GetOverviewItemsForRoot(0);
    for (size_t i = 0; i < test_windows + 1; ++i) {
      SendKeyUntilOverviewItemIsHighlighted(arrow_keys[key_index]);
      // TODO(flackr): Add a more readable error message by constructing a
      // string from the window IDs.
      const int index = index_path_for_direction[key_index][i];
      EXPECT_EQ(GetOverviewHighlightedWindow()->GetId(),
                overview_windows[index - 1]->GetWindow()->GetId());
    }
    ToggleOverview();
  }
}

// Tests that when an item is removed while highlighted, the highlight
// disappears, and when we tab again we pick up where we left off.
TEST_P(OverviewHighlightControllerTest, ItemClosed) {
  auto widget1 = CreateTestWidget();
  auto widget2 = CreateTestWidget();
  auto widget3 = CreateTestWidget();
  ToggleOverview();

  SendKeyUntilOverviewItemIsHighlighted(ui::VKEY_TAB);
  SendKeyUntilOverviewItemIsHighlighted(ui::VKEY_TAB);
  EXPECT_EQ(widget2->GetNativeWindow(), GetOverviewHighlightedWindow());

  // Remove |widget2| by closing it with ctrl + W. Test that the highlight
  // becomes invisible (neither widget is highlighted).
  SendKey(ui::VKEY_W, ui::EF_CONTROL_DOWN);
  EXPECT_TRUE(widget2->IsClosed());
  widget2.reset();
  EXPECT_FALSE(GetOverviewHighlightedWindow());

  // Tests that on pressing tab, the highlight becomes visible and we highlight
  // the window that comes after the deleted one.
  SendKeyUntilOverviewItemIsHighlighted(ui::VKEY_TAB);
  EXPECT_EQ(widget1->GetNativeWindow(), GetOverviewHighlightedWindow());
}

// Tests basic selection across multiple monitors.
TEST_P(OverviewHighlightControllerTest, BasicMultiMonitorArrowKeyNavigation) {
  UpdateDisplay("500x400,500x400");
  const gfx::Rect bounds1(100, 100);
  const gfx::Rect bounds2(550, 0, 100, 100);
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
TEST_P(OverviewHighlightControllerTest, MultiMonitorReversedOrder) {
  UpdateDisplay("500x400,500x400");
  Shell::Get()->display_manager()->SetLayoutForCurrentDisplays(
      display::test::CreateDisplayLayout(display_manager(),
                                         display::DisplayPlacement::LEFT, 0));
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  std::unique_ptr<aura::Window> window2(CreateTestWindow(gfx::Rect(100, 100)));
  std::unique_ptr<aura::Window> window1(
      CreateTestWindow(gfx::Rect(-450, 0, 100, 100)));
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
TEST_P(OverviewHighlightControllerTest, ThreeMonitor) {
  UpdateDisplay("500x400,500x400,500x400");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  std::unique_ptr<aura::Window> window3(
      CreateTestWindow(gfx::Rect(1000, 0, 100, 100)));
  std::unique_ptr<aura::Window> window2(
      CreateTestWindow(gfx::Rect(500, 0, 100, 100)));
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

  window3 = CreateTestWindow(gfx::Rect(1000, 0, 100, 100));
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
TEST_P(OverviewHighlightControllerTest, HighlightOverviewWindowWithReturnKey) {
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
TEST_P(OverviewHighlightControllerTest, HighlightLocationWhileDragging) {
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

  DesksOverviewHighlightControllerTest(
      const DesksOverviewHighlightControllerTest&) = delete;
  DesksOverviewHighlightControllerTest& operator=(
      const DesksOverviewHighlightControllerTest&) = delete;

  ~DesksOverviewHighlightControllerTest() override = default;

  // OverviewHighlightControllerTest:
  void SetUp() override {
    OverviewHighlightControllerTest::SetUp();

    // All tests in this suite require the desks bar to be visible in overview,
    // which requires at least two desks.
    auto* desk_controller = DesksController::Get();
    desk_controller->NewDesk(DesksCreationRemovalSource::kButton);
    ASSERT_EQ(2u, desk_controller->desks().size());

    // Give the second desk a name. The desk name gets exposed as the accessible
    // name. And the focusable views that are painted in these tests will fail
    // the accessibility paint checker checks if they lack an accessible name.
    desk_controller->desks()[1]->SetName(u"Desk 2", false);
  }

  OverviewHighlightableView* GetHighlightedView() {
    return GetHighlightController()->highlighted_view();
  }

  const LegacyDeskBarView* GetDesksBarViewForRoot(aura::Window* root_window) {
    OverviewGrid* grid =
        GetOverviewSession()->GetGridWithRootWindow(root_window);
    const LegacyDeskBarView* bar_view = grid->desks_bar_view();
    DCHECK(bar_view->IsZeroState() ^ grid->IsDesksBarViewActive());
    return bar_view;
  }

 protected:
  static void CheckDeskBarViewSize(const LegacyDeskBarView* view,
                                   const std::string& scope) {
    SCOPED_TRACE(scope);
    EXPECT_EQ(view->bounds().height(),
              view->GetWidget()->GetWindowBoundsInScreen().height());
  }
};

// Tests that we can tab through the desk mini views, new desk button and
// overview items in the correct order. Overview items will have the overview
// highlight shown when highlighted, but desks items will not.
TEST_P(DesksOverviewHighlightControllerTest, TabbingBasic) {
  std::unique_ptr<aura::Window> window1(CreateTestWindow(gfx::Rect(200, 200)));
  std::unique_ptr<aura::Window> window2(CreateTestWindow(gfx::Rect(200, 200)));

  ToggleOverview();
  const auto* desk_bar_view =
      GetDesksBarViewForRoot(Shell::GetPrimaryRootWindow());

  CheckDeskBarViewSize(desk_bar_view, "initial");
  EXPECT_EQ(2u, desk_bar_view->mini_views().size());

  // Tests that the overview item gets highlighted first.
  SendKey(ui::VKEY_TAB);
  auto* item2 = GetOverviewItemForWindow(window2.get());
  EXPECT_EQ(item2->overview_item_view(), GetHighlightedView());
  CheckDeskBarViewSize(desk_bar_view, "overview item");

  // Tests that the first highlighted desk item is the first desk preview view.
  SendKey(ui::VKEY_TAB);
  SendKey(ui::VKEY_TAB);
  EXPECT_EQ(desk_bar_view->mini_views()[0]->desk_preview(),
            GetHighlightedView());
  CheckDeskBarViewSize(desk_bar_view, "first mini view");

  // Test that one more tab highlights the desks name view.
  SendKey(ui::VKEY_TAB);
  EXPECT_EQ(desk_bar_view->mini_views()[0]->desk_name_view(),
            GetHighlightedView());

  // Tests that after tabbing through the mini views, we highlight the new desk
  // button.
  SendKey(ui::VKEY_TAB);
  SendKey(ui::VKEY_TAB);
  SendKey(ui::VKEY_TAB);

  EXPECT_EQ(desk_bar_view->expanded_state_new_desk_button()->GetInnerButton(),
            GetHighlightedView());
  CheckDeskBarViewSize(desk_bar_view, "new desk button");

  // Tests that tabbing past the new desk button, we highlight the save to a new
  // desk template. The templates button is not in the tab traversal since it is
  // hidden when we have no templates.
  if (AreDeskTemplatesEnabled()) {
    SendKey(ui::VKEY_TAB);
    EXPECT_EQ(desk_bar_view->overview_grid()->GetSaveDeskAsTemplateButton(),
              GetHighlightedView());
  }

  // Tests that after the save desk as template button (if the feature was
  // enabled), wo get the save desk for later button.
  SendKey(ui::VKEY_TAB);
  EXPECT_EQ(desk_bar_view->overview_grid()->GetSaveDeskForLaterButton(),
            GetHighlightedView());
  // Tests that after tabbing through the overview items, we go back to the
  // first overview item.
  SendKey(ui::VKEY_TAB);
  EXPECT_EQ(item2->overview_item_view(), GetHighlightedView());
  CheckDeskBarViewSize(desk_bar_view, "go back to first");
}

// Tests that we can reverse tab through the desk mini views, new desk button
// and overview items in the correct order.
TEST_P(DesksOverviewHighlightControllerTest, TabbingReverse) {
  std::unique_ptr<aura::Window> window1(CreateTestWindow(gfx::Rect(200, 200)));
  std::unique_ptr<aura::Window> window2(CreateTestWindow(gfx::Rect(200, 200)));

  ToggleOverview();
  const auto* desk_bar_view =
      GetDesksBarViewForRoot(Shell::GetPrimaryRootWindow());
  EXPECT_EQ(2u, desk_bar_view->mini_views().size());

  // Tests that the first highlighted item when reversing is the save desk for
  // later button.
  SendKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
  EXPECT_EQ(desk_bar_view->overview_grid()->GetSaveDeskForLaterButton(),
            GetHighlightedView());

  // Tests that after the save desk for later button, we get the save desk as
  // template button, if the feature is enabled.
  if (AreDeskTemplatesEnabled()) {
    SendKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
    EXPECT_EQ(desk_bar_view->overview_grid()->GetSaveDeskAsTemplateButton(),
              GetHighlightedView());
  }

  // Tests that after the desks templates button (if the feature was enabled),
  // we get to the new desk button.
  SendKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
  EXPECT_EQ(desk_bar_view->expanded_state_new_desk_button()->GetInnerButton(),
            GetHighlightedView());

  // Tests that after the new desk button comes the preview views and the desk
  // name views in reverse order.
  SendKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
  EXPECT_EQ(desk_bar_view->mini_views()[1]->desk_name_view(),
            GetHighlightedView());
  SendKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
  EXPECT_EQ(desk_bar_view->mini_views()[1]->desk_preview(),
            GetHighlightedView());

  SendKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
  EXPECT_EQ(desk_bar_view->mini_views()[0]->desk_name_view(),
            GetHighlightedView());
  SendKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
  EXPECT_EQ(desk_bar_view->mini_views()[0]->desk_preview(),
            GetHighlightedView());

  // Tests that the next highlighted item when reversing is the last overview
  // item.
  SendKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
  auto* item1 = GetOverviewItemForWindow(window1.get());
  EXPECT_EQ(item1->overview_item_view(), GetHighlightedView());

  // Tests that the next highlighted item when reversing is the save desk for
  // later button.
  SendKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
  SendKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
  EXPECT_EQ(desk_bar_view->overview_grid()->GetSaveDeskForLaterButton(),
            GetHighlightedView());

  // Tests that we return to the save desk as template button after reverse
  // tabbing through the save desk for later button if the feature is enabled.
  if (AreDeskTemplatesEnabled()) {
    SendKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
    EXPECT_EQ(desk_bar_view->overview_grid()->GetSaveDeskAsTemplateButton(),
              GetHighlightedView());
  }
}

// Tests that we can tab and chromevox interchangeably through the desk mini
// views and new desk button in the correct order.
TEST_P(DesksOverviewHighlightControllerTest, TabbingChromevox) {
  Shell::Get()->accessibility_controller()->spoken_feedback().SetEnabled(true);
  ToggleOverview();
  const auto* desk_bar_view =
      GetDesksBarViewForRoot(Shell::GetPrimaryRootWindow());
  EXPECT_EQ(2u, desk_bar_view->mini_views().size());

  // Alternate between tabbing and chromevoxing through the 2 desk preview views
  // and name views.
  SendKey(ui::VKEY_TAB);
  EXPECT_EQ(desk_bar_view->mini_views()[0]->desk_preview(),
            GetHighlightedView());
  SendKey(ui::VKEY_RIGHT, ui::EF_COMMAND_DOWN);
  EXPECT_EQ(desk_bar_view->mini_views()[0]->desk_name_view(),
            GetHighlightedView());

  SendKey(ui::VKEY_RIGHT, ui::EF_COMMAND_DOWN);
  EXPECT_EQ(desk_bar_view->mini_views()[1]->desk_preview(),
            GetHighlightedView());
  SendKey(ui::VKEY_TAB);
  EXPECT_EQ(desk_bar_view->mini_views()[1]->desk_name_view(),
            GetHighlightedView());

  // Check for the new desk button.
  SendKey(ui::VKEY_RIGHT, ui::EF_COMMAND_DOWN);
  EXPECT_EQ(desk_bar_view->expanded_state_new_desk_button()->GetInnerButton(),
            GetHighlightedView());
}

// Tests that tabbing with desk items and multiple displays works as expected.
TEST_P(DesksOverviewHighlightControllerTest, TabbingMultiDisplay) {
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

  // Tests that tabbing initially will go through the two overview items on the
  // first display.
  SendKey(ui::VKEY_TAB);
  auto* item2 = GetOverviewItemForWindow(window2.get());
  EXPECT_EQ(item2->overview_item_view(), GetHighlightedView());
  SendKey(ui::VKEY_TAB);
  auto* item1 = GetOverviewItemForWindow(window1.get());
  EXPECT_EQ(item1->overview_item_view(), GetHighlightedView());

  // Tests that further tabbing will go through the desk preview views,  desk
  // name views, the new desk button, and finally the desks templates button on
  // the first display.
  SendKey(ui::VKEY_TAB);
  EXPECT_EQ(desk_bar_view1->mini_views()[0]->desk_preview(),
            GetHighlightedView());
  SendKey(ui::VKEY_TAB);
  EXPECT_EQ(desk_bar_view1->mini_views()[0]->desk_name_view(),
            GetHighlightedView());

  SendKey(ui::VKEY_TAB);
  EXPECT_EQ(desk_bar_view1->mini_views()[1]->desk_preview(),
            GetHighlightedView());
  SendKey(ui::VKEY_TAB);
  EXPECT_EQ(desk_bar_view1->mini_views()[1]->desk_name_view(),
            GetHighlightedView());
  SendKey(ui::VKEY_TAB);

  EXPECT_EQ(desk_bar_view1->expanded_state_new_desk_button()->GetInnerButton(),
            GetHighlightedView());
  if (AreDeskTemplatesEnabled()) {
    SendKey(ui::VKEY_TAB);
    EXPECT_EQ(desk_bar_view1->overview_grid()->GetSaveDeskAsTemplateButton(),
              GetHighlightedView());
  }
  SendKey(ui::VKEY_TAB);
  EXPECT_EQ(desk_bar_view1->overview_grid()->GetSaveDeskForLaterButton(),
            GetHighlightedView());

  // Tests that the next tab will bring us to the first overview item on the
  // second display.
  SendKey(ui::VKEY_TAB);
  auto* item3 = GetOverviewItemForWindow(window3.get());
  EXPECT_EQ(item3->overview_item_view(), GetHighlightedView());

  SendKey(ui::VKEY_TAB);
  const auto* desk_bar_view2 = GetDesksBarViewForRoot(roots[1]);
  EXPECT_EQ(desk_bar_view2->mini_views()[0]->desk_preview(),
            GetHighlightedView());

  // Tab through all items on the second display.
  SendKey(ui::VKEY_TAB);
  SendKey(ui::VKEY_TAB);
  SendKey(ui::VKEY_TAB);
  SendKey(ui::VKEY_TAB);
  EXPECT_EQ(desk_bar_view2->expanded_state_new_desk_button()->GetInnerButton(),
            GetHighlightedView());
  if (AreDeskTemplatesEnabled()) {
    SendKey(ui::VKEY_TAB);
    EXPECT_EQ(desk_bar_view2->overview_grid()->GetSaveDeskAsTemplateButton(),
              GetHighlightedView());
  }
  SendKey(ui::VKEY_TAB);
  EXPECT_EQ(desk_bar_view2->overview_grid()->GetSaveDeskForLaterButton(),
            GetHighlightedView());

  // Tests that after tabbing through the items on the second display, the
  // next tab will bring us to the first overview item on the third display.
  SendKey(ui::VKEY_TAB);
  auto* item4 = GetOverviewItemForWindow(window4.get());
  EXPECT_EQ(item4->overview_item_view(), GetHighlightedView());

  SendKey(ui::VKEY_TAB);
  const auto* desk_bar_view3 = GetDesksBarViewForRoot(roots[2]);
  EXPECT_EQ(desk_bar_view3->mini_views()[0]->desk_preview(),
            GetHighlightedView());

  // Tab through all items on the third display.
  SendKey(ui::VKEY_TAB);
  SendKey(ui::VKEY_TAB);
  SendKey(ui::VKEY_TAB);
  SendKey(ui::VKEY_TAB);
  EXPECT_EQ(desk_bar_view3->expanded_state_new_desk_button()->GetInnerButton(),
            GetHighlightedView());
  if (AreDeskTemplatesEnabled()) {
    SendKey(ui::VKEY_TAB);
    EXPECT_EQ(desk_bar_view3->overview_grid()->GetSaveDeskAsTemplateButton(),
              GetHighlightedView());
  }
  SendKey(ui::VKEY_TAB);
  EXPECT_EQ(desk_bar_view3->overview_grid()->GetSaveDeskForLaterButton(),
            GetHighlightedView());

  // Tests that after tabbing through the items on the third display, the next
  // tab will bring us to the first overview item on the first display.
  SendKey(ui::VKEY_TAB);
  EXPECT_EQ(item2->overview_item_view(), GetHighlightedView());
}

TEST_P(DesksOverviewHighlightControllerTest, ActivateHighlightOnMiniView) {
  // We are initially on desk 1.
  const auto* desks_controller = DesksController::Get();
  auto& desks = desks_controller->desks();
  ASSERT_EQ(desks_controller->active_desk(), desks[0].get());

  ToggleOverview();
  const auto* desk_bar_view =
      GetDesksBarViewForRoot(Shell::GetPrimaryRootWindow());

  // Use keyboard to navigate to the preview view associated with desk 2.
  SendKey(ui::VKEY_TAB);
  SendKey(ui::VKEY_TAB);
  SendKey(ui::VKEY_TAB);
  ASSERT_EQ(desk_bar_view->mini_views()[1]->desk_preview(),
            GetHighlightedView());

  // Tests that after hitting the return key on the highlighted preview view
  // associated with desk 2, we switch to desk 2.
  DeskSwitchAnimationWaiter waiter;
  SendKey(ui::VKEY_RETURN);
  waiter.Wait();
  EXPECT_EQ(desks_controller->active_desk(), desks[1].get());
}

TEST_P(DesksOverviewHighlightControllerTest, CloseHighlightOnMiniView) {
  const auto* desks_controller = DesksController::Get();
  ASSERT_EQ(2u, desks_controller->desks().size());
  auto* desk1 = desks_controller->desks()[0].get();
  auto* desk2 = desks_controller->desks()[1].get();
  ASSERT_EQ(desk1, desks_controller->active_desk());

  ToggleOverview();
  const auto* desk_bar_view =
      GetDesksBarViewForRoot(Shell::GetPrimaryRootWindow());
  auto* mini_view2 = desk_bar_view->mini_views()[1];

  // Use keyboard to navigate to the miniview associated with desk 2.
  SendKey(ui::VKEY_TAB);
  SendKey(ui::VKEY_TAB);
  SendKey(ui::VKEY_TAB);
  ASSERT_EQ(mini_view2->desk_preview(), GetHighlightedView());

  // Tests that after hitting ctrl-w on the highlighted preview view associated
  // with desk 2, desk 2 is destroyed.
  SendKey(ui::VKEY_W, ui::EF_CONTROL_DOWN);
  EXPECT_EQ(1u, desks_controller->desks().size());
  EXPECT_NE(desk2, desks_controller->desks()[0].get());

  // Go back to zero state since there is only a single desk and mini views
  // are empty in zero state.
  EXPECT_TRUE(desk_bar_view->IsZeroState());
  EXPECT_TRUE(desk_bar_view->mini_views().empty());
}

TEST_P(DesksOverviewHighlightControllerTest, ActivateDeskNameView) {
  ToggleOverview();
  const auto* desk_bar_view =
      GetDesksBarViewForRoot(Shell::GetPrimaryRootWindow());
  auto* desk_name_view_1 = desk_bar_view->mini_views()[0]->desk_name_view();

  // Tab until the desk name view of the first desk is highlighted.
  SendKey(ui::VKEY_TAB);
  SendKey(ui::VKEY_TAB);
  EXPECT_EQ(desk_name_view_1, GetHighlightedView());

  // Press enter and expect that the desk name is being edited.
  SendKey(ui::VKEY_RETURN);
  EXPECT_TRUE(desk_name_view_1->HasFocus());
  EXPECT_TRUE(desk_bar_view->IsDeskNameBeingModified());

  // All should be selected.
  EXPECT_TRUE(desk_name_view_1->HasSelection());
  const auto* desks_controller = DesksController::Get();
  auto* desk_1 = desks_controller->desks()[0].get();
  EXPECT_EQ(desk_1->name(), desk_name_view_1->GetSelectedText());

  // Arrow keys should not change neither the focus nor the highlight.
  SendKey(ui::VKEY_RIGHT);
  SendKey(ui::VKEY_RIGHT);
  SendKey(ui::VKEY_RIGHT);
  SendKey(ui::VKEY_LEFT);
  EXPECT_EQ(desk_name_view_1, GetHighlightedView());
  EXPECT_TRUE(desk_name_view_1->HasFocus());

  // Select all and delete.
  SendKey(ui::VKEY_A, ui::EF_CONTROL_DOWN);
  SendKey(ui::VKEY_BACK);
  // Type "code" and hit Tab, this should commit the changes and move the
  // highlight to the next item.
  SendKey(ui::VKEY_C);
  SendKey(ui::VKEY_O);
  SendKey(ui::VKEY_D);
  SendKey(ui::VKEY_E);
  SendKey(ui::VKEY_TAB);

  EXPECT_FALSE(desk_name_view_1->HasFocus());
  EXPECT_EQ(desk_bar_view->mini_views()[1]->desk_preview(),
            GetHighlightedView());
  EXPECT_EQ(u"code", desk_1->name());
  EXPECT_TRUE(desk_1->is_name_set_by_user());
}

TEST_P(DesksOverviewHighlightControllerTest, RemoveDeskWhileNameIsHighlighted) {
  ToggleOverview();
  const auto* desk_bar_view =
      GetDesksBarViewForRoot(Shell::GetPrimaryRootWindow());
  auto* desk_name_view_1 = desk_bar_view->mini_views()[0]->desk_name_view();

  // Tab until the desk name view of the first desk is highlighted.
  SendKey(ui::VKEY_TAB);
  SendKey(ui::VKEY_TAB);
  EXPECT_EQ(desk_name_view_1, GetHighlightedView());

  const auto* desks_controller = DesksController::Get();
  auto* desk_1 = desks_controller->desks()[0].get();
  RemoveDesk(desk_1);

  // Tabbing again should cause no crashes.
  EXPECT_EQ(nullptr, GetHighlightedView());
  SendKey(ui::VKEY_TAB);
  EXPECT_TRUE(desk_bar_view->IsZeroState());
  EXPECT_EQ(desk_bar_view->zero_state_default_desk_button(),
            GetHighlightedView());
}

// Tests the overview highlight controller behavior when a user uses the new
// desk button.
TEST_P(DesksOverviewHighlightControllerTest,
       ActivateCloseHighlightOnNewDeskButton) {
  // Make sure the display is large enough to hold the max number of desks.
  UpdateDisplay("1200x800");
  ToggleOverview();
  const auto* desk_bar_view =
      GetDesksBarViewForRoot(Shell::GetPrimaryRootWindow());
  ASSERT_FALSE(desk_bar_view->IsZeroState());
  const auto* new_desk_button =
      desk_bar_view->expanded_state_new_desk_button()->GetInnerButton();
  const auto* desks_controller = DesksController::Get();

  auto check_name_view_at_index = [this, desks_controller](
                                      const auto* desk_bar_view, int index) {
    const auto* desk_name_view =
        desk_bar_view->mini_views()[index]->desk_name_view();
    EXPECT_TRUE(desk_name_view->HasFocus());
    if (desks_controller->CanCreateDesks())
      EXPECT_EQ(GetHighlightedView(), desk_name_view);
    EXPECT_EQ(std::u16string(), desk_name_view->GetText());
  };

  // Use the keyboard to navigate to the new desk button.
  SendKey(ui::VKEY_TAB);
  SendKey(ui::VKEY_TAB);
  SendKey(ui::VKEY_TAB);
  SendKey(ui::VKEY_TAB);
  SendKey(ui::VKEY_TAB);
  ASSERT_EQ(new_desk_button, GetHighlightedView());

  // Keep adding new desks until we reach the maximum allowed amount. Verify the
  // amount of desks is indeed the maximum allowed and that the new desk button
  // is disabled.
  while (desks_controller->CanCreateDesks()) {
    SendKey(ui::VKEY_RETURN);
    check_name_view_at_index(desk_bar_view,
                             desks_controller->desks().size() - 1);
    SendKey(ui::VKEY_TAB);
  }
  EXPECT_FALSE(new_desk_button->GetEnabled());
  EXPECT_EQ(desks_util::GetMaxNumberOfDesks(),
            desks_controller->desks().size());
}

TEST_P(DesksOverviewHighlightControllerTest, ZeroStateOfDesksBar) {
  ToggleOverview();
  auto* desks_bar_view = GetDesksBarViewForRoot(Shell::GetPrimaryRootWindow());
  ASSERT_FALSE(desks_bar_view->IsZeroState());
  ASSERT_EQ(2u, desks_bar_view->mini_views().size());

  // Remove one desk to enter zero state desks bar.
  auto* event_generator = GetEventGenerator();
  auto* mini_view = desks_bar_view->mini_views()[1];
  event_generator->MoveMouseTo(mini_view->GetBoundsInScreen().CenterPoint());
  EXPECT_TRUE(GetDeskActionVisibilityForMiniView(mini_view));
  event_generator->MoveMouseTo(GetCloseDeskButtonForMiniView(mini_view)
                                   ->GetBoundsInScreen()
                                   .CenterPoint());
  event_generator->ClickLeftButton();
  EXPECT_TRUE(desks_bar_view->IsZeroState());

  // Both zero state default desk button and zero state new desk button can be
  // focused in overview mode.
  SendKey(ui::VKEY_TAB);
  EXPECT_EQ(desks_bar_view->zero_state_default_desk_button(),
            GetHighlightedView());
  SendKey(ui::VKEY_TAB);
  EXPECT_EQ(desks_bar_view->zero_state_new_desk_button(), GetHighlightedView());

  // Trigger the zero state default desk button will focus on the default desk's
  // name view.
  SendKey(ui::VKEY_TAB);
  EXPECT_EQ(desks_bar_view->zero_state_default_desk_button(),
            GetHighlightedView());
  SendKey(ui::VKEY_RETURN);
  EXPECT_EQ(desks_bar_view->mini_views()[0]->desk_name_view(),
            GetHighlightedView());
  ToggleOverview();

  // Trigger the zero state new desk button will focus on the new created desk's
  // name view.
  ToggleOverview();
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());
  desks_bar_view = GetOverviewSession()
                       ->GetGridWithRootWindow(Shell::GetPrimaryRootWindow())
                       ->desks_bar_view();
  EXPECT_TRUE(desks_bar_view->IsZeroState());
  SendKey(ui::VKEY_TAB);
  SendKey(ui::VKEY_TAB);
  EXPECT_EQ(desks_bar_view->zero_state_new_desk_button(), GetHighlightedView());
  SendKey(ui::VKEY_RETURN);
  EXPECT_EQ(desks_bar_view->mini_views()[1]->desk_name_view(),
            GetHighlightedView());
}

TEST_P(DesksOverviewHighlightControllerTest, ActivateHighlightOnViewFocused) {
  // Set up an overview with 2 mini desk items.
  ToggleOverview();
  const auto* desk_bar_view =
      GetDesksBarViewForRoot(Shell::GetPrimaryRootWindow());
  CheckDeskBarViewSize(desk_bar_view, "initial");
  EXPECT_EQ(2u, desk_bar_view->mini_views().size());

  // Tab to first mini desk view's preview view.
  SendKey(ui::VKEY_TAB);
  ASSERT_EQ(desk_bar_view->mini_views()[0]->desk_preview(),
            GetHighlightedView());
  CheckDeskBarViewSize(desk_bar_view, "overview item");

  // Click on the second mini desk item's name view.
  auto* event_generator = GetEventGenerator();
  auto* desk_name_view_1 = desk_bar_view->mini_views()[1]->desk_name_view();
  event_generator->MoveMouseTo(
      desk_name_view_1->GetBoundsInScreen().CenterPoint());
  event_generator->ClickLeftButton();
  EXPECT_FALSE(desk_bar_view->IsZeroState());

  // Verify that focus has moved to the clicked desk item.
  EXPECT_EQ(desk_name_view_1, GetHighlightedView());
  EXPECT_TRUE(desk_name_view_1->HasFocus());
}

// Tests that there is no crash when tabbing after we switch to the zero state
// desks bar. Regression test for https://crbug.com/1301134.
TEST_P(DesksOverviewHighlightControllerTest, SwitchingToZeroStateWhileTabbing) {
  ToggleOverview();
  auto* desks_bar_view = GetDesksBarViewForRoot(Shell::GetPrimaryRootWindow());
  ASSERT_FALSE(desks_bar_view->IsZeroState());
  ASSERT_EQ(2u, desks_bar_view->mini_views().size());

  // Tab to first mini desk view's preview view.
  SendKey(ui::VKEY_TAB);
  ASSERT_EQ(desks_bar_view->mini_views()[0]->desk_preview(),
            GetHighlightedView());

  // Remove one desk to enter zero state desks bar.
  auto* event_generator = GetEventGenerator();
  auto* mini_view = desks_bar_view->mini_views()[1];
  event_generator->MoveMouseTo(mini_view->GetBoundsInScreen().CenterPoint());
  ASSERT_TRUE(GetDeskActionVisibilityForMiniView(mini_view));
  event_generator->MoveMouseTo(GetCloseDeskButtonForMiniView(mini_view)
                                   ->GetBoundsInScreen()
                                   .CenterPoint());
  event_generator->ClickLeftButton();
  ASSERT_TRUE(desks_bar_view->IsZeroState());

  // Try tabbing after removing the second desk triggers us to transition to
  // zero state desks bar. There should not be a crash.
  SendKey(ui::VKEY_TAB);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    OverviewHighlightControllerTest,
    testing::Values(OverviewHighlightControllerTestParams{true},
                    OverviewHighlightControllerTestParams{false}));
INSTANTIATE_TEST_SUITE_P(
    All,
    DesksOverviewHighlightControllerTest,
    testing::Values(OverviewHighlightControllerTestParams{true},
                    OverviewHighlightControllerTestParams{false}));

}  // namespace ash
