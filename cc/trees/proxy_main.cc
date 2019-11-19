// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/proxy_main.h"

#include <algorithm>
#include <string>

#include "base/bind.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "cc/base/completion_event.h"
#include "cc/base/devtools_instrumentation.h"
#include "cc/benchmarks/benchmark_instrumentation.h"
#include "cc/paint/paint_worklet_layer_painter.h"
#include "cc/resources/ui_resource_manager.h"
#include "cc/trees/latency_info_swap_promise.h"
#include "cc/trees/layer_tree_frame_sink.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/mutator_host.h"
#include "cc/trees/proxy_impl.h"
#include "cc/trees/render_frame_metadata_observer.h"
#include "cc/trees/scoped_abort_remaining_swap_promises.h"
#include "cc/trees/swap_promise.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

namespace cc {

ProxyMain::ProxyMain(LayerTreeHost* layer_tree_host,
                     TaskRunnerProvider* task_runner_provider)
    : layer_tree_host_(layer_tree_host),
      task_runner_provider_(task_runner_provider),
      layer_tree_host_id_(layer_tree_host->GetId()),
      max_requested_pipeline_stage_(NO_PIPELINE_STAGE),
      current_pipeline_stage_(NO_PIPELINE_STAGE),
      final_pipeline_stage_(NO_PIPELINE_STAGE),
      deferred_final_pipeline_stage_(NO_PIPELINE_STAGE),
      commit_waits_for_activation_(false),
      started_(false),
      defer_main_frame_update_(false),
      defer_commits_(false) {
  TRACE_EVENT0("cc", "ProxyMain::ProxyMain");
  DCHECK(task_runner_provider_);
  DCHECK(IsMainThread());
}

ProxyMain::~ProxyMain() {
  TRACE_EVENT0("cc", "ProxyMain::~ProxyMain");
  DCHECK(IsMainThread());
  DCHECK(!started_);
}

void ProxyMain::InitializeOnImplThread(CompletionEvent* completion_event) {
  DCHECK(task_runner_provider_->IsImplThread());
  DCHECK(!proxy_impl_);
  proxy_impl_ = std::make_unique<ProxyImpl>(
      weak_factory_.GetWeakPtr(), layer_tree_host_, task_runner_provider_);
  completion_event->Signal();
}

void ProxyMain::DestroyProxyImplOnImplThread(
    CompletionEvent* completion_event) {
  DCHECK(task_runner_provider_->IsImplThread());
  proxy_impl_.reset();
  completion_event->Signal();
}

void ProxyMain::DidReceiveCompositorFrameAck() {
  DCHECK(IsMainThread());
  layer_tree_host_->DidReceiveCompositorFrameAck();
}

void ProxyMain::BeginMainFrameNotExpectedSoon() {
  TRACE_EVENT0("cc", "ProxyMain::BeginMainFrameNotExpectedSoon");
  DCHECK(IsMainThread());
  layer_tree_host_->BeginMainFrameNotExpectedSoon();
}

void ProxyMain::BeginMainFrameNotExpectedUntil(base::TimeTicks time) {
  TRACE_EVENT0("cc", "ProxyMain::BeginMainFrameNotExpectedUntil");
  DCHECK(IsMainThread());
  layer_tree_host_->BeginMainFrameNotExpectedUntil(time);
}

void ProxyMain::DidCommitAndDrawFrame() {
  DCHECK(IsMainThread());
  layer_tree_host_->DidCommitAndDrawFrame();
}

void ProxyMain::SetAnimationEvents(std::unique_ptr<MutatorEvents> events) {
  TRACE_EVENT0("cc", "ProxyMain::SetAnimationEvents");
  DCHECK(IsMainThread());
  layer_tree_host_->SetAnimationEvents(std::move(events));
}

void ProxyMain::DidLoseLayerTreeFrameSink() {
  TRACE_EVENT0("cc", "ProxyMain::DidLoseLayerTreeFrameSink");
  DCHECK(IsMainThread());
  layer_tree_host_->DidLoseLayerTreeFrameSink();
}

void ProxyMain::RequestNewLayerTreeFrameSink() {
  TRACE_EVENT0("cc", "ProxyMain::RequestNewLayerTreeFrameSink");
  DCHECK(IsMainThread());
  layer_tree_host_->RequestNewLayerTreeFrameSink();
}

void ProxyMain::DidInitializeLayerTreeFrameSink(bool success) {
  TRACE_EVENT0("cc", "ProxyMain::DidInitializeLayerTreeFrameSink");
  DCHECK(IsMainThread());

  if (!success)
    layer_tree_host_->DidFailToInitializeLayerTreeFrameSink();
  else
    layer_tree_host_->DidInitializeLayerTreeFrameSink();
}

void ProxyMain::DidCompletePageScaleAnimation() {
  DCHECK(IsMainThread());
  layer_tree_host_->DidCompletePageScaleAnimation();
}

void ProxyMain::BeginMainFrame(
    std::unique_ptr<BeginMainFrameAndCommitState> begin_main_frame_state) {
  DCHECK(IsMainThread());
  DCHECK_EQ(NO_PIPELINE_STAGE, current_pipeline_stage_);

  base::TimeTicks begin_main_frame_start_time = base::TimeTicks::Now();

  benchmark_instrumentation::ScopedBeginFrameTask begin_frame_task(
      benchmark_instrumentation::kDoBeginFrame,
      begin_main_frame_state->begin_frame_id);

  // This needs to run unconditionally, so do it before any early-returns.
  if (layer_tree_host_->scheduling_client())
    layer_tree_host_->scheduling_client()->DidRunBeginMainFrame();

  // If the commit finishes, LayerTreeHost will transfer its swap promises to
  // LayerTreeImpl. The destructor of ScopedSwapPromiseChecker aborts the
  // remaining swap promises.
  ScopedAbortRemainingSwapPromises swap_promise_checker(
      layer_tree_host_->GetSwapPromiseManager());

  // We need to issue image decode callbacks whether or not we will abort this
  // update and commit, since the request ids are only stored in
  // |begin_main_frame_state|.
  layer_tree_host_->ImageDecodesFinished(
      std::move(begin_main_frame_state->completed_image_decode_requests));

  // Visibility check needs to happen before setting
  // max_requested_pipeline_stage_. Otherwise a requested commit could get lost
  // after tab becomes visible again.
  if (!layer_tree_host_->IsVisible()) {
    TRACE_EVENT_INSTANT0("cc", "EarlyOut_NotVisible", TRACE_EVENT_SCOPE_THREAD);
    std::vector<std::unique_ptr<SwapPromise>> empty_swap_promises;
    ImplThreadTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(&ProxyImpl::BeginMainFrameAbortedOnImpl,
                                  base::Unretained(proxy_impl_.get()),
                                  CommitEarlyOutReason::ABORTED_NOT_VISIBLE,
                                  begin_main_frame_start_time,
                                  std::move(empty_swap_promises)));
    return;
  }

  final_pipeline_stage_ = max_requested_pipeline_stage_;
  max_requested_pipeline_stage_ = NO_PIPELINE_STAGE;

  // When we don't need to produce a CompositorFrame, there's also no need to
  // commit our updates. We still need to run layout and paint though, as it can
  // have side effects on page loading behavior.
  bool skip_commit = begin_main_frame_state->begin_frame_args.animate_only;

  // If main frame updates and commits are deferred, skip the entire pipeline.
  bool skip_full_pipeline = defer_main_frame_update_;

  // We may have previously skipped paint and commit. If we should still skip it
  // now, and there was no intermediate request for a commit since the last
  // BeginMainFrame, we can skip the full pipeline.
  skip_full_pipeline |=
      skip_commit && final_pipeline_stage_ == NO_PIPELINE_STAGE;

  if (skip_full_pipeline) {
    TRACE_EVENT_INSTANT0("cc", "EarlyOut_DeferCommit",
                         TRACE_EVENT_SCOPE_THREAD);
    std::vector<std::unique_ptr<SwapPromise>> empty_swap_promises;
    ImplThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(&ProxyImpl::BeginMainFrameAbortedOnImpl,
                       base::Unretained(proxy_impl_.get()),
                       CommitEarlyOutReason::ABORTED_DEFERRED_MAIN_FRAME_UPDATE,
                       begin_main_frame_start_time,
                       std::move(empty_swap_promises)));
    // When we stop deferring main frame updates, we should resume any
    // previously requested pipeline stages.
    deferred_final_pipeline_stage_ =
        std::max(final_pipeline_stage_, deferred_final_pipeline_stage_);
    return;
  }

  final_pipeline_stage_ =
      std::max(final_pipeline_stage_, deferred_final_pipeline_stage_);
  deferred_final_pipeline_stage_ = NO_PIPELINE_STAGE;

  current_pipeline_stage_ = ANIMATE_PIPELINE_STAGE;

  // Check now if we should stop deferring commits due to a timeout. We
  // may also stop deferring in layer_tree_host_->BeginMainFrame, but update
  // the status at this point to keep scroll in sync.
  if (defer_commits_ && base::TimeTicks::Now() > commits_restart_time_)
    StopDeferringCommits(PaintHoldingCommitTrigger::kTimeout);
  skip_commit |= defer_commits_;

  if (!skip_commit) {
    // Synchronizes scroll offsets and page scale deltas (for pinch zoom) from
    // the compositor thread to the main thread for both cc and and its client
    // (e.g. Blink). Do not do this if we explicitly plan to not commit the
    // layer tree, to prevent scroll offsets getting out of sync.
    layer_tree_host_->ApplyScrollAndScale(
        begin_main_frame_state->scroll_info.get());
  }

  layer_tree_host_->WillBeginMainFrame();

  // This call winds through to the LocalFrameView to mark the beginning
  // of a main frame for metrics purposes. Some metrics are only gathered
  // between calls to RecordStartOfFrameMetrics and RecordEndOfFrameMetrics.
  // This is not wrapped into layer_tree_host_->WillBeginMainFrame because
  // it should only be called from the multi-threaded proxy (we do not want
  // metrics gathering in tests).
  layer_tree_host_->RecordStartOfFrameMetrics();

  // See LayerTreeHostClient::BeginMainFrame for more documentation on
  // what this does.
  layer_tree_host_->BeginMainFrame(begin_main_frame_state->begin_frame_args);

  // Updates cc animations on the main-thread. This is necessary in order
  // to track animation states such that they are cleaned up properly.
  layer_tree_host_->AnimateLayers(
      begin_main_frame_state->begin_frame_args.frame_time);

  // Recreates all UI resources if the compositor thread evicted UI resources
  // because it became invisible or there was a lost context when the compositor
  // thread initiated the commit.
  if (begin_main_frame_state->evicted_ui_resources)
    layer_tree_host_->GetUIResourceManager()->RecreateUIResources();

  // See LayerTreeHostClient::MainFrameUpdate for more documentation on
  // what this does.
  layer_tree_host_->RequestMainFrameUpdate(true /* report_cc_metrics */);

  // At this point the main frame may have deferred main frame updates to
  // avoid committing right now, or we may be deferring commits but not
  // deferring main frame updates. Either may have changed the status
  // of the defer... flags, so re-evaluate skip_commit.
  skip_commit |= defer_main_frame_update_ || defer_commits_;

  if (skip_commit) {
    current_pipeline_stage_ = NO_PIPELINE_STAGE;
    layer_tree_host_->DidBeginMainFrame();
    TRACE_EVENT_INSTANT0("cc", "EarlyOut_DeferCommit_InsideBeginMainFrame",
                         TRACE_EVENT_SCOPE_THREAD);
    layer_tree_host_->RecordEndOfFrameMetrics(begin_main_frame_start_time);
    std::vector<std::unique_ptr<SwapPromise>> empty_swap_promises;
    ImplThreadTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(&ProxyImpl::BeginMainFrameAbortedOnImpl,
                                  base::Unretained(proxy_impl_.get()),
                                  CommitEarlyOutReason::ABORTED_DEFERRED_COMMIT,
                                  begin_main_frame_start_time,
                                  std::move(empty_swap_promises)));
    // We intentionally don't report CommitComplete() here since it was aborted
    // prematurely and we're waiting to do another commit in the future.
    // When we stop deferring commits, we should resume any previously requested
    // pipeline stages.
    deferred_final_pipeline_stage_ = final_pipeline_stage_;
    return;
  }

  // If UI resources were evicted on the impl thread, we need a commit.
  if (begin_main_frame_state->evicted_ui_resources)
    final_pipeline_stage_ = COMMIT_PIPELINE_STAGE;

  current_pipeline_stage_ = UPDATE_LAYERS_PIPELINE_STAGE;
  bool should_update_layers =
      final_pipeline_stage_ >= UPDATE_LAYERS_PIPELINE_STAGE;

  // Among other things, UpdateLayers:
  // -Updates property trees in cc.
  // -Updates state for and "paints" display lists for cc layers by asking
  // cc's client to do so.
  // If the layer painting is backed by Blink, Blink generates the display
  // list in advance, and "painting" amounts to copying the Blink display list
  // to corresponding  cc display list. An exception is for painted scrollbars,
  // which paint eagerly during layer update.
  bool updated = should_update_layers && layer_tree_host_->UpdateLayers();

  // If updating the layers resulted in a content update, we need a commit.
  if (updated)
    final_pipeline_stage_ = COMMIT_PIPELINE_STAGE;

  layer_tree_host_->WillCommit();
  devtools_instrumentation::ScopedCommitTrace commit_task(
      layer_tree_host_->GetId());

  current_pipeline_stage_ = COMMIT_PIPELINE_STAGE;
  if (final_pipeline_stage_ < COMMIT_PIPELINE_STAGE) {
    current_pipeline_stage_ = NO_PIPELINE_STAGE;
    layer_tree_host_->DidBeginMainFrame();
    TRACE_EVENT_INSTANT0("cc", "EarlyOut_NoUpdates", TRACE_EVENT_SCOPE_THREAD);
    std::vector<std::unique_ptr<SwapPromise>> swap_promises =
        layer_tree_host_->GetSwapPromiseManager()->TakeSwapPromises();
    ImplThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(&ProxyImpl::BeginMainFrameAbortedOnImpl,
                       base::Unretained(proxy_impl_.get()),
                       CommitEarlyOutReason::FINISHED_NO_UPDATES,
                       begin_main_frame_start_time, std::move(swap_promises)));

    // Although the commit is internally aborted, this is because it has been
    // detected to be a no-op.  From the perspective of an embedder, this commit
    // went through, and input should no longer be throttled, etc.
    layer_tree_host_->CommitComplete();
    layer_tree_host_->RecordEndOfFrameMetrics(begin_main_frame_start_time);
    return;
  }

  current_pipeline_stage_ = NO_PIPELINE_STAGE;

  // Notify the impl thread that the main thread is ready to commit. This will
  // begin the commit process, which is blocking from the main thread's
  // point of view, but asynchronously performed on the impl thread,
  // coordinated by the Scheduler.
  {
    TRACE_EVENT0("cc", "ProxyMain::BeginMainFrame::commit");

    DebugScopedSetMainThreadBlocked main_thread_blocked(task_runner_provider_);

    bool hold_commit_for_activation = commit_waits_for_activation_;
    commit_waits_for_activation_ = false;
    CompletionEvent completion;
    ImplThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(&ProxyImpl::NotifyReadyToCommitOnImpl,
                       base::Unretained(proxy_impl_.get()), &completion,
                       layer_tree_host_, begin_main_frame_start_time,
                       hold_commit_for_activation));
    completion.Wait();
  }
  layer_tree_host_->CommitComplete();

  // For Blink implementations, this updates frame throttling and
  // delivers IntersectionObserver events for Chromium-internal customers
  // but *not* script-created IntersectionObserver. See
  // blink::LocalFrameView::RunPostLifecycleSteps.
  layer_tree_host_->DidBeginMainFrame();

  layer_tree_host_->RecordEndOfFrameMetrics(begin_main_frame_start_time);
}

void ProxyMain::DidPresentCompositorFrame(
    uint32_t frame_token,
    std::vector<LayerTreeHost::PresentationTimeCallback> callbacks,
    const gfx::PresentationFeedback& feedback) {
  layer_tree_host_->DidPresentCompositorFrame(frame_token, std::move(callbacks),
                                              feedback);
}

bool ProxyMain::IsStarted() const {
  DCHECK(IsMainThread());
  return started_;
}

void ProxyMain::SetLayerTreeFrameSink(
    LayerTreeFrameSink* layer_tree_frame_sink) {
  ImplThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&ProxyImpl::InitializeLayerTreeFrameSinkOnImpl,
                     base::Unretained(proxy_impl_.get()), layer_tree_frame_sink,
                     frame_sink_bound_weak_factory_.GetWeakPtr()));
}

void ProxyMain::SetVisible(bool visible) {
  TRACE_EVENT1("cc", "ProxyMain::SetVisible", "visible", visible);
  ImplThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&ProxyImpl::SetVisibleOnImpl,
                                base::Unretained(proxy_impl_.get()), visible));
}

void ProxyMain::SetNeedsAnimate() {
  DCHECK(IsMainThread());
  if (SendCommitRequestToImplThreadIfNeeded(ANIMATE_PIPELINE_STAGE)) {
    TRACE_EVENT_INSTANT0("cc", "ProxyMain::SetNeedsAnimate",
                         TRACE_EVENT_SCOPE_THREAD);
  }
}

void ProxyMain::SetNeedsUpdateLayers() {
  DCHECK(IsMainThread());
  // If we are currently animating, make sure we also update the layers.
  if (current_pipeline_stage_ == ANIMATE_PIPELINE_STAGE) {
    final_pipeline_stage_ =
        std::max(final_pipeline_stage_, UPDATE_LAYERS_PIPELINE_STAGE);
    return;
  }
  if (SendCommitRequestToImplThreadIfNeeded(UPDATE_LAYERS_PIPELINE_STAGE)) {
    TRACE_EVENT_INSTANT0("cc", "ProxyMain::SetNeedsUpdateLayers",
                         TRACE_EVENT_SCOPE_THREAD);
  }
}

void ProxyMain::SetNeedsCommit() {
  DCHECK(IsMainThread());
  // If we are currently animating, make sure we don't skip the commit. Note
  // that requesting a commit during the layer update stage means we need to
  // schedule another full commit.
  if (current_pipeline_stage_ == ANIMATE_PIPELINE_STAGE) {
    final_pipeline_stage_ =
        std::max(final_pipeline_stage_, COMMIT_PIPELINE_STAGE);
    return;
  }
  if (SendCommitRequestToImplThreadIfNeeded(COMMIT_PIPELINE_STAGE)) {
    TRACE_EVENT_INSTANT0("cc", "ProxyMain::SetNeedsCommit",
                         TRACE_EVENT_SCOPE_THREAD);
  }
}

void ProxyMain::SetNeedsRedraw(const gfx::Rect& damage_rect) {
  TRACE_EVENT0("cc", "ProxyMain::SetNeedsRedraw");
  DCHECK(IsMainThread());
  ImplThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&ProxyImpl::SetNeedsRedrawOnImpl,
                     base::Unretained(proxy_impl_.get()), damage_rect));
}

void ProxyMain::SetNextCommitWaitsForActivation() {
  DCHECK(IsMainThread());
  commit_waits_for_activation_ = true;
}

bool ProxyMain::RequestedAnimatePending() {
  return max_requested_pipeline_stage_ >= ANIMATE_PIPELINE_STAGE;
}

void ProxyMain::SetDeferMainFrameUpdate(bool defer_main_frame_update) {
  DCHECK(IsMainThread());
  if (defer_main_frame_update_ == defer_main_frame_update)
    return;

  defer_main_frame_update_ = defer_main_frame_update;
  if (defer_main_frame_update_) {
    TRACE_EVENT_ASYNC_BEGIN0("cc", "ProxyMain::SetDeferMainFrameUpdate", this);
  } else {
    TRACE_EVENT_ASYNC_END0("cc", "ProxyMain::SetDeferMainFrameUpdate", this);
  }

  // Notify dependent systems that the deferral status has changed.
  layer_tree_host_->OnDeferMainFrameUpdatesChanged(defer_main_frame_update_);

  // The impl thread needs to know that it should not issue BeginMainFrame.
  ImplThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&ProxyImpl::SetDeferBeginMainFrameOnImpl,
                                base::Unretained(proxy_impl_.get()),
                                defer_main_frame_update));
}

void ProxyMain::StartDeferringCommits(base::TimeDelta timeout) {
  DCHECK(task_runner_provider_->IsMainThread());

  // Do nothing if already deferring. The timeout remains as it was from when
  // we most recently began deferring.
  if (defer_commits_)
    return;

  TRACE_EVENT_ASYNC_BEGIN0("cc", "ProxyMain::SetDeferCommits", this);

  defer_commits_ = true;
  commits_restart_time_ = base::TimeTicks::Now() + timeout;

  // Notify dependent systems that the deferral status has changed.
  layer_tree_host_->OnDeferCommitsChanged(defer_commits_);
}

void ProxyMain::StopDeferringCommits(PaintHoldingCommitTrigger trigger) {
  if (!defer_commits_)
    return;
  defer_commits_ = false;
  UMA_HISTOGRAM_ENUMERATION("PaintHolding.CommitTrigger2", trigger);
  commits_restart_time_ = base::TimeTicks();
  TRACE_EVENT_ASYNC_END0("cc", "ProxyMain::SetDeferCommits", this);

  // Notify depended systems that the deferral status has changed.
  layer_tree_host_->OnDeferCommitsChanged(defer_commits_);
}

bool ProxyMain::CommitRequested() const {
  DCHECK(IsMainThread());
  // TODO(skyostil): Split this into something like CommitRequested() and
  // CommitInProgress().
  return current_pipeline_stage_ != NO_PIPELINE_STAGE ||
         max_requested_pipeline_stage_ >= COMMIT_PIPELINE_STAGE;
}

void ProxyMain::Start() {
  DCHECK(IsMainThread());
  DCHECK(layer_tree_host_->IsThreaded());

  {
    DebugScopedSetMainThreadBlocked main_thread_blocked(task_runner_provider_);
    CompletionEvent completion;
    ImplThreadTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(&ProxyMain::InitializeOnImplThread,
                                  base::Unretained(this), &completion));
    completion.Wait();
  }

  started_ = true;
}

void ProxyMain::Stop() {
  TRACE_EVENT0("cc", "ProxyMain::Stop");
  DCHECK(IsMainThread());
  DCHECK(started_);

  // Synchronously finishes pending GL operations and deletes the impl.
  // The two steps are done as separate post tasks, so that tasks posted
  // by the GL implementation due to the Finish can be executed by the
  // renderer before shutting it down.
  {
    DebugScopedSetMainThreadBlocked main_thread_blocked(task_runner_provider_);
    CompletionEvent completion;
    ImplThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(&ProxyImpl::FinishGLOnImpl,
                       base::Unretained(proxy_impl_.get()), &completion));
    completion.Wait();
  }
  {
    DebugScopedSetMainThreadBlocked main_thread_blocked(task_runner_provider_);
    CompletionEvent completion;
    ImplThreadTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(&ProxyMain::DestroyProxyImplOnImplThread,
                                  base::Unretained(this), &completion));
    completion.Wait();
  }

  weak_factory_.InvalidateWeakPtrs();
  layer_tree_host_ = nullptr;
  started_ = false;
}

void ProxyMain::SetMutator(std::unique_ptr<LayerTreeMutator> mutator) {
  TRACE_EVENT0("cc", "ThreadProxy::SetMutator");
  ImplThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&ProxyImpl::InitializeMutatorOnImpl,
                     base::Unretained(proxy_impl_.get()), std::move(mutator)));
}

void ProxyMain::SetPaintWorkletLayerPainter(
    std::unique_ptr<PaintWorkletLayerPainter> painter) {
  TRACE_EVENT0("cc", "ThreadProxy::SetPaintWorkletLayerPainter");
  ImplThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&ProxyImpl::InitializePaintWorkletLayerPainterOnImpl,
                     base::Unretained(proxy_impl_.get()), std::move(painter)));
}

bool ProxyMain::SupportsImplScrolling() const {
  return true;
}

bool ProxyMain::MainFrameWillHappenForTesting() {
  DCHECK(IsMainThread());
  bool main_frame_will_happen = false;
  {
    DebugScopedSetMainThreadBlocked main_thread_blocked(task_runner_provider_);
    CompletionEvent completion;
    ImplThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(&ProxyImpl::MainFrameWillHappenOnImplForTesting,
                       base::Unretained(proxy_impl_.get()), &completion,
                       &main_frame_will_happen));
    completion.Wait();
  }
  return main_frame_will_happen;
}

void ProxyMain::ReleaseLayerTreeFrameSink() {
  DCHECK(IsMainThread());
  frame_sink_bound_weak_factory_.InvalidateWeakPtrs();
  DebugScopedSetMainThreadBlocked main_thread_blocked(task_runner_provider_);
  CompletionEvent completion;
  ImplThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&ProxyImpl::ReleaseLayerTreeFrameSinkOnImpl,
                     base::Unretained(proxy_impl_.get()), &completion));
  completion.Wait();
}

void ProxyMain::UpdateBrowserControlsState(BrowserControlsState constraints,
                                           BrowserControlsState current,
                                           bool animate) {
  DCHECK(IsMainThread());
  ImplThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&ProxyImpl::UpdateBrowserControlsStateOnImpl,
                                base::Unretained(proxy_impl_.get()),
                                constraints, current, animate));
}

void ProxyMain::RequestBeginMainFrameNotExpected(bool new_state) {
  DCHECK(IsMainThread());
  ImplThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&ProxyImpl::RequestBeginMainFrameNotExpected,
                     base::Unretained(proxy_impl_.get()), new_state));
}

bool ProxyMain::SendCommitRequestToImplThreadIfNeeded(
    CommitPipelineStage required_stage) {
  DCHECK(IsMainThread());
  DCHECK_NE(NO_PIPELINE_STAGE, required_stage);
  bool already_posted = max_requested_pipeline_stage_ != NO_PIPELINE_STAGE;
  max_requested_pipeline_stage_ =
      std::max(max_requested_pipeline_stage_, required_stage);
  if (already_posted)
    return false;
  ImplThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&ProxyImpl::SetNeedsCommitOnImpl,
                                base::Unretained(proxy_impl_.get())));
  return true;
}

bool ProxyMain::IsMainThread() const {
  return task_runner_provider_->IsMainThread();
}

bool ProxyMain::IsImplThread() const {
  return task_runner_provider_->IsImplThread();
}

base::SingleThreadTaskRunner* ProxyMain::ImplThreadTaskRunner() {
  return task_runner_provider_->ImplThreadTaskRunner();
}

void ProxyMain::SetSourceURL(ukm::SourceId source_id, const GURL& url) {
  DCHECK(IsMainThread());
  ImplThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&ProxyImpl::SetSourceURL,
                                base::Unretained(proxy_impl_.get()),
                                source_id, url));
}

void ProxyMain::ClearHistory() {
  // Must only be called from the impl thread during commit.
  DCHECK(task_runner_provider_->IsImplThread());
  DCHECK(task_runner_provider_->IsMainThreadBlocked());
  proxy_impl_->ClearHistory();
}

void ProxyMain::SetRenderFrameObserver(
    std::unique_ptr<RenderFrameMetadataObserver> observer) {
  ImplThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&ProxyImpl::SetRenderFrameObserver,
                     base::Unretained(proxy_impl_.get()), std::move(observer)));
}

}  // namespace cc
