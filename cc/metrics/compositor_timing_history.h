// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_COMPOSITOR_TIMING_HISTORY_H_
#define CC_METRICS_COMPOSITOR_TIMING_HISTORY_H_

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "cc/base/rolling_time_delta_history.h"
#include "cc/cc_export.h"
#include "cc/metrics/event_metrics.h"
#include "cc/tiles/tile_priority.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"

namespace perfetto {
namespace protos {
namespace pbzero {
class CompositorTimingHistory;
}
}  // namespace protos
}  // namespace perfetto

namespace cc {
class RenderingStatsInstrumentation;

class CC_EXPORT CompositorTimingHistory {
 public:
  enum UMACategory {
    RENDERER_UMA,
    BROWSER_UMA,
    NULL_UMA,
  };
  class UMAReporter;

  CompositorTimingHistory(
      UMACategory uma_category,
      RenderingStatsInstrumentation* rendering_stats_instrumentation);
  CompositorTimingHistory(const CompositorTimingHistory&) = delete;
  virtual ~CompositorTimingHistory();

  CompositorTimingHistory& operator=(const CompositorTimingHistory&) = delete;

  // The main thread responsiveness depends heavily on whether or not the
  // on_critical_path flag is set, so we record response times separately.
  virtual base::TimeDelta BeginMainFrameQueueDurationCriticalEstimate() const;
  virtual base::TimeDelta BeginMainFrameQueueDurationNotCriticalEstimate()
      const;
  virtual base::TimeDelta BeginMainFrameStartToReadyToCommitDurationEstimate()
      const;
  virtual base::TimeDelta CommitDurationEstimate() const;
  virtual base::TimeDelta CommitToReadyToActivateDurationEstimate() const;
  virtual base::TimeDelta ActivateDurationEstimate() const;
  virtual base::TimeDelta DrawDurationEstimate() const;

  base::TimeDelta BeginMainFrameStartToReadyToCommitCriticalEstimate() const;
  base::TimeDelta BeginMainFrameStartToReadyToCommitNotCriticalEstimate() const;
  base::TimeDelta BeginMainFrameQueueToActivateCriticalEstimate() const;

  // State that affects when events should be expected/recorded/reported.
  void SetRecordingEnabled(bool enabled);

  // Events to be timed.
  void WillFinishImplFrame(bool needs_redraw);
  void BeginImplFrameNotExpectedSoon();
  void WillBeginMainFrame(const viz::BeginFrameArgs& args);
  void BeginMainFrameStarted(base::TimeTicks begin_main_frame_start_time_);
  void BeginMainFrameAborted();
  void NotifyReadyToCommit();
  void WillCommit();
  void DidCommit();
  void ReadyToActivate();
  void WillActivate();
  void DidActivate();
  void WillDraw();
  void DidDraw();
  void WillInvalidateOnImplSide();

  base::TimeTicks begin_main_frame_sent_time() const {
    return begin_main_frame_sent_time_;
  }

  void ClearHistory();

  size_t CommitDurationSampleCountForTesting() const;

 protected:
  void DidBeginMainFrame(base::TimeTicks begin_main_frame_end_time);

  void SetCompositorDrawingContinuously(bool active);

  virtual base::TimeTicks Now() const;

  bool enabled_;

  // Used to calculate frame rates of Main and Impl threads.
  bool compositor_drawing_continuously_;
  base::TimeTicks draw_end_time_prev_;

  // If you add any history here, please remember to reset it in
  // ClearHistory.
  RollingTimeDeltaHistory begin_main_frame_queue_duration_history_;
  RollingTimeDeltaHistory begin_main_frame_queue_duration_critical_history_;
  RollingTimeDeltaHistory begin_main_frame_queue_duration_not_critical_history_;
  RollingTimeDeltaHistory
      begin_main_frame_start_to_ready_to_commit_duration_history_;
  RollingTimeDeltaHistory commit_duration_history_;
  RollingTimeDeltaHistory commit_to_ready_to_activate_duration_history_;
  RollingTimeDeltaHistory activate_duration_history_;
  RollingTimeDeltaHistory draw_duration_history_;

  // The time between when BMF was posted to the main thread task queue, and the
  // timestamp taken on the main thread when the BMF started running.
  base::TimeDelta begin_main_frame_queue_duration_;
  // The value of begin_main_frame_queue_duration_ that was measured for the
  // pending tree.
  base::TimeDelta pending_tree_bmf_queue_duration_;
  // The time between when BMF was posted to the main thread task queue, and
  // when the result of the BMF finished activation.
  base::TimeDelta bmf_start_to_ready_to_activate_duration_;

  bool begin_main_frame_on_critical_path_ = false;
  bool pending_commit_on_critical_path_ = false;
  bool pending_tree_on_critical_path_ = false;
  base::TimeTicks begin_main_frame_sent_time_;
  base::TimeTicks begin_main_frame_start_time_;
  base::TimeTicks ready_to_commit_time_;
  base::TimeTicks commit_start_time_;
  base::TimeTicks pending_tree_creation_time_;
  base::TimeTicks pending_tree_ready_to_activate_time_;
  base::TimeTicks activate_start_time_;
  base::TimeTicks draw_start_time_;

  bool pending_tree_is_impl_side_;

  const UMACategory uma_category_;

  // Owned by LayerTreeHost and is destroyed when LayerTreeHost is destroyed.
  raw_ptr<RenderingStatsInstrumentation, DanglingUntriaged>
      rendering_stats_instrumentation_;
};

}  // namespace cc

#endif  // CC_METRICS_COMPOSITOR_TIMING_HISTORY_H_
