// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_METRICS_PICKER_SESSION_METRICS_H_
#define ASH_PICKER_METRICS_PICKER_SESSION_METRICS_H_

#include <optional>

#include "ash/ash_export.h"

namespace ash {

// Records metrics for a session of using Picker.
class ASH_EXPORT PickerSessionMetrics {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class SessionOutcome {
    // The outcome is unknown.
    kUnknown = 0,
    // User inserts or copies a result.
    kInsertedOrCopied = 1,
    // User abandons the session (e.g. by closing the window without inserting).
    kAbandoned = 2,
    kMaxValue = kAbandoned,
  };

  PickerSessionMetrics();
  ~PickerSessionMetrics();

  void RecordOutcome(SessionOutcome outcome);

 private:
  // Whether the outcome of this session has been recorded.
  bool recorded_outcome_ = false;
};

}  // namespace ash

#endif  // ASH_PICKER_METRICS_PICKER_SESSION_METRICS_H_
