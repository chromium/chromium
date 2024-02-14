// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_POWER_POWER_BUTTON_MENU_SCREEN_VIEW_H_
#define ASH_SYSTEM_POWER_POWER_BUTTON_MENU_SCREEN_VIEW_H_

#include <unordered_map>

#include "ash/ash_export.h"
#include "ash/display/screen_orientation_controller.h"
#include "ash/system/power/power_button_controller.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/display/display_observer.h"
#include "ui/views/view.h"

namespace ash {
enum class ShutdownReason;
class PowerButtonMenuView;
class PowerButtonMenuCurtainView;

// PowerButtonMenuScreenView is the top-level view of power button menu UI. It
// creates a PowerButtonMenuBackgroundView to display the fullscreen background
// and a PowerButtonMenuView to display the menu.
class ASH_EXPORT PowerButtonMenuScreenView : public views::View,
                                             public display::DisplayObserver {
  METADATA_HEADER(PowerButtonMenuScreenView, views::View)

 public:
  // |show_animation_done| is a callback for when the animation that shows the
  // power menu has finished.
  PowerButtonMenuScreenView(
      ShutdownReason shutdown_reason,
      PowerButtonController::PowerButtonPosition power_button_position,
      double power_button_offset,
      base::RepeatingClosure show_animation_done);
  PowerButtonMenuScreenView(const PowerButtonMenuScreenView&) = delete;
  PowerButtonMenuScreenView& operator=(const PowerButtonMenuScreenView&) =
      delete;
  ~PowerButtonMenuScreenView() override;

  PowerButtonMenuView* power_button_menu_view() const {
    return power_button_menu_view_;
  }

  PowerButtonMenuCurtainView* power_button_menu_curtain_view() const {
    return power_button_menu_curtain_view_;
  }

  // Schedules an animation to show or hide the view.
  void ScheduleShowHideAnimation(bool show);

  // Resets the shield and menu's opacity to 0. Used when dismissing the menu
  // without animation to prepare for the next fade in animation.
  void ResetOpacity();

  // Called when the associated widget is shown. Updates power button related
  // info and calculates |menu_bounds_origins_| if needed. Recreates menu items.
  void OnWidgetShown(PowerButtonController::PowerButtonPosition position,
                     double offset_percentage);

 private:
  class PowerButtonMenuBackgroundView;

  // views::View:
  void Layout(PassKey) override;
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

  // Helper methods for performing layout.
  void LayoutMenuView();
  void LayoutMenuCurtainView();

  // Updates |menu_bounds_origins_| according to power button position info.
  void UpdateMenuBoundsOrigins();

  // Gets the bounds of power button menu.
  gfx::Rect GetMenuBounds();

  gfx::Size GetMenuViewPreferredSize();

  PowerButtonMenuCurtainView* GetOrCreateCurtainView();

  // Created by PowerButtonMenuScreenView. Owned by views hierarchy. Only
  // power_button_menu_view_ or power_button_menu_curtain_view_ will be
  // available at a time.
  raw_ptr<PowerButtonMenuView> power_button_menu_view_ = nullptr;
  raw_ptr<PowerButtonMenuCurtainView> power_button_menu_curtain_view_ = nullptr;

  raw_ptr<PowerButtonMenuBackgroundView>
      power_button_screen_background_shield_ = nullptr;

  // The physical display side of power button in landscape primary.
  PowerButtonController::PowerButtonPosition power_button_position_;

  // The center of the power button's offset from the top of the screen (for
  // left/right) or left side of the screen (for top/bottom) in
  // landscape_primary. Values are in [0.0, 1.0] and express a fraction of the
  // display's height or width, respectively.
  double power_button_offset_percentage_ = 0.f;

  // The origin of the menu bounds in different screen orientations.
  std::unordered_map<chromeos::OrientationType, gfx::Point>
      menu_bounds_origins_;

  display::ScopedDisplayObserver display_observer_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_POWER_POWER_BUTTON_MENU_SCREEN_VIEW_H_
