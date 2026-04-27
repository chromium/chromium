// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_SCROLL_SEQUENCE_TRACKER_H_
#define CC_METRICS_SCROLL_SEQUENCE_TRACKER_H_

#include "base/time/time.h"
#include "cc/cc_export.h"
#include "cc/metrics/event_metrics.h"

namespace cc {

// Class for tracking the timestamp of the arrival of the most recent scroll
// begin event and whether any scroll updates have arrived since then.
//
// This tracker is intended to be used by classes which process chronologically
// arriving scroll events. It allows them to group scroll events belonging to
// the same scroll and identify the first scroll update in a scroll.
class CC_EXPORT ScrollSequenceTracker {
 public:
  // Note: The argument cannot be of type `const EventMetrics&` because the
  // various `Scroll(Update)EventMetrics::Create*()` methods might return a null
  // pointer, which the tracker needs to handle gracefully.
  void OnScrollBegin(const EventMetrics* metrics);

  void OnScrollUpdate();

  base::TimeTicks scroll_begin_arrival_timestamp() const {
    return scroll_begin_arrival_timestamp_;
  }

  bool has_seen_scroll_update_after_begin() const {
    return has_seen_scroll_update_after_begin_;
  }

 private:
  base::TimeTicks scroll_begin_arrival_timestamp_;
  bool has_seen_scroll_update_after_begin_ = false;
};

}  // namespace cc

#endif  // CC_METRICS_SCROLL_SEQUENCE_TRACKER_H_
