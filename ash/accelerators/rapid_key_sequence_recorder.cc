// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/rapid_key_sequence_recorder.h"

#include <optional>
#include <tuple>

#include "ash/events/event_rewriter_controller_impl.h"
#include "ash/events/prerewritten_event_forwarder.h"
#include "ash/public/cpp/accelerators_util.h"
#include "ash/shell.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/types/event_type.h"

namespace ash {

namespace {

void RecordDoubleTapShiftMetric(base::TimeDelta time_between_taps,
                                bool is_left_shift) {
  if (is_left_shift) {
    UMA_HISTOGRAM_CUSTOM_TIMES("ChromeOS.Inputs.DoubleTapLeftShiftDuration",
                               time_between_taps, kDoubleTapShiftBucketMinimum,
                               kDoubleTapShiftTime,
                               /*bucket_count=*/kDoubleTapShiftBucketCount);
  } else {
    UMA_HISTOGRAM_CUSTOM_TIMES("ChromeOS.Inputs.DoubleTapRightShiftDuration",
                               time_between_taps, kDoubleTapShiftBucketMinimum,
                               kDoubleTapShiftTime,
                               /*bucket_count=*/kDoubleTapShiftBucketCount);
  }
}

}  // namespace

RapidKeySequenceRecorder::RapidKeySequenceRecorder() = default;
RapidKeySequenceRecorder::~RapidKeySequenceRecorder() {
  CHECK(Shell::Get());
  auto* event_forwarder =
      Shell::Get()->event_rewriter_controller()->prerewritten_event_forwarder();
  if (initialized_ && event_forwarder) {
    event_forwarder->RemoveObserver(this);
  }
}

void RapidKeySequenceRecorder::Initialize() {
  CHECK(Shell::Get());
  auto* event_forwarder =
      Shell::Get()->event_rewriter_controller()->prerewritten_event_forwarder();
  if (!event_forwarder) {
    LOG(ERROR) << "Attempted to initialiaze RapidKeySequenceRecorder before "
               << "PrerewrittenEventForwarder was initialized.";
    return;
  }

  initialized_ = true;
  event_forwarder->AddObserver(this);
}

void RapidKeySequenceRecorder::OnPrerewriteKeyInputEvent(
    const ui::KeyEvent& key_event) {
  const bool same_key_pressed_again =
      key_event.code() == last_dom_code_pressed_;

  if (key_event.type() == ui::EventType::kKeyReleased) {
    // Reset last_dom_code_pressed_ to NONE to avoid other key
    // pressed in the middle of pressing shift key triggers metrics record.
    if (!same_key_pressed_again) {
      last_dom_code_pressed_ = ui::DomCode::NONE;
    }
    return;
  }

  if (key_event.is_repeat()) {
    return;
  }

  const bool is_shift = key_event.code() == ui::DomCode::SHIFT_LEFT ||
                        key_event.code() == ui::DomCode::SHIFT_RIGHT;
  last_dom_code_pressed_ = key_event.code();

  if (same_key_pressed_again && is_shift) {
    DCHECK(!last_key_press_time_.is_null());
    auto time_since_last_shift_press =
        key_event.time_stamp() - last_key_press_time_;
    if (time_since_last_shift_press < kDoubleTapShiftTime) {
      RecordDoubleTapShiftMetric(
          /*time_between_taps=*/time_since_last_shift_press,
          /*is_left_shift=*/key_event.code() == ui::DomCode::SHIFT_LEFT);
      last_dom_code_pressed_ = ui::DomCode::NONE;
    }
  }
  last_key_press_time_ = key_event.time_stamp();
}

}  // namespace ash
