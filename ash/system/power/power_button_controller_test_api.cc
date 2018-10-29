// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/power_button_controller_test_api.h"

#include "ash/system/power/power_button_display_controller.h"
#include "ash/system/power/power_button_menu_screen_view.h"
#include "ash/system/power/power_button_menu_view.h"
#include "ash/system/power/power_button_screenshot_controller.h"
#include "base/time/default_tick_clock.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget.h"

namespace ash {

PowerButtonControllerTestApi::PowerButtonControllerTestApi(
    PowerButtonController* controller)
    : controller_(controller) {}

PowerButtonControllerTestApi::~PowerButtonControllerTestApi() = default;

bool PowerButtonControllerTestApi::PreShutdownTimerIsRunning() const {
  return controller_->pre_shutdown_timer_.IsRunning();
}

bool PowerButtonControllerTestApi::TriggerPreShutdownTimeout() {
  if (!controller_->pre_shutdown_timer_.IsRunning())
    return false;

  controller_->pre_shutdown_timer_.FireNow();
  return true;
}

bool PowerButtonControllerTestApi::PowerButtonMenuTimerIsRunning() const {
  return controller_->power_button_menu_timer_.IsRunning();
}

bool PowerButtonControllerTestApi::TriggerPowerButtonMenuTimeout() {
  if (!controller_->power_button_menu_timer_.IsRunning())
    return false;

  controller_->power_button_menu_timer_.FireNow();
  return true;
}

void PowerButtonControllerTestApi::SendKeyEvent(ui::KeyEvent* event) {
  controller_->display_controller_->OnKeyEvent(event);
}

gfx::Rect PowerButtonControllerTestApi::GetMenuBoundsInScreen() const {
  return IsMenuOpened() ? GetPowerButtonMenuView()->GetBoundsInScreen()
                        : gfx::Rect();
}

PowerButtonMenuView* PowerButtonControllerTestApi::GetPowerButtonMenuView()
    const {
  return IsMenuOpened() ? static_cast<PowerButtonMenuScreenView*>(
                              controller_->menu_widget_->GetContentsView())
                              ->power_button_menu_view()
                        : nullptr;
}

bool PowerButtonControllerTestApi::IsMenuOpened() const {
  return controller_->IsMenuOpened();
}

bool PowerButtonControllerTestApi::MenuHasSignOutItem() const {
  return IsMenuOpened() && GetPowerButtonMenuView()->sign_out_item_for_test();
}

bool PowerButtonControllerTestApi::MenuHasLockScreenItem() const {
  return IsMenuOpened() &&
         GetPowerButtonMenuView()->lock_screen_item_for_test();
}

bool PowerButtonControllerTestApi::MenuHasFeedbackItem() const {
  return IsMenuOpened() && GetPowerButtonMenuView()->feedback_item_for_test();
}

PowerButtonScreenshotController*
PowerButtonControllerTestApi::GetScreenshotController() {
  return controller_->screenshot_controller_.get();
}

void PowerButtonControllerTestApi::SetPowerButtonType(
    PowerButtonController::ButtonType button_type) {
  controller_->button_type_ = button_type;
}

void PowerButtonControllerTestApi::SetTickClock(
    const base::TickClock* tick_clock) {
  DCHECK(tick_clock);
  controller_->tick_clock_ = tick_clock;

  controller_->display_controller_ =
      std::make_unique<PowerButtonDisplayController>(
          controller_->backlights_forced_off_setter_, controller_->tick_clock_);
}

void PowerButtonControllerTestApi::SetShowMenuAnimationDone(
    bool show_menu_animation_done) {
  controller_->show_menu_animation_done_ = show_menu_animation_done;
}

bool PowerButtonControllerTestApi::ShowMenuAnimationDone() const {
  return controller_->show_menu_animation_done_;
}

}  // namespace ash
