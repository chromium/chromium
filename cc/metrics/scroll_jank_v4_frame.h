// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_SCROLL_JANK_V4_FRAME_H_
#define CC_METRICS_SCROLL_JANK_V4_FRAME_H_

#include <ostream>

#include "base/time/time.h"
#include "cc/cc_export.h"
#include "cc/metrics/event_metrics.h"
#include "cc/metrics/scroll_jank_v4_frame_stage.h"
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
  // risking use-after-free).
  struct CC_EXPORT BeginFrameArgsForScrollJank {
    // See `viz::BeginFrameArgs::frame_time`.
    base::TimeTicks frame_time;

    // See `viz::BeginFrameArgs::interval`.
    base::TimeDelta interval;

    // Note: If this were an explicit constructor, it would prevent us from
    // using designated initializers (e.g. `{.frame_time = X, .interval = Y}`).
    static BeginFrameArgsForScrollJank From(const viz::BeginFrameArgs& args);

    bool operator==(const BeginFrameArgsForScrollJank&) const = default;
  };
  BeginFrameArgsForScrollJank args;

  ScrollDamage damage;

  ScrollJankV4FrameStage::List stages;

  // A chronological timeline of frames (both damaging and non-damaging) which
  // contains at least one scroll update or scroll end. The timeline can
  // theoretically contain any number of frames (zero or more), but it typically
  // contains just one frame.
  using Timeline = absl::InlinedVector<ScrollJankV4Frame, 1>;

  ScrollJankV4Frame(BeginFrameArgsForScrollJank args,
                    ScrollDamage damage,
                    ScrollJankV4FrameStage::List stages);
  ScrollJankV4Frame(const ScrollJankV4Frame& frame);
  ~ScrollJankV4Frame();

  bool operator==(const ScrollJankV4Frame&) const = default;

  // Calculates the frame timeline (for the purposes of evaluating scroll jank)
  // based on `events_metrics` which were first presented at `presentation_ts`
  // in a frame with `presented_args`.
  //
  // This method groups scroll updates and ends into frames as follows:
  //
  //   * If there's at least one damaging scroll update/end in `events_metrics`:
  //
  //       1. This method finds the minimum begin frame ID
  //          (`scroll_event.begin_frame_args().frame_id`) over all damaging
  //          scroll events.
  //       2. It associates all scroll events whose begin frame ID is greater
  //          than or equal to the minimum damaging begin frame ID with the
  //          presented frame (`presented_args`). It marks the presented frame
  //          as damaging.
  //       3. It associates all scroll events whose begin frame ID is less than
  //          the minimum damaging begin frame ID with their original frame
  //          (`scroll_event.begin_frame_args()`). It marks each of the original
  //          frames as non-damaging.
  //
  //   * If there are no damaging scroll updates/ends in `events_metrics`, this
  //     method assigns all scroll events to their original frames
  //     (`scroll_event.begin_frame_args()`). It marks each of the original
  //     frames as non-damaging. Note that, if any of the scroll events'
  //     original frame is the presented frame (i.e.
  //     `scroll_event.begin_frame_args().frame_id == presented_args.frame_id`),
  //     this method will mark the presented frame as non-damaging as well.
  //
  // This method returns a timeline of frames sorted in ascending order of begin
  // frame ID (`frame.args->frame_id`). Each frame in the returned timeline is
  // guaranteed to have different begin frame arguments. This method assumes
  // that the presented frame's begin frame ID is greater than or equal to the
  // begin frame IDs of all scroll events in `events_metrics` (i.e.
  // `presented_frame.frame_id >= scroll_event.begin_frame_args().frame_id`).
  // Given this assumption, all frames in the returned timeline will be
  // non-damaging EXCEPT the last frame, which can be either damaging or
  // non-damaging. In other words, the timeline matches the informal regular
  // expression "(non-damaging-frame)*(damaging-frame)?". If the last frame is
  // damaging, its begin frame arguments will be `presented_args`. If
  // `events_metrics` doesn't contain any scroll updates/ends, this method will
  // return an empty timeline.
  //
  // For example, given the following events (not necessarily provided in this
  // order):
  //
  //    1. Non-damaging GSB for BFA1
  //    2. Non-damaging GSU for BFA1
  //    3. Non-damaging GSU for BFA1
  //    4. Non-damaging GSE for BFA2
  //    5. Non-damaging GSU for BFA3
  //    6. Damaging GSU for BFA3
  //    7. Non-damaging GSU for BFA4
  //    8. Non-damaging GSU for BFA4
  //    9. Damaging GSU for BFA5
  //   10. Non-damaging GSU for BFA5
  //
  // and presented begin frame arguments BFA6 where:
  //
  //   * GSB is a scroll start (`ScrollEventMetrics` of type
  //     `kGestureScrollBegin`).
  //   * GSU is a scroll update (`ScrollUpdateEventMetrics` of type
  //     `kFirstGestureScrollUpdate`, `kGestureScrollUpdate` or
  //     `kInertialGestureScrollUpdate`)
  //   * GSE is a scroll end (`ScrollEventMetrics` of type `kGestureScrollEnd`
  //     or `kInertialGestureScrollEnd`).
  //   * BFA1-BFA6 are `viz::BeginFrameArgs` sorted by
  //     `viz::BeginFrameArgs::frame_id` in ascending/chronological order.
  //
  // this method will return three frames (in this order):
  //
  //   a. Non-damaging frame with BFA1 and events 2-3.
  //   b. Non-damaging frame with BFA2 and event 4.
  //   c. Damaging frame with BFA6 and eccents 5-10.
  //
  // Note that, since event 5 is damaging, BFA3-BFA5 are damaging, so this
  // method associates events that were originally associated with BFA3 (5-6),
  // BFA4 (7-8) and BFA5 (9-10) with the presented frame (BFA6). The method only
  // cares about GSUs and GSEs, so it will ignore the initial GSB (1).
  //
  // This method does NOT require that `events_metrics` is sorted. It will not
  // modify `events_metrics` in any way. All pointers in the result point to
  // events in `events_metrics`.
  static ScrollJankV4Frame::Timeline CalculateTimeline(
      const EventMetrics::List& events_metrics,
      const viz::BeginFrameArgs& presented_args,
      base::TimeTicks presentation_ts);
};

inline std::ostream& operator<<(
    std::ostream& os,
    const ScrollJankV4Frame::BeginFrameArgsForScrollJank& args) {
  return os << "BeginFrameArgsForScrollJank{frame_time: " << args.frame_time
            << ", interval: " << args.interval << "}";
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
