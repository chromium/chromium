// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/control_v_histogram_recorder.h"

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/accelerators/accelerator_history_impl.h"
#include "ash/shell.h"
#include "base/containers/contains.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"

namespace ash {

namespace {

// Returns whether Ctrl and V are currently pressed, with no additional keys.
bool IsControlVPressed() {
  const auto& currently_pressed_keys = Shell::Get()
                                           ->accelerator_controller()
                                           ->GetAcceleratorHistory()
                                           ->currently_pressed_keys();
  return base::Contains(currently_pressed_keys, ui::VKEY_CONTROL) &&
         base::Contains(currently_pressed_keys, ui::VKEY_V) &&
         currently_pressed_keys.size() == 2;
}

}  // namespace

void ControlVHistogramRecorder::OnKeyEvent(ui::KeyEvent* event) {
  if (event->type() == ui::EventType::kKeyPressed) {
    switch (event->key_code()) {
      case ui::VKEY_CONTROL:
        // If Ctrl is held down and either auto-repeat begins or the other Ctrl
        // key is pressed, the timestamp used for calculating Ctrl to V delay
        // should not change.
        if (!(event->flags() & ui::EF_IS_REPEAT))
          ctrl_pressed_time_ = base::TimeTicks::Now();
        return;
      case ui::VKEY_V:
        if (IsControlVPressed()) {
          // When Ctrl+V is held and keyboard auto-repeat is enabled, the OS
          // will synthesize more key pressed events to trigger further pasting.
          // Record the initial V press to measure how long Ctrl+V is held.
          if (ctrl_v_pressed_time_.is_null())
            ctrl_v_pressed_time_ = base::TimeTicks::Now();

          // The V key can be pressed to issue multiple pastes without ever
          // releasing Ctrl. Measure only the time between a Ctrl press and the
          // first V press in the user's accelerator sequence.
          if (!ctrl_pressed_time_.is_null()) {
            base::UmaHistogramTimes("Ash.ClipboardHistory.ControlToVDelayV2",
                                    ctrl_v_pressed_time_ - ctrl_pressed_time_);
            ctrl_pressed_time_ = base::TimeTicks();
          }
        }
        return;
      default:
        // Pressing an unrelated key ends a Ctrl+V paste sequence.
        MaybeRecordControlVHeldTime();
        return;
    }
  }

  if (event->type() == ui::EventType::kKeyReleased) {
    switch (event->key_code()) {
      case ui::VKEY_CONTROL:
      case ui::VKEY_V:
        // Releasing either key ends a Ctrl+V paste sequence.
        MaybeRecordControlVHeldTime();
        return;
      default:
        return;
    }
  }
}

void ControlVHistogramRecorder::MaybeRecordControlVHeldTime() {
  if (ctrl_v_pressed_time_.is_null())
    return;

  base::UmaHistogramTimes("Ash.ClipboardHistory.ControlVHeldTime",
                          base::TimeTicks::Now() - ctrl_v_pressed_time_);
  ctrl_v_pressed_time_ = base::TimeTicks();
}

}  // namespace ash
