// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_COMPOSITOR_TIMING_HISTORY_H_
#define CC_METRICS_COMPOSITOR_TIMING_HISTORY_H_

#include <memory>
#include <vector>

#include "cc/base/rolling_time_delta_history.h"
#include "cc/cc_export.h"
#include "cc/metrics/event_metrics.h"
#include "cc/scheduler/scheduler.h"
#include "cc/tiles/tile_priority.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"

namespace perfetto {
namespace protos {
namespace pbzero {
class CompositorTimingHistory;
}
}  // namespace protos
}  // namespace perfetto

namespace viz {
struct FrameTimingDetails;
}

namespace cc {
struct BeginMainFrameMetrics;
class CompositorFrameReportingController;
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
      bool using_synchronous_renderer_compositor,
      UMACategory uma_category,
      RenderingStatsInstrumentation* rendering_stats_instrumentation,
      CompositorFrameReportingController*
          compositor_frame_reporting_controller);
  CompositorTimingHistory(const CompositorTimingHistory&) = delete;
  virtual ~CompositorTimingHistory();

  CompositorTimingHistory& operator=(const CompositorTimingHistory&) = delete;

  void AsProtozeroInto(
      perfetto::protos::pbzero::CompositorTimingHistory* state) const;

  // The main thread responsiveness depends heavily on whether or not the
  // on_critical_path flag is set, so we record response times separately.
  virtual base::TimeDelta BeginMainFrameQueueDurationCriticalEstimate() const;
  virtual base::TimeDelta BeginMainFrameQueueDurationNotCriticalEstimate()
      const;
  virtual base::TimeDelta BeginMainFrameStartToReadyToCommitDurationEstimate()
      const;
  virtual base::TimeDelta CommitDurationEstimate() const;
  virtual base::TimeDelta CommitToReadyToActivateDurationEstimate() const;
  virtual base::TimeDelta PrepareTilesDurationEstimate() const;
  virtual base::TimeDelta ActivateDurationEstimate() const;
  virtual base::TimeDelta DrawDurationEstimate() const;

  // State that affects when events should be expected/recorded/reported.
  void SetRecordingEnabled(bool enabled);
  void DidCreateAndInitializeLayerTreeFrameSink();

  // Events to be timed.
  void WillBeginImplFrame(const viz::BeginFrameArgs& args,
                          base::TimeTicks now);
  void WillFinishImplFrame(bool needs_redraw, const viz::BeginFrameId& id);
  void BeginImplFrameNotExpectedSoon();
  void WillBeginMainFrame(const viz::BeginFrameArgs& args);
  void BeginMainFrameStarted(base::TimeTicks begin_main_frame_start_time_);
  void BeginMainFrameAborted(const viz::BeginFrameId& id,
                             CommitEarlyOutReason reason);
  void NotifyReadyToCommit(std::unique_ptr<BeginMainFrameMetrics> details);
  void WillCommit();
  void DidCommit();
  void WillPrepareTiles();
  void DidPrepareTiles();
  void ReadyToActivate();
  void WillActivate();
  void DidActivate();
  void WillDraw();
  void DidDraw(bool used_new_active_tree,
               bool has_custom_property_animations);
  void DidSubmitCompositorFrame(
      uint32_t frame_token,
      const viz::BeginFrameId& current_frame_id,
      const viz::BeginFrameId& last_activated_frame_id,
      EventMetricsSet events_metrics);
  void DidNotProduceFrame(const viz::BeginFrameId& id,
                          FrameSkippedReason skip_reason);
  void DidReceiveCompositorFrameAck();
  void DidPresentCompositorFrame(uint32_t frame_token,
                                 const viz::FrameTimingDetails& details);
  void WillInvalidateOnImplSide();
  void SetTreePriority(TreePriority priority);

  base::TimeTicks begin_main_frame_sent_time() const {
    return begin_main_frame_sent_time_;
  }

  void ClearHistory();
  size_t begin_main_frame_start_to_ready_to_commit_sample_count() const {
    return begin_main_frame_start_to_ready_to_commit_duration_history_
        .sample_count();
  }
  size_t commit_to_ready_to_activate_sample_count() const {
    return commit_to_ready_to_activate_duration_history_.sample_count();
  }

 protected:
  void DidBeginMainFrame(base::TimeTicks begin_main_frame_end_time);

  void SetCompositorDrawingContinuously(bool active);

  static std::unique_ptr<UMAReporter> CreateUMAReporter(UMACategory category);
  virtual base::TimeTicks Now() const;

  bool using_synchronous_renderer_compositor_;
  bool enabled_;

  // Used to calculate frame rates of Main and Impl threads.
  bool did_send_begin_main_frame_;
  bool compositor_drawing_continuously_;
  base::TimeTicks new_active_tree_draw_end_time_prev_;
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
  RollingTimeDeltaHistory prepare_tiles_duration_history_;
  RollingTimeDeltaHistory activate_duration_history_;
  RollingTimeDeltaHistory draw_duration_history_;

  bool begin_main_frame_on_critical_path_;
  base::TimeTicks begin_main_frame_sent_time_;
  base::TimeTicks begin_main_frame_start_time_;
  base::TimeTicks commit_start_time_;
  base::TimeTicks pending_tree_creation_time_;
  base::TimeTicks pending_tree_ready_to_activate_time_;
  base::TimeTicks prepare_tiles_start_time_;
  base::TimeTicks activate_start_time_;
  base::TimeTicks draw_start_time_;
  base::TimeTicks submit_start_time_;

  bool pending_tree_is_impl_side_;

  std::unique_ptr<UMAReporter> uma_reporter_;

  // Owned by LayerTreeHost and is destroyed when LayerTreeHost is destroyed.
  RenderingStatsInstrumentation* rendering_stats_instrumentation_;

  // Owned by LayerTreeHostImpl and is destroyed when LayerTreeHostImpl is
  // destroyed.
  CompositorFrameReportingController* compositor_frame_reporting_controller_;

  // Used only for reporting animation targeted UMA.
  bool previous_frame_had_custom_property_animations_ = false;

  TreePriority tree_priority_ = SAME_PRIORITY_FOR_BOTH_TREES;
};

}  // namespace cc

#endif  // CC_METRICS_COMPOSITOR_TIMING_HISTORY_H_
