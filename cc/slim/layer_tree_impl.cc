// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/slim/layer_tree_impl.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "base/auto_reset.h"
#include "base/containers/adapters.h"
#include "base/trace_event/trace_event.h"
#include "cc/slim/frame_sink_impl.h"
#include "cc/slim/layer.h"
#include "cc/slim/layer_tree_client.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/hit_test/hit_test_region_list.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/quads/compositor_frame_metadata.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/quads/draw_quad.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/transform.h"

namespace cc::slim {

LayerTreeImpl::PresentationCallbackInfo::PresentationCallbackInfo(
    uint32_t frame_token,
    std::vector<PresentationCallback> presentation_callbacks,
    std::vector<SuccessfulCallback> success_callbacks)
    : frame_token(frame_token),
      presentation_callbacks(std::move(presentation_callbacks)),
      success_callbacks(std::move(success_callbacks)) {}
LayerTreeImpl::PresentationCallbackInfo::~PresentationCallbackInfo() = default;
LayerTreeImpl::PresentationCallbackInfo::PresentationCallbackInfo(
    PresentationCallbackInfo&&) = default;
LayerTreeImpl::PresentationCallbackInfo&
LayerTreeImpl::PresentationCallbackInfo::operator=(PresentationCallbackInfo&&) =
    default;

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
  presentation_callback_for_next_frame_.emplace_back(std::move(callback));
}

void LayerTreeImpl::RequestSuccessfulPresentationTimeForNextFrame(
    SuccessfulCallback callback) {
  success_callback_for_next_frame_.emplace_back(std::move(callback));
}

void LayerTreeImpl::set_display_transform_hint(gfx::OverlayTransform hint) {
  display_transform_hint_ = hint;
}

void LayerTreeImpl::RequestCopyOfOutput(
    std::unique_ptr<viz::CopyOutputRequest> request) {
  if (request->has_source()) {
    const base::UnguessableToken& source = request->source();
    auto it = base::ranges::find_if(
        copy_requests_for_next_frame_,
        [&source](const std::unique_ptr<viz::CopyOutputRequest>& x) {
          return x->has_source() && x->source() == source;
        });
    if (it != copy_requests_for_next_frame_.end()) {
      copy_requests_for_next_frame_.erase(it);
    }
  }
  copy_requests_for_next_frame_.push_back(std::move(request));
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
  if (root_) {
    root_->SetLayerTree(this);
    SetNeedsDraw();
  }
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

  if (!root_) {
    UpdateNeedsBeginFrame();
    return false;
  }

  GenerateCompositorFrame(args, out_frame, out_resource_ids,
                          out_hit_test_region_list);
  UpdateNeedsBeginFrame();
  return true;
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
  const bool success = !details.presentation_feedback.failed();
  for (auto itr = pending_presentation_callbacks_.begin();
       itr != pending_presentation_callbacks_.end();) {
    if (viz::FrameTokenGT(itr->frame_token, frame_token)) {
      break;
    }
    for (auto& callback : itr->presentation_callbacks) {
      std::move(callback).Run(details.presentation_feedback);
    }
    itr->presentation_callbacks.clear();

    // Only run `success_callbacks` if successful.
    if (success) {
      for (auto& callback : itr->success_callbacks) {
        std::move(callback).Run(details.presentation_feedback.timestamp);
      }
      itr->success_callbacks.clear();
    }
    // Keep the entry of `success_callbacks` is not empty, meaning this frame
    // wasn't successful, so that it can run on a subsequent successful frame.
    if (itr->success_callbacks.empty()) {
      itr = pending_presentation_callbacks_.erase(itr);
    } else {
      itr++;
    }
  }
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

viz::ClientResourceProvider* LayerTreeImpl::GetClientResourceProvider() {
  if (!frame_sink_) {
    return nullptr;
  }
  return frame_sink_->client_resource_provider();
}

viz::ResourceId LayerTreeImpl::GetVizResourceId(cc::UIResourceId id) {
  if (!frame_sink_) {
    return viz::kInvalidResourceId;
  }
  return frame_sink_->GetVizResourceId(id);
}

bool LayerTreeImpl::IsUIResourceOpaque(int resource_id) {
  return !frame_sink_ || frame_sink_->IsUIResourceOpaque(resource_id);
}

gfx::Size LayerTreeImpl::GetUIResourceSize(int resource_id) {
  if (!frame_sink_) {
    return gfx::Size();
  }

  return frame_sink_->GetUIResourceSize(resource_id);
}

void LayerTreeImpl::AddSurfaceRange(const viz::SurfaceRange& range) {
  DCHECK(range.IsValid());
  DCHECK(!referenced_surfaces_.contains(range));
  referenced_surfaces_.insert(range);
}

void LayerTreeImpl::RemoveSurfaceRange(const viz::SurfaceRange& range) {
  DCHECK(range.IsValid());
  DCHECK(referenced_surfaces_.contains(range));
  referenced_surfaces_.erase(range);
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

void LayerTreeImpl::GenerateCompositorFrame(
    const viz::BeginFrameArgs& args,
    viz::CompositorFrame& out_frame,
    base::flat_set<viz::ResourceId>& out_resource_ids,
    viz::HitTestRegionList& out_hit_test_region_list) {
  // TODO(crbug.com/1408128): Only has a very simple and basic compositor frame
  // generation. Some missing features include:
  // * Support multiple render passes (non-axis aligned clip, filters)
  // * Damage tracking
  // * Occlusion culling
  // * Visible rect (ie clip) on quads
  // * Surface embedding fields (referenced surfaces, activation dependency,
  //   deadline)
  TRACE_EVENT0("cc", "slim::LayerTreeImpl::ProduceFrame");

  for (auto& resource_request :
       ui_resource_manager_.TakeUIResourcesRequests()) {
    switch (resource_request.GetType()) {
      case cc::UIResourceRequest::UI_RESOURCE_CREATE:
        frame_sink_->UploadUIResource(resource_request.GetId(),
                                      resource_request.GetBitmap());
        break;
      case cc::UIResourceRequest::UI_RESOURCE_DELETE:
        frame_sink_->MarkUIResourceForDeletion(resource_request.GetId());
        break;
    }
  }

  auto render_pass = viz::CompositorRenderPass::Create();
  render_pass->SetNew(viz::CompositorRenderPassId(root_->id()),
                      /*output_rect=*/device_viewport_rect_,
                      /*damage_rect=*/device_viewport_rect_,
                      /*transform_to_root_target=*/gfx::Transform());

  out_frame.metadata.frame_token = ++next_frame_token_;
  out_frame.metadata.begin_frame_ack =
      viz::BeginFrameAck(args, /*has_damage=*/true);
  out_frame.metadata.device_scale_factor = device_scale_factor_;
  out_frame.metadata.root_background_color = background_color_;
  out_frame.metadata.referenced_surfaces = std::vector<viz::SurfaceRange>(
      referenced_surfaces_.begin(), referenced_surfaces_.end());
  out_frame.metadata.top_controls_visible_height = top_controls_visible_height_;
  top_controls_visible_height_.reset();
  out_frame.metadata.display_transform_hint = display_transform_hint_;

  Draw(*root_, *render_pass, /*transform_to_target=*/gfx::Transform(),
       /*clip_from_parent=*/nullptr);

  render_pass->copy_requests = std::move(copy_requests_for_next_frame_);
  copy_requests_for_next_frame_.clear();
  out_frame.render_pass_list.push_back(std::move(render_pass));

  for (const auto& pass : out_frame.render_pass_list) {
    for (const auto* quad : pass->quad_list) {
      for (viz::ResourceId resource_id : quad->resources) {
        out_resource_ids.insert(resource_id);
      }
    }
  }

  if (!presentation_callback_for_next_frame_.empty() ||
      !success_callback_for_next_frame_.empty()) {
    pending_presentation_callbacks_.emplace_back(
        out_frame.metadata.frame_token,
        std::move(presentation_callback_for_next_frame_),
        std::move(success_callback_for_next_frame_));
  }
}

void LayerTreeImpl::Draw(Layer& layer,
                         viz::CompositorRenderPass& parent_pass,
                         const gfx::Transform& transform_to_target,
                         const gfx::Rect* clip_from_parent) {
  if (layer.hide_layer_and_subtree()) {
    return;
  }

  gfx::Transform transform_to_parent = layer.ComputeTransformToParent();

  // New transform is: parent transform x layer transform.
  gfx::Transform new_transform_to_target = transform_to_target;
  new_transform_to_target.PreConcat(transform_to_parent);

  bool use_new_clip = false;
  gfx::Rect new_clip;
  // Drop non-axis aligned clip instead of using new render pass.
  // TODO(crbug.com/1408128): Clip in layer space (visible rect) for clip
  // that is not an exact integer.
  if (layer.masks_to_bounds() &&
      new_transform_to_target.Preserves2dAxisAlignment()) {
    new_clip.set_size(layer.bounds());
    new_clip = new_transform_to_target.MapRect(new_clip);
    if (clip_from_parent) {
      new_clip.Intersect(*clip_from_parent);
    }
    use_new_clip = true;
  }
  const gfx::Rect* clip = use_new_clip ? &new_clip : clip_from_parent;

  for (auto& child : base::Reversed(layer.children())) {
    Draw(*child, parent_pass, new_transform_to_target, clip);
  }

  if (!layer.bounds().IsEmpty() && layer.HasDrawableContent()) {
    layer.AppendQuads(parent_pass, new_transform_to_target, clip);
  }
}

}  // namespace cc::slim
