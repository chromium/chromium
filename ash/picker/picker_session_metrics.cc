// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/picker_session_metrics.h"

#include "base/metrics/histogram_functions.h"

namespace ash {

PickerSessionMetrics::PickerSessionMetrics(
    const base::TimeTicks trigger_start_timestamp)
    : trigger_start_timestamp_(trigger_start_timestamp) {}

void PickerSessionMetrics::MarkInputFocus() {
  if (marked_first_focus_) {
    return;
  }

  base::UmaHistogramCustomTimes(
      "Ash.Picker.Session.InputReadyLatency",
      /*sample=*/base::TimeTicks::Now() - trigger_start_timestamp_,
      /*min=*/base::Seconds(0), /*max=*/base::Seconds(5), /*buckets=*/100);
  marked_first_focus_ = true;
}

}  // namespace ash
