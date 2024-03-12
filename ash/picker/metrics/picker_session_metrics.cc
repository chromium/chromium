// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/metrics/picker_session_metrics.h"

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"

namespace ash {

PickerSessionMetrics::PickerSessionMetrics() = default;

PickerSessionMetrics::~PickerSessionMetrics() {
  if (recorded_outcome_) {
    return;
  }

  RecordOutcome(SessionOutcome::kUnknown);
}

void PickerSessionMetrics::RecordOutcome(SessionOutcome outcome) {
  if (recorded_outcome_) {
    return;
  }

  base::UmaHistogramEnumeration("Ash.Picker.Session.Outcome", outcome);
  recorded_outcome_ = true;
}

}  // namespace ash
