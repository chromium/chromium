// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CLIPBOARD_CONTROL_V_HISTOGRAM_RECORDER_H_
#define ASH_CLIPBOARD_CONTROL_V_HISTOGRAM_RECORDER_H_

#include "base/time/time.h"
#include "ui/events/event_handler.h"

namespace ui {

class KeyEvent;

}  // namespace ui

namespace ash {

// A `Shell` pretarget `EventHandler` to record timings for users'
// accelerator-initiated pastes.
class ControlVHistogramRecorder : public ui::EventHandler {
 public:
  ControlVHistogramRecorder() = default;
  ControlVHistogramRecorder(const ControlVHistogramRecorder&) = delete;
  ControlVHistogramRecorder& operator=(const ControlVHistogramRecorder&) =
      delete;
  ~ControlVHistogramRecorder() override = default;

  // ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override;

 private:
  // If a Ctrl+V paste sequence was in progress, records an entry for the
  // Ash.ClipboardHistory.ControlVHeldTime histogram and resets state so that
  // new entries can be recorded. Otherwise, does nothing.
  void MaybeRecordControlVHeldTime();

  // The last time a user pressed Ctrl. Reset to null when V is pressed and an
  // entry is recorded for the Ash.ClipboardHistory.ControlToVDelayV2 histogram.
  base::TimeTicks ctrl_pressed_time_;

  // The last time a user pressed V with Ctrl held. Reset to null when the paste
  // sequence ends and an entry is recorded for the
  // Ash.ClipboardHistory.ControlVHeldTime histogram.
  base::TimeTicks ctrl_v_pressed_time_;
};

}  // namespace ash

#endif  // ASH_CLIPBOARD_CONTROL_V_HISTOGRAM_RECORDER_H_