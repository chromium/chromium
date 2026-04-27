// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/scroll_sequence_tracker.h"

#include "base/time/time.h"
#include "cc/metrics/event_metrics.h"

namespace cc {

void ScrollSequenceTracker::OnScrollBegin(const EventMetrics* metrics) {
  scroll_begin_arrival_timestamp_ = [&] {
    if (!metrics) {
      return base::TimeTicks::Now();
    }
    CHECK_EQ(metrics->type(), EventMetrics::EventType::kGestureScrollBegin);
    return metrics->GetDispatchStageTimestamp(
        EventMetrics::DispatchStage::kArrivedInRendererCompositor);
  }();
  has_seen_scroll_update_after_begin_ = false;
}

void ScrollSequenceTracker::OnScrollUpdate() {
  has_seen_scroll_update_after_begin_ = true;
}

}  // namespace cc
