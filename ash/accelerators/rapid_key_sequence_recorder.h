// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCELERATORS_RAPID_KEY_SEQUENCE_RECORDER_H_
#define ASH_ACCELERATORS_RAPID_KEY_SEQUENCE_RECORDER_H_

#include "ash/ash_export.h"
#include "ash/events/prerewritten_event_forwarder.h"
#include "base/time/time.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_handler.h"
#include "ui/events/keycodes/dom/dom_code.h"

namespace ash {

// If a second consecutive shift press happens within this time window, it is
// considered a double tap of the key.
inline constexpr base::TimeDelta kDoubleTapShiftTime = base::Milliseconds(500);
inline constexpr base::TimeDelta kDoubleTapShiftBucketMinimum =
    base::Milliseconds(10);
inline constexpr int kDoubleTapShiftBucketCount = 50;

// Emits a metric when a user double taps either the left shift or right shift
// key within a short window of time.
class ASH_EXPORT RapidKeySequenceRecorder
    : public PrerewrittenEventForwarder::Observer {
 public:
  RapidKeySequenceRecorder();
  RapidKeySequenceRecorder(const RapidKeySequenceRecorder&) = delete;
  RapidKeySequenceRecorder& operator=(const RapidKeySequenceRecorder&) = delete;
  ~RapidKeySequenceRecorder() override;

  void Initialize();

  // ui::PrerewrittenEventForwarder::Observer:
  void OnPrerewriteKeyInputEvent(const ui::KeyEvent& event) override;

 private:
  bool initialized_ = false;

  base::TimeTicks last_key_press_time_;
  ui::DomCode last_dom_code_pressed_ = ui::DomCode::NONE;
};

}  // namespace ash

#endif  // ASH_ACCELERATORS_RAPID_KEY_SEQUENCE_RECORDER_H_
