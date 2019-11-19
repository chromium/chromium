// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/single_thread_proxy.h"

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/trace_event/trace_event.h"
#include "cc/base/devtools_instrumentation.h"
#include "cc/benchmarks/benchmark_instrumentation.h"
#include "cc/input/browser_controls_offset_manager.h"
#include "cc/metrics/compositor_timing_history.h"
#include "cc/paint/paint_worklet_layer_painter.h"
#include "cc/resources/ui_resource_manager.h"
#include "cc/scheduler/commit_earlyout_reason.h"
#include "cc/scheduler/scheduler.h"
#include "cc/trees/latency_info_swap_promise.h"
#include "cc/trees/layer_tree_frame_sink.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_host_single_thread_client.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/mutator_host.h"
#include "cc/trees/render_frame_metadata_observer.h"
#include "cc/trees/scoped_abort_remaining_swap_promises.h"
#include "cc/trees/scroll_and_scale_set.h"
#include "components/viz/common/frame_sinks/delay_based_time_source.h"
#include "components/viz/common/frame_timing_details.h"
#include "components/viz/common/gpu/context_provider.h"

namespace cc {

std::unique_ptr<Proxy> SingleThreadProxy::Create(
    LayerTreeHost* layer_tree_host,
    LayerTreeHostSingleThreadClient* client,
    TaskRunnerProvider* task_runner_provider) {
  return base::WrapUnique(
      new SingleThreadProxy(layer_tree_host, client, task_runner_provider));
}

SingleThreadProxy::SingleThreadProxy(LayerTreeHost* layer_tree_host,
                                     LayerTreeHostSingleThreadClient* client,
                                     TaskRunnerProvider* task_runner_provider)
    : layer_tree_host_(layer_tree_host),
      single_thread_client_(client),
      task_runner_provider_(task_runner_provider),
      next_frame_is_newly_committed_frame_(false),
#if DCHECK_IS_ON()
      inside_impl_frame_(false),
#endif
      inside_draw_(false),
      defer_main_frame_update_(false),
      defer_commits_(false),
      animate_requested_(false),
      commit_requested_(false),
      inside_synchronous_composite_(false),
      needs_impl_frame_(false),
      layer_tree_frame_sink_creation_requested_(false),
      layer_tree_frame_sink_lost_(true) {
  TRACE_EVENT0("cc", "SingleThreadProxy::SingleThreadProxy");
  DCHECK(task_runner_provider_);
  DCHECK(task_runner_provider_->IsMainThread());
  DCHECK(layer_tree_host);
}

void SingleThreadProxy::Start() {
  DebugScopedSetImplThread impl(task_runner_provider_);

  const LayerTreeSettings& settings = layer_tree_host_->GetSettings();
  DCHECK(settings.single_thread_proxy_scheduler ||
         !settings.enable_checker_imaging)
      << "Checker-imaging is not supported in synchronous single threaded mode";
  host_impl_ = layer_tree_host_->CreateLayerTreeHostImpl(this);
  if (settings.single_thread_proxy_scheduler && !scheduler_on_impl_thread_) {
    SchedulerSettings scheduler_settings(settings.ToSchedulerSettings());
    scheduler_settings.commit_to_active_tree = true;

    std::unique_ptr<CompositorTimingHistory> compositor_timing_history(
        new CompositorTimingHistory(
            scheduler_settings.using_synchronous_renderer_compositor,
            CompositorTimingHistory::BROWSER_UMA,
            layer_tree_host_->rendering_stats_instrumentation(),
            host_impl_->compositor_frame_reporting_controller()));
    scheduler_on_impl_thread_.reset(
        new Scheduler(this, scheduler_settings, layer_tree_host_->GetId(),
                      task_runner_provider_->MainThreadTaskRunner(),
                      std::move(compositor_timing_history)));
  }
}

SingleThreadProxy::~SingleThreadProxy() {
  TRACE_EVENT0("cc", "SingleThreadProxy::~SingleThreadProxy");
  DCHECK(task_runner_provider_->IsMainThread());
  // Make sure Stop() got called or never Started.
  DCHECK(!host_impl_);
}

bool SingleThreadProxy::IsStarted() const {
  DCHECK(task_runner_provider_->IsMainThread());
  return !!host_impl_;
}

void SingleThreadProxy::SetVisible(bool visible) {
  TRACE_EVENT1("cc", "SingleThreadProxy::SetVisible", "visible", visible);
  DebugScopedSetImplThread impl(task_runner_provider_);

  host_impl_->SetVisible(visible);

  if (scheduler_on_impl_thread_)
    scheduler_on_impl_thread_->SetVisible(host_impl_->visible());
}

void SingleThreadProxy::RequestNewLayerTreeFrameSink() {
  DCHECK(task_runner_provider_->IsMainThread());
  layer_tree_frame_sink_creation_callback_.Cancel();
  if (layer_tree_frame_sink_creation_requested_)
    return;
  layer_tree_frame_sink_creation_requested_ = true;
  layer_tree_host_->RequestNewLayerTreeFrameSink();
}

void SingleThreadProxy::ReleaseLayerTreeFrameSink() {
  layer_tree_frame_sink_lost_ = true;
  frame_sink_bound_weak_factory_.InvalidateWeakPtrs();
  if (scheduler_on_impl_thread_)
    scheduler_on_impl_thread_->DidLoseLayerTreeFrameSink();
  return host_impl_->ReleaseLayerTreeFrameSink();
}

void SingleThreadProxy::SetLayerTreeFrameSink(
    LayerTreeFrameSink* layer_tree_frame_sink) {
  DCHECK(task_runner_provider_->IsMainThread());
  DCHECK(layer_tree_frame_sink_creation_requested_);

  bool success;
  {
    DebugScopedSetMainThreadBlocked main_thread_blocked(task_runner_provider_);
    DebugScopedSetImplThread impl(task_runner_provider_);
    success = host_impl_->InitializeFrameSink(layer_tree_frame_sink);
  }

  if (success) {
    frame_sink_bound_weak_ptr_ = frame_sink_bound_weak_factory_.GetWeakPtr();
    layer_tree_host_->DidInitializeLayerTreeFrameSink();
    if (scheduler_on_impl_thread_)
      scheduler_on_impl_thread_->DidCreateAndInitializeLayerTreeFrameSink();
    else if (!inside_synchronous_composite_)
      SetNeedsCommit();
    layer_tree_frame_sink_creation_requested_ = false;
    layer_tree_frame_sink_lost_ = false;
  } else {
    // DidFailToInitializeLayerTreeFrameSink is treated as a
    // RequestNewLayerTreeFrameSink, and so
    // layer_tree_frame_sink_creation_requested remains true.
    layer_tree_host_->DidFailToInitializeLayerTreeFrameSink();
  }
}

void SingleThreadProxy::SetNeedsAnimate() {
  TRACE_EVENT0("cc", "SingleThreadProxy::SetNeedsAnimate");
  DCHECK(task_runner_provider_->IsMainThread());
  if (animate_requested_)
    return;
  animate_requested_ = true;
  DebugScopedSetImplThread impl(task_runner_provider_);
  if (scheduler_on_impl_thread_)
    scheduler_on_impl_thread_->SetNeedsBeginMainFrame();
}

void SingleThreadProxy::SetNeedsUpdateLayers() {
  TRACE_EVENT0("cc", "SingleThreadProxy::SetNeedsUpdateLayers");
  DCHECK(task_runner_provider_->IsMainThread());
  SetNeedsCommit();
}

void SingleThreadProxy::DoCommit() {
  TRACE_EVENT0("cc", "SingleThreadProxy::DoCommit");
  DCHECK(task_runner_provider_->IsMainThread());

  layer_tree_host_->WillCommit();
  devtools_instrumentation::ScopedCommitTrace commit_task(
      layer_tree_host_->GetId());

  // Commit immediately.
  {
    DebugScopedSetMainThreadBlocked main_thread_blocked(task_runner_provider_);
    DebugScopedSetImplThread impl(task_runner_provider_);

    host_impl_->ReadyToCommit();
    host_impl_->BeginCommit();

    if (host_impl_->EvictedUIResourcesExist())
      layer_tree_host_->GetUIResourceManager()->RecreateUIResources();

    layer_tree_host_->FinishCommitOnImplThread(host_impl_.get());

    if (scheduler_on_impl_thread_) {
      scheduler_on_impl_thread_->DidCommit();
    }

    IssueImageDecodeFinishedCallbacks();
    host_impl_->CommitComplete();

    // Commit goes directly to the active tree, but we need to synchronously
    // "activate" the tree still during commit to satisfy any potential
    // SetNextCommitWaitsForActivation calls.  Unfortunately, the tree
    // might not be ready to draw, so DidActivateSyncTree must set
    // the flag to force the tree to not draw until textures are ready.
    NotifyReadyToActivate();
  }
}

void SingleThreadProxy::IssueImageDecodeFinishedCallbacks() {
  DCHECK(task_runner_provider_->IsImplThread());

  layer_tree_host_->ImageDecodesFinished(
      host_impl_->TakeCompletedImageDecodeRequests());
}

void SingleThreadProxy::CommitComplete() {
  // Commit complete happens on the main side after activate to satisfy any
  // SetNextCommitWaitsForActivation calls.
  DCHECK(!host_impl_->pending_tree())
      << "Activation is expected to have synchronously occurred by now.";

  DebugScopedSetMainThread main(task_runner_provider_);
  layer_tree_host_->CommitComplete();
  layer_tree_host_->DidBeginMainFrame();

  next_frame_is_newly_committed_frame_ = true;
}

void SingleThreadProxy::SetNeedsCommit() {
  DCHECK(task_runner_provider_->IsMainThread());
  single_thread_client_->RequestScheduleComposite();
  if (commit_requested_)
    return;
  commit_requested_ = true;
  DebugScopedSetImplThread impl(task_runner_provider_);
  if (scheduler_on_impl_thread_)
    scheduler_on_impl_thread_->SetNeedsBeginMainFrame();
}

void SingleThreadProxy::SetNeedsRedraw(const gfx::Rect& damage_rect) {
  TRACE_EVENT0("cc", "SingleThreadProxy::SetNeedsRedraw");
  DCHECK(task_runner_provider_->IsMainThread());
  DebugScopedSetImplThread impl(task_runner_provider_);
  host_impl_->SetViewportDamage(damage_rect);
  SetNeedsRedrawOnImplThread();
}

void SingleThreadProxy::SetNextCommitWaitsForActivation() {
  // Activation always forced in commit, so nothing to do.
  DCHECK(task_runner_provider_->IsMainThread());
}

bool SingleThreadProxy::RequestedAnimatePending() {
  return animate_requested_ || commit_requested_ || needs_impl_frame_;
}

void SingleThreadProxy::SetDeferMainFrameUpdate(bool defer_main_frame_update) {
  DCHECK(task_runner_provider_->IsMainThread());
  // Deferring main frame updates only makes sense if there's a scheduler.
  if (!scheduler_on_impl_thread_)
    return;
  if (defer_main_frame_update_ == defer_main_frame_update)
    return;

  if (defer_main_frame_update) {
    TRACE_EVENT_ASYNC_BEGIN0("cc", "SingleThreadProxy::SetDeferMainFrameUpdate",
                             this);
  } else {
    TRACE_EVENT_ASYNC_END0("cc", "SingleThreadProxy::SetDeferMainFrameUpdate",
                           this);
  }

  defer_main_frame_update_ = defer_main_frame_update;

  // Notify dependent systems that the deferral status has changed.
  layer_tree_host_->OnDeferMainFrameUpdatesChanged(defer_main_frame_update_);

  // The scheduler needs to know that it should not issue BeginMainFrame.
  scheduler_on_impl_thread_->SetDeferBeginMainFrame(defer_main_frame_update_);
}

void SingleThreadProxy::StartDeferringCommits(base::TimeDelta timeout) {
  DCHECK(task_runner_provider_->IsMainThread());

  // Do nothing if already deferring. The timeout remains as it was from when
  // we most recently began deferring.
  if (defer_commits_)
    return;

  TRACE_EVENT_ASYNC_BEGIN0("cc", "SingleThreadProxy::SetDeferCommits", this);

  defer_commits_ = true;
  commits_restart_time_ = base::TimeTicks::Now() + timeout;

  // Notify dependent systems that the deferral status has changed.
  layer_tree_host_->OnDeferCommitsChanged(defer_commits_);
}

void SingleThreadProxy::StopDeferringCommits(
    PaintHoldingCommitTrigger trigger) {
  if (!defer_commits_)
    return;
  defer_commits_ = false;
  commits_restart_time_ = base::TimeTicks();
  UMA_HISTOGRAM_ENUMERATION("PaintHolding.CommitTrigger2", trigger);
  TRACE_EVENT_ASYNC_END0("cc", "SingleThreadProxy::SetDeferCommits", this);

  // Notify dependent systems that the deferral status has changed.
  layer_tree_host_->OnDeferCommitsChanged(defer_commits_);
}

bool SingleThreadProxy::CommitRequested() const {
  DCHECK(task_runner_provider_->IsMainThread());
  return commit_requested_;
}

void SingleThreadProxy::Stop() {
  TRACE_EVENT0("cc", "SingleThreadProxy::stop");
  DCHECK(task_runner_provider_->IsMainThread());
  {
    DebugScopedSetMainThreadBlocked main_thread_blocked(task_runner_provider_);
    DebugScopedSetImplThread impl(task_runner_provider_);

    // Prevent the scheduler from performing actions while we're in an
    // inconsistent state.
    if (scheduler_on_impl_thread_)
      scheduler_on_impl_thread_->Stop();
    // Take away the LayerTreeFrameSink before destroying things so it doesn't
    // try to call into its client mid-shutdown.
    host_impl_->ReleaseLayerTreeFrameSink();

    // It is important to destroy LTHI before the Scheduler since it can make
    // callbacks that access it during destruction cleanup.
    host_impl_ = nullptr;
    scheduler_on_impl_thread_ = nullptr;
  }
  layer_tree_host_ = nullptr;
}

void SingleThreadProxy::SetMutator(std::unique_ptr<LayerTreeMutator> mutator) {
  DCHECK(task_runner_provider_->IsMainThread());
  DebugScopedSetImplThread impl(task_runner_provider_);
  host_impl_->SetLayerTreeMutator(std::move(mutator));
}

void SingleThreadProxy::SetPaintWorkletLayerPainter(
    std::unique_ptr<PaintWorkletLayerPainter> painter) {
  NOTREACHED();
}

void SingleThreadProxy::OnCanDrawStateChanged(bool can_draw) {
  TRACE_EVENT1("cc", "SingleThreadProxy::OnCanDrawStateChanged", "can_draw",
               can_draw);
  DCHECK(task_runner_provider_->IsImplThread());
  if (scheduler_on_impl_thread_)
    scheduler_on_impl_thread_->SetCanDraw(can_draw);
}

void SingleThreadProxy::NotifyReadyToActivate() {
  TRACE_EVENT0("cc", "SingleThreadProxy::NotifyReadyToActivate");
  DebugScopedSetImplThread impl(task_runner_provider_);
  if (scheduler_on_impl_thread_)
    scheduler_on_impl_thread_->NotifyReadyToActivate();
}

void SingleThreadProxy::NotifyReadyToDraw() {
  TRACE_EVENT0("cc", "SingleThreadProxy::NotifyReadyToDraw");
  DebugScopedSetImplThread impl(task_runner_provider_);
  if (scheduler_on_impl_thread_)
    scheduler_on_impl_thread_->NotifyReadyToDraw();
}

void SingleThreadProxy::SetNeedsRedrawOnImplThread() {
  single_thread_client_->RequestScheduleComposite();
  if (scheduler_on_impl_thread_)
    scheduler_on_impl_thread_->SetNeedsRedraw();
}

void SingleThreadProxy::SetNeedsOneBeginImplFrameOnImplThread() {
  TRACE_EVENT0("cc",
               "SingleThreadProxy::SetNeedsOneBeginImplFrameOnImplThread");
  single_thread_client_->RequestScheduleComposite();
  if (scheduler_on_impl_thread_)
    scheduler_on_impl_thread_->SetNeedsOneBeginImplFrame();
  needs_impl_frame_ = true;
}

void SingleThreadProxy::SetNeedsPrepareTilesOnImplThread() {
  TRACE_EVENT0("cc", "SingleThreadProxy::SetNeedsPrepareTilesOnImplThread");
  if (scheduler_on_impl_thread_)
    scheduler_on_impl_thread_->SetNeedsPrepareTiles();
}

void SingleThreadProxy::SetNeedsCommitOnImplThread() {
  single_thread_client_->RequestScheduleComposite();
  if (scheduler_on_impl_thread_)
    scheduler_on_impl_thread_->SetNeedsBeginMainFrame();
  commit_requested_ = true;
}

void SingleThreadProxy::SetVideoNeedsBeginFrames(bool needs_begin_frames) {
  TRACE_EVENT1("cc", "SingleThreadProxy::SetVideoNeedsBeginFrames",
               "needs_begin_frames", needs_begin_frames);
  // In tests the layer tree is destroyed after the scheduler is.
  if (scheduler_on_impl_thread_)
    scheduler_on_impl_thread_->SetVideoNeedsBeginFrames(needs_begin_frames);
}

void SingleThreadProxy::PostAnimationEventsToMainThreadOnImplThread(
    std::unique_ptr<MutatorEvents> events) {
  TRACE_EVENT0(
      "cc", "SingleThreadProxy::PostAnimationEventsToMainThreadOnImplThread");
  DCHECK(task_runner_provider_->IsImplThread());
  DebugScopedSetMainThread main(task_runner_provider_);
  layer_tree_host_->SetAnimationEvents(std::move(events));
}

size_t SingleThreadProxy::CompositedAnimationsCount() const {
  return 0;
}

size_t SingleThreadProxy::MainThreadAnimationsCount() const {
  return 0;
}

bool SingleThreadProxy::HasCustomPropertyAnimations() const {
  return false;
}

bool SingleThreadProxy::CurrentFrameHadRAF() const {
  return false;
}

bool SingleThreadProxy::NextFrameHasPendingRAF() const {
  return false;
}

bool SingleThreadProxy::IsInsideDraw() {
  return inside_draw_;
}

bool SingleThreadProxy::IsBeginMainFrameExpected() {
  return true;
}

void SingleThreadProxy::DidActivateSyncTree() {
  CommitComplete();
}

void SingleThreadProxy::WillPrepareTiles() {
  DCHECK(task_runner_provider_->IsImplThread());
  if (scheduler_on_impl_thread_)
    scheduler_on_impl_thread_->WillPrepareTiles();
}

void SingleThreadProxy::DidPrepareTiles() {
  DCHECK(task_runner_provider_->IsImplThread());
  if (scheduler_on_impl_thread_)
    scheduler_on_impl_thread_->DidPrepareTiles();
}

void SingleThreadProxy::DidCompletePageScaleAnimationOnImplThread() {
  layer_tree_host_->DidCompletePageScaleAnimation();
}

void SingleThreadProxy::DidLoseLayerTreeFrameSinkOnImplThread() {
  TRACE_EVENT0("cc",
               "SingleThreadProxy::DidLoseLayerTreeFrameSinkOnImplThread");
  {
    DebugScopedSetMainThread main(task_runner_provider_);
    // This must happen before we notify the scheduler as it may try to recreate
    // the output surface if already in BEGIN_IMPL_FRAME_STATE_IDLE.
    layer_tree_host_->DidLoseLayerTreeFrameSink();
  }
  single_thread_client_->DidLoseLayerTreeFrameSink();
  if (scheduler_on_impl_thread_)
    scheduler_on_impl_thread_->DidLoseLayerTreeFrameSink();
  layer_tree_frame_sink_lost_ = true;
}

void SingleThreadProxy::SetBeginFrameSource(viz::BeginFrameSource* source) {
  if (scheduler_on_impl_thread_)
    scheduler_on_impl_thread_->SetBeginFrameSource(source);
}

void SingleThreadProxy::DidReceiveCompositorFrameAckOnImplThread() {
  TRACE_EVENT0("cc,benchmark",
               "SingleThreadProxy::DidReceiveCompositorFrameAckOnImplThread");
  if (scheduler_on_impl_thread_)
    scheduler_on_impl_thread_->DidReceiveCompositorFrameAck();
  if (layer_tree_host_->GetSettings().send_compositor_frame_ack) {
    // We do a PostTask here because freeing resources in some cases (such as in
    // TextureLayer) is PostTasked and we want to make sure ack is received
    // after resources are returned.
    task_runner_provider_->MainThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(&SingleThreadProxy::DidReceiveCompositorFrameAck,
                       frame_sink_bound_weak_ptr_));
  }
}

void SingleThreadProxy::OnDrawForLayerTreeFrameSink(
    bool resourceless_software_draw,
    bool skip_draw) {
  NOTREACHED() << "Implemented by ThreadProxy for synchronous compositor.";
}

void SingleThreadProxy::NeedsImplSideInvalidation(
    bool needs_first_draw_on_activation) {
  if (scheduler_on_impl_thread_) {
    scheduler_on_impl_thread_->SetNeedsImplSideInvalidation(
        needs_first_draw_on_activation);
  }
}

void SingleThreadProxy::NotifyImageDecodeRequestFinished() {
  // If we don't have a scheduler, then just issue the callbacks here.
  // Otherwise, schedule a commit.
  if (!scheduler_on_impl_thread_) {
    DebugScopedSetMainThreadBlocked main_thread_blocked(task_runner_provider_);
    DebugScopedSetImplThread impl(task_runner_provider_);

    IssueImageDecodeFinishedCallbacks();
    return;
  }
  SetNeedsCommitOnImplThread();
}

void SingleThreadProxy::DidPresentCompositorFrameOnImplThread(
    uint32_t frame_token,
    std::vector<LayerTreeHost::PresentationTimeCallback> callbacks,
    const viz::FrameTimingDetails& details) {
  layer_tree_host_->DidPresentCompositorFrame(frame_token, std::move(callbacks),
                                              details.presentation_feedback);

  if (scheduler_on_impl_thread_) {
    scheduler_on_impl_thread_->DidPresentCompositorFrame(frame_token, details);
  }
}

void SingleThreadProxy::NotifyAnimationWorkletStateChange(
    AnimationWorkletMutationState state,
    ElementListType element_list_type) {
  layer_tree_host_->NotifyAnimationWorkletStateChange(state, element_list_type);
}

void SingleThreadProxy::NotifyPaintWorkletStateChange(
    Scheduler::PaintWorkletState state) {
  // Off-Thread PaintWorklet is only supported on the threaded compositor.
  NOTREACHED();
}

void SingleThreadProxy::RequestBeginMainFrameNotExpected(bool new_state) {
  if (scheduler_on_impl_thread_) {
    scheduler_on_impl_thread_->SetMainThreadWantsBeginMainFrameNotExpected(
        new_state);
  }
}

void SingleThreadProxy::CompositeImmediately(base::TimeTicks frame_begin_time,
                                             bool raster) {
  TRACE_EVENT0("cc,benchmark", "SingleThreadProxy::CompositeImmediately");
  DCHECK(task_runner_provider_->IsMainThread());
#if DCHECK_IS_ON()
  DCHECK(!inside_impl_frame_);
#endif
  base::AutoReset<bool> inside_composite(&inside_synchronous_composite_, true);

  if (layer_tree_frame_sink_lost_) {
    RequestNewLayerTreeFrameSink();
    // RequestNewLayerTreeFrameSink could have synchronously created an output
    // surface, so check again before returning.
    if (layer_tree_frame_sink_lost_)
      return;
  }

  viz::BeginFrameArgs begin_frame_args(viz::BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, viz::BeginFrameArgs::kManualSourceId,
      begin_frame_sequence_number_++, frame_begin_time, base::TimeTicks(),
      viz::BeginFrameArgs::DefaultInterval(), viz::BeginFrameArgs::NORMAL));

  // Start the impl frame.
  {
    DebugScopedSetImplThread impl(task_runner_provider_);
    WillBeginImplFrame(begin_frame_args);
  }

  // Run the "main thread" and get it to commit.
  {
#if DCHECK_IS_ON()
    DCHECK(inside_impl_frame_);
#endif
    animate_requested_ = false;
    needs_impl_frame_ = false;
    // Prevent new commits from being requested inside DoBeginMainFrame.
    // Note: We do not want to prevent SetNeedsAnimate from requesting
    // a commit here.
    commit_requested_ = true;
    DoBeginMainFrame(begin_frame_args);
    commit_requested_ = false;
    DoPainting();
    DoCommit();

    DCHECK_EQ(
        0u,
        layer_tree_host_->GetSwapPromiseManager()->num_queued_swap_promises())
        << "Commit should always succeed and transfer promises.";
  }

  // Finish the impl frame.
  {
    DebugScopedSetImplThread impl(task_runner_provider_);
    host_impl_->ActivateSyncTree();
    if (raster) {
      host_impl_->PrepareTiles();
      host_impl_->SynchronouslyInitializeAllTiles();
    }

    // TODO(danakj): Don't do this last... we prepared the wrong things. D:
    host_impl_->Animate();

    if (raster) {
      LayerTreeHostImpl::FrameData frame;
      frame.begin_frame_ack = viz::BeginFrameAck(begin_frame_args, true);
      frame.origin_begin_main_frame_args = begin_frame_args;
      DoComposite(&frame);
    }

    // DoComposite could abort, but because this is a synchronous composite
    // another draw will never be scheduled, so break remaining promises.
    host_impl_->active_tree()->BreakSwapPromises(SwapPromise::SWAP_FAILS);

    DidFinishImplFrame();
  }
}

bool SingleThreadProxy::SupportsImplScrolling() const {
  return false;
}

bool SingleThreadProxy::ShouldComposite() const {
  DCHECK(task_runner_provider_->IsImplThread());
  return host_impl_->visible() && host_impl_->CanDraw();
}

void SingleThreadProxy::ScheduleRequestNewLayerTreeFrameSink() {
  if (layer_tree_frame_sink_creation_callback_.IsCancelled() &&
      !layer_tree_frame_sink_creation_requested_) {
    layer_tree_frame_sink_creation_callback_.Reset(
        base::BindOnce(&SingleThreadProxy::RequestNewLayerTreeFrameSink,
                       weak_factory_.GetWeakPtr()));
    task_runner_provider_->MainThreadTaskRunner()->PostTask(
        FROM_HERE, layer_tree_frame_sink_creation_callback_.callback());
  }
}

DrawResult SingleThreadProxy::DoComposite(LayerTreeHostImpl::FrameData* frame) {
  TRACE_EVENT0("cc", "SingleThreadProxy::DoComposite");

  DrawResult draw_result;
  bool draw_frame;
  {
    DebugScopedSetImplThread impl(task_runner_provider_);
    base::AutoReset<bool> mark_inside(&inside_draw_, true);

    // We guard PrepareToDraw() with CanDraw() because it always returns a valid
    // frame, so can only be used when such a frame is possible. Since
    // DrawLayers() depends on the result of PrepareToDraw(), it is guarded on
    // CanDraw() as well.
    if (!ShouldComposite()) {
      return DRAW_ABORTED_CANT_DRAW;
    }

    // This CapturePostTasks should be destroyed before
    // DidCommitAndDrawFrame() is called since that goes out to the
    // embedder, and we want the embedder to receive its callbacks before that.
    // NOTE: This maintains consistent ordering with the ThreadProxy since
    // the DidCommitAndDrawFrame() must be post-tasked from the impl thread
    // there as the main thread is not blocked, so any posted tasks inside
    // the swap buffers will execute first.
    DebugScopedSetMainThreadBlocked main_thread_blocked(task_runner_provider_);

    draw_result = host_impl_->PrepareToDraw(frame);
    draw_frame = draw_result == DRAW_SUCCESS;
    if (draw_frame) {
      if (host_impl_->DrawLayers(frame)) {
        if (scheduler_on_impl_thread_) {
          // Drawing implies we submitted a frame to the LayerTreeFrameSink.
          scheduler_on_impl_thread_->DidSubmitCompositorFrame(
              frame->frame_token);
        }
        single_thread_client_->DidSubmitCompositorFrame();
      }
    }
    host_impl_->DidDrawAllLayers(*frame);

    bool start_ready_animations = draw_frame;
    host_impl_->UpdateAnimationState(start_ready_animations);
  }
  DidCommitAndDrawFrame();

  return draw_result;
}

void SingleThreadProxy::DidCommitAndDrawFrame() {
  if (next_frame_is_newly_committed_frame_) {
    DebugScopedSetMainThread main(task_runner_provider_);
    next_frame_is_newly_committed_frame_ = false;
    layer_tree_host_->DidCommitAndDrawFrame();
  }
}

bool SingleThreadProxy::MainFrameWillHappenForTesting() {
  if (!scheduler_on_impl_thread_)
    return false;
  return scheduler_on_impl_thread_->MainFrameForTestingWillHappen();
}

void SingleThreadProxy::ClearHistory() {
  DCHECK(task_runner_provider_->IsImplThread());
  if (scheduler_on_impl_thread_)
    scheduler_on_impl_thread_->ClearHistory();
}

void SingleThreadProxy::SetRenderFrameObserver(
    std::unique_ptr<RenderFrameMetadataObserver> observer) {
  host_impl_->SetRenderFrameObserver(std::move(observer));
}

void SingleThreadProxy::UpdateBrowserControlsState(
    BrowserControlsState constraints,
    BrowserControlsState current,
    bool animate) {
  host_impl_->browser_controls_manager()->UpdateBrowserControlsState(
      constraints, current, animate);
}

bool SingleThreadProxy::WillBeginImplFrame(const viz::BeginFrameArgs& args) {
  DebugScopedSetImplThread impl(task_runner_provider_);
#if DCHECK_IS_ON()
  DCHECK(!inside_impl_frame_)
      << "WillBeginImplFrame called while already inside an impl frame!";
  inside_impl_frame_ = true;
#endif
  return host_impl_->WillBeginImplFrame(args);
}

void SingleThreadProxy::ScheduledActionSendBeginMainFrame(
    const viz::BeginFrameArgs& begin_frame_args) {
  TRACE_EVENT0("cc", "SingleThreadProxy::ScheduledActionSendBeginMainFrame");
#if DCHECK_IS_ON()
  // Although this proxy is single-threaded, it's problematic to synchronously
  // have BeginMainFrame happen after ScheduledActionSendBeginMainFrame.  This
  // could cause a commit to occur in between a series of SetNeedsCommit calls
  // (i.e. property modifications) causing some to fall on one frame and some to
  // fall on the next.  Doing it asynchronously instead matches the semantics of
  // ThreadProxy::SetNeedsCommit where SetNeedsCommit will not cause a
  // synchronous commit.
  DCHECK(inside_impl_frame_)
      << "BeginMainFrame should only be sent inside a BeginImplFrame";
#endif

  host_impl_->WillSendBeginMainFrame();
  task_runner_provider_->MainThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&SingleThreadProxy::BeginMainFrame,
                                weak_factory_.GetWeakPtr(), begin_frame_args));
  host_impl_->DidSendBeginMainFrame(begin_frame_args);
}

void SingleThreadProxy::FrameIntervalUpdated(base::TimeDelta interval) {
  DebugScopedSetMainThread main(task_runner_provider_);
  single_thread_client_->FrameIntervalUpdated(interval);
}

void SingleThreadProxy::SendBeginMainFrameNotExpectedSoon() {
  layer_tree_host_->BeginMainFrameNotExpectedSoon();
}

void SingleThreadProxy::ScheduledActionBeginMainFrameNotExpectedUntil(
    base::TimeTicks time) {
  layer_tree_host_->BeginMainFrameNotExpectedUntil(time);
}

void SingleThreadProxy::BeginMainFrame(
    const viz::BeginFrameArgs& begin_frame_args) {
  // This checker assumes NotifyReadyToCommit in this stack causes a synchronous
  // commit.
  ScopedAbortRemainingSwapPromises swap_promise_checker(
      layer_tree_host_->GetSwapPromiseManager());

  if (scheduler_on_impl_thread_) {
    scheduler_on_impl_thread_->NotifyBeginMainFrameStarted(
        base::TimeTicks::Now());
  }

  commit_requested_ = false;
  needs_impl_frame_ = false;
  animate_requested_ = false;

  if (defer_main_frame_update_) {
    TRACE_EVENT_INSTANT0("cc", "EarlyOut_DeferBeginMainFrame",
                         TRACE_EVENT_SCOPE_THREAD);
    BeginMainFrameAbortedOnImplThread(
        CommitEarlyOutReason::ABORTED_DEFERRED_MAIN_FRAME_UPDATE);
    return;
  }

  if (!layer_tree_host_->IsVisible()) {
    TRACE_EVENT_INSTANT0("cc", "EarlyOut_NotVisible", TRACE_EVENT_SCOPE_THREAD);
    BeginMainFrameAbortedOnImplThread(
        CommitEarlyOutReason::ABORTED_NOT_VISIBLE);
    return;
  }

  // Prevent new commits from being requested inside DoBeginMainFrame.
  // Note: We do not want to prevent SetNeedsAnimate from requesting
  // a commit here.
  commit_requested_ = true;

  // Check now if we should stop deferring commits. Do this before
  // DoBeginMainFrame because the latter updates scroll offsets, which
  // we should avoid if deferring commits.
  if (defer_commits_ && base::TimeTicks::Now() > commits_restart_time_)
    StopDeferringCommits(PaintHoldingCommitTrigger::kTimeout);

  DoBeginMainFrame(begin_frame_args);

  // New commits requested inside UpdateLayers should be respected.
  commit_requested_ = false;

  // At this point the main frame may have deferred commits to avoid committing
  // right now.
  if (defer_main_frame_update_ || defer_commits_ ||
      begin_frame_args.animate_only) {
    TRACE_EVENT_INSTANT0("cc", "EarlyOut_DeferCommit_InsideBeginMainFrame",
                         TRACE_EVENT_SCOPE_THREAD);
    BeginMainFrameAbortedOnImplThread(
        CommitEarlyOutReason::ABORTED_DEFERRED_COMMIT);
    layer_tree_host_->DidBeginMainFrame();
    return;
  }

  DoPainting();
}

void SingleThreadProxy::DoBeginMainFrame(
    const viz::BeginFrameArgs& begin_frame_args) {
  // Only update scroll deltas if we are going to commit the frame, otherwise
  // scroll offsets get confused.
  if (!defer_commits_) {
    // The impl-side scroll deltas may be manipulated directly via the
    // InputHandler on the UI thread and the scale deltas may change when they
    // are clamped on the impl thread.
    std::unique_ptr<ScrollAndScaleSet> scroll_info =
        host_impl_->ProcessScrollDeltas();
    layer_tree_host_->ApplyScrollAndScale(scroll_info.get());
  }

  layer_tree_host_->WillBeginMainFrame();
  layer_tree_host_->BeginMainFrame(begin_frame_args);
  layer_tree_host_->AnimateLayers(begin_frame_args.frame_time);
  layer_tree_host_->RequestMainFrameUpdate(false /* record_cc_metrics */);
}

void SingleThreadProxy::DoPainting() {
  layer_tree_host_->UpdateLayers();

  // TODO(enne): SingleThreadProxy does not support cancelling commits yet,
  // search for CommitEarlyOutReason::FINISHED_NO_UPDATES inside
  // thread_proxy.cc
  if (scheduler_on_impl_thread_) {
    scheduler_on_impl_thread_->NotifyReadyToCommit(
        layer_tree_host_->begin_main_frame_metrics());
  }
}

void SingleThreadProxy::BeginMainFrameAbortedOnImplThread(
    CommitEarlyOutReason reason) {
  DebugScopedSetImplThread impl(task_runner_provider_);
  DCHECK(scheduler_on_impl_thread_->CommitPending());
  DCHECK(!host_impl_->pending_tree());

  std::vector<std::unique_ptr<SwapPromise>> empty_swap_promises;
  host_impl_->BeginMainFrameAborted(
      reason, std::move(empty_swap_promises),
      scheduler_on_impl_thread_->last_dispatched_begin_main_frame_args());
  scheduler_on_impl_thread_->BeginMainFrameAborted(reason);
}

DrawResult SingleThreadProxy::ScheduledActionDrawIfPossible() {
  DebugScopedSetImplThread impl(task_runner_provider_);
  LayerTreeHostImpl::FrameData frame;
  frame.begin_frame_ack =
      scheduler_on_impl_thread_->CurrentBeginFrameAckForActiveTree();
  frame.origin_begin_main_frame_args =
      scheduler_on_impl_thread_->last_activate_origin_frame_args();
  return DoComposite(&frame);
}

DrawResult SingleThreadProxy::ScheduledActionDrawForced() {
  NOTREACHED();
  return INVALID_RESULT;
}

void SingleThreadProxy::ScheduledActionCommit() {
  DebugScopedSetMainThread main(task_runner_provider_);
  DoCommit();
}

void SingleThreadProxy::ScheduledActionActivateSyncTree() {
  DebugScopedSetImplThread impl(task_runner_provider_);
  host_impl_->ActivateSyncTree();
}

void SingleThreadProxy::ScheduledActionBeginLayerTreeFrameSinkCreation() {
  DebugScopedSetMainThread main(task_runner_provider_);
  DCHECK(scheduler_on_impl_thread_);
  // If possible, create the output surface in a post task.  Synchronously
  // creating the output surface makes tests more awkward since this differs
  // from the ThreadProxy behavior.  However, sometimes there is no
  // task runner.
  if (task_runner_provider_->MainThreadTaskRunner()) {
    ScheduleRequestNewLayerTreeFrameSink();
  } else {
    RequestNewLayerTreeFrameSink();
  }
}

void SingleThreadProxy::ScheduledActionPrepareTiles() {
  TRACE_EVENT0("cc", "SingleThreadProxy::ScheduledActionPrepareTiles");
  DebugScopedSetImplThread impl(task_runner_provider_);
  host_impl_->PrepareTiles();
}

void SingleThreadProxy::ScheduledActionInvalidateLayerTreeFrameSink(
    bool needs_redraw) {
  // This is an Android WebView codepath, which only uses multi-thread
  // compositor. So this should not occur in single-thread mode.
  NOTREACHED() << "Android Webview use-case, so multi-thread only";
}

void SingleThreadProxy::ScheduledActionPerformImplSideInvalidation() {
  DCHECK(scheduler_on_impl_thread_);

  DebugScopedSetImplThread impl(task_runner_provider_);
  host_impl_->InvalidateContentOnImplSide();

  // Invalidations go directly to the active tree, so we synchronously call
  // NotifyReadyToActivate to update the scheduler and LTHI state correctly.
  // Since in single-threaded mode the scheduler will wait for a ready to draw
  // signal from LTHI, the draw will remain blocked till the invalidated tiles
  // are ready.
  NotifyReadyToActivate();
}

void SingleThreadProxy::DidFinishImplFrame() {
  host_impl_->DidFinishImplFrame();
#if DCHECK_IS_ON()
  DCHECK(inside_impl_frame_)
      << "DidFinishImplFrame called while not inside an impl frame!";
  inside_impl_frame_ = false;
#endif
}

void SingleThreadProxy::DidNotProduceFrame(const viz::BeginFrameAck& ack) {
  DebugScopedSetImplThread impl(task_runner_provider_);
  host_impl_->DidNotProduceFrame(ack);
}

void SingleThreadProxy::WillNotReceiveBeginFrame() {
  host_impl_->DidNotNeedBeginFrame();
}

void SingleThreadProxy::DidReceiveCompositorFrameAck() {
  layer_tree_host_->DidReceiveCompositorFrameAck();
}

}  // namespace cc
