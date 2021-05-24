// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/control_v_histogram_recorder.h"

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/accelerators/accelerator_history_impl.h"
#include "ash/shell.h"
#include "base/metrics/histogram_functions.h"
#include "ui/events/event.h"

namespace ash {

void ControlVHistogramRecorder::OnKeyEvent(ui::KeyEvent* event) {
  if (event->type() != ui::ET_KEY_PRESSED)
    return;

  switch (event->key_code()) {
    case ui::VKEY_CONTROL:
      ctrl_pressed_time_ = base::TimeTicks::Now();
      break;
    case ui::VKEY_V: {
      auto& currently_pressed_keys = Shell::Get()
                                         ->accelerator_controller()
                                         ->GetAcceleratorHistory()
                                         ->currently_pressed_keys();
      if (currently_pressed_keys.find(ui::VKEY_CONTROL) !=
              currently_pressed_keys.end() &&
          currently_pressed_keys.find(ui::VKEY_V) !=
              currently_pressed_keys.end() &&
          currently_pressed_keys.size() == 2 && !ctrl_pressed_time_.is_null()) {
        base::UmaHistogramTimes("Ash.ClipboardHistory.ControlToVDelay",
                                base::TimeTicks::Now() - ctrl_pressed_time_);
        // Prevent a second V from recording a second metric.
        ctrl_pressed_time_ = base::TimeTicks();
      }
    } break;
    default:
      break;
  }
}

}  // namespace ash
