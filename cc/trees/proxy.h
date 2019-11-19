// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_PROXY_H_
#define CC_TREES_PROXY_H_

#include <memory>
#include <string>

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/values.h"
#include "cc/cc_export.h"
#include "cc/input/browser_controls_state.h"
#include "cc/trees/paint_holding_commit_trigger.h"
#include "cc/trees/task_runner_provider.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "url/gurl.h"

namespace gfx {
class Rect;
}

namespace cc {
class LayerTreeFrameSink;
class LayerTreeMutator;
class PaintWorkletLayerPainter;
class RenderFrameMetadataObserver;

// Abstract interface responsible for proxying commands from the main-thread
// side of the compositor over to the compositor implementation.
class CC_EXPORT Proxy {
 public:
  virtual ~Proxy() {}

  virtual bool IsStarted() const = 0;

  virtual void SetLayerTreeFrameSink(
      LayerTreeFrameSink* layer_tree_frame_sink) = 0;
  virtual void ReleaseLayerTreeFrameSink() = 0;

  virtual void SetVisible(bool visible) = 0;

  virtual void SetNeedsAnimate() = 0;
  virtual void SetNeedsUpdateLayers() = 0;
  virtual void SetNeedsCommit() = 0;
  virtual void SetNeedsRedraw(const gfx::Rect& damage_rect) = 0;
  virtual void SetNextCommitWaitsForActivation() = 0;

  // Returns true if an animate or commit has been requested, and hasn't
  // completed yet.
  virtual bool RequestedAnimatePending() = 0;

  // Defers LayerTreeHost::BeginMainFrameUpdate and commits until it is
  // reset. It is only supported when using a scheduler.
  virtual void SetDeferMainFrameUpdate(bool defer_main_frame_update) = 0;

  // Defers commits until at most the given |timeout| period has passed,
  // but continues to update the document lifecycle in
  // LayerTreeHost::BeginMainFrameUpdate. If multiple calls are made when
  // deferal is active the first |timeout| continues to apply.
  virtual void StartDeferringCommits(base::TimeDelta timeout) = 0;

  // Immediately stop deferring commits.
  virtual void StopDeferringCommits(PaintHoldingCommitTrigger) = 0;

  virtual bool CommitRequested() const = 0;

  // Must be called before using the proxy.
  virtual void Start() = 0;
  // Must be called before deleting the proxy.
  virtual void Stop() = 0;

  virtual void SetMutator(std::unique_ptr<LayerTreeMutator> mutator) = 0;

  virtual void SetPaintWorkletLayerPainter(
      std::unique_ptr<PaintWorkletLayerPainter> painter) = 0;

  virtual bool SupportsImplScrolling() const = 0;

  virtual void UpdateBrowserControlsState(BrowserControlsState constraints,
                                          BrowserControlsState current,
                                          bool animate) = 0;

  virtual void RequestBeginMainFrameNotExpected(bool new_state) = 0;

  // Testing hooks
  virtual bool MainFrameWillHappenForTesting() = 0;

  virtual void SetSourceURL(ukm::SourceId source_id, const GURL& url) = 0;

  virtual void ClearHistory() = 0;

  virtual void SetRenderFrameObserver(
      std::unique_ptr<RenderFrameMetadataObserver> observer) = 0;
};

}  // namespace cc

#endif  // CC_TREES_PROXY_H_
