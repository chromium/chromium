// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/scroll_timing_emitter.h"

#include <memory>
#include <utility>
#include <variant>

#include "base/check.h"
#include "base/notreached.h"
#include "base/types/optional_util.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace cc {

namespace {

// Returns the `ui::ScrollInputType` exposed by the Performance Scroll Timing
// API, or `std::nullopt` for scroll types the API does not report yet.
// TODO(crbug.com/504094429): Report scrollbar and autoscroll scrolls once their
// applied movement is observed.
std::optional<ui::ScrollInputType> ToExposedInputType(
    ScrollEventMetrics::ScrollType scroll_type) {
  switch (scroll_type) {
    case ScrollEventMetrics::ScrollType::kTouchscreen:
      return ui::ScrollInputType::kTouchscreen;
    case ScrollEventMetrics::ScrollType::kWheel:
      return ui::ScrollInputType::kWheel;
    case ScrollEventMetrics::ScrollType::kAutoscroll:
    case ScrollEventMetrics::ScrollType::kScrollbar:
      return std::nullopt;
  }
  NOTREACHED();
}

}  // namespace

ScrollTimingEmitter::ScrollTimingEmitter() = default;

ScrollTimingEmitter::~ScrollTimingEmitter() = default;

void ScrollTimingEmitter::ProcessTimeline(
    const ScrollJankV4Frame::Timeline& timeline,
    const EventMetrics::List& events_metrics) {
  for (const ScrollJankV4Frame& frame : timeline) {
    ProcessFrame(frame, events_metrics);
  }
}

void ScrollTimingEmitter::FlushActiveSegment() {
  if (!active_segment_.has_value()) {
    return;
  }
  // Nothing orders `start_time` against presentation timestamps, and
  // `ScrollSequenceTracker` does no gesture matching, so it can even belong to
  // a different gesture. A segment whose end would precede its start is
  // therefore a record to drop, not a broken invariant.
  if (!active_segment_->last_movement_presentation.has_value() ||
      active_segment_->start_time >=
          *active_segment_->last_movement_presentation) {
    // The segment stays open. Outside a gesture boundary the flush cannot tell
    // whether the gesture is over, and re-opening the segment on later movement
    // would re-derive its target from a later frame, naming a scroller which is
    // not the gesture's first moved one.
    return;
  }
  completed_scroll_timing_infos_.push_back(ScrollTimingInfo{
      .start_time = active_segment_->start_time,
      .end_time = *active_segment_->last_movement_presentation,
      .input_type = active_segment_->input_type,
      .element_id = active_segment_->element_id,
  });
  // A gesture reports at most one record. A mid-gesture flush would otherwise
  // let later movement re-open a segment with the same propagated
  // GestureScrollBegin timestamp, nesting a second record inside this one. A
  // real segment boundary needs a `start_time` that is not the scroll begin.
  last_recorded_scroll_id_ = active_segment_->scroll_id;
  active_segment_.reset();
}

std::vector<ScrollTimingInfo>
ScrollTimingEmitter::TakeCompletedScrollTimingInfos() {
  return std::exchange(completed_scroll_timing_infos_, {});
}

void ScrollTimingEmitter::ProcessFrame(
    const ScrollJankV4Frame& frame,
    const EventMetrics::List& events_metrics) {
  for (const ScrollJankV4Frame::Stage& stage : frame.stages) {
    std::visit(
        absl::Overload{
            [&](const ScrollJankV4Frame::Stage::ScrollStart&) { EndGesture(); },
            [&](const ScrollUpdates& updates) {
              ActiveSegment* segment =
                  ProcessUpdates(updates, events_metrics, frame.args.result_id);
              if (!segment) {
                return;
              }
              if (const auto* damaging_frame =
                      std::get_if<ScrollJankV4Frame::DamagingFrame>(
                          &frame.damage)) {
                segment->last_movement_presentation =
                    damaging_frame->presentation_ts;
              }
            },
            [&](const ScrollJankV4Frame::Stage::ScrollEnd&) { EndGesture(); },
        },
        stage.stage);
  }
}

void ScrollTimingEmitter::EndGesture() {
  FlushActiveSegment();
  active_segment_.reset();
}

ScrollTimingEmitter::ActiveSegment* ScrollTimingEmitter::ProcessUpdates(
    const ScrollUpdates& updates,
    const EventMetrics::List& events_metrics,
    uint64_t result_id) {
  // The stage's scroll ID identifies the gesture the calculator attributed the
  // frame's updates to.
  const base::TimeTicks scroll_id = [&] {
    const std::optional<base::TimeTicks>& optional_scroll_id =
        updates.scroll_begin_arrival_timestamp();
    CHECK(optional_scroll_id.has_value());
    return *optional_scroll_id;
  }();
  // Guaranteed by `ScrollJankV4FrameStageCalculator`, which never lowers its
  // current scroll ID.
  DCHECK(!last_recorded_scroll_id_ || *last_recorded_scroll_id_ <= scroll_id);
  if (last_recorded_scroll_id_ == scroll_id) {
    return nullptr;
  }

  // The segment's target is the earliest movement in the frame, ordered by
  // `AppliedScrollObservation::update_input_timestamp` with ties broken by
  // event list order.
  const ScrollUpdateEventMetrics* first_moved_update = nullptr;
  const ScrollUpdateEventMetrics::AppliedScrollObservation* first_observation =
      nullptr;
  for (const std::unique_ptr<EventMetrics>& event : events_metrics) {
    // The event list spans the whole timeline, and
    // `ScrollJankV4FrameStageCalculator` stamps every scroll event it is given
    // with its frame's result ID before any eligibility filtering. A frame can
    // also carry updates from several gestures, in which case the calculator
    // builds the stage for the lowest scroll ID only. Both filters are
    // therefore needed to select the updates this stage was built from.
    const ScrollUpdateEventMetrics* update = event->AsScrollUpdate();
    if (!update || update->scroll_jank_v4_result_id() != result_id ||
        update->scroll_begin_arrival_timestamp() != scroll_id) {
      continue;
    }
    for (const auto& observation : update->applied_scroll_observations()) {
      if (!first_observation || observation.update_input_timestamp <
                                    first_observation->update_input_timestamp) {
        first_moved_update = update;
        first_observation = &observation;
      }
    }
  }

  if (!first_moved_update) {
    return nullptr;
  }
  // `ScrollJankV4FrameStageCalculator` emits a `ScrollStart` before the first
  // `ScrollUpdates` of a new scroll ID, and that flushes the previous segment,
  // so an open segment here always belongs to this stage's gesture.
  if (active_segment_.has_value()) {
    DCHECK_EQ(active_segment_->scroll_id, scroll_id);
  } else {
    MaybeOpenActiveSegment(scroll_id, *first_moved_update,
                           first_observation->element_id);
  }
  return base::OptionalToPtr(active_segment_);
}

void ScrollTimingEmitter::MaybeOpenActiveSegment(
    base::TimeTicks scroll_id,
    const ScrollUpdateEventMetrics& update,
    ElementId element_id) {
  CHECK(!active_segment_.has_value());
  const std::optional<ui::ScrollInputType> input_type =
      ToExposedInputType(update.scroll_type());
  const base::TimeTicks start_time = update.scroll_begin_generated_timestamp();
  if (!input_type.has_value() || start_time.is_null()) {
    return;
  }
  active_segment_ = ActiveSegment{
      .scroll_id = scroll_id,
      .start_time = start_time,
      .input_type = *input_type,
      .element_id = element_id,
  };
}

}  // namespace cc
