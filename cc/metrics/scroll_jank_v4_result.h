// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_SCROLL_JANK_V4_RESULT_H_
#define CC_METRICS_SCROLL_JANK_V4_RESULT_H_

#include <array>
#include <optional>
#include <variant>

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
  static constexpr int kMaxVsyncsSincePreviousFrame = 1000000;
  static constexpr int kMaxMissedVsyncs = kMaxVsyncsSincePreviousFrame - 1;

  // Number of VSyncs that that Chrome missed before presenting the scroll
  // update for each reason. If at least one value is greater than zero, this
  // frame was delayed and thus the scroll update is considered janky. Each
  // value is guaranteed to be at most `kMaxMissedVsyncs`.
  JankReasonArray<int> missed_vsyncs_per_reason = {};

  // How many VSyncs were between (A) this frame and (B) the previous frame.
  // If this value is greater than one, then Chrome potentially missed one or
  // more VSyncs (i.e. might have been able to present this scroll update
  // earlier). Guaranteed to be at most `kMaxVsyncsSincePreviousFrame`. Empty if
  // this frame is the first frame in a scroll.
  std::optional<int> vsyncs_since_previous_frame = std::nullopt;

  // The running delivery cut-off based on frames preceding this frame. See
  // `ScrollJankDroppedFrameTracker::running_delivery_cutoff_` for more
  // information. Empty if ANY of the following holds:
  //
  //   * This frame is the first frame in a scroll.
  //   * All frames since the beginning of the scroll up to and including the
  //     previous frame have been non-damaging or synthetic.
  //   * The most recent janky frame was non-damaging or synthetic and all
  //     frames since then up to and including the previous frame have been
  //     non-damaging or synthetic.
  std::optional<base::TimeDelta> running_delivery_cutoff = std::nullopt;

  // The running delivery cut-off adjusted for this frame. See
  // `ScrollJankDroppedFrameTracker::CalculateMissedVsyncsPerReasonV4()` for
  // more information. Empty if ANY of the following holds:
  //
  //   * This frame is the first frame in a scroll.
  //   * This frame is non-damaging or synthetic.
  //   * All frames since the beginning of the scroll up to and including the
  //     previous frame have been non-damaging or synthetic.
  //   * The most recent janky frame was non-damaging or synthetic and all
  //     frames since then up to and including the previous frame have been
  //     non-damaging or synthetic.
  //   * `vsyncs_since_previous_frame` is equal to one.
  std::optional<base::TimeDelta> adjusted_delivery_cutoff = std::nullopt;

  // The delivery cut-off of this frame. See
  // `ScrollJankDroppedFrameTracker::ReportLatestPresentationDataV4()` for
  // more information. Empty if this frame is non-damaging or synthetic.
  std::optional<base::TimeDelta> current_delivery_cutoff = std::nullopt;

  // The input generation timestamp of the first scroll update in the frame.
  //
  //   * If this frame contains ONLY REAL scroll updates, it's the actual input
  //     generation timestamp of the earliest scroll update.
  //   * If this frame contains ONLY SYNTHETIC scroll updates, it's an
  //     extrapolated input generation timestamp based on the input generation
  //     → begin frame duration of the most recent real scroll update.
  //   * If this frame contains BOTH real and synthetic scroll updates, it's
  //     the earlier input generation timestamp of the two.
  //
  // The extrapolated timestamp for a frame which contains only synthetic scroll
  // updates is empty if ANY of the following holds:
  //
  //   * This frame is janky and synthetic.
  //   * All frames since the beginning of the scroll up to and including this
  //     frame have been synthetic.
  //   * The most recent janky frame was synthetic and all frames since then up
  //     to and including the this frame have been synthetic.
  struct RealFirstScrollUpdate {
    base::TimeTicks actual_input_generation_ts;
  };
  struct SyntheticFirstScrollUpdate {
    std::optional<base::TimeTicks> extrapolated_input_generation_ts;
  };
  using FirstScrollUpdate =
      std::variant<RealFirstScrollUpdate, SyntheticFirstScrollUpdate>;
  FirstScrollUpdate first_scroll_update =
      SyntheticFirstScrollUpdate(std::nullopt);

  // The presentation timestamp of the frame.
  //
  //   * If this frame is DAMAGING, it's the actual presentation timestamp.
  //   * If this frame is NON-DAMAGING, it's an extrapolated timestamp based on
  //     the begin frame → presentation duration of the most recent damaging
  //     frame.
  //
  // The extrapolated timestamp for a non-damaging frame is empty if ANY of the
  // following holds:
  //
  //   * This frame is janky and non-damaging.
  //   * All frames since the beginning of the scroll up to and including this
  //     frame have been non-damaging.
  //   * The most recent janky frame was non-damaging and all frames since then
  //     up to and including the this frame have been non-damaging.
  struct DamagingPresentation {
    base::TimeTicks actual_presentation_ts;
  };
  struct NonDamagingPresentation {
    std::optional<base::TimeTicks> extrapolated_presentation_ts;
  };
  using Presentation =
      std::variant<DamagingPresentation, NonDamagingPresentation>;
  Presentation presentation = NonDamagingPresentation(std::nullopt);
};

}  // namespace cc

#endif  // CC_METRICS_SCROLL_JANK_V4_RESULT_H_
