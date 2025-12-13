// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/scroll_jank_v4_frame.h"

#include <algorithm>
#include <map>
#include <memory>
#include <optional>
#include <vector>

#include "base/check_op.h"
#include "cc/metrics/event_metrics.h"
#include "cc/metrics/scroll_jank_v4_frame_stage.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "third_party/abseil-cpp/absl/container/inlined_vector.h"

namespace cc {

namespace {

ScrollEventMetrics* AsScrollUpdateOrEnd(EventMetrics& event) {
  switch (event.type()) {
    case EventMetrics::EventType::kFirstGestureScrollUpdate:
    case EventMetrics::EventType::kGestureScrollUpdate:
    case EventMetrics::EventType::kInertialGestureScrollUpdate:
    case EventMetrics::EventType::kGestureScrollEnd:
    case EventMetrics::EventType::kInertialGestureScrollEnd:
      return event.AsScroll();
    default:
      return nullptr;
  }
}

// Information about frames associated with scroll updates and scroll ends in an
// `EventMetrics::List`.
struct FrameBounds {
  // The minimum frame ID associated with any scroll update or scroll end in the
  // list.
  viz::BeginFrameId min_id;

  // The maximum frame ID associated with any scroll update or scroll end in the
  // list.
  viz::BeginFrameId max_id;

  // The minimum frame ID associated with a damaging scroll update or scroll end
  // in the list. Empty if there are no damaging scroll updates/ends in the
  // list.
  std::optional<viz::BeginFrameId> min_damaging_id;

  // Begin frame args associated with an arbitrary scroll update or scroll end
  // in the list.
  ScrollJankV4Frame::BeginFrameArgsForScrollJank some_args;

  bool IsDamaging(const viz::BeginFrameId& frame_id) const {
    return min_damaging_id.has_value() && frame_id >= *min_damaging_id;
  }
};

// Information about frames associated with scroll updates and scroll ends in
// `events_metrics`. The return value is empty if and only if `events_metrics`
// doesn't contain any scroll updates/ends.
std::optional<FrameBounds> GetFrameBounds(
    const EventMetrics::List& events_metrics) {
  std::optional<FrameBounds> result = std::nullopt;
  for (const auto& event : events_metrics) {
    ScrollEventMetrics* scroll_event = AsScrollUpdateOrEnd(*event);
    if (!scroll_event) {
      continue;
    }
    const ScrollEventMetrics::DispatchBeginFrameArgs& args =
        scroll_event->dispatch_args();
    viz::BeginFrameId frame_id = args.frame_id;
    bool is_damaging = [&] {
      if (!scroll_event->caused_frame_update()) {
        return false;
      }
      ScrollUpdateEventMetrics* scroll_update_event =
          scroll_event->AsScrollUpdate();
      if (scroll_update_event == nullptr) {
        return false;
      }
      return scroll_update_event->did_scroll();
    }();

    if (result.has_value()) {
      result->min_id = std::min(result->min_id, frame_id);
      result->max_id = std::max(result->max_id, frame_id);
      if (is_damaging) {
        result->min_damaging_id =
            result->min_damaging_id.has_value()
                ? std::min(*result->min_damaging_id, frame_id)
                : frame_id;
      }
    } else {
      result = FrameBounds{
          .min_id = frame_id,
          .max_id = frame_id,
          .min_damaging_id = is_damaging
                                 ? std::optional<viz::BeginFrameId>(frame_id)
                                 : std::nullopt,
          .some_args =
              ScrollJankV4Frame::BeginFrameArgsForScrollJank::From(args),
      };
    }
  }
  return result;
}

}  // namespace

// static
ScrollJankV4Frame::BeginFrameArgsForScrollJank
ScrollJankV4Frame::BeginFrameArgsForScrollJank::From(
    const viz::BeginFrameArgs& args) {
  return {.frame_time = args.frame_time, .interval = args.interval};
}

// static
ScrollJankV4Frame::BeginFrameArgsForScrollJank
ScrollJankV4Frame::BeginFrameArgsForScrollJank::From(
    const ScrollEventMetrics::DispatchBeginFrameArgs& args) {
  return {.frame_time = args.frame_time, .interval = args.interval};
}

ScrollJankV4Frame::ScrollJankV4Frame(BeginFrameArgsForScrollJank args,
                                     ScrollDamage damage,
                                     ScrollJankV4FrameStage::List stages)
    : args(args), damage(damage), stages(stages) {}

ScrollJankV4Frame::ScrollJankV4Frame(const ScrollJankV4Frame& frame) = default;

ScrollJankV4Frame::~ScrollJankV4Frame() = default;

// static
ScrollJankV4Frame::Timeline ScrollJankV4Frame::CalculateTimeline(
    const EventMetrics::List& events_metrics,
    const viz::BeginFrameArgs& presented_args,
    base::TimeTicks presentation_ts) {
  ScrollJankV4Frame::Timeline result;

  // We start by figuring out the range of `viz::BeginFrameId`s in
  // `events_metrics`.
  std::optional<FrameBounds> frame_bounds = GetFrameBounds(events_metrics);

  // We special-case the handling of several common scenarios and edge cases.
  // This allows us to avoid constructing a `std::map` of `std::vector`s on the
  // heap unless we absolutely have to.

  // Case 1: `events_metrics` doesn't contain any scroll updates/ends, so the
  // result is simply empty.
  if (!frame_bounds.has_value()) {
    return result;
  }

  // Case 2: All scroll updates/ends are non-damaging and associated with the
  // same frame.
  if (!frame_bounds->min_damaging_id.has_value() &&
      frame_bounds->min_id == frame_bounds->max_id) {
    result.emplace_back(
        frame_bounds->some_args, NonDamagingFrame{},
        ScrollJankV4FrameStage::CalculateStages(
            events_metrics, /* skip_non_damaging_events= */ false));
    return result;
  }

  // Case 3: All frames are damaging, so all scroll updates/ends should be
  // associated with the presented damaging frame.
  if (frame_bounds->min_damaging_id.has_value() &&
      *frame_bounds->min_damaging_id == frame_bounds->min_id) {
    result.emplace_back(
        BeginFrameArgsForScrollJank::From(presented_args),
        DamagingFrame{.presentation_ts = presentation_ts},
        ScrollJankV4FrameStage::CalculateStages(
            events_metrics, /* skip_non_damaging_events= */ false));
    return result;
  }

  // None of the special cases above apply, so we take the generic approach and
  // re-construct the mapping from frames to scroll events.
  struct ArgsAndEvents {
    BeginFrameArgsForScrollJank args;
    bool is_damaging;
    std::vector<ScrollEventMetrics*> events;
  };
  std::map<viz::BeginFrameId, ArgsAndEvents> frame_id_to_args_and_events;
  for (const auto& event : events_metrics) {
    ScrollEventMetrics* scroll_event = AsScrollUpdateOrEnd(*event);
    if (!scroll_event) {
      continue;
    }
    const ScrollEventMetrics::DispatchBeginFrameArgs& original_args =
        scroll_event->dispatch_args();
    bool is_damaging = frame_bounds->IsDamaging(original_args.frame_id);
    // If the frame is damaging, the scroll event should be associated with the
    // presented frame (because that's the first frame where the user could see
    // the damage). If the frame is non-damaging, it should instead be
    // associated with its original frame (because the user isn't able to tell
    // when/whether it was presented).
    const viz::BeginFrameId& effective_frame_id =
        is_damaging ? presented_args.frame_id : original_args.frame_id;
    // Find the position where `effective_frame_id` is or should be in the map.
    auto it = frame_id_to_args_and_events.lower_bound(effective_frame_id);
    if (it != frame_id_to_args_and_events.end() &&
        it->first == effective_frame_id) {
      // If `effective_frame_id` already has an entry in the map, just append to
      // the vector of events.
      it->second.events.push_back(scroll_event);
    } else {
      // If `effective_frame_id` doesn't have an entry in the map yet, create a
      // new entry with a reference to `effective_args` and a singleton vector.
      frame_id_to_args_and_events.emplace_hint(
          it, effective_frame_id,
          ArgsAndEvents{
              .args = is_damaging
                          ? BeginFrameArgsForScrollJank::From(presented_args)
                          : BeginFrameArgsForScrollJank::From(original_args),
              .is_damaging = is_damaging,
              .events = {scroll_event},
          });
    }
  }
  for (const auto& [frame_id, args_and_events] : frame_id_to_args_and_events) {
    ScrollDamage damage =
        args_and_events.is_damaging
            ? ScrollDamage{DamagingFrame{.presentation_ts = presentation_ts}}
            : ScrollDamage{NonDamagingFrame{}};
    result.emplace_back(
        args_and_events.args, damage,
        ScrollJankV4FrameStage::CalculateStages(
            args_and_events.events, /* skip_non_damaging_events= */ false));
  }
  return result;
}

}  // namespace cc
