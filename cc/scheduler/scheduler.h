// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SCHEDULER_SCHEDULER_H_
#define CC_SCHEDULER_SCHEDULER_H_

#include <memory>
#include <string>

#include "base/cancelable_callback.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "cc/cc_export.h"
#include "cc/scheduler/begin_frame_tracker.h"
#include "cc/scheduler/draw_result.h"
#include "cc/scheduler/scheduler_settings.h"
#include "cc/scheduler/scheduler_state_machine.h"
#include "cc/tiles/tile_priority.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/frame_sinks/delay_based_time_source.h"

namespace base {
namespace trace_event {
class ConvertableToTraceFormat;
}
class SingleThreadTaskRunner;
}

namespace cc {

class CompositorTimingHistory;

class SchedulerClient {
 public:
  // Returns whether the frame has damage.
  virtual bool WillBeginImplFrame(const viz::BeginFrameArgs& args) = 0;
  virtual void ScheduledActionSendBeginMainFrame(
      const viz::BeginFrameArgs& args) = 0;
  virtual DrawResult ScheduledActionDrawIfPossible() = 0;
  virtual DrawResult ScheduledActionDrawForced() = 0;
  virtual void ScheduledActionCommit() = 0;
  virtual void ScheduledActionActivateSyncTree() = 0;
  virtual void ScheduledActionBeginLayerTreeFrameSinkCreation() = 0;
  virtual void ScheduledActionPrepareTiles() = 0;
  virtual void ScheduledActionInvalidateLayerTreeFrameSink(
      bool needs_redraw) = 0;
  virtual void ScheduledActionPerformImplSideInvalidation() = 0;
  virtual void DidFinishImplFrame() = 0;
  virtual void DidNotProduceFrame(const viz::BeginFrameAck& ack) = 0;
  virtual void SendBeginMainFrameNotExpectedSoon() = 0;
  virtual void ScheduledActionBeginMainFrameNotExpectedUntil(
      base::TimeTicks time) = 0;

  // Functions used for reporting anmation targeting UMA, crbug.com/758439.
  virtual size_t CompositedAnimationsCount() const = 0;
  virtual size_t MainThreadAnimationsCount() const = 0;
  virtual bool CurrentFrameHadRAF() const = 0;
  virtual bool NextFrameHasPendingRAF() const = 0;

 protected:
  virtual ~SchedulerClient() {}
};

class CC_EXPORT Scheduler : public viz::BeginFrameObserverBase {
 public:
  Scheduler(SchedulerClient* client,
            const SchedulerSettings& scheduler_settings,
            int layer_tree_host_id,
            base::SingleThreadTaskRunner* task_runner,
            std::unique_ptr<CompositorTimingHistory> compositor_timing_history);
  ~Scheduler() override;

  // This is needed so that the scheduler doesn't perform spurious actions while
  // the compositor is being torn down.
  void Stop();

  // BeginFrameObserverBase
  void OnBeginFrameSourcePausedChanged(bool paused) override;
  bool OnBeginFrameDerivedImpl(const viz::BeginFrameArgs& args) override;

  void OnDrawForLayerTreeFrameSink(bool resourceless_software_draw,
                                   bool skip_draw);

  const SchedulerSettings& settings() const { return settings_; }

  void SetVisible(bool visible);
  bool visible() { return state_machine_.visible(); }
  void SetCanDraw(bool can_draw);
  void NotifyReadyToActivate();
  void NotifyReadyToDraw();
  void SetBeginFrameSource(viz::BeginFrameSource* source);

  void SetNeedsBeginMainFrame();
  // Requests a single impl frame (after the current frame if there is one
  // active).
  void SetNeedsOneBeginImplFrame();

  void SetNeedsRedraw();

  void SetNeedsPrepareTiles();

  // Requests a pending tree should be created to invalidate content on the impl
  // thread, after the current tree is activated, if any. If the request
  // necessitates creating a pending tree only for impl-side invalidations, the
  // |client_| is informed to perform this action using
  // ScheduledActionRunImplSideInvalidation.
  // If ScheduledActionCommit is performed, the impl-side invalidations should
  // be merged with the main frame and the request is assumed to be completed.
  // If |needs_first_draw_on_activation| is set to true, an impl-side pending
  // tree creates for this invalidation must be drawn at least once before a
  // new tree can be activated.
  void SetNeedsImplSideInvalidation(bool needs_first_draw_on_activation);

  // Drawing should result in submitting a CompositorFrame to the
  // LayerTreeFrameSink and then calling this.
  void DidSubmitCompositorFrame();
  // The LayerTreeFrameSink acks when it is ready for a new frame which
  // should result in this getting called to unblock the next draw.
  void DidReceiveCompositorFrameAck();

  void SetTreePrioritiesAndScrollState(TreePriority tree_priority,
                                       ScrollHandlerState scroll_handler_state);

  void NotifyReadyToCommit();
  void BeginMainFrameAborted(CommitEarlyOutReason reason);
  void DidCommit();

  void WillPrepareTiles();
  void DidPrepareTiles();
  void DidLoseLayerTreeFrameSink();
  void DidCreateAndInitializeLayerTreeFrameSink();

  // Tests do not want to shut down until all possible BeginMainFrames have
  // occured to prevent flakiness.
  bool MainFrameForTestingWillHappen() const {
    return state_machine_.CommitPending() ||
           state_machine_.CouldSendBeginMainFrame();
  }

  bool CommitPending() const { return state_machine_.CommitPending(); }
  bool RedrawPending() const { return state_machine_.RedrawPending(); }
  bool PrepareTilesPending() const {
    return state_machine_.PrepareTilesPending();
  }
  bool ImplLatencyTakesPriority() const {
    return state_machine_.ImplLatencyTakesPriority();
  }

  // Pass in a main_thread_start_time of base::TimeTicks() if it is not
  // known or not trustworthy (e.g. blink is running on a remote channel)
  // to signal that the start time isn't known and should not be used for
  // scheduling or statistics purposes.
  void NotifyBeginMainFrameStarted(base::TimeTicks main_thread_start_time);

  base::TimeTicks LastBeginImplFrameTime();

  void SetDeferCommits(bool defer_commits);

  // Controls whether the BeginMainFrameNotExpected messages should be sent to
  // the main thread by the cc scheduler.
  void SetMainThreadWantsBeginMainFrameNotExpected(bool new_state);

  std::unique_ptr<base::trace_event::ConvertableToTraceFormat> AsValue() const;

  void AsValueInto(base::trace_event::TracedValue* state) const;

  void SetVideoNeedsBeginFrames(bool video_needs_begin_frames);

  const viz::BeginFrameSource* begin_frame_source() const {
    return begin_frame_source_;
  }

  viz::BeginFrameAck CurrentBeginFrameAckForActiveTree() const;

  void ClearHistory();

 protected:
  // Virtual for testing.
  virtual base::TimeTicks Now() const;

  const SchedulerSettings settings_;
  SchedulerClient* const client_;
  const int layer_tree_host_id_;
  base::SingleThreadTaskRunner* task_runner_;

  viz::BeginFrameSource* begin_frame_source_ = nullptr;
  bool observing_begin_frame_source_ = false;

  bool skipped_last_frame_missed_exceeded_deadline_ = false;
  bool skipped_last_frame_to_reduce_latency_ = false;

  std::unique_ptr<CompositorTimingHistory> compositor_timing_history_;

  // What the latest deadline was, and when it was scheduled.
  base::TimeTicks deadline_;
  base::TimeTicks deadline_scheduled_at_;
  SchedulerStateMachine::BeginImplFrameDeadlineMode deadline_mode_;

  BeginFrameTracker begin_impl_frame_tracker_;
  viz::BeginFrameAck last_begin_frame_ack_;
  viz::BeginFrameArgs begin_main_frame_args_;

  // Task posted for the deadline or drawing phase of the scheduler. This task
  // can be rescheduled e.g. when the condition for the deadline is met, it is
  // scheduled to run immediately.
  // NOTE: Scheduler weak ptrs are not necessary if CancelableCallback is used.
  base::CancelableOnceClosure begin_impl_frame_deadline_task_;

  // This is used for queueing begin frames while scheduler is waiting for
  // previous frame's deadline, or if it's inside ProcessScheduledActions().
  // Only one such task is posted at any time, but the args are updated as we
  // get new begin frames.
  viz::BeginFrameArgs pending_begin_frame_args_;
  base::CancelableOnceClosure pending_begin_frame_task_;

  SchedulerStateMachine state_machine_;
  bool inside_process_scheduled_actions_ = false;
  bool inside_scheduled_action_ = false;
  SchedulerStateMachine::Action inside_action_ =
      SchedulerStateMachine::Action::NONE;

  bool stopped_ = false;

 private:
  // Posts the deadline task if needed by checking
  // SchedulerStateMachine::CurrentBeginImplFrameDeadlineMode(). This only
  // happens when the scheduler is processing a begin frame
  // (BeginImplFrameState::INSIDE_BEGIN_FRAME).
  void ScheduleBeginImplFrameDeadline();

  // Starts or stops begin frames as needed by checking
  // SchedulerStateMachine::BeginFrameNeeded(). This only happens when the
  // scheduler is not processing a begin frame (BeginImplFrameState::IDLE).
  void StartOrStopBeginFrames();

  // This will only post a task if the args are valid and there's no existing
  // task. That implies that we're still expecting begin frames. If begin frames
  // aren't needed this will be a nop. This only happens when the scheduler is
  // not processing a begin frame (BeginImplFrameState::IDLE).
  void PostPendingBeginFrameTask();

  // Use |pending_begin_frame_args_| to begin a new frame like it was received
  // in OnBeginFrameDerivedImpl().
  void HandlePendingBeginFrame();

  // Used to drop the pending begin frame before we go idle.
  void CancelPendingBeginFrameTask();

  void BeginImplFrameNotExpectedSoon();
  void BeginMainFrameNotExpectedUntil(base::TimeTicks time);
  void DrawIfPossible();
  void DrawForced();
  void ProcessScheduledActions();
  void UpdateCompositorTimingHistoryRecordingEnabled();
  bool ShouldDropBeginFrame(const viz::BeginFrameArgs& args) const;
  bool ShouldRecoverMainLatency(const viz::BeginFrameArgs& args,
                                bool can_activate_before_deadline) const;
  bool ShouldRecoverImplLatency(const viz::BeginFrameArgs& args,
                                bool can_activate_before_deadline) const;
  bool CanBeginMainFrameAndActivateBeforeDeadline(
      const viz::BeginFrameArgs& args,
      base::TimeDelta bmf_to_activate_estimate,
      base::TimeTicks now) const;
  void AdvanceCommitStateIfPossible();
  bool IsBeginMainFrameSentOrStarted() const;

  void BeginImplFrameWithDeadline(const viz::BeginFrameArgs& args);
  void BeginImplFrameSynchronous(const viz::BeginFrameArgs& args);
  void BeginImplFrame(const viz::BeginFrameArgs& args, base::TimeTicks now);
  void FinishImplFrame();
  void SendDidNotProduceFrame(const viz::BeginFrameArgs& args);
  void OnBeginImplFrameDeadline();
  void PollToAdvanceCommitState();
  void BeginMainFrameAnimateAndLayoutOnly(const viz::BeginFrameArgs& args);

  bool IsInsideAction(SchedulerStateMachine::Action action) {
    return inside_action_ == action;
  }

  DISALLOW_COPY_AND_ASSIGN(Scheduler);
};

}  // namespace cc

#endif  // CC_SCHEDULER_SCHEDULER_H_
