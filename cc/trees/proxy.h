// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_PROXY_H_
#define CC_TREES_PROXY_H_

#include <memory>
#include <string>

#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/types/optional_ref.h"
#include "cc/cc_export.h"
#include "cc/input/browser_controls_offset_tags_info.h"
#include "cc/input/browser_controls_state.h"
#include "cc/trees/paint_holding_commit_trigger.h"
#include "cc/trees/paint_holding_reason.h"
#include "cc/trees/task_runner_provider.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/surfaces/local_surface_id.h"
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
  virtual void SetShouldWarmUp() = 0;

  virtual void SetNeedsAnimate() = 0;
  virtual void SetNeedsUpdateLayers() = 0;
  virtual void SetNeedsCommit() = 0;
  virtual void SetNeedsRedraw(const gfx::Rect& damage_rect) = 0;
  virtual void SetTargetLocalSurfaceId(
      const viz::LocalSurfaceId& target_local_surface_id) = 0;

  // Detaches the InputDelegateForCompositor (InputHandler) bound on the
  // compositor thread.
  virtual void DetachInputDelegateAndRenderFrameObserver() = 0;

  // Returns true if an animate or commit has been requested, and hasn't
  // completed yet.
  virtual bool RequestedAnimatePending() = 0;

  // Defers LayerTreeHost::BeginMainFrameUpdate and commits until it is
  // reset. It is only supported when using a scheduler.
  virtual void SetDeferMainFrameUpdate(bool defer_main_frame_update) = 0;

  // Pauses all main and impl-side rendering.
  virtual void SetPauseRendering(bool pause_rendering) = 0;

  // Indicates that the next main frame will contain the result of running an
  // event handler for an input event.
  virtual void SetInputResponsePending() = 0;

  // Defers commits until at most the given |timeout| period has passed,
  // but continues to update the document lifecycle in
  // LayerTreeHost::BeginMainFrameUpdate. If multiple calls are made when
  // deferal is active the first |timeout| continues to apply.
  virtual bool StartDeferringCommits(base::TimeDelta timeout,
                                     PaintHoldingReason reason) = 0;

  // Immediately stop deferring commits.
  virtual void StopDeferringCommits(PaintHoldingCommitTrigger) = 0;

  virtual bool IsDeferringCommits() const = 0;

  virtual bool CommitRequested() const = 0;

  // Must be called before using the proxy.
  virtual void Start() = 0;
  // Must be called before deleting the proxy.
  virtual void Stop() = 0;

  virtual void QueueImageDecode(int request_id, const PaintImage& image) = 0;
  virtual void SetMutator(std::unique_ptr<LayerTreeMutator> mutator) = 0;

  virtual void SetPaintWorkletLayerPainter(
      std::unique_ptr<PaintWorkletLayerPainter> painter) = 0;

  virtual void UpdateBrowserControlsState(
      BrowserControlsState constraints,
      BrowserControlsState current,
      bool animate,
      base::optional_ref<const BrowserControlsOffsetTagsInfo>
          offset_tags_info) = 0;

  virtual void RequestBeginMainFrameNotExpected(bool new_state) = 0;

  // Testing hooks
  virtual bool MainFrameWillHappenForTesting() = 0;

  virtual void SetSourceURL(ukm::SourceId source_id, const GURL& url) = 0;

  virtual void SetUkmSmoothnessDestination(
      base::WritableSharedMemoryMapping ukm_smoothness_data) = 0;

  virtual void SetRenderFrameObserver(
      std::unique_ptr<RenderFrameMetadataObserver> observer) = 0;

  virtual void CompositeImmediatelyForTest(base::TimeTicks frame_begin_time,
                                           bool raster,
                                           base::OnceClosure callback) = 0;

  // Returns a percentage of dropped frames of the last second.
  // Only implemenented for single threaded proxy.
  virtual double GetPercentDroppedFrames() const = 0;
};

}  // namespace cc

#endif  // CC_TREES_PROXY_H_
