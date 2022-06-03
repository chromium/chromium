// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CLIPBOARD_CONTROL_V_HISTOGRAM_RECORDER_H_
#define ASH_CLIPBOARD_CONTROL_V_HISTOGRAM_RECORDER_H_

#include "ash/ash_export.h"
#include "base/time/time.h"
#include "ui/events/event_handler.h"

namespace ui {

class KeyEvent;

}  // namespace ui

namespace ash {

// An EventHandler added as a pretarget handler to Shell to record the time
// delay between ui::VKEY_CONTROL and ui::VKEY_V when a user is pasting.
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
  // The last time a user pressed ui::VKEY_CONTROL. Used to establish a time
  // range for user patterns while pressing ui::VKEY_CONTROL + ui::VKEY_V.
  base::TimeTicks ctrl_pressed_time_;
};

}  // namespace ash

#endif  // ASH_CLIPBOARD_CONTROL_V_HISTOGRAM_RECORDER_H_