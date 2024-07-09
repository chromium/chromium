// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SCHEDULER_SCHEDULER_H_
#define CC_SCHEDULER_SCHEDULER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/cancelable_callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "cc/cc_export.h"
#include "cc/metrics/event_metrics.h"
#include "cc/metrics/submit_info.h"
#include "cc/scheduler/begin_frame_tracker.h"
#include "cc/scheduler/draw_result.h"
#include "cc/scheduler/scheduler_settings.h"
#include "cc/scheduler/scheduler_state_machine.h"
#include "cc/tiles/tile_priority.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/frame_sinks/delay_based_time_source.h"

namespace perfetto {
namespace protos {
namespace pbzero {
class ChromeCompositorSchedulerStateV2;
}
}  // namespace protos
}  // namespace perfetto
namespace base {
class SingleThreadTaskRunner;
}

namespace viz {
struct FrameTimingDetails;
}

namespace cc {
struct BeginMainFrameMetrics;
class CompositorTimingHistory;
class CompositorFrameReportingController;

enum class FrameSkippedReason {
  kRecoverLatency,
  kNoDamage,
  kWaitingOnMain,
  kDrawThrottled,
};

class SchedulerClient {
 public:
  // Returns whether the frame has damage.
  virtual bool WillBeginImplFrame(const viz::BeginFrameArgs& args) = 0;
  virtual void ScheduledActionSendBeginMainFrame(
      const viz::BeginFrameArgs& args) = 0;
  virtual DrawResult ScheduledActionDrawIfPossible() = 0;
  virtual DrawResult ScheduledActionDrawForced() = 0;
  virtual void ScheduledActionUpdateDisplayTree() = 0;

  // The Commit step occurs when the client received the BeginFrame from the
  // source and we perform at most one commit per BeginFrame. In this step the
  // main thread collects all updates then blocks and gives control to the
  // compositor thread, which allows Compositor thread to update its layer tree
  // to match the state of the layer tree on the main thread.
  virtual void ScheduledActionCommit() = 0;
  virtual void ScheduledActionPostCommit() = 0;
  virtual void ScheduledActionActivateSyncTree() = 0;
  virtual void ScheduledActionBeginLayerTreeFrameSinkCreation() = 0;
  virtual void ScheduledActionPrepareTiles() = 0;
  virtual void ScheduledActionInvalidateLayerTreeFrameSink(
      bool needs_redraw) = 0;
  virtual void ScheduledActionPerformImplSideInvalidation() = 0;
  // Called when the scheduler is done processing a frame. Note that the
  // BeginFrameArgs instance passed may not necessarily be the same instance
  // that was passed to WillBeginImplFrame(). Rather, |last_activated_args|
  // represents the latest BeginFrameArgs instance that caused an activation to
  // happen.
  virtual void DidFinishImplFrame(
      const viz::BeginFrameArgs& last_activated_args) = 0;
  virtual void DidNotProduceFrame(const viz::BeginFrameAck& ack,
                                  FrameSkippedReason reason) = 0;
  virtual void WillNotReceiveBeginFrame() = 0;
  virtual void SendBeginMainFrameNotExpectedSoon() = 0;
  virtual void ScheduledActionBeginMainFrameNotExpectedUntil(
      base::TimeTicks time) = 0;
  virtual void FrameIntervalUpdated(base::TimeDelta interval) = 0;
  virtual void OnBeginImplFrameDeadline() = 0;

 protected:
  virtual ~SchedulerClient() {}
};

class CC_EXPORT Scheduler : public viz::BeginFrameObserverBase {
 public:
  Scheduler(SchedulerClient* client,
            const SchedulerSettings& scheduler_settings,
            int layer_tree_host_id,
            base::SingleThreadTaskRunner* task_runner,
            std::unique_ptr<CompositorTimingHistory> compositor_timing_history,
            CompositorFrameReportingController*
                compositor_frame_reporting_controller);
  Scheduler(const Scheduler&) = delete;
  ~Scheduler() override;

  Scheduler& operator=(const Scheduler&) = delete;

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
  void SetShouldWarmUp();
  void SetCanDraw(bool can_draw);

  // We have 2 copies of the layer trees on the compositor thread: pending_tree
  // and active_tree. When we finish asynchronously rastering all tiles on
  // pending_tree, call this method to notify that this pending tree is ready to
  // be activated, that is to be copied to the active tree.
  void NotifyReadyToActivate();
  bool IsReadyToActivate();
  void NotifyReadyToDraw();
  void SetBeginFrameSource(viz::BeginFrameSource* source);

  using AnimationWorkletState = SchedulerStateMachine::AnimationWorkletState;
  using PaintWorkletState = SchedulerStateMachine::PaintWorkletState;
  using TreeType = SchedulerStateMachine::TreeType;

  // Sets whether asynchronous animation worklet mutations are running.
  // Mutations on the pending tree should block activiation. Mutations on the
  // active tree should delay draw to allow time for the mutations to complete.
  void NotifyAnimationWorkletStateChange(AnimationWorkletState state,
                                         TreeType tree);

  // Sets whether asynchronous paint worklets are running. Paint worklets
  // running should block activation of the pending tree, as it isn't fully
  // painted until they are done.
  void NotifyPaintWorkletStateChange(PaintWorkletState state);

  // Set |needs_begin_main_frame_| to true, which will cause the BeginFrame
  // source to be told to send BeginFrames to this client so that this client
  // can send a CompositorFrame to the display compositor with appropriate
  // timing.
  void SetNeedsBeginMainFrame();

  // Requests a single impl frame (after the current frame if there is one
  // active).
  void SetNeedsOneBeginImplFrame();

  void SetNeedsRedraw();
  void SetNeedsUpdateDisplayTree();

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

  bool pending_tree_is_ready_for_activation() const {
    return state_machine_.pending_tree_is_ready_for_activation();
  }

  // Drawing should result in submitting a CompositorFrame to the
  // LayerTreeFrameSink and then calling this.
  void DidSubmitCompositorFrame(SubmitInfo& submit_info);
  // The LayerTreeFrameSink acks when it is ready for a new frame which
  // should result in this getting called to unblock the next draw.
  void DidReceiveCompositorFrameAck();

  void SetTreePrioritiesAndScrollState(TreePriority tree_priority,
                                       ScrollHandlerState scroll_handler_state);

  // Commit step happens after the main thread has completed updating for a
  // BeginMainFrame request from the compositor, and blocks the main thread
  // to copy the layer tree to the compositor thread. Call this method when the
  // main thread updates are completed to signal it is ready for the commmit.
  void NotifyReadyToCommit(std::unique_ptr<BeginMainFrameMetrics> details);
  void BeginMainFrameAborted(CommitEarlyOutReason reason);

  // In the PrepareTiles step, compositor thread divides the layers into tiles
  // to reduce cost of raster large layers. Then, each tile is rastered by a
  // dedicated thread.
  void DidPrepareTiles();

  // |DidPresentCompositorFrame| is called when the renderer receives
  // presentation feedback.
  void DidPresentCompositorFrame(uint32_t frame_token,
                                 const viz::FrameTimingDetails& details);

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

  // Deferring begin main frame prevents all document lkifecycle updates and
  // updates of new layer tree state.
  void SetDeferBeginMainFrame(bool defer_begin_main_frame);

  // Pausing rendering prevents new main frames and impl-side invalidations from
  // being triggered. Impl frames are drawn until any in-flight updates from the
  // main thread are drawn.
  void SetPauseRendering(bool pause_rendering);

  // Controls whether the BeginMainFrameNotExpected messages should be sent to
  // the main thread by the cc scheduler.
  void SetMainThreadWantsBeginMainFrameNotExpected(bool new_state);

  void AsProtozeroInto(
      perfetto::EventContext& ctx,
      perfetto::protos::pbzero::ChromeCompositorSchedulerStateV2* state) const;

  void SetVideoNeedsBeginFrames(bool video_needs_begin_frames);

  // When `SetIsScrolling` notifies of a scroll, and when
  // `SetWaitingForScrollEvent` notifies that we do not yet have input to
  // process, we will prioritize BeginImplFrameDeadlineMode::SCROLL over that of
  // BeginImplFrameDeadlineMode::IMMEDIATE, BeginImplFrameDeadlineMode::REGULAR,
  // and BeginImplFrameDeadlineMode::LATE.
  void SetIsScrolling(bool is_scrolling);
  void SetWaitingForScrollEvent(bool waiting_for_scroll_event);

  const viz::BeginFrameSource* begin_frame_source() const {
    return begin_frame_source_;
  }

  viz::BeginFrameAck CurrentBeginFrameAckForActiveTree() const;

  const viz::BeginFrameArgs& last_dispatched_begin_main_frame_args() const {
    return last_dispatched_begin_main_frame_args_;
  }
  const viz::BeginFrameArgs& last_commit_origin_frame_args() const {
    return last_commit_origin_frame_args_;
  }
  const viz::BeginFrameArgs& last_activate_origin_frame_args() const {
    return last_activate_origin_frame_args_;
  }

  void ClearHistory();

  size_t CommitDurationSampleCountForTesting() const;

 protected:
  // Virtual for testing.
  virtual base::TimeTicks Now() const;

  const SchedulerSettings settings_;
  const raw_ptr<SchedulerClient> client_;
  const int layer_tree_host_id_;
  raw_ptr<base::SingleThreadTaskRunner> task_runner_;

  raw_ptr<viz::BeginFrameSource> begin_frame_source_ = nullptr;
  bool observing_begin_frame_source_ = false;

  bool skipped_last_frame_missed_exceeded_deadline_ = false;

  std::unique_ptr<CompositorTimingHistory> compositor_timing_history_;

  // Owned by LayerTreeHostImpl and is destroyed when LayerTreeHostImpl is
  // destroyed.
  raw_ptr<CompositorFrameReportingController, AcrossTasksDanglingUntriaged>
      compositor_frame_reporting_controller_;

  // What the latest deadline was, and when it was scheduled.
  base::TimeTicks deadline_;
  base::TimeTicks deadline_scheduled_at_;
  SchedulerStateMachine::BeginImplFrameDeadlineMode deadline_mode_ =
      SchedulerStateMachine::BeginImplFrameDeadlineMode::NONE;

  BeginFrameTracker begin_impl_frame_tracker_;
  viz::BeginFrameAck last_begin_frame_ack_;
  viz::BeginFrameArgs begin_main_frame_args_;

  // For keeping track of the original BeginFrameArgs from the Main Thread
  // that led to the corresponding action, i.e.:
  //    BeginMainFrame => Commit => Activate => Submit
  // So, |last_commit_origin_frame_args_| is the BeginFrameArgs that was
  // dispatched to the main-thread, and lead to the commit to happen.
  // |last_activate_origin_frame_args_| is then set to that BeginFrameArgs when
  // the committed change is activated.
  viz::BeginFrameArgs last_dispatched_begin_main_frame_args_;
  viz::BeginFrameArgs next_commit_origin_frame_args_;
  viz::BeginFrameArgs last_commit_origin_frame_args_;
  viz::BeginFrameArgs last_activate_origin_frame_args_;

  // Task posted for the deadline or drawing phase of the scheduler. This task
  // can be rescheduled e.g. when the condition for the deadline is met, it is
  // scheduled to run immediately.
  // NOTE: Scheduler weak ptrs are not necessary if CancelableOnceCallback is
  // used.
  base::DeadlineTimer begin_impl_frame_deadline_timer_;

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

  bool needs_finish_frame_for_synchronous_compositor_ = false;

  // Keeps track of the begin frame interval from the last BeginFrameArgs to
  // arrive so that |client_| can be informed about changes.
  base::TimeDelta last_frame_interval_;

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

  void BeginMainFrameNotExpectedUntil(base::TimeTicks time);
  void BeginMainFrameNotExpectedSoon();
  void DrawIfPossible();
  void DrawForced();
  void UpdateDisplayTree();
  void ProcessScheduledActions();
  void UpdateCompositorTimingHistoryRecordingEnabled();
  void AdvanceCommitStateIfPossible();

  void BeginImplFrameWithDeadline(const viz::BeginFrameArgs& args);
  void BeginImplFrameSynchronous(const viz::BeginFrameArgs& args);
  void FinishImplFrameSynchronous();
  void BeginImplFrame(const viz::BeginFrameArgs& args, base::TimeTicks now);
  void FinishImplFrame();
  void SendDidNotProduceFrame(const viz::BeginFrameArgs& args,
                              FrameSkippedReason reason);
  void OnBeginImplFrameDeadline();
  void PollToAdvanceCommitState();
  void BeginMainFrameAnimateAndLayoutOnly(const viz::BeginFrameArgs& args);

  bool IsInsideAction(SchedulerStateMachine::Action action) {
    return inside_action_ == action;
  }
};

}  // namespace cc

#endif  // CC_SCHEDULER_SCHEDULER_H_
