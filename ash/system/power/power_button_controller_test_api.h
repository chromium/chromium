// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_POWER_POWER_BUTTON_CONTROLLER_TEST_API_H_
#define ASH_SYSTEM_POWER_POWER_BUTTON_CONTROLLER_TEST_API_H_

#include "ash/system/power/power_button_controller.h"
#include "base/compiler_specific.h"
#include "base/macros.h"

namespace base {
class TickClock;
}  // namespace base

namespace ui {
class KeyEvent;
}  // namespace ui

namespace gfx {
class Rect;
}  // namespace gfx

namespace ash {
class PowerButtonMenuView;
class PowerButtonScreenshotController;

// Helper class used by tests to access PowerButtonController's internal state.
class PowerButtonControllerTestApi {
 public:
  explicit PowerButtonControllerTestApi(PowerButtonController* controller);
  ~PowerButtonControllerTestApi();

  // Returns true when |controller_->pre_shutdown_timer_| is running.
  bool PreShutdownTimerIsRunning() const;

  // If |controller_->pre_shutdown_timer_| is running, stops it, runs its task,
  // and returns true. Otherwise, returns false.
  bool TriggerPreShutdownTimeout() WARN_UNUSED_RESULT;

  // Returns true when |power_button_menu_timer_| is running.
  bool PowerButtonMenuTimerIsRunning() const;

  // If |controller_->power_button_menu_timer_| is running, stops it, runs its
  // task, and returns true. Otherwise, returns false.
  bool TriggerPowerButtonMenuTimeout() WARN_UNUSED_RESULT;

  // Sends |event| to |controller_->display_controller_|.
  void SendKeyEvent(ui::KeyEvent* event);

  // Gets the bounds of the menu view in screen.
  gfx::Rect GetMenuBoundsInScreen() const;

  // Gets the PowerButtonMenuView of the |controller_|'s menu, which is used by
  // GetMenuBoundsInScreen.
  PowerButtonMenuView* GetPowerButtonMenuView() const;

  // True if the menu is opened.
  bool IsMenuOpened() const;

  // True if |controller_|'s menu has a sign out item.
  bool MenuHasSignOutItem() const;

  // True if |controller_|'s menu has a lock screen item.
  bool MenuHasLockScreenItem() const;

  // True if |controller_|'s menu has a feedback item.
  bool MenuHasFeedbackItem() const;

  PowerButtonScreenshotController* GetScreenshotController();

  void SetPowerButtonType(PowerButtonController::ButtonType button_type);

  void SetTickClock(const base::TickClock* tick_clock);

  void SetShowMenuAnimationDone(bool show_menu_animation_done);

  // Gets |show_menu_animation_done_| of |controller_|.
  bool ShowMenuAnimationDone() const;

 private:
  PowerButtonController* controller_;  // Not owned.

  DISALLOW_COPY_AND_ASSIGN(PowerButtonControllerTestApi);
};

}  // namespace ash

#endif  // ASH_SYSTEM_POWER_POWER_BUTTON_CONTROLLER_TEST_API_H_
