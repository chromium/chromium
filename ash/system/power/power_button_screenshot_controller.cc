// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/power_button_screenshot_controller.h"

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/power/power_button_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/time/tick_clock.h"
#include "ui/events/event.h"

namespace ash {

namespace {

bool IsTabletMode() {
  return Shell::Get()->tablet_mode_controller()->InTabletMode();
}

}  // namespace

constexpr base::TimeDelta
    PowerButtonScreenshotController::kScreenshotChordDelay;

PowerButtonScreenshotController::PowerButtonScreenshotController(
    const base::TickClock* tick_clock)
    : tick_clock_(tick_clock) {
  DCHECK(tick_clock_);
  // Using prepend to make sure this event handler is put in front of
  // AcceleratorFilter. See Shell::Init().
  Shell::Get()->AddPreTargetHandler(this, ui::EventTarget::Priority::kSystem);
}

PowerButtonScreenshotController::~PowerButtonScreenshotController() {
  Shell::Get()->RemovePreTargetHandler(this);
}

bool PowerButtonScreenshotController::OnPowerButtonEvent(
    bool down,
    const base::TimeTicks& timestamp) {
  if (!IsTabletMode())
    return false;

  power_button_pressed_ = down;
  if (power_button_pressed_) {
    volume_down_timer_.Stop();
    volume_up_timer_.Stop();
    power_button_pressed_time_ = tick_clock_->NowTicks();
    if (InterceptScreenshotChord())
      return true;
  }

  if (!down)
    return false;

  // If volume key is pressed, mark power button as consumed. This invalidates
  // other power button's behavior when user tries to operate screenshot.
  return volume_down_key_pressed_ || volume_up_key_pressed_;
}

void PowerButtonScreenshotController::OnKeyEvent(ui::KeyEvent* event) {
  if (!IsTabletMode())
    return;

  ui::KeyboardCode key_code = event->key_code();
  if (key_code != ui::VKEY_VOLUME_DOWN && key_code != ui::VKEY_VOLUME_UP)
    return;

  const bool is_volume_down = key_code == ui::VKEY_VOLUME_DOWN;
  if (event->type() == ui::ET_KEY_PRESSED) {
    if (!volume_down_key_pressed_ && !volume_up_key_pressed_) {
      if (is_volume_down) {
        volume_down_key_pressed_ = true;
        volume_down_key_pressed_time_ = tick_clock_->NowTicks();
        consume_volume_down_ = false;
        InterceptScreenshotChord();
      } else {
        volume_up_key_pressed_ = true;
        volume_up_key_pressed_time_ = tick_clock_->NowTicks();
        consume_volume_up_ = false;
        InterceptScreenshotChord();
      }
    }

    if (consume_volume_down_ || consume_volume_up_)
      event->StopPropagation();
  } else {
    is_volume_down ? volume_down_key_pressed_ = false
                   : volume_up_key_pressed_ = false;
  }

  // When volume key is pressed, cancel the ongoing power button behavior.
  if (volume_down_key_pressed_ || volume_up_key_pressed_)
    Shell::Get()->power_button_controller()->CancelPowerButtonEvent();

  // On volume down/up key pressed while power button not pressed yet state, do
  // not propagate volume down/up key pressed event for chord delay time. Start
  // the timer to wait power button pressed for screenshot operation, and on
  // timeout perform the delayed volume down/up operation.
  if (power_button_pressed_)
    return;

  base::TimeTicks now = tick_clock_->NowTicks();
  if (volume_down_key_pressed_ && is_volume_down &&
      now <= volume_down_key_pressed_time_ + kScreenshotChordDelay) {
    event->StopPropagation();
    if (!volume_down_timer_.IsRunning()) {
      volume_down_timer_.Start(
          FROM_HERE, kScreenshotChordDelay,
          base::BindOnce(
              &PowerButtonScreenshotController::OnVolumeControlTimeout,
              base::Unretained(this), ui::Accelerator(*event), /*down=*/true));
    }
  } else if (volume_up_key_pressed_ && !is_volume_down &&
             now <= volume_up_key_pressed_time_ + kScreenshotChordDelay) {
    event->StopPropagation();
    if (!volume_up_timer_.IsRunning()) {
      volume_up_timer_.Start(
          FROM_HERE, kScreenshotChordDelay,
          base::BindOnce(
              &PowerButtonScreenshotController::OnVolumeControlTimeout,
              base::Unretained(this), ui::Accelerator(*event), /*down=*/false));
    }
  }
}

bool PowerButtonScreenshotController::InterceptScreenshotChord() {
  if (!power_button_pressed_ ||
      (!volume_down_key_pressed_ && !volume_up_key_pressed_)) {
    return false;
  }

  // Record the delay when power button and volume down/up key are both pressed,
  // which indicates user might want to use accelerator to take screenshot.
  // This will help us determine the best chord delay among metrics.
  const base::TimeDelta key_pressed_delay =
      volume_down_key_pressed_
          ? power_button_pressed_time_ - volume_down_key_pressed_time_
          : power_button_pressed_time_ - volume_up_key_pressed_time_;
  UMA_HISTOGRAM_TIMES("Ash.PowerButtonScreenshot.DelayBetweenAccelKeyPressed",
                      key_pressed_delay.magnitude());

  base::TimeTicks now = tick_clock_->NowTicks();
  if (now > power_button_pressed_time_ + kScreenshotChordDelay)
    return false;

  consume_volume_down_ =
      volume_down_key_pressed_ &&
      now <= volume_down_key_pressed_time_ + kScreenshotChordDelay;
  consume_volume_up_ =
      volume_up_key_pressed_ &&
      now <= volume_up_key_pressed_time_ + kScreenshotChordDelay;
  if (consume_volume_down_ || consume_volume_up_) {
    Shell::Get()->accelerator_controller()->PerformActionIfEnabled(
        TAKE_SCREENSHOT, {});

    base::RecordAction(base::UserMetricsAction("Accel_PowerButton_Screenshot"));
  }
  return consume_volume_down_ || consume_volume_up_;
}

void PowerButtonScreenshotController::OnVolumeControlTimeout(
    const ui::Accelerator& accelerator,
    bool down) {
  Shell::Get()->accelerator_controller()->PerformActionIfEnabled(
      down ? VOLUME_DOWN : VOLUME_UP, accelerator);
}

}  // namespace ash
