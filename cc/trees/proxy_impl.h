// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_PROXY_IMPL_H_
#define CC_TREES_PROXY_IMPL_H_

#include <memory>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/types/optional_ref.h"
#include "cc/base/completion_event.h"
#include "cc/base/delayed_unique_notifier.h"
#include "cc/input/browser_controls_offset_tags_info.h"
#include "cc/input/browser_controls_state.h"
#include "cc/scheduler/scheduler.h"
#include "cc/trees/layer_tree_host_impl.h"

namespace viz {
struct FrameTimingDetails;
}

namespace cc {
class LayerTreeHost;
class ProxyMain;
class RenderFrameMetadataObserver;
class ScopedCommitCompletionEvent;

// This class aggregates all the interactions that the main side of the
// compositor needs to have with the impl side.
// The class is created and lives on the impl thread.
class CC_EXPORT ProxyImpl : public LayerTreeHostImplClient,
                            public SchedulerClient {
 public:
  ProxyImpl(base::WeakPtr<ProxyMain> proxy_main_weak_ptr,
            LayerTreeHost* layer_tree_host,
            int id,
            const LayerTreeSettings* settings,
            RenderingStatsInstrumentation* rendering_stats_instrumentation,
            TaskRunnerProvider* task_runner_provider);
  ProxyImpl(const ProxyImpl&) = delete;
  ~ProxyImpl() override;

  ProxyImpl& operator=(const ProxyImpl&) = delete;

  void UpdateBrowserControlsStateOnImpl(
      BrowserControlsState constraints,
      BrowserControlsState current,
      bool animate,
      base::optional_ref<const BrowserControlsOffsetTagsInfo> offset_tags_info);
  void InitializeLayerTreeFrameSinkOnImpl(
      LayerTreeFrameSink* layer_tree_frame_sink,
      base::WeakPtr<ProxyMain> proxy_main_frame_sink_bound_weak_ptr);
  void InitializeMutatorOnImpl(std::unique_ptr<LayerTreeMutator> mutator);
  void InitializePaintWorkletLayerPainterOnImpl(
      std::unique_ptr<PaintWorkletLayerPainter> painter);
  void SetDeferBeginMainFrameFromMain(bool defer_begin_main_frame);
  void SetPauseRendering(bool pause_rendering);
  void SetNeedsRedrawOnImpl(const gfx::Rect& damage_rect);
  void SetNeedsCommitOnImpl();
  void SetTargetLocalSurfaceIdOnImpl(
      const viz::LocalSurfaceId& target_local_surface_id);
  void BeginMainFrameAbortedOnImpl(
      CommitEarlyOutReason reason,
      base::TimeTicks main_thread_start_time,
      std::vector<std::unique_ptr<SwapPromise>> swap_promises,
      bool scroll_and_viewport_changes_synced);
  void SetVisibleOnImpl(bool visible);
  void SetShouldWarmUpOnImpl();
  void ReleaseLayerTreeFrameSinkOnImpl(CompletionEvent* completion);
  void FinishGLOnImpl(CompletionEvent* completion);
  void NotifyReadyToCommitOnImpl(CompletionEvent* completion_event,
                                 std::unique_ptr<CommitState> commit_state,
                                 const ThreadUnsafeCommitState* unsafe_state,
                                 base::TimeTicks main_thread_start_time,
                                 const viz::BeginFrameArgs& commit_args,
                                 bool scroll_and_viewport_changes_synced,
                                 CommitTimestamps* commit_timestamps,
                                 bool commit_timeout = false);
  void QueueImageDecodeOnImpl(int request_id,
                              std::unique_ptr<PaintImage> image);
  void SetSourceURL(ukm::SourceId source_id, const GURL& url);
  void SetUkmSmoothnessDestination(
      base::WritableSharedMemoryMapping ukm_smoothness_data);
  void SetRenderFrameObserver(
      std::unique_ptr<RenderFrameMetadataObserver> observer);
  void DetachInputDelegateAndRenderFrameObserver(
      CompletionEvent* completion_event);

  void MainFrameWillHappenOnImplForTesting(CompletionEvent* completion,
                                           bool* main_frame_will_happen);
  void RequestBeginMainFrameNotExpectedOnImpl(bool new_state);

  void ClearHistory() override;
  size_t CommitDurationSampleCountForTesting() const override;
  const DelayedUniqueNotifier& SmoothnessPriorityExpirationNotifierForTesting()
      const {
    return smoothness_priority_expiration_notifier_;
  }

 private:
  // LayerTreeHostImplClient implementation
  void DidLoseLayerTreeFrameSinkOnImplThread() override;
  void SetBeginFrameSource(viz::BeginFrameSource* source) override;
  void DidReceiveCompositorFrameAckOnImplThread() override;
  void OnCanDrawStateChanged(bool can_draw) override;
  void NotifyReadyToActivate() override;
  bool IsReadyToActivate() override;
  void NotifyReadyToDraw() override;
  // Please call these 2 functions through
  // LayerTreeHostImpl's SetNeedsRedraw() and SetNeedsOneBeginImplFrame().
  void SetNeedsRedrawOnImplThread() override;
  void SetNeedsOneBeginImplFrameOnImplThread() override;
  void SetNeedsUpdateDisplayTreeOnImplThread() override;
  void SetNeedsPrepareTilesOnImplThread() override;
  void SetNeedsCommitOnImplThread() override;
  void SetVideoNeedsBeginFrames(bool needs_begin_frames) override;
  void SetDeferBeginMainFrameFromImpl(bool defer_begin_main_frame) override;
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
      PresentationTimeCallbackBuffer::PendingCallbacks activated,
      const viz::FrameTimingDetails& details) override;
  void NotifyAnimationWorkletStateChange(
      AnimationWorkletMutationState state,
      ElementListType element_list_type) override;
  void NotifyPaintWorkletStateChange(
      Scheduler::PaintWorkletState state) override;
  void NotifyThroughputTrackerResults(CustomTrackerResults results) override;
  void DidObserveFirstScrollDelay(
      int source_frame_number,
      base::TimeDelta first_scroll_delay,
      base::TimeTicks first_scroll_timestamp) override;
  bool IsInSynchronousComposite() const override;
  void FrameSinksToThrottleUpdated(
      const base::flat_set<viz::FrameSinkId>& id) override;
  void SetHasActiveThreadedScroll(bool is_scrolling) override;
  void SetWaitingForScrollEvent(bool waiting_for_scroll_event) override;

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
  void FrameIntervalUpdated(base::TimeDelta interval) override {}
  void OnBeginImplFrameDeadline() override;

  DrawResult DrawInternal(bool forced_draw);

  bool IsImplThread() const;
  bool IsMainThreadBlocked() const;
  base::SingleThreadTaskRunner* MainThreadTaskRunner();
  bool ShouldDeferBeginMainFrame() const;

  const int layer_tree_host_id_;

  std::unique_ptr<Scheduler> scheduler_;

  struct DataForCommit {
    DataForCommit(
        std::unique_ptr<ScopedCommitCompletionEvent> commit_completion_event,
        std::unique_ptr<CommitState> commit_state,
        const ThreadUnsafeCommitState* unsafe_state,
        CommitTimestamps* commit_timestamps);

    ~DataForCommit();

    bool IsValid() const;

    // Set when the main thread is waiting on a commit to complete.
    std::unique_ptr<ScopedCommitCompletionEvent> commit_completion_event;
    std::unique_ptr<CommitState> commit_state;
    raw_ptr<const ThreadUnsafeCommitState> unsafe_state;
    // This is passed from the main thread so the impl thread can record
    // timestamps at the beginning and end of commit.
    raw_ptr<CommitTimestamps> commit_timestamps = nullptr;
  };

  std::unique_ptr<DataForCommit> data_for_commit_;

  // Set when the main thread is waiting for activation to complete.
  std::unique_ptr<ScopedCommitCompletionEvent> activation_completion_event_;

  // Set when the next draw should post DidCommitAndDrawFrame to the main
  // thread.
  bool next_frame_is_newly_committed_frame_;

  bool inside_draw_;

  raw_ptr<TaskRunnerProvider> task_runner_provider_;

  DelayedUniqueNotifier smoothness_priority_expiration_notifier_;

  std::unique_ptr<LayerTreeHostImpl> host_impl_;

  // Used to post tasks to ProxyMain on the main thread.
  base::WeakPtr<ProxyMain> proxy_main_weak_ptr_;

  // A weak pointer to ProxyMain that is invalidated when LayerTreeFrameSink is
  // released.
  base::WeakPtr<ProxyMain> proxy_main_frame_sink_bound_weak_ptr_;

  // Either thread can request deferring BeginMainFrame; keep track of both.
  bool main_wants_defer_begin_main_frame_ = false;
  bool impl_wants_defer_begin_main_frame_ = false;
};

}  // namespace cc

#endif  // CC_TREES_PROXY_IMPL_H_
