// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_SCROLL_TIMING_EMITTER_H_
#define CC_METRICS_SCROLL_TIMING_EMITTER_H_

#include <cstdint>
#include <optional>
#include <vector>

#include "base/time/time.h"
#include "cc/cc_export.h"
#include "cc/metrics/event_metrics.h"
#include "cc/metrics/scroll_jank_v4_frame.h"
#include "cc/metrics/scroll_timing_info.h"
#include "cc/paint/element_id.h"
#include "ui/events/types/scroll_input_type.h"

namespace cc {

// Turns the scroll lifecycle calculated for the scroll jank v4 metric into
// finalized `ScrollTimingInfo` records for the Performance Scroll Timing API.
//
// This class is a read-only consumer: it never takes ownership of an
// `EventMetrics`, never modifies one, and never infers scroll boundaries from
// raw event sequences. Scroll starts and ends come from
// `ScrollJankV4FrameStageCalculator`, and applied movement comes from
// `ScrollUpdateEventMetrics::applied_scroll_observations()`.
class CC_EXPORT ScrollTimingEmitter {
 public:
  ScrollTimingEmitter();
  ~ScrollTimingEmitter();

  ScrollTimingEmitter(const ScrollTimingEmitter&) = delete;
  ScrollTimingEmitter& operator=(const ScrollTimingEmitter&) = delete;

  // Consumes the lifecycle calculated for one `timeline`. `events_metrics` must
  // be the event list `timeline` was calculated from, so that
  // `ScrollEventMetrics::scroll_jank_v4_result_id()` maps each event to its
  // frame.
  void ProcessTimeline(const ScrollJankV4Frame::Timeline& timeline,
                       const EventMetrics::List& events_metrics);

  // Finalizes the active segment; idempotent. Needed because the stages which
  // would otherwise finalize it may never arrive. A flush which records a
  // segment reports nothing further for that gesture; a flush with nothing to
  // record leaves it open, keeping its original start time and target.
  void FlushActiveSegment();

  // Returns the records finalized so far and clears them.
  std::vector<ScrollTimingInfo> TakeCompletedScrollTimingInfos();

 private:
  using ScrollUpdates = ScrollJankV4Frame::Stage::ScrollUpdates;

  struct ActiveSegment {
    // Identifies the gesture this segment belongs to. This is the propagated
    // GestureScrollBegin arrival timestamp that scroll jank v4 uses as a
    // scroll ID, not the segment's `start_time`.
    base::TimeTicks scroll_id;
    base::TimeTicks start_time;
    // Tracks the segment's eventual `ScrollTimingInfo::end_time`.
    std::optional<base::TimeTicks> last_movement_presentation;
    ui::ScrollInputType input_type;
    ElementId element_id;
  };

  void ProcessFrame(const ScrollJankV4Frame& frame,
                    const EventMetrics::List& events_metrics);

  // Finalizes the active segment and drops it, unlike `FlushActiveSegment()`,
  // which can keep it open. Called at a gesture boundary, past which no
  // movement belongs to the finished gesture.
  void EndGesture();

  // Returns the segment the movement presented by `updates` belongs to,
  // opening one if needed. `updates` identifies the gesture and `result_id`
  // the frame; only events matching both are considered.
  //
  // The segment is owned by this class. `EndGesture()`, `FlushActiveSegment()`
  // and the next `ProcessUpdates()` can invalidate the pointer, so the caller
  // must not hold it across stages.
  //
  // Returns null when there is no such segment: the frame moved nothing for
  // this gesture, the gesture is not eligible for a record, or it already
  // reported one.
  ActiveSegment* ProcessUpdates(const ScrollUpdates& updates,
                                const EventMetrics::List& events_metrics,
                                uint64_t result_id);

  // Opens a segment for the gesture `update` belongs to, targeting
  // `element_id`. Does nothing when the Performance Scroll Timing API does not
  // report this scroll's input type, or when `update` carries no
  // GestureScrollBegin timestamp to use as the segment's start time.
  void MaybeOpenActiveSegment(base::TimeTicks scroll_id,
                              const ScrollUpdateEventMetrics& update,
                              ElementId element_id);

  std::optional<ActiveSegment> active_segment_;
  // The ID of the most recent gesture for which a record was already emitted.
  // See the record-once rule in `FlushActiveSegment()`.
  std::optional<base::TimeTicks> last_recorded_scroll_id_;
  // Finalized records, in the order their segments were finalized.
  std::vector<ScrollTimingInfo> completed_scroll_timing_infos_;
};

}  // namespace cc

#endif  // CC_METRICS_SCROLL_TIMING_EMITTER_H_
