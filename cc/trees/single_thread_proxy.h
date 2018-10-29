// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_SINGLE_THREAD_PROXY_H_
#define CC_TREES_SINGLE_THREAD_PROXY_H_

#include <limits>

#include "base/cancelable_callback.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "cc/scheduler/scheduler.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "cc/trees/proxy.h"
#include "cc/trees/task_runner_provider.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"

namespace viz {
class BeginFrameSource;
}

namespace cc {

class MutatorEvents;
class LayerTreeHost;
class LayerTreeHostSingleThreadClient;
class RenderFrameMetadataObserver;

class CC_EXPORT SingleThreadProxy : public Proxy,
                                    LayerTreeHostImplClient,
                                    public SchedulerClient {
 public:
  static std::unique_ptr<Proxy> Create(
      LayerTreeHost* layer_tree_host,
      LayerTreeHostSingleThreadClient* client,
      TaskRunnerProvider* task_runner_provider_);
  ~SingleThreadProxy() override;

  // Proxy implementation
  bool IsStarted() const override;
  bool CommitToActiveTree() const override;
  void SetLayerTreeFrameSink(
      LayerTreeFrameSink* layer_tree_frame_sink) override;
  void ReleaseLayerTreeFrameSink() override;
  void SetVisible(bool visible) override;
  void SetNeedsAnimate() override;
  void SetNeedsUpdateLayers() override;
  void SetNeedsCommit() override;
  void SetNeedsRedraw(const gfx::Rect& damage_rect) override;
  void SetNextCommitWaitsForActivation() override;
  bool RequestedAnimatePending() override;
  void NotifyInputThrottledUntilCommit() override {}
  void SetDeferCommits(bool defer_commits) override;
  bool CommitRequested() const override;
  void Start() override;
  void Stop() override;
  void SetMutator(std::unique_ptr<LayerTreeMutator> mutator) override;
  bool SupportsImplScrolling() const override;
  bool MainFrameWillHappenForTesting() override;
  void SetURLForUkm(const GURL& url) override {
    // Single-threaded mode is only for browser compositing and for renderers in
    // layout tests. This will still get called in the latter case, but we don't
    // need to record UKM in that case.
  }
  void ClearHistory() override;
  void SetRenderFrameObserver(
      std::unique_ptr<RenderFrameMetadataObserver> observer) override;

  void UpdateBrowserControlsState(BrowserControlsState constraints,
                                  BrowserControlsState current,
                                  bool animate) override;

  // SchedulerClient implementation
  bool WillBeginImplFrame(const viz::BeginFrameArgs& args) override;
  void DidFinishImplFrame() override;
  void DidNotProduceFrame(const viz::BeginFrameAck& ack) override;
  void ScheduledActionSendBeginMainFrame(
      const viz::BeginFrameArgs& args) override;
  DrawResult ScheduledActionDrawIfPossible() override;
  DrawResult ScheduledActionDrawForced() override;
  void ScheduledActionCommit() override;
  void ScheduledActionActivateSyncTree() override;
  void ScheduledActionBeginLayerTreeFrameSinkCreation() override;
  void ScheduledActionPrepareTiles() override;
  void ScheduledActionInvalidateLayerTreeFrameSink(bool needs_redraw) override;
  void ScheduledActionPerformImplSideInvalidation() override;
  void SendBeginMainFrameNotExpectedSoon() override;
  void ScheduledActionBeginMainFrameNotExpectedUntil(
      base::TimeTicks time) override;
  size_t CompositedAnimationsCount() const override;
  size_t MainThreadAnimationsCount() const override;
  bool CurrentFrameHadRAF() const override;
  bool NextFrameHasPendingRAF() const override;

  // LayerTreeHostImplClient implementation
  void DidLoseLayerTreeFrameSinkOnImplThread() override;
  void SetBeginFrameSource(viz::BeginFrameSource* source) override;
  void DidReceiveCompositorFrameAckOnImplThread() override;
  void OnCanDrawStateChanged(bool can_draw) override;
  void NotifyReadyToActivate() override;
  void NotifyReadyToDraw() override;
  void SetNeedsRedrawOnImplThread() override;
  void SetNeedsOneBeginImplFrameOnImplThread() override;
  void SetNeedsPrepareTilesOnImplThread() override;
  void SetNeedsCommitOnImplThread() override;
  void SetVideoNeedsBeginFrames(bool needs_begin_frames) override;
  void PostAnimationEventsToMainThreadOnImplThread(
      std::unique_ptr<MutatorEvents> events) override;
  bool IsInsideDraw() override;
  void RenewTreePriority() override {}
  void PostDelayedAnimationTaskOnImplThread(const base::Closure& task,
                                            base::TimeDelta delay) override {}
  void DidActivateSyncTree() override;
  void WillPrepareTiles() override;
  void DidPrepareTiles() override;
  void DidCompletePageScaleAnimationOnImplThread() override;
  void OnDrawForLayerTreeFrameSink(bool resourceless_software_draw,
                                   bool skip_draw) override;
  void NeedsImplSideInvalidation(bool needs_first_draw_on_activation) override;
  void RequestBeginMainFrameNotExpected(bool new_state) override;
  void NotifyImageDecodeRequestFinished() override;
  void DidPresentCompositorFrameOnImplThread(
      uint32_t frame_token,
      std::vector<LayerTreeHost::PresentationTimeCallback> callbacks,
      const gfx::PresentationFeedback& feedback) override;

  void RequestNewLayerTreeFrameSink();

  // Called by the legacy path where RenderWidget does the scheduling.
  // Rasterization of tiles is only performed when |raster| is true.
  void CompositeImmediately(base::TimeTicks frame_begin_time, bool raster);

 protected:
  SingleThreadProxy(LayerTreeHost* layer_tree_host,
                    LayerTreeHostSingleThreadClient* client,
                    TaskRunnerProvider* task_runner_provider);

 private:
  void BeginMainFrame(const viz::BeginFrameArgs& begin_frame_args);
  void BeginMainFrameAbortedOnImplThread(CommitEarlyOutReason reason);
  void DoBeginMainFrame(const viz::BeginFrameArgs& begin_frame_args);
  void DoPainting();
  void DoCommit();
  DrawResult DoComposite(LayerTreeHostImpl::FrameData* frame);
  void DoSwap();
  void DidCommitAndDrawFrame();
  void CommitComplete();

  bool ShouldComposite() const;
  void ScheduleRequestNewLayerTreeFrameSink();
  void IssueImageDecodeFinishedCallbacks();

  void DidReceiveCompositorFrameAck();

  // Accessed on main thread only.
  LayerTreeHost* layer_tree_host_;
  LayerTreeHostSingleThreadClient* single_thread_client_;

  TaskRunnerProvider* task_runner_provider_;

  // Used on the Thread, but checked on main thread during
  // initialization/shutdown.
  std::unique_ptr<LayerTreeHostImpl> host_impl_;

  // Accessed from both threads.
  std::unique_ptr<Scheduler> scheduler_on_impl_thread_;

  bool next_frame_is_newly_committed_frame_;

#if DCHECK_IS_ON()
  bool inside_impl_frame_;
#endif
  bool inside_draw_;
  bool defer_commits_;
  bool animate_requested_;
  bool commit_requested_;
  bool inside_synchronous_composite_;
  bool needs_impl_frame_;

  // True if a request to the LayerTreeHostClient to create an output surface
  // is still outstanding.
  bool layer_tree_frame_sink_creation_requested_;
  // When output surface is lost, is set to true until a new output surface is
  // initialized.
  bool layer_tree_frame_sink_lost_;

  // This is the callback for the scheduled RequestNewLayerTreeFrameSink.
  base::CancelableClosure layer_tree_frame_sink_creation_callback_;

  base::WeakPtr<SingleThreadProxy> frame_sink_bound_weak_ptr_;

  // WeakPtrs generated by this factory will be invalidated when
  // LayerTreeFrameSink is released.
  base::WeakPtrFactory<SingleThreadProxy> frame_sink_bound_weak_factory_;

  base::WeakPtrFactory<SingleThreadProxy> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SingleThreadProxy);
};

// For use in the single-threaded case. In debug builds, it pretends that the
// code is running on the impl thread to satisfy assertion checks.
class DebugScopedSetImplThread {
 public:
#if DCHECK_IS_ON()
  explicit DebugScopedSetImplThread(TaskRunnerProvider* task_runner_provider)
      : task_runner_provider_(task_runner_provider) {
    previous_value_ = task_runner_provider_->impl_thread_is_overridden_;
    task_runner_provider_->SetCurrentThreadIsImplThread(true);
  }
#else
  explicit DebugScopedSetImplThread(TaskRunnerProvider* task_runner_provider) {}
#endif

  ~DebugScopedSetImplThread() {
#if DCHECK_IS_ON()
    task_runner_provider_->SetCurrentThreadIsImplThread(previous_value_);
#endif
  }

 private:
#if DCHECK_IS_ON()
  bool previous_value_;
  TaskRunnerProvider* task_runner_provider_;
#endif

  DISALLOW_COPY_AND_ASSIGN(DebugScopedSetImplThread);
};

// For use in the single-threaded case. In debug builds, it pretends that the
// code is running on the main thread to satisfy assertion checks.
class DebugScopedSetMainThread {
 public:
#if DCHECK_IS_ON()
  explicit DebugScopedSetMainThread(TaskRunnerProvider* task_runner_provider)
      : task_runner_provider_(task_runner_provider) {
    previous_value_ = task_runner_provider_->impl_thread_is_overridden_;
    task_runner_provider_->SetCurrentThreadIsImplThread(false);
  }
#else
  explicit DebugScopedSetMainThread(TaskRunnerProvider* task_runner_provider) {}
#endif

  ~DebugScopedSetMainThread() {
#if DCHECK_IS_ON()
    task_runner_provider_->SetCurrentThreadIsImplThread(previous_value_);
#endif
  }

 private:
#if DCHECK_IS_ON()
  bool previous_value_;
  TaskRunnerProvider* task_runner_provider_;
#endif

  DISALLOW_COPY_AND_ASSIGN(DebugScopedSetMainThread);
};

// For use in the single-threaded case. In debug builds, it pretends that the
// code is running on the impl thread and that the main thread is blocked to
// satisfy assertion checks
class DebugScopedSetImplThreadAndMainThreadBlocked {
 public:
  explicit DebugScopedSetImplThreadAndMainThreadBlocked(
      TaskRunnerProvider* task_runner_provider)
      : impl_thread_(task_runner_provider),
        main_thread_blocked_(task_runner_provider) {}

 private:
  DebugScopedSetImplThread impl_thread_;
  DebugScopedSetMainThreadBlocked main_thread_blocked_;

  DISALLOW_COPY_AND_ASSIGN(DebugScopedSetImplThreadAndMainThreadBlocked);
};

}  // namespace cc

#endif  // CC_TREES_SINGLE_THREAD_PROXY_H_
