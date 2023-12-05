// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_PICKER_SESSION_METRICS_H_
#define ASH_PICKER_PICKER_SESSION_METRICS_H_

#include "ash/ash_export.h"
#include "base/time/time.h"

namespace ash {

// Records metrics for a session of using Picker, such as latency, memory usage,
// and user funnel metrics.
class ASH_EXPORT PickerSessionMetrics {
 public:
  // `trigger_event_timestamp` is the timestamp of the event that triggered the
  // session. By default, this uses the time PickerSessionMetrics is created.
  explicit PickerSessionMetrics(
      base::TimeTicks trigger_start_timestamp = base::TimeTicks::Now());

  // Marks a focus event on the picker search field.
  void MarkInputFocus();

 private:
  // The timestamp of earliest the feature was triggered.
  base::TimeTicks trigger_start_timestamp_;

  // Whether the first input focus has been marked yet.
  bool marked_first_focus_ = false;
};

}  // namespace ash

#endif
