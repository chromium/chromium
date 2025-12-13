// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_SCROLL_JANK_V4_RESULT_H_
#define CC_METRICS_SCROLL_JANK_V4_RESULT_H_

#include <array>
#include <optional>

#include "base/time/time.h"

namespace cc {

// Reason why Chrome's scroll jank v4 metric marked a scroll update as janky. A
// single scroll update can be janky for more than one reason. See
// https://docs.google.com/document/d/1AaBvTIf8i-c-WTKkjaL4vyhQMkSdynxo3XEiwpofdeA
// for more details.
// LINT.IfChange(JankReason)
enum class JankReason {
  // Chrome's input→frame delivery slowed down to the point that it missed one
  // or more VSyncs.
  kMissedVsyncDueToDeceleratingInputFrameDelivery,
  kMinValue = kMissedVsyncDueToDeceleratingInputFrameDelivery,

  // Chrome missed one or more VSyncs in the middle of a fast regular scroll.
  kMissedVsyncDuringFastScroll,

  // Chrome missed one or more VSyncs during the transition from a fast regular
  // scroll to a fling.
  kMissedVsyncAtStartOfFling,

  // Chrome missed one or more VSyncs in the middle of a fling.
  kMissedVsyncDuringFling,
  kMaxValue = kMissedVsyncDuringFling,
};
// LINT.ThenChange(//base/tracing/protos/chrome_track_event.proto:JankReason,//tools/metrics/histograms/metadata/event/histograms.xml:ScrollJankReasonV4)

template <typename T>
using JankReasonArray =
    std::array<T, static_cast<size_t>(JankReason::kMaxValue) + 1>;

// Result of the Scroll Jank V4 Metric for a scroll update. See
// https://docs.google.com/document/d/1AaBvTIf8i-c-WTKkjaL4vyhQMkSdynxo3XEiwpofdeA
// and the Event.ScrollJank.DelayedFramesPercentage4.FixedWindow histogram's
// documentation for more information.
//
// To simplify the documentation below, we use "this frame" to refer to the
// frame in which the scroll update was presented.
struct ScrollJankV4Result {
  // Number of VSyncs that that Chrome missed before presenting the scroll
  // update for each reason. If at least one value is greater than zero, this
  // frame was delayed and thus the scroll update is considered janky.
  JankReasonArray<int> missed_vsyncs_per_reason;

  // Whether this frame is damaging. This frame is non-damaging if the
  // following conditions are BOTH true:
  //
  //   1. All scroll updates in this frame are non-damaging. A scroll update
  //      is non-damaging if it didn't cause a frame update (i.e.
  //      `EventMetrics::caused_frame_update()` is false) and/or didn't change
  //      the scroll offset (i.e. `ScrollEventMetrics::did_scroll()` is
  //      false).
  //
  //   2. All frames between (both ends exclusive):
  //        a. the last frame presented by Chrome before this frame and
  //        b. this frame
  //      are non-damaging.
  bool is_damaging_frame;

  // The absolute total raw (unpredicted) delta of all scroll updates
  // included in this frame (in pixels).
  float abs_total_raw_delta_pixels;

  // The maximum absolute raw (unpredicted) delta out of all inertial (fling)
  // scroll updates included in this frame (in pixels). Zero if there were no
  // inertial scroll updates in this frame.
  float max_abs_inertial_raw_delta_pixels;

  // How many VSyncs were between (A) this frame and (B) the previous frame.
  // If this value is greater than one, then Chrome potentially missed one or
  // more VSyncs (i.e. might have been able to present this scroll update
  // earlier). Empty if this frame is the first frame in a scroll.
  std::optional<int> vsyncs_since_previous_frame;

  // The running delivery cut-off based on frames preceding this frame. See
  // `ScrollJankDroppedFrameTracker::running_delivery_cutoff_` for more
  // information. Empty if ANY of the following holds:
  //
  //   * This frame is the first frame in a scroll.
  //   * All frames since the beginning of the scroll up to and including the
  //     previous frame have been non-damaging.
  //   * The most recent janky frame was non-damaging and all frames since
  //     then up to and including the previous frame have been non-damaging.
  std::optional<base::TimeDelta> running_delivery_cutoff;

  // The running delivery cut-off adjusted for this frame. See
  // `ScrollJankDroppedFrameTracker::CalculateMissedVsyncsPerReasonV4()` for
  // more information. Empty if ANY of the following holds:
  //
  //   * This frame is the first frame in a scroll.
  //   * This frame is non-damaging.
  //   * All frames since the beginning of the scroll up to and including the
  //     previous frame have been non-damaging.
  //   * The most recent janky frame was non-damaging and all frames since
  //     then up to and including the previous frame have been non-damaging.
  //   * `vsyncs_since_previous_frame` is equal to one.
  std::optional<base::TimeDelta> adjusted_delivery_cutoff;

  // The delivery cut-off of this frame. See
  // `ScrollJankDroppedFrameTracker::ReportLatestPresentationDataV4()` for
  // more information. Empty if this frame is non-damaging.
  std::optional<base::TimeDelta> current_delivery_cutoff;
};

}  // namespace cc

#endif  // CC_METRICS_SCROLL_JANK_V4_RESULT_H_
