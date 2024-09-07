// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_SINGLE_THREAD_PROXY_H_
#define CC_TREES_SINGLE_THREAD_PROXY_H_

#include <limits>
#include <memory>
#include <vector>

#include "base/cancelable_callback.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/types/optional_ref.h"
#include "cc/input/browser_controls_offset_tags_info.h"
#include "cc/scheduler/scheduler.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "cc/trees/paint_holding_reason.h"
#include "cc/trees/proxy.h"
#include "cc/trees/task_runner_provider.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/surfaces/local_surface_id.h"

namespace viz {
class BeginFrameSource;
struct FrameTimingDetails;
}

namespace cc {

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
      TaskRunnerProvider* task_runner_provider);
  SingleThreadProxy(const SingleThreadProxy&) = delete;
  ~SingleThreadProxy() override;

  SingleThreadProxy& operator=(const SingleThreadProxy&) = delete;

  // Proxy implementation
  bool IsStarted() const override;
  void SetLayerTreeFrameSink(
      LayerTreeFrameSink* layer_tree_frame_sink) override;
  void ReleaseLayerTreeFrameSink() override;
  void SetVisible(bool visible) override;
  void SetShouldWarmUp() override;
  void SetNeedsAnimate() override;
  void SetNeedsUpdateLayers() override;
  void SetNeedsCommit() override;
  void SetNeedsRedraw(const gfx::Rect& damage_rect) override;
  void SetTargetLocalSurfaceId(
      const viz::LocalSurfaceId& target_local_surface_id) override;
  void DetachInputDelegateAndRenderFrameObserver() override;
  bool RequestedAnimatePending() override;
  void SetDeferMainFrameUpdate(bool defer_main_frame_update) override;
  void SetPauseRendering(bool pause_rendering) override;
  void SetInputResponsePending() override;
  bool StartDeferringCommits(base::TimeDelta timeout,
                             PaintHoldingReason reason) override;
  void StopDeferringCommits(PaintHoldingCommitTrigger) override;
  bool IsDeferringCommits() const override;
  bool CommitRequested() const override;
  void Start() override;
  void Stop() override;
  void QueueImageDecode(int request_id, const PaintImage& image) override;
  void SetMutator(std::unique_ptr<LayerTreeMutator> mutator) override;
  void SetPaintWorkletLayerPainter(
      std::unique_ptr<PaintWorkletLayerPainter> painter) override;
  bool MainFrameWillHappenForTesting() override;
  void RequestBeginMainFrameNotExpected(bool new_state) override;
  void SetSourceURL(ukm::SourceId source_id, const GURL& url) override;
  void SetUkmSmoothnessDestination(
      base::WritableSharedMemoryMapping ukm_smoothness_data) override;
  void SetRenderFrameObserver(
      std::unique_ptr<RenderFrameMetadataObserver> observer) override;
  void CompositeImmediatelyForTest(base::TimeTicks frame_begin_time,
                                   bool raster,
                                   base::OnceClosure callback) override;
  double GetPercentDroppedFrames() const override;

  void UpdateBrowserControlsState(
      BrowserControlsState constraints,
      BrowserControlsState current,
      bool animate,
      base::optional_ref<const BrowserControlsOffsetTagsInfo> offset_tags_info)
      override;

  // SchedulerClient implementation
  bool WillBeginImplFrame(const viz::BeginFrameArgs& args) override;
  void DidFinishImplFrame(
      const viz::BeginFrameArgs& last_activated_args) override;
  void DidNotProduceFrame(const viz::BeginFrameAck& ack,
                          FrameSkippedReason reason) override;
  void WillNotReceiveBeginFrame() override;
  void ScheduledActionSendBeginMainFrame(
      const viz::BeginFrameArgs& args) override;
  DrawResult ScheduledActionDrawIfPossible() override;
  DrawResult ScheduledActionDrawForced() override;
  void ScheduledActionUpdateDisplayTree() override;
  void ScheduledActionCommit() override;
  void ScheduledActionPostCommit() override;
  void ScheduledActionActivateSyncTree() override;
  void ScheduledActionBeginLayerTreeFrameSinkCreation() override;
  void ScheduledActionPrepareTiles() override;
  void ScheduledActionInvalidateLayerTreeFrameSink(bool needs_redraw) override;
  void ScheduledActionPerformImplSideInvalidation() override;
  void SendBeginMainFrameNotExpectedSoon() override;
  void ScheduledActionBeginMainFrameNotExpectedUntil(
      base::TimeTicks time) override;
  void FrameIntervalUpdated(base::TimeDelta interval) override;
  void OnBeginImplFrameDeadline() override;

  // LayerTreeHostImplClient implementation
  void DidLoseLayerTreeFrameSinkOnImplThread() override;
  void SetBeginFrameSource(viz::BeginFrameSource* source) override;
  void DidReceiveCompositorFrameAckOnImplThread() override;
  void OnCanDrawStateChanged(bool can_draw) override;
  void NotifyReadyToActivate() override;
  bool IsReadyToActivate() override;
  void NotifyReadyToDraw() override;
  void SetNeedsRedrawOnImplThread() override;
  void SetNeedsOneBeginImplFrameOnImplThread() override;
  void SetNeedsUpdateDisplayTreeOnImplThread() override {}
  void SetNeedsPrepareTilesOnImplThread() override;
  void SetNeedsCommitOnImplThread() override;
  void SetVideoNeedsBeginFrames(bool needs_begin_frames) override;
  void SetDeferBeginMainFrameFromImpl(bool defer_begin_main_frame) override {}
  bool IsInsideDraw() override;
  void RenewTreePriority() override;
  void PostDelayedAnimationTaskOnImplThread(base::OnceClosure task,
                                            base::TimeDelta delay) override;
  void DidActivateSyncTree() override;
  void DidPrepareTiles() override;
  void DidCompletePageScaleAnimationOnImplThread() override;
  void OnDrawForLayerTreeFrameSink(bool resourceless_software_draw,
                                   bool skip_draw) override;
  void SetNeedsImplSideInvalidation(
      bool needs_first_draw_on_activation) override;
  void NotifyImageDecodeRequestFinished(int request_id,
                                        bool decode_succeeded) override;
  void NotifyTransitionRequestFinished(uint32_t sequence_id) override;
  void DidPresentCompositorFrameOnImplThread(
      uint32_t frame_token,
      PresentationTimeCallbackBuffer::PendingCallbacks callbacks,
      const viz::FrameTimingDetails& details) override;
  void NotifyAnimationWorkletStateChange(
      AnimationWorkletMutationState state,
      ElementListType element_list_type) override;
  void NotifyPaintWorkletStateChange(
      Scheduler::PaintWorkletState state) override;
  void NotifyThroughputTrackerResults(CustomTrackerResults results) override;
  bool IsInSynchronousComposite() const override;
  void FrameSinksToThrottleUpdated(
      const base::flat_set<viz::FrameSinkId>& ids) override;
  void ClearHistory() override;
  void SetHasActiveThreadedScroll(bool is_scrolling) override;
  void SetWaitingForScrollEvent(bool waiting_for_scroll_event) override;

  size_t CommitDurationSampleCountForTesting() const override;

  void RequestNewLayerTreeFrameSink();

  void DidObserveFirstScrollDelay(
      int source_frame_number,
      base::TimeDelta first_scroll_delay,
      base::TimeTicks first_scroll_timestamp) override;

  LayerTreeHostImpl* LayerTreeHostImplForTesting() const {
    return host_impl_.get();
  }

  viz::BeginFrameArgs BeginImplFrameForTest(base::TimeTicks frame_begin_time);

 protected:
  SingleThreadProxy(LayerTreeHost* layer_tree_host,
                    LayerTreeHostSingleThreadClient* client,
                    TaskRunnerProvider* task_runner_provider);

 private:
  void BeginMainFrame(const viz::BeginFrameArgs& begin_frame_args);
  void BeginMainFrameAbortedOnImplThread(CommitEarlyOutReason reason);
  void DoBeginMainFrame(const viz::BeginFrameArgs& begin_frame_args);
  void DoPainting(const viz::BeginFrameArgs& commit_args);
  void DoCommit(const viz::BeginFrameArgs& commit_args);
  void DoPostCommit();
  DrawResult DoComposite(LayerTreeHostImpl::FrameData* frame);
  void DoSwap();
  void DidCommitAndDrawFrame(int source_frame_number);
  void CommitComplete();

  bool ShouldComposite() const;
  void ScheduleRequestNewLayerTreeFrameSink();
  void IssueImageDecodeFinishedCallbacks();

  void DidReceiveCompositorFrameAck();

  // Accessed on main thread only.
  raw_ptr<LayerTreeHost> layer_tree_host_;
  raw_ptr<LayerTreeHostSingleThreadClient> single_thread_client_;

  raw_ptr<TaskRunnerProvider> task_runner_provider_;

  // Used on the Thread, but checked on main thread during
  // initialization/shutdown.
  std::unique_ptr<LayerTreeHostImpl> host_impl_;

  // Accessed from both threads.
  std::unique_ptr<Scheduler> scheduler_on_impl_thread_;

  // Only used when defer_commits_ is active and must be set in such cases.
  base::TimeTicks commits_restart_time_;

  bool next_frame_is_newly_committed_frame_;

#if DCHECK_IS_ON()
  bool inside_impl_frame_;
#endif
  bool inside_draw_;
  bool defer_main_frame_update_;
  bool pause_rendering_;
  std::optional<PaintHoldingReason> paint_holding_reason_;
  bool did_apply_compositor_deltas_ = false;
  bool animate_requested_;
  bool update_layers_requested_;
  bool commit_requested_;
  bool inside_synchronous_composite_;
  bool needs_impl_frame_;

  // True if a request to the LayerTreeHostClient to create an output surface
  // is still outstanding.
  bool layer_tree_frame_sink_creation_requested_;
  // When output surface is lost, is set to true until a new output surface is
  // initialized.
  bool layer_tree_frame_sink_lost_;

  // A number that kept incrementing in CompositeImmediately, which indicates a
  // new impl frame.
  uint64_t begin_frame_sequence_number_ = 1u;

  int source_frame_number_for_next_commit_ = kInvalidSourceFrameNumber;

  // This is the callback for the scheduled RequestNewLayerTreeFrameSink.
  base::CancelableOnceClosure layer_tree_frame_sink_creation_callback_;

  base::WeakPtr<SingleThreadProxy> frame_sink_bound_weak_ptr_;

  // WeakPtrs generated by this factory will be invalidated when
  // LayerTreeFrameSink is released.
  base::WeakPtrFactory<SingleThreadProxy> frame_sink_bound_weak_factory_{this};

  base::WeakPtrFactory<SingleThreadProxy> weak_factory_{this};
};

// For use in the single-threaded case. In debug builds, it pretends that the
// code is running on the impl thread to satisfy assertion checks.
class DebugScopedSetImplThread {
 public:
#if DCHECK_IS_ON()
  explicit DebugScopedSetImplThread(TaskRunnerProvider* task_runner_provider)
      : task_runner_provider_(task_runner_provider) {
    previous_value_ = task_runner_provider_->impl_thread_is_overridden_;
    // Some test code will used this in both single- and multi-threaded mode;
    // we should just ignore it in multi-threaded mode.
    if (!task_runner_provider_->HasImplThread())
      task_runner_provider_->SetCurrentThreadIsImplThread(true);
  }
#else
  explicit DebugScopedSetImplThread(TaskRunnerProvider* task_runner_provider) {}
#endif

  DebugScopedSetImplThread(const DebugScopedSetImplThread&) = delete;

  ~DebugScopedSetImplThread() {
#if DCHECK_IS_ON()
    if (!task_runner_provider_->HasImplThread())
      task_runner_provider_->SetCurrentThreadIsImplThread(previous_value_);
#endif
  }

  DebugScopedSetImplThread& operator=(const DebugScopedSetImplThread&) = delete;

#if DCHECK_IS_ON()

 private:
  bool previous_value_;
  raw_ptr<TaskRunnerProvider> task_runner_provider_;
#endif
};

// For use in the single-threaded case. In debug builds, it pretends that the
// code is running on the main thread to satisfy assertion checks.
class DebugScopedSetMainThread {
 public:
#if DCHECK_IS_ON()
  explicit DebugScopedSetMainThread(TaskRunnerProvider* task_runner_provider)
      : task_runner_provider_(task_runner_provider) {
    previous_value_ = task_runner_provider_->impl_thread_is_overridden_;
    if (!task_runner_provider_->HasImplThread())
      task_runner_provider_->SetCurrentThreadIsImplThread(false);
  }
#else
  explicit DebugScopedSetMainThread(TaskRunnerProvider* task_runner_provider) {}
#endif

  DebugScopedSetMainThread(const DebugScopedSetMainThread&) = delete;

  ~DebugScopedSetMainThread() {
#if DCHECK_IS_ON()
    if (!task_runner_provider_->HasImplThread())
      task_runner_provider_->SetCurrentThreadIsImplThread(previous_value_);
#endif
  }

  DebugScopedSetMainThread& operator=(const DebugScopedSetMainThread&) = delete;

#if DCHECK_IS_ON()

 private:
  bool previous_value_;
  raw_ptr<TaskRunnerProvider> task_runner_provider_;
#endif
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
  DebugScopedSetImplThreadAndMainThreadBlocked(
      const DebugScopedSetImplThreadAndMainThreadBlocked&) = delete;
  DebugScopedSetImplThreadAndMainThreadBlocked& operator=(
      const DebugScopedSetImplThreadAndMainThreadBlocked&) = delete;

 private:
  DebugScopedSetImplThread impl_thread_;
  DebugScopedSetMainThreadBlocked main_thread_blocked_;
};

}  // namespace cc

#endif  // CC_TREES_SINGLE_THREAD_PROXY_H_
