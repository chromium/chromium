// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/contextual_nudge_status_tracker.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"

namespace {

// The maximum number of seconds that should be recorded in the TimeDelta
// histogram. Time between showing the nudge and recording the gesture are
// separated into 61 buckets: 0-1 second, 1-2 second ... 59-60 seconds and 60+
// seconds.
constexpr int kMaxHistogramTime = 61;

std::string GetEnumHistogramName(ash::contextual_tooltip::TooltipType type) {
  switch (type) {
    case ash::contextual_tooltip::TooltipType::kBackGesture:
      return "Ash.ContextualNudgeDismissContext.BackGesture";
    case ash::contextual_tooltip::TooltipType::kHomeToOverview:
      return "Ash.ContextualNudgeDismissContext.HomeToOverview";
    case ash::contextual_tooltip::TooltipType::kInAppToHome:
      return "Ash.ContextualNudgeDismissContext.InAppToHome";
  }
}

std::string GetTimeDeltaHistogramName(
    ash::contextual_tooltip::TooltipType type) {
  switch (type) {
    case ash::contextual_tooltip::TooltipType::kBackGesture:
      return "Ash.ContextualNudgeDismissTime.BackGesture";
    case ash::contextual_tooltip::TooltipType::kHomeToOverview:
      return "Ash.ContextualNudgeDismissTime.HomeToOverview";
    case ash::contextual_tooltip::TooltipType::kInAppToHome:
      return "Ash.ContextualNudgeDismissTime.InAppToHome";
  }
}

}  // namespace

namespace ash {

ContextualNudgeStatusTracker::ContextualNudgeStatusTracker(
    ash::contextual_tooltip::TooltipType type)
    : type_(type) {}

ContextualNudgeStatusTracker::~ContextualNudgeStatusTracker() = default;

void ContextualNudgeStatusTracker::HandleNudgeShown(
    base::TimeTicks shown_time) {
  nudge_shown_time_ = shown_time;
  gesture_time_recorded_ = false;
  can_record_dismiss_metrics_ = true;
}

void ContextualNudgeStatusTracker::HandleGesturePerformed(
    base::TimeTicks hide_time) {
  if (gesture_time_recorded_)
    return;

  if (can_record_dismiss_metrics_) {
    MaybeLogNudgeDismissedMetrics(
        contextual_tooltip::DismissNudgeReason::kPerformedGesture);
  }

  base::TimeDelta time_since_show = hide_time - nudge_shown_time_;
  base::UmaHistogramCustomTimes(
      GetTimeDeltaHistogramName(type_), time_since_show, base::Seconds(1),
      base::Seconds(kMaxHistogramTime), kMaxHistogramTime);
  gesture_time_recorded_ = true;
}

void ContextualNudgeStatusTracker::MaybeLogNudgeDismissedMetrics(
    contextual_tooltip::DismissNudgeReason reason) {
  if (!can_record_dismiss_metrics_)
    return;
  base::UmaHistogramEnumeration(GetEnumHistogramName(type_), reason);
  can_record_dismiss_metrics_ = false;
}

}  // namespace ash
