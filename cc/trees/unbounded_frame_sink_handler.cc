// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/unbounded_frame_sink_handler.h"

#include <utility>
#include <vector>

#include "cc/trees/layer_tree_host_impl.h"
#include "components/viz/client/client_resource_provider.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/resources/returned_resource.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "ui/gfx/gpu_fence_handle.h"

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
  DismissFrameSink();
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
  last_requested_size_in_pixels_ = gfx::Size();

  if (exported_resources_.empty()) {
    return;
  }
  // Viz can't return resources to a detached sink, so release this sink's
  // references as lost because Viz may still be using them.
  std::vector<viz::ReturnedResource> lost_resources;
  lost_resources.reserve(exported_resources_.size());
  for (const auto& [id, count] : exported_resources_) {
    lost_resources.emplace_back(id, gpu::SharedImageExportResult(),
                                gfx::GpuFenceHandle(), count, /*lost=*/true);
  }
  exported_resources_.clear();
  host_impl_->resource_provider()->ReceiveReturnsFromParent(
      std::move(lost_resources));
}

void UnboundedFrameSinkHandler::SetLocalSurfaceId(
    const viz::LocalSurfaceId& local_surface_id) {
  local_surface_id_ = local_surface_id;
  if (frame_sink_) {
    frame_sink_->SetLocalSurfaceId(local_surface_id);
  }
}

void UnboundedFrameSinkHandler::SubmitFrame(viz::CompositorFrame frame) {
  if (frame_sink_ && local_surface_id_.is_valid()) {
    // Viz treats the first frame under a new LocalSurfaceId as a continuation
    // of the previous surface and only redraws its damage_rect, but cc may
    // have already reported that damage on an earlier, clamped frame. The
    // first frame clamped below after a size change has damage computed for
    // its unclamped size, which misses the strip left behind when the surface
    // shrinks. Fully damage both. Later clamped frames at the same unclamped
    // size keep cc's damage.
    const gfx::Size requested_size_in_pixels = frame.size_in_pixels();
    const bool needs_full_damage =
        !frame.render_pass_list.empty() &&
        (local_surface_id_ != last_submitted_local_surface_id_ ||
         requested_size_in_pixels != last_requested_size_in_pixels_);
    if (local_surface_id_ == last_submitted_local_surface_id_ &&
        !last_submitted_size_in_pixels_.IsEmpty() &&
        !frame.render_pass_list.empty()) {
      frame.render_pass_list.back()->output_rect.set_size(
          last_submitted_size_in_pixels_);
    }
    if (needs_full_damage) {
      viz::CompositorRenderPass& root_pass = *frame.render_pass_list.back();
      root_pass.damage_rect = root_pass.output_rect;
      root_pass.has_damage_from_contributing_content = true;
    }
    last_requested_size_in_pixels_ = requested_size_in_pixels;
    last_submitted_size_in_pixels_ = frame.size_in_pixels();
    last_submitted_local_surface_id_ = local_surface_id_;
    // In TreesInViz, Viz returns unbounded resources directly to
    // LayerContextImpl.
    if (!host_impl_->settings().trees_in_viz_in_viz_process) {
      for (const viz::TransferableResource& resource : frame.resource_list) {
        ++exported_resources_[resource.id];
      }
    }
    frame_sink_->SubmitCompositorFrame(std::move(frame),
                                       /*hit_test_data_changed=*/false);
  } else {
    // Reclaim resources immediately for the dropped frame so the renderer
    // doesn't leak them.
    std::vector<viz::ReturnedResource> returned_resources =
        viz::TransferableResource::ReturnResources(frame.resource_list);
    if (!returned_resources.empty()) {
      host_impl_->resource_provider()->ReceiveReturnsFromParent(
          std::move(returned_resources));
    }
  }
}

std::optional<viz::HitTestRegionList>
UnboundedFrameSinkHandler::BuildHitTestData() {
  return std::nullopt;
}

void UnboundedFrameSinkHandler::ReclaimResources(
    std::vector<viz::ReturnedResource> resources) {
  for (const viz::ReturnedResource& resource : resources) {
    auto it = exported_resources_.find(resource.id);
    if (it == exported_resources_.end()) {
      // Exports aren't tracked in the Viz process (see SubmitFrame()).
      continue;
    }
    CHECK_GE(it->second, resource.count);
    it->second -= resource.count;
    if (it->second == 0) {
      exported_resources_.erase(it);
    }
  }
  host_impl_->resource_provider()->ReceiveReturnsFromParent(
      std::move(resources));
}

void UnboundedFrameSinkHandler::DidLoseLayerTreeFrameSink() {
  host_impl_->DismissUnboundedFrameSink();
}

}  // namespace cc
