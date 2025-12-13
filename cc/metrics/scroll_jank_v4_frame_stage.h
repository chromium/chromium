// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_SCROLL_JANK_V4_FRAME_STAGE_H_
#define CC_METRICS_SCROLL_JANK_V4_FRAME_STAGE_H_

#include <concepts>
#include <ostream>
#include <variant>
#include <vector>

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
  // The result or issues encountered by the `CalculateStages()` method when
  // processing scroll events in a single frame.
  // LINT.IfChange(FrameStageCalculationResult)
  enum class FrameStageCalculationResult {
    // The frame only contained one or more scroll updates.
    kScrollUpdatesOnly = 0,

    // The frame only contained one scroll end (and nothing else).
    kScrollEndOnly = 1,

    // The frame contained one or more scroll updates followed by one scroll
    // end.
    kScrollUpdatesThenEnd = 2,

    // The frame contained one scroll end, one scroll start and then zero or
    // more scroll updates (in this order).
    kScrollEndThenStartThenUpdates = 3,

    // The frame contained multiple scroll ends (unexpected issue).
    kMultipleScrollEnds = 4,

    // The frame contained multiple scroll starts (unexpected issue).
    kMultipleScrollStarts = 5,

    // The frame contained a scroll update followed by a scroll start
    // (unexpected issue).
    kScrollStartAfterUpdate = 6,

    // The frame contained a scroll end between two scroll updates (unexpected
    // issue).
    kScrollEndBetweenUpdates = 7,

    // The frame contained a scroll end and then one or more scroll updates
    // without a scroll start in between (unexpected issue).
    kScrollEndThenUpdatesWithoutStart = 8,

    // The frame suffered from more than one unexpected issue.
    kMultipleIssues = 9,

    // The frame contained one scroll start followed by one or more scroll
    // updates.
    kScrollStartThenUpdates = 10,

    // The frame contained one scroll start, one or more scroll updates and then
    // one scroll end (in this order). It's strange to have a scroll that starts
    // and ends within the same frame, but it's technically not an issue.
    kScrollStartThenUpdatesThenEnd = 11,

    kMaxValue = kScrollStartThenUpdatesThenEnd,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/event/enums.xml:ScrollJankFrameStageCalculationResult)

  // A stage that corresponds to the beginning of a scroll (triggered by
  // `kFirstGestureScrollUpdate`).
  struct CC_EXPORT ScrollStart {
    bool operator==(const ScrollStart&) const = default;
  };

  // A stage that corresponds to one or more scroll updates that were first
  // presented in a frame (one or more
  // `EventType::kFirstGestureScrollUpdate`,
  // `EventType::kGestureScrollUpdate`(s) and/or
  // `EventType::kInertialGestureScrollUpdate`(s)).
  //
  // Can be in one of the following 3 states:
  //
  //   1. The frame contains only real scroll updates.
  //   2. The frame contains only synthetic scroll updates.
  //   3. The frame contains both real and synthetic scroll updates.
  //
  // Consequently, at least one of `ScrollInputs::real()` and
  // `ScrollInputs::synthetic()` is guaranteed to contain a value.
  //
  // We say that a frame is "synthetic" if all scroll updates in the frame are
  // synthetic (state 2 above). We say that a frame is "real" if at least one
  // scroll update in the frame is real (states 1 and 3 above).
  class CC_EXPORT ScrollUpdates {
   public:
    // Real scroll updates which originated from hardware/OS.
    struct CC_EXPORT Real {
      // The generation timestamp of the first non-synthetic scroll update
      // included (coalesced) in the frame. See
      // `EventMetrics::GetDispatchStageTimestamp()` and
      // `EventMetrics::DispatchStage::kGenerated`.
      base::TimeTicks first_input_generation_ts;

      // The generation timestamp of the last non-synthetic scroll update
      // included (coalesced) in the frame. See
      // `EventMetrics::GetDispatchStageTimestamp()` and
      // `EventMetrics::DispatchStage::kGenerated`.
      base::TimeTicks last_input_generation_ts;

      // Whether at least one of the scroll updates is a fling
      // (`EventType::kInertialGestureScrollUpdate`).
      bool has_inertial_input;

      // The absolute total raw delta (`ScrollUpdateEventMetrics::delta()`) of
      // all scroll updates. Can be zero or positive. Cannot be negative.
      //
      // Note that the individual scroll updates, over which this total is
      // calculated, might have different signs of the raw deltas (e.g. some
      // might have positive raw deltas and some negative raw deltas).
      //
      // For example, if the frame contained three real inputs with scroll
      // deltas of -1, 3, and -4 pixels respectively, this field would be equal
      // to |(-1) + 3 + (-4)| = 2.
      float abs_total_raw_delta_pixels;

      // The maximum absolute value of raw delta
      // (`ScrollUpdateEventMetrics::delta()`) over all inertial scroll updates.
      // Can be zero or positive. Cannot be negative. If positive,
      // `has_inertial_input` must be true.
      //
      // Note that the individual inertial scroll updates, over which this max
      // is calculated, might have different signs of the raw deltas (e.g. some
      // might have positive raw deltas and some negative raw deltas).
      //
      // For example, if the frame contained three real inputs with scroll
      // deltas of -1 (regular), 3 (inertial), and -4 (inertial) pixels
      // respectively, this field would be equal to max(|3|, |-4|) = 4.
      float max_abs_inertial_raw_delta_pixels;

      // Trace ID of the first real input in the frame, whose input generation
      // timestamp is equal to `first_input_generation_ts`.
      std::optional<EventMetrics::TraceId> first_input_trace_id;

      bool operator==(const Real&) const = default;
    };

    // Synthetic scroll updates which were predicted by Chrome (see
    // `blink::ScrollPredictor::GenerateSyntheticScrollUpdate()`). In contrast
    // to real scroll updates, the scroll jank metric cannot "trust" synthetic
    // scroll updates' input generation timestamps and raw scroll deltas because
    // the scroll updates didn't originate from hardware/OS.
    struct CC_EXPORT Synthetic {
      // The begin frame timestamp (`viz::BeginFrameArgs::frame_time` in
      // `ScrollEventMetrics::begin_frame_args()`) of the first synthetic scroll
      // update.
      base::TimeTicks first_input_begin_frame_ts;

      // Trace ID of the first synthetic input in the frame, whose begin frame
      // timestamp is equal to `first_input_begin_frame_ts`.
      std::optional<EventMetrics::TraceId> first_input_trace_id;

      bool operator==(const Synthetic&) const = default;
    };

    // At least one of `real` and `synthetic` must be non-empty.
    ScrollUpdates(std::optional<Real> real, std::optional<Synthetic> synthetic);

    bool operator==(const ScrollUpdates&) const = default;

    const std::optional<Real>& real() const { return real_; }
    const std::optional<Synthetic>& synthetic() const { return synthetic_; }

   private:
    const std::optional<Real> real_;
    const std::optional<Synthetic> synthetic_;
  };

  // A stage that corresponds to a single scroll end event
  // (`kGestureScrollEnd` or `kInertialGestureScrollEnd`).
  struct CC_EXPORT ScrollEnd {
    bool operator==(const ScrollEnd&) const = default;
  };

  std::variant<ScrollStart, ScrollUpdates, ScrollEnd> stage;

  // A chronologically ordered list of stages. For example, if the list
  // contains a `ScrollEnd`, a `ScrollStart` and a `ScrollUpdates` (in this
  // order), then the `ScrollEnd` corresponds to the end of the previous scroll,
  // `ScrollStart` corresponds to the start of the new scroll and
  // `ScrollUpdates` are the first scroll updates in the new scroll. The list
  // can contain at most one of each stage, so its length will be at most 3.
  using List = absl::InlinedVector<ScrollJankV4FrameStage, 3>;

  explicit ScrollJankV4FrameStage(
      std::variant<ScrollStart, ScrollUpdates, ScrollEnd> stage);
  ScrollJankV4FrameStage(const ScrollJankV4FrameStage& stage);
  ~ScrollJankV4FrameStage();

  bool operator==(const ScrollJankV4FrameStage&) const = default;

  // Calculates the scroll jank reporting stages based on `events_metrics`
  // associated with a frame.
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

template <typename T, typename... Ts>
concept IsOneOf = (std::same_as<T, Ts> || ...);

template <typename T>
  requires IsOneOf<T,
                   ScrollJankV4FrameStage::ScrollUpdates::Real,
                   ScrollJankV4FrameStage::ScrollUpdates::Synthetic,
                   EventMetrics::TraceId>
inline std::ostream& operator<<(std::ostream& os,
                                const std::optional<T>& value) {
  if (value.has_value()) {
    return os << *value;
  }
  return os << "empty";
}

inline std::ostream& operator<<(
    std::ostream& os,
    const ScrollJankV4FrameStage::ScrollUpdates::Real& real_updates) {
  return os << "Real{first_input_generation_ts: "
            << real_updates.first_input_generation_ts
            << ", last_input_generation_ts: "
            << real_updates.last_input_generation_ts
            << ", has_inertial_input: " << real_updates.has_inertial_input
            << ", abs_total_raw_delta_pixels: "
            << real_updates.abs_total_raw_delta_pixels
            << ", max_abs_inertial_raw_delta_pixels: "
            << real_updates.max_abs_inertial_raw_delta_pixels
            << ", first_input_trace_id: " << real_updates.first_input_trace_id
            << "}";
}

inline std::ostream& operator<<(
    std::ostream& os,
    const ScrollJankV4FrameStage::ScrollUpdates::Synthetic& synthetic_updates) {
  return os << "Synthetic{first_input_begin_frame_ts: "
            << synthetic_updates.first_input_begin_frame_ts
            << ", first_input_trace_id: "
            << synthetic_updates.first_input_trace_id << "}";
}

inline std::ostream& operator<<(
    std::ostream& os,
    const ScrollJankV4FrameStage::ScrollUpdates& updates) {
  return os << "ScrollUpdates{real: " << updates.real()
            << ", synthetic: " << updates.synthetic() << "}";
}

inline std::ostream& operator<<(
    std::ostream& os,
    const ScrollJankV4FrameStage::ScrollStart& start) {
  return os << "ScrollStart{}";
}

inline std::ostream& operator<<(std::ostream& os,
                                const ScrollJankV4FrameStage::ScrollEnd& end) {
  return os << "ScrollEnd{}";
}

inline std::ostream& operator<<(std::ostream& os,
                                const ScrollJankV4FrameStage& stage) {
  return std::visit(
      [&os](const auto& stage) -> std::ostream& { return os << stage; },
      stage.stage);
}

}  // namespace cc

#endif  // CC_METRICS_SCROLL_JANK_V4_FRAME_STAGE_H_
