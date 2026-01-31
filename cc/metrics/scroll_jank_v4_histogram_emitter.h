// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_SCROLL_JANK_V4_HISTOGRAM_EMITTER_H_
#define CC_METRICS_SCROLL_JANK_V4_HISTOGRAM_EMITTER_H_

#include <variant>

#include "base/containers/enum_set.h"
#include "cc/cc_export.h"
#include "cc/metrics/scroll_jank_v4_result.h"
#include "third_party/abseil-cpp/absl/container/inlined_vector.h"

namespace cc {

// Class responsible for emitting UMA histograms for the scroll jank v4 metric.
// It emits the following histograms after each window of
// `kHistogramEmitFrequency` presented frames:
//
//   * Event.ScrollJank.DelayedFramesPercentage4.FixedWindow
//   * Event.ScrollJank.DelayedFramesPercentage4.FixedWindow.
//       MissedVsyncDueToDeceleratingInputFrameDelivery
//   * Event.ScrollJank.DelayedFramesPercentage4.FixedWindow.
//       MissedVsyncDuringFastScroll
//   * Event.ScrollJank.DelayedFramesPercentage4.FixedWindow.
//       MissedVsyncAtStartOfFling
//   * Event.ScrollJank.DelayedFramesPercentage4.FixedWindow.
//       MissedVsyncDuringFling
//   * Event.ScrollJank.MissedVsyncsSum4.FixedWindow
//   * Event.ScrollJank.MissedVsyncsMax4.FixedWindow
//
// and the following histograms after each scroll:
//
//   * Event.ScrollJank.DelayedFramesPercentage4.PerScroll
//
// The histogram emitter's behavior with respect to non-damaging frames and
// scrolls is controlled via `features::kHistogramEmissionPolicy`.
class CC_EXPORT ScrollJankV4HistogramEmitter {
 public:
  ScrollJankV4HistogramEmitter();
  ~ScrollJankV4HistogramEmitter();

  void OnFrameWithScrollUpdates(
      const JankReasonArray<int>& missed_vsyncs_per_reason,
      bool is_damaging);
  void OnScrollStarted();
  void OnScrollEnded();

  static constexpr int kHistogramEmitFrequency = 64;
  static constexpr const char* kDelayedFramesWindowHistogram =
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow";
  static constexpr const char*
      kMissedVsyncDueToDeceleratingInputFrameDeliveryHistogram =
          "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
          "MissedVsyncDueToDeceleratingInputFrameDelivery";
  static constexpr const char* kMissedVsyncDuringFastScrollHistogram =
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
      "MissedVsyncDuringFastScroll";
  static constexpr const char* kMissedVsyncAtStartOfFlingHistogram =
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
      "MissedVsyncAtStartOfFling";
  static constexpr const char* kMissedVsyncDuringFlingHistogram =
      "Event.ScrollJank.DelayedFramesPercentage4.FixedWindow."
      "MissedVsyncDuringFling";
  static constexpr const char* kDelayedFramesPerScrollHistogram =
      "Event.ScrollJank.DelayedFramesPercentage4.PerScroll";
  static constexpr const char* kMissedVsyncsSumInWindowHistogram =
      "Event.ScrollJank.MissedVsyncsSum4.FixedWindow";
  static constexpr const char* kMissedVsyncsMaxInWindowHistogram =
      "Event.ScrollJank.MissedVsyncsMax4.FixedWindow";

 private:
  // Jank data about a single frame that counts towards the scroll jank v4
  // metric histograms frame count.
  //
  // Might actually contain data from multiple non-damaging frames in case they
  // don't count towards the histogram frame count.
  struct SingleFrameData {
    // Reasons why the frame is janky. Might be empty.
    base::EnumSet<JankReason, JankReason::kMinValue, JankReason::kMaxValue>
        jank_reasons;

    // Total number of VSyncs that Chrome missed (for any reason). Whenever a
    // frame is missed, it could be delayed by >=1 vsyncs, this helps us track
    // how "long" the janks are.
    //
    // Must be zero if `jank_reasons` is empty. Must be positive if
    // `jank_reasons` is non-empty.
    int missed_vsyncs = 0;

    // Maximum number of consecutive VSyncs that Chrome missed (for any reason).
    //
    // Must be zero if `jank_reasons` is empty. Must be less than or equal to
    // `missed_vsyncs`.
    int max_consecutive_missed_vsyncs = 0;

    static SingleFrameData From(
        const JankReasonArray<int>& missed_vsyncs_per_reason);
    void MergeWith(const SingleFrameData& other);
    bool HasJankReasons() const;
  };

  struct JankDataFixedWindow {
    // Total number of frames that Chrome presented.
    int presented_frames = 0;

    // Total number of frames that Chrome didn't present on time, i.e. presented
    // one or more VSyncs later than it should have (for any reason).
    // Must be less than or equal to `presented_frames`.
    int delayed_frames = 0;

    // Number of frames that Chrome didn't present on time for each reason.
    // Each value must be less than or equal to `delayed_frames`.
    JankReasonArray<int> delayed_frames_per_reason = {};

    // Total number of VSyncs that Chrome missed (for any reason). Whenever a
    // frame is missed, it could be delayed by >=1 vsyncs, this helps us track
    // how "long" the janks are.
    // Must be greater than or equal to `delayed_frames`.
    int missed_vsyncs = 0;

    // Maximum number of consecutive VSyncs that Chrome missed (for any reason).
    // Must be less than or equal to `missed_vsyncs`.
    int max_consecutive_missed_vsyncs = 0;

    void AddFrame(const SingleFrameData& frame_data);
    void MergeWith(const JankDataFixedWindow& other);
  };

  struct JankDataPerScroll {
    // Total number of frames that Chrome presented.
    int presented_frames = 0;

    // Total number of frames that Chrome didn't present on time, i.e. presented
    // one or more VSyncs later than it should have (for any reason).
    // Must be less than or equal to `presented_frames`.
    int delayed_frames = 0;

    void AddFrame(const SingleFrameData& frame_data);
    void MergeWith(const JankDataPerScroll& other);
  };

  class EmitForDamagingScrolls;

  // Emitter with the policy that all frames in ALL scrolls (regardless of
  // damage, even if the scroll is completely non-damaging) count towards the
  // UMA histograms.
  //
  // See `features::kEmitForAllScrolls`.
  class CC_EXPORT EmitForAllScrolls {
   public:
    void AddFrame(const SingleFrameData& frame_data, bool is_damaging);
    void FinishScroll();

   private:
    friend class ScrollJankV4HistogramEmitter::EmitForDamagingScrolls;

    void MaybeEmitPerWindowHistogramsAndResetCounters();
    void MaybeEmitPerScrollHistogramsAndResetCounters();

    JankDataFixedWindow fixed_window_;
    JankDataPerScroll per_scroll_;
  };

  // Emitter with the policy that all frames in DAMAGING scrolls (containing at
  // least one damaging frame) count towards the UMA histograms. Jank identified
  // in frames in a non-damaging scroll (containing only non-damaging frames)
  // won't be reported in the UMA histograms.
  //
  // See `features::kEmitForDamagingScrolls`.
  class CC_EXPORT EmitForDamagingScrolls {
   public:
    EmitForDamagingScrolls();
    EmitForDamagingScrolls(const EmitForDamagingScrolls&);
    ~EmitForDamagingScrolls();

    void AddFrame(const SingleFrameData& frame_data, bool is_damaging);
    void FinishScroll();

   private:
    // The emitter hasn't encountered any damaging frame since the beginning of
    // the current scroll. It might have pending data from one or more
    // non-damaging scrolls, which the emitter will flush if it encounters a
    // damaging frame later within the current scroll.
    struct NoDamagingFrameEncounteredYet {
      // Maximum size of `pending_fixed_windows`. If there are any more
      // non-damaging frames at the beginning of a scroll, the emitter will
      // ignore them.
      static constexpr int kPendingFixedWindowsMaxSize = 20;

      // The pending data for fixed window histograms in chronological order.
      // It's bucketed by `kHistogramEmitFrequency` so that, if the histogram
      // emitter encounters a damaging frame, it will merge
      // `pending_fixed_windows[0]` (if present) into
      // `ScrollJankV4HistogramEmitter::fixed_window_`. So
      // `pending_fixed_windows[0].presented_frames <= kHistogramEmitFrequency -
      // ScrollJankV4HistogramEmitter::fixed_window_.presented_frames` and
      // `pending_fixed_windows[i].presented_frames <= kHistogramEmitFrequency`
      // for all other indices `i > 0`.
      //
      // For example, if
      // `ScrollJankV4HistogramEmitter::fixed_window_.presented_frames` is 50
      // and there have been 100 non-damaging frames since the beginning of the
      // scroll (and zero damaging frames),this this field would contain the
      // following numbers of frames:
      //
      //   * `pending_fixed_windows[0].presented_frames = 14`
      //   * `pending_fixed_windows[1].presented_frames = 64`
      //   * `pending_fixed_windows[2].presented_frames = 22`
      //
      // Note that all items in `fixed_windows` except possibly the last one are
      // saturated in the sense that, if the histogram emitter encountered a
      // damaging frame next, it would immediately emit two fixed window UMA
      // histograms:
      //
      //   * one for `ScrollJankV4HistogramEmitter::fixed_window_` +
      //     `pending_fixed_windows[0]` and
      //   * one for `pending_fixed_windows[1]`.
      //
      // We set the estimated capacity to 2 so that, in the worst case scenario
      // (when `fixed_window_.presented_frames == kHistogramEmitFrequency - 1`),
      // the emitter could handle a scroll that starts with up to
      // `kHistogramEmitFrequency + 1` non-damaging frames without allocating
      // additional memory on the heap.
      absl::InlinedVector<JankDataFixedWindow, 2> pending_fixed_windows;

      // The pending data for per-scroll histograms. If the histogram emitter
      // encounters a damaging frame, it will merge `pending_per_scroll` into
      // `ScrollJankV4HistogramEmitter::per_scroll_`.
      JankDataPerScroll pending_per_scroll = {};

      NoDamagingFrameEncounteredYet();
      NoDamagingFrameEncounteredYet(const NoDamagingFrameEncounteredYet&);
      ~NoDamagingFrameEncounteredYet();
    };

    // The emitter has encountered at least one damaging frame since the
    // beginning of the current scroll.
    struct DamagingFrameAlreadyEncountered {};

    std::variant<NoDamagingFrameEncounteredYet, DamagingFrameAlreadyEncountered>
        state_ = NoDamagingFrameEncounteredYet();

    void StashPendingFrame(const SingleFrameData& frame_data);
    void FlushPendingFrames();

    EmitForAllScrolls wrapped_emitter_;
  };

  using InnerEmitter = std::variant<EmitForAllScrolls, EmitForDamagingScrolls>;

  static InnerEmitter CreateInnerEmitter();
  void FinishScroll();

  InnerEmitter inner_emitter_;

  friend class ScrollJankV4HistogramEmitterPolicySelectionTest;
};

}  // namespace cc

#endif  // CC_METRICS_SCROLL_JANK_V4_HISTOGRAM_EMITTER_H_
