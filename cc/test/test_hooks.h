// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_TEST_HOOKS_H_
#define CC_TEST_TEST_HOOKS_H_

#include <memory>

#include "cc/animation/animation_delegate.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "components/viz/common/quads/aggregated_render_pass.h"
#include "ui/gfx/animation/keyframe/animation_curve.h"

namespace gfx {
struct PresentationFeedback;
}

namespace viz {
class CompositorFrame;
class DisplayCompositorMemoryAndTaskController;
class OutputSurface;
class SkiaOutputSurface;
}

namespace cc {

struct ApplyViewportChangesArgs;
struct BeginMainFrameMetrics;

// Used by test stubs to notify the test when something interesting happens.
class TestHooks : public AnimationDelegate {
 public:
  TestHooks();
  ~TestHooks() override;

  // Compositor thread hooks.
  virtual std::unique_ptr<RasterBufferProvider> CreateRasterBufferProvider(
      LayerTreeHostImpl* host_impl);
  virtual void WillBeginImplFrameOnThread(LayerTreeHostImpl* host_impl,
                                          const viz::BeginFrameArgs& args,
                                          bool has_damage) {}
  virtual void DidFinishImplFrameOnThread(LayerTreeHostImpl* host_impl) {}
  virtual void WillSendBeginMainFrameOnThread(LayerTreeHostImpl* host_impl) {}
  virtual void BeginMainFrameAbortedOnThread(
      LayerTreeHostImpl* host_impl,
      CommitEarlyOutReason reason,
      bool scroll_and_viewport_changes_synced) {}
  virtual void ReadyToCommitOnThread(LayerTreeHostImpl* host_impl) {}
  virtual void WillPrepareTilesOnThread(LayerTreeHostImpl* host_impl) {}
  virtual void BeginCommitOnThread(LayerTreeHostImpl* host_impl) {}
  virtual void WillCommitCompleteOnThread(LayerTreeHostImpl* host_impl) {}
  virtual void CommitCompleteOnThread(LayerTreeHostImpl* host_impl) {}
  virtual void WillActivateTreeOnThread(LayerTreeHostImpl* host_impl) {}
  virtual void DidActivateTreeOnThread(LayerTreeHostImpl* host_impl) {}
  virtual void InitializedRendererOnThread(LayerTreeHostImpl* host_impl,
                                           bool success) {}
  virtual void WillPrepareToDrawOnThread(LayerTreeHostImpl* host_impl) {}
  virtual DrawResult PrepareToDrawOnThread(
      LayerTreeHostImpl* host_impl,
      LayerTreeHostImpl::FrameData* frame_data,
      DrawResult draw_result);
  virtual void DrawLayersOnThread(LayerTreeHostImpl* host_impl) {}
  virtual void WillSubmitCompositorFrame(LayerTreeHostImpl* host_impl,
                                         const viz::CompositorFrame& frame) {}
  virtual void WillNotifyReadyToActivateOnThread(LayerTreeHostImpl* host_impl) {
  }
  virtual void NotifyReadyToActivateOnThread(LayerTreeHostImpl* host_impl) {}
  virtual void NotifyReadyToDrawOnThread(LayerTreeHostImpl* host_impl) {}
  virtual void NotifyAllTileTasksCompleted(LayerTreeHostImpl* host_impl) {}
  virtual void NotifyTileStateChangedOnThread(LayerTreeHostImpl* host_impl,
                                              const Tile* tile) {}
  virtual void DidRunBeginMainFrame() {}
  virtual void DidReceivePresentationTimeOnThread(
      LayerTreeHostImpl* host_impl,
      uint32_t frame_token,
      const gfx::PresentationFeedback& feedback) {}
  virtual void DidSetVisibleOnImplTree(LayerTreeHostImpl* host_impl,
                                       bool visible) {}
  virtual void AnimateLayers(LayerTreeHostImpl* host_impl,
                             base::TimeTicks monotonic_time) {}
  virtual void UpdateAnimationState(LayerTreeHostImpl* host_impl,
                                    bool has_unfinished_animation) {}
  virtual void WillAnimateLayers(LayerTreeHostImpl* host_impl,
                                 base::TimeTicks monotonic_time) {}
  virtual void DidInvalidateContentOnImplSide(LayerTreeHostImpl* host_impl) {}
  virtual void DidInvalidateLayerTreeFrameSink(LayerTreeHostImpl* host_impl) {}
  virtual void DidReceiveImplSideInvalidationRequest(
      LayerTreeHostImpl* host_impl) {}
  virtual void DidRequestImplSideInvalidation(LayerTreeHostImpl* host_impl) {}

  // Asynchronous compositor thread hooks.
  // These are called asynchronously from the LayerTreeHostImpl performing its
  // draw, so you should record state you want to use here in
  // DrawLayersOnThread() instead. For that reason these methods do not receive
  // a LayerTreeHostImpl pointer.
  virtual void DisplayReceivedLocalSurfaceIdOnThread(
      const viz::LocalSurfaceId& local_surface_id) {}
  virtual void DisplayReceivedCompositorFrameOnThread(
      const viz::CompositorFrame& frame) {}
  virtual void DisplayWillDrawAndSwapOnThread(
      bool will_draw_and_swap,
      const viz::AggregatedRenderPassList& render_passes) {}
  virtual void DisplayDidDrawAndSwapOnThread() {}

  // Main thread hooks.
  virtual void ApplyViewportChanges(const ApplyViewportChangesArgs& args) {}
  virtual void BeginMainFrameNotExpectedSoon() {}
  virtual void WillApplyCompositorChanges() {}
  virtual void BeginMainFrame(const viz::BeginFrameArgs& args) {}
  virtual void WillBeginMainFrame() {}
  virtual void DidBeginMainFrame() {}
  virtual void UpdateLayerTreeHost() {}
  virtual void DidInitializeLayerTreeFrameSink() {}
  virtual void DidFailToInitializeLayerTreeFrameSink() {}
  virtual void DidAddAnimation() {}
  virtual void WillCommit(const CommitState&) {}
  virtual void DidCommit() {}
  virtual void DidCommitAndDrawFrame() {}
  virtual void DidActivateSyncTree() {}
  virtual void NotifyThroughputTrackerResults(CustomTrackerResults results) {}
  virtual std::unique_ptr<BeginMainFrameMetrics> GetBeginMainFrameMetrics();
  virtual void DidPresentCompositorFrame(
      uint32_t frame_token,
      const viz::FrameTimingDetails& frame_timing_details) {}

  // AnimationDelegate implementation.
  void NotifyAnimationStarted(base::TimeTicks monotonic_time,
                              int target_property,
                              int group) override {}
  void NotifyAnimationFinished(base::TimeTicks monotonic_time,
                               int target_property,
                               int group) override {}
  void NotifyAnimationAborted(base::TimeTicks monotonic_time,
                              int target_property,
                              int group) override {}
  void NotifyAnimationTakeover(
      base::TimeTicks monotonic_time,
      int target_property,
      base::TimeTicks animation_start_time,
      std::unique_ptr<gfx::AnimationCurve> curve) override {}
  void NotifyLocalTimeUpdated(
      std::optional<base::TimeDelta> local_time) override {}

  // OutputSurface indirections to the LayerTreeTest, that can be further
  // overridden.
  virtual void RequestNewLayerTreeFrameSink() = 0;
  virtual std::unique_ptr<viz::DisplayCompositorMemoryAndTaskController>
  CreateDisplayControllerOnThread() = 0;
  virtual std::unique_ptr<viz::SkiaOutputSurface>
  CreateSkiaOutputSurfaceOnThread(
      viz::DisplayCompositorMemoryAndTaskController*) = 0;
  virtual std::unique_ptr<viz::OutputSurface>
  CreateSoftwareOutputSurfaceOnThread() = 0;
};

}  // namespace cc

#endif  // CC_TEST_TEST_HOOKS_H_
