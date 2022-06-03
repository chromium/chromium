// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_CONTEXTUAL_NUDGE_STATUS_TRACKER_H_
#define ASH_SHELF_CONTEXTUAL_NUDGE_STATUS_TRACKER_H_

#include "ash/ash_export.h"
#include "ash/shelf/contextual_tooltip.h"
#include "base/time/time.h"

namespace ash {

// This class keeps track of the visibility of a single nudge and reports
// metrics related to the nudge's state changes.
class ASH_EXPORT ContextualNudgeStatusTracker {
 public:
  explicit ContextualNudgeStatusTracker(
      ash::contextual_tooltip::TooltipType type_);
  ContextualNudgeStatusTracker(const ContextualNudgeStatusTracker&) = delete;
  ContextualNudgeStatusTracker& operator=(const ContextualNudgeStatusTracker&) =
      delete;
  ~ContextualNudgeStatusTracker();

  // Activates the status tracker and records the time the nudge was shown.
  void HandleNudgeShown(base::TimeTicks shown_time);

  // Should only be called once per shown nudge as metrics are only measuring
  // the time between showing the nudge and the user performing the gesture.
  void HandleGesturePerformed(base::TimeTicks hide_time);

  // Records relevant metrics when the user exits the state that showed
  // the contextual nudge if |dismissal_reason_recorded_| is false.
  void MaybeLogNudgeDismissedMetrics(
      contextual_tooltip::DismissNudgeReason reason);

  bool gesture_time_recorded() const { return gesture_time_recorded_; }

  bool can_record_dismiss_metrics() const {
    return can_record_dismiss_metrics_;
  }

 private:
  // Time the nudge was last shown.
  base::TimeTicks nudge_shown_time_;

  // The tooltip type tracked by this object.
  const ash::contextual_tooltip::TooltipType type_;

  // Tracks whether the tooltip dismiss time has been recorded because of the
  // gesture is performed. Sets after gesture is performed. Resets when the
  // tooltip is shown.
  bool gesture_time_recorded_ = false;

  // Tracks whether the nudge dismiss metrics can be recorded. Only after the
  // tooltip is shown, the tooltip can be dismissed and the dismiss reason
  // metrics can be logged. Otherwise, calling MaybeLogNudgeDismissedMetrics()
  // is a no-op.
  bool can_record_dismiss_metrics_ = false;
};

}  // namespace ash

#endif  // ASH_SHELF_CONTEXTUAL_NUDGE_STATUS_TRACKER_H_
