// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_PROXY_IMPL_H_
#define CC_TREES_PROXY_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "cc/base/completion_event.h"
#include "cc/base/delayed_unique_notifier.h"
#include "cc/input/browser_controls_state.h"
#include "cc/scheduler/scheduler.h"
#include "cc/trees/layer_tree_host_impl.h"

namespace cc {
class LayerTreeHost;
class ProxyMain;
class RenderFrameMetadataObserver;

// This class aggregates all the interactions that the main side of the
// compositor needs to have with the impl side.
// The class is created and lives on the impl thread.
class CC_EXPORT ProxyImpl : public LayerTreeHostImplClient,
                            public SchedulerClient {
 public:
  ProxyImpl(base::WeakPtr<ProxyMain> proxy_main_weak_ptr,
            LayerTreeHost* layer_tree_host,
            TaskRunnerProvider* task_runner_provider);
  ~ProxyImpl() override;

  void UpdateBrowserControlsStateOnImpl(BrowserControlsState constraints,
                                        BrowserControlsState current,
                                        bool animate);
  void InitializeLayerTreeFrameSinkOnImpl(
      LayerTreeFrameSink* layer_tree_frame_sink,
      base::WeakPtr<ProxyMain> proxy_main_frame_sink_bound_weak_ptr);
  void InitializeMutatorOnImpl(std::unique_ptr<LayerTreeMutator> mutator);
  void SetInputThrottledUntilCommitOnImpl(bool is_throttled);
  void SetDeferCommitsOnImpl(bool defer_commits) const;
  void SetNeedsRedrawOnImpl(const gfx::Rect& damage_rect);
  void SetNeedsCommitOnImpl();
  void BeginMainFrameAbortedOnImpl(
      CommitEarlyOutReason reason,
      base::TimeTicks main_thread_start_time,
      std::vector<std::unique_ptr<SwapPromise>> swap_promises);
  void SetVisibleOnImpl(bool visible);
  void ReleaseLayerTreeFrameSinkOnImpl(CompletionEvent* completion);
  void FinishGLOnImpl(CompletionEvent* completion);
  void NotifyReadyToCommitOnImpl(CompletionEvent* completion,
                                 LayerTreeHost* layer_tree_host,
                                 base::TimeTicks main_thread_start_time,
                                 bool hold_commit_for_activation);
  void SetURLForUkm(const GURL& url);
  void ClearHistory();
  void SetRenderFrameObserver(
      std::unique_ptr<RenderFrameMetadataObserver> observer);

  void MainFrameWillHappenOnImplForTesting(CompletionEvent* completion,
                                           bool* main_frame_will_happen);

  void RequestBeginMainFrameNotExpected(bool new_state) override;

 private:
  // The members of this struct should be accessed on the impl thread only when
  // the main thread is blocked for a commit.
  struct BlockedMainCommitOnly {
    BlockedMainCommitOnly();
    ~BlockedMainCommitOnly();
    LayerTreeHost* layer_tree_host;
  };

  // LayerTreeHostImplClient implementation
  void DidLoseLayerTreeFrameSinkOnImplThread() override;
  void SetBeginFrameSource(viz::BeginFrameSource* source) override;
  void DidReceiveCompositorFrameAckOnImplThread() override;
  void OnCanDrawStateChanged(bool can_draw) override;
  void NotifyReadyToActivate() override;
  void NotifyReadyToDraw() override;
  // Please call these 2 functions through
  // LayerTreeHostImpl's SetNeedsRedraw() and SetNeedsOneBeginImplFrame().
  void SetNeedsRedrawOnImplThread() override;
  void SetNeedsOneBeginImplFrameOnImplThread() override;
  void SetNeedsPrepareTilesOnImplThread() override;
  void SetNeedsCommitOnImplThread() override;
  void SetVideoNeedsBeginFrames(bool needs_begin_frames) override;
  void PostAnimationEventsToMainThreadOnImplThread(
      std::unique_ptr<MutatorEvents> events) override;
  bool IsInsideDraw() override;
  void RenewTreePriority() override;
  void PostDelayedAnimationTaskOnImplThread(const base::Closure& task,
                                            base::TimeDelta delay) override;
  void DidActivateSyncTree() override;
  void WillPrepareTiles() override;
  void DidPrepareTiles() override;
  void DidCompletePageScaleAnimationOnImplThread() override;
  void OnDrawForLayerTreeFrameSink(bool resourceless_software_draw,
                                   bool skip_draw) override;
  void NeedsImplSideInvalidation(bool needs_first_draw_on_activation) override;
  void NotifyImageDecodeRequestFinished() override;
  void DidPresentCompositorFrameOnImplThread(
      uint32_t frame_token,
      std::vector<LayerTreeHost::PresentationTimeCallback> callbacks,
      const gfx::PresentationFeedback& feedback) override;

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

  DrawResult DrawInternal(bool forced_draw);

  bool IsImplThread() const;
  bool IsMainThreadBlocked() const;
  base::SingleThreadTaskRunner* MainThreadTaskRunner();

  const int layer_tree_host_id_;

  std::unique_ptr<Scheduler> scheduler_;

  // Set when the main thread is waiting on a pending tree activation.
  bool commit_completion_waits_for_activation_;

  // Set when the main thread is waiting on a commit to complete.
  CompletionEvent* commit_completion_event_;

  // Set when the main thread is waiting for activation to complete.
  CompletionEvent* activation_completion_event_;

  // Set when the next draw should post DidCommitAndDrawFrame to the main
  // thread.
  bool next_frame_is_newly_committed_frame_;

  bool inside_draw_;
  bool input_throttled_until_commit_;

  bool send_compositor_frame_ack_;

  TaskRunnerProvider* task_runner_provider_;

  DelayedUniqueNotifier smoothness_priority_expiration_notifier_;

  RenderingStatsInstrumentation* rendering_stats_instrumentation_;

  std::unique_ptr<LayerTreeHostImpl> host_impl_;

  // Use accessors instead of this variable directly.
  BlockedMainCommitOnly main_thread_blocked_commit_vars_unsafe_;
  BlockedMainCommitOnly& blocked_main_commit();

  // Used to post tasks to ProxyMain on the main thread.
  base::WeakPtr<ProxyMain> proxy_main_weak_ptr_;

  // A weak pointer to ProxyMain that is invalidated when LayerTreeFrameSink is
  // released.
  base::WeakPtr<ProxyMain> proxy_main_frame_sink_bound_weak_ptr_;

  DISALLOW_COPY_AND_ASSIGN(ProxyImpl);
};

}  // namespace cc

#endif  // CC_TREES_PROXY_IMPL_H_
