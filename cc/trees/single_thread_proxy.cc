// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/single_thread_proxy.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/auto_reset.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/synchronization/waitable_event.h"
#include "base/trace_event/trace_event.h"
#include "base/types/optional_ref.h"
#include "build/chromeos_buildflags.h"
#include "cc/base/completion_event.h"
#include "cc/base/devtools_instrumentation.h"
#include "cc/benchmarks/benchmark_instrumentation.h"
#include "cc/input/browser_controls_offset_manager.h"
#include "cc/input/browser_controls_offset_tags_info.h"
#include "cc/metrics/compositor_timing_history.h"
#include "cc/paint/paint_worklet_layer_painter.h"
#include "cc/resources/ui_resource_manager.h"
#include "cc/scheduler/commit_earlyout_reason.h"
#include "cc/scheduler/scheduler.h"
#include "cc/trees/compositor_commit_data.h"
#include "cc/trees/latency_info_swap_promise.h"
#include "cc/trees/layer_tree_frame_sink.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_host_single_thread_client.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/mutator_host.h"
#include "cc/trees/paint_holding_reason.h"
#include "cc/trees/render_frame_metadata_observer.h"
#include "cc/trees/scoped_abort_remaining_swap_promises.h"
#include "components/viz/common/frame_sinks/delay_based_time_source.h"
#include "components/viz/common/frame_timing_details.h"
#include "components/viz/common/gpu/raster_context_provider.h"

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
      pause_rendering_(false),
      animate_requested_(false),
      update_layers_requested_(false),
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
  DCHECK(task_runner_provider_->IsMainThread());

  const LayerTreeSettings& settings = layer_tree_host_->GetSettings();
  DCHECK(settings.single_thread_proxy_scheduler ||
         !settings.enable_checker_imaging)
      << "Checker-imaging is not supported in synchronous single threaded mode";
  {
    DebugScopedSetMainThreadBlocked main_thread_blocked(task_runner_provider_);
    DebugScopedSetImplThread impl(task_runner_provider_);
    host_impl_ = layer_tree_host_->CreateLayerTreeHostImpl(this);
  }
  if (settings.single_thread_proxy_scheduler && !scheduler_on_impl_thread_) {
    SchedulerSettings scheduler_settings(settings.ToSchedulerSettings());
    scheduler_settings.commit_to_active_tree = true;

    std::unique_ptr<CompositorTimingHistory> compositor_timing_history(
        new CompositorTimingHistory(
            CompositorTimingHistory::BROWSER_UMA,
            layer_tree_host_->rendering_stats_instrumentation()));
    scheduler_on_impl_thread_ = std::make_unique<Scheduler>(
        this, scheduler_settings, layer_tree_host_->GetId(),
        task_runner_provider_->MainThreadTaskRunner(),
        std::move(compositor_timing_history),
        host_impl_->compositor_frame_reporting_controller());
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
  DCHECK(task_runner_provider_->IsMainThread());
  TRACE_EVENT1("cc", "SingleThreadProxy::SetVisible", "visible", visible);
  DebugScopedSetImplThread impl(task_runner_provider_);
  host_impl_->SetVisible(visible);
  if (scheduler_on_impl_thread_)
    scheduler_on_impl_thread_->SetVisible(host_impl_->visible());
}

void SingleThreadProxy::SetShouldWarmUp() {
  DCHECK(task_runner_provider_->IsMainThread());
  DebugScopedSetImplThread impl(task_runner_provider_);
  if (scheduler_on_impl_thread_) {
    scheduler_on_impl_thread_->SetShouldWarmUp();
  }
}

void SingleThreadProxy::RequestNewLayerTreeFrameSink() {
  DCHECK(task_runner_provider_->IsMainThread());
  layer_tree_frame_sink_creation_callback_.Cancel();
  if (layer_tree_frame_sink_creation_requested_)
    return;
  layer_tree_frame_sink_creation_requested_ = true;
  layer_tree_host_->RequestNewLayerTreeFrameSink();
}

void SingleThreadProxy::DidObserveFirstScrollDelay(
    int source_frame_number,
    base::TimeDelta first_scroll_delay,
    base::TimeTicks first_scroll_timestamp) {
  DCHECK(!task_runner_provider_->HasImplThread() ||
         task_runner_provider_->IsImplThread());
}

void SingleThreadProxy::ReleaseLayerTreeFrameSink() {
  DCHECK(task_runner_provider_->IsMainThread());
  layer_tree_frame_sink_lost_ = true;
  frame_sink_bound_weak_factory_.InvalidateWeakPtrs();
  DebugScopedSetImplThread impl(task_runner_provider_);
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
    if (scheduler_on_impl_thread_) {
      DebugScopedSetImplThread impl(task_runner_provider_);
      scheduler_on_impl_thread_->DidCreateAndInitializeLayerTreeFrameSink();
    } else if (!inside_synchronous_composite_) {
      SetNeedsCommit();
    }
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
  layer_tree_host_->OnCommitRequested();
}

void SingleThreadProxy::SetNeedsUpdateLayers() {
  TRACE_EVENT0("cc", "SingleThreadProxy::SetNeedsUpdateLayers");
  DCHECK(task_runner_provider_->IsMainThread());
  if (!RequestedAnimatePending()) {
    DebugScopedSetImplThread impl(task_runner_provider_);
    if (scheduler_on_impl_thread_)
      scheduler_on_impl_thread_->SetNeedsBeginMainFrame();
  }
  update_layers_requested_ = true;
}

void SingleThreadProxy::DoCommit(const viz::BeginFrameArgs& commit_args) {
  TRACE_EVENT0("cc", "SingleThreadProxy::DoCommit");
  DCHECK(task_runner_provider_->IsMainThread());
  CHECK_EQ(source_frame_number_for_next_commit_, kInvalidSourceFrameNumber);

  IssueImageDecodeFinishedCallbacks();

  if (host_impl_->EvictedUIResourcesExist())
    layer_tree_host_->GetUIResourceManager()->RecreateUIResources();

  // Strictly speaking, it's not necessary to pass a CompletionEvent to
  // WillCommit, since we can't have thread contention issues. The benefit to
  // creating one here is that it simplifies LayerTreeHost::in_commit(), which
  // is useful in DCHECKs sprinkled throughout the code.
  auto completion_event_ptr = std::make_unique<CompletionEvent>(
      base::WaitableEvent::ResetPolicy::MANUAL);
  auto* completion_event = completion_event_ptr.get();
  // Must get unsafe_state before calling WillCommit() to avoid deadlock.
  auto& unsafe_state = layer_tree_host_->GetUnsafeStateForCommit();
  std::unique_ptr<CommitState> commit_state =
      layer_tree_host_->WillCommit(std::move(completion_event_ptr),
                                   /*has_updates=*/true);
  DCHECK(commit_state.get());
  devtools_instrumentation::ScopedCommitTrace commit_task(
      layer_tree_host_->GetId(), commit_args.frame_id.sequence_number);

  // Commit immediately.
  DebugScopedSetMainThreadBlocked main_thread_blocked(task_runner_provider_);
  DebugScopedSetImplThread impl(task_runner_provider_);

  source_frame_number_for_next_commit_ = commit_state->source_frame_number;
  host_impl_->BeginCommit(commit_state->source_frame_number,
                          commit_state->trace_id);

  host_impl_->FinishCommit(*commit_state, unsafe_state);
  commit_state.reset();
  completion_event->Signal();

  {
    DebugScopedSetMainThread main(task_runner_provider_);
    IssueImageDecodeFinishedCallbacks();
  }
}

void SingleThreadProxy::DoPostCommit() {
  TRACE_EVENT0("cc", "SingleThreadProxy::DoPostCommit");
  DCHECK(task_runner_provider_->IsMainThread());

  DebugScopedSetImplThread impl(task_runner_provider_);
  host_impl_->CommitComplete();

  // Commit goes directly to the active tree, but we need to synchronously
  // "activate" the tree still during commit to satisfy any potential
  // SetNextCommitWaitsForActivation calls.  Unfortunately, the tree
  // might not be ready to draw, so DidActivateSyncTree must set
  // the flag to force the tree to not draw until textures are ready.
  NotifyReadyToActivate();
}

void SingleThreadProxy::IssueImageDecodeFinishedCallbacks() {
  DCHECK(task_runner_provider_->IsMainThread());

  layer_tree_host_->ImageDecodesFinished(
      host_impl_->TakeCompletedImageDecodeRequests());
}

void SingleThreadProxy::CommitComplete() {
  // Commit complete happens on the main side after activate to satisfy any
  // SetNextCommitWaitsForActivation calls.
  DCHECK(task_runner_provider_->IsImplThread());
  DCHECK(!host_impl_->pending_tree())
      << "Activation is expected to have synchronously occurred by now.";
  CHECK_NE(source_frame_number_for_next_commit_, kInvalidSourceFrameNumber);

  DebugScopedSetMainThread main(task_runner_provider_);
  layer_tree_host_->DidBeginMainFrame();
  layer_tree_host_->CommitComplete(source_frame_number_for_next_commit_,
                                   {base::TimeTicks(), base::TimeTicks::Now()});
  source_frame_number_for_next_commit_ = kInvalidSourceFrameNumber;

  next_frame_is_newly_committed_frame_ = true;
}

void SingleThreadProxy::SetNeedsCommit() {
  DCHECK(task_runner_provider_->IsMainThread());
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

void SingleThreadProxy::SetTargetLocalSurfaceId(
    const viz::LocalSurfaceId& target_local_surface_id) {
  DCHECK(task_runner_provider_->IsMainThread());
  if (!scheduler_on_impl_thread_)
    return;
  DebugScopedSetImplThread impl(task_runner_provider_);
  host_impl_->SetTargetLocalSurfaceId(target_local_surface_id);
}

void SingleThreadProxy::DetachInputDelegateAndRenderFrameObserver() {
  DCHECK(task_runner_provider_->IsMainThread());

  DebugScopedSetImplThread impl(task_runner_provider_);
  host_impl_->DetachInputDelegateAndRenderFrameObserver();
}

bool SingleThreadProxy::RequestedAnimatePending() {
  DCHECK(task_runner_provider_->IsMainThread());
  return animate_requested_ || update_layers_requested_ || commit_requested_ ||
         needs_impl_frame_;
}

void SingleThreadProxy::SetDeferMainFrameUpdate(bool defer_main_frame_update) {
  DCHECK(task_runner_provider_->IsMainThread());
  // Deferring main frame updates only makes sense if there's a scheduler.
  if (!scheduler_on_impl_thread_)
    return;
  if (defer_main_frame_update_ == defer_main_frame_update)
    return;

  if (defer_main_frame_update) {
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN0(
        "cc", "SingleThreadProxy::SetDeferMainFrameUpdate",
        TRACE_ID_LOCAL(this));
  } else {
    TRACE_EVENT_NESTABLE_ASYNC_END0(
        "cc", "SingleThreadProxy::SetDeferMainFrameUpdate",
        TRACE_ID_LOCAL(this));
  }

  defer_main_frame_update_ = defer_main_frame_update;

  // Notify dependent systems that the deferral status has changed.
  layer_tree_host_->OnDeferMainFrameUpdatesChanged(defer_main_frame_update_);

  // The scheduler needs to know that it should not issue BeginMainFrame.
  DebugScopedSetImplThread impl(task_runner_provider_);
  scheduler_on_impl_thread_->SetDeferBeginMainFrame(defer_main_frame_update_);
}

void SingleThreadProxy::SetPauseRendering(bool pause_rendering) {
  DCHECK(task_runner_provider_->IsMainThread());
  // Pause updates only makes sense if there's a scheduler. In synchronous mode,
  // the client controls when a frame is produced.
  if (!scheduler_on_impl_thread_)
    return;
  if (pause_rendering_ == pause_rendering)
    return;

  pause_rendering_ = pause_rendering;
  if (pause_rendering_) {
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN0(
        "cc", "SingleThreadProxy::SetPauseRendering", TRACE_ID_LOCAL(this));
  } else {
    TRACE_EVENT_NESTABLE_ASYNC_END0(
        "cc", "SingleThreadProxy::SetPauseRendering", TRACE_ID_LOCAL(this));
  }

  // The scheduler needs to know that it should not issue BeginFrame.
  DebugScopedSetImplThread impl(task_runner_provider_);
  scheduler_on_impl_thread_->SetPauseRendering(pause_rendering_);
}

void SingleThreadProxy::SetInputResponsePending() {}

bool SingleThreadProxy::StartDeferringCommits(base::TimeDelta timeout,
                                              PaintHoldingReason reason) {
  DCHECK(task_runner_provider_->IsMainThread());

  // Do nothing if already deferring. The timeout remains as it was from when
  // we most recently began deferring.
  if (IsDeferringCommits())
    return false;

  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("cc", "SingleThreadProxy::SetDeferCommits",
                                    TRACE_ID_LOCAL(this));

  paint_holding_reason_ = reason;
  commits_restart_time_ = base::TimeTicks::Now() + timeout;

  // Notify dependent systems that the deferral status has changed.
  layer_tree_host_->OnDeferCommitsChanged(true, reason, std::nullopt);
  return true;
}

void SingleThreadProxy::StopDeferringCommits(
    PaintHoldingCommitTrigger trigger) {
  DCHECK(task_runner_provider_->IsMainThread());
  if (!IsDeferringCommits())
    return;
  auto reason = *paint_holding_reason_;
  paint_holding_reason_.reset();
  commits_restart_time_ = base::TimeTicks();
  UMA_HISTOGRAM_ENUMERATION("PaintHolding.CommitTrigger2", trigger);
  TRACE_EVENT_NESTABLE_ASYNC_END0("cc", "SingleThreadProxy::SetDeferCommits",
                                  TRACE_ID_LOCAL(this));

  // Notify dependent systems that the deferral status has changed.
  layer_tree_host_->OnDeferCommitsChanged(false, reason, trigger);
}

bool SingleThreadProxy::IsDeferringCommits() const {
  DCHECK(task_runner_provider_->IsMainThread());
  return paint_holding_reason_.has_value();
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

void SingleThreadProxy::QueueImageDecode(int request_id,
                                         const PaintImage& image) {
  DCHECK(task_runner_provider_->IsMainThread());
  DebugScopedSetImplThread impl(task_runner_provider_);
  host_impl_->QueueImageDecode(request_id, image);
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
  DCHECK(!task_runner_provider_->HasImplThread() ||
         task_runner_provider_->IsImplThread());
  TRACE_EVENT1("cc", "SingleThreadProxy::OnCanDrawStateChanged", "can_draw",
               can_draw);
  if (scheduler_on_impl_thread_)
    scheduler_on_impl_thread_->SetCanDraw(can_draw);
}

void SingleThreadProxy::NotifyReadyToActivate() {
  DCHECK(!task_runner_provider_->HasImplThread() ||
         task_runner_provider_->IsImplThread());
  TRACE_EVENT0("cc", "SingleThreadProxy::NotifyReadyToActivate");
  if (scheduler_on_impl_thread_)
    scheduler_on_impl_thread_->NotifyReadyToActivate();
}

bool SingleThreadProxy::IsReadyToActivate() {
  DCHECK(!task_runner_provider_->HasImplThread() ||
         task_runner_provider_->IsImplThread());
  return scheduler_on_impl_thread_ &&
         scheduler_on_impl_thread_->IsReadyToActivate();
}

void SingleThreadProxy::NotifyReadyToDraw() {
  DCHECK(!task_runner_provider_->HasImplThread() ||
         task_runner_provider_->IsImplThread());
  TRACE_EVENT0("cc", "SingleThreadProxy::NotifyReadyToDraw");
  if (scheduler_on_impl_thread_)
    scheduler_on_impl_thread_->NotifyReadyToDraw();
}

void SingleThreadProxy::SetNeedsRedrawOnImplThread() {
  DCHECK(!task_runner_provider_->HasImplThread() ||
         task_runner_provider_->IsImplThread());
  if (scheduler_on_impl_thread_) {
    scheduler_on_impl_thread_->SetNeedsRedraw();
  }
}

void SingleThreadProxy::SetNeedsOneBeginImplFrameOnImplThread() {
  DCHECK(!task_runner_provider_->HasImplThread() ||
         task_runner_provider_->IsImplThread());
  TRACE_EVENT0("cc",
               "SingleThreadProxy::SetNeedsOneBeginImplFrameOnImplThread");
  single_thread_client_->ScheduleAnimationForWebTests();
  if (scheduler_on_impl_thread_)
    scheduler_on_impl_thread_->SetNeedsOneBeginImplFrame();
  needs_impl_frame_ = true;
}

void SingleThreadProxy::SetNeedsPrepareTilesOnImplThread() {
  DCHECK(!task_runner_provider_->HasImplThread() ||
         task_runner_provider_->IsImplThread());
  TRACE_EVENT0("cc", "SingleThreadProxy::SetNeedsPrepareTilesOnImplThread");
  if (scheduler_on_impl_thread_)
    scheduler_on_impl_thread_->SetNeedsPrepareTiles();
}

void SingleThreadProxy::SetNeedsCommitOnImplThread() {
  DCHECK(!task_runner_provider_->HasImplThread() ||
         task_runner_provider_->IsImplThread());
  single_thread_client_->ScheduleAnimationForWebTests();
  if (scheduler_on_impl_thread_)
    scheduler_on_impl_thread_->SetNeedsBeginMainFrame();
  commit_requested_ = true;
}

void SingleThreadProxy::SetVideoNeedsBeginFrames(bool needs_begin_frames) {
  DCHECK(!task_runner_provider_->HasImplThread() ||
         task_runner_provider_->IsImplThread());
  TRACE_EVENT1("cc", "SingleThreadProxy::SetVideoNeedsBeginFrames",
               "needs_begin_frames", needs_begin_frames);
  // In tests the layer tree is destroyed after the scheduler is.
  if (scheduler_on_impl_thread_)
    scheduler_on_impl_thread_->SetVideoNeedsBeginFrames(needs_begin_frames);
}

bool SingleThreadProxy::IsInsideDraw() {
  DCHECK(!task_runner_provider_->HasImplThread() ||
         task_runner_provider_->IsImplThread());
  return inside_draw_;
}

void SingleThreadProxy::RenewTreePriority() {
  DCHECK(!task_runner_provider_->HasImplThread() ||
         task_runner_provider_->IsImplThread());
}

void SingleThreadProxy::PostDelayedAnimationTaskOnImplThread(
    base::OnceClosure task,
    base::TimeDelta delay) {
  DCHECK(!task_runner_provider_->HasImplThread() ||
         task_runner_provider_->IsImplThread());
}

void SingleThreadProxy::DidActivateSyncTree() {
  DCHECK(!task_runner_provider_->HasImplThread() ||
         task_runner_provider_->IsImplThread());
  CommitComplete();
}

void SingleThreadProxy::DidPrepareTiles() {
  DCHECK(!task_runner_provider_->HasImplThread() ||
         task_runner_provider_->IsImplThread());
  if (scheduler_on_impl_thread_)
    scheduler_on_impl_thread_->DidPrepareTiles();
}

void SingleThreadProxy::DidCompletePageScaleAnimationOnImplThread() {
  DCHECK(!task_runner_provider_->HasImplThread() ||
         task_runner_provider_->IsImplThread());
  DebugScopedSetMainThread main(task_runner_provider_);
  layer_tree_host_->DidCompletePageScaleAnimation();
}

void SingleThreadProxy::DidLoseLayerTreeFrameSinkOnImplThread() {
  DCHECK(!task_runner_provider_->HasImplThread() ||
         task_runner_provider_->IsImplThread());
  TRACE_EVENT0("cc",
               "SingleThreadProxy::DidLoseLayerTreeFrameSinkOnImplThread");
  {
    DebugScopedSetMainThread main(task_runner_provider_);
    // This must happen before we notify the scheduler as it may try to recreate
    // the output surface if already in BEGIN_IMPL_FRAME_STATE_IDLE.
    layer_tree_host_->DidLoseLayerTreeFrameSink();
    single_thread_client_->DidLoseLayerTreeFrameSink();
  }
  if (scheduler_on_impl_thread_)
    scheduler_on_impl_thread_->DidLoseLayerTreeFrameSink();
  layer_tree_frame_sink_lost_ = true;
}

void SingleThreadProxy::SetBeginFrameSource(viz::BeginFrameSource* source) {
  DCHECK(!task_runner_provider_->HasImplThread() ||
         task_runner_provider_->IsImplThread());
  if (scheduler_on_impl_thread_)
    scheduler_on_impl_thread_->SetBeginFrameSource(source);
}

void SingleThreadProxy::DidReceiveCompositorFrameAckOnImplThread() {
  DCHECK(!task_runner_provider_->HasImplThread() ||
         task_runner_provider_->IsImplThread());
  TRACE_EVENT0("cc,benchmark",
               "SingleThreadProxy::DidReceiveCompositorFrameAckOnImplThread");
  if (scheduler_on_impl_thread_)
    scheduler_on_impl_thread_->DidReceiveCompositorFrameAck();

  // We do a PostTask here because freeing resources in some cases (such as in
  // TextureLayer) is PostTasked and we want to make sure ack is received
  // after resources are returned.
  task_runner_provider_->MainThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&SingleThreadProxy::DidReceiveCompositorFrameAck,
                     frame_sink_bound_weak_ptr_));
}

void SingleThreadProxy::OnDrawForLayerTreeFrameSink(
    bool resourceless_software_draw,
    bool skip_draw) {
  NOTREACHED() << "Implemented by ThreadProxy for synchronous compositor.";
}

void SingleThreadProxy::SetNeedsImplSideInvalidation(
    bool needs_first_draw_on_activation) {
  DCHECK(!task_runner_provider_->HasImplThread() ||
         task_runner_provider_->IsImplThread());
  if (scheduler_on_impl_thread_) {
    scheduler_on_impl_thread_->SetNeedsImplSideInvalidation(
        needs_first_draw_on_activation);
  }
}

void SingleThreadProxy::NotifyImageDecodeRequestFinished(
    int request_id,
    bool decode_succeeded) {
  DCHECK(!task_runner_provider_->HasImplThread() ||
         task_runner_provider_->IsImplThread());
  if (base::FeatureList::IsEnabled(
          features::kSendExplicitDecodeRequestsImmediately)) {
    DebugScopedSetMainThread main_thread(task_runner_provider_);
    layer_tree_host_->NotifyImageDecodeFinished(request_id, decode_succeeded);
  } else {
    // If we don't have a scheduler, then just issue the callbacks here.
    // Otherwise, schedule a commit.
    if (!scheduler_on_impl_thread_) {
      DebugScopedSetMainThread main_thread(task_runner_provider_);
      IssueImageDecodeFinishedCallbacks();
    } else {
      SetNeedsCommitOnImplThread();
    }
  }
}

void SingleThreadProxy::NotifyTransitionRequestFinished(uint32_t sequence_id) {
  DCHECK(!task_runner_provider_->HasImplThread() ||
         task_runner_provider_->IsImplThread());

  DebugScopedSetMainThread main_thread(task_runner_provider_);
  layer_tree_host_->NotifyTransitionRequestsFinished({sequence_id});
}

void SingleThreadProxy::DidPresentCompositorFrameOnImplThread(
    uint32_t frame_token,
    PresentationTimeCallbackBuffer::PendingCallbacks callbacks,
    const viz::FrameTimingDetails& details) {
  DCHECK(!task_runner_provider_->HasImplThread() ||
         task_runner_provider_->IsImplThread());
  host_impl_->NotifyDidPresentCompositorFrameOnImplThread(
      frame_token, std::move(callbacks.compositor_successful_callbacks),
      details);
  {
    DebugScopedSetMainThread main(task_runner_provider_);
    layer_tree_host_->DidPresentCompositorFrame(
        frame_token, std::move(callbacks.main_callbacks),
        std::move(callbacks.main_successful_callbacks), details);
  }
  if (scheduler_on_impl_thread_) {
    scheduler_on_impl_thread_->DidPresentCompositorFrame(frame_token, details);
  }
}

void SingleThreadProxy::NotifyAnimationWorkletStateChange(
    AnimationWorkletMutationState state,
    ElementListType element_list_type) {
  DCHECK(!task_runner_provider_->HasImplThread() ||
         task_runner_provider_->IsImplThread());
  DebugScopedSetMainThread main(task_runner_provider_);
  layer_tree_host_->NotifyAnimationWorkletStateChange(state, element_list_type);
}

void SingleThreadProxy::NotifyPaintWorkletStateChange(
    Scheduler::PaintWorkletState state) {
  // Off-Thread PaintWorklet is only supported on the threaded compositor.
  NOTREACHED();
}

void SingleThreadProxy::NotifyThroughputTrackerResults(
    CustomTrackerResults results) {
  DCHECK(!task_runner_provider_->HasImplThread() ||
         task_runner_provider_->IsImplThread());
  // This method is called from ImplThread side to report after being requested,
  // or from the MainThread when releasing FrameSequenceTrackers during
  // destruction. Regardless, `layer_tree_host_` should be accessed from
  // MainThread side.
  DebugScopedSetMainThread main(task_runner_provider_);
  layer_tree_host_->NotifyThroughputTrackerResults(std::move(results));
}

bool SingleThreadProxy::IsInSynchronousComposite() const {
  DCHECK(!task_runner_provider_->HasImplThread() ||
         task_runner_provider_->IsImplThread());
  return inside_synchronous_composite_;
}

void SingleThreadProxy::FrameSinksToThrottleUpdated(
    const base::flat_set<viz::FrameSinkId>& ids) {
  DCHECK(!task_runner_provider_->HasImplThread() ||
         task_runner_provider_->IsImplThread());
  single_thread_client_->FrameSinksToThrottleUpdated(ids);
}

void SingleThreadProxy::RequestBeginMainFrameNotExpected(bool new_state) {
  DCHECK(task_runner_provider_->IsMainThread());
  if (scheduler_on_impl_thread_) {
    DebugScopedSetImplThread impl(task_runner_provider_);
    scheduler_on_impl_thread_->SetMainThreadWantsBeginMainFrameNotExpected(
        new_state);
  }
}

viz::BeginFrameArgs SingleThreadProxy::BeginImplFrameForTest(
    base::TimeTicks frame_begin_time) {
  viz::BeginFrameArgs begin_frame_args(viz::BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, viz::BeginFrameArgs::kManualSourceId,
      begin_frame_sequence_number_++, frame_begin_time, base::TimeTicks(),
      viz::BeginFrameArgs::DefaultInterval(), viz::BeginFrameArgs::NORMAL));

  // Start the impl frame.
  {
    DebugScopedSetImplThread impl(task_runner_provider_);
    WillBeginImplFrame(begin_frame_args);
  }
  return begin_frame_args;
}

void SingleThreadProxy::CompositeImmediatelyForTest(
    base::TimeTicks frame_begin_time,
    bool raster,
    base::OnceClosure callback) {
  TRACE_EVENT0("cc,benchmark",
               "SingleThreadProxy::CompositeImmediatelyForTest");
  DCHECK(task_runner_provider_->IsMainThread());
#if DCHECK_IS_ON()
  DCHECK(!inside_impl_frame_);
#endif
  base::AutoReset<bool> inside_composite(&inside_synchronous_composite_, true);

  if (layer_tree_frame_sink_lost_) {
    auto sync = layer_tree_host_->ForceSyncCompositeForTest();  // IN-TEST
    RequestNewLayerTreeFrameSink();
    // RequestNewLayerTreeFrameSink could have synchronously created an output
    // surface, so check again before returning.
    if (layer_tree_frame_sink_lost_) {
      if (callback) {
        std::move(callback).Run();
      }
      return;
    }
  }

  viz::BeginFrameArgs begin_frame_args =
      BeginImplFrameForTest(frame_begin_time);  // IN-TEST

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
    StopDeferringCommits(PaintHoldingCommitTrigger::kFeatureDisabled);
    layer_tree_host_->RecordStartOfFrameMetrics();
    DoBeginMainFrame(begin_frame_args);
    commit_requested_ = false;
    DoPainting(begin_frame_args);
    layer_tree_host_->RecordEndOfFrameMetrics(frame_begin_time,
                                              /* trackers */ 0u);
    DoCommit(begin_frame_args);
    DoPostCommit();

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

    DidFinishImplFrame(begin_frame_args);
  }
  if (callback) {
    std::move(callback).Run();
  }
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
      return DrawResult::kAbortedCantDraw;
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
    draw_frame = draw_result == DrawResult::kSuccess;
    if (draw_frame) {
      if (std::optional<SubmitInfo> submit_info =
              host_impl_->DrawLayers(frame)) {
        if (scheduler_on_impl_thread_) {
          // Drawing implies we submitted a frame to the LayerTreeFrameSink.
          scheduler_on_impl_thread_->DidSubmitCompositorFrame(
              submit_info.value());
        }
        single_thread_client_->DidSubmitCompositorFrame();
      }
    }
    host_impl_->DidDrawAllLayers(*frame);

    bool start_ready_animations = draw_frame;
    host_impl_->UpdateAnimationState(start_ready_animations);
  }
  DidCommitAndDrawFrame(host_impl_->active_tree()->source_frame_number());

  return draw_result;
}

void SingleThreadProxy::DidCommitAndDrawFrame(int source_frame_number) {
  if (next_frame_is_newly_committed_frame_) {
    DebugScopedSetMainThread main(task_runner_provider_);
    next_frame_is_newly_committed_frame_ = false;
    layer_tree_host_->DidCommitAndDrawFrame(source_frame_number);
  }
}

bool SingleThreadProxy::MainFrameWillHappenForTesting() {
  DCHECK(task_runner_provider_->IsMainThread());
  if (!scheduler_on_impl_thread_)
    return false;
  DebugScopedSetImplThread impl(task_runner_provider_);
  return scheduler_on_impl_thread_->MainFrameForTestingWillHappen();
}

void SingleThreadProxy::SetSourceURL(ukm::SourceId source_id, const GURL& url) {
  DCHECK(task_runner_provider_->IsMainThread());
  // Single-threaded mode is only for browser compositing and for renderers in
  // layout tests. This will still get called in the latter case, but we don't
  // need to record UKM in that case.
}

void SingleThreadProxy::SetUkmSmoothnessDestination(
    base::WritableSharedMemoryMapping ukm_smoothness_data) {
  DCHECK(task_runner_provider_->IsMainThread());
}

void SingleThreadProxy::ClearHistory() {
  DCHECK(task_runner_provider_->IsImplThread());
  if (scheduler_on_impl_thread_)
    scheduler_on_impl_thread_->ClearHistory();
}

void SingleThreadProxy::SetHasActiveThreadedScroll(bool is_scrolling) {
  // Some tests use `SingleThreadProxy` however they are setup with
  // `LayerTreeSettings.single_thread_proxy_scheduler` true. Meaning we are in
  // a mixed state. This is done to support `CompositeImmediatelyForTest`.
  //
  // We do not want to run the checks while in this state. We only create
  // `scheduler_on_impl_thread_` when properly created with
  // `single_thread_proxy_scheduler`.
  if (scheduler_on_impl_thread_) {
    NOTREACHED();
  }
}
void SingleThreadProxy::SetWaitingForScrollEvent(
    bool waiting_for_scroll_event) {
  if (scheduler_on_impl_thread_) {
    NOTREACHED();
  }
}

size_t SingleThreadProxy::CommitDurationSampleCountForTesting() const {
  DCHECK(scheduler_on_impl_thread_);
  return scheduler_on_impl_thread_
      ->CommitDurationSampleCountForTesting();  // IN-TEST
}

void SingleThreadProxy::SetRenderFrameObserver(
    std::unique_ptr<RenderFrameMetadataObserver> observer) {
  DCHECK(task_runner_provider_->IsMainThread());
  DebugScopedSetImplThread impl(task_runner_provider_);
  host_impl_->SetRenderFrameObserver(std::move(observer));
}

double SingleThreadProxy::GetPercentDroppedFrames() const {
  DebugScopedSetImplThread impl(task_runner_provider_);
  return host_impl_->dropped_frame_counter()
      ->sliding_window_current_percent_dropped();
}

void SingleThreadProxy::UpdateBrowserControlsState(
    BrowserControlsState constraints,
    BrowserControlsState current,
    bool animate,
    base::optional_ref<const BrowserControlsOffsetTagsInfo> offset_tags_info) {
  DCHECK(task_runner_provider_->IsMainThread());
  DebugScopedSetImplThread impl(task_runner_provider_);
  host_impl_->browser_controls_manager()->UpdateBrowserControlsState(
      constraints, current, animate, offset_tags_info);
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
  DebugScopedSetImplThread impl(task_runner_provider_);
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
}

void SingleThreadProxy::FrameIntervalUpdated(base::TimeDelta interval) {
  DebugScopedSetImplThread impl(task_runner_provider_);
  single_thread_client_->FrameIntervalUpdated(interval);
}

void SingleThreadProxy::OnBeginImplFrameDeadline() {
  host_impl_->OnBeginImplFrameDeadline();
}

void SingleThreadProxy::SendBeginMainFrameNotExpectedSoon() {
  // DebugScopedSetImplThread here is just a formality; all SchedulerClient
  // methods should have it.
  DebugScopedSetImplThread impl(task_runner_provider_);
  DebugScopedSetMainThread main(task_runner_provider_);
  layer_tree_host_->BeginMainFrameNotExpectedSoon();
}

void SingleThreadProxy::ScheduledActionBeginMainFrameNotExpectedUntil(
    base::TimeTicks time) {
  // DebugScopedSetImplThread here is just a formality; all SchedulerClient
  // methods should have it.
  DebugScopedSetImplThread impl(task_runner_provider_);
  DebugScopedSetMainThread main(task_runner_provider_);
  layer_tree_host_->BeginMainFrameNotExpectedUntil(time);
}

void SingleThreadProxy::BeginMainFrame(
    const viz::BeginFrameArgs& begin_frame_args) {
  // This checker assumes NotifyReadyToCommit in this stack causes a synchronous
  // commit.
  ScopedAbortRemainingSwapPromises swap_promise_checker(
      layer_tree_host_->GetSwapPromiseManager());

  base::TimeTicks frame_start_time = base::TimeTicks::Now();

  if (scheduler_on_impl_thread_) {
    scheduler_on_impl_thread_->NotifyBeginMainFrameStarted(frame_start_time);
  }

  commit_requested_ = false;
  needs_impl_frame_ = false;
  animate_requested_ = false;
  update_layers_requested_ = false;

  if (defer_main_frame_update_) {
    TRACE_EVENT_INSTANT0("cc", "EarlyOut_DeferBeginMainFrame",
                         TRACE_EVENT_SCOPE_THREAD);
    BeginMainFrameAbortedOnImplThread(
        CommitEarlyOutReason::kAbortedDeferredMainFrameUpdate);
    return;
  }

  if (!layer_tree_host_->IsVisible()) {
    TRACE_EVENT_INSTANT0("cc", "EarlyOut_NotVisible", TRACE_EVENT_SCOPE_THREAD);

    // Since the commit is deferred due to the page becoming invisible, the
    // metrics are not meaningful anymore (as the page might become visible in
    // any arbitrary time in the future and cause an arbitrarily large latency).
    // Discard event metrics.
    layer_tree_host_->ClearEventsMetrics();

    BeginMainFrameAbortedOnImplThread(CommitEarlyOutReason::kAbortedNotVisible);
    return;
  }

  // Prevent new commits from being requested inside DoBeginMainFrame.
  // Note: We do not want to prevent SetNeedsAnimate from requesting
  // a commit here.
  commit_requested_ = true;

  // Check now if we should stop deferring commits. Do this before
  // DoBeginMainFrame because the latter updates scroll offsets, which
  // we should avoid if deferring commits.
  if (IsDeferringCommits() && frame_start_time > commits_restart_time_)
    StopDeferringCommits(ReasonToTimeoutTrigger(*paint_holding_reason_));

  layer_tree_host_->RecordStartOfFrameMetrics();
  DoBeginMainFrame(begin_frame_args);

  // New commits requested inside UpdateLayers should be respected.
  commit_requested_ = false;

  // At this point the main frame may have deferred commits to avoid committing
  // right now.
  if (defer_main_frame_update_ || IsDeferringCommits() ||
      begin_frame_args.animate_only) {
    TRACE_EVENT_INSTANT0("cc", "EarlyOut_DeferCommit_InsideBeginMainFrame",
                         TRACE_EVENT_SCOPE_THREAD);
    BeginMainFrameAbortedOnImplThread(
        CommitEarlyOutReason::kAbortedDeferredCommit);
    layer_tree_host_->RecordEndOfFrameMetrics(frame_start_time,
                                              /* trackers */ 0u);
    layer_tree_host_->DidBeginMainFrame();
    return;
  }

  DoPainting(begin_frame_args);
  layer_tree_host_->RecordEndOfFrameMetrics(frame_start_time,
                                            /* trackers */ 0u);
}

void SingleThreadProxy::DoBeginMainFrame(
    const viz::BeginFrameArgs& begin_frame_args) {
  // Only update scroll deltas if we are going to commit the frame, otherwise
  // scroll offsets get confused.
  if (!IsDeferringCommits()) {
    // The impl-side scroll deltas may be manipulated directly via the
    // InputHandler on the UI thread and the scale deltas may change when they
    // are clamped on the impl thread.
    std::unique_ptr<CompositorCommitData> commit_data;
    {
      DebugScopedSetImplThread impl(task_runner_provider_);
      commit_data = host_impl_->ProcessCompositorDeltas(
          /* main_thread_mutator_host */ nullptr);
    }
    layer_tree_host_->ApplyCompositorChanges(commit_data.get());
    did_apply_compositor_deltas_ = true;
  }
  layer_tree_host_->ApplyMutatorEvents(host_impl_->TakeMutatorEvents());
  layer_tree_host_->WillBeginMainFrame();
  layer_tree_host_->BeginMainFrame(begin_frame_args);
  layer_tree_host_->AnimateLayers(begin_frame_args.frame_time);

#if BUILDFLAG(IS_CHROMEOS)
  const bool record_metrics =
      layer_tree_host_->GetSettings().is_layer_tree_for_ui;
#else
  constexpr bool record_metrics = false;
#endif
  layer_tree_host_->RequestMainFrameUpdate(record_metrics);

  // Reset the flag for the next time around. It has been used for this frame.
  did_apply_compositor_deltas_ = false;
}

void SingleThreadProxy::DoPainting(const viz::BeginFrameArgs& commit_args) {
  layer_tree_host_->UpdateLayers();
  update_layers_requested_ = false;

  std::unique_ptr<BeginMainFrameMetrics> begin_main_frame_metrics =
      layer_tree_host_->TakeBeginMainFrameMetrics();
  host_impl_->ReadyToCommit(commit_args, true, begin_main_frame_metrics.get());

  // TODO(enne): SingleThreadProxy does not support cancelling commits yet,
  // search for CommitEarlyOutReason::FINISHED_NO_UPDATES inside
  // thread_proxy.cc
  if (scheduler_on_impl_thread_) {
    scheduler_on_impl_thread_->NotifyReadyToCommit(
        std::move(begin_main_frame_metrics));
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
      scheduler_on_impl_thread_->last_dispatched_begin_main_frame_args(),
      /* next_bmf */ false, did_apply_compositor_deltas_);
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
}

void SingleThreadProxy::ScheduledActionUpdateDisplayTree() {
  NOTREACHED();
}

void SingleThreadProxy::ScheduledActionCommit() {
  // DebugScopedSetImplThread here is just a formality; all SchedulerClient
  // methods should have it.
  DebugScopedSetImplThread impl(task_runner_provider_);
  DebugScopedSetMainThread main(task_runner_provider_);
  DoCommit(scheduler_on_impl_thread_->last_dispatched_begin_main_frame_args());
}

void SingleThreadProxy::ScheduledActionPostCommit() {
  // DebugScopedSetImplThread here is just a formality; all SchedulerClient
  // methods should have it.
  DebugScopedSetImplThread impl(task_runner_provider_);
  DebugScopedSetMainThread main(task_runner_provider_);
  DoPostCommit();
}

void SingleThreadProxy::ScheduledActionActivateSyncTree() {
  DebugScopedSetImplThread impl(task_runner_provider_);
  host_impl_->ActivateSyncTree();
}

void SingleThreadProxy::ScheduledActionBeginLayerTreeFrameSinkCreation() {
  DCHECK(scheduler_on_impl_thread_);
  DebugScopedSetImplThread impl(task_runner_provider_);
  // If possible, create the output surface in a post task.  Synchronously
  // creating the output surface makes tests more awkward since this differs
  // from the ThreadProxy behavior.  However, sometimes there is no
  // task runner.
  if (task_runner_provider_->MainThreadTaskRunner()) {
    ScheduleRequestNewLayerTreeFrameSink();
  } else {
    DebugScopedSetMainThread main(task_runner_provider_);
    auto sync = layer_tree_host_->ForceSyncCompositeForTest();  // IN-TEST
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

void SingleThreadProxy::DidFinishImplFrame(
    const viz::BeginFrameArgs& last_activated_args) {
  DebugScopedSetImplThread impl(task_runner_provider_);
  host_impl_->DidFinishImplFrame(last_activated_args);
#if DCHECK_IS_ON()
  DCHECK(inside_impl_frame_)
      << "DidFinishImplFrame called while not inside an impl frame!";
  inside_impl_frame_ = false;
#endif
}

void SingleThreadProxy::DidNotProduceFrame(const viz::BeginFrameAck& ack,
                                           FrameSkippedReason reason) {
  DebugScopedSetImplThread impl(task_runner_provider_);
  host_impl_->DidNotProduceFrame(ack, reason);
}

void SingleThreadProxy::WillNotReceiveBeginFrame() {
  DebugScopedSetImplThread impl(task_runner_provider_);
  host_impl_->DidNotNeedBeginFrame();
}

void SingleThreadProxy::DidReceiveCompositorFrameAck() {
  DebugScopedSetMainThread main(task_runner_provider_);
  layer_tree_host_->DidReceiveCompositorFrameAckDeprecatedForCompositor();
}

}  // namespace cc
