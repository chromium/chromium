// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_SCROLL_JANK_V4_FRAME_STAGE_H_
#define CC_METRICS_SCROLL_JANK_V4_FRAME_STAGE_H_

#include <ostream>
#include <variant>
#include <vector>

#include "base/memory/raw_ref.h"
#include "base/time/time.h"
#include "cc/cc_export.h"
#include "cc/metrics/event_metrics.h"
#include "third_party/abseil-cpp/absl/container/inlined_vector.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace cc {

// A stage of a single frame for the purposes of reporting the scroll jank v4
// metric. Depending on the `EventMetrics` associated with a frame, there might
// be one or more scroll updates (`ScrollJankV4FrameStage::ScrollUpdates`)
// and/or a scroll end in the frame (`ScrollJankV4FrameStage::ScrollEnd`) in
// either order.
struct CC_EXPORT ScrollJankV4FrameStage {
  // A stage that corresponds to one or more scroll updates that were first
  // presented in the frame. If `is_scroll_start` is true, the first scroll
  // update in the frame was a `kFirstGestureScrollUpdate`. All other scroll
  // updates were `kGestureScrollUpdate`s and/or
  // `kInertialGestureScrollUpdate`s.
  struct ScrollUpdates {
    // Whether the first scroll update in the frame was a
    // `kFirstGestureScrollUpdate`.
    bool is_scroll_start;

    // The earliest scroll update included in the frame.
    base::raw_ref<ScrollUpdateEventMetrics> earliest_event;

    // The generation timestamp of the last (coalesced) input included in the
    // frame.
    base::TimeTicks last_input_generation_ts;

    // Whether at least one of the scroll updates included in the frame was a
    // fling (`kInertialGestureScrollUpdate`).
    bool has_inertial_input;

    // The total raw delta (`ScrollUpdateEventMetrics::delta()`) of all scroll
    // updates included in the frame. Can be zero, positive or negative. Note
    // that the individual scroll updates, over which this total is calculated,
    // might have different signs of the raw deltas (e.g. some might have
    // positive raw deltas and some negative raw deltas).
    float total_raw_delta_pixels;

    // The maximum absolute value of raw delta
    // (`ScrollUpdateEventMetrics::delta()`) over all inertial scroll updates
    // included in the frame. Can be zero or positive. Cannot be negative. If
    // positive, `has_inertial_input` must be true.
    float max_abs_inertial_raw_delta_pixels;

    bool operator==(const ScrollUpdates&) const = default;
  };

  // A stage that corresponds to a single scroll end event
  // (`kGestureScrollEnd` or `kInertialGestureScrollEnd`).
  struct ScrollEnd {
    bool operator==(const ScrollEnd&) const = default;
  };

  std::variant<ScrollUpdates, ScrollEnd> stage;

  // A chronologically ordered list of stages. For example, if the list
  // contains a `ScrollEnd` and a `ScrollUpdates` (in this order), then the
  // `ScrollEnd` corresponds to the end of the previous scroll and the
  // `ScrollUpdates` is the start of a new scroll in the frame. The list
  // can contain at most one of each stage, so it's length will be at most 2.
  using List = absl::InlinedVector<ScrollJankV4FrameStage, 2>;

  explicit ScrollJankV4FrameStage(std::variant<ScrollUpdates, ScrollEnd> stage);
  ScrollJankV4FrameStage(const ScrollJankV4FrameStage& stage);
  ~ScrollJankV4FrameStage();

  bool operator==(const ScrollJankV4FrameStage&) const = default;

  // Calculates the scroll jank reporting stages based on `events_metrics`
  // associated with a frame. This function will not modify `events_metrics`
  // in any way. If there's a `ScrollUpdates` stage in the returned list,
  // `ScrollUpdates::earliest_event` will be a reference to an item in
  // `events_metrics` (possibly the same item).
  //
  // `skip_non_damaging_events` controls whether the method ignores non-damaging
  // scroll updates. This allows us to experiment with the legacy behavior of
  // the scroll jank v4 metric (`skip_non_damaging_events=true`) and the new
  // logic for handling non-damaging frames (`skip_non_damaging_events=false`).
  // See `ScrollJankV4Frame` and `ScrollJankV4Decider` for more information.
  // TODO(crbug.com/444183591): Remove `skip_non_damaging_events`.
  static ScrollJankV4FrameStage::List CalculateStages(
      const EventMetrics::List& events_metrics,
      bool skip_non_damaging_events = true);
  static ScrollJankV4FrameStage::List CalculateStages(
      const std::vector<ScrollEventMetrics*>& events_metrics,
      bool skip_non_damaging_events = true);
};

inline std::ostream& operator<<(std::ostream& os,
                                const ScrollJankV4FrameStage& stage) {
  std::visit(
      absl::Overload{
          [&](const ScrollJankV4FrameStage::ScrollUpdates& updates) {
            os << "ScrollUpdates{is_scroll_start: " << updates.is_scroll_start
               << ", earliest_event: " << updates.earliest_event->GetTypeName()
               << "@" << &(*updates.earliest_event)
               << ", last_input_generation_ts: "
               << updates.last_input_generation_ts
               << ", has_inertial_input: " << updates.has_inertial_input
               << ", total_raw_delta_pixels: " << updates.total_raw_delta_pixels
               << ", max_abs_inertial_raw_delta_pixels: "
               << updates.max_abs_inertial_raw_delta_pixels << "}";
          },
          [&](const ScrollJankV4FrameStage::ScrollEnd& end) {
            os << "ScrollEnd{}";
          }},
      stage.stage);
  return os;
}

}  // namespace cc

#endif  // CC_METRICS_SCROLL_JANK_V4_FRAME_STAGE_H_
