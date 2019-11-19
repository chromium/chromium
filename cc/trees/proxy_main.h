// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_PROXY_MAIN_H_
#define CC_TREES_PROXY_MAIN_H_

#include "cc/cc_export.h"
#include "cc/input/browser_controls_state.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/proxy.h"
#include "cc/trees/proxy_common.h"

namespace cc {

class MutatorEvents;
class CompletionEvent;
class LayerTreeFrameSink;
class LayerTreeHost;
class LayerTreeMutator;
class PaintWorkletLayerPainter;
class ProxyImpl;
class RenderFrameMetadataObserver;

// This class aggregates all interactions that the impl side of the compositor
// needs to have with the main side.
// The class is created and lives on the main thread.
class CC_EXPORT ProxyMain : public Proxy {
 public:
  ProxyMain(LayerTreeHost* layer_tree_host,
            TaskRunnerProvider* task_runner_provider);
  ProxyMain(const ProxyMain&) = delete;
  ~ProxyMain() override;

  ProxyMain& operator=(const ProxyMain&) = delete;

  // Commits between the main and impl threads are processed through a pipeline
  // with the following stages. For efficiency we can early out at any stage if
  // we decide that no further processing is necessary.
  enum CommitPipelineStage {
    NO_PIPELINE_STAGE,
    ANIMATE_PIPELINE_STAGE,
    UPDATE_LAYERS_PIPELINE_STAGE,
    COMMIT_PIPELINE_STAGE,
  };

  void DidReceiveCompositorFrameAck();
  void BeginMainFrameNotExpectedSoon();
  void BeginMainFrameNotExpectedUntil(base::TimeTicks time);
  void DidCommitAndDrawFrame();
  void SetAnimationEvents(std::unique_ptr<MutatorEvents> events);
  void DidLoseLayerTreeFrameSink();
  void RequestNewLayerTreeFrameSink();
  void DidInitializeLayerTreeFrameSink(bool success);
  void DidCompletePageScaleAnimation();
  void BeginMainFrame(
      std::unique_ptr<BeginMainFrameAndCommitState> begin_main_frame_state);
  void DidPresentCompositorFrame(
      uint32_t frame_token,
      std::vector<LayerTreeHost::PresentationTimeCallback> callbacks,
      const gfx::PresentationFeedback& feedback);

  CommitPipelineStage max_requested_pipeline_stage() const {
    return max_requested_pipeline_stage_;
  }
  CommitPipelineStage current_pipeline_stage() const {
    return current_pipeline_stage_;
  }
  CommitPipelineStage final_pipeline_stage() const {
    return final_pipeline_stage_;
  }

 private:
  // Proxy implementation.
  bool IsStarted() const override;
  void SetLayerTreeFrameSink(
      LayerTreeFrameSink* layer_tree_frame_sink) override;
  void SetVisible(bool visible) override;
  void SetNeedsAnimate() override;
  void SetNeedsUpdateLayers() override;
  void SetNeedsCommit() override;
  void SetNeedsRedraw(const gfx::Rect& damage_rect) override;
  void SetNextCommitWaitsForActivation() override;
  bool RequestedAnimatePending() override;
  void SetDeferMainFrameUpdate(bool defer_main_frame_update) override;
  void StartDeferringCommits(base::TimeDelta timeout) override;
  void StopDeferringCommits(PaintHoldingCommitTrigger) override;
  bool CommitRequested() const override;
  void Start() override;
  void Stop() override;
  bool SupportsImplScrolling() const override;
  void SetMutator(std::unique_ptr<LayerTreeMutator> mutator) override;
  void SetPaintWorkletLayerPainter(
      std::unique_ptr<PaintWorkletLayerPainter> painter) override;
  bool MainFrameWillHappenForTesting() override;
  void ReleaseLayerTreeFrameSink() override;
  void UpdateBrowserControlsState(BrowserControlsState constraints,
                                  BrowserControlsState current,
                                  bool animate) override;
  void RequestBeginMainFrameNotExpected(bool new_state) override;
  void SetSourceURL(ukm::SourceId source_id, const GURL& url) override;
  void ClearHistory() override;
  void SetRenderFrameObserver(
      std::unique_ptr<RenderFrameMetadataObserver> observer) override;

  // Returns |true| if the request was actually sent, |false| if one was
  // already outstanding.
  bool SendCommitRequestToImplThreadIfNeeded(
      CommitPipelineStage required_stage);
  bool IsMainThread() const;
  bool IsImplThread() const;
  base::SingleThreadTaskRunner* ImplThreadTaskRunner();

  void InitializeOnImplThread(CompletionEvent* completion_event);
  void DestroyProxyImplOnImplThread(CompletionEvent* completion_event);

  LayerTreeHost* layer_tree_host_;

  TaskRunnerProvider* task_runner_provider_;

  const int layer_tree_host_id_;

  // The furthest pipeline stage which has been requested for the next
  // commit.
  CommitPipelineStage max_requested_pipeline_stage_;
  // The commit pipeline stage that is currently being processed.
  CommitPipelineStage current_pipeline_stage_;
  // The commit pipeline stage at which processing for the current commit
  // will stop. Only valid while we are executing the pipeline (i.e.,
  // |current_pipeline_stage| is set to a pipeline stage).
  CommitPipelineStage final_pipeline_stage_;
  // The final_pipeline_stage_ that was requested before the last commit was
  // deferred.
  CommitPipelineStage deferred_final_pipeline_stage_;

  bool commit_waits_for_activation_;

  // Set when the Proxy is started using Proxy::Start() and reset when it is
  // stopped using Proxy::Stop().
  bool started_;

  // defer_main_frame_update_ will also cause commits to be deferred, regardless
  // of the setting for defer_commits_.
  bool defer_main_frame_update_;
  bool defer_commits_;

  // Only used when defer_commits_ is active and must be set in such cases.
  base::TimeTicks commits_restart_time_;

  // ProxyImpl is created and destroyed on the impl thread, and should only be
  // accessed on the impl thread.
  // It is safe to use base::Unretained to post tasks to ProxyImpl on the impl
  // thread, since we control its lifetime. Any tasks posted to it are bound to
  // run before we destroy it on the impl thread.
  std::unique_ptr<ProxyImpl> proxy_impl_;

  // WeakPtrs generated by this factory will be invalidated when
  // LayerTreeFrameSink is released.
  base::WeakPtrFactory<ProxyMain> frame_sink_bound_weak_factory_{this};

  base::WeakPtrFactory<ProxyMain> weak_factory_{this};
};

}  // namespace cc

#endif  // CC_TREES_PROXY_MAIN_H_
