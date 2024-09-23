// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_PROXY_H_
#define CC_TEST_FAKE_PROXY_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/types/optional_ref.h"
#include "cc/input/browser_controls_offset_tags_info.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/paint_holding_reason.h"
#include "cc/trees/proxy.h"
#include "cc/trees/render_frame_metadata_observer.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

namespace cc {
class PaintWorkletLayerPainter;
class FakeProxy : public Proxy {
 public:
  FakeProxy() : layer_tree_host_(nullptr) {}

  void SetLayerTreeHost(LayerTreeHost* host);

  bool IsStarted() const override;
  void SetLayerTreeFrameSink(
      LayerTreeFrameSink* layer_tree_frame_sink) override {}
  void ReleaseLayerTreeFrameSink() override {}
  void SetShouldWarmUp() override {}
  void SetVisible(bool visible) override {}
  void SetNeedsAnimate() override {}
  void SetNeedsUpdateLayers() override {}
  void SetNeedsCommit() override {}
  void SetNeedsRedraw(const gfx::Rect& damage_rect) override {}
  void SetTargetLocalSurfaceId(
      const viz::LocalSurfaceId& target_local_surface_id) override {}
  void DetachInputDelegateAndRenderFrameObserver() override {}
  bool RequestedAnimatePending() override;
  void SetDeferMainFrameUpdate(bool defer_main_frame_update) override {}
  bool StartDeferringCommits(base::TimeDelta timeout,
                             PaintHoldingReason reason) override;
  void StopDeferringCommits(PaintHoldingCommitTrigger) override {}
  bool IsDeferringCommits() const override;
  bool CommitRequested() const override;
  void Start() override {}
  void Stop() override {}
  void QueueImageDecode(int request_id, const PaintImage& image) override;
  void SetMutator(std::unique_ptr<LayerTreeMutator> mutator) override;
  void SetPaintWorkletLayerPainter(
      std::unique_ptr<PaintWorkletLayerPainter> painter) override;
  bool MainFrameWillHappenForTesting() override;
  void UpdateBrowserControlsState(
      BrowserControlsState constraints,
      BrowserControlsState current,
      bool animate,
      base::optional_ref<const BrowserControlsOffsetTagsInfo> offset_tags_info)
      override {}
  void RequestBeginMainFrameNotExpected(bool new_state) override {}
  void SetSourceURL(ukm::SourceId source_id, const GURL& url) override {}
  void SetUkmSmoothnessDestination(
      base::WritableSharedMemoryMapping ukm_smoothness_data) override {}
  void SetRenderFrameObserver(
      std::unique_ptr<RenderFrameMetadataObserver> observer) override {}
  void CompositeImmediatelyForTest(base::TimeTicks frame_begin_time,
                                   bool raster,
                                   base::OnceClosure callback) override {}
  double GetPercentDroppedFrames() const override;
  void SetPauseRendering(bool pause_rendering) override {}
  void SetInputResponsePending() override {}

 private:
  raw_ptr<LayerTreeHost> layer_tree_host_;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_PROXY_H_
