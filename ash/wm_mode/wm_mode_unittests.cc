// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ash/constants/ash_features.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_test_util.h"
#include "ash/wm_mode/pie_menu_view.h"
#include "ash/wm_mode/wm_mode_button_tray.h"
#include "ash/wm_mode/wm_mode_controller.h"
#include "base/containers/contains.h"
#include "base/test/scoped_feature_list.h"
#include "ui/base/models/image_model.h"
#include "ui/compositor/layer.h"
#include "ui/display/manager/display_manager.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/controls/image_view.h"

namespace ash {

class WmModeTests : public AshTestBase {
 public:
  WmModeTests() = default;
  WmModeTests(const WmModeTests&) = delete;
  WmModeTests& operator=(const WmModeTests&) = delete;
  ~WmModeTests() override = default;

  static WmModeButtonTray* GetWmModeButtonTrayForRoot(aura::Window* root) {
    auto* root_controller = RootWindowController::ForWindow(root);
    DCHECK(root_controller);
    return root_controller->GetStatusAreaWidget()->wm_mode_button_tray();
  }

  static bool IsRootWindowDimmed(aura::Window* root) {
    return WmModeController::Get()->dimmers_.contains(root);
  }

  static views::View* GetPieMenuButtonById(int button_id) {
    auto* controller = WmModeController::Get();
    CHECK(controller->pie_menu_view_);
    return controller->pie_menu_view_->GetButtonByIdAsView(button_id);
  }

  static PieMenuView* GetPieMenuView() {
    return WmModeController::Get()->pie_menu_view_;
  }

  static PieSubMenuContainerView* GetPieSubMenuContainerView(int button_id) {
    return GetPieMenuView()->GetOrAddSubMenuForButton(button_id);
  }

  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kWmMode);
    AshTestBase::SetUp();
  }

  void LeftClickPieMenuButton(int button_id) {
    auto* event_generator = GetEventGenerator();
    const auto button_center =
        GetPieMenuView()->GetButtonContentsCenterInScreen(button_id);
    event_generator->MoveMouseTo(button_center);
    event_generator->ClickLeftButton();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(WmModeTests, Basic) {
  auto* controller = WmModeController::Get();
  EXPECT_FALSE(controller->is_active());
  EXPECT_FALSE(controller->layer());
  controller->Toggle();
  EXPECT_TRUE(controller->is_active());
  EXPECT_TRUE(controller->layer());
  controller->Toggle();
  EXPECT_FALSE(controller->is_active());
  EXPECT_FALSE(controller->layer());
}

TEST_F(WmModeTests, ToggleFromTray) {
  auto* controller = WmModeController::Get();
  EXPECT_FALSE(controller->is_active());

  WmModeButtonTray* tray_button =
      GetWmModeButtonTrayForRoot(Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(tray_button);
  LeftClickOn(tray_button);
  EXPECT_TRUE(controller->is_active());

  LeftClickOn(tray_button);
  EXPECT_FALSE(controller->is_active());
}

TEST_F(WmModeTests, TraysOnMultipleDisplays) {
  display_manager()->AddRemoveDisplay();
  auto roots = Shell::GetAllRootWindows();
  EXPECT_EQ(roots.size(), 2u);

  auto* controller = WmModeController::Get();
  EXPECT_FALSE(controller->is_active());

  WmModeButtonTray* tray_button_1 = GetWmModeButtonTrayForRoot(roots[0]);
  WmModeButtonTray* tray_button_2 = GetWmModeButtonTrayForRoot(roots[1]);
  ASSERT_TRUE(tray_button_1);
  ASSERT_TRUE(tray_button_2);

  LeftClickOn(tray_button_2);
  EXPECT_TRUE(controller->is_active());
  EXPECT_TRUE(tray_button_1->is_active());
  EXPECT_TRUE(tray_button_2->is_active());

  // Returns true if `image_view` has the same `vector_icon` on its image model.
  auto has_same_vector_icon = [](const views::ImageView* image_view,
                                 const gfx::VectorIcon& vector_icon) -> bool {
    const auto image_model = image_view->GetImageModel();
    if (!image_model.IsVectorIcon() || image_model.GetVectorIcon().is_empty())
      return false;
    return std::string(vector_icon.name) ==
           std::string(image_model.GetVectorIcon().vector_icon()->name);
  };

  EXPECT_TRUE(has_same_vector_icon(tray_button_1->GetImageViewForTesting(),
                                   kWmModeOnIcon));
  EXPECT_TRUE(has_same_vector_icon(tray_button_2->GetImageViewForTesting(),
                                   kWmModeOnIcon));

  LeftClickOn(tray_button_1);
  EXPECT_FALSE(controller->is_active());
  EXPECT_FALSE(tray_button_1->is_active());
  EXPECT_FALSE(tray_button_2->is_active());

  EXPECT_TRUE(has_same_vector_icon(tray_button_1->GetImageViewForTesting(),
                                   kWmModeOffIcon));
  EXPECT_TRUE(has_same_vector_icon(tray_button_2->GetImageViewForTesting(),
                                   kWmModeOffIcon));
}

TEST_F(WmModeTests, ScreenDimming) {
  auto* controller = WmModeController::Get();
  EXPECT_FALSE(controller->is_active());
  auto roots = Shell::GetAllRootWindows();
  EXPECT_EQ(roots.size(), 1u);
  EXPECT_FALSE(IsRootWindowDimmed(roots[0]));

  controller->Toggle();
  EXPECT_TRUE(IsRootWindowDimmed(roots[0]));
  EXPECT_TRUE(roots[0]->layer()->Contains(controller->layer()));

  // Add a new display while the mode is active, and expect that it gets dimmed.
  display_manager()->AddRemoveDisplay();
  roots = Shell::GetAllRootWindows();
  EXPECT_EQ(roots.size(), 2u);
  EXPECT_TRUE(IsRootWindowDimmed(roots[0]));
  EXPECT_TRUE(IsRootWindowDimmed(roots[1]));

  // Deactivate the mode, and all displays are back to normal.
  controller->Toggle();
  EXPECT_FALSE(IsRootWindowDimmed(roots[0]));
  EXPECT_FALSE(IsRootWindowDimmed(roots[1]));
}

TEST_F(WmModeTests, WindowSelection) {
  // Create 2 displays with one window on each.
  UpdateDisplay("800x700,801+0-800x700");
  auto roots = Shell::GetAllRootWindows();
  EXPECT_EQ(roots.size(), 2u);
  auto win1 = CreateAppWindow(gfx::Rect(50, 60, 400, 400));
  auto win2 = CreateAppWindow(gfx::Rect(1000, 200, 400, 400));

  auto* controller = WmModeController::Get();
  controller->Toggle();
  EXPECT_TRUE(controller->is_active());

  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseToCenterOf(win2.get());
  event_generator->ClickLeftButton();
  EXPECT_EQ(controller->selected_window(), win2.get());
  EXPECT_TRUE(roots[1]->layer()->Contains(controller->layer()));

  // Clicking outside the bounds of any window will remove any window
  // selection. However, the layer remains parented to the same root window.
  event_generator->MoveMouseTo(win2->GetBoundsInScreen().bottom_right() +
                               gfx::Vector2d(20, 20));
  event_generator->ClickLeftButton();
  EXPECT_FALSE(controller->selected_window());
  EXPECT_TRUE(roots[1]->layer()->Contains(controller->layer()));

  // The layer will change roots once cursor moves to its display, and clicked
  // even if there is no selected window.
  event_generator->MoveMouseTo(gfx::Point(0, 0));
  event_generator->ClickLeftButton();
  EXPECT_FALSE(controller->selected_window());
  EXPECT_TRUE(roots[0]->layer()->Contains(controller->layer()));

  event_generator->MoveMouseToCenterOf(win1.get());
  event_generator->ClickLeftButton();
  EXPECT_EQ(controller->selected_window(), win1.get());
}

TEST_F(WmModeTests, RemovingSelectedRoot) {
  display_manager()->AddRemoveDisplay();
  auto roots = Shell::GetAllRootWindows();
  EXPECT_EQ(roots.size(), 2u);
  auto* controller = WmModeController::Get();
  controller->Toggle();
  EXPECT_TRUE(controller->is_active());

  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(roots[1]->GetBoundsInScreen().CenterPoint());
  event_generator->ClickLeftButton();
  EXPECT_TRUE(roots[1]->layer()->Contains(controller->layer()));

  // Remove the second display (which is currently selected), and expect that
  // the controller's layer will move to the primary display's root layer.
  display_manager()->AddRemoveDisplay();
  roots = Shell::GetAllRootWindows();
  EXPECT_EQ(roots.size(), 1u);
  EXPECT_TRUE(roots[0]->layer()->Contains(controller->layer()));

  controller->Toggle();
  EXPECT_FALSE(controller->is_active());
}

TEST_F(WmModeTests, PieMenuVisibility) {
  UpdateDisplay("800x700");
  auto roots = Shell::GetAllRootWindows();
  auto win1 = CreateAppWindow(gfx::Rect(400, 400));
  auto win2 = CreateAppWindow(gfx::Rect(400, 300, 400, 400));

  auto* controller = WmModeController::Get();
  controller->Toggle();
  EXPECT_TRUE(controller->is_active());
  // The pie menu should be created but not visible.
  ASSERT_TRUE(controller->pie_menu_widget());
  EXPECT_FALSE(controller->pie_menu_widget()->IsVisible());

  auto* event_generator = GetEventGenerator();

  for (auto* window : {win1.get(), win2.get()}) {
    event_generator->MoveMouseToCenterOf(window);
    event_generator->ClickLeftButton();
    EXPECT_EQ(controller->selected_window(), window);
    EXPECT_TRUE(controller->pie_menu_widget()->IsVisible());
    EXPECT_EQ(
        controller->pie_menu_widget()->GetWindowBoundsInScreen().CenterPoint(),
        window->GetBoundsInScreen().CenterPoint());
  }

  // Clicking outside the bounds of any window will remove any window selection.
  // However, the layer remains parented to the same root window.
  event_generator->MoveMouseTo(win1->GetBoundsInScreen().bottom_center() +
                               gfx::Vector2d(20, 20));
  event_generator->ClickLeftButton();
  EXPECT_FALSE(controller->selected_window());
  EXPECT_FALSE(controller->pie_menu_widget()->IsVisible());
}

TEST_F(WmModeTests, MoveToDeskSubMenu) {
  // Start with 2 desks.
  UpdateDisplay("800x700");
  NewDesk();
  auto* controller = WmModeController::Get();
  controller->Toggle();
  EXPECT_TRUE(controller->is_active());
  // The pie menu should be created but not visible.
  ASSERT_TRUE(controller->pie_menu_widget());
  EXPECT_FALSE(controller->pie_menu_widget()->IsVisible());

  // There should be 2 buttons on the move-to-desk sub menu, one for each
  // available desk.
  EXPECT_EQ(GetPieSubMenuContainerView(WmModeController::kMoveToDeskButtonId)
                ->button_count(),
            2u);

  // Add 2 new desks, the menu should be updated.
  NewDesk();
  NewDesk();
  EXPECT_EQ(GetPieSubMenuContainerView(WmModeController::kMoveToDeskButtonId)
                ->button_count(),
            4u);

  // Removing a desk will also be observed.
  auto* desks_controller = DesksController::Get();
  RemoveDesk(desks_controller->desks().back().get());
  EXPECT_EQ(GetPieSubMenuContainerView(WmModeController::kMoveToDeskButtonId)
                ->button_count(),
            3u);

  // Only the active desk button is disabled, since you can't move the window to
  // it.
  EXPECT_FALSE(
      GetPieMenuButtonById(WmModeController::kDeskButtonIdStart)->GetEnabled());
  EXPECT_TRUE(GetPieMenuButtonById(WmModeController::kDeskButtonIdStart + 1)
                  ->GetEnabled());
  EXPECT_TRUE(GetPieMenuButtonById(WmModeController::kDeskButtonIdStart + 2)
                  ->GetEnabled());

  // Switching desks will turn off WM Mode.
  ActivateDesk(desks_controller->desks().back().get());
  EXPECT_FALSE(controller->is_active());

  // Activate WM Mode again, and expect that the pie menu's active desk button
  // is disabled.
  controller->Toggle();
  EXPECT_TRUE(controller->is_active());

  EXPECT_TRUE(
      GetPieMenuButtonById(WmModeController::kDeskButtonIdStart)->GetEnabled());
  EXPECT_TRUE(GetPieMenuButtonById(WmModeController::kDeskButtonIdStart + 1)
                  ->GetEnabled());
  EXPECT_FALSE(GetPieMenuButtonById(WmModeController::kDeskButtonIdStart + 2)
                   ->GetEnabled());
}

TEST_F(WmModeTests, MoveWindowToDeskFromPieMenu) {
  // Start with 2 desks.
  UpdateDisplay("800x700");
  NewDesk();
  auto window = CreateAppWindow(gfx::Rect(400, 400));
  auto* controller = WmModeController::Get();
  controller->Toggle();
  EXPECT_TRUE(controller->is_active());
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseToCenterOf(window.get());
  event_generator->ClickLeftButton();
  EXPECT_EQ(controller->selected_window(), window.get());
  EXPECT_TRUE(controller->pie_menu_widget()->IsVisible());

  // The move-to-desk sub menu is initially hidden.
  auto* move_to_desk_sub_menu =
      GetPieSubMenuContainerView(WmModeController::kMoveToDeskButtonId);
  EXPECT_FALSE(move_to_desk_sub_menu->GetVisible());
  // Once the associated menu button is pressed, the sub menu is shown.
  LeftClickPieMenuButton(WmModeController::kMoveToDeskButtonId);
  EXPECT_TRUE(move_to_desk_sub_menu->GetVisible());

  // Clicking on the button for the second desk will move the window to that
  // desk.
  LeftClickPieMenuButton(WmModeController::kDeskButtonIdStart + 1);
  EXPECT_TRUE(base::Contains(DesksController::Get()->desks().back()->windows(),
                             window.get()));

  // The pie menu should have been hidden, and the selected window is now
  // cleared.
  EXPECT_TRUE(controller->is_active());
  EXPECT_FALSE(controller->selected_window());
  EXPECT_FALSE(controller->pie_menu_widget()->IsVisible());
}

}  // namespace ash
