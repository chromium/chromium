// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_SCROLL_JANK_V4_FRAME_H_
#define CC_METRICS_SCROLL_JANK_V4_FRAME_H_

#include <concepts>
#include <ostream>
#include <variant>

#include "base/time/time.h"
#include "cc/cc_export.h"
#include "cc/metrics/event_metrics.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "third_party/abseil-cpp/absl/container/inlined_vector.h"

namespace cc {

// Information about a single frame which contains at least one scroll update
// or scroll end.
struct CC_EXPORT ScrollJankV4Frame {
  // Representation of a presented damaging frame. See `ScrollDamage` below.
  struct DamagingFrame {
    // When the frame was presented to the user.
    base::TimeTicks presentation_ts;

    bool operator==(const DamagingFrame&) const = default;
  };

  // Representation of a non-damaging frame. See `ScrollDamage` below.
  struct NonDamagingFrame {
    bool operator==(const NonDamagingFrame&) const = default;
  };

  // Information about a frame's scroll damage. A frame F is non-damaging if the
  // following conditions are BOTH true:
  //
  //   1. All scroll updates in F are non-damaging. A scroll update
  //      is non-damaging if it didn't cause a frame update (i.e.
  //      `EventMetrics::caused_frame_update()` is false) and/or didn't change
  //      the scroll offset (i.e. `ScrollEventMetrics::did_scroll()` is
  //      false).
  //
  //   2. All frames between (both ends exclusive):
  //        a. the last frame presented by Chrome before F and
  //        b. F
  //      are non-damaging.
  using ScrollDamage = std::variant<DamagingFrame, NonDamagingFrame>;

  // A small number of fields from `viz::BeginFrameArgs` relevant to scroll jank
  // to avoid copying the whole args (>100 bytes) or holding a reference (and
  // risking use-after-free). Plus a few frame-related fields which aren't
  // present in `viz::BeginFrameArgs`.
  struct CC_EXPORT BeginFrameArgsForScrollJank {
    // See `viz::BeginFrameArgs::frame_time`.
    base::TimeTicks frame_time;

    // See `viz::BeginFrameArgs::interval`.
    base::TimeDelta interval;

    // The ID of the result of the scroll jank V4 metric for the frame. This ID
    // makes it easy to retrieve the mapping between "EventLatency" and
    // "ScrollJankV4" slices in traces.
    //
    // Note: This field isn't present in `viz::BeginFrameArgs`.
    uint64_t result_id;

    // Note: If these were explicit constructors, they would prevent us from
    // using designated initializers (e.g. `{.frame_time = X, .interval = Y}`).
    static BeginFrameArgsForScrollJank From(const viz::BeginFrameArgs& args,
                                            uint64_t result_id);
    static BeginFrameArgsForScrollJank From(
        const ScrollEventMetrics::DispatchBeginFrameArgs& args,
        uint64_t result_id);

    bool operator==(const BeginFrameArgsForScrollJank&) const = default;
  };

  // A stage of a single frame for the purposes of reporting the scroll jank v4
  // metric. Depending on the `EventMetrics` associated with a frame, there
  // might be one or more scroll updates (`ScrollUpdates`) and/or a scroll end
  // in the frame (`ScrollEnd`) in either order.
  struct CC_EXPORT Stage {
    // A stage that corresponds to the beginning of a scroll (triggered by
    // `kFirstGestureScrollUpdate`).
    struct CC_EXPORT ScrollStart {
      bool operator==(const ScrollStart&) const = default;
    };

    // A stage that corresponds to one or more scroll updates that were first
    // presented in a frame (one or more `EventType::kFirstGestureScrollUpdate`,
    // `EventType::kGestureScrollUpdate`(s) and/or
    // `EventType::kInertialGestureScrollUpdate`(s)).
    class CC_EXPORT ScrollUpdates {
     public:
      // Real scroll updates which originated from hardware/OS.
      struct CC_EXPORT Real {
        base::TimeTicks first_input_generation_ts;
        base::TimeTicks last_input_generation_ts;
        bool has_inertial_input;
        float abs_total_raw_delta_pixels;
        float max_abs_inertial_raw_delta_pixels;
        std::optional<EventMetrics::TraceId> first_input_trace_id;

        bool operator==(const Real&) const = default;
      };

      // Synthetic scroll updates which were predicted by Chrome.
      struct CC_EXPORT Synthetic {
        base::TimeTicks first_input_begin_frame_ts;
        bool has_inertial_input = false;
        std::optional<EventMetrics::TraceId> first_input_trace_id;

        bool operator==(const Synthetic&) const = default;
      };

      // At least one of `real` or `synthetic` must contain a value.
      ScrollUpdates(std::optional<Real> real,
                    std::optional<Synthetic> synthetic,
                    std::optional<base::TimeTicks>
                        scroll_begin_arrival_timestamp = std::nullopt);

      bool operator==(const ScrollUpdates&) const = default;

      const std::optional<Real>& real() const { return real_; }
      const std::optional<Synthetic>& synthetic() const { return synthetic_; }
      const std::optional<base::TimeTicks>& scroll_begin_arrival_timestamp()
          const {
        return scroll_begin_arrival_timestamp_;
      }

     private:
      const std::optional<Real> real_;
      const std::optional<Synthetic> synthetic_;
      const std::optional<base::TimeTicks> scroll_begin_arrival_timestamp_;
    };

    // A stage that corresponds to a single scroll end event
    // (`kGestureScrollEnd` or `kInertialGestureScrollEnd`).
    struct CC_EXPORT ScrollEnd {
      bool operator==(const ScrollEnd&) const = default;
    };

    std::variant<ScrollStart, ScrollUpdates, ScrollEnd> stage;

    explicit Stage(std::variant<ScrollStart, ScrollUpdates, ScrollEnd> stage);
    Stage(const Stage& stage);
    ~Stage();

    bool operator==(const Stage&) const = default;
  };

  // A chronologically ordered list of stages. For example, if the list contains
  // a `ScrollEnd`, a `ScrollStart` and a `ScrollUpdates` (in this order), then
  // the `ScrollEnd` corresponds to the end of the previous scroll,
  // `ScrollStart` corresponds to the start of the new scroll and
  // `ScrollUpdates` are the first scroll updates in the new scroll. The list
  // can contain at most one of each stage, so its length will be at most 3.
  using StageList = absl::InlinedVector<Stage, 3>;

  BeginFrameArgsForScrollJank args;

  ScrollDamage damage;

  StageList stages;

  // A chronological timeline of frames (both damaging and non-damaging) which
  // contains at least one scroll update or scroll end. The timeline can
  // theoretically contain any number of frames (zero or more), but it typically
  // contains just one frame.
  using Timeline = absl::InlinedVector<ScrollJankV4Frame, 1>;

  ScrollJankV4Frame(BeginFrameArgsForScrollJank args,
                    ScrollDamage damage,
                    StageList stages);
  ScrollJankV4Frame(const ScrollJankV4Frame& frame);
  ~ScrollJankV4Frame();

  bool operator==(const ScrollJankV4Frame&) const = default;
};

inline std::ostream& operator<<(
    std::ostream& os,
    const ScrollJankV4Frame::BeginFrameArgsForScrollJank& args) {
  return os << "BeginFrameArgsForScrollJank{frame_time: " << args.frame_time
            << ", interval: " << args.interval
            << ", result_id: " << args.result_id << "}";
}

inline std::ostream& operator<<(
    std::ostream& os,
    const ScrollJankV4Frame::DamagingFrame& damaging_frame) {
  return os << "DamagingFrame{presentation_ts: "
            << damaging_frame.presentation_ts << "}";
}

inline std::ostream& operator<<(
    std::ostream& os,
    const ScrollJankV4Frame::NonDamagingFrame& non_damaging_frame) {
  return os << "NonDamagingFrame{}";
}

inline std::ostream& operator<<(std::ostream& os,
                                const ScrollJankV4Frame::ScrollDamage damage) {
  os << "ScrollDamage{";
  std::visit([&os](const auto& value) { os << value; }, damage);
  return os << "}";
}

template <typename T, typename... Ts>
concept IsOneOf = (std::same_as<T, Ts> || ...);

template <typename T>
  requires IsOneOf<T,
                   ScrollJankV4Frame::Stage::ScrollUpdates::Real,
                   ScrollJankV4Frame::Stage::ScrollUpdates::Synthetic,
                   EventMetrics::TraceId,
                   base::TimeTicks>
inline std::ostream& operator<<(std::ostream& os,
                                const std::optional<T>& value) {
  if (value.has_value()) {
    return os << *value;
  }
  return os << "empty";
}

inline std::ostream& operator<<(
    std::ostream& os,
    const ScrollJankV4Frame::Stage::ScrollUpdates::Real& real_updates) {
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
    const ScrollJankV4Frame::Stage::ScrollUpdates::Synthetic&
        synthetic_updates) {
  return os << "Synthetic{first_input_begin_frame_ts: "
            << synthetic_updates.first_input_begin_frame_ts
            << ", has_inertial_input: " << synthetic_updates.has_inertial_input
            << ", first_input_trace_id: "
            << synthetic_updates.first_input_trace_id << "}";
}

inline std::ostream& operator<<(
    std::ostream& os,
    const ScrollJankV4Frame::Stage::ScrollUpdates& updates) {
  return os << "ScrollUpdates{real: " << updates.real()
            << ", synthetic: " << updates.synthetic()
            << ", scroll_begin_arrival_timestamp: "
            << updates.scroll_begin_arrival_timestamp() << "}";
}

inline std::ostream& operator<<(
    std::ostream& os,
    const ScrollJankV4Frame::Stage::ScrollStart& start) {
  return os << "ScrollStart{}";
}

inline std::ostream& operator<<(
    std::ostream& os,
    const ScrollJankV4Frame::Stage::ScrollEnd& end) {
  return os << "ScrollEnd{}";
}

inline std::ostream& operator<<(std::ostream& os,
                                const ScrollJankV4Frame::Stage& stage) {
  return std::visit([&os](const auto& s) -> std::ostream& { return os << s; },
                    stage.stage);
}

inline std::ostream& operator<<(std::ostream& os,
                                const ScrollJankV4Frame& frame) {
  os << "ScrollJankV4Frame{args: " << frame.args << ", damage: " << frame.damage
     << ", stages: [";
  bool is_first = true;
  for (const auto& stage : frame.stages) {
    if (is_first) {
      is_first = false;
    } else {
      os << ", ";
    }
    os << stage;
  }
  return os << "]}";
}

}  // namespace cc

#endif  // CC_METRICS_SCROLL_JANK_V4_FRAME_H_
