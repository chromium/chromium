// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_POWER_POWER_BUTTON_MENU_VIEW_H_
#define ASH_SYSTEM_POWER_POWER_BUTTON_MENU_VIEW_H_

#include "ash/ash_export.h"
#include "ash/shutdown_reason.h"
#include "ash/system/power/power_button_controller.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/views/view.h"

namespace ash {
enum class PowerButtonMenuActionType;
class PowerButtonMenuItemView;
class SystemShadow;

// PowerButtonMenuView displays the menu items of the power button menu. It
// includes power off and sign out items currently.
class ASH_EXPORT PowerButtonMenuView : public views::View,
                                       public ui::ImplicitAnimationObserver {
  METADATA_HEADER(PowerButtonMenuView, views::View)

 public:
  // Direction of the animation transform. X means to translate from
  // x-coordinate. Y means to translate from y-coordinate.
  enum class TransformDirection { NONE, X, Y };

  // The translate direction and distance of the animation transform.
  struct TransformDisplacement {
    TransformDirection direction;
    int distance;
  };

  PowerButtonMenuView(
      ShutdownReason shutdown_reason,
      PowerButtonController::PowerButtonPosition power_button_position);
  PowerButtonMenuView(const PowerButtonMenuView&) = delete;
  PowerButtonMenuView& operator=(const PowerButtonMenuView&) = delete;
  ~PowerButtonMenuView() override;

  PowerButtonMenuItemView* sign_out_item_for_test() const {
    return sign_out_item_;
  }
  PowerButtonMenuItemView* power_off_item_for_test() const {
    return power_off_item_;
  }
  PowerButtonMenuItemView* lock_screen_item_for_test() const {
    return lock_screen_item_;
  }
  PowerButtonMenuItemView* capture_mode_item_for_test() const {
    return capture_mode_item_;
  }
  PowerButtonMenuItemView* feedback_item_for_test() const {
    return feedback_item_;
  }

  // Requests focus for |power_off_item_|.
  void FocusPowerOffButton();

  // Schedules an animation to show or hide the view.
  void ScheduleShowHideAnimation(bool show);

  // Gets the transform displacement, which contains direction and distance.
  TransformDisplacement GetTransformDisplacement() const;

  // Called whenever the associated widget is shown and when |this| is
  // constructed. Adds/removes menu items as needed.
  void RecreateItems();

 private:
  // views::View:
  void Layout(PassKey) override;
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override;

  void ButtonPressed(PowerButtonMenuActionType action,
                     base::RepeatingClosure callback);

  // Items in the menu. Owned by views hierarchy.
  raw_ptr<PowerButtonMenuItemView> power_off_item_ = nullptr;
  raw_ptr<PowerButtonMenuItemView> sign_out_item_ = nullptr;
  raw_ptr<PowerButtonMenuItemView> lock_screen_item_ = nullptr;
  raw_ptr<PowerButtonMenuItemView> capture_mode_item_ = nullptr;
  raw_ptr<PowerButtonMenuItemView> feedback_item_ = nullptr;

  ShutdownReason shutdown_reason_;
  // The physical display side of power button in landscape primary.
  PowerButtonController::PowerButtonPosition power_button_position_;

  std::unique_ptr<SystemShadow> shadow_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_POWER_POWER_BUTTON_MENU_VIEW_H_
