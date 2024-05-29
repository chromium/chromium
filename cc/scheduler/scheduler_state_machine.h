// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SCHEDULER_SCHEDULER_STATE_MACHINE_H_
#define CC_SCHEDULER_SCHEDULER_STATE_MACHINE_H_

#include <stdint.h>

#include "base/tracing/protos/chrome_track_event.pbzero.h"
#include "cc/cc_export.h"
#include "cc/scheduler/commit_earlyout_reason.h"
#include "cc/scheduler/draw_result.h"
#include "cc/scheduler/scheduler_settings.h"
#include "cc/tiles/tile_priority.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"

namespace cc {

enum class ScrollHandlerState {
  SCROLL_AFFECTS_SCROLL_HANDLER,
  SCROLL_DOES_NOT_AFFECT_SCROLL_HANDLER,
};

// The SchedulerStateMachine decides how to coordinate main thread activites
// like painting/running javascript with rendering and input activities on the
// impl thread.
//
// The state machine tracks internal state but is also influenced by external
// state.  Internal state includes things like whether a frame has been
// requested, while external state includes things like the current time being
// near to the vblank time.
//
// The scheduler seperates "what to do next" from the updating of its internal
// state to make testing cleaner.
class CC_EXPORT SchedulerStateMachine {
 public:
  // settings must be valid for the lifetime of this class.
  explicit SchedulerStateMachine(const SchedulerSettings& settings);
  SchedulerStateMachine(const SchedulerStateMachine&) = delete;
  ~SchedulerStateMachine();

  SchedulerStateMachine& operator=(const SchedulerStateMachine&) = delete;

  enum class LayerTreeFrameSinkState {
    NONE,
    ACTIVE,
    CREATING,
    WAITING_FOR_FIRST_COMMIT,
    WAITING_FOR_FIRST_ACTIVATION,
  };
  static perfetto::protos::pbzero::ChromeCompositorStateMachineV2::
      MajorStateV2::LayerTreeFrameSinkState
      LayerTreeFrameSinkStateToProtozeroEnum(LayerTreeFrameSinkState state);

  // Note: BeginImplFrameState does not cycle through these states in a fixed
  // order on all platforms. It's up to the scheduler to set these correctly.
  enum class BeginImplFrameState {
    IDLE,
    INSIDE_BEGIN_FRAME,
    INSIDE_DEADLINE,
  };
  static perfetto::protos::pbzero::ChromeCompositorStateMachineV2::
      MajorStateV2::BeginImplFrameState
      BeginImplFrameStateToProtozeroEnum(BeginImplFrameState state);

  // The scheduler uses a deadline to wait for main thread updates before
  // submitting a compositor frame. BeginImplFrameDeadlineMode specifies when
  // the deadline should run.
  enum class BeginImplFrameDeadlineMode {
    NONE = 0,       // No deadline should be scheduled e.g. for synchronous
                    // compositor.
    IMMEDIATE = 1,  // Deadline should be scheduled to run immediately.
    WAIT_FOR_SCROLL = 2,  // Deadline should be delayed as we are expecting
                          // input for a scroll.
    REGULAR = 3,          // Deadline should be scheduled to run at the deadline
                          // provided by in the BeginFrameArgs.
    LATE = 4,     // Deadline should be scheduled run when the next frame is
                  // expected to arrive.
    BLOCKED = 5,  // Deadline should be blocked indefinitely until the next
                  // frame arrives.
    kMaxValue = BLOCKED,
  };
  // TODO(nuskos): Update Scheduler::ScheduleBeginImplFrameDeadline event to
  // used typed macros so we can remove this ToString function.
  static const char* BeginImplFrameDeadlineModeToString(
      BeginImplFrameDeadlineMode mode);
  static perfetto::protos::pbzero::ChromeCompositorSchedulerStateV2::
      BeginImplFrameDeadlineMode
      BeginImplFrameDeadlineModeToProtozeroEnum(
          BeginImplFrameDeadlineMode mode);

  enum class BeginMainFrameState {
    IDLE,             // A new BeginMainFrame can start.
    SENT,             // A BeginMainFrame has already been issued.
    READY_TO_COMMIT,  // A previously issued BeginMainFrame has been processed,
                      // and is ready to commit.
  };
  static perfetto::protos::pbzero::ChromeCompositorStateMachineV2::
      MajorStateV2::BeginMainFrameState
      BeginMainFrameStateToProtozeroEnum(BeginMainFrameState state);

  // When a redraw is forced, it goes through a complete commit -> activation ->
  // draw cycle. Until a redraw has been forced, it remains in IDLE state.
  enum class ForcedRedrawOnTimeoutState {
    IDLE,
    WAITING_FOR_COMMIT,
    WAITING_FOR_ACTIVATION,
    WAITING_FOR_DRAW,
  };
  static perfetto::protos::pbzero::ChromeCompositorStateMachineV2::
      MajorStateV2::ForcedRedrawOnTimeoutState
      ForcedRedrawOnTimeoutStateToProtozeroEnum(
          ForcedRedrawOnTimeoutState state);

  BeginMainFrameState begin_main_frame_state() const {
    return begin_main_frame_state_;
  }

  bool CommitPending() const {
    return begin_main_frame_state_ != BeginMainFrameState::IDLE;
  }

  bool NewActiveTreeLikely() const {
    return (needs_begin_main_frame_ && !last_commit_had_no_updates_) ||
           CommitPending() || has_pending_tree_;
  }

  bool RedrawPending() const { return needs_redraw_; }
  bool PrepareTilesPending() const { return needs_prepare_tiles_; }

  enum class Action {
    NONE,
    SEND_BEGIN_MAIN_FRAME,
    COMMIT,
    POST_COMMIT,
    ACTIVATE_SYNC_TREE,
    PERFORM_IMPL_SIDE_INVALIDATION,
    DRAW_IF_POSSIBLE,
    DRAW_FORCED,
    DRAW_ABORT,
    UPDATE_DISPLAY_TREE,
    BEGIN_LAYER_TREE_FRAME_SINK_CREATION,
    PREPARE_TILES,
    INVALIDATE_LAYER_TREE_FRAME_SINK,
    NOTIFY_BEGIN_MAIN_FRAME_NOT_EXPECTED_UNTIL,
    NOTIFY_BEGIN_MAIN_FRAME_NOT_EXPECTED_SOON,
  };
  static perfetto::protos::pbzero::ChromeCompositorSchedulerActionV2
  ActionToProtozeroEnum(Action action);

  void AsProtozeroInto(
      perfetto::protos::pbzero::ChromeCompositorStateMachineV2* state) const;

  Action NextAction() const;
  void WillSendBeginMainFrame();
  void WillNotifyBeginMainFrameNotExpectedUntil();
  void WillNotifyBeginMainFrameNotExpectedSoon();
  void WillCommit(bool commit_had_no_updates);
  void DidCommit();
  void DidPostCommit();
  void WillActivate();
  void WillDraw();
  void WillUpdateDisplayTree();
  void WillBeginLayerTreeFrameSinkCreation();
  void WillPrepareTiles();
  void WillInvalidateLayerTreeFrameSink();
  void WillPerformImplSideInvalidation();

  void DidDraw(DrawResult draw_result);

  void AbortDraw();

  // Indicates whether the impl thread needs a BeginImplFrame callback in order
  // to make progress.
  bool BeginFrameNeeded() const;

  // Indicates whether the compositor should continue to receive BeginFrame
  // notifications. This is different from BeginFrameNeeded() for cases where we
  // temporarily stop drawing. Unsubscribing and re-subscribing to BeginFrame
  // notifications creates unnecessary overhead.
  bool ShouldSubscribeToBeginFrames() const;

  // Indicates that the system has entered and left a BeginImplFrame callback.
  // The scheduler will not draw more than once in a given BeginImplFrame
  // callback nor send more than one BeginMainFrame message.
  void OnBeginImplFrame(const viz::BeginFrameId& frame_id, bool animate_only);
  // Indicates that the scheduler has entered the draw phase. The scheduler
  // will not draw more than once in a single draw phase.
  // TODO(sunnyps): Rename OnBeginImplFrameDeadline to OnDraw or similar.
  void OnBeginImplFrameDeadline();
  void OnBeginImplFrameIdle();

  int current_frame_number() const { return current_frame_number_; }

  BeginImplFrameState begin_impl_frame_state() const {
    return begin_impl_frame_state_;
  }

  // Returns BeginImplFrameDeadlineMode computed based on current state.
  BeginImplFrameDeadlineMode CurrentBeginImplFrameDeadlineMode() const;

  // If the main thread didn't manage to produce a new frame in time for the
  // impl thread to draw, it is in a high latency mode.
  bool main_thread_missed_last_deadline() const {
    return main_thread_missed_last_deadline_;
  }

  bool IsDrawThrottled() const;

  // Indicates whether the LayerTreeHostImpl is visible.
  void SetVisible(bool visible);
  bool visible() const { return visible_; }

  // Indicates that warming up is requested to create a new LayerTreeFrameSink
  // even if the LayerTreeHost is invisible. This is an experimental function
  // and only used if `kWarmUpCompositor` is enabled. Currently, this will be
  // requested only from prerendered pages. Please see crbug.com/40240492 for
  // more details.
  void SetShouldWarmUp();

  void SetBeginFrameSourcePaused(bool paused);
  bool begin_frame_source_paused() const { return begin_frame_source_paused_; }

  // Indicates that a redraw is required, either due to the impl tree changing
  // or the screen being damaged and simply needing redisplay. Note that if the
  // changes in the impl tree has not been activated yet, then |needs_redraw()|
  // can return false. For checking any invalidations, check
  // |did_invalidate_layer_tree_frame_sink()|.
  void SetNeedsRedraw();
  bool needs_redraw() const { return needs_redraw_; }

  // Indicates that the display tree needs an update, implying that the active
  // tree has changed in some meaningful way since the last update.
  void SetNeedsUpdateDisplayTree();
  bool needs_update_display_tree() const { return needs_update_display_tree_; }

  bool did_invalidate_layer_tree_frame_sink() const {
    return did_invalidate_layer_tree_frame_sink_;
  }

  // Indicates that prepare-tiles is required. This guarantees another
  // PrepareTiles will occur shortly (even if no redraw is required).
  void SetNeedsPrepareTiles();

  // If the scheduler attempted to draw, this provides feedback regarding
  // whether or not a CompositorFrame was actually submitted. We might skip the
  // submitting anything when there is not damage, for example.
  void DidSubmitCompositorFrame();

  // Notification from the LayerTreeFrameSink that a submitted frame has been
  // consumed and it is ready for the next one.
  void DidReceiveCompositorFrameAck();

  int pending_submit_frames() const { return pending_submit_frames_; }

  // Indicates whether to prioritize impl thread latency (i.e., animation
  // smoothness) over new content activation.
  void SetTreePrioritiesAndScrollState(TreePriority tree_priority,
                                       ScrollHandlerState scroll_handler_state);

  // Indicates if the main thread will likely respond within 1 vsync.
  void SetCriticalBeginMainFrameToActivateIsFast(bool is_fast);

  // A function of SetTreePrioritiesAndScrollState and
  // SetCriticalBeginMainFrameToActivateIsFast.
  bool ImplLatencyTakesPriority() const;

  // Indicates that a new begin main frame flow needs to be performed, either
  // to pull updates from the main thread to the impl, or to push deltas from
  // the impl thread to main.
  void SetNeedsBeginMainFrame();
  bool needs_begin_main_frame() const { return needs_begin_main_frame_; }

  void SetMainThreadWantsBeginMainFrameNotExpectedMessages(bool new_state);

  // Requests a single impl frame (after the current frame if there is one
  // active).
  void SetNeedsOneBeginImplFrame();

  // Call this only in response to receiving an Action::SEND_BEGIN_MAIN_FRAME
  // from NextAction.
  // Indicates that all painting is complete.
  void NotifyReadyToCommit();

  // Call this only in response to receiving an Action::SEND_BEGIN_MAIN_FRAME
  // from NextAction if the client rejects the BeginMainFrame message.
  void BeginMainFrameAborted(CommitEarlyOutReason reason);

  // For Android WebView, resourceless software draws are allowed even when
  // invisible.
  void SetResourcelessSoftwareDraw(bool resourceless_draw);

  // Indicates whether drawing would, at this time, make sense.
  // CanDraw can be used to suppress flashes or checkerboarding
  // when such behavior would be undesirable.
  void SetCanDraw(bool can);

  // For Android WebView, indicates that the draw should be skipped because the
  // frame sink is not ready to receive frames.
  void SetSkipDraw(bool skip);

  // Indicates that the pending tree is ready for activation. Returns whether
  // the notification received updated the state for the current pending tree,
  // if any.
  bool NotifyReadyToActivate();
  bool IsReadyToActivate();

  // Indicates the active tree's visible tiles are ready to be drawn.
  void NotifyReadyToDraw();

  enum class AnimationWorkletState { PROCESSING, IDLE };
  enum class PaintWorkletState { PROCESSING, IDLE };
  enum class TreeType { ACTIVE, PENDING };

  // Indicates if currently processing animation worklets for the active or
  // pending tree. This is used to determine if the draw deadline should be
  // extended or activation delayed.
  void NotifyAnimationWorkletStateChange(AnimationWorkletState state,
                                         TreeType tree);

  // Sets whether asynchronous paint worklets are running. Paint worklets
  // running should block activation of the pending tree, as it isn't fully
  // painted until they are done.
  void NotifyPaintWorkletStateChange(PaintWorkletState state);

  void SetNeedsImplSideInvalidation(bool needs_first_draw_on_activation);

  bool has_pending_tree() const { return has_pending_tree_; }
  bool active_tree_needs_first_draw() const {
    return active_tree_needs_first_draw_;
  }

  void DidPrepareTiles();
  void DidLoseLayerTreeFrameSink();
  void DidCreateAndInitializeLayerTreeFrameSink();
  bool HasInitializedLayerTreeFrameSink() const;

  // True if we need to abort draws to make forward progress.
  bool PendingDrawsShouldBeAborted() const;

  bool CouldSendBeginMainFrame() const;

  void SetDeferBeginMainFrame(bool defer_begin_main_frame);
  void SetPauseRendering(bool pause_rendering);

  void SetVideoNeedsBeginFrames(bool video_needs_begin_frames);

  bool did_submit_in_last_frame() const { return did_submit_in_last_frame_; }
  bool draw_succeeded_in_last_frame() const {
    return draw_succeeded_in_last_frame_;
  }

  bool needs_impl_side_invalidation() const {
    return needs_impl_side_invalidation_;
  }
  bool previous_pending_tree_was_impl_side() const {
    return previous_pending_tree_was_impl_side_;
  }
  bool critical_begin_main_frame_to_activate_is_fast() const {
    return critical_begin_main_frame_to_activate_is_fast_;
  }

  void set_should_defer_invalidation_for_fast_main_frame(bool defer) {
    should_defer_invalidation_for_fast_main_frame_ = defer;
  }
  bool should_defer_invalidation_for_fast_main_frame() const {
    return should_defer_invalidation_for_fast_main_frame_;
  }

  bool pending_tree_is_ready_for_activation() const {
    return pending_tree_is_ready_for_activation_;
  }

  bool resourceless_draw() const { return resourceless_draw_; }

  void set_is_scrolling(bool is_scrolling) { is_scrolling_ = is_scrolling; }
  void set_waiting_for_scroll_event(bool waiting_for_scroll_event) {
    waiting_for_scroll_event_ = waiting_for_scroll_event;
  }

 protected:
  bool BeginFrameRequiredForAction() const;
  bool BeginFrameNeededForVideo() const;
  bool ProactiveBeginFrameWanted() const;

  // Indicates if we should post a deadline for drawing, and if we should delay
  // sending BeginMainFrame. This is true when we are expecting a scroll event
  // to arrive, are prioritizing smoothness, and have begun frame production.
  bool ShouldWaitForScrollEvent() const;

  // Indicates if we should post the deadline to draw immediately. This is true
  // when we aren't expecting a commit or activation, or we're prioritizing
  // active tree draw (see ImplLatencyTakesPriority()).
  bool ShouldTriggerBeginImplFrameDeadlineImmediately() const;

  // Indicates if we shouldn't schedule a deadline. Used to defer drawing until
  // the entire pipeline is flushed and active tree is ready to draw for
  // headless.
  bool ShouldBlockDeadlineIndefinitely() const;

  bool ShouldPerformImplSideInvalidation() const;
  bool CouldCreatePendingTree() const;
  bool ShouldDeferInvalidatingForMainFrame() const;

  bool ShouldAbortCurrentFrame() const;

  bool ShouldBeginLayerTreeFrameSinkCreation() const;
  bool ShouldDraw() const;
  bool ShouldUpdateDisplayTree() const;
  bool ShouldActivateSyncTree() const;
  bool ShouldSendBeginMainFrame() const;
  bool ShouldCommit() const;
  bool ShouldRunPostCommit() const;
  bool ShouldPrepareTiles() const;
  bool ShouldInvalidateLayerTreeFrameSink() const;
  bool ShouldNotifyBeginMainFrameNotExpectedUntil() const;
  bool ShouldNotifyBeginMainFrameNotExpectedSoon() const;

  void WillDrawInternal();
  void WillPerformImplSideInvalidationInternal();
  void DidDrawInternal(DrawResult draw_result);

  const SchedulerSettings settings_;

  LayerTreeFrameSinkState layer_tree_frame_sink_state_ =
      LayerTreeFrameSinkState::NONE;
  BeginImplFrameState begin_impl_frame_state_ = BeginImplFrameState::IDLE;
  BeginMainFrameState begin_main_frame_state_ = BeginMainFrameState::IDLE;
  // This tracks a BMF sent to the main thread before we're finished processing
  // the previous BMF (tracked by begin_main_frame_state_) on the impl thread.
  BeginMainFrameState next_begin_main_frame_state_ = BeginMainFrameState::IDLE;

  // A redraw is forced when too many checkerboarded-frames are produced during
  // an animation.
  ForcedRedrawOnTimeoutState forced_redraw_state_ =
      ForcedRedrawOnTimeoutState::IDLE;

  // These are used for tracing only.
  int commit_count_ = 0;
  int current_frame_number_ = 0;
  int last_frame_number_submit_performed_ = -1;
  int last_frame_number_draw_performed_ = -1;
  int last_frame_number_begin_main_frame_sent_ = -1;
  int last_frame_number_invalidate_layer_tree_frame_sink_performed_ = -1;

  // Inputs from the last impl frame that are required for decisions made in
  // this impl frame. The values from the last frame are cached before being
  // reset in OnBeginImplFrame.
  struct FrameEvents {
    bool commit_had_no_updates = false;
    bool did_commit_during_frame = false;
  };
  FrameEvents last_frame_events_;

  // These are used to ensure that an action only happens once per frame,
  // deadline, etc.
  bool did_draw_ = false;
  bool did_update_display_tree_ = false;
  bool did_send_begin_main_frame_for_current_frame_ = true;

  // Initialized to true to prevent begin main frame before begin frames have
  // started. Reset to true when we stop asking for begin frames.
  bool did_notify_begin_main_frame_not_expected_until_ = true;
  bool did_notify_begin_main_frame_not_expected_soon_ = true;

  bool did_commit_during_frame_ = false;
  bool did_invalidate_layer_tree_frame_sink_ = false;
  bool did_perform_impl_side_invalidation_ = false;
  bool did_prepare_tiles_ = false;

  int consecutive_checkerboard_animations_ = 0;
  int pending_submit_frames_ = 0;
  int submit_frames_with_current_layer_tree_frame_sink_ = 0;
  bool needs_redraw_ = false;
  bool needs_prepare_tiles_ = false;
  bool needs_begin_main_frame_ = false;
  bool needs_one_begin_impl_frame_ = false;
  bool needs_post_commit_ = false;
  bool needs_update_display_tree_ = false;
  bool visible_ = false;
  bool should_warm_up_ = false;
  bool begin_frame_source_paused_ = false;
  bool resourceless_draw_ = false;
  bool can_draw_ = false;
  bool skip_draw_ = false;
  bool has_pending_tree_ = false;
  bool pending_tree_is_ready_for_activation_ = false;
  bool active_tree_needs_first_draw_ = false;
  bool did_create_and_initialize_first_layer_tree_frame_sink_ = false;
  TreePriority tree_priority_ = NEW_CONTENT_TAKES_PRIORITY;
  ScrollHandlerState scroll_handler_state_ =
      ScrollHandlerState::SCROLL_DOES_NOT_AFFECT_SCROLL_HANDLER;
  bool critical_begin_main_frame_to_activate_is_fast_ = true;
  bool main_thread_missed_last_deadline_ = false;
  bool defer_begin_main_frame_ = false;
  bool pause_rendering_ = false;
  bool waiting_for_activation_after_rendering_resumed_ = false;
  bool video_needs_begin_frames_ = false;
  bool last_commit_had_no_updates_ = false;
  bool active_tree_is_ready_to_draw_ = true;
  bool did_attempt_draw_in_last_frame_ = false;
  bool draw_succeeded_in_last_frame_ = false;
  bool did_submit_in_last_frame_ = false;
  bool needs_impl_side_invalidation_ = false;
  bool next_invalidation_needs_first_draw_on_activation_ = false;
  bool should_defer_invalidation_for_fast_main_frame_ = true;
  bool begin_frame_is_animate_only_ = false;

  // Number of async mutation cycles for the active tree that are in-flight or
  // queued.  Can be 0, 1 or 2.
  int processing_animation_worklets_for_active_tree_ = 0;
  // Indicates if an aysnc mutation cycle is in-flight or queued for the pending
  // tree.  Only one can be running or queued at any time.
  bool processing_animation_worklets_for_pending_tree_ = false;
  // Indicates if asychronous paint worklet painting is ongoing for the pending
  // tree. During this time we should not activate the pending tree.
  bool processing_paint_worklets_for_pending_tree_ = false;

  bool previous_pending_tree_was_impl_side_ = false;
  bool current_pending_tree_is_impl_side_ = false;

  bool wants_begin_main_frame_not_expected_ = false;

  // If set to true, the pending tree must be drawn at least once after
  // activation before a new tree can be activated.
  bool pending_tree_needs_first_draw_on_activation_ = false;

  bool draw_aborted_for_paused_begin_frame_ = false;

  unsigned consecutive_cant_draw_count_ = 0u;

  // When true we will prioritize BeginImplFrameDeadlineMode::SCROLL if
  // `SchedulerSettings.scroll_deadline_mode_enabled_` is enabled.
  bool is_scrolling_ = false;
  // Only true when `is_scrolling_` is also true. While true there was no
  // available scroll events at the start of OnBeginImplFrame and we were
  // expecting some. Once `is_scrolling_` is false, we are no longer expecting
  // scroll events to arrive.
  bool waiting_for_scroll_event_ = false;
};

}  // namespace cc

#endif  // CC_SCHEDULER_SCHEDULER_STATE_MACHINE_H_
