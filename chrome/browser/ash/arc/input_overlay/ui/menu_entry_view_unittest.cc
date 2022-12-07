// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/menu_entry_view.h"

#include <string>

#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "base/test/bind.h"
#include "chrome/browser/ash/arc/input_overlay/constants.h"
#include "chrome/browser/ash/arc/input_overlay/display_overlay_controller.h"
#include "chrome/browser/ash/arc/input_overlay/test/arc_test_window.h"
#include "chrome/browser/ash/arc/input_overlay/touch_injector.h"
#include "chrome/browser/ash/arc/input_overlay/ui/input_menu_view.h"
#include "components/exo/test/exo_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/test/geometry_util.h"

namespace arc::input_overlay {

namespace {
// Consider two points are at the same position within kTolerance.
constexpr const float kTolerance = 0.999f;
}  // namespace

class MenuEntryViewTest : public exo::test::ExoTestBase {
 protected:
  MenuEntryViewTest() = default;
  ~MenuEntryViewTest() override = default;

  void PressLeftMouseAtMenuEntryView() {
    // Press down at the center of the menu entry view.
    menu_entry_view_ = display_overlay_controller_->GetMenuEntryView();
    local_location_ = menu_entry_view_->bounds().CenterPoint();
    menu_entry_view_->OnMousePressed(
        ui::MouseEvent(ui::ET_MOUSE_PRESSED, local_location_, local_location_,
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                       ui::EF_LEFT_MOUSE_BUTTON));
  }

  void TouchPressAtMenuEntryView() {
    menu_entry_view_ = display_overlay_controller_->GetMenuEntryView();
    local_location_ = menu_entry_view_->bounds().CenterPoint();
    auto scroll_begin = ui::GestureEvent(
        local_location_.x(), local_location_.y(), ui::EF_NONE,
        ui::EventTimeForNow(),
        ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_BEGIN, 0, 0));
    menu_entry_view_->OnGestureEvent(&scroll_begin);
  }

  void MouseDragMenuEntryViewBy(const gfx::Vector2d& move) {
    menu_entry_view_ = display_overlay_controller_->GetMenuEntryView();
    local_location_ += move;
    menu_entry_view_->OnMouseDragged(
        ui::MouseEvent(ui::ET_MOUSE_DRAGGED, local_location_, local_location_,
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0));
  }

  void TouchMoveAtMenuEntryViewBy(const gfx::Vector2d& move) {
    local_location_ += move;
    auto scroll_begin =
        ui::GestureEvent(local_location_.x(), local_location_.y(), ui::EF_NONE,
                         ui::EventTimeForNow(),
                         ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_UPDATE,
                                                 move.x(), move.y()));
    menu_entry_view_->OnGestureEvent(&scroll_begin);
  }

  void ReleaseLeftMouseAtMenuEntryView() {
    menu_entry_view_ = display_overlay_controller_->GetMenuEntryView();
    menu_entry_view_->OnMouseReleased(
        ui::MouseEvent(ui::ET_MOUSE_RELEASED, local_location_, local_location_,
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                       ui::EF_LEFT_MOUSE_BUTTON));
  }

  void TouchReleaseAtMenuEntryView() {
    auto scroll_end =
        ui::GestureEvent(local_location_.x(), local_location_.y(), ui::EF_NONE,
                         ui::EventTimeForNow(),
                         ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_END));
    menu_entry_view_->OnGestureEvent(&scroll_end);
  }

  void TapAtMenuEntryView() {
    auto scroll_end = ui::GestureEvent(
        local_location_.x(), local_location_.y(), ui::EF_NONE,
        ui::EventTimeForNow(), ui::GestureEventDetails(ui::ET_GESTURE_TAP));
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
    display_overlay_controller_->SetDisplayMode(DisplayMode::kView);
  }

  void SimulateMinimizeAndRestoreApp() {
    display_overlay_controller_.reset();
    display_overlay_controller_ =
        std::make_unique<DisplayOverlayController>(touch_injector_.get(),
                                                   /*first_launch=*/false);
    menu_entry_view_ = display_overlay_controller_->GetMenuEntryView();
    menu_entry_view_->set_allow_reposition(true);
  }

  // Kept around to determine bounds(), not owned.
  raw_ptr<MenuEntryView> menu_entry_view_;
  // Used to simulate mouse actions at a particular location, to be changed upon
  // dragging.
  gfx::Point local_location_;

 private:
  void SetUp() override {
    exo::test::ExoTestBase::SetUp();
    arc_test_window_ = std::make_unique<test::ArcTestWindow>(
        exo_test_helper(), ash::Shell::GetPrimaryRootWindow(),
        "org.chromium.arc.testapp.inputoverlay");
    touch_injector_ = std::make_unique<TouchInjector>(
        arc_test_window_->GetWindow(),
        *arc_test_window_->GetWindow()->GetProperty(ash::kArcPackageNameKey),
        base::BindLambdaForTesting(
            [&](std::unique_ptr<AppDataProto>, std::string) {}));
    touch_injector_->set_allow_reposition(true);
    display_overlay_controller_ = std::make_unique<DisplayOverlayController>(
        touch_injector_.get(), /*first_launch=*/false);
    menu_entry_view_ = display_overlay_controller_->GetMenuEntryView();
    menu_entry_view_->set_allow_reposition(true);
  }

  void TearDown() override {
    menu_entry_view_ = nullptr;
    display_overlay_controller_.reset();
    touch_injector_.reset();
    arc_test_window_.reset();
    exo::test::ExoTestBase::TearDown();
  }

  std::unique_ptr<TouchInjector> touch_injector_;
  std::unique_ptr<test::ArcTestWindow> arc_test_window_;
  std::unique_ptr<DisplayOverlayController> display_overlay_controller_;
};

TEST_F(MenuEntryViewTest, RepositionTest) {
  // Get initial positions.
  auto bounds = GetInputMappingViewBounds();
  auto initial_pos = menu_entry_view_->bounds().CenterPoint();
  auto move_vector = gfx::Vector2d(5, 5);
  // Drag move by mouse.
  PressLeftMouseAtMenuEntryView();
  MouseDragMenuEntryViewBy(move_vector);
  ReleaseLeftMouseAtMenuEntryView();
  // Check that input menu view does not open as a result of mouse dragging.
  EXPECT_FALSE(DisplayControllerHasInputMenuView());
  // Verify that the initial position is within expectation.
  auto final_pos = menu_entry_view_->bounds().CenterPoint();
  EXPECT_POINTF_NEAR(gfx::PointF(final_pos),
                     gfx::PointF(initial_pos + move_vector), kTolerance);
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
  initial_pos = menu_entry_view_->bounds().CenterPoint();
  move_vector = gfx::Vector2d(-5, -5);
  // Drag move by touch.
  TouchPressAtMenuEntryView();
  TouchMoveAtMenuEntryViewBy(move_vector);
  TouchReleaseAtMenuEntryView();
  // Check that input menu view does not open as a result of touch dragging.
  EXPECT_FALSE(DisplayControllerHasInputMenuView());
  // Verify that the initial position is within expectation.
  final_pos = menu_entry_view_->bounds().CenterPoint();
  EXPECT_POINTF_NEAR(gfx::PointF(final_pos),
                     gfx::PointF(initial_pos + move_vector), kTolerance);
  // Tap menu entry view.
  TapAtMenuEntryView();
  // Check that input menu view exists as a result of a touch.
  EXPECT_TRUE(DisplayControllerHasInputMenuView());
  // Check that resulting input menu view is not offscreen.
  input_menu_view = GetInputMenuFromDisplayController();
  EXPECT_LE(input_menu_view->y() + input_menu_view->height(), bounds.height());
}

TEST_F(MenuEntryViewTest, PersistentPositionTest) {
  // Move menu entry to another location.
  auto initial_pos = menu_entry_view_->bounds().CenterPoint();
  auto move_vector = gfx::Vector2d(5, 5);
  PressLeftMouseAtMenuEntryView();
  MouseDragMenuEntryViewBy(move_vector);
  ReleaseLeftMouseAtMenuEntryView();
  // Verify that the resulting position is within expectation.
  auto final_pos = menu_entry_view_->bounds().CenterPoint();
  auto expected_pos = initial_pos + move_vector;
  EXPECT_POINTF_NEAR(gfx::PointF(final_pos), gfx::PointF(expected_pos),
                     kTolerance);

  // Simulate minimizing and restoring the test application.
  SimulateMinimizeAndRestoreApp();

  // Check that the position of the menu entry view persisted from the last
  // customization.
  final_pos = menu_entry_view_->bounds().CenterPoint();
  EXPECT_POINTF_NEAR(gfx::PointF(final_pos), gfx::PointF(expected_pos),
                     kTolerance);
}

}  // namespace arc::input_overlay
