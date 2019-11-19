// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_POWER_POWER_BUTTON_MENU_VIEW_H_
#define ASH_SYSTEM_POWER_POWER_BUTTON_MENU_VIEW_H_

#include "ash/ash_export.h"
#include "ash/system/power/power_button_controller.h"
#include "base/macros.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace ash {
class PowerButtonMenuItemView;

// PowerButtonMenuView displays the menu items of the power button menu. It
// includes power off and sign out items currently.
class ASH_EXPORT PowerButtonMenuView : public views::View,
                                       public views::ButtonListener,
                                       public ui::ImplicitAnimationObserver {
 public:
  // The duration of showing or dismissing power button menu animation.
  static constexpr base::TimeDelta kMenuAnimationDuration =
      base::TimeDelta::FromMilliseconds(250);

  // Distance of the menu animation transform.
  static constexpr int kMenuViewTransformDistanceDp = 16;

  // Direction of the animation transform. X means to translate from
  // x-coordinate. Y means to translate from y-coordinate.
  enum class TransformDirection { NONE, X, Y };

  // The translate direction and distance of the animation transform.
  struct TransformDisplacement {
    TransformDirection direction;
    int distance;
  };

  explicit PowerButtonMenuView(
      PowerButtonController::PowerButtonPosition power_button_position);
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
  PowerButtonMenuItemView* feedback_item_for_test() const {
    return feedback_item_;
  }

  // Requests focus for |power_off_item_|.
  void FocusPowerOffButton();

  // Schedules an animation to show or hide the view.
  void ScheduleShowHideAnimation(bool show);

  // Gets the transform displacement, which contains direction and distance.
  TransformDisplacement GetTransformDisplacement() const;

  // views::View:
  const char* GetClassName() const override;

 private:
  // Creates the items that in the menu.
  void CreateItems();

  // views::View:
  void Layout() override;
  void OnPaint(gfx::Canvas* canvas) override;
  gfx::Size CalculatePreferredSize() const override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override;

  // Items in the menu. Owned by views hierarchy.
  PowerButtonMenuItemView* power_off_item_ = nullptr;
  PowerButtonMenuItemView* sign_out_item_ = nullptr;
  PowerButtonMenuItemView* lock_screen_item_ = nullptr;
  PowerButtonMenuItemView* feedback_item_ = nullptr;

  // The physical display side of power button in landscape primary.
  PowerButtonController::PowerButtonPosition power_button_position_;

  DISALLOW_COPY_AND_ASSIGN(PowerButtonMenuView);
};

}  // namespace ash

#endif  // ASH_SYSTEM_POWER_POWER_BUTTON_MENU_VIEW_H_
