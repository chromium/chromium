// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/work_area_insets.h"

#include "ash/display/window_tree_host_manager.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/root_window_controller.h"
#include "ash/screen_util.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shell.h"
#include "base/auto_reset.h"
#include "ui/aura/window.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

// Returns work area insets calculated for the provided parameters.
gfx::Insets CalculateWorkAreaInsets(const gfx::Insets& accessibility_insets,
                                    const gfx::Insets& shelf_insets,
                                    const gfx::Rect& keyboard_bounds) {
  gfx::Insets work_area_insets;
  work_area_insets += accessibility_insets;
  // The virtual keyboard always hides the shelf (in any orientation).
  // Therefore, if the keyboard is shown, there is no need to reduce the work
  // area by the size of the shelf.
  if (keyboard_bounds.IsEmpty())
    work_area_insets += shelf_insets;
  else
    work_area_insets += gfx::Insets::TLBR(0, 0, keyboard_bounds.height(), 0);
  return work_area_insets;
}

// Returns work area bounds calculated for the given |window| and given
// parameters.
gfx::Rect CalculateWorkAreaBounds(const gfx::Insets& accessibility_insets,
                                  const gfx::Rect& shelf_bounds_in_screen,
                                  const gfx::Rect& keyboard_bounds_in_screen,
                                  aura::Window* window) {
  gfx::Rect work_area_bounds = screen_util::GetDisplayBoundsWithShelf(window);
  work_area_bounds.Inset(accessibility_insets);
  work_area_bounds.Subtract(shelf_bounds_in_screen);
  work_area_bounds.Subtract(keyboard_bounds_in_screen);
  return work_area_bounds;
}

}  // namespace

// static
WorkAreaInsets* WorkAreaInsets::ForWindow(const aura::Window* window) {
  return RootWindowController::ForWindow(window)->work_area_insets();
}

// static
void WorkAreaInsets::UpdateWorkAreaInsetsForTest(
    aura::Window* window,
    const gfx::Rect& shelf_bounds_for_workarea_calculation,
    const gfx::Insets& shelf_insets,
    const gfx::Insets& in_session_shelf_insets) {
  DCHECK(window);
  Shelf::ForWindow(window)
      ->shelf_layout_manager()
      ->UpdateWorkAreaInsetsAndNotifyObservers(
          shelf_bounds_for_workarea_calculation, shelf_insets,
          in_session_shelf_insets);
}

WorkAreaInsets::WorkAreaInsets(RootWindowController* root_window_controller)
    : root_window_controller_(root_window_controller) {
  keyboard::KeyboardUIController::Get()->AddObserver(this);
}

WorkAreaInsets::~WorkAreaInsets() {
  keyboard::KeyboardUIController::Get()->RemoveObserver(this);
}

gfx::Insets WorkAreaInsets::GetAccessibilityInsets() const {
  return gfx::Insets::TLBR(
      accessibility_panel_height_ + docked_magnifier_height_, 0, 0, 0);
}

gfx::Rect WorkAreaInsets::ComputeStableWorkArea() const {
  aura::Window* root_window = root_window_controller_->GetRootWindow();

  // The ideal shelf bounds are not in screen coordinates.
  gfx::Rect shelf_bounds_in_screen(
      root_window_controller_->shelf()->GetIdealBoundsForWorkAreaCalculation());
  ::wm::ConvertRectToScreen(root_window, &shelf_bounds_in_screen);

  return CalculateWorkAreaBounds(GetAccessibilityInsets(),
                                 shelf_bounds_in_screen,
                                 keyboard_displaced_bounds_, root_window);
}

bool WorkAreaInsets::IsKeyboardShown() const {
  return !keyboard_displaced_bounds_.IsEmpty();
}

void WorkAreaInsets::SetDockedMagnifierHeight(int height) {
  docked_magnifier_height_ = height;
  UpdateWorkArea();
  Shell::Get()->NotifyUserWorkAreaInsetsChanged(
      root_window_controller_->GetRootWindow());
}

void WorkAreaInsets::SetAccessibilityPanelHeight(int height) {
  accessibility_panel_height_ = height;
  UpdateWorkArea();
  Shell::Get()->NotifyUserWorkAreaInsetsChanged(
      root_window_controller_->GetRootWindow());
}

void WorkAreaInsets::SetShelfBoundsAndInsets(
    const gfx::Rect& shelf_bounds,
    const gfx::Insets& insets,
    const gfx::Insets& in_session_insets) {
  shelf_bounds_ = shelf_bounds;
  shelf_insets_ = insets;

  in_session_shelf_insets_ = in_session_insets;

  UpdateWorkArea();
}

void WorkAreaInsets::OnKeyboardAppearanceChanged(
    const KeyboardStateDescriptor& state) {
  aura::Window* window = root_window_controller_->GetRootWindow();

  keyboard_occluded_bounds_ = state.occluded_bounds_in_screen;
  keyboard_displaced_bounds_ = state.displaced_bounds_in_screen;

  UpdateWorkArea();
  Shell::Get()->NotifyUserWorkAreaInsetsChanged(window);
}

void WorkAreaInsets::OnKeyboardVisibilityChanged(const bool is_visible) {
  // On login screen if keyboard has been just hidden, update bounds just once
  // but ignore work area insets since shelf overlaps with login window.
  if (Shell::Get()->session_controller()->IsUserSessionBlocked() &&
      !is_visible) {
    Shell::Get()
        ->window_tree_host_manager()
        ->UpdateWorkAreaOfDisplayNearestWindow(
            root_window_controller_->GetRootWindow(), gfx::Insets());
  }
}

void WorkAreaInsets::UpdateWorkArea() {
  // Note: Different keyboard bounds properties are used to calculate insets and
  // bounds. See ui/keyboard/keyboard_controller_observer.h for details.
  user_work_area_insets_ = CalculateWorkAreaInsets(
      GetAccessibilityInsets(), shelf_insets_, keyboard_displaced_bounds_);
  user_work_area_bounds_ = CalculateWorkAreaBounds(
      GetAccessibilityInsets(), shelf_bounds_, keyboard_occluded_bounds_,
      root_window_controller_->GetRootWindow());

  in_session_user_work_area_insets_ = CalculateWorkAreaInsets(
      GetAccessibilityInsets(), in_session_shelf_insets_,
      keyboard_displaced_bounds_);
}

}  // namespace ash
