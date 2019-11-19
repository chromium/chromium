// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_POWER_POWER_BUTTON_MENU_SCREEN_VIEW_H_
#define ASH_SYSTEM_POWER_POWER_BUTTON_MENU_SCREEN_VIEW_H_

#include <unordered_map>

#include "ash/ash_export.h"
#include "ash/display/screen_orientation_controller.h"
#include "ash/system/power/power_button_controller.h"
#include "base/macros.h"
#include "ui/display/display_observer.h"
#include "ui/views/view.h"

namespace ash {
class PowerButtonMenuView;

// PowerButtonMenuScreenView is the top-level view of power button menu UI. It
// creates a PowerButtonMenuBackgroundView to display the fullscreen background
// and a PowerButtonMenuView to display the menu.
class ASH_EXPORT PowerButtonMenuScreenView : public views::View,
                                             public display::DisplayObserver {
 public:
  // |show_animation_done| is a callback for when the animation that shows the
  // power menu has finished.
  PowerButtonMenuScreenView(
      PowerButtonController::PowerButtonPosition power_button_position,
      double power_button_offset,
      base::RepeatingClosure show_animation_done);
  ~PowerButtonMenuScreenView() override;

  PowerButtonMenuView* power_button_menu_view() const {
    return power_button_menu_view_;
  }

  // Schedules an animation to show or hide the view.
  void ScheduleShowHideAnimation(bool show);

  // views::View:
  const char* GetClassName() const override;

 private:
  class PowerButtonMenuBackgroundView;

  // views::View:
  void Layout() override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;

  // ui::EventHandler:
  void OnGestureEvent(ui::GestureEvent* event) override;

  // display::DisplayObserver:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

  // Lays out the view without animation transform.
  void LayoutWithoutTransform();

  // Initializes |menu_bounds_origins_| according to power button position info.
  void InitializeMenuBoundsOrigins();

  // Gets the bounds of power button menu.
  gfx::Rect GetMenuBounds();

  // Created by PowerButtonMenuScreenView. Owned by views hierarchy.
  PowerButtonMenuView* power_button_menu_view_ = nullptr;
  PowerButtonMenuBackgroundView* power_button_screen_background_shield_ =
      nullptr;

  // The physical display side of power button in landscape primary.
  PowerButtonController::PowerButtonPosition power_button_position_;

  // The center of the power button's offset from the top of the screen (for
  // left/right) or left side of the screen (for top/bottom) in
  // landscape_primary. Values are in [0.0, 1.0] and express a fraction of the
  // display's height or width, respectively.
  double power_button_offset_percentage_ = 0.f;

  // The origin of the menu bounds in different screen orientations.
  std::unordered_map<OrientationLockType, gfx::Point> menu_bounds_origins_;

  DISALLOW_COPY_AND_ASSIGN(PowerButtonMenuScreenView);
};

}  // namespace ash

#endif  // ASH_SYSTEM_POWER_POWER_BUTTON_MENU_SCREEN_VIEW_H_
