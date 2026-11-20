// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_UNBOUNDED_FRAME_SINK_HANDLER_H_
#define CC_TREES_UNBOUNDED_FRAME_SINK_HANDLER_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "cc/cc_export.h"
#include "cc/trees/layer_tree_frame_sink.h"
#include "cc/trees/layer_tree_frame_sink_client.h"
#include "components/viz/common/hit_test/hit_test_region_list.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/common/resources/returned_resource.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "ui/gfx/geometry/size.h"

namespace viz {
class CompositorFrame;
}  // namespace viz

namespace cc {

class LayerTreeHostImpl;

class CC_EXPORT UnboundedFrameSinkHandler : public LayerTreeFrameSinkClient {
 public:
  explicit UnboundedFrameSinkHandler(LayerTreeHostImpl* host_impl);
  ~UnboundedFrameSinkHandler() override;

  void SetFrameSink(std::unique_ptr<LayerTreeFrameSink> frame_sink,
                    const viz::LocalSurfaceId& local_surface_id);
  void DismissFrameSink();

  void SetLocalSurfaceId(const viz::LocalSurfaceId& local_surface_id);
  bool HasUnsubmittedLocalSurfaceId() const {
    return local_surface_id_.is_valid() &&
           local_surface_id_ != last_submitted_local_surface_id_;
  }

  void SubmitFrame(viz::CompositorFrame frame);

  // LayerTreeFrameSinkClient overrides:
  void SetBeginFrameSource(viz::BeginFrameSource* source) override {}
  std::optional<viz::HitTestRegionList> BuildHitTestData() override;
  void ReclaimResources(std::vector<viz::ReturnedResource> resources) override;
  void SetTreeActivationCallback(base::RepeatingClosure callback) override {}
  void DidReceiveCompositorFrameAck() override {}
  void DidPresentCompositorFrame(
      uint32_t frame_token,
      const viz::FrameTimingDetails& details) override {}
  void DidLoseLayerTreeFrameSink() override;
  void OnDraw(const gfx::Transform& transform,
              const gfx::Rect& viewport,
              bool resourceless_software_draw,
              bool skip_draw) override {}
  void SetMemoryPolicy(const ManagedMemoryPolicy& policy) override {}
  void SetExternalTilePriorityConstraints(
      const gfx::Rect& viewport_rect,
      const gfx::Transform& transform) override {}

 private:
  const raw_ptr<LayerTreeHostImpl> host_impl_;
  std::unique_ptr<LayerTreeFrameSink> frame_sink_;
  // Unreturned export counts for resources submitted through `frame_sink_`,
  // tracked so dismissing or replacing it releases only this sink's references.
  absl::flat_hash_map<viz::ResourceId, int> exported_resources_;
  viz::LocalSurfaceId local_surface_id_;
  viz::LocalSurfaceId last_submitted_local_surface_id_;
  // Size of the last submitted frame, which later frames under the same
  // LocalSurfaceId are clamped to.
  gfx::Size last_submitted_size_in_pixels_;
  // Size of the last submitted frame before clamping.
  gfx::Size last_requested_size_in_pixels_;
};

}  // namespace cc

#endif  // CC_TREES_UNBOUNDED_FRAME_SINK_HANDLER_H_
