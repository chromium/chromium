// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SLIM_LAYER_TREE_CC_WRAPPER_H_
#define CC_SLIM_LAYER_TREE_CC_WRAPPER_H_

#include <memory>

#include "cc/slim/layer_tree.h"
#include "cc/trees/layer_tree_host_client.h"
#include "cc/trees/layer_tree_host_single_thread_client.h"

namespace cc {
class AnimationHost;
class LayerTreeHost;
}  // namespace cc

namespace cc::slim {

class Layer;

class LayerTreeCcWrapper : public LayerTree,
                           public cc::LayerTreeHostClient,
                           public cc::LayerTreeHostSingleThreadClient {
 public:
  ~LayerTreeCcWrapper() override;

  // LayerTree.
  cc::UIResourceManager* GetUIResourceManager() override;
  void SetViewportRectAndScale(
      const gfx::Rect& device_viewport_rect,
      float device_scale_factor,
      const viz::LocalSurfaceId& local_surface_id) override;
  void set_background_color(SkColor4f color) override;
  void SetVisible(bool visible) override;
  bool IsVisible() const override;
  using PresentationCallback =
      base::OnceCallback<void(const gfx::PresentationFeedback&)>;
  void RequestPresentationTimeForNextFrame(
      PresentationCallback callback) override;
  void RequestSuccessfulPresentationTimeForNextFrame(
      SuccessfulCallback callback) override;
  void set_display_transform_hint(gfx::OverlayTransform hint) override;
  void RequestCopyOfOutput(
      std::unique_ptr<viz::CopyOutputRequest> request) override;
  base::OnceClosure DeferBeginFrame() override;
  void UpdateTopControlsVisibleHeight(float height) override;
  void SetNeedsAnimate() override;
  void SetNeedsRedraw() override;
  void MaybeCompositeNow() override {}
  const scoped_refptr<Layer>& root() const override;
  void SetRoot(scoped_refptr<Layer> root) override;
  void SetFrameSink(std::unique_ptr<FrameSink> sink) override;
  void ReleaseLayerTreeFrameSink() override;
  std::unique_ptr<ScopedKeepSurfaceAlive> CreateScopedKeepSurfaceAlive(
      const viz::SurfaceId& surface_id) override;
  const SurfaceRangesAndCounts& GetSurfaceRangesForTesting() const override;

  // cc::LayerTreeHostClient.
  void WillBeginMainFrame() override {}
  void DidBeginMainFrame() override {}
  void WillUpdateLayers() override {}
  void DidUpdateLayers() override {}
  void BeginMainFrame(const viz::BeginFrameArgs& args) override;
  void OnDeferMainFrameUpdatesChanged(bool) override {}
  void OnDeferCommitsChanged(
      bool,
      cc::PaintHoldingReason,
      absl::optional<cc::PaintHoldingCommitTrigger>) override {}
  void OnCommitRequested() override {}
  void BeginMainFrameNotExpectedSoon() override {}
  void BeginMainFrameNotExpectedUntil(base::TimeTicks time) override {}
  void UpdateLayerTreeHost() override {}
  void ApplyViewportChanges(const cc::ApplyViewportChangesArgs& args) override {
  }
  void UpdateCompositorScrollState(
      const cc::CompositorCommitData& commit_data) override {}
  void RequestNewLayerTreeFrameSink() override;
  void DidInitializeLayerTreeFrameSink() override;
  void DidFailToInitializeLayerTreeFrameSink() override;
  void WillCommit(const cc::CommitState&) override {}
  void DidCommit(base::TimeTicks, base::TimeTicks) override {}
  void DidCommitAndDrawFrame() override {}
  void DidReceiveCompositorFrameAck() override;
  void DidCompletePageScaleAnimation() override {}
  void DidPresentCompositorFrame(
      uint32_t frame_token,
      const gfx::PresentationFeedback& feedback) override {}
  void RecordStartOfFrameMetrics() override {}
  void RecordEndOfFrameMetrics(
      base::TimeTicks frame_begin_time,
      cc::ActiveFrameSequenceTrackers trackers) override {}
  std::unique_ptr<cc::BeginMainFrameMetrics> GetBeginMainFrameMetrics()
      override;
  std::unique_ptr<cc::WebVitalMetrics> GetWebVitalMetrics() override;
  void NotifyThroughputTrackerResults(
      cc::CustomTrackerResults results) override {}
  void DidObserveFirstScrollDelay(
      base::TimeDelta first_scroll_delay,
      base::TimeTicks first_scroll_timestamp) override {}

  // cc::LayerTreeHostSingleThreadClient.
  void DidSubmitCompositorFrame() override;
  void DidLoseLayerTreeFrameSink() override;

  // Called by `LayerTree::ScopedKeepSurfaceAlive`'s to keep the surface
  // referenced.
  void AddSurfaceRange(const viz::SurfaceRange& surface_range);
  void RemoveSurfaceRange(const viz::SurfaceRange& surface_range);

 private:
  friend LayerTree;
  explicit LayerTreeCcWrapper(InitParams init_params);

  const raw_ptr<LayerTreeClient> client_;
  std::unique_ptr<cc::AnimationHost> animation_host_;
  std::unique_ptr<cc::LayerTreeHost> host_;

  scoped_refptr<Layer> root_;

  base::WeakPtrFactory<LayerTreeCcWrapper> weak_factory_{this};
};

}  // namespace cc::slim

#endif  // CC_SLIM_LAYER_TREE_CC_WRAPPER_H_
