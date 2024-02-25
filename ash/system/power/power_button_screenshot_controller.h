// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_POWER_POWER_BUTTON_SCREENSHOT_CONTROLLER_H_
#define ASH_SYSTEM_POWER_POWER_BUTTON_SCREENSHOT_CONTROLLER_H_

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/event_handler.h"

namespace base {
class TickClock;
}  // namespace base

namespace ash {

// Handles power button screenshot accelerator. The screenshot condition is
// pressing power button and volume down key simultaneously, similar to Android
// phones.
class ASH_EXPORT PowerButtonScreenshotController : public ui::EventHandler {
 public:
  // Time that volume down key and power button must be pressed within this
  // interval of each other to make a screenshot.
  static constexpr base::TimeDelta kScreenshotChordDelay =
      base::Milliseconds(150);

  explicit PowerButtonScreenshotController(const base::TickClock* tick_clock);

  PowerButtonScreenshotController(const PowerButtonScreenshotController&) =
      delete;
  PowerButtonScreenshotController& operator=(
      const PowerButtonScreenshotController&) = delete;

  ~PowerButtonScreenshotController() override;

  // Returns true if power button event is consumed by |this|, otherwise false.
  bool OnPowerButtonEvent(bool down, const base::TimeTicks& timestamp);

  // Overridden from ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override;

 private:
  friend class PowerButtonScreenshotControllerTestApi;

  // Helper method used to intercept power button event or volume down key event
  // to check screenshot chord condition. Return true if screenshot is taken to
  // indicate that power button and volume down key is consumed by screenshot.
  bool InterceptScreenshotChord();

  // Called by |volume_down_timer_| or |volume_up_timer_| to perform volume down
  // or up accelerator.
  void OnVolumeControlTimeout(const ui::Accelerator& accelerator, bool down);

  // True if volume down/up key is pressed.
  bool volume_down_key_pressed_ = false;
  bool volume_up_key_pressed_ = false;

  // True if volume down/up key is consumed by screenshot accelerator.
  bool consume_volume_down_ = false;
  bool consume_volume_up_ = false;

  // True if power button is pressed.
  bool power_button_pressed_ = false;

  // Saves the most recent volume down/up key pressed time.
  base::TimeTicks volume_down_key_pressed_time_;
  base::TimeTicks volume_up_key_pressed_time_;

  // Saves the most recent power button pressed time.
  base::TimeTicks power_button_pressed_time_;

  // Started when volume down/up key is pressed and power button is not pressed.
  // Stopped when power button is pressed. Runs OnVolumeControlTimeout to
  // perform a volume down/up accelerator.
  base::OneShotTimer volume_down_timer_;
  base::OneShotTimer volume_up_timer_;

  // Time source for performed action times.
  raw_ptr<const base::TickClock> tick_clock_;  // Not owned.
};

}  // namespace ash

#endif  // ASH_SYSTEM_POWER_POWER_BUTTON_SCREENSHOT_CONTROLLER_H_
