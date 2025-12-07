// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_SCROLL_JANK_V4_DECIDER_H_
#define CC_METRICS_SCROLL_JANK_V4_DECIDER_H_

#include <optional>
#include <utility>

#include "base/time/time.h"
#include "cc/cc_export.h"
#include "cc/metrics/scroll_jank_v4_frame.h"
#include "cc/metrics/scroll_jank_v4_frame_stage.h"
#include "cc/metrics/scroll_jank_v4_result.h"

namespace cc {

// Class responsible for deciding whether a frame containing one or more scroll
// updates was janky or not according to the scroll jank v4 metric. In order to
// work correctly, it must be informed about each frame that contained one or
// more scroll updates in chronological order.
//
// Scroll updates (and subsequently frames) can be categorized according to the
// following criteria:
//
//   1. Whether the scroll update cause a frame update and changed the scroll
//      offset: damaging vs. non-damaging scroll updates. See
//      `ScrollJankV4Result::is_damaging_frame` for
//      the definition of non-damaging scroll updates and frames.
//   2. Whether the scroll update originated from hardware/OS: real vs.
//      synthetic scroll updates. See `ScrollJankV4FrameStage::ScrollUpdates`
//      for the definition of synthetic scroll updates and frames.
//
// To avoid false positives, the decider must be informed about all four types
// of scroll updates and frames that occur within a scroll (damaging real,
// non-damaging real, damaging synthetic, non-damaging synthetic).
//
// See
// https://docs.google.com/document/d/1AaBvTIf8i-c-WTKkjaL4vyhQMkSdynxo3XEiwpofdeA
// for more details about the scroll jank v4 metric.
class CC_EXPORT ScrollJankV4Decider {
 public:
  // Decides whether a real frame, which contains at least one real scroll
  // update, was janky based on the following information:
  //
  //   * `updates` the real and/or synthetic scroll updates included (coalesced)
  //     in the frame.
  //   * `damage`: Information about a frame's scroll damage. For damaging
  //     frames, `DamagingFrame::presentation_ts` specifies when the frame was
  //     presented to the user.
  //   * `args`: The presented frame's arguments (especially `args.interval`).
  //
  // This method treats non-damaging frames as if Chrome successfully presented
  // them on time, even if Chrome ended up not presenting the frames or they
  // were dropped/throttled/delayed. Rationale: If a frame is non-damaging, the
  // user can't tell whether Chrome presented the frame on time (or even whether
  // Chrome presented the frame at all).
  //
  // This method requires that the frame is valid (see `IsValidFrame()`).
  // Furthermore, calls to this method must be ordered chronologically with
  // respect to the begin frame timestamp, i.e. given the following calls:
  //
  // ```
  // decider.DecideJankForFrameWithRealScrollUpdates(updates1, damage1, args1,
  //                                                 treat_as_fast_scroll1);
  // decider.DecideJankForFrameWithRealScrollUpdates(updates2, damage2, args2,
  //                                                 treat_as_fast_scroll2);
  // ```
  //
  // `args2.frame_time` must be strictly greater than `args1.frame_time`.
  ScrollJankV4Result DecideJankForFrameWithRealScrollUpdates(
      const ScrollJankV4FrameStage::ScrollUpdates& updates,
      const ScrollJankV4Frame::ScrollDamage& damage,
      const ScrollJankV4Frame::BeginFrameArgsForScrollJank& args);

  // Decides whether a synthetic frame, which contains only synthetic scroll
  // updates, was janky based on the following information:
  //
  // This method behaves similarly to
  // `DecideJankForFrameWithRealScrollUpdates()` with one additional argument:
  //
  //   * `future_real_frame_is_fast_scroll_or_sufficiently_fast_fling`: Whether
  //     the earliest future real frame is a fast scroll or a sufficiently fast
  //     scroll (i.e.
  //     `future_real_frame_is_fast_scroll_or_sufficiently_fast_fling ==
  //     (IsFastScroll(future_real) || IsSufficientlyFastFling(future_real))`).
  //     False if there are no real frames between this synthetic frame and the
  //     end of the current scroll.
  //
  // Note: The method signature (specifically the
  // `future_real_frame_is_fast_scroll_or_sufficiently_fast_fling` argument)
  // might seem awkward because, in order to decide whether a synthetic frame is
  // janky, the decider needs information about the speed of a future real
  // frame. `ScrollJankV4DecisionQueue` takes care of this "look-ahead"
  // dependency and presents a simpler callback-based API.
  ScrollJankV4Result DecideJankForFrameWithSyntheticScrollUpdatesOnly(
      const ScrollJankV4FrameStage::ScrollUpdates& updates,
      const ScrollJankV4Frame::ScrollDamage& damage,
      const ScrollJankV4Frame::BeginFrameArgsForScrollJank& args,
      bool future_real_frame_is_fast_scroll_or_sufficiently_fast_fling);

  void OnScrollStarted();
  void OnScrollEnded();

  static bool IsValidFrame(
      const ScrollJankV4FrameStage::ScrollUpdates& updates,
      const ScrollJankV4Frame::ScrollDamage& damage,
      const ScrollJankV4Frame::BeginFrameArgsForScrollJank& args);
  static bool IsFastScroll(
      const ScrollJankV4FrameStage::ScrollUpdates::Real& real_updates);
  static bool IsSufficientlyFastFling(
      const ScrollJankV4FrameStage::ScrollUpdates::Real& real_updates);

 private:
  // Information about the previous frame, for which the decider has most
  // recently decided whether it's janky or not.
  struct PreviousFrameData {
    // Whether the previous frame contained an inertial input (i.e. was it a
    // fling).
    bool has_inertial_input;

    // Whether the most recent real frame (possibly the previous frame) was a
    // fast scroll.
    //
    // True if the absolute total raw (unpredicted) delta of all
    // real inputs in the most recent real frame was at least
    // `features::kScrollJankV4MetricFastScrollContinuityThreshold`. See
    // `IsFastScroll()`.
    //
    // False if there have been no real frames since the start of the scroll
    // (which is very unlikely).
    bool is_most_recent_real_frame_fast_scroll;

    // When the last real input included (coalesced) in the previous frame was
    // generated by the hardware.
    //
    // If the frame was synthetic (i.e. contained only synthetic inputs):
    //
    //   1. If there have been no real frames since the beginning of the
    //      scroll, this value is empty.
    //   2. If the frame that the decider most recently marked as janky was
    //      synthetic and there have been no real frames since then, this
    //      value is empty.
    //   3. Otherwise, this value is extrapolated from the most recently
    //      presented real frame (i.e. we assume a constant duration between
    //      `last_input_generation_ts` and `begin_frame_ts`):
    //
    //        ```
    //        synthetic_frame.last_input_generation_ts
    //          = real_frame.last_input_generation_ts
    //          + (synthetic_input.begin_frame_ts - real_frame.begin_frame_ts)
    //        ```
    //
    //      where `synthetic_input.begin_frame_ts` refers to the frame for
    //      which the synthetic input was originally predicted and
    //      `synthetic_frame` refers to the frame where it was actually
    //      presented (the same frame if it was presented on time or if it was
    //      a non-damaging frame).
    std::optional<base::TimeTicks> last_input_generation_ts;

    // The time at which the frame started. See
    // `viz::BeginFrameArgs::frame_time`.
    //
    // The following inequality should hold:
    //
    //   ```
    //   *last_input_generation_ts <= begin_frame_ts <= *presentation_ts
    //   ```
    base::TimeTicks begin_frame_ts;

    // When the frame was presented to the user.
    //
    // If the frame was non-damaging:
    //
    //   1. If there have been no damaging frames since the beginning of the
    //      scroll, this value is empty.
    //   2. If the frame that the decider most recently marked as janky was
    //      non-damaging and there have been no damaging frames since then,
    //      this value is empty.
    //   3. Otherwise, this value is extrapolated from the most recently
    //      presented damaging frame (i.e. we assume a constant duration
    //      between `begin_frame_ts` and `presentation_ts`):
    //
    //        ```
    //        non_damaging_frame.presentation_ts
    //          = presented_damaging_frame.presentation_ts
    //          + (non_damaging_frame.begin_frame_ts
    //              - presented_damaging_frame.begin_frame_ts)
    //        ```
    std::optional<base::TimeTicks> presentation_ts;

    // The running delivery cut-off. At a high-level, this value represents
    // how quickly Chrome was previously able to present inputs (weighted
    // towards recent frames). If Chrome misses a VSync, the decider will
    // judge the subsequent frame (i.e. determine whether the frame should
    // be marked as janky) against this value.
    //
    // If there have been no real damaging frames since the beginning scroll
    // or the frame that the decider most recently marked as janky was
    // synthetic or non-damaging and there have been no real damaging frames
    // since then, this value is empty. Otherwise, this value equals:
    //
    // ```
    // min_{i from 1 to N} (
    //   presentation_ts[i]
    //     - last_input_generation_ts[i]
    //     + (
    //         VsyncsBetween(i, N)
    //           * features::kScrollJankV4MetricDiscountFactor.Get()
    //           * vsync_interval
    //       )
    // )
    // ```
    //
    // where:
    //
    //   * `i = 1` corresponds a presented real damaging frame as follows:
    //       * If the frame that the decider most recently marked as janky was
    //         real and damaging, `i = 1` corresponds to that janky frame.
    //       * If the frame that the decider most recently marked as janky was
    //         synthetic or non-damaging, `i = 1` corresponds to the first
    //         real damaging frame that the decider processed after that janky
    //         frame.
    //       * If the decider hasn't marked any frame in this scroll as janky,
    //         `i = 1` corresponds to the first real damaging frame within the
    //         current scroll.
    //   * `i = N` corresponds to the frame (damaging or non-damaging) that
    //     the decider has most recently processed.
    //   * `presentation_ts[i]` and `last_input_generation_ts[i]` refer to:
    //       * If the i-th frame was a real damaging frame, they refer to the
    //         values supplied to the i-th
    //         `DecideJankForPresentedDamagingFrame()` call.
    //       * Otherwise, assume `presentation_ts[i] ==
    //       base::TimeTicks::Max()` and `last_input_generation_ts[i] ==
    //       base::TimeTicks::Min()` (i.e. ignore the i-th frame if it was
    //       synthetic or non-damaging).
    //   * `VsyncsBetween(i, N)` is approximately:
    //
    //     ```
    //     (presentation_ts[N] - presentation_ts[i] + (vsync_interval / 2))
    //       / vsync_interval
    //     ```
    //
    //     Approximation for non-damaging frames:
    //
    //     ```
    //     (begin_frame_ts[N] - begin_frame_ts[i] + (vsync_interval / 2))
    //       / vsync_interval
    //     ```
    std::optional<base::TimeDelta> running_delivery_cutoff;
  };

  ScrollJankV4Result DecideJankForFrameWithScrollUpdates(
      const ScrollJankV4FrameStage::ScrollUpdates& updates,
      const ScrollJankV4Frame::ScrollDamage& damage,
      const ScrollJankV4Frame::BeginFrameArgsForScrollJank& args,
      bool treat_as_fast_scroll);

  JankReasonArray<int> CalculateMissedVsyncsPerReason(
      int vsyncs_since_previous_frame,
      std::optional<base::TimeTicks> earliest_input_generation_ts,
      const ScrollJankV4FrameStage::ScrollUpdates& updates,
      const ScrollJankV4Frame::ScrollDamage& damage,
      const ScrollJankV4Frame::BeginFrameArgsForScrollJank& args,
      bool treat_as_fast_scroll,
      ScrollJankV4Result& result) const;

  std::optional<base::TimeDelta> CalculateRunningDeliveryCutoff(
      int vsyncs_since_previous_frame,
      bool is_janky,
      const ScrollJankV4FrameStage::ScrollUpdates& updates,
      const ScrollJankV4Frame::ScrollDamage& damage,
      const ScrollJankV4Frame::BeginFrameArgsForScrollJank& args,
      ScrollJankV4Result& result) const;

  void Reset();

  // Returns the scroll update in `updates` with the earliest input generation
  // timestamp.
  //
  // For real scroll updates, this method uses their actual input generation
  // timestamp.
  //
  // For synthetic scroll updates, this method extrapolates their input
  // generation timestamp from their begin frame timestamp and the most recently
  // presented real frame (similarly to
  // `PreviousFrameData::last_input_generation_ts`).
  ScrollJankV4Result::FirstScrollUpdate GetFirstScrollUpdate(
      const ScrollJankV4FrameStage::ScrollUpdates& updates) const;

  // Information about the previous frame, for which the decider has most
  // recently decided whether it's janky or not.
  //
  // Empty if the decider hasn't processed any frames since the beginning of
  // the current scroll, in which case the decider will definitely NOT mark
  // the next frame as janky.
  std::optional<PreviousFrameData> prev_frame_data_ = std::nullopt;
};

}  // namespace cc

#endif  // CC_METRICS_SCROLL_JANK_V4_DECIDER_H_
