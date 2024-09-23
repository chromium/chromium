// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_LAYER_TREE_FRAME_SINK_CLIENT_H_
#define CC_TREES_LAYER_TREE_FRAME_SINK_CLIENT_H_

#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "cc/cc_export.h"
#include "components/viz/common/resources/returned_resource.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "ui/gfx/geometry/rect.h"

namespace gfx {
class Transform;
}

namespace viz {
class BeginFrameSource;
struct FrameTimingDetails;
struct HitTestRegionList;
}

namespace cc {

struct ManagedMemoryPolicy;

class CC_EXPORT LayerTreeFrameSinkClient {
 public:
  // Pass the begin frame source for the client to observe.  Client does not own
  // the viz::BeginFrameSource. LayerTreeFrameSink should call this once after
  // binding to the client and then call again with a null while detaching.
  virtual void SetBeginFrameSource(viz::BeginFrameSource* source) = 0;

  // Builds and returns a HitTestRegionList from the active LayerTreeImpl. To be
  // called during SubmitCompositorFrame().
  // TODO(danakj): Just pass it into SubmitCompositorFrame(), with a
  // LayerTreeSetting to enable it or not.
  virtual std::optional<viz::HitTestRegionList> BuildHitTestData() = 0;

  // Returns resources sent to SubmitCompositorFrame to be reused or freed.
  virtual void ReclaimResources(
      std::vector<viz::ReturnedResource> resources) = 0;

  // If set, |callback| will be called subsequent to each new tree activation,
  // regardless of the compositor visibility or damage. |callback| must remain
  // valid for the lifetime of the LayerTreeFrameSinkClient or until
  // unregistered by giving a null callback.
  virtual void SetTreeActivationCallback(base::RepeatingClosure callback) = 0;

  // Notification that the previous CompositorFrame given to
  // SubmitCompositorFrame() has been processed and that another frame
  // can be submitted. This provides backpressure from the display compositor
  // so that frames are submitted only at the rate it can handle them.
  virtual void DidReceiveCompositorFrameAck() = 0;

  // See components/viz/common/frame_timing_details.h for details on args.
  virtual void DidPresentCompositorFrame(
      uint32_t frame_token,
      const viz::FrameTimingDetails& details) = 0;

  // The LayerTreeFrameSink is lost when the viz::RasterContextProviders held by
  // it encounter an error. In this case the LayerTreeFrameSink (and the
  // viz::RasterContextProviders) must be recreated.
  virtual void DidLoseLayerTreeFrameSink() = 0;

  // For SynchronousCompositor (WebView) to ask the layer compositor to submit
  // a new CompositorFrame synchronously.
  virtual void OnDraw(const gfx::Transform& transform,
                      const gfx::Rect& viewport,
                      bool resourceless_software_draw,
                      bool skip_draw) = 0;

  // For SynchronousCompositor (WebView) to set how much memory the compositor
  // can use without changing visibility.
  virtual void SetMemoryPolicy(const ManagedMemoryPolicy& policy) = 0;

  // For SynchronousCompositor (WebView) to change which tiles should be
  // included in submitted CompositorFrames independently of what the viewport
  // is.
  // |viewport_rect| is in device viewport space.
  // |transform| transforms from from device viewport space to screen space.
  virtual void SetExternalTilePriorityConstraints(
      const gfx::Rect& viewport_rect,
      const gfx::Transform& transform) = 0;

  // Notification that the compositor frame transition directive has been
  // processed.
  virtual void OnCompositorFrameTransitionDirectiveProcessed(
      uint32_t sequence_id) {}

  virtual void OnSurfaceEvicted(const viz::LocalSurfaceId& local_surface_id) {}

 protected:
  virtual ~LayerTreeFrameSinkClient() {}
};

}  // namespace cc

#endif  // CC_TREES_LAYER_TREE_FRAME_SINK_CLIENT_H_
