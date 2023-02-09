// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/slim/layer_tree_impl.h"

#include <algorithm>
#include <memory>

#include "base/auto_reset.h"
#include "cc/slim/frame_sink_impl.h"
#include "cc/slim/layer.h"
#include "cc/slim/layer_tree_client.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/hit_test/hit_test_region_list.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect.h"

namespace cc::slim {

LayerTreeImpl::LayerTreeImpl(LayerTreeClient* client) : client_(client) {}

LayerTreeImpl::~LayerTreeImpl() = default;

cc::UIResourceManager* LayerTreeImpl::GetUIResourceManager() {
  return &ui_resource_manager_;
}

void LayerTreeImpl::SetViewportRectAndScale(
    const gfx::Rect& device_viewport_rect,
    float device_scale_factor,
    const viz::LocalSurfaceId& local_surface_id) {
  if (local_surface_id_ != local_surface_id) {
    local_surface_id_ = local_surface_id;
    if (frame_sink_) {
      frame_sink_->SetLocalSurfaceId(local_surface_id);
    }
  }

  device_viewport_rect_ = device_viewport_rect;
  device_scale_factor_ = device_scale_factor;
  SetNeedsDraw();
}

void LayerTreeImpl::set_background_color(SkColor4f color) {
  if (background_color_ == color) {
    return;
  }

  background_color_ = color;
  SetNeedsDraw();
}

void LayerTreeImpl::SetVisible(bool visible) {
  if (visible_ == visible) {
    return;
  }

  visible_ = visible;
  MaybeRequestFrameSink();
  SetNeedsDraw();
}

bool LayerTreeImpl::IsVisible() const {
  return visible_;
}

void LayerTreeImpl::RequestPresentationTimeForNextFrame(
    PresentationCallback callback) {
  // TODO(crbug.com/1408128): Implement.
}

void LayerTreeImpl::RequestSuccessfulPresentationTimeForNextFrame(
    SuccessfulCallback callback) {
  // TODO(crbug.com/1408128): Implement.
}

void LayerTreeImpl::set_display_transform_hint(gfx::OverlayTransform hint) {
  // TODO(crbug.com/1408128): Implement.
}

void LayerTreeImpl::RequestCopyOfOutput(
    std::unique_ptr<viz::CopyOutputRequest> request) {
  // TODO(crbug.com/1408128): Implement.
}

base::OnceClosure LayerTreeImpl::DeferBeginFrame() {
  num_defer_begin_frame_++;
  UpdateNeedsBeginFrame();
  return base::BindOnce(&LayerTreeImpl::ReleaseDeferBeginFrame,
                        weak_factory_.GetWeakPtr());
}

void LayerTreeImpl::ReleaseDeferBeginFrame() {
  DCHECK_GT(num_defer_begin_frame_, 0u);
  num_defer_begin_frame_--;
  UpdateNeedsBeginFrame();
}

void LayerTreeImpl::UpdateTopControlsVisibleHeight(float height) {
  if (top_controls_visible_height_ &&
      top_controls_visible_height_.value() == height) {
    return;
  }
  top_controls_visible_height_ = height;
  SetNeedsDraw();
}

void LayerTreeImpl::SetNeedsAnimate() {
  SetClientNeedsOneBeginFrame();
}

void LayerTreeImpl::SetNeedsRedraw() {
  SetClientNeedsOneBeginFrame();
}

const scoped_refptr<Layer>& LayerTreeImpl::root() const {
  return root_;
}

void LayerTreeImpl::SetRoot(scoped_refptr<Layer> root) {
  if (root_ == root) {
    return;
  }
  if (root_) {
    root_->SetLayerTree(nullptr);
  }
  root_ = std::move(root);
  root_->SetLayerTree(this);
  SetNeedsDraw();
}

void LayerTreeImpl::SetFrameSink(std::unique_ptr<FrameSink> sink) {
  DCHECK(sink);
  frame_sink_.reset(static_cast<FrameSinkImpl*>(sink.release()));
  if (!frame_sink_->BindToClient(this)) {
    frame_sink_.reset();
    // This is equivalent to requesting another frame sink, so do not reset
    // `frame_sink_request_pending_` to avoid extra unexpected calls to
    // `RequestNewFrameSink`.
    client_->DidFailToInitializeLayerTreeFrameSink();
    return;
  }
  frame_sink_request_pending_ = false;
  if (local_surface_id_.is_valid()) {
    frame_sink_->SetLocalSurfaceId(local_surface_id_);
  }
  client_->DidInitializeLayerTreeFrameSink();
  ui_resource_manager_.RecreateUIResources();

  UpdateNeedsBeginFrame();
}

void LayerTreeImpl::ReleaseLayerTreeFrameSink() {
  DCHECK(!IsVisible());
  frame_sink_.reset();
}

bool LayerTreeImpl::BeginFrame(
    const viz::BeginFrameArgs& args,
    viz::CompositorFrame& out_frame,
    base::flat_set<viz::ResourceId>& out_resource_ids,
    viz::HitTestRegionList& out_hit_test_region_list) {
  // Skip any delayed BeginFrame messages that arrive even after we no longer
  // need it.
  if (!NeedsBeginFrames()) {
    frame_sink_->SetNeedsBeginFrame(false);
    return false;
  }

  // Unset `client_needs_one_begin_frame_` before BeginFrame. If client
  // requests more frames from inside the BeginFrame call, it's for the next
  // frame.
  client_needs_one_begin_frame_ = false;
  {
    base::AutoReset<bool> reset(&update_needs_begin_frame_pending_, true);
    client_->BeginFrame(args);
  }
  // Unset `needs_draw_` after client `BeginFrame`. Any layer or tree property
  // changes made by client `BeginFrame` are about to be drawn, so there is no
  // need for another frame.
  needs_draw_ = false;
  // TODO(crbug.com/1408128): Implement frame production here.
  UpdateNeedsBeginFrame();
  return false;
}

void LayerTreeImpl::DidReceiveCompositorFrameAck() {
  client_->DidReceiveCompositorFrameAck();
}

void LayerTreeImpl::DidSubmitCompositorFrame() {
  client_->DidSubmitCompositorFrame();
}

void LayerTreeImpl::DidPresentCompositorFrame(
    uint32_t frame_token,
    const viz::FrameTimingDetails& details) {
  // TODO(crbug.com/1408128): Implement.
}

void LayerTreeImpl::DidLoseLayerTreeFrameSink() {
  client_->DidLoseLayerTreeFrameSink();
  frame_sink_.reset();
  MaybeRequestFrameSink();
}

void LayerTreeImpl::NotifyTreeChanged() {
  SetNeedsDraw();
}

void LayerTreeImpl::NotifyPropertyChanged() {
  SetNeedsDraw();
}

void LayerTreeImpl::MaybeRequestFrameSink() {
  if (frame_sink_ || !visible_ || frame_sink_request_pending_) {
    return;
  }
  frame_sink_request_pending_ = true;
  client_->RequestNewFrameSink();
}

void LayerTreeImpl::UpdateNeedsBeginFrame() {
  if (update_needs_begin_frame_pending_) {
    return;
  }

  if (frame_sink_ && NeedsBeginFrames()) {
    frame_sink_->SetNeedsBeginFrame(true);
  }
}

void LayerTreeImpl::SetClientNeedsOneBeginFrame() {
  client_needs_one_begin_frame_ = true;
  UpdateNeedsBeginFrame();
}

void LayerTreeImpl::SetNeedsDraw() {
  needs_draw_ = true;
  UpdateNeedsBeginFrame();
}

bool LayerTreeImpl::NeedsBeginFrames() const {
  if (!visible_ || !frame_sink_ || num_defer_begin_frame_ > 0u) {
    return false;
  }
  return client_needs_one_begin_frame_ || needs_draw_;
}

}  // namespace cc::slim
