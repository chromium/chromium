// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/menu_entry_view.h"

#include <string>

#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/arc/input_overlay/constants.h"
#include "chrome/browser/ash/arc/input_overlay/display_overlay_controller.h"
#include "chrome/browser/ash/arc/input_overlay/test/arc_test_window.h"
#include "chrome/browser/ash/arc/input_overlay/touch_injector.h"
#include "chrome/browser/ash/arc/input_overlay/ui/input_menu_view.h"
#include "chrome/browser/ash/arc/input_overlay/util.h"
#include "components/exo/test/exo_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/test/geometry_util.h"

namespace arc::input_overlay {

class MenuEntryViewTest : public exo::test::ExoTestBase {
 protected:
  MenuEntryViewTest() = default;
  ~MenuEntryViewTest() override = default;

  void PressLeftMouseAtMenuEntryView() {
    // Press down at the center of the menu entry view.
    menu_entry_view_ = display_overlay_controller_->GetMenuEntryView();
    local_location_ = menu_entry_view_->bounds().CenterPoint();
    menu_entry_view_->OnMousePressed(
        ui::MouseEvent(ui::EventType::kMousePressed, local_location_,
                       local_location_, ui::EventTimeForNow(),
                       ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));
  }

  void TouchPressAtMenuEntryView() {
    menu_entry_view_ = display_overlay_controller_->GetMenuEntryView();
    local_location_ = menu_entry_view_->bounds().CenterPoint();
    auto scroll_begin = ui::GestureEvent(
        local_location_.x(), local_location_.y(), ui::EF_NONE,
        ui::EventTimeForNow(),
        ui::GestureEventDetails(ui::EventType::kGestureScrollBegin, 0, 0));
    menu_entry_view_->OnGestureEvent(&scroll_begin);
  }

  void MouseDragMenuEntryViewBy(const gfx::Vector2d& move) {
    menu_entry_view_ = display_overlay_controller_->GetMenuEntryView();
    local_location_ += move;
    menu_entry_view_->OnMouseDragged(ui::MouseEvent(
        ui::EventType::kMouseDragged, local_location_, local_location_,
        ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0));
  }

  void TouchMoveAtMenuEntryViewBy(const gfx::Vector2d& move) {
    local_location_ += move;
    auto scroll_begin = ui::GestureEvent(
        local_location_.x(), local_location_.y(), ui::EF_NONE,
        ui::EventTimeForNow(),
        ui::GestureEventDetails(ui::EventType::kGestureScrollUpdate, move.x(),
                                move.y()));
    menu_entry_view_->OnGestureEvent(&scroll_begin);
  }

  void ReleaseLeftMouseAtMenuEntryView() {
    menu_entry_view_ = display_overlay_controller_->GetMenuEntryView();
    menu_entry_view_->OnMouseReleased(
        ui::MouseEvent(ui::EventType::kMouseReleased, gfx::Point(),
                       gfx::Point(), ui::EventTimeForNow(),
                       ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));
  }

  void TouchReleaseAtMenuEntryView() {
    auto scroll_end = ui::GestureEvent(
        local_location_.x(), local_location_.y(), ui::EF_NONE,
        ui::EventTimeForNow(),
        ui::GestureEventDetails(ui::EventType::kGestureScrollEnd));
    menu_entry_view_->OnGestureEvent(&scroll_end);
  }

  void TapAtMenuEntryView() {
    auto scroll_end =
        ui::GestureEvent(local_location_.x(), local_location_.y(), ui::EF_NONE,
                         ui::EventTimeForNow(),
                         ui::GestureEventDetails(ui::EventType::kGestureTap));
    menu_entry_view_->OnGestureEvent(&scroll_end);
  }

  bool DisplayControllerHasInputMenuView() {
    return display_overlay_controller_->HasMenuView();
  }

  gfx::Rect GetInputMappingViewBounds() {
    return display_overlay_controller_->GetInputMappingViewBoundsForTesting();
  }

  InputMenuView* GetInputMenuFromDisplayController() {
    return display_overlay_controller_->GetInputMenuView();
  }

  void CloseInputMenuView() {
    display_overlay_controller_->SetDisplayModeAlpha(DisplayMode::kView);
  }

  void SimulateMinimizeAndRestoreApp() {
    display_overlay_controller_.reset();
    display_overlay_controller_ =
        std::make_unique<DisplayOverlayController>(touch_injector_.get(),
                                                   /*first_launch=*/false);
    menu_entry_view_ = display_overlay_controller_->GetMenuEntryView();
  }

  // Kept around to determine bounds(), not owned.
  raw_ptr<MenuEntryView, DanglingUntriaged> menu_entry_view_;
  // Used to simulate mouse actions at a particular location, to be changed upon
  // dragging.
  gfx::Point local_location_;

 private:
  void SetUp() override {
    scoped_feature_list_.InitAndDisableFeature(ash::features::kGameDashboard);
    exo::test::ExoTestBase::SetUp();
    arc_test_window_ = std::make_unique<test::ArcTestWindow>(
        exo_test_helper(), ash::Shell::GetPrimaryRootWindow(),
        "org.chromium.arc.testapp.inputoverlay", gfx::Rect(10, 10, 250, 250));
    touch_injector_ = std::make_unique<TouchInjector>(
        arc_test_window_->GetWindow(),
        *arc_test_window_->GetWindow()->GetProperty(ash::kArcPackageNameKey),
        base::BindLambdaForTesting(
            [&](std::unique_ptr<AppDataProto>, std::string) {}));
    display_overlay_controller_ = std::make_unique<DisplayOverlayController>(
        touch_injector_.get(), /*first_launch=*/false);
    menu_entry_view_ = display_overlay_controller_->GetMenuEntryView();
  }

  void TearDown() override {
    menu_entry_view_ = nullptr;
    display_overlay_controller_.reset();
    touch_injector_.reset();
    arc_test_window_.reset();
    exo::test::ExoTestBase::TearDown();
  }

  base::test::ScopedFeatureList scoped_feature_list_;

  std::unique_ptr<TouchInjector> touch_injector_;
  std::unique_ptr<test::ArcTestWindow> arc_test_window_;
  std::unique_ptr<DisplayOverlayController> display_overlay_controller_;
};

TEST_F(MenuEntryViewTest, TestDragMove) {
  // Get initial positions.
  auto bounds = GetInputMappingViewBounds();
  auto initial_pos = menu_entry_view_->origin();
  auto move_vector = gfx::Vector2d(5, 5);
  // Drag move by mouse.
  PressLeftMouseAtMenuEntryView();
  MouseDragMenuEntryViewBy(move_vector);
  ReleaseLeftMouseAtMenuEntryView();
  // Check that input menu view does not open as a result of mouse dragging.
  EXPECT_FALSE(DisplayControllerHasInputMenuView());
  // Verify that the initial position is within expectation.
  EXPECT_EQ(menu_entry_view_->origin(), initial_pos + move_vector);
  // Click menu entry view.
  PressLeftMouseAtMenuEntryView();
  ReleaseLeftMouseAtMenuEntryView();
  // Check that resulting input menu view is not offscreen.
  auto* input_menu_view = GetInputMenuFromDisplayController();
  EXPECT_NE(input_menu_view, nullptr);
  EXPECT_LE(input_menu_view->y() + input_menu_view->height(), bounds.height());

  // Close the input menu view.
  CloseInputMenuView();

  // Get initial positions again.
  initial_pos = menu_entry_view_->origin();
  move_vector = gfx::Vector2d(-5, -5);
  // Drag move by touch.
  TouchPressAtMenuEntryView();
  TouchMoveAtMenuEntryViewBy(move_vector);
  TouchReleaseAtMenuEntryView();
  // Check that input menu view does not open as a result of touch dragging.
  EXPECT_FALSE(DisplayControllerHasInputMenuView());
  // Verify that the initial position is within expectation.
  EXPECT_EQ(menu_entry_view_->origin(), initial_pos + move_vector);
  // Tap menu entry view.
  TapAtMenuEntryView();
  // Check that input menu view exists as a result of a touch.
  EXPECT_TRUE(DisplayControllerHasInputMenuView());
  // Check that resulting input menu view is not offscreen.
  input_menu_view = GetInputMenuFromDisplayController();
  EXPECT_LE(input_menu_view->y() + input_menu_view->height(), bounds.height());
}

TEST_F(MenuEntryViewTest, TestArrowKeyMove) {
  // Arrow key left single press & release.
  auto updated_pos = menu_entry_view_->origin();
  menu_entry_view_->OnKeyPressed(
      ui::KeyEvent(ui::EventType::kKeyPressed, ui::VKEY_LEFT, ui::EF_NONE));
  menu_entry_view_->OnKeyReleased(
      ui::KeyEvent(ui::EventType::kKeyReleased, ui::VKEY_LEFT, ui::EF_NONE));
  auto move_left = gfx::Vector2d(-kArrowKeyMoveDistance, 0);
  updated_pos += move_left;
  EXPECT_EQ(updated_pos, menu_entry_view_->origin());

  // Arrow key down single press & release.
  updated_pos = menu_entry_view_->origin();
  menu_entry_view_->OnKeyPressed(
      ui::KeyEvent(ui::EventType::kKeyPressed, ui::VKEY_DOWN, ui::EF_NONE));
  menu_entry_view_->OnKeyReleased(
      ui::KeyEvent(ui::EventType::kKeyReleased, ui::VKEY_DOWN, ui::EF_NONE));
  auto move_down = gfx::Vector2d(0, kArrowKeyMoveDistance);
  updated_pos += move_down;
  EXPECT_EQ(updated_pos, menu_entry_view_->origin());

  // Arrow key right single press & release.
  updated_pos = menu_entry_view_->origin();
  int key_press_times = 5;
  auto move_right = gfx::Vector2d(kArrowKeyMoveDistance, 0);
  for (int i = 0; i < key_press_times; i++) {
    menu_entry_view_->OnKeyPressed(
        ui::KeyEvent(ui::EventType::kKeyPressed, ui::VKEY_RIGHT, ui::EF_NONE));
    updated_pos += move_right;
  }
  menu_entry_view_->OnKeyReleased(
      ui::KeyEvent(ui::EventType::kKeyReleased, ui::VKEY_RIGHT, ui::EF_NONE));
  EXPECT_EQ(updated_pos, menu_entry_view_->origin());
}

TEST_F(MenuEntryViewTest, TestPersistentPosition) {
  // Move menu entry to another location.
  auto initial_pos = menu_entry_view_->origin();
  auto move_vector = gfx::Vector2d(5, 5);
  PressLeftMouseAtMenuEntryView();
  MouseDragMenuEntryViewBy(move_vector);
  ReleaseLeftMouseAtMenuEntryView();
  // Verify that the resulting position is within expectation.
  auto expected_pos = initial_pos + move_vector;
  EXPECT_EQ(menu_entry_view_->origin(), expected_pos);

  // Simulate minimizing and restoring the test application.
  SimulateMinimizeAndRestoreApp();

  // Check that the position of the menu entry view persisted from the last
  // customization.
  EXPECT_EQ(menu_entry_view_->origin(), expected_pos);
}

}  // namespace arc::input_overlay
