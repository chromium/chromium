// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_PROXY_H_
#define CC_TEST_FAKE_PROXY_H_

#include "base/single_thread_task_runner.h"
#include "cc/trees/layer_tree_host.h"
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
  void SetVisible(bool visible) override {}
  void SetNeedsAnimate() override {}
  void SetNeedsUpdateLayers() override {}
  void SetNeedsCommit() override {}
  void SetNeedsRedraw(const gfx::Rect& damage_rect) override {}
  void SetNextCommitWaitsForActivation() override {}
  bool RequestedAnimatePending() override;
  void SetDeferMainFrameUpdate(bool defer_main_frame_update) override {}
  void StartDeferringCommits(base::TimeDelta timeout) override {}
  void StopDeferringCommits(PaintHoldingCommitTrigger) override {}
  bool CommitRequested() const override;
  void Start() override {}
  void Stop() override {}
  void SetMutator(std::unique_ptr<LayerTreeMutator> mutator) override;
  void SetPaintWorkletLayerPainter(
      std::unique_ptr<PaintWorkletLayerPainter> painter) override;
  bool SupportsImplScrolling() const override;
  bool MainFrameWillHappenForTesting() override;
  void UpdateBrowserControlsState(BrowserControlsState constraints,
                                  BrowserControlsState current,
                                  bool animate) override {}
  void RequestBeginMainFrameNotExpected(bool new_state) override {}
  void SetSourceURL(ukm::SourceId source_id, const GURL& url) override {}
  void ClearHistory() override {}
  void SetRenderFrameObserver(
      std::unique_ptr<RenderFrameMetadataObserver> observer) override {}

 private:
  LayerTreeHost* layer_tree_host_;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_PROXY_H_
