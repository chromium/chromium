// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_PICKER_SESSION_METRICS_H_
#define ASH_PICKER_PICKER_SESSION_METRICS_H_

#include "ash/ash_export.h"
#include "base/time/time.h"

namespace ui {
class PresentationTimeRecorder;
}

namespace views {
class Widget;
}

namespace ash {

// Records metrics for a session of using Picker, such as latency, memory usage,
// and user funnel metrics.
class ASH_EXPORT PickerSessionMetrics {
 public:
  // `trigger_event_timestamp` is the timestamp of the event that triggered the
  // session. By default, this uses the time PickerSessionMetrics is created.
  // Call `StartRecording` to start recording metrics for the session.
  PickerSessionMetrics(
      base::TimeTicks trigger_start_timestamp = base::TimeTicks::Now());
  ~PickerSessionMetrics();

  // Starts recording metrics for this session.
  // `widget` is the widget that contains the Picker UI. It must outlive this
  // class.
  void StartRecording(views::Widget& widget);

  // Stops recording metrics for this session.
  // This should be called, for example, when the widget containing the Picker
  // UI is destroyed.
  void StopRecording();

  // Marks a focus event on the picker search field.
  void MarkInputFocus();

  // Marks that the search field contents changed.
  void MarkContentsChanged();

 private:
  bool is_recording_ = false;

  // The timestamp of earliest the feature was triggered.
  base::TimeTicks trigger_start_timestamp_;

  // Whether the first input focus has been marked yet.
  bool marked_first_focus_ = false;

  // Records the presentation delay when search field contents change.
  std::unique_ptr<ui::PresentationTimeRecorder>
      search_field_presentation_time_recorder_;
};

}  // namespace ash

#endif
