// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_SCROLL_JANK_V4_DECISION_QUEUE_H_
#define CC_METRICS_SCROLL_JANK_V4_DECISION_QUEUE_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/time/time.h"
#include "cc/cc_export.h"
#include "cc/metrics/scroll_jank_v4_decider.h"
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
class CC_EXPORT ScrollJankV4DecisionQueue {
 public:
  class CC_EXPORT ResultConsumer {
   public:
    virtual ~ResultConsumer();
    virtual void OnFrameResult(
        const ScrollJankV4FrameStage::ScrollUpdates& updates,
        const ScrollJankV4Frame::ScrollDamage& damage,
        const ScrollJankV4Frame::BeginFrameArgsForScrollJank& args,
        const ScrollJankV4Result& result) = 0;
    virtual void OnScrollStarted() = 0;
    virtual void OnScrollEnded() = 0;
  };

  explicit ScrollJankV4DecisionQueue(
      std::unique_ptr<ResultConsumer> result_consumer);
  ~ScrollJankV4DecisionQueue();
  ScrollJankV4DecisionQueue(const ScrollJankV4DecisionQueue&) = delete;
  ScrollJankV4DecisionQueue(ScrollJankV4DecisionQueue&&) = delete;

  // Processes a frame which contains scroll updates to decide whether it was
  // janky.
  //
  // If the frame is malformed in any way (e.g. it has an earlier presentation
  // time than the previous frame provided to the decider), this method false.
  // returns without invoking the queue's `ResultConsumer`. Otherwise, this
  // method returns true and provides the jank result to the queue's
  // `ResultConsumer`. Depending on whether there is enough information to make
  // a decision, the method will provide the jank result to the
  // `ResultConsumer`:
  //
  //   * either immediately before returning,
  //   * or at a later point when it receives the necessary information (e.g.
  //     when this method is called later with information about a subsequent
  //     frame or when the scroll ends).
  //
  // Either way, this method guarantees that the queue's `ResultConsumer` will
  // be invoked with jank results in the same order as the frames were provided
  // to this method. For example, if the caller does the following (with valid
  // arguments):
  //
  // ```
  // queue.ProcessFrameWithScrollUpdates(updates1, damage1, args1);
  // queue.ProcessFrameWithScrollUpdates(updates2, damage2, args2);
  // ```
  //
  // this method guarantees to invoke the `ResultConsumer` with the jank results
  // for (`updates1`, `damage1`, `args1`) before invoking it with the jank
  // results for (`updates2`, `damage2`, `args2`).
  bool ProcessFrameWithScrollUpdates(
      const ScrollJankV4FrameStage::ScrollUpdates& updates,
      const ScrollJankV4Frame::ScrollDamage& damage,
      const ScrollJankV4Frame::BeginFrameArgsForScrollJank& args);

  void OnScrollStarted();
  void OnScrollEnded();

 private:
  bool AcceptFrameIfValidAndChronological(
      const ScrollJankV4FrameStage::ScrollUpdates& updates,
      const ScrollJankV4Frame::ScrollDamage& damage,
      const ScrollJankV4Frame::BeginFrameArgsForScrollJank& args);

  void FlushDeferredSyntheticFrames(
      bool future_real_frame_is_fast_scroll_or_sufficiently_fast_fling);

  ScrollJankV4Decider decider_;

  std::unique_ptr<ResultConsumer> result_consumer_;

  // Begin frame and presentation timestamps of the most recent valid frame
  // provided to `ProcessFrameWithScrollUpdates()`. The timestamps increase
  // monotonically with each new valid frame.
  base::TimeTicks last_provided_valid_begin_frame_ts_ = base::TimeTicks::Min();
  base::TimeTicks last_provided_valid_presentation_ts_ = base::TimeTicks::Min();

  // Synthetic frames provided to `ProcessFrameWithScrollUpdates()` which the
  // decider hasn't yet decided whether they're janky or not. They're waiting to
  // hear if they're in the middle of a fast scroll.
  //
  // The vector is sorted in chronological order (i.e. new synthetic frames are
  // appended to the end of the vector). If this vector is non-empty, then
  // `last_provided_valid_begin_frame_ts_` is equal to the begin frame timestamp
  // of the last frame in this vector
  // (`deferred_synthetic_frames_.rbegin()->args.frame_time`). Furthermore, if
  // this vector contains any damaging frames,
  // `last_provided_valid_presentation_ts_` is equal to the presentation time of
  // the last damaging frame in this vector.
  struct DeferredSyntheticFrame {
    ScrollJankV4FrameStage::ScrollUpdates::Synthetic synthetic_updates;
    ScrollJankV4Frame::ScrollDamage damage;
    ScrollJankV4Frame::BeginFrameArgsForScrollJank args;
  };
  std::vector<DeferredSyntheticFrame> deferred_synthetic_frames_;
};

}  // namespace cc

#endif  // CC_METRICS_SCROLL_JANK_V4_DECISION_QUEUE_H_
