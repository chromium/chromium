// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_cueing/contextual_cueing_service.h"

#include <cmath>

#include "chrome/browser/contextual_cueing/contextual_cueing_features.h"

namespace contextual_cueing {

ContextualCueingService::ContextualCueingService() = default;

ContextualCueingService::~ContextualCueingService() = default;

void ContextualCueingService::CueingNudgeShown() {
  size_t max_queue_size = kNudgeCapCount.Get();
  CHECK(recent_nudge_timestamps_.size() <= max_queue_size);

  if (recent_nudge_timestamps_.size() == max_queue_size) {
    recent_nudge_timestamps_.pop();
  }
  recent_nudge_timestamps_.push(base::Time::Now());
}

void ContextualCueingService::CueingNudgeDismissed() {
  base::TimeDelta backoff_duration =
      kBackoffTime.Get() * pow(kBackoffMultiplierBase.Get(), dismiss_count_);

  backoff_end_time_ = base::Time::Now() + backoff_duration;
  ++dismiss_count_;
}

void ContextualCueingService::CueingNudgeClicked() {
  dismiss_count_ = 0;
}

bool ContextualCueingService::CanShowNudge() {
  return !(IsNudgeBlockedByBackoffRule() || IsNudgeBlockedByNudgeCap());
}

bool ContextualCueingService::IsNudgeBlockedByBackoffRule() const {
  return backoff_end_time_ && (base::Time::Now() < backoff_end_time_);
}

bool ContextualCueingService::IsNudgeBlockedByNudgeCap() const {
  size_t max_queue_size = kNudgeCapCount.Get();
  if (recent_nudge_timestamps_.size() < max_queue_size) {
    return false;
  }

  base::TimeDelta time_diff =
      base::Time::Now() - recent_nudge_timestamps_.front();
  CHECK(time_diff.is_positive());
  if (time_diff > kNudgeCapTime.Get()) {
    return false;
  }

  return true;
}

}  // namespace contextual_cueing
