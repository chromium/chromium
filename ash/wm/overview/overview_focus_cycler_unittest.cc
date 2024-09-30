// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_focus_cycler.h"

#include "ash/accessibility/accessibility_controller.h"
#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/test/ash_test_util.h"
#include "ash/wm/desks/desk_action_button.h"
#include "ash/wm/desks/desk_action_view.h"
#include "ash/wm/desks/desk_name_view.h"
#include "ash/wm/desks/desk_preview_view.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_test_util.h"
#include "ash/wm/desks/overview_desk_bar_view.h"
#include "ash/wm/desks/templates/saved_desk_save_desk_button.h"
#include "ash/wm/overview/overview_item_view.h"
#include "ash/wm/overview/overview_test_base.h"
#include "ash/wm/overview/overview_test_util.h"
#include "ash/wm/splitview/split_view_setup_view.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "ash/wm/window_util.h"
#include "base/test/scoped_feature_list.h"
#include "overview_focus_cycler.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/test/display_manager_test_api.h"

namespace ash {

class OverviewFocusCyclerTest : public OverviewTestBase,
                                public testing::WithParamInterface<bool> {
 public:
  OverviewFocusCyclerTest() = default;
  OverviewFocusCyclerTest(const OverviewFocusCyclerTest&) = delete;
  OverviewFocusCyclerTest& operator=(const OverviewFocusCyclerTest&) = delete;
  ~OverviewFocusCyclerTest() override = default;

  // Helper to make tests more readable.
  bool AreDeskTemplatesEnabled() const { return GetParam(); }

  views::View* GetFocusedView() {
    return GetOverviewSession()->focus_cycler()->GetOverviewFocusedView();
  }

  // OverviewTestBase:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatureStates(
        {{features::kDesksTemplates, AreDeskTemplatesEnabled()},
         {features::kDeskBarWindowOcclusionOptimization, true}});
    OverviewTestBase::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests traversing some windows in overview mode with the tab key.
TEST_P(OverviewFocusCyclerTest, BasicTabKeyNavigation) {
  std::unique_ptr<aura::Window> window2 = CreateAppWindow();
  std::unique_ptr<aura::Window> window1 = CreateAppWindow();

  ToggleOverview();
  const std::vector<std::unique_ptr<OverviewItemBase>>& overview_windows =
      GetOverviewItemsForRoot(0);
  auto* event_generator = GetEventGenerator();
  SendKeyUntilOverviewItemIsFocused(ui::VKEY_TAB, event_generator);
  EXPECT_EQ(overview_windows[0]->item_widget()->GetNativeWindow(),
            window_util::GetFocusedWindow());
  SendKeyUntilOverviewItemIsFocused(ui::VKEY_TAB, event_generator);
  EXPECT_EQ(overview_windows[1]->item_widget()->GetNativeWindow(),
            window_util::GetFocusedWindow());
  SendKeyUntilOverviewItemIsFocused(ui::VKEY_TAB, event_generator);
  EXPECT_EQ(overview_windows[0]->item_widget()->GetNativeWindow(),
            window_util::GetFocusedWindow());
  SendKeyUntilOverviewItemIsFocused(ui::VKEY_RIGHT, event_generator);
  EXPECT_EQ(overview_windows[1]->item_widget()->GetNativeWindow(),
            window_util::GetFocusedWindow());
  SendKeyUntilOverviewItemIsFocused(ui::VKEY_LEFT, event_generator);
  EXPECT_EQ(overview_windows[0]->item_widget()->GetNativeWindow(),
            window_util::GetFocusedWindow());
}

// Same as above but for tablet mode.
TEST_P(OverviewFocusCyclerTest, BasicTabKeyNavigationTablet) {
  std::unique_ptr<aura::Window> window1 = CreateAppWindow();
  std::unique_ptr<aura::Window> window2 = CreateAppWindow();
  std::unique_ptr<aura::Window> window3 = CreateAppWindow();

  TabletModeControllerTestApi().EnterTabletMode();
  ToggleOverview();
  const std::vector<std::unique_ptr<OverviewItemBase>>& overview_windows =
      GetOverviewItemsForRoot(0);
  auto* event_generator = GetEventGenerator();
  SendKeyUntilOverviewItemIsFocused(ui::VKEY_TAB, event_generator);
  EXPECT_EQ(overview_windows[0]->item_widget()->GetNativeWindow(),
            window_util::GetFocusedWindow());
  SendKeyUntilOverviewItemIsFocused(ui::VKEY_TAB, event_generator);
  EXPECT_EQ(overview_windows[1]->item_widget()->GetNativeWindow(),
            window_util::GetFocusedWindow());
  SendKeyUntilOverviewItemIsFocused(ui::VKEY_RIGHT, event_generator);
  EXPECT_EQ(overview_windows[2]->item_widget()->GetNativeWindow(),
            window_util::GetFocusedWindow());
  SendKeyUntilOverviewItemIsFocused(ui::VKEY_LEFT, event_generator);
  EXPECT_EQ(overview_windows[1]->item_widget()->GetNativeWindow(),
            window_util::GetFocusedWindow());
}

// Tests that pressing Ctrl+W while a window is selected in overview closes it.
TEST_P(OverviewFocusCyclerTest, CloseWindowWithKey) {
  std::unique_ptr<aura::Window> window = CreateAppWindow();
  ToggleOverview();

  SendKeyUntilOverviewItemIsFocused(ui::VKEY_RIGHT, GetEventGenerator());
  PressAndReleaseKey(ui::VKEY_W, ui::EF_CONTROL_DOWN);
  EXPECT_TRUE(
      views::Widget::GetWidgetForNativeWindow(window.get())->IsClosed());
}

// Tests traversing some windows in overview mode with the arrow keys in every
// possible direction.
TEST_P(OverviewFocusCyclerTest, BasicArrowKeyNavigation) {
  const size_t test_windows = 9;
  std::vector<std::unique_ptr<aura::Window>> windows;
  for (size_t i = test_windows; i > 0; --i) {
    std::unique_ptr<aura::Window> window = CreateAppWindow();
    window->SetId(i);
    windows.push_back(std::move(window));
  }

  struct TestData {
    ui::KeyboardCode key_code;
    std::string msg;
    std::vector<int> expected_ids;
  };

  // The rows contain variable number of items making vertical navigation not
  // feasible. [Down] is equivalent to [Right] and [Up] is equivalent to [Left].
  std::vector<TestData> test_data = {
      {ui::VKEY_RIGHT, "right", {1, 2, 3, 4, 5, 6, 7, 8, 9, 1}},
      {ui::VKEY_DOWN, "down", {1, 2, 3, 4, 5, 6, 7, 8, 9, 1}},
      {ui::VKEY_LEFT, "left", {9, 8, 7, 6, 5, 4, 3, 2, 1, 9}},
      {ui::VKEY_UP, "up", {9, 8, 7, 6, 5, 4, 3, 2, 1, 9}}};

  // In this test, the original windows are assigned id's. However, when focus
  // traversing, the window that gets focused is the overview item widget
  // window. This helper gets the id of the original window associated with
  // the current focused window, if the focused window is an overview item
  // widget window.
  auto get_id_for_focused_window =
      [](const std::vector<std::unique_ptr<OverviewItemBase>>& items) -> int {
    aura::Window* window = window_util::GetFocusedWindow();
    if (!window) {
      return aura::Window::kInitialId;
    }
    for (const auto& item : items) {
      if (window == item->item_widget()->GetNativeWindow()) {
        return item->GetWindow()->GetId();
      }
    }
    return aura::Window::kInitialId;
  };

  auto* event_generator = GetEventGenerator();
  for (const TestData& test_case : test_data) {
    SCOPED_TRACE("Using " + test_case.msg + " key.");

    ToggleOverview();
    const std::vector<std::unique_ptr<OverviewItemBase>>& overview_windows =
        GetOverviewItemsForRoot(0);
    std::vector<int> actual_ids;
    for (size_t i = 0; i < test_windows + 1; ++i) {
      SendKeyUntilOverviewItemIsFocused(test_case.key_code, event_generator);
      actual_ids.push_back(get_id_for_focused_window(overview_windows));
    }
    EXPECT_THAT(test_case.expected_ids, actual_ids);
    ToggleOverview();
  }
}

// Tests basic selection across multiple monitors.
TEST_P(OverviewFocusCyclerTest, BasicMultiMonitorArrowKeyNavigation) {
  UpdateDisplay("500x400,500x400");

  // Create two windows on each display.
  const gfx::Rect bounds1(100, 100);
  const gfx::Rect bounds2(550, 0, 100, 100);
  std::unique_ptr<aura::Window> window4 = CreateAppWindow(bounds2);
  std::unique_ptr<aura::Window> window3 = CreateAppWindow(bounds2);
  std::unique_ptr<aura::Window> window2 = CreateAppWindow(bounds1);
  std::unique_ptr<aura::Window> window1 = CreateAppWindow(bounds1);

  ToggleOverview();

  auto* event_generator = GetEventGenerator();
  const std::vector<std::unique_ptr<OverviewItemBase>>& overview_root1 =
      GetOverviewItemsForRoot(0);
  const std::vector<std::unique_ptr<OverviewItemBase>>& overview_root2 =
      GetOverviewItemsForRoot(1);
  SendKeyUntilOverviewItemIsFocused(ui::VKEY_RIGHT, event_generator);
  EXPECT_EQ(overview_root1[0]->item_widget(), GetFocusedView()->GetWidget());
  SendKeyUntilOverviewItemIsFocused(ui::VKEY_RIGHT, event_generator);
  EXPECT_EQ(overview_root1[1]->item_widget(), GetFocusedView()->GetWidget());
  SendKeyUntilOverviewItemIsFocused(ui::VKEY_RIGHT, event_generator);
  EXPECT_EQ(overview_root2[0]->item_widget(), GetFocusedView()->GetWidget());
  SendKeyUntilOverviewItemIsFocused(ui::VKEY_RIGHT, event_generator);
  EXPECT_EQ(overview_root2[1]->item_widget(), GetFocusedView()->GetWidget());
}

// Tests first monitor when display order doesn't match left to right screen
// positions.
TEST_P(OverviewFocusCyclerTest, MultiMonitorReversedOrder) {
  UpdateDisplay("500x400,500x400");
  Shell::Get()->display_manager()->SetLayoutForCurrentDisplays(
      display::test::CreateDisplayLayout(display_manager(),
                                         display::DisplayPlacement::LEFT, 0));
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  std::unique_ptr<aura::Window> window2 = CreateAppWindow(gfx::Rect(100, 100));
  std::unique_ptr<aura::Window> window1 =
      CreateAppWindow(gfx::Rect(-450, 0, 100, 100));
  ASSERT_EQ(root_windows[1], window1->GetRootWindow());
  ASSERT_EQ(root_windows[0], window2->GetRootWindow());

  ToggleOverview();

  // Coming from the left to right, we should select `window1` first being on
  // the display to the left.
  auto* event_generator = GetEventGenerator();
  SendKeyUntilOverviewItemIsFocused(ui::VKEY_RIGHT, event_generator);
  EXPECT_EQ(GetOverviewItemForWindow(window1.get())->item_widget(),
            GetFocusedView()->GetWidget());

  // Exit and reenter overview.
  ToggleOverview();
  ToggleOverview();

  // Coming from right to left, we should select window2 first being on the
  // display on the right.
  SendKeyUntilOverviewItemIsFocused(ui::VKEY_LEFT, event_generator);
  EXPECT_EQ(GetOverviewItemForWindow(window2.get())->item_widget(),
            GetFocusedView()->GetWidget());
}

// Tests three monitors where the grid becomes empty on one of the monitors.
TEST_P(OverviewFocusCyclerTest, ThreeMonitors) {
  UpdateDisplay("500x400,500x400,500x400");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  std::unique_ptr<aura::Window> window3 =
      CreateAppWindow(gfx::Rect(1000, 0, 100, 100));
  std::unique_ptr<aura::Window> window2 =
      CreateAppWindow(gfx::Rect(500, 0, 100, 100));
  std::unique_ptr<aura::Window> window1 = CreateAppWindow(gfx::Rect(100, 100));
  EXPECT_EQ(root_windows[0], window1->GetRootWindow());
  EXPECT_EQ(root_windows[1], window2->GetRootWindow());
  EXPECT_EQ(root_windows[2], window3->GetRootWindow());

  ToggleOverview();

  auto* event_generator = GetEventGenerator();
  SendKeyUntilOverviewItemIsFocused(ui::VKEY_RIGHT, event_generator);
  SendKeyUntilOverviewItemIsFocused(ui::VKEY_RIGHT, event_generator);
  SendKeyUntilOverviewItemIsFocused(ui::VKEY_RIGHT, event_generator);
  EXPECT_EQ(GetOverviewItemForWindow(window3.get())->item_widget(),
            GetFocusedView()->GetWidget());

  // If the selected window is closed, then something should be selected.
  // Closing a window is done on a post task.
  window3.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(GetFocusedView());
  ToggleOverview();

  window3 = CreateTestWindow(gfx::Rect(1000, 0, 100, 100));
  ToggleOverview();
  SendKeyUntilOverviewItemIsFocused(ui::VKEY_RIGHT, event_generator);
  SendKeyUntilOverviewItemIsFocused(ui::VKEY_RIGHT, event_generator);
  SendKeyUntilOverviewItemIsFocused(ui::VKEY_RIGHT, event_generator);

  // If the window on the second display is removed, the selected window should
  // remain window3.
  EXPECT_EQ(GetOverviewItemForWindow(window3.get())->item_widget(),
            GetFocusedView()->GetWidget());
  window2.reset();
  EXPECT_EQ(GetOverviewItemForWindow(window3.get())->item_widget(),
            GetFocusedView()->GetWidget());
}

// Tests selecting a window in overview mode with the return key.
TEST_P(OverviewFocusCyclerTest, FocusOverviewWindowWithReturnKey) {
  std::unique_ptr<aura::Window> window2 = CreateAppWindow();
  std::unique_ptr<aura::Window> window1 = CreateAppWindow();
  ToggleOverview();

  // Pressing the return key on an item that is not focused should not do
  // anything.
  PressAndReleaseKey(ui::VKEY_RETURN);
  OverviewController* overview_controller = OverviewController::Get();
  EXPECT_TRUE(overview_controller->InOverviewSession());

  // Focus and select the first window.
  PressAndReleaseKey(ui::VKEY_TAB);
  PressAndReleaseKey(ui::VKEY_RETURN);
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_TRUE(wm::IsActiveWindow(window1.get()));

  // Focus and select the second window.
  ToggleOverview();
  PressAndReleaseKey(ui::VKEY_TAB);
  PressAndReleaseKey(ui::VKEY_TAB);
  PressAndReleaseKey(ui::VKEY_RETURN);
  EXPECT_FALSE(OverviewController::Get()->InOverviewSession());
  EXPECT_TRUE(wm::IsActiveWindow(window2.get()));
}

// Tests that the location of the overview focus ring is as expected while
// dragging an overview item.
TEST_P(OverviewFocusCyclerTest, FocusLocationWhileDragging) {
  std::unique_ptr<aura::Window> window1 = CreateAppWindow();
  std::unique_ptr<aura::Window> window2 = CreateAppWindow();
  std::unique_ptr<aura::Window> window3 = CreateAppWindow();

  ToggleOverview();

  // Tab once to show the focus ring.
  auto* event_generator = GetEventGenerator();
  SendKeyUntilOverviewItemIsFocused(ui::VKEY_TAB, event_generator);
  OverviewItemBase* item3 = GetOverviewItemForWindow(window3.get());
  EXPECT_EQ(item3->item_widget()->GetContentsView(), GetFocusedView());

  // Tests that the there is no focused view while dragging. Drag the item in a
  // way which does not enter splitview, or close overview.
  DragItemToPoint(item3, gfx::Point(20, 20), event_generator,
                  /*by_touch_gestures=*/false, /*drop=*/false);
  EXPECT_FALSE(GetFocusedView());

  PressAndReleaseKey(ui::VKEY_TAB);
  EXPECT_FALSE(GetFocusedView());

  DragItemToPoint(item3, gfx::Point(300, 200), event_generator,
                  /*by_touch_gestures=*/false, /*drop=*/true);
  EXPECT_FALSE(GetFocusedView());

  // Tests that tabbing after dragging works as expected.
  SendKeyUntilOverviewItemIsFocused(ui::VKEY_TAB, event_generator);
  EXPECT_EQ(item3->item_widget()->GetContentsView(), GetFocusedView());
}

// Tests that the locations of the overview focus ring are as expected when
// tabbing through split view setup UI. This tests the case were splitview is
// entered via snapping a window while already in overview. Regression test for
// http://b/369539129 for more details.
TEST_P(OverviewFocusCyclerTest, TabbingWithSplitview) {
  std::unique_ptr<aura::Window> window1 = CreateAppWindow();
  std::unique_ptr<aura::Window> window2 = CreateAppWindow();

  ToggleOverview();
  SplitViewController::Get(Shell::GetPrimaryRootWindow())
      ->SnapWindow(window1.get(), SnapPosition::kPrimary);

  // Tab once to focus `item2`.
  PressAndReleaseKey(ui::VKEY_TAB);
  EXPECT_EQ(
      GetOverviewItemForWindow(window2.get())->item_widget()->GetContentsView(),
      GetFocusedView());

  // Tab to the toast dismiss button and then the settings button.
  OverviewGrid* grid = GetOverviewSession()->grid_list()[0].get();
  PressAndReleaseKey(ui::VKEY_TAB);
  EXPECT_EQ(grid->GetSplitViewSetupView()->GetViewByID(
                SplitViewSetupView::kDismissButtonIDForTest),
            GetFocusedView());

  PressAndReleaseKey(ui::VKEY_TAB);
  EXPECT_EQ(grid->GetSplitViewSetupView()->GetViewByID(
                SplitViewSetupView::kSettingsButtonIDForTest),
            GetFocusedView());
}

// ----------------------------------------------------------------------------
// DesksOverviewFocusCyclerTest:

class DesksOverviewFocusCyclerTest : public OverviewFocusCyclerTest {
 public:
  DesksOverviewFocusCyclerTest()
      : saved_desk_ui_revamp_enabled_(features::IsSavedDeskUiRevampEnabled()) {}
  DesksOverviewFocusCyclerTest(const DesksOverviewFocusCyclerTest&) = delete;
  DesksOverviewFocusCyclerTest& operator=(const DesksOverviewFocusCyclerTest&) =
      delete;
  ~DesksOverviewFocusCyclerTest() override = default;

  const OverviewDeskBarView* GetDesksBarViewForRoot(aura::Window* root_window) {
    OverviewGrid* grid =
        GetOverviewSession()->GetGridWithRootWindow(root_window);
    const OverviewDeskBarView* bar_view = grid->desks_bar_view();
    CHECK(bar_view->IsZeroState() ^ grid->IsDesksBarViewActive());
    return bar_view;
  }

  // OverviewFocusCyclerTest:
  void SetUp() override {
    OverviewFocusCyclerTest::SetUp();

    // All tests in this suite require the desks bar to be visible in overview,
    // which requires at least two desks.
    auto* desk_controller = DesksController::Get();
    desk_controller->NewDesk(DesksCreationRemovalSource::kButton);
    ASSERT_EQ(2u, desk_controller->desks().size());

    // Give the second desk a name. The desk name gets exposed as the accessible
    // name. And the focusable views that are painted in these tests will fail
    // the accessibility paint checker checks if they lack an accessible name.
    desk_controller->GetDeskAtIndex(1)->SetName(u"Desk 2", false);
  }

 protected:
  static void CheckDeskBarViewSize(const OverviewDeskBarView* view,
                                   const std::string& scope) {
    SCOPED_TRACE(scope);
    EXPECT_EQ(view->bounds().height(),
              view->GetWidget()->GetWindowBoundsInScreen().height());
  }

  const bool saved_desk_ui_revamp_enabled_;
};

// Tests that we can tab through the desk mini views, new desk button and other
// desk items in the correct order.
TEST_P(DesksOverviewFocusCyclerTest, TabbingBasic) {
  base::AutoReset<bool> disable_app_id_check =
      OverviewController::Get()->SetDisableAppIdCheckForTests();

  std::unique_ptr<aura::Window> window1(CreateAppWindow(gfx::Rect(200, 200)));
  std::unique_ptr<aura::Window> window2(CreateAppWindow(gfx::Rect(200, 200)));

  ToggleOverview();
  const auto* desk_bar_view =
      GetDesksBarViewForRoot(Shell::GetPrimaryRootWindow());

  CheckDeskBarViewSize(desk_bar_view, "initial");
  ASSERT_EQ(2u, desk_bar_view->mini_views().size());

  // Tests that the overview item gets focused first.
  PressAndReleaseKey(ui::VKEY_TAB);
  auto* item2 = GetOverviewItemForWindow(window2.get())
                    ->GetLeafItemForWindow(window2.get());
  EXPECT_EQ(item2->overview_item_view(), GetFocusedView());
  CheckDeskBarViewSize(desk_bar_view, "overview item");

  // Tests that the first focused desk item is the first desk preview view.
  DeskMiniView* first_mini_view = desk_bar_view->mini_views()[0];
  PressAndReleaseKey(ui::VKEY_TAB);
  PressAndReleaseKey(ui::VKEY_TAB);
  EXPECT_EQ(first_mini_view->desk_preview(), GetFocusedView());
  CheckDeskBarViewSize(desk_bar_view, "first mini view");

  // Tests that the context menu/combine desks and close all buttons of the
  // first desk preview is focused next.
  PressAndReleaseKey(ui::VKEY_TAB);
  const DeskActionView* desk_action_view = first_mini_view->desk_action_view();
  EXPECT_EQ(saved_desk_ui_revamp_enabled_
                ? desk_action_view->context_menu_button()
                : desk_action_view->combine_desks_button(),
            GetFocusedView());
  PressAndReleaseKey(ui::VKEY_TAB);
  EXPECT_EQ(desk_action_view->close_all_button(), GetFocusedView());

  // Test that one more tab focuses the desks name view.
  PressAndReleaseKey(ui::VKEY_TAB);
  EXPECT_EQ(first_mini_view->desk_name_view(), GetFocusedView());

  // Tab three times through the second mini view (it has no windows so there is
  // no combine desks button). The next tab should focus the new desk button.
  PressAndReleaseKey(ui::VKEY_TAB);
  PressAndReleaseKey(ui::VKEY_TAB);
  PressAndReleaseKey(ui::VKEY_TAB);
  PressAndReleaseKey(ui::VKEY_TAB);
  EXPECT_EQ(desk_bar_view->new_desk_button(), GetFocusedView());
  CheckDeskBarViewSize(desk_bar_view, "new desk button");

  // With forest, there are is no saved desk save desk container.
  if (saved_desk_ui_revamp_enabled_) {
    return;
  }

  // Tests that tabbing past the new desk button, we focus the save to a new
  // desk template. The templates button is not in the tab traversal since it is
  // hidden when we have no templates.
  if (AreDeskTemplatesEnabled()) {
    PressAndReleaseKey(ui::VKEY_TAB);
    EXPECT_EQ(desk_bar_view->overview_grid()->GetSaveDeskAsTemplateButton(),
              GetFocusedView());
  }

  // Tests that after the save desk as template button (if the feature was
  // enabled), focus goes to the save desk for later button.
  PressAndReleaseKey(ui::VKEY_TAB);
  EXPECT_EQ(desk_bar_view->overview_grid()->GetSaveDeskForLaterButton(),
            GetFocusedView());
}

// Tests that we can reverse tab through the desk mini views, new desk button
// and overview items in the correct order.
TEST_P(DesksOverviewFocusCyclerTest, TabbingReverse) {
  base::AutoReset<bool> disable_app_id_check =
      OverviewController::Get()->SetDisableAppIdCheckForTests();

  std::unique_ptr<aura::Window> window1(CreateAppWindow(gfx::Rect(200, 200)));
  std::unique_ptr<aura::Window> window2(CreateAppWindow(gfx::Rect(200, 200)));

  ToggleOverview();
  const auto* desk_bar_view =
      GetDesksBarViewForRoot(Shell::GetPrimaryRootWindow());
  ASSERT_EQ(2u, desk_bar_view->mini_views().size());

  if (!saved_desk_ui_revamp_enabled_) {
    // Tests that the first focused item when reversing is the save desk for
    // later button.
    PressAndReleaseKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
    EXPECT_EQ(desk_bar_view->overview_grid()->GetSaveDeskForLaterButton(),
              GetFocusedView());

    // Tests that after the save desk for later button, we get the save desk as
    // template button, if the feature is enabled.
    if (AreDeskTemplatesEnabled()) {
      PressAndReleaseKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
      EXPECT_EQ(desk_bar_view->overview_grid()->GetSaveDeskAsTemplateButton(),
                GetFocusedView());
    }
  }

  // Tests that after the desks templates button (if the feature was enabled),
  // we get to the new desk button.
  PressAndReleaseKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
  EXPECT_EQ(desk_bar_view->new_desk_button(), GetFocusedView());

  // Tests that after the new desk button comes the preview views, desk action
  // buttons, and the desk name views in reverse order.
  PressAndReleaseKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
  DeskMiniView* second_mini_view = desk_bar_view->mini_views()[1];
  EXPECT_EQ(second_mini_view->desk_name_view(), GetFocusedView());
  PressAndReleaseKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
  EXPECT_EQ(second_mini_view->desk_action_view()->close_all_button(),
            GetFocusedView());
  PressAndReleaseKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
  EXPECT_EQ(second_mini_view->desk_preview(), GetFocusedView());

  PressAndReleaseKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
  DeskMiniView* first_mini_view = desk_bar_view->mini_views()[0];
  DeskActionView* first_action_view = first_mini_view->desk_action_view();
  EXPECT_EQ(first_mini_view->desk_name_view(), GetFocusedView());
  PressAndReleaseKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
  EXPECT_EQ(first_action_view->close_all_button(), GetFocusedView());
  PressAndReleaseKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
  EXPECT_EQ(saved_desk_ui_revamp_enabled_
                ? first_action_view->context_menu_button()
                : first_action_view->combine_desks_button(),
            GetFocusedView());
  PressAndReleaseKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
  EXPECT_EQ(first_mini_view->desk_preview(), GetFocusedView());

  // Tests that the next focused item when reversing is the last overview item.
  PressAndReleaseKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
  auto* item1 = GetOverviewItemForWindow(window1.get())
                    ->GetLeafItemForWindow(window1.get());
  EXPECT_EQ(item1->overview_item_view(), GetFocusedView());

  // With forest, there are is no saved desk save desk container.
  if (saved_desk_ui_revamp_enabled_) {
    return;
  }

  // Tests that the next focused item when reversing is the save desk for later
  // button.
  PressAndReleaseKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
  PressAndReleaseKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
  EXPECT_EQ(desk_bar_view->overview_grid()->GetSaveDeskForLaterButton(),
            GetFocusedView());

  // Tests that we return to the save desk as template button after reverse
  // tabbing through the save desk for later button if the feature is enabled.
  if (AreDeskTemplatesEnabled()) {
    PressAndReleaseKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
    EXPECT_EQ(desk_bar_view->overview_grid()->GetSaveDeskAsTemplateButton(),
              GetFocusedView());
  }
}

// Tests that tabbing with desk items and multiple displays works as expected.
TEST_P(DesksOverviewFocusCyclerTest, TabbingMultiDisplay) {
  base::AutoReset<bool> disable_app_id_check =
      OverviewController::Get()->SetDisableAppIdCheckForTests();

  UpdateDisplay("600x400,600x400,600x400");
  aura::Window::Windows roots = Shell::GetAllRootWindows();
  ASSERT_EQ(3u, roots.size());

  // Create two windows on the first display, and one each on the second and
  // third displays.
  std::unique_ptr<aura::Window> window1(CreateAppWindow(gfx::Rect(200, 200)));
  std::unique_ptr<aura::Window> window2(CreateAppWindow(gfx::Rect(200, 200)));
  std::unique_ptr<aura::Window> window3(
      CreateAppWindow(gfx::Rect(600, 0, 200, 200)));
  std::unique_ptr<aura::Window> window4(
      CreateAppWindow(gfx::Rect(1200, 0, 200, 200)));
  ASSERT_EQ(roots[0], window1->GetRootWindow());
  ASSERT_EQ(roots[0], window2->GetRootWindow());
  ASSERT_EQ(roots[1], window3->GetRootWindow());
  ASSERT_EQ(roots[2], window4->GetRootWindow());

  ToggleOverview();
  const auto* desk_bar_view1 = GetDesksBarViewForRoot(roots[0]);
  EXPECT_EQ(2u, desk_bar_view1->mini_views().size());

  // Tests that tabbing initially will go through the two overview items on the
  // first display.
  PressAndReleaseKey(ui::VKEY_TAB);
  auto* item2 = GetOverviewItemForWindow(window2.get())
                    ->GetLeafItemForWindow(window2.get());
  EXPECT_EQ(item2->overview_item_view(), GetFocusedView());
  PressAndReleaseKey(ui::VKEY_TAB);
  auto* item1 = GetOverviewItemForWindow(window1.get())
                    ->GetLeafItemForWindow(window1.get());
  EXPECT_EQ(item1->overview_item_view(), GetFocusedView());

  // Tests that further tabbing will go through the desk preview views, desk
  // name views, the new desk button, and finally the desks templates button on
  // the first display.
  PressAndReleaseKey(ui::VKEY_TAB);
  DeskMiniView* mini_view1 = desk_bar_view1->mini_views()[0];
  DeskActionView* action_view1 = mini_view1->desk_action_view();
  EXPECT_EQ(mini_view1->desk_preview(), GetFocusedView());
  PressAndReleaseKey(ui::VKEY_TAB);
  EXPECT_EQ(saved_desk_ui_revamp_enabled_
                ? action_view1->context_menu_button()
                : action_view1->combine_desks_button(),
            GetFocusedView());

  PressAndReleaseKey(ui::VKEY_TAB);
  EXPECT_EQ(action_view1->close_all_button(), GetFocusedView());

  PressAndReleaseKey(ui::VKEY_TAB);
  EXPECT_EQ(mini_view1->desk_name_view(), GetFocusedView());

  PressAndReleaseKey(ui::VKEY_TAB);
  DeskMiniView* mini_view2 = desk_bar_view1->mini_views()[1];
  EXPECT_EQ(mini_view2->desk_preview(), GetFocusedView());
  PressAndReleaseKey(ui::VKEY_TAB);
  EXPECT_EQ(mini_view2->desk_action_view()->close_all_button(),
            GetFocusedView());
  PressAndReleaseKey(ui::VKEY_TAB);
  EXPECT_EQ(mini_view2->desk_name_view(), GetFocusedView());
  PressAndReleaseKey(ui::VKEY_TAB);
  EXPECT_EQ(desk_bar_view1->new_desk_button(), GetFocusedView());

  if (!saved_desk_ui_revamp_enabled_) {
    if (AreDeskTemplatesEnabled()) {
      PressAndReleaseKey(ui::VKEY_TAB);
      EXPECT_EQ(desk_bar_view1->overview_grid()->GetSaveDeskAsTemplateButton(),
                GetFocusedView());
    }
    PressAndReleaseKey(ui::VKEY_TAB);
    EXPECT_EQ(desk_bar_view1->overview_grid()->GetSaveDeskForLaterButton(),
              GetFocusedView());
  }

  // Tests that the next tab will bring us to the first overview item on the
  // second display.
  PressAndReleaseKey(ui::VKEY_TAB);
  auto* item3 = GetOverviewItemForWindow(window3.get())
                    ->GetLeafItemForWindow(window3.get());
  EXPECT_EQ(item3->overview_item_view(), GetFocusedView());

  PressAndReleaseKey(ui::VKEY_TAB);
  const auto* desk_bar_view2 = GetDesksBarViewForRoot(roots[1]);
  EXPECT_EQ(desk_bar_view2->mini_views()[0]->desk_preview(), GetFocusedView());

  // Tab through all items on the second display.
  SendKey(ui::VKEY_TAB, GetEventGenerator(), ui::EF_NONE, /*count=*/7);
  EXPECT_EQ(desk_bar_view2->new_desk_button(), GetFocusedView());

  if (!saved_desk_ui_revamp_enabled_) {
    if (AreDeskTemplatesEnabled()) {
      PressAndReleaseKey(ui::VKEY_TAB);
      EXPECT_EQ(desk_bar_view2->overview_grid()->GetSaveDeskAsTemplateButton(),
                GetFocusedView());
    }
    PressAndReleaseKey(ui::VKEY_TAB);
    EXPECT_EQ(desk_bar_view2->overview_grid()->GetSaveDeskForLaterButton(),
              GetFocusedView());
  }

  // Tests that after tabbing through the items on the second display, the
  // next tab will bring us to the first overview item on the third display.
  PressAndReleaseKey(ui::VKEY_TAB);
  auto* item4 = GetOverviewItemForWindow(window4.get())
                    ->GetLeafItemForWindow(window4.get());
  EXPECT_EQ(item4->overview_item_view(), GetFocusedView());

  PressAndReleaseKey(ui::VKEY_TAB);
  const auto* desk_bar_view3 = GetDesksBarViewForRoot(roots[2]);
  EXPECT_EQ(desk_bar_view3->mini_views()[0]->desk_preview(), GetFocusedView());

  // Tab through all items on the third display.
  SendKey(ui::VKEY_TAB, GetEventGenerator(), ui::EF_NONE, /*count=*/7);
  EXPECT_EQ(desk_bar_view3->new_desk_button(), GetFocusedView());

  if (!saved_desk_ui_revamp_enabled_) {
    if (AreDeskTemplatesEnabled()) {
      PressAndReleaseKey(ui::VKEY_TAB);
      EXPECT_EQ(desk_bar_view3->overview_grid()->GetSaveDeskAsTemplateButton(),
                GetFocusedView());
    }
    PressAndReleaseKey(ui::VKEY_TAB);
    EXPECT_EQ(desk_bar_view3->overview_grid()->GetSaveDeskForLaterButton(),
              GetFocusedView());
  }

  // Tests that after tabbing through the items on the third display, the next
  // tab will bring us to the first overview item on the first display.
  PressAndReleaseKey(ui::VKEY_TAB);
  EXPECT_EQ(item2->overview_item_view(), GetFocusedView());
}

// Tests that we can tab and chromevox interchangeably through the desk mini
// views and new desk button in the correct order.
TEST_P(DesksOverviewFocusCyclerTest, TabbingChromevox) {
  Shell::Get()->accessibility_controller()->spoken_feedback().SetEnabled(true);
  ToggleOverview();
  const auto* desk_bar_view =
      GetDesksBarViewForRoot(Shell::GetPrimaryRootWindow());
  EXPECT_EQ(2u, desk_bar_view->mini_views().size());

  auto* mini_view1 = desk_bar_view->mini_views()[0].get();
  auto* mini_view2 = desk_bar_view->mini_views()[1].get();

  // Alternate between tabbing and chromevoxing through the 2 desk preview views
  // and name views.
  PressAndReleaseKey(ui::VKEY_TAB);
  EXPECT_EQ(mini_view1->desk_preview(), GetFocusedView());
  PressAndReleaseKey(ui::VKEY_RIGHT, ui::EF_COMMAND_DOWN);
  EXPECT_EQ(mini_view1->desk_action_view()->close_all_button(),
            GetFocusedView());
  PressAndReleaseKey(ui::VKEY_RIGHT, ui::EF_COMMAND_DOWN);
  EXPECT_EQ(mini_view1->desk_name_view(), GetFocusedView());

  PressAndReleaseKey(ui::VKEY_RIGHT, ui::EF_COMMAND_DOWN);
  EXPECT_EQ(mini_view2->desk_preview(), GetFocusedView());
  PressAndReleaseKey(ui::VKEY_RIGHT, ui::EF_COMMAND_DOWN);
  EXPECT_EQ(mini_view2->desk_action_view()->close_all_button(),
            GetFocusedView());
  PressAndReleaseKey(ui::VKEY_TAB);
  EXPECT_EQ(mini_view2->desk_name_view(), GetFocusedView());

  // Check for the new desk button.
  PressAndReleaseKey(ui::VKEY_RIGHT, ui::EF_COMMAND_DOWN);
  EXPECT_EQ(desk_bar_view->new_desk_button(), GetFocusedView());
}

TEST_P(DesksOverviewFocusCyclerTest, MiniViewAccelerator) {
  // We are initially on desk 1.
  const auto* desks_controller = DesksController::Get();
  auto& desks = desks_controller->desks();
  ASSERT_EQ(desks_controller->active_desk(), desks[0].get());

  ToggleOverview();
  const auto* desk_bar_view =
      GetDesksBarViewForRoot(Shell::GetPrimaryRootWindow());

  // Use keyboard to navigate to the preview view associated with desk 2.
  PressAndReleaseKey(ui::VKEY_TAB);
  PressAndReleaseKey(ui::VKEY_TAB);
  PressAndReleaseKey(ui::VKEY_TAB);
  PressAndReleaseKey(ui::VKEY_TAB);
  ASSERT_EQ(desk_bar_view->mini_views()[1]->desk_preview(), GetFocusedView());

  // Tests that after hitting the space key on the focused preview view
  // associated with desk 2, we switch to desk 2.
  PressAndReleaseKey(ui::VKEY_SPACE);
  DeskSwitchAnimationWaiter().Wait();
  EXPECT_EQ(desks_controller->active_desk(), desks[1].get());
}

TEST_P(DesksOverviewFocusCyclerTest, CloseDeskWithMiniViewAccelerator) {
  const auto* desks_controller = DesksController::Get();
  ASSERT_EQ(2u, desks_controller->desks().size());
  auto* desk1 = desks_controller->GetDeskAtIndex(0);
  auto* desk2 = desks_controller->GetDeskAtIndex(1);
  ASSERT_EQ(desk1, desks_controller->active_desk());

  ToggleOverview();
  const auto* desk_bar_view =
      GetDesksBarViewForRoot(Shell::GetPrimaryRootWindow());
  auto* mini_view2 = desk_bar_view->mini_views()[1].get();

  // Use keyboard to navigate to the miniview associated with desk 2.
  PressAndReleaseKey(ui::VKEY_TAB);
  PressAndReleaseKey(ui::VKEY_TAB);
  PressAndReleaseKey(ui::VKEY_TAB);
  PressAndReleaseKey(ui::VKEY_TAB);
  ASSERT_EQ(mini_view2->desk_preview(), GetFocusedView());

  // Tests that after hitting ctrl-w on the focused preview view associated with
  // `desk2`, `desk2` is destroyed.
  PressAndReleaseKey(ui::VKEY_W, ui::EF_CONTROL_DOWN);
  EXPECT_EQ(1u, desks_controller->desks().size());
  EXPECT_NE(desk2, desks_controller->GetDeskAtIndex(0));

  // Desks bar never goes back to zero state after it's initialized.
  EXPECT_FALSE(desk_bar_view->IsZeroState());
  EXPECT_FALSE(desk_bar_view->mini_views().empty());
}

TEST_P(DesksOverviewFocusCyclerTest, DeskNameView) {
  ToggleOverview();
  const auto* desk_bar_view =
      GetDesksBarViewForRoot(Shell::GetPrimaryRootWindow());
  auto* desk_name_view_1 = desk_bar_view->mini_views()[0]->desk_name_view();

  auto* desk_1 = DesksController::Get()->GetDeskAtIndex(0);
  const std::u16string original_name = desk_1->name();

  // Tab until the desk name view of the first desk is focused. Verify that the
  // desk name is being edited.
  PressAndReleaseKey(ui::VKEY_TAB);
  PressAndReleaseKey(ui::VKEY_TAB);
  PressAndReleaseKey(ui::VKEY_TAB);
  EXPECT_EQ(desk_name_view_1, GetFocusedView());
  EXPECT_TRUE(desk_name_view_1->HasFocus());
  EXPECT_TRUE(desk_bar_view->IsDeskNameBeingModified());

  // The whole name starts off selected.
  EXPECT_TRUE(desk_name_view_1->HasSelection());
  EXPECT_EQ(original_name, desk_name_view_1->GetSelectedText());

  // Left and right arrow keys should not change neither the focus as they need
  // to move the text caret.
  PressAndReleaseKey(ui::VKEY_RIGHT);
  PressAndReleaseKey(ui::VKEY_LEFT);
  EXPECT_EQ(desk_name_view_1, GetFocusedView());
  EXPECT_TRUE(desk_name_view_1->HasFocus());

  // Tests ctrl + A and backspace to select all and delete.
  PressAndReleaseKey(ui::VKEY_A, ui::EF_CONTROL_DOWN);
  PressAndReleaseKey(ui::VKEY_BACK);
  EXPECT_EQ(u"", desk_name_view_1->GetText());

  // Type "code" into the desk name textfield. It should update, but the desk
  // name has not change.
  PressAndReleaseKey(ui::VKEY_C);
  PressAndReleaseKey(ui::VKEY_O);
  PressAndReleaseKey(ui::VKEY_D);
  PressAndReleaseKey(ui::VKEY_E);
  EXPECT_EQ(u"code", desk_name_view_1->GetText());
  EXPECT_EQ(original_name, desk_1->name());

  // Tests that pressing tab will advance focus and commit the desk name
  // changes.
  PressAndReleaseKey(ui::VKEY_TAB);
  EXPECT_FALSE(desk_name_view_1->HasFocus());
  EXPECT_EQ(u"code", desk_1->name());
  EXPECT_TRUE(desk_1->is_name_set_by_user());
}

TEST_P(DesksOverviewFocusCyclerTest, RemoveDeskWhileNameFocused) {
  ToggleOverview();
  const auto* desk_bar_view =
      GetDesksBarViewForRoot(Shell::GetPrimaryRootWindow());
  auto* desk_name_view_1 = desk_bar_view->mini_views()[0]->desk_name_view();

  // Tab until the desk name view of the first desk is focused.
  PressAndReleaseKey(ui::VKEY_TAB);
  PressAndReleaseKey(ui::VKEY_TAB);
  PressAndReleaseKey(ui::VKEY_TAB);
  EXPECT_EQ(desk_name_view_1, GetFocusedView());

  // Desks bar never goes back to zero state after it's initialized.
  const auto* desks_controller = DesksController::Get();
  auto* desk_1 = desks_controller->GetDeskAtIndex(0);
  RemoveDesk(desk_1);
  RunScheduledLayoutForAllOverviewDeskBars();
  EXPECT_EQ(nullptr, GetFocusedView());
  EXPECT_FALSE(desk_bar_view->IsZeroState());

  // Tabbing again should cause no crashes.
  PressAndReleaseKey(ui::VKEY_TAB);
  RunScheduledLayoutForAllOverviewDeskBars();
  EXPECT_EQ(desk_bar_view->mini_views()[0]->desk_preview(), GetFocusedView());
}

// Tests the overview focus cycler behavior when a user uses the new desk
// button.
TEST_P(DesksOverviewFocusCyclerTest, NewDesksWithKeyboard) {
  // Make sure the display is large enough to hold the max number of desks.
  UpdateDisplay("1200x800");

  ToggleOverview();
  const auto* desk_bar_view =
      GetDesksBarViewForRoot(Shell::GetPrimaryRootWindow());
  ASSERT_FALSE(desk_bar_view->IsZeroState());
  const views::LabelButton* new_desk_button = desk_bar_view->new_desk_button();
  const auto* desks_controller = DesksController::Get();

  auto check_name_view_at_index = [this, desks_controller](
                                      const auto* desk_bar_view, int index) {
    const auto* desk_name_view =
        desk_bar_view->mini_views()[index]->desk_name_view();
    EXPECT_TRUE(desk_name_view->HasFocus());
    if (desks_controller->CanCreateDesks()) {
      EXPECT_EQ(desk_name_view, GetFocusedView());
    }
    EXPECT_EQ(std::u16string(), desk_name_view->GetText());
  };

  // Tab seven times, three times for each desk (preview, close button, name),
  // and then one more time to focus the new desk button.
  for (int i = 0; i < 7; ++i) {
    PressAndReleaseKey(ui::VKEY_TAB);
  }
  ASSERT_EQ(new_desk_button, GetFocusedView());

  // Keep adding new desks until we reach the maximum allowed amount. Verify the
  // amount of desks is indeed the maximum allowed and that the new desk button
  // is disabled.
  while (desks_controller->CanCreateDesks()) {
    PressAndReleaseKey(ui::VKEY_SPACE);
    RunScheduledLayoutForAllOverviewDeskBars();
    check_name_view_at_index(desk_bar_view,
                             desks_controller->desks().size() - 1);
    PressAndReleaseKey(ui::VKEY_TAB);
  }
  EXPECT_FALSE(new_desk_button->GetEnabled());
  EXPECT_EQ(desks_util::GetMaxNumberOfDesks(),
            desks_controller->desks().size());
}

TEST_P(DesksOverviewFocusCyclerTest, ZeroStateOfDesksBar) {
  ToggleOverview();
  auto* desks_bar_view = GetDesksBarViewForRoot(Shell::GetPrimaryRootWindow());
  ASSERT_FALSE(desks_bar_view->IsZeroState());
  ASSERT_EQ(2u, desks_bar_view->mini_views().size());

  // Remove one desk to enter zero state desks bar.
  auto* mini_view = desks_bar_view->mini_views()[1].get();
  GetEventGenerator()->MoveMouseTo(
      mini_view->GetBoundsInScreen().CenterPoint());
  EXPECT_TRUE(GetDeskActionVisibilityForMiniView(mini_view));
  LeftClickOn(GetCloseDeskButtonForMiniView(mini_view));

  // Desks bar never goes back to zero state after it's initialized.
  ASSERT_FALSE(desks_bar_view->IsZeroState());

  // Exit and reenter overview to show the zero state desks bar.
  ToggleOverview();
  ToggleOverview();
  ASSERT_TRUE(OverviewController::Get()->InOverviewSession());

  // Tab through and verify the zero state desk bar views.
  desks_bar_view = GetOverviewSession()
                       ->GetGridWithRootWindow(Shell::GetPrimaryRootWindow())
                       ->desks_bar_view();
  ASSERT_TRUE(desks_bar_view->IsZeroState());
  PressAndReleaseKey(ui::VKEY_TAB);
  EXPECT_EQ(desks_bar_view->default_desk_button(), GetFocusedView());
  PressAndReleaseKey(ui::VKEY_TAB);
  EXPECT_EQ(desks_bar_view->new_desk_button(), GetFocusedView());

  // Test that when we create a new desk, we focus the desk name view of that
  // desk.
  PressAndReleaseKey(ui::VKEY_SPACE);
  EXPECT_EQ(desks_bar_view->mini_views()[1]->desk_name_view(),
            GetFocusedView());
}

TEST_P(DesksOverviewFocusCyclerTest, ClickingNameViewMovesFocus) {
  ToggleOverview();
  const auto* desk_bar_view =
      GetDesksBarViewForRoot(Shell::GetPrimaryRootWindow());
  CheckDeskBarViewSize(desk_bar_view, "initial");
  ASSERT_EQ(2u, desk_bar_view->mini_views().size());

  // Tab to first mini desk view's preview view.
  PressAndReleaseKey(ui::VKEY_TAB);
  ASSERT_EQ(desk_bar_view->mini_views()[0]->desk_preview(), GetFocusedView());
  CheckDeskBarViewSize(desk_bar_view, "tabbed once");

  // Click on the second mini desk item's name view.
  auto* event_generator = GetEventGenerator();
  auto* desk_name_view_1 = desk_bar_view->mini_views()[1]->desk_name_view();
  event_generator->MoveMouseTo(
      desk_name_view_1->GetBoundsInScreen().CenterPoint());
  event_generator->ClickLeftButton();
  EXPECT_FALSE(desk_bar_view->IsZeroState());

  // Verify that focus has moved to the clicked desk item.
  EXPECT_EQ(desk_name_view_1, GetFocusedView());
}

// Tests that there is no crash when tabbing after we switch to the zero state
// desks bar. Regression test for https://crbug.com/1301134.
TEST_P(DesksOverviewFocusCyclerTest, SwitchingToZeroStateWhileTabbing) {
  ToggleOverview();
  auto* desks_bar_view = GetDesksBarViewForRoot(Shell::GetPrimaryRootWindow());
  ASSERT_FALSE(desks_bar_view->IsZeroState());
  ASSERT_EQ(2u, desks_bar_view->mini_views().size());

  // Tab to first mini desk view's preview view.
  PressAndReleaseKey(ui::VKEY_TAB);
  ASSERT_EQ(desks_bar_view->mini_views()[0]->desk_preview(), GetFocusedView());

  // Remove one desk to have only one desk left.
  auto* mini_view = desks_bar_view->mini_views()[1].get();
  GetEventGenerator()->MoveMouseTo(
      mini_view->GetBoundsInScreen().CenterPoint());
  ASSERT_TRUE(GetDeskActionVisibilityForMiniView(mini_view));
  LeftClickOn(GetCloseDeskButtonForMiniView(mini_view));

  // Desks bar never goes back to zero state after it's initialized.
  ASSERT_FALSE(desks_bar_view->IsZeroState());

  // Try tabbing after removing the second desk triggers us to transition to
  // zero state desks bar. There should not be a crash.
  PressAndReleaseKey(ui::VKEY_TAB);
}

INSTANTIATE_TEST_SUITE_P(All, OverviewFocusCyclerTest, testing::Bool());
INSTANTIATE_TEST_SUITE_P(All, DesksOverviewFocusCyclerTest, testing::Bool());

}  // namespace ash
