// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/scroll_sequence_tracker.h"

#include "base/time/time.h"
#include "cc/metrics/event_metrics.h"

namespace cc {

void ScrollSequenceTracker::OnScrollBegin(const EventMetrics* metrics) {
  if (metrics) {
    CHECK_EQ(metrics->type(), EventMetrics::EventType::kGestureScrollBegin);
    scroll_begin_generated_timestamp_ = metrics->GetDispatchStageTimestamp(
        EventMetrics::DispatchStage::kGenerated);
    scroll_begin_arrival_timestamp_ = metrics->GetDispatchStageTimestamp(
        EventMetrics::DispatchStage::kArrivedInRendererCompositor);
  } else {
    // Nothing can stand in for the generated timestamp; see
    // `ScrollEventMetrics::scroll_begin_generated_timestamp()`.
    scroll_begin_generated_timestamp_ = base::TimeTicks();
    // In contrast, `Now()` is an accurate arrival timestamp because this code
    // runs when the scroll begin arrives.
    scroll_begin_arrival_timestamp_ = base::TimeTicks::Now();
  }
  has_seen_scroll_update_after_begin_ = false;
}

void ScrollSequenceTracker::OnScrollUpdate() {
  has_seen_scroll_update_after_begin_ = true;
}

}  // namespace cc
