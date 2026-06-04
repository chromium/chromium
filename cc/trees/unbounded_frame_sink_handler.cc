// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/unbounded_frame_sink_handler.h"

#include <utility>

#include "cc/trees/layer_tree_host_impl.h"
#include "components/viz/client/client_resource_provider.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/resources/returned_resource.h"
#include "components/viz/common/resources/transferable_resource.h"

namespace cc {

UnboundedFrameSinkHandler::UnboundedFrameSinkHandler(
    LayerTreeHostImpl* host_impl)
    : host_impl_(host_impl) {
  DCHECK(host_impl_->settings().enable_unbounded_element);
}

UnboundedFrameSinkHandler::~UnboundedFrameSinkHandler() {
  DismissFrameSink();
}

void UnboundedFrameSinkHandler::SetFrameSink(
    std::unique_ptr<LayerTreeFrameSink> frame_sink,
    const viz::LocalSurfaceId& local_surface_id) {
  frame_sink_ = std::move(frame_sink);
  local_surface_id_ = local_surface_id;
  if (frame_sink_) {
    frame_sink_->BindToClient(this);
    frame_sink_->SetLocalSurfaceId(local_surface_id);
  }
}

void UnboundedFrameSinkHandler::DismissFrameSink() {
  if (frame_sink_) {
    frame_sink_->DetachFromClient();
    frame_sink_.reset();
  }
  local_surface_id_ = viz::LocalSurfaceId();
  last_submitted_local_surface_id_ = viz::LocalSurfaceId();
  last_submitted_size_in_pixels_ = gfx::Size();
}

void UnboundedFrameSinkHandler::SetLocalSurfaceId(
    const viz::LocalSurfaceId& local_surface_id) {
  local_surface_id_ = local_surface_id;
  if (frame_sink_) {
    frame_sink_->SetLocalSurfaceId(local_surface_id);
  }
}

void UnboundedFrameSinkHandler::SubmitFrame(viz::CompositorFrame frame) {
  // If the frame size changed but we haven't received a new LocalSurfaceId from
  // the browser yet, we must drop the frame. Submitting a frame with a
  // mismatched size/ID causes validation errors and crashes in Viz.
  bool size_changed_without_new_local_surface_id =
      frame.size_in_pixels() != last_submitted_size_in_pixels_ &&
      local_surface_id_ == last_submitted_local_surface_id_;
  if (frame_sink_ && !size_changed_without_new_local_surface_id) {
    last_submitted_size_in_pixels_ = frame.size_in_pixels();
    last_submitted_local_surface_id_ = local_surface_id_;
    frame_sink_->SubmitCompositorFrame(std::move(frame),
                                       /*hit_test_data_changed=*/false);
  } else {
    // Reclaim resources immediately for the dropped frame so the renderer
    // doesn't leak them.
    std::vector<viz::ReturnedResource> returned_resources =
        viz::TransferableResource::ReturnResources(frame.resource_list);
    if (!returned_resources.empty()) {
      ReclaimResources(std::move(returned_resources));
    }
  }
}

std::optional<viz::HitTestRegionList>
UnboundedFrameSinkHandler::BuildHitTestData() {
  return std::nullopt;
}

void UnboundedFrameSinkHandler::ReclaimResources(
    std::vector<viz::ReturnedResource> resources) {
  host_impl_->resource_provider()->ReceiveReturnsFromParent(
      std::move(resources));
}

void UnboundedFrameSinkHandler::DidLoseLayerTreeFrameSink() {
  host_impl_->DismissUnboundedFrameSink();
}

}  // namespace cc
