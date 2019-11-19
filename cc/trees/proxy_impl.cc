// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/proxy_impl.h"

#include <string.h>

#include <algorithm>
#include <string>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "cc/base/devtools_instrumentation.h"
#include "cc/benchmarks/benchmark_instrumentation.h"
#include "cc/input/browser_controls_offset_manager.h"
#include "cc/metrics/compositor_timing_history.h"
#include "cc/paint/paint_worklet_layer_painter.h"
#include "cc/trees/layer_tree_frame_sink.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/mutator_host.h"
#include "cc/trees/proxy_main.h"
#include "cc/trees/render_frame_metadata_observer.h"
#include "cc/trees/scroll_and_scale_set.h"
#include "cc/trees/task_runner_provider.h"
#include "components/viz/common/frame_sinks/delay_based_time_source.h"
#include "components/viz/common/frame_timing_details.h"
#include "components/viz/common/gpu/context_provider.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

namespace cc {

namespace {

// Measured in seconds.
const double kSmoothnessTakesPriorityExpirationDelay = 0.25;

unsigned int nextBeginFrameId = 0;

}  // namespace

// Ensures that a CompletionEvent is always signaled.
class ScopedCompletionEvent {
 public:
  explicit ScopedCompletionEvent(CompletionEvent* event) : event_(event) {}
  ScopedCompletionEvent(const ScopedCompletionEvent&) = delete;
  ~ScopedCompletionEvent() { event_->Signal(); }

  ScopedCompletionEvent& operator=(const ScopedCompletionEvent&) = delete;

 private:
  CompletionEvent* const event_;
};

ProxyImpl::ProxyImpl(base::WeakPtr<ProxyMain> proxy_main_weak_ptr,
                     LayerTreeHost* layer_tree_host,
                     TaskRunnerProvider* task_runner_provider)
    : layer_tree_host_id_(layer_tree_host->GetId()),
      commit_completion_waits_for_activation_(false),
      next_frame_is_newly_committed_frame_(false),
      inside_draw_(false),
      task_runner_provider_(task_runner_provider),
      smoothness_priority_expiration_notifier_(
          task_runner_provider->ImplThreadTaskRunner(),
          base::BindRepeating(&ProxyImpl::RenewTreePriority,
                              base::Unretained(this)),
          base::TimeDelta::FromSecondsD(
              kSmoothnessTakesPriorityExpirationDelay)),
      proxy_main_weak_ptr_(proxy_main_weak_ptr) {
  TRACE_EVENT0("cc", "ProxyImpl::ProxyImpl");
  DCHECK(IsImplThread());
  DCHECK(IsMainThreadBlocked());

  // Double checking we set this correctly since double->int truncations are
  // silent and have been done mistakenly: crbug.com/568120.
  DCHECK(!smoothness_priority_expiration_notifier_.delay().is_zero());

  host_impl_ = layer_tree_host->CreateLayerTreeHostImpl(this);
  const LayerTreeSettings& settings = layer_tree_host->GetSettings();
  send_compositor_frame_ack_ = settings.send_compositor_frame_ack;

  SchedulerSettings scheduler_settings(settings.ToSchedulerSettings());

  std::unique_ptr<CompositorTimingHistory> compositor_timing_history(
      new CompositorTimingHistory(
          scheduler_settings.using_synchronous_renderer_compositor,
          CompositorTimingHistory::RENDERER_UMA,
          layer_tree_host->rendering_stats_instrumentation(),
          host_impl_->compositor_frame_reporting_controller()));
  scheduler_.reset(new Scheduler(this, scheduler_settings, layer_tree_host_id_,
                                 task_runner_provider_->ImplThreadTaskRunner(),
                                 std::move(compositor_timing_history)));

  DCHECK_EQ(scheduler_->visible(), host_impl_->visible());
}

ProxyImpl::BlockedMainCommitOnly::BlockedMainCommitOnly()
    : layer_tree_host(nullptr) {}

ProxyImpl::BlockedMainCommitOnly::~BlockedMainCommitOnly() = default;

ProxyImpl::~ProxyImpl() {
  TRACE_EVENT0("cc", "ProxyImpl::~ProxyImpl");
  DCHECK(IsImplThread());
  DCHECK(IsMainThreadBlocked());

  // Prevent the scheduler from performing actions while we're in an
  // inconsistent state.
  scheduler_->Stop();
  // Take away the LayerTreeFrameSink before destroying things so it doesn't
  // try to call into its client mid-shutdown.
  host_impl_->ReleaseLayerTreeFrameSink();

  // It is important to destroy LTHI before the Scheduler since it can make
  // callbacks that access it during destruction cleanup.
  host_impl_ = nullptr;
  scheduler_ = nullptr;

  // We need to explicitly shutdown the notifier to destroy any weakptrs it is
  // holding while still on the compositor thread. This also ensures any
  // callbacks holding a ProxyImpl pointer are cancelled.
  smoothness_priority_expiration_notifier_.Shutdown();
}

void ProxyImpl::InitializeMutatorOnImpl(
    std::unique_ptr<LayerTreeMutator> mutator) {
  TRACE_EVENT0("cc", "ProxyImpl::InitializeMutatorOnImpl");
  DCHECK(IsImplThread());
  host_impl_->SetLayerTreeMutator(std::move(mutator));
}

void ProxyImpl::InitializePaintWorkletLayerPainterOnImpl(
    std::unique_ptr<PaintWorkletLayerPainter> painter) {
  TRACE_EVENT0("cc", "ProxyImpl::InitializePaintWorkletLayerPainterOnImpl");
  DCHECK(IsImplThread());
  host_impl_->SetPaintWorkletLayerPainter(std::move(painter));
}

void ProxyImpl::UpdateBrowserControlsStateOnImpl(
    BrowserControlsState constraints,
    BrowserControlsState current,
    bool animate) {
  DCHECK(IsImplThread());
  host_impl_->browser_controls_manager()->UpdateBrowserControlsState(
      constraints, current, animate);
}

void ProxyImpl::InitializeLayerTreeFrameSinkOnImpl(
    LayerTreeFrameSink* layer_tree_frame_sink,
    base::WeakPtr<ProxyMain> proxy_main_frame_sink_bound_weak_ptr) {
  TRACE_EVENT0("cc", "ProxyImpl::InitializeLayerTreeFrameSinkOnImplThread");
  DCHECK(IsImplThread());

  proxy_main_frame_sink_bound_weak_ptr_ = proxy_main_frame_sink_bound_weak_ptr;

  LayerTreeHostImpl* host_impl = host_impl_.get();
  bool success = host_impl->InitializeFrameSink(layer_tree_frame_sink);
  MainThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&ProxyMain::DidInitializeLayerTreeFrameSink,
                                proxy_main_weak_ptr_, success));
  if (success)
    scheduler_->DidCreateAndInitializeLayerTreeFrameSink();
}

void ProxyImpl::SetDeferBeginMainFrameOnImpl(
    bool defer_begin_main_frame) const {
  DCHECK(IsImplThread());
  scheduler_->SetDeferBeginMainFrame(defer_begin_main_frame);
}

void ProxyImpl::SetNeedsRedrawOnImpl(const gfx::Rect& damage_rect) {
  DCHECK(IsImplThread());
  host_impl_->SetViewportDamage(damage_rect);
  SetNeedsRedrawOnImplThread();
}

void ProxyImpl::SetNeedsCommitOnImpl() {
  DCHECK(IsImplThread());
  SetNeedsCommitOnImplThread();
}

void ProxyImpl::BeginMainFrameAbortedOnImpl(
    CommitEarlyOutReason reason,
    base::TimeTicks main_thread_start_time,
    std::vector<std::unique_ptr<SwapPromise>> swap_promises) {
  TRACE_EVENT1("cc", "ProxyImpl::BeginMainFrameAbortedOnImplThread", "reason",
               CommitEarlyOutReasonToString(reason));
  DCHECK(IsImplThread());
  DCHECK(scheduler_->CommitPending());

  host_impl_->BeginMainFrameAborted(
      reason, std::move(swap_promises),
      scheduler_->last_dispatched_begin_main_frame_args());
  scheduler_->NotifyBeginMainFrameStarted(main_thread_start_time);
  scheduler_->BeginMainFrameAborted(reason);
}

void ProxyImpl::SetVisibleOnImpl(bool visible) {
  TRACE_EVENT1("cc", "ProxyImpl::SetVisibleOnImplThread", "visible", visible);
  DCHECK(IsImplThread());
  host_impl_->SetVisible(visible);
  scheduler_->SetVisible(visible);
}

void ProxyImpl::ReleaseLayerTreeFrameSinkOnImpl(CompletionEvent* completion) {
  DCHECK(IsImplThread());

  // Unlike DidLoseLayerTreeFrameSinkOnImplThread, we don't need to call
  // LayerTreeHost::DidLoseLayerTreeFrameSink since it already knows.
  scheduler_->DidLoseLayerTreeFrameSink();
  host_impl_->ReleaseLayerTreeFrameSink();
  completion->Signal();
}

void ProxyImpl::FinishGLOnImpl(CompletionEvent* completion) {
  TRACE_EVENT0("cc", "ProxyImpl::FinishGLOnImplThread");
  DCHECK(IsImplThread());
  if (host_impl_->layer_tree_frame_sink()) {
    viz::ContextProvider* context_provider =
        host_impl_->layer_tree_frame_sink()->context_provider();
    if (context_provider)
      context_provider->ContextGL()->Finish();
  }
  completion->Signal();
}

void ProxyImpl::MainFrameWillHappenOnImplForTesting(
    CompletionEvent* completion,
    bool* main_frame_will_happen) {
  DCHECK(IsImplThread());
  if (host_impl_->layer_tree_frame_sink()) {
    *main_frame_will_happen = scheduler_->MainFrameForTestingWillHappen();
  } else {
    *main_frame_will_happen = false;
  }
  completion->Signal();
}

void ProxyImpl::RequestBeginMainFrameNotExpected(bool new_state) {
  DCHECK(IsImplThread());
  DCHECK(scheduler_);
  TRACE_EVENT1("cc", "ProxyImpl::RequestBeginMainFrameNotExpected", "new_state",
               new_state);
  scheduler_->SetMainThreadWantsBeginMainFrameNotExpected(new_state);
}

void ProxyImpl::NotifyReadyToCommitOnImpl(
    CompletionEvent* completion,
    LayerTreeHost* layer_tree_host,
    base::TimeTicks main_thread_start_time,
    bool hold_commit_for_activation) {
  TRACE_EVENT0("cc", "ProxyImpl::NotifyReadyToCommitOnImpl");
  DCHECK(!commit_completion_event_);
  DCHECK(IsImplThread() && IsMainThreadBlocked());
  DCHECK(scheduler_);
  DCHECK(scheduler_->CommitPending());

  if (!host_impl_) {
    TRACE_EVENT_INSTANT0("cc", "EarlyOut_NoLayerTree",
                         TRACE_EVENT_SCOPE_THREAD);
    completion->Signal();
    return;
  }

  // Ideally, we should inform to impl thread when BeginMainFrame is started.
  // But, we can avoid a PostTask in here.
  scheduler_->NotifyBeginMainFrameStarted(main_thread_start_time);

  host_impl_->ReadyToCommit();

  commit_completion_event_ =
      std::make_unique<ScopedCompletionEvent>(completion);
  commit_completion_waits_for_activation_ = hold_commit_for_activation;

  DCHECK(!blocked_main_commit().layer_tree_host);
  blocked_main_commit().layer_tree_host = layer_tree_host;

  // Extract metrics data from the layer tree host and send them to the
  // scheduler to pass them to the compositor_timing_history object.
  std::unique_ptr<BeginMainFrameMetrics> main_frame_metrics =
      layer_tree_host->begin_main_frame_metrics();

  scheduler_->NotifyReadyToCommit(std::move(main_frame_metrics));
}

void ProxyImpl::DidLoseLayerTreeFrameSinkOnImplThread() {
  TRACE_EVENT0("cc", "ProxyImpl::DidLoseLayerTreeFrameSinkOnImplThread");
  DCHECK(IsImplThread());
  MainThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&ProxyMain::DidLoseLayerTreeFrameSink,
                                proxy_main_weak_ptr_));
  scheduler_->DidLoseLayerTreeFrameSink();
}

void ProxyImpl::SetBeginFrameSource(viz::BeginFrameSource* source) {
  // During shutdown, destroying the LayerTreeFrameSink may unset the
  // viz::BeginFrameSource.
  if (scheduler_) {
    // TODO(enne): this overrides any preexisting begin frame source.  Those
    // other sources will eventually be removed and this will be the only path.
    scheduler_->SetBeginFrameSource(source);
  }
}

void ProxyImpl::DidReceiveCompositorFrameAckOnImplThread() {
  TRACE_EVENT0("cc,benchmark",
               "ProxyImpl::DidReceiveCompositorFrameAckOnImplThread");
  DCHECK(IsImplThread());
  scheduler_->DidReceiveCompositorFrameAck();
  if (send_compositor_frame_ack_) {
    MainThreadTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(&ProxyMain::DidReceiveCompositorFrameAck,
                                  proxy_main_frame_sink_bound_weak_ptr_));
  }
}

void ProxyImpl::OnCanDrawStateChanged(bool can_draw) {
  TRACE_EVENT1("cc", "ProxyImpl::OnCanDrawStateChanged", "can_draw", can_draw);
  DCHECK(IsImplThread());
  scheduler_->SetCanDraw(can_draw);
}

void ProxyImpl::NotifyReadyToActivate() {
  TRACE_EVENT0("cc", "ProxyImpl::NotifyReadyToActivate");
  DCHECK(IsImplThread());
  scheduler_->NotifyReadyToActivate();
}

void ProxyImpl::NotifyReadyToDraw() {
  TRACE_EVENT0("cc", "ProxyImpl::NotifyReadyToDraw");
  DCHECK(IsImplThread());
  scheduler_->NotifyReadyToDraw();
}

void ProxyImpl::SetNeedsRedrawOnImplThread() {
  TRACE_EVENT0("cc", "ProxyImpl::SetNeedsRedrawOnImplThread");
  DCHECK(IsImplThread());
  scheduler_->SetNeedsRedraw();
}

void ProxyImpl::SetNeedsOneBeginImplFrameOnImplThread() {
  TRACE_EVENT0("cc", "ProxyImpl::SetNeedsOneBeginImplFrameOnImplThread");
  DCHECK(IsImplThread());
  scheduler_->SetNeedsOneBeginImplFrame();
}

void ProxyImpl::SetNeedsPrepareTilesOnImplThread() {
  DCHECK(IsImplThread());
  scheduler_->SetNeedsPrepareTiles();
}

void ProxyImpl::SetNeedsCommitOnImplThread() {
  TRACE_EVENT0("cc", "ProxyImpl::SetNeedsCommitOnImplThread");
  DCHECK(IsImplThread());
  scheduler_->SetNeedsBeginMainFrame();
}

void ProxyImpl::SetVideoNeedsBeginFrames(bool needs_begin_frames) {
  TRACE_EVENT1("cc", "ProxyImpl::SetVideoNeedsBeginFrames",
               "needs_begin_frames", needs_begin_frames);
  DCHECK(IsImplThread());
  // In tests the layer tree is destroyed after the scheduler is.
  if (scheduler_)
    scheduler_->SetVideoNeedsBeginFrames(needs_begin_frames);
}

void ProxyImpl::PostAnimationEventsToMainThreadOnImplThread(
    std::unique_ptr<MutatorEvents> events) {
  TRACE_EVENT0("cc", "ProxyImpl::PostAnimationEventsToMainThreadOnImplThread");
  DCHECK(IsImplThread());
  MainThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&ProxyMain::SetAnimationEvents,
                                proxy_main_weak_ptr_, std::move(events)));
}

size_t ProxyImpl::CompositedAnimationsCount() const {
  return host_impl_->mutator_host()->CompositedAnimationsCount();
}

size_t ProxyImpl::MainThreadAnimationsCount() const {
  return host_impl_->mutator_host()->MainThreadAnimationsCount();
}

bool ProxyImpl::HasCustomPropertyAnimations() const {
  return host_impl_->mutator_host()->HasCustomPropertyAnimations();
}

bool ProxyImpl::CurrentFrameHadRAF() const {
  return host_impl_->mutator_host()->CurrentFrameHadRAF();
}

bool ProxyImpl::NextFrameHasPendingRAF() const {
  return host_impl_->mutator_host()->NextFrameHasPendingRAF();
}

bool ProxyImpl::IsInsideDraw() {
  return inside_draw_;
}

bool ProxyImpl::IsBeginMainFrameExpected() {
  // Check whether the main-thread has requested for updates. If main-thread has
  // not responded to a previously dispatched BeginMainFrame, then assume that
  // main-thread would want to produce an update for the current frame too.
  return scheduler_->needs_begin_main_frame() ||
         scheduler_->IsBeginMainFrameSent();
}

void ProxyImpl::RenewTreePriority() {
  DCHECK(IsImplThread());
  const bool user_interaction_in_progress =
      host_impl_->pinch_gesture_active() ||
      host_impl_->page_scale_animation_active() ||
      host_impl_->IsActivelyScrolling();

  if (host_impl_->ukm_manager()) {
    host_impl_->ukm_manager()->SetUserInteractionInProgress(
        user_interaction_in_progress);
  }

  // Schedule expiration if smoothness currently takes priority.
  if (user_interaction_in_progress)
    smoothness_priority_expiration_notifier_.Schedule();

  // We use the same priority for both trees by default.
  TreePriority tree_priority = SAME_PRIORITY_FOR_BOTH_TREES;

  // Smoothness takes priority if we have an expiration for it scheduled.
  if (smoothness_priority_expiration_notifier_.HasPendingNotification())
    tree_priority = SMOOTHNESS_TAKES_PRIORITY;

  // New content always takes priority when ui resources have been evicted.
  if (host_impl_->active_tree()->GetDeviceViewport().size().IsEmpty() ||
      host_impl_->EvictedUIResourcesExist()) {
    // Once we enter NEW_CONTENTS_TAKES_PRIORITY mode, visible tiles on active
    // tree might be freed. We need to set RequiresHighResToDraw to ensure that
    // high res tiles will be required to activate pending tree.
    host_impl_->SetRequiresHighResToDraw();
    tree_priority = NEW_CONTENT_TAKES_PRIORITY;
  }

  host_impl_->SetTreePriority(tree_priority);

  // Only put the scheduler in impl latency prioritization mode if we don't
  // have a scroll listener. This gives the scroll listener a better chance of
  // handling scroll updates within the same frame. The tree itself is still
  // kept in prefer smoothness mode to allow checkerboarding.
  ScrollHandlerState scroll_handler_state =
      host_impl_->scroll_affects_scroll_handler()
          ? ScrollHandlerState::SCROLL_AFFECTS_SCROLL_HANDLER
          : ScrollHandlerState::SCROLL_DOES_NOT_AFFECT_SCROLL_HANDLER;
  scheduler_->SetTreePrioritiesAndScrollState(tree_priority,
                                              scroll_handler_state);
}

void ProxyImpl::PostDelayedAnimationTaskOnImplThread(base::OnceClosure task,
                                                     base::TimeDelta delay) {
  DCHECK(IsImplThread());
  task_runner_provider_->ImplThreadTaskRunner()->PostDelayedTask(
      FROM_HERE, std::move(task), delay);
}

void ProxyImpl::DidActivateSyncTree() {
  TRACE_EVENT0("cc", "ProxyImpl::DidActivateSyncTreeOnImplThread");
  DCHECK(IsImplThread());

  if (activation_completion_event_) {
    TRACE_EVENT_INSTANT0("cc", "ReleaseCommitbyActivation",
                         TRACE_EVENT_SCOPE_THREAD);
    activation_completion_event_ = nullptr;
  }
}

void ProxyImpl::WillPrepareTiles() {
  DCHECK(IsImplThread());
  scheduler_->WillPrepareTiles();
}

void ProxyImpl::DidPrepareTiles() {
  DCHECK(IsImplThread());
  scheduler_->DidPrepareTiles();
}

void ProxyImpl::DidCompletePageScaleAnimationOnImplThread() {
  DCHECK(IsImplThread());
  MainThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&ProxyMain::DidCompletePageScaleAnimation,
                                proxy_main_weak_ptr_));
}

void ProxyImpl::OnDrawForLayerTreeFrameSink(bool resourceless_software_draw,
                                            bool skip_draw) {
  DCHECK(IsImplThread());
  scheduler_->OnDrawForLayerTreeFrameSink(resourceless_software_draw,
                                          skip_draw);
}

void ProxyImpl::NeedsImplSideInvalidation(bool needs_first_draw_on_activation) {
  DCHECK(IsImplThread());
  scheduler_->SetNeedsImplSideInvalidation(needs_first_draw_on_activation);
}

void ProxyImpl::NotifyImageDecodeRequestFinished() {
  DCHECK(IsImplThread());
  SetNeedsCommitOnImplThread();
}

void ProxyImpl::DidPresentCompositorFrameOnImplThread(
    uint32_t frame_token,
    std::vector<LayerTreeHost::PresentationTimeCallback> callbacks,
    const viz::FrameTimingDetails& details) {
  MainThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&ProxyMain::DidPresentCompositorFrame,
                     proxy_main_weak_ptr_, frame_token, std::move(callbacks),
                     details.presentation_feedback));
  if (scheduler_)
    scheduler_->DidPresentCompositorFrame(frame_token, details);
}

void ProxyImpl::NotifyAnimationWorkletStateChange(
    AnimationWorkletMutationState state,
    ElementListType element_list_type) {
  DCHECK(IsImplThread());
  Scheduler::AnimationWorkletState animation_worklet_state =
      (state == AnimationWorkletMutationState::STARTED)
          ? Scheduler::AnimationWorkletState::PROCESSING
          : Scheduler::AnimationWorkletState::IDLE;
  Scheduler::TreeType tree_type = (element_list_type == ElementListType::ACTIVE)
                                      ? Scheduler::TreeType::ACTIVE
                                      : Scheduler::TreeType::PENDING;
  scheduler_->NotifyAnimationWorkletStateChange(animation_worklet_state,
                                                tree_type);
}

void ProxyImpl::NotifyPaintWorkletStateChange(
    Scheduler::PaintWorkletState state) {
  DCHECK(IsImplThread());
  scheduler_->NotifyPaintWorkletStateChange(state);
}

bool ProxyImpl::WillBeginImplFrame(const viz::BeginFrameArgs& args) {
  DCHECK(IsImplThread());
  return host_impl_->WillBeginImplFrame(args);
}

void ProxyImpl::DidFinishImplFrame() {
  DCHECK(IsImplThread());
  host_impl_->DidFinishImplFrame();
}

void ProxyImpl::DidNotProduceFrame(const viz::BeginFrameAck& ack) {
  DCHECK(IsImplThread());
  host_impl_->DidNotProduceFrame(ack);
}

void ProxyImpl::WillNotReceiveBeginFrame() {
  DCHECK(IsImplThread());
  host_impl_->DidNotNeedBeginFrame();
}

void ProxyImpl::ScheduledActionSendBeginMainFrame(
    const viz::BeginFrameArgs& args) {
  DCHECK(IsImplThread());
  unsigned int begin_frame_id = nextBeginFrameId++;
  benchmark_instrumentation::ScopedBeginFrameTask begin_frame_task(
      benchmark_instrumentation::kSendBeginFrame, begin_frame_id);
  std::unique_ptr<BeginMainFrameAndCommitState> begin_main_frame_state(
      new BeginMainFrameAndCommitState);
  begin_main_frame_state->begin_frame_id = begin_frame_id;
  begin_main_frame_state->begin_frame_args = args;
  begin_main_frame_state->scroll_info = host_impl_->ProcessScrollDeltas();
  begin_main_frame_state->evicted_ui_resources =
      host_impl_->EvictedUIResourcesExist();
  begin_main_frame_state->completed_image_decode_requests =
      host_impl_->TakeCompletedImageDecodeRequests();
  host_impl_->WillSendBeginMainFrame();
  MainThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&ProxyMain::BeginMainFrame, proxy_main_weak_ptr_,
                     std::move(begin_main_frame_state)));
  host_impl_->DidSendBeginMainFrame(args);
  devtools_instrumentation::DidRequestMainThreadFrame(layer_tree_host_id_);
}

DrawResult ProxyImpl::ScheduledActionDrawIfPossible() {
  TRACE_EVENT0("cc", "ProxyImpl::ScheduledActionDraw");
  DCHECK(IsImplThread());

  // The scheduler should never generate this call when it can't draw.
  DCHECK(host_impl_->CanDraw());

  bool forced_draw = false;
  return DrawInternal(forced_draw);
}

DrawResult ProxyImpl::ScheduledActionDrawForced() {
  TRACE_EVENT0("cc", "ProxyImpl::ScheduledActionDrawForced");
  DCHECK(IsImplThread());
  bool forced_draw = true;
  return DrawInternal(forced_draw);
}

void ProxyImpl::ScheduledActionCommit() {
  TRACE_EVENT0("cc", "ProxyImpl::ScheduledActionCommit");
  DCHECK(IsImplThread());
  DCHECK(IsMainThreadBlocked());
  DCHECK(commit_completion_event_);

  // Relax the cross-thread access restriction to non-thread-safe RefCount.
  // It's safe since the main thread is blocked while a main-thread-bound
  // compositor stuff are accessed from the impl thread.
  base::ScopedAllowCrossThreadRefCountAccess
      allow_cross_thread_ref_count_access;

  host_impl_->BeginCommit();
  blocked_main_commit().layer_tree_host->FinishCommitOnImplThread(
      host_impl_.get());

  // Remove the LayerTreeHost reference before the completion event is signaled
  // and cleared. This is necessary since blocked_main_commit() allows access
  // only while we have the completion event to ensure the main thread is
  // blocked for a commit.
  blocked_main_commit().layer_tree_host = nullptr;

  if (commit_completion_waits_for_activation_) {
    // For some layer types in impl-side painting, the commit is held until the
    // sync tree is activated.  It's also possible that the sync tree has
    // already activated if there was no work to be done.
    TRACE_EVENT_INSTANT0("cc", "HoldCommit", TRACE_EVENT_SCOPE_THREAD);
    commit_completion_waits_for_activation_ = false;
    activation_completion_event_ = std::move(commit_completion_event_);
  }
  commit_completion_event_ = nullptr;

  scheduler_->DidCommit();

  // Delay this step until afer the main thread has been released as it's
  // often a good bit of work to update the tree and prepare the new frame.
  host_impl_->CommitComplete();

  next_frame_is_newly_committed_frame_ = true;
}

void ProxyImpl::ScheduledActionActivateSyncTree() {
  TRACE_EVENT0("cc", "ProxyImpl::ScheduledActionActivateSyncTree");
  DCHECK(IsImplThread());
  host_impl_->ActivateSyncTree();
}

void ProxyImpl::ScheduledActionBeginLayerTreeFrameSinkCreation() {
  TRACE_EVENT0("cc",
               "ProxyImpl::ScheduledActionBeginLayerTreeFrameSinkCreation");
  DCHECK(IsImplThread());
  MainThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&ProxyMain::RequestNewLayerTreeFrameSink,
                                proxy_main_weak_ptr_));
}

void ProxyImpl::ScheduledActionPrepareTiles() {
  TRACE_EVENT0("cc", "ProxyImpl::ScheduledActionPrepareTiles");
  DCHECK(IsImplThread());
  host_impl_->PrepareTiles();
}

void ProxyImpl::ScheduledActionInvalidateLayerTreeFrameSink(bool needs_redraw) {
  TRACE_EVENT0("cc", "ProxyImpl::ScheduledActionInvalidateLayerTreeFrameSink");
  DCHECK(IsImplThread());
  host_impl_->InvalidateLayerTreeFrameSink(needs_redraw);
}

void ProxyImpl::ScheduledActionPerformImplSideInvalidation() {
  TRACE_EVENT0("cc", "ProxyImpl::ScheduledActionPerformImplSideInvalidation");
  DCHECK(IsImplThread());
  host_impl_->InvalidateContentOnImplSide();
}

void ProxyImpl::SendBeginMainFrameNotExpectedSoon() {
  DCHECK(IsImplThread());
  MainThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&ProxyMain::BeginMainFrameNotExpectedSoon,
                                proxy_main_weak_ptr_));
}

void ProxyImpl::ScheduledActionBeginMainFrameNotExpectedUntil(
    base::TimeTicks time) {
  DCHECK(IsImplThread());
  MainThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&ProxyMain::BeginMainFrameNotExpectedUntil,
                                proxy_main_weak_ptr_, time));
}

DrawResult ProxyImpl::DrawInternal(bool forced_draw) {
  DCHECK(IsImplThread());
  DCHECK(host_impl_.get());

  base::AutoReset<bool> mark_inside(&inside_draw_, true);

  if (host_impl_->pending_tree())
    host_impl_->pending_tree()->UpdateDrawProperties();

  // This method is called on a forced draw, regardless of whether we are able
  // to produce a frame, as the calling site on main thread is blocked until its
  // request completes, and we signal completion here. If CanDraw() is false, we
  // will indicate success=false to the caller, but we must still signal
  // completion to avoid deadlock.

  // We guard PrepareToDraw() with CanDraw() because it always returns a valid
  // frame, so can only be used when such a frame is possible. Since
  // DrawLayers() depends on the result of PrepareToDraw(), it is guarded on
  // CanDraw() as well.

  LayerTreeHostImpl::FrameData frame;
  frame.begin_frame_ack = scheduler_->CurrentBeginFrameAckForActiveTree();
  frame.origin_begin_main_frame_args =
      scheduler_->last_activate_origin_frame_args();
  bool draw_frame = false;

  DrawResult result;
  if (host_impl_->CanDraw()) {
    result = host_impl_->PrepareToDraw(&frame);
    draw_frame = forced_draw || result == DRAW_SUCCESS;
  } else {
    result = DRAW_ABORTED_CANT_DRAW;
  }

  if (draw_frame) {
    if (host_impl_->DrawLayers(&frame)) {
      DCHECK_NE(frame.frame_token, 0u);
      // Drawing implies we submitted a frame to the LayerTreeFrameSink.
      scheduler_->DidSubmitCompositorFrame(frame.frame_token);
    }
    result = DRAW_SUCCESS;
  } else {
    DCHECK_NE(DRAW_SUCCESS, result);
  }

  host_impl_->DidDrawAllLayers(frame);

  bool start_ready_animations = draw_frame;
  host_impl_->UpdateAnimationState(start_ready_animations);

  // Tell the main thread that the newly-commited frame was drawn.
  if (next_frame_is_newly_committed_frame_) {
    next_frame_is_newly_committed_frame_ = false;
    MainThreadTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(&ProxyMain::DidCommitAndDrawFrame,
                                  proxy_main_weak_ptr_));
  }

  DCHECK_NE(INVALID_RESULT, result);
  return result;
}

bool ProxyImpl::IsImplThread() const {
  return task_runner_provider_->IsImplThread();
}

bool ProxyImpl::IsMainThreadBlocked() const {
  return task_runner_provider_->IsMainThreadBlocked();
}

ProxyImpl::BlockedMainCommitOnly& ProxyImpl::blocked_main_commit() {
  DCHECK(IsMainThreadBlocked() && commit_completion_event_);
  return main_thread_blocked_commit_vars_unsafe_;
}

base::SingleThreadTaskRunner* ProxyImpl::MainThreadTaskRunner() {
  return task_runner_provider_->MainThreadTaskRunner();
}

void ProxyImpl::SetSourceURL(ukm::SourceId source_id, const GURL& url) {
  DCHECK(IsImplThread());
  host_impl_->SetActiveURL(url, source_id);
}

void ProxyImpl::ClearHistory() {
  DCHECK(IsImplThread());
  DCHECK(IsMainThreadBlocked());
  scheduler_->ClearHistory();
}

void ProxyImpl::SetRenderFrameObserver(
    std::unique_ptr<RenderFrameMetadataObserver> observer) {
  host_impl_->SetRenderFrameObserver(std::move(observer));
}

}  // namespace cc
